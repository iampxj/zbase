/*
 * Copyright 2023 wtcat
 * 
 * Simple parition filesystem low-level implement
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<ptfs>: "fmt
// #define CONFIG_LOGLEVEL LOGLEVEL_DEBUG
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "basework/dev/ptfs_ext.h"
#include "basework/dev/disk.h"
#include "basework/dev/blkdev.h"
#include "basework/dev/buffer_io.h"
#include "basework/lib/crc.h"
#include "basework/ilog2.h"
#include "basework/bitops.h"
#include "basework/log.h"
#include "basework/malloc.h"
#include "basework/minmax.h"
#include "basework/system.h"
#include "basework/generic.h"
#include "basework/assert.h"

struct file_metadata {
#define MAX_PTFS_FILENAME 64 
    char       name[MAX_PTFS_FILENAME];
    uint32_t   size;
    uint32_t   mtime;
    uint16_t   chksum;
    uint16_t   permision;
    uint8_t    i_meta;
    uint8_t    i_count;
    uint8_t    i_frag[];
};

struct pt_inode {
#define MESSAGE_MAGIC BUILD_NAME('P', 'T', 'F', 'S')
	uint32_t   magic;
    uint32_t   hcrc;
    char       data[] __rte_aligned(sizeof(void *));
};

struct ptfs_observer {
    struct observer_base base;
    struct ptfs_class *ctx;
};

#define PTFS_SYNC_PERIOD (1000ul * 60 * 15)
#define PTFS_INVALID_IDX 0
#define BUILD_NAME(_a, _b, _c, _d) \
    (((uint32_t)(_d) << 24) | \
     ((uint32_t)(_c) << 16) | \
     ((uint32_t)(_b) << 8)  | \
     ((uint32_t)(_a) << 0))

#define PFLILE_O_FLAGS    (0x1 << 16)
#define IDX_CONVERT(_idx) ((_idx) - 1)
#define MTX_LOCK(_mtx)    (void) os_mtx_lock(&_mtx)
#define MTX_UNLOCK(_mtx)  (void) os_mtx_unlock(&_mtx)
#define MTX_INIT(_mtx)    (void) os_mtx_init(&_mtx, 0)

#define PTFS_READ(ctx, buf, size, ofs) \
    ctx->read(ctx, buf, size, ofs)

#define PTFS_WRITE(ctx, buf, size, ofs) \
    ctx->write(ctx, buf, size, ofs)

#define PTFS_SYNC(ctx) \
    ctx->flush(ctx)


static ssize_t ptblk_bread(struct ptfs_class *ctx, void *buf, size_t size, 
    uint32_t offset) {
    return buffered_read(ctx->bio, buf, size, offset);
}

static ssize_t ptblk_bwrite(struct ptfs_class *ctx, const void *buf, size_t size, 
    uint32_t offset) {
    return buffered_write(ctx->bio, buf, size, offset);
}

static int ptblk_bflush(struct ptfs_class *ctx) {
    return buffered_ioflush(ctx->bio, false);
}

static ssize_t ptblk_read(struct ptfs_class *ctx, void *buf, size_t size, 
    uint32_t offset) {
    return blkdev_read(ctx->dd, buf, size, offset);
}

static ssize_t ptblk_write(struct ptfs_class *ctx, const void *buf, size_t size, 
    uint32_t offset) {
    return blkdev_write(ctx->dd, buf, size, offset);
}

static int ptblk_flush(struct ptfs_class *ctx) {
    return blkdev_sync();
}

static uint32_t log2_u32(uint32_t val) {
    if (val == 0)
        return 0;
#if __has_builtin(__builtin_ctz)
    return __builtin_ctz(val);
#else
    for (uint32_t i = 0; i < 32; i++) {
        if (val & BIT(i))
            return i;
    }
    return 0;
#endif
}

static inline void do_dirty(struct ptfs_class *ctx, bool force) {
    if (force)
        os_timer_mod(ctx->timer, PTFS_SYNC_PERIOD);
}

static inline uint32_t idx_to_offset(struct ptfs_class *ctx, 
    uint32_t idx, uint32_t *end) {
    uint32_t start = ctx->offset + (idx << ctx->log2_blksize);
    *end = start + ctx->blksize;
    return start;
}

static uint32_t extofs_to_inofs(struct ptfs_class *ctx, struct pt_file *filp, 
    uint32_t offset, uint32_t *end) {
    uint8_t idx = offset >> ctx->log2_blksize;
    uint32_t ofs = offset & (ctx->blksize - 1);
    uint32_t start = idx_to_offset(ctx, IDX_CONVERT(filp->pmeta->i_frag[idx]), end);
    pr_dbg("## extofs_to_inofs: idx(%d) start(%x) end(%x) offset(%d)\n", 
        idx, start, *end, offset);
    return start + ofs;
}

static int bitmap_allocate(uint32_t *bitmap, size_t n) {
    int index = 0;
    for (size_t i = 0; i < n; i++) {
        uint32_t mask = bitmap[i];
        if (bitmap[i] != UINT32_MAX) {
            uint32_t ofs = ffs(~mask) - 1;
            index += ofs;
            bitmap[i] |= BIT(ofs);
            return index + 1;
        }
        index += 32;
    }
    return PTFS_INVALID_IDX;
}

static int bitmap_free(uint32_t *bitmap, size_t n, int idx) {
    rte_assert(idx > 0);
    rte_assert(idx <= (int)(n << 5));
    /* Remove from bitmap */
    idx -= 1;
    bitmap[(size_t)idx >> 5] &= ~BIT((idx & 31));
    return 0;
}

