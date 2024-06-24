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

#include "basework/dev/ptfs.h"
#include "basework/ilog2.h"
#include "basework/dev/disk.h"
#include "basework/dev/blkdev.h"
#include "basework/bitops.h"
#include "basework/log.h"
#include "basework/malloc.h"
#include "basework/minmax.h"
#include "basework/system.h"
#include "basework/generic.h"


struct ptfs_file {
    struct file_metadata *pmeta;
    uint32_t rawofs;
    int oflags;
    bool written;
};

struct ptfs_observer {
    struct observer_base base;
    struct ptfs_context *ctx;
};

#define PTFS_SYNC_PERIOD 3000
#define PTFS_INVALID_IDX 0
#define PTFS_INODE_MIN  \
    ((CONFIG_PTFS_INODE_SIZE + CONFIG_PTFS_BLKSIZE - 1) / CONFIG_PTFS_BLKSIZE)
#define BUILD_NAME(_a, _b, _c, _d) \
    (((uint32_t)(_d) << 24) | \
     ((uint32_t)(_c) << 16) | \
     ((uint32_t)(_b) << 8)  | \
     ((uint32_t)(_a) << 0))

#define PFLILE_O_FLAGS  (0x1 << 16)
#define IDX_CONVERT(_idx) ((_idx) - 1)
#define MTX_LOCK(_mtx)   (void)os_mtx_lock(&_mtx)
#define MTX_UNLOCK(_mtx) (void)os_mtx_unlock(&_mtx)
#define MTX_INIT(_mtx)   (void)os_mtx_init(&_mtx, 0)


_Static_assert(PTFS_MAX_INODES >= CONFIG_PTFS_MAXFILES, "");
_Static_assert(!(CONFIG_PTFS_BLKSIZE & (CONFIG_PTFS_BLKSIZE - 1)), "");
_Static_assert(CONFIG_PTFS_INODE_SIZE < CONFIG_PTFS_BLKSIZE, "");

static inline void do_dirty(struct ptfs_context *ctx, bool force) {
    if (force)
        os_timer_mod(ctx->timer, PTFS_SYNC_PERIOD);
}

static inline uint32_t idx_to_offset(struct ptfs_context *ctx, 
    uint32_t idx, uint32_t *end) {
    uint32_t start = ctx->offset + (idx << const_ilog2(CONFIG_PTFS_BLKSIZE));
    *end = start + CONFIG_PTFS_BLKSIZE;
    return start;
}

static uint32_t extofs_to_inofs(struct ptfs_context *ctx, struct ptfs_file *filp, 
    uint32_t offset, uint32_t *end) {
    uint8_t idx = offset >> const_ilog2(CONFIG_PTFS_BLKSIZE);
    uint32_t ofs = offset & (CONFIG_PTFS_BLKSIZE - 1);
    uint32_t start = idx_to_offset(ctx, IDX_CONVERT(filp->pmeta->i_frag[idx]), end);
    pr_dbg("## extofs_to_inofs: idx(%d) start(%x) end(%x) offset(%d)\n", idx, start, *end, offset);
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
    assert(idx > 0);
    assert(idx <= (int)(n << 5));
    /* Remove from bitmap */
    idx -= 1;
    bitmap[(size_t)idx >> 5] &= ~BIT((idx & 31));
    return 0;
}

static inline int inode_allocate(struct ptfs_inode *inode) {
    return bitmap_allocate(inode->i_bitmap, 
        rte_array_size(inode->i_bitmap));
}

static inline int inode_free(struct ptfs_inode *inode, int idx) {
    return bitmap_free(inode->i_bitmap, 
        rte_array_size(inode->i_bitmap), idx);
}

static inline int fnode_allocate(struct ptfs_inode *inode) {
    return bitmap_allocate(inode->f_bitmap, 
        rte_array_size(inode->f_bitmap));
}

static inline int fnode_free(struct ptfs_inode *inode, int idx) {
    return bitmap_free(inode->f_bitmap, 
        rte_array_size(inode->f_bitmap), idx);
}

static void file_metadata_clear_locked(struct ptfs_context *ctx, 
    struct file_metadata *pmeta) {
    for (int i = 0; i < (int)pmeta->i_count; i++)
        inode_free(&ctx->inode, pmeta->i_frag[i]);
    fnode_free(&ctx->inode, pmeta->i_meta);
    memset(pmeta, 0, sizeof(*pmeta));
    ctx->dirty = true;
}

//TODO: optimize performance
static struct file_metadata *file_search(struct ptfs_inode *inode, 
    const char *name) {
    for (int i = 0; i < (int)rte_array_size(inode->meta); i++) {
        if (inode->meta[i].name[0]) {
            if (!strcmp(inode->meta[i].name, name))
                return &inode->meta[i];
        }
    }
    return NULL;
}

static struct ptfs_file *ptfile_allocate(struct ptfs_context *ctx) {
    struct ptfs_file *filp;

