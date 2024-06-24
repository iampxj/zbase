/*
 * Copyright 2022 wtcat
 * 
 * This is simple partition file component,
 * The file size is fixed and can't dynamic create a new file 
 * 
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<partition-file>: "fmt
#define CONFIG_LOGLEVEL   LOGLEVEL_DEBUG
#include <errno.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <sys/types.h>

#include "basework/os/osapi.h"
#include "basework/dev/partition_file.h"
#include "basework/dev/partition.h"
#include "basework/dev/disk.h"
#include "basework/dev/blkdev.h"
#include "basework/lib/crc.h"
#include "basework/malloc.h"
#include "basework/minmax.h"
#include "basework/log.h"


struct sfile_metadata {
#define USR_FILE_MAGIC 0xdeadbeef
    uint32_t magic;
    uint32_t len;
    uint32_t crc;
} __packed;

struct sfile {
    struct sfile *link;
    const struct disk_partition *dp;
    struct disk_device *dd;
    struct sfile_metadata metadata;
    const char *pname;
    size_t base;
    size_t offset;
    size_t fsize;
    int refcnt;
    bool dirty;
};

STATIC_ASSERT(SFILE_METADATA_SIZE == sizeof(struct sfile_metadata), "");

#define SFILE_MTX_LOCK()   os_mtx_lock(&sfile_mtx)
#define SFILE_MTX_UNLOCK() os_mtx_unlock(&sfile_mtx)

static os_mutex_t sfile_mtx;
static struct sfile *sfile_head;

static inline ssize_t sfile_read(struct sfile *fp, void *buf, 
    size_t size, uint32_t offset) {
    return blkdev_read(fp->dd, buf, size, fp->dp->offset + offset);
}

static inline ssize_t sfile_write(struct sfile *fp, const void *buf, 
    size_t size, uint32_t offset) {
    return blkdev_write(fp->dd, buf, size, fp->dp->offset + offset);
}

static struct sfile* sfile_open(const char *name, bool *found) {
    const struct disk_partition *dp;
    struct disk_device *dd;
    struct sfile *fp;

    dp = disk_partition_find(name);
    if (dp == NULL) {
        pr_err("not found partition file(%s)\n", name);
        return NULL;
    }
    if (disk_device_open(dp->parent, &dd)) {
        pr_err("not found disk device(%s)\n", dp->parent);
        return NULL;
    }
    for (fp = sfile_head; fp; fp = fp->link) {
        if (fp->dp == dp) {
            fp->refcnt++;
            if (found)
                *found = true;
            return fp;
        }
    }
    fp = general_calloc(1, sizeof(*fp));
    if (!fp) {
        pr_err("no more memory!\n");
        return NULL;
    }

    fp->link = sfile_head;
    sfile_head = fp;

	fp->pname = name;
    fp->base = sizeof(fp->metadata);
    fp->dd = dd;
    fp->dp = dp;
    *found = false;
    return fp;
}

static int sfile_close(struct sfile *fp) {
    struct sfile **ifp = &sfile_head;
    while (*ifp) {
        if ((*ifp) == fp) {
            *ifp = fp->link;
            fp->link = NULL;
            general_free(fp);
            return 0;
        }
        ifp = &(*ifp)->link;
    }
    return -ENODATA;
}

static uint32_t usr_sfile_chksum(struct sfile *fp, const char *name) {
    size_t size = fp->metadata.len;
    size_t ofs = fp->base;
    char buffer[256];
    uint32_t crc = 0;

    while (size > 0) {
        size_t bytes = rte_min(sizeof(buffer), size);
        int ret = sfile_read(fp, buffer, bytes, ofs);
        if ((size_t)ret != bytes) {
            pr_err("read file failed\n");
            return false;
        }

        crc = lib_crc32part((const uint8_t *)buffer, bytes, crc);
        size -= bytes;
        ofs += bytes;
    }

    /* Append name checksum */
    return lib_crc32part((const uint8_t *)name, strlen(name), crc);
}

int usr_sfile_open(const char *name, struct sfile **p) {
    const struct disk_partition *dp;
    struct sfile *fp;
    bool found = false;
    int err = 0;
	
    if (!name || !p)
        return -EINVAL;

    SFILE_MTX_LOCK();
    if ((fp = sfile_open(name, &found)) == NULL) {
        err = -ENOMEM;
        goto _unlock;
    }
        
    if (found) {
        *p = fp;
        goto _unlock;
    }
    dp = fp->dp;
    assert(dp != NULL);

    err = sfile_read(fp, &fp->metadata, sizeof(fp->metadata), 0);
    if (err <= 0) {
        sfile_close(fp);
        err = -EIO;
        goto _unlock;
    }

    if (fp->metadata.magic != USR_FILE_MAGIC ||
        usr_sfile_chksum(fp, name) != fp->metadata.crc) {
        pr_warn("file(%s) data is invalid (magic:0x%08x, crc: 0x%08x size: %d) \n", 
        	name, fp->metadata.magic, fp->metadata.crc, fp->metadata.len);
        memset(&fp->metadata, 0, sizeof(fp->metadata));
        fp->metadata.magic = USR_FILE_MAGIC;
    }

    fp->refcnt = 1;
    fp->dirty = false;
    fp->fsize = fp->metadata.len;
    fp->offset = 0;
    *p = fp;
	err = 0;
_unlock:
    SFILE_MTX_UNLOCK();
    return err;
}