static inline int inode_allocate(struct ptfs_class *ctx) {
    return bitmap_allocate(ctx->i_bitmap, ctx->i_bitmap_count);
}

static inline int inode_free(struct ptfs_class *ctx, int idx) {
    return bitmap_free(ctx->i_bitmap, ctx->i_bitmap_count, idx);
}

static inline int fnode_allocate(struct ptfs_class *ctx) {
    return bitmap_allocate(ctx->f_bitmap, ctx->f_bitmap_count);
}

static inline int fnode_free(struct ptfs_class *ctx, int idx) {
    return bitmap_free(ctx->f_bitmap, ctx->f_bitmap_count, idx);
}

static void file_metadata_reset_locked(struct ptfs_class *ctx, 
    struct file_metadata *pmeta) {
    for (int i = 0; i < (int)pmeta->i_count; i++) {
        inode_free(ctx, pmeta->i_frag[i]);
        pmeta->i_frag[i] = 0;
    }
    pmeta->i_count = 0;
    pmeta->size = 0;
    pmeta->chksum = 0;
    pmeta->mtime = 0;
    ctx->dirty = true;
}

static void file_metadata_clear_locked(struct ptfs_class *ctx, 
    struct file_metadata *pmeta) {
    for (int i = 0; i < (int)pmeta->i_count; i++)
        inode_free(ctx, pmeta->i_frag[i]);
    fnode_free(ctx, pmeta->i_meta);
    memset(pmeta, 0, sizeof(*pmeta));
    ctx->dirty = true;
}

//TODO: optimize performance
static struct file_metadata *file_search(struct ptfs_class *ctx, 
    const char *name) {
    for (int i = 0; i < (int)ctx->maxfiles; i++) {
        if (ctx->f_meta[i]->name[0]) {
            if (!strcmp(ctx->f_meta[i]->name, name))
                return ctx->f_meta[i];
        }
    }
    return NULL;
}