    filp = general_malloc(sizeof(*filp));
    if (!filp) {
        pr_err("allocate file handle failed\n");
        return NULL;
    }

    filp->pmeta = NULL;
    filp->rawofs = 0;
    return filp;
}

static int curr_avaliable_space(struct ptfs_context *ctx, 
    struct ptfs_file *filp, uint32_t offset, 
    uint32_t *start, uint32_t *end) {
    struct file_metadata *pmeta = filp->pmeta;
    uint16_t idx = offset >> const_ilog2(CONFIG_PTFS_BLKSIZE);

    if (idx + 1 > pmeta->i_count) {
        uint16_t newidx = (uint16_t)inode_allocate(&ctx->inode);
        if (!newidx) {
            pr_err("allocate inode failed\n");
            return -ENODATA;
        }
        assert(pmeta->i_count < PTFS_MAX_INODES);
        pmeta->i_frag[pmeta->i_count++] = newidx;
    }
    *start = extofs_to_inofs(ctx, filp, offset, end);
    pr_dbg("## curr_avaliable_space: idx(%d) offset(%d) i_count(%d) start(0x%x) end(0x%x)\n", 
        idx, offset, pmeta->i_count, *start, *end);
    return 0;
}

struct ptfs_file *ptfile_ll_open(struct ptfs_context *ctx,
    const char *name, int mode) {
    struct file_metadata *pmeta;
    struct ptfs_file *filp;
    int fidx;

    filp = ptfile_allocate(ctx);
    if (!filp)
        return NULL;

    MTX_LOCK(ctx->mtx);
    pmeta = file_search(&ctx->inode, name);
    if (!pmeta) {
        if (!(mode & VFS_O_CREAT))
            goto _free_fp;

        fidx = fnode_allocate(&ctx->inode);
        if (!fidx) {
            pr_err("allocate file node failed\n");
            goto _free_fp;
        }
        if (fidx > CONFIG_PTFS_MAXFILES) {
            pr_err("too many files\n");
            fnode_free(&ctx->inode, fidx);
            goto _free_fp;
        }

        pmeta = ctx->inode.meta + IDX_CONVERT(fidx);
        memset(pmeta, 0, sizeof(*filp->pmeta));
        pmeta->i_meta = fidx;
        strncpy(pmeta->name, name, MAX_PTFS_FILENAME-1);
    }

    filp->pmeta = pmeta;
    filp->oflags = mode & VFS_O_MASK;
    filp->oflags |= PFLILE_O_FLAGS;
    filp->written = false;
    MTX_UNLOCK(ctx->mtx);
    return filp;

_free_fp:
    general_free(filp);
    MTX_UNLOCK(ctx->mtx);
    return NULL;
}