int usr_sfile_invalid(struct sfile *fp) {
    int err;
    if (!fp || !fp->dp)
        return -EINVAL;

    SFILE_MTX_LOCK();
    memset(&fp->metadata, 0, sizeof(fp->metadata));
    fp->metadata.magic = USR_FILE_MAGIC;
    err = lgpt_write(fp->dp, 0, &fp->metadata, sizeof(fp->metadata));
    SFILE_MTX_UNLOCK();
    return err > 0? 0: err;
}

ssize_t usr_sfile_write(struct sfile *fp, const void *buffer ,size_t size) {
    int ret;
    assert(fp != NULL);
    assert(fp->dp != NULL);
    if (!size)
        return 0;

    SFILE_MTX_LOCK();
    if (fp->base + fp->offset + size > fp->dp->len) {
		pr_err("write buffer is too large\n");
        ret = -EINVAL;
        goto _unlock;
    }

	pr_dbg("write usrfile(%s) buffer(%p) size(%d) offset(%d)\n", fp->pname, 
		buffer, size, fp->base + fp->offset);

    ret = sfile_write(fp, buffer, size, fp->base + fp->offset);
    if (ret != (int)size) {
		pr_err("write file failed(%d)\n", ret);
        ret = -EIO;
        goto _unlock;
    }
    fp->dirty = true;
    fp->offset += ret;
    if (fp->offset > fp->fsize)
        fp->fsize = fp->offset;
_unlock:
    SFILE_MTX_UNLOCK();
    return ret;
}

ssize_t usr_sfile_read(struct sfile *fp, void *buffer ,size_t size) {
    int ret;
    assert(fp != NULL);
    assert(fp->dp != NULL);

    /* Read bytes can not large than real bytes */
    size = rte_min(size, fp->fsize);

    if (!size)
        return 0;

    SFILE_MTX_LOCK();
    if (fp->base + fp->offset + size > fp->dp->len) {
		pr_err("read buffer is too large\n");
        ret = -EINVAL;
        goto _unlock;
    }

	pr_dbg("read usrfile(%s) buffer(%p) size(%d) offset(%d)\n", fp->pname, 
		buffer, size, fp->base + fp->offset);
    ret = sfile_read(fp, buffer, size, fp->base + fp->offset);
    if (ret != (int)size) {
		pr_err("read file failed(%d)\n", ret);
        ret = -EIO;
        goto _unlock;
    }
    fp->offset += ret;

_unlock:
    SFILE_MTX_UNLOCK();
    return ret;
}

int usr_sfile_setoffset(struct sfile *fp, off_t offset) {
    int err = 0;
    assert(fp != NULL);
    assert(fp->dp != NULL);
    
    SFILE_MTX_LOCK();
    if (offset > (long)fp->dp->len) {
        err = -EINVAL;
        goto _unlock;
    }
	pr_dbg("set usrfile(%s) offset(%d)\n", fp->pname, offset);
    fp->offset = (long)offset;
    
_unlock:
    SFILE_MTX_UNLOCK();
    return err;
}

size_t usr_sfile_size(struct sfile *fp) {
    assert(fp != NULL);
    assert(fp->dp != NULL);
    
    SFILE_MTX_LOCK();
    size_t size = fp->fsize;
    SFILE_MTX_UNLOCK();
    return size;
}

int usr_sfile_close(struct sfile *fp) {
    int err = 0;
    assert(fp != NULL);
    assert(fp->dp != NULL);
    SFILE_MTX_LOCK();
    if (fp->refcnt > 0) {
        fp->refcnt--;
        if (!fp->refcnt) {
            if (fp->dirty) {
                fp->dirty = false;
                fp->metadata.len = fp->fsize;
                fp->metadata.crc = usr_sfile_chksum(fp, fp->pname);
				pr_dbg("close usrfile(%s) size(%d) crc(0x%08x)\n", fp->pname, 
					fp->metadata.len, fp->metadata.crc);
                err = sfile_write(fp, &fp->metadata, 
                    sizeof(fp->metadata), 0);
                if (err <= 0) {
					pr_err("write usrfile metadata error(%d)\n", err);
                    err = -EIO;
                    goto _unlock;
                }
            }
            const char *name = fp->pname;
            err = sfile_close(fp);
            pr_dbg("close usrfile(%s) with error(%d)\n", name, err);
            (void) name;
        }
    }
_unlock:
    SFILE_MTX_UNLOCK();
    return err;
}

int usr_sfile_init(void) {
    os_mtx_init(&sfile_mtx, 0);
    return 0;
}