static int curr_avaliable_space(struct ptfs_class *ctx, 
    struct pt_file *filp, uint32_t offset, 
    uint32_t *start, uint32_t *end) {
    struct file_metadata *pmeta = filp->pmeta;
    uint16_t idx = offset >> ctx->log2_blksize;

    if (idx + 1 > pmeta->i_count) {
        uint16_t newidx = (uint16_t)inode_allocate(ctx);
        if (!newidx) {
            pr_err("allocate inode failed\n");
            return -ENOMEM;
        }
        if (pmeta->i_count >= ctx->inodes) {
            pr_err("The file is too large\n");
            return -E2BIG;
        }
        pmeta->i_frag[pmeta->i_count++] = newidx;
    }
    *start = extofs_to_inofs(ctx, filp, offset, end);
    pr_dbg("%s: idx(%d) offset(%d) i_count(%d) start(0x%x) end(0x%x)\n", 
        __func__, idx, offset, pmeta->i_count, *start, *end);
    return 0;
}

static int file_set_offset(struct ptfs_class *ctx, struct pt_file *filp, 
    uint32_t offset) {
    struct file_metadata *pmeta = filp->pmeta;
    uint16_t count;
    int err;

    if (offset > ctx->inodes * ctx->blksize)
        return -E2BIG;

    count = pmeta->i_count;
    for (uint32_t ofs = pmeta->size; ofs <= offset; ofs += ctx->blksize) {
        uint16_t idx = ofs >> ctx->log2_blksize;
        if (idx + 1 > count) {
            uint16_t newidx = (uint16_t)inode_allocate(ctx);
            if (!newidx) {
                pr_err("allocate inode failed\n");
                err = -ENODATA;
                goto _failed;
            }
            if (count >= ctx->inodes) {
                pr_err("The file is too large\n");
                err = -ENOENT;
                goto _failed;
            }
            pmeta->i_frag[count++] = newidx;
        }
    }

    pmeta->i_count = count;
    filp->rawofs = offset;
    return 0;

_failed:
    while (count > pmeta->i_count) {
        count--;
        inode_free(ctx, pmeta->i_frag[count]);
    }
    return err;
}

static uint32_t file_checksum(struct ptfs_class *ctx) {
    const struct pt_inode *inode = ctx->p_inode;
    return lib_crc32((const uint8_t *)inode->data, 
                ctx->p_inode_size - offsetof(struct pt_inode, data));
}

int pt_file_open(struct ptfs_class *ctx, struct pt_file *filp,
    const char *name, int mode) {
    struct file_metadata *pmeta;
    int err = 0;
    int fidx;

    MTX_LOCK(ctx->mtx);
    pmeta = file_search(ctx, name);
    if (!pmeta) {
        if (!(mode & VFS_O_CREAT)) {
            err = -EPERM;
            goto _unlock;
        }

        fidx = fnode_allocate(ctx);
        if (!fidx) {
            pr_err("allocate file node failed\n");
            err = -ENOMEM;
            goto _unlock;
        }
        if (fidx > (int)ctx->maxfiles) {
            pr_err("too many files\n");
            fnode_free(ctx, fidx);
            err = -ENFILE;
            goto _unlock;
        }

        pmeta = ctx->f_meta[IDX_CONVERT(fidx)];
        memset(pmeta, 0, sizeof(*filp->pmeta));
        pmeta->i_meta = fidx;
        strncpy(pmeta->name, name, MAX_PTFS_FILENAME-1);
    } else {
        if ((mode & VFS_O_MASK) == VFS_O_WRONLY) {
            if (!(mode & VFS_O_APPEND))
                file_metadata_reset_locked(ctx, pmeta);
            else
                filp->rawofs = pmeta->size;
        }
    }

    filp->rawofs = 0;
    filp->pmeta = pmeta;
    filp->oflags = mode & VFS_O_MASK;
    filp->oflags |= PFLILE_O_FLAGS;
    filp->written = false;

_unlock:
    MTX_UNLOCK(ctx->mtx);
    return err;
}