ssize_t ptfile_ll_read(struct ptfs_context *ctx, struct ptfs_file *filp, 
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
            ret = disk_device_read(ctx->dd, pbuffer, bytes, start);
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

ssize_t ptfile_ll_write(struct ptfs_context *ctx, struct ptfs_file *filp, 
    const void *buffer, size_t size) {
    const char *pbuffer = buffer;
    size_t osize = size;
    uint32_t offset, start;
    uint32_t end;
    int ret;

    MTX_LOCK(ctx->mtx);
    if ((filp->oflags & VFS_O_MASK) == VFS_O_RDONLY) {
        ret = -EACCES;
        goto _unlock;
    }

    offset = filp->rawofs;
    ret = curr_avaliable_space(ctx, filp, offset, &start, &end);
    if (ret < 0)
        goto _unlock;

    while (size > 0) {
        size_t bytes = rte_min_t(size_t, end - start, size);
        if (bytes > 0) {
            ret = blkdev_write(ctx->dd, pbuffer, bytes, start);
            if (ret < 0) {
                filp->rawofs = 0;
                file_metadata_clear_locked(ctx, filp->pmeta);
                goto _unlock;
            }
            size -= bytes;
            pbuffer += bytes;
            start += bytes;
            offset += bytes;
        } else {
            ret = curr_avaliable_space(ctx, filp, offset, &start, &end);
            if (ret < 0) {
                filp->rawofs = 0;
                file_metadata_clear_locked(ctx, filp->pmeta);
                goto _unlock;
            }
        }
    }
    filp->pmeta->size = offset;
    filp->rawofs = offset;
    filp->written = true;
    if (!ctx->dirty)
        ctx->dirty = true;

    ret = osize - size;

_unlock:
    MTX_UNLOCK(ctx->mtx);
    return ret;
}

int ptfile_ll_close(struct ptfs_context *ctx, struct ptfs_file *filp) {
	int err = 0;
	
    if (!ctx || !filp)
        return -EINVAL;

	MTX_LOCK(ctx->mtx);
    if (!(filp->oflags & PFLILE_O_FLAGS)) {
        err = -EINVAL;
		goto _unlock;
    }

    filp->oflags &= ~PFLILE_O_FLAGS;
    do_dirty(ctx, ctx->dirty);

    if (filp->written)
        blkdev_sync();

    general_free(filp);
_unlock:
	MTX_UNLOCK(ctx->mtx);
    return err;
}

int ptfile_ll_seek(struct ptfs_context *ctx, struct ptfs_file *filp, 
    off_t offset, int whence) {

    MTX_LOCK(ctx->mtx);
    switch (whence) {
    case VFS_SEEK_SET:
        if (offset > (off_t)filp->pmeta->size)
            return -EINVAL;
        filp->rawofs = offset;
        break;
    case VFS_SEEK_CUR:
        if (filp->rawofs + offset > (uint32_t)filp->pmeta->size)
            return -EINVAL;
        filp->rawofs += offset;
        break;
    case VFS_SEEK_END:
        if (offset > 0)
            return -EINVAL;
        filp->rawofs = filp->pmeta->size;
        break;
    }

    MTX_UNLOCK(ctx->mtx);
    return 0;
}

int ptfile_ll_unlink(struct ptfs_context *ctx, const char *name) {
    struct file_metadata *pmeta;
	int err;

    if (!ctx || !name)
        return -EINVAL;
	
    MTX_LOCK(ctx->mtx);
    pmeta = file_search(&ctx->inode, name);
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

int ptfile_ll_stat(struct ptfs_context *ctx, const char *name, 
    struct vfs_stat *buf) {
    struct file_metadata *pmeta;
    int err = -ENOENT;

    MTX_LOCK(ctx->mtx);
    pmeta = file_search(&ctx->inode, name);
    if (pmeta) {
        buf->st_size = pmeta->size;
        buf->st_blocks = pmeta->i_count;
        buf->st_blksize = CONFIG_PTFS_BLKSIZE;
        err = 0;
    }
    MTX_UNLOCK(ctx->mtx);
    return err;
}

const char *ptfile_get_fname(struct ptfs_context *ctx, int *idx) {
    int size = (int)rte_array_size(ctx->inode.meta);
    const char *fname = NULL;

    if (*idx >= size)
        return NULL;

    MTX_LOCK(ctx->mtx);
    for (int i = *idx; i < size; i++) {
        if (ctx->inode.meta[i].name[0]) {
            *idx = i + 1;
            fname = ctx->inode.meta[i].name;
            break;
        }
    }
    MTX_UNLOCK(ctx->mtx);
    return fname;
}

static void ptfile_sync_check(os_timer_t timer, void *arg) {
    struct ptfs_context *ctx = arg;
    (void) timer;
    if (ctx->dirty) {
        MTX_LOCK(ctx->mtx);
        if (ctx->dirty) {
            ctx->dirty = false;
            blkdev_write(ctx->dd, &ctx->inode, 
                sizeof(ctx->inode), ctx->offset);
        }
        MTX_UNLOCK(ctx->mtx);
    }
}

void ptfile_ll_reset(struct ptfs_context *ctx) {
    MTX_LOCK(ctx->mtx);
    memset(&ctx->inode, 0, sizeof(ctx->inode));
    ctx->inode.magic = MESSAGE_MAGIC;
    for (int i = 0; i < PTFS_INODE_MIN; i++)
        ctx->inode.i_bitmap[i >> 5] |= 1ul << (i & 31);
    ctx->dirty = true;
    do_dirty(ctx, ctx->dirty);
    MTX_UNLOCK(ctx->mtx);
    pr_dbg("inode placehold: %d\n", PTFS_INODE_MIN);
}

static int shutdown_listen(struct observer_base *nb,
	unsigned long action, void *data) {
    struct ptfs_observer *p = (struct ptfs_observer *)nb;
    ptfile_sync_check(NULL, p->ctx);
    return 0;
}

int ptfile_ll_init(struct ptfs_context *ctx, const char *name, 
    uint32_t start) {
    static struct ptfs_observer obs = {
        .base = {
            .update = shutdown_listen,
            .priority = 100
        },
    };
    int err;

    if (!ctx || !name)
        return -EINVAL;

    err = disk_device_open(name, &ctx->dd);
    if (err)
        return err;

    MTX_INIT(ctx->mtx);
    MTX_LOCK(ctx->mtx);
    err = os_timer_create(&ctx->timer, ptfile_sync_check, 
        ctx, false);
    assert(err == 0);
    if (start == UINT32_MAX)
        ctx->offset = ctx->dd->addr;
    else
        ctx->offset = start;

    err = blkdev_read(ctx->dd, &ctx->inode, 
        sizeof(ctx->inode), ctx->offset);
    MTX_UNLOCK(ctx->mtx);
    if (err < 0)
        return err;

    obs.ctx = ctx;
    system_add_observer(&obs.base);
    if (ctx->inode.magic != MESSAGE_MAGIC) {
        pr_warn("PTFS is invalid\n");
        ptfile_ll_reset(ctx);
    }
	
    return 0;
}