ssize_t pt_file_read(struct ptfs_class *ctx, struct pt_file *filp, 
    void *buffer, size_t size) {
    char *pbuffer = buffer;
    uint32_t offset, start;
    size_t osize;
    uint32_t end;
    int ret;

    MTX_LOCK(ctx->mtx);
    if ((filp->oflags & VFS_O_MASK) == VFS_O_WRONLY) {
        ret = -EACCES;
        goto _unlock;
    }

    offset = filp->rawofs;
    osize = size = rte_min_t(size_t, size, filp->pmeta->size - offset);
    start = extofs_to_inofs(ctx, filp, offset, &end);
    while (size > 0) {
        size_t bytes = rte_min_t(size_t, end - start, size);
        if (bytes > 0) {
            ret = PTFS_READ(ctx, pbuffer, bytes, start);
            if (ret < 0)
                goto _unlock;

            size -= bytes;
            pbuffer += bytes;
            start += bytes;
            offset += bytes;
        } else {
            start = extofs_to_inofs(ctx, filp, offset, &end);
        }
    }
    filp->rawofs = offset;
    ret = osize - size;
_unlock:
    MTX_UNLOCK(ctx->mtx);
    return ret;
}

ssize_t pt_file_write(struct ptfs_class *ctx, struct pt_file *filp, 
    const void *buffer, size_t size) {
    struct file_metadata *pmeta;
    const char *pbuffer = buffer;
    size_t osize = size;
    uint32_t offset, start;
    uint32_t i_count, i_offset;
    uint32_t end;
    int ret;

    MTX_LOCK(ctx->mtx);
    if ((filp->oflags & VFS_O_MASK) == VFS_O_RDONLY) {
        ret = -EACCES;
        goto _unlock;
    }

    pmeta    = filp->pmeta;
    i_count  = pmeta->i_count;
    i_offset = filp->rawofs;
    offset   = i_offset;
    ret = curr_avaliable_space(ctx, filp, offset, &start, &end);
    if (ret < 0)
        goto _unlock;

    while (size > 0) {
        size_t bytes = rte_min_t(size_t, end - start, size);
        if (bytes > 0) {
            ret = PTFS_WRITE(ctx, pbuffer, bytes, start);
            if (ret < 0) {
                filp->rawofs = 0;
                file_metadata_clear_locked(ctx, pmeta);
                goto _unlock;
            }
            size    -= bytes;
            pbuffer += bytes;
            start   += bytes;
            offset  += bytes;
        } else {
            ret = curr_avaliable_space(ctx, filp, offset, &start, &end);
            if (ret < 0) {
                /*
                 * If there is no free space, the initialize state is restored
                 */
                filp->rawofs = i_offset;
                while (pmeta->i_count > i_count) {
                    pmeta->i_count--;
                    inode_free(ctx, pmeta->i_frag[pmeta->i_count]);
                }
                goto _unlock;
            }
        }
    }

    if (pmeta->size < offset)
        pmeta->size = offset;

    filp->rawofs  = offset;
    filp->written = true;
    if (!ctx->dirty)
        ctx->dirty = true;

    ret = osize - size;

_unlock:
    MTX_UNLOCK(ctx->mtx);
    return ret;
}

int pt_file_close(struct ptfs_class *ctx, struct pt_file *filp) {
	int err = 0;
	
    if (!ctx || !filp)
        return -EINVAL;

	MTX_LOCK(ctx->mtx);
    if (!(filp->oflags & PFLILE_O_FLAGS)) {
        err = -EINVAL;
		goto _unlock;
    }

    filp->oflags &= ~PFLILE_O_FLAGS;
    if (filp->pmeta->size == 0)
        file_metadata_clear_locked(ctx, filp->pmeta);

    do_dirty(ctx, ctx->dirty);
    if (filp->written)
        PTFS_SYNC(ctx);

_unlock:
	MTX_UNLOCK(ctx->mtx);
    return err;
}

int pt_file_seek(struct ptfs_class *ctx, struct pt_file *filp, 
    off_t offset, int whence) {
    int err = -EINVAL;

    MTX_LOCK(ctx->mtx);
    switch (whence) {
    case VFS_SEEK_SET:
        err = file_set_offset(ctx, filp, offset);
        break;
    case VFS_SEEK_CUR:
        err = file_set_offset(ctx, filp, filp->rawofs + offset);
        break;
    case VFS_SEEK_END:
        err = file_set_offset(ctx, filp, filp->pmeta->size + offset);
        break;
    }

    MTX_UNLOCK(ctx->mtx);
    return err;
}

int pt_file_unlink(struct ptfs_class *ctx, const char *name) {
    struct file_metadata *pmeta;
	int err;

    if (!ctx || !name)
        return -EINVAL;
	
    MTX_LOCK(ctx->mtx);
    pmeta = file_search(ctx, name);
    if (pmeta) {
        file_metadata_clear_locked(ctx, pmeta);
        do_dirty(ctx, ctx->dirty);
        err = 0;
        goto _unlock;
    }
	err = -ENODATA;
	
_unlock:
    MTX_UNLOCK(ctx->mtx);
    return err;
}

int pt_file_stat(struct ptfs_class *ctx, const char *name, 
    struct vfs_stat *buf) {
    struct file_metadata *pmeta;
    int err = -ENOENT;

    MTX_LOCK(ctx->mtx);
    pmeta = file_search(ctx, name);
    if (pmeta) {
        buf->st_size    = pmeta->size;
        buf->st_blocks  = pmeta->i_count;
        buf->st_blksize = ctx->blksize;
        err = 0;
    }
    MTX_UNLOCK(ctx->mtx);
    return err;
}

const char *pt_file_getname(struct ptfs_class *ctx, int *idx) {
    int size = (int)ctx->maxfiles;
    const char *fname = NULL;

    if (*idx >= size)
        return NULL;

    MTX_LOCK(ctx->mtx);
    for (int i = *idx; i < size; i++) {
        if (ctx->f_meta[i]->name[0]) {
            *idx = i + 1;
            fname = ctx->f_meta[i]->name;
            break;
        }
    }
    MTX_UNLOCK(ctx->mtx);
    return fname;
}

static void pt_file_sync(os_timer_t timer, void *arg) {
    struct ptfs_class *ctx = arg;
    (void) timer;

    if (ctx->dirty) {
        MTX_LOCK(ctx->mtx);
        if (ctx->dirty) {
            ctx->dirty = false;
            ctx->p_inode->hcrc = file_checksum(ctx);
            PTFS_WRITE(ctx, ctx->p_inode, ctx->p_inode_size, ctx->offset);
            PTFS_SYNC(ctx);
        }
        MTX_UNLOCK(ctx->mtx);
    }
}

void pt_file_reset(struct ptfs_class *ctx) {
    int nbits = rte_div_roundup(ctx->p_inode_size, ctx->blksize);;

    MTX_LOCK(ctx->mtx);
    memset(ctx->p_inode, 0, ctx->p_inode_size);
    ctx->p_inode->magic = MESSAGE_MAGIC;

    for (int i = 0; i < nbits; i++)
        ctx->i_bitmap[i >> 5] |= 1ul << (i & 31);

    ctx->dirty = true;
    do_dirty(ctx, ctx->dirty);
    MTX_UNLOCK(ctx->mtx);
    pr_dbg("inode placehold: %d\n", (int)ctx->inodes);
}

static int shutdown_listen(struct observer_base *nb,
	unsigned long action, void *data) {
    struct ptfs_observer *p = (struct ptfs_observer *)nb;
    pt_file_sync(NULL, p->ctx);
    return 0;
}

int pt_file_init(struct ptfs_class *ctx, const char *name, uint32_t start, 
    size_t size, size_t blksize, size_t maxfiles, uint32_t maxlimit, bool bio) {
    static struct ptfs_observer obs = {
        .base = {
            .update = shutdown_listen,
            .priority = 100
        },
    };
    size_t alloc_size;
    size_t slot_size;
    size_t fsize;
    char *buffer;
    int err;

    if (!ctx || !name)
        return -EINVAL;

    if (blksize & (blksize - 1)) {
        pr_err("The blocksize must be the power of 2\n");
        return -EINVAL;
    }

    if (size < 2*blksize || size / blksize > UINT8_MAX) {
        pr_err("The size and blocksize are invalid\n");
        return -EINVAL;
    }

    if (maxlimit == 0)
        maxlimit = UINT32_MAX;

    err = disk_device_open(name, &ctx->dd);
    if (err)
        return err;

    MTX_INIT(ctx->mtx);
    MTX_LOCK(ctx->mtx);
    err = os_timer_create(&ctx->timer, pt_file_sync, 
        ctx, false);
    rte_assert(err == 0);

    if (bio) {
        size_t devblksz = disk_device_get_block_size(ctx->dd);
        err = buffered_iocreate(ctx->dd, devblksz, false, &ctx->bio);
        if (err) {
            pr_err("Create buffered-I/O failed(%d)\n", err);
            return err;
        }
        ctx->read       = ptblk_bread;
        ctx->write      = ptblk_bwrite;
        ctx->flush      = ptblk_bflush;
    } else {
        ctx->read       = ptblk_read;
        ctx->write      = ptblk_write;
        ctx->flush      = ptblk_flush;
    }

    if (start == UINT32_MAX)
        ctx->offset = ctx->dd->addr;
    else
        ctx->offset = start;

    ctx->size           = size;
    ctx->inodes         = rte_min(size / blksize, maxlimit / blksize + 1);
    ctx->blksize        = blksize;
    ctx->maxfiles       = maxfiles;
    ctx->log2_blksize   = log2_u32(blksize);
    ctx->i_bitmap_count = (ctx->inodes + 31) / 32;
    ctx->f_bitmap_count = (ctx->maxfiles + 31) / 32;

    slot_size  = sizeof(struct file_metadata *) * maxfiles;
    alloc_size = sizeof(struct pt_inode) + 
                (ctx->i_bitmap_count + ctx->f_bitmap_count) * sizeof(uint32_t);
    alloc_size = RTE_ALIGN(alloc_size, sizeof(void *));
    fsize = sizeof(struct file_metadata) + (ctx->inodes * sizeof(uint8_t));
    fsize = RTE_ALIGN(fsize, sizeof(void *));

    buffer = general_calloc(1, slot_size + alloc_size + fsize * maxfiles);
    if (buffer == NULL)
        return -ENOMEM;

    ctx->p_inode_size  = alloc_size + fsize * maxfiles;
    ctx->buffer        = buffer;
    ctx->f_meta        = (struct file_metadata **)buffer;
    ctx->p_inode       = (struct pt_inode *)(buffer + slot_size);
    ctx->i_bitmap      = (uint32_t *)ctx->p_inode->data;
    ctx->f_bitmap      = ctx->i_bitmap + ctx->i_bitmap_count;
    buffer             = buffer + slot_size + alloc_size;

    for (size_t i = 0; i < maxfiles; i++) {
        ctx->f_meta[i] =  (struct file_metadata *)buffer;
        buffer         += fsize;
    }

    pr_info("Dump PTFS information:\n"
        "\tstart(0x%x) size(0x%x) blocksize(%d) header_size(%d) maxfiles(%d) inodes(%d)\n",
        start, size, blksize, ctx->p_inode_size, maxfiles, ctx->inodes);
        
    err = PTFS_READ(ctx, ctx->p_inode, 
        ctx->p_inode_size, ctx->offset);
    MTX_UNLOCK(ctx->mtx);
    if (err < 0) {
        pr_err("Read PTFS header failed\n");
        general_free(buffer);
        return err;
    }

    obs.ctx = ctx;
    system_add_observer(&obs.base);
    if (ctx->p_inode->magic != MESSAGE_MAGIC || 
        ctx->p_inode->hcrc != file_checksum(ctx)) {
        pr_warn("PTFS is invalid!(magic:0x%08x hcrc:0x%08x)\n", 
            ctx->p_inode->magic, ctx->p_inode->hcrc);
        pt_file_reset(ctx);
    }
	
    return 0;
}
