/*
 * Copyright 2024 wtcat
 * 
 * XIP filesystem implement
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<xipfs>: "fmt
#define CONFIG_LOGLEVEL LOGLEVEL_DEBUG
#define USE_XIPFS_DUMP

#include <errno.h>
#include <stdint.h>
#include <string.h>

#include "basework/dev/xipfs.h"
#include "basework/ilog2.h"
#include "basework/dev/disk.h"
#include "basework/dev/blkdev.h"
#include "basework/bitops.h"
#include "basework/log.h"
#include "basework/malloc.h"
#include "basework/minmax.h"
#include "basework/system.h"
#include "basework/generic.h"


struct xip_file {
    uint32_t rawofs;
    int oflags;
    uint16_t req_blksz;
    union {
        uint16_t idx_offset;
        int16_t  i16_idx_offset;
    };
	bool used;
	bool dirty;
    struct fmeta_data *pmeta;
};

struct xipfs_observer {
    struct observer_base base;
    struct xipfs_context *ctx;
};

struct free_area {
    uint16_t blkofs;
    uint16_t blknum;
};

struct scan_result {
    struct free_area areas[CONFIG_XIPFS_MAXFILES + 1];
    uint16_t count;
};

#define XIPFS_MAGIC BUILD_NAME('X', 'P', 'F', 'S')
#define BUILD_NAME(_a, _b, _c, _d) \
    (((uint32_t)(_d) << 24) | \
     ((uint32_t)(_c) << 16) | \
     ((uint32_t)(_b) << 8)  | \
     ((uint32_t)(_a) << 0))

#define XIPFS_MAXFILE_OPENED CONFIG_XIPFS_MAXFILES
#define XIPFS_SYNC_PERIOD    (10000) //10s

#define PFLILE_O_FLAGS  (0x1 << 16)
#define IDX_CONVERT(_idx) ((_idx))
#define MTX_LOCK(_mtx)   (void)os_mtx_lock(_mtx)
#define MTX_UNLOCK(_mtx) (void)os_mtx_unlock(_mtx)
#define MTX_INIT(_mtx)   (void)os_mtx_init(_mtx, 0)

#if defined(__linux__) || \
    defined(_WIN32) || \
    CONFIG_PSRAM_SIZE > 0
# define cache_blk_alloc(size) general_malloc(size)
# define cache_blk_free(ptr)   general_free(ptr)
# else
# define cache_blk_alloc(size) (void *)0x2FF50000
# define cache_blk_free(ptr)   (void)ptr
#endif

_Static_assert(XIPFS_MAX_INODES >= CONFIG_XIPFS_MAXFILES, "");
_Static_assert(!(CONFIG_XIPFS_BLKSIZE & (CONFIG_XIPFS_BLKSIZE - 1)), "");

static int check_free_space(struct xipfs_context *ctx, size_t reqblksz);

static struct xip_file fds_table[XIPFS_MAXFILE_OPENED];

#ifdef USE_XIPFS_DUMP
static void xipfs_file_dump(void) {
	pr_out("Opened files dump:\n");
	for (size_t i = 0; i < rte_array_size(fds_table); i++) {
		if (fds_table[i].used)
			pr_out("\t {filename: %s, size: %d}\n", 
				fds_table[i].pmeta->name,
				fds_table[i].pmeta->size
			);
	}
}

static void xipfs_dump(const struct xipfs_inode *inode) {
	pr_out("XIPFS information:\n");
	pr_out("\tmagic:  0x%08x\n", inode->magic);
	pr_out("\tnfiles: %d\n", inode->nfiles);
	pr_out("\thcrc:   0x%x\n", inode->hcrc);
	for (size_t i = 0; i < XIPFS_IMAP_SIZE; i++)
		pr_out("\tbitmap[0] = 0x%08x\n", inode->i_bitmap[i]);
	for (size_t i = 0; i < CONFIG_XIPFS_MAXFILES; i++) {
		const struct fmeta_data *p = &inode->meta[i];
		if (inode->meta[i].name[0]) {
			pr_out("\n\t" 
				"{name: %s, size: %d, blkofs: %d, blknum: %d, used: %d, chksum: %x",
				p->name, 
				p->size, 
				(int)p->blkofs, 
				(int)p->blknum, 
				(int)p->used, 
				(int)p->chksum
			);
		}
	}
	pr_out("\n\n");
	xipfs_file_dump();
}

#else
#define xipfs_dump(...)       (void)0
#define xipfs_file_dump(...)  (void)0
#endif

static inline void delayed_sync(struct xipfs_context *ctx, bool force) {
    if (force)
        os_timer_mod(ctx->timer, XIPFS_SYNC_PERIOD);
}

static inline uint32_t idx_to_offset(struct xipfs_context *ctx, 
    uint32_t idx, uint32_t *end) {
    uint32_t start = ctx->offset + (idx << const_ilog2(CONFIG_XIPFS_BLKSIZE));
    *end = start + CONFIG_XIPFS_BLKSIZE;
    return start;
}

static uint32_t extofs_to_inofs(struct xipfs_context *ctx, 
	struct fmeta_data *pmeta, uint32_t offset, uint32_t *end) {
    uint8_t idx = offset >> const_ilog2(CONFIG_XIPFS_BLKSIZE);
    assert(idx < XIPFS_MAX_INODES);

    uint32_t ofs = offset & (CONFIG_XIPFS_BLKSIZE - 1);
    uint32_t start = idx_to_offset(ctx,
        IDX_CONVERT(pmeta->blkofs + idx), end);

    pr_dbg("%s idx(%d) start(%x) offset(%d)\n", 
        __func__, idx, start, offset);
    return start + ofs;
}

static inline void bitvect_set(uint32_t *bitmap, uint32_t idx) {
    bitmap[idx >> 5] |= BIT((idx & 31));
}

static inline void bitvect_clear(uint32_t *bitmap, uint32_t idx) {
    bitmap[idx >> 5] &= ~BIT((idx & 31));
}

static void bitvec_move(uint32_t *bitmap, uint32_t dstidx, 
    uint32_t srcidx) {
    bitvect_clear(bitmap, srcidx);
    bitvect_set(bitmap, dstidx);
}

static int idx_alloc(uint32_t *bitmap, size_t n, uint32_t offset) {
    uint32_t ofs = offset & 31;
    for (uint32_t index = 0, i = offset >> 5; i < n; i++) {
        while (ofs < 32) {
            if (!(bitmap[i] & BIT(ofs))) {
                bitmap[i] |= BIT(ofs);
                index = (i << 5) + ofs;
                return index;
            }
            ofs++;
        }
        ofs = 0;
    }
    return -1;
}

static int idx_free(uint32_t *bitmap, size_t n, int idx) {
    assert(idx >= 0);
    assert(idx <= (int)(n << 5));
    /* Remove from bitmap */
    bitmap[(size_t)idx >> 5] &= ~BIT((idx & 31));
    return 0;
}

static inline int inode_alloc(struct xipfs_inode *inode, uint32_t idxofs) {
    return idx_alloc(inode->i_bitmap, 
        rte_array_size(inode->i_bitmap), idxofs);
}

static inline int inode_free(struct xipfs_inode *inode, int idx) {
    return idx_free(inode->i_bitmap, 
        rte_array_size(inode->i_bitmap), idx);
}

static void file_metadata_clear_locked(struct xipfs_context *ctx, 
    struct fmeta_data *pmeta) {
    for (int i = 0; i < (int)pmeta->blknum; i++)
        inode_free(&ctx->inode, pmeta->blkofs + i);
    memset(pmeta, 0, sizeof(*pmeta));
    ctx->dirty = true;
}

//TODO: optimize performance
static struct fmeta_data *file_search(struct xipfs_inode *inode, 
    const char *name) {
    for (int i = 0; i < (int)rte_array_size(inode->meta); i++) {
        if (inode->meta[i].name[0] != '\0') {
            if (!strcmp(inode->meta[i].name, name))
                return &inode->meta[i];
        }
    }
    return NULL;
}

static struct fmeta_data *file_meta_alloc(struct xipfs_inode *inode) {
    for (size_t i = 0; i < rte_array_size(inode->meta); i++) {
        if (!inode->meta[i].used) {
            memset(&inode->meta[i], 0, sizeof(struct fmeta_data));
            inode->meta[i].used = true;
            return &inode->meta[i];
        }
    }
    return NULL;
}

static struct xip_file *file_alloc(struct xipfs_context *ctx) {
	for (size_t i = 0; i < rte_array_size(fds_table); i++) {
		struct xip_file *filp = &fds_table[i];
		if (!filp->used) {
			memset(filp, 0, sizeof(*filp));
			filp->used = true;
			return filp;
		}
	}

    return NULL;
}

static void file_free(struct xip_file *filp) {
	filp->used = false;
}

static int curr_avaliable_space(struct xipfs_context *ctx, 
    struct xip_file *filp, uint32_t offset, 
    uint32_t *start, uint32_t *end) {
    struct fmeta_data *pmeta = filp->pmeta;
    uint16_t idx = offset >> const_ilog2(CONFIG_XIPFS_BLKSIZE);

    if (idx + 1 > pmeta->blknum) {
        int newidx = inode_alloc(&ctx->inode, 
            filp->idx_offset + pmeta->blknum);
        if (newidx < 0) {
            pr_err("allocate inode failed\n");
            return -ENODATA;
        }

        if (rte_unlikely(pmeta->blknum == 0))
            pmeta->blkofs = (uint16_t)newidx;

        /* 
         * Make sure indexes are consecutive in the same file
         */
        assert((pmeta->blkofs + pmeta->blknum) == (uint16_t)newidx);
        assert(pmeta->blknum < XIPFS_MAX_INODES);
        pmeta->blknum++;

		pr_dbg("%s allocate new index(%d) blkofs(%d) blknum(%d)\n", 
			__func__, newidx, pmeta->blkofs, pmeta->blknum);
    }

    *start = extofs_to_inofs(ctx, pmeta, offset, end);
    pr_dbg("%s idx(%d) offset(%d) blknum(%d) start(0x%x) end(0x%x)\n", 
        __func__, (uint32_t)idx, offset, (uint32_t)pmeta->blknum, *start, *end);
    return 0;
}

struct xip_file *fxip_ll_open(struct xipfs_context *ctx,
    const char *name, int mode) {
    struct fmeta_data *pmeta;
    struct xip_file *filp;

    MTX_LOCK(&ctx->mtx);
	
	pr_dbg("%s open %s\n", __func__, name);
	xipfs_dump(&ctx->inode);
	
    filp = file_alloc(ctx);
    if (!filp) {
        pr_err("allocate file handle failed\n");
		xipfs_file_dump();
        goto _unlock;
    }

    pmeta = file_search(&ctx->inode, name);
    if (!pmeta) {
        if (!(mode & VFS_O_CREAT))
            goto _free_fp;

        pmeta = file_meta_alloc(&ctx->inode);
        if (pmeta == NULL) {
            pr_err("too many files\n");
            goto _free_fp;
        }
        
        strncpy(pmeta->name, name, MAX_XIPFS_FILENAME-1);
    }

    MTX_UNLOCK(&ctx->mtx);
    filp->pmeta = pmeta;
    filp->oflags = mode & VFS_O_MASK;
    filp->oflags |= PFLILE_O_FLAGS;
    return filp;

_free_fp:
    file_free(filp);
_unlock:
    MTX_UNLOCK(&ctx->mtx);
    return NULL;
}

ssize_t fxip_ll_read(struct xipfs_context *ctx, struct xip_file *filp, 
    void *buffer, size_t size) {
    char *pbuffer = buffer;
    uint32_t offset, start;
    size_t osize;
    uint32_t end;
    int ret;

    MTX_LOCK(&ctx->mtx);
    if ((filp->oflags & VFS_O_MASK) == VFS_O_WRONLY) {
        ret = -EACCES;
        goto _unlock;
    }

    offset = filp->rawofs;
    osize = size = rte_min_t(size_t, size, filp->pmeta->size - offset);
    start = extofs_to_inofs(ctx, filp->pmeta, offset, &end);
    while (size > 0) {
        size_t bytes = rte_min_t(size_t, end - start, size);
        if (bytes > 0) {
            ret = blkdev_read(ctx->dd, pbuffer, bytes, start);
            if (ret < 0)
                goto _unlock;

            size -= bytes;
            pbuffer += bytes;
            start += bytes;
            offset += bytes;
        } else {
            start = extofs_to_inofs(ctx, filp->pmeta, offset, &end);
        }
    }
    filp->rawofs = offset;
    ret = osize - size;

_unlock:
    MTX_UNLOCK(&ctx->mtx);
    return ret;
}

ssize_t fxip_ll_write(struct xipfs_context *ctx, struct xip_file *filp, 
    const void *buffer, size_t size) {
    const char *pbuffer = buffer;
    size_t osize = size;
    uint32_t offset, start;
    uint32_t end;
    int ret;

    MTX_LOCK(&ctx->mtx);
    if (filp->i16_idx_offset < 0) {
        ret = filp->i16_idx_offset;
        goto _unlock;
    }

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
            pr_dbg("## fxip_ll_write bytes(%d)\n", bytes);
        } else {
            ret = curr_avaliable_space(ctx, filp, offset, &start, &end);
            if (ret < 0) {
                filp->rawofs = 0;
                file_metadata_clear_locked(ctx, filp->pmeta);
                goto _unlock;
            }
            pr_dbg("## fxip_ll_write start(0x%x) end(0x%x)\n", start, end);
        }
    }
    filp->pmeta->size = offset;
    filp->rawofs = offset;
    filp->dirty = true;
    ctx->dirty = true;
    ret = osize - size;

_unlock:
    MTX_UNLOCK(&ctx->mtx);
    return ret;
}

int fxip_ll_close(struct xipfs_context *ctx, struct xip_file *filp) {
    if (!ctx || !filp)
        return -EINVAL;

    if (!(filp->oflags & PFLILE_O_FLAGS))
        return -EINVAL;

    MTX_LOCK(&ctx->mtx);
	pr_dbg("%s close %s\n", __func__, filp->pmeta->name);
    filp->oflags &= ~PFLILE_O_FLAGS;
    delayed_sync(ctx, ctx->dirty);
    if (filp->dirty)
        blkdev_sync();
    file_free(filp);
	xipfs_dump(&ctx->inode);
    MTX_UNLOCK(&ctx->mtx);
    return 0;
}

int fxip_ll_seek(struct xipfs_context *ctx, struct xip_file *filp, 
    off_t offset, int whence) {
    int err = 0;

    MTX_LOCK(&ctx->mtx);
    switch (whence) {
    case VFS_SEEK_SET:
        if (offset > (off_t)filp->pmeta->size) {
            err = -EINVAL;
            goto _unlock;
        }
        filp->rawofs = offset;
        break;
    case VFS_SEEK_CUR:
        if (filp->rawofs + offset > (uint32_t)filp->pmeta->size) {
            err = -EINVAL;
            goto _unlock;
        }
        filp->rawofs += offset;
        break;
    case VFS_SEEK_END:
        if (offset > 0) {
            err = -EINVAL;
            goto _unlock;
        }
        filp->rawofs = filp->pmeta->size;
        break;
    }

_unlock:
    MTX_UNLOCK(&ctx->mtx);
    return err;
}

int fxip_ll_unlink(struct xipfs_context *ctx, const char *name) {
    struct fmeta_data *pmeta;
    int err;

    if (!ctx || !name)
        return -EINVAL;

    MTX_LOCK(&ctx->mtx);
    pmeta = file_search(&ctx->inode, name);
    if (pmeta) {
		pr_dbg("%s delete %s\n", name);
        file_metadata_clear_locked(ctx, pmeta);
        delayed_sync(ctx, ctx->dirty);
        err = 0;
        goto _unlock;
    }
    err = -ENODATA;

_unlock:
    MTX_UNLOCK(&ctx->mtx);
    return err;
}

int fxip_ll_stat(struct xipfs_context *ctx, const char *name, 
    struct vfs_stat *buf) {
    struct fmeta_data *pmeta;
    int err = -ENOENT;

    MTX_LOCK(&ctx->mtx);
    pmeta = file_search(&ctx->inode, name);
    if (pmeta) {
        buf->st_size = pmeta->size;
        buf->st_blocks = pmeta->blknum;
        buf->st_blksize = CONFIG_XIPFS_BLKSIZE;
        err = 0;
    }
    MTX_UNLOCK(&ctx->mtx);
    return err;
}

const char *fxip_get_fname(struct xipfs_context *ctx, int *idx) {
    int size = (int)rte_array_size(ctx->inode.meta);
    const char *fname = NULL;

    if (*idx >= size)
        return NULL;

    MTX_LOCK(&ctx->mtx);
    for (int i = *idx; i < size; i++) {
        if (ctx->inode.meta[i].name[0]) {
            *idx = i + 1;
            fname = ctx->inode.meta[i].name;
            break;
        }
    }
    MTX_UNLOCK(&ctx->mtx);
    return fname;
}

static void fxip_sync_check(os_timer_t timer, void *arg) {
    struct xipfs_context *ctx = arg;
    (void) timer;
    if (ctx->dirty) {
        MTX_LOCK(&ctx->mtx);
        if (ctx->dirty) {
            ctx->dirty = false;
            blkdev_write(ctx->dd, &ctx->inode, 
                sizeof(ctx->inode), ctx->offset);
        }
        MTX_UNLOCK(&ctx->mtx);
    }
}

void fxip_ll_reset(struct xipfs_context *ctx) {
	pr_dbg("Format xip filesystem\n");
	
    MTX_LOCK(&ctx->mtx);
    memset(&ctx->inode, 0, sizeof(ctx->inode));
    ctx->inode.magic = XIPFS_MAGIC;

	/* 
	 * The first block is used to save filesystem information 
	 */
    ctx->inode.i_bitmap[0] = 0x1;
	
    ctx->dirty = true;
    delayed_sync(ctx, ctx->dirty);
	xipfs_dump(&ctx->inode);
    MTX_UNLOCK(&ctx->mtx);
}

uint32_t fxip_ll_map(struct xipfs_context *ctx, struct xip_file *filp, 
    size_t *size) {
    uint32_t end, addr;
	
    if (size)
        *size = filp->pmeta->size;
	
    addr = idx_to_offset(ctx, 
        IDX_CONVERT(filp->pmeta->blkofs), &end);

	pr_dbg("%s map %s to memory 0x%08x (blkofs: %d)\n", __func__, 
		filp->pmeta->name, addr, filp->pmeta->blkofs);
	
    return addr;
}

static int shutdown_listen(struct observer_base *nb,
	unsigned long action, void *data) {
    struct xipfs_observer *p = (struct xipfs_observer *)nb;
    fxip_sync_check(NULL, p->ctx);
    return 0;
}

int fxip_ll_init(struct xipfs_context *ctx, const char *name, 
    uint32_t start) {
    static struct xipfs_observer obs = {
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

    MTX_INIT(&ctx->mtx);
    MTX_LOCK(&ctx->mtx);
    err = os_timer_create(&ctx->timer, fxip_sync_check, 
        ctx, false);
    assert(err == 0);
    if (start == UINT32_MAX)
        ctx->offset = ctx->dd->addr;
    else
        ctx->offset = start;

    err = blkdev_read(ctx->dd, &ctx->inode, 
        sizeof(ctx->inode), ctx->offset);
    MTX_UNLOCK(&ctx->mtx);
    if (err < 0)
        return err;

    obs.ctx = ctx;
    system_add_observer(&obs.base);
    if (ctx->inode.magic != XIPFS_MAGIC) {
        pr_warn("XIPFS is invalid\n");
        fxip_ll_reset(ctx);
    }
	
    return 0;
}

int fxip_ll_ioctl(struct xipfs_context *ctx, struct xip_file *filp,
    uint32_t cmd, void *arg) {
    int ret = -EINVAL;

    MTX_LOCK(&ctx->mtx);
    if (cmd == VFS_IOSET_FILESIZE) {
        /*
         * Prepare to set file size
         */
        size_t size = (size_t)(uintptr_t)arg;
        filp->req_blksz = (size + CONFIG_XIPFS_BLKSIZE - 1) / CONFIG_XIPFS_BLKSIZE;
        ret = check_free_space(ctx, filp->req_blksz);
        filp->i16_idx_offset = (int16_t)ret;
        if (ret > 0) 
            ret = 0;
    }

    MTX_UNLOCK(&ctx->mtx);
    return ret;
}

static void scan_area(const struct xipfs_inode *inode, struct scan_result *r) {
    /* 
     * Scan filesystem bitmap 
     */
    uint32_t freeblks = 0;
    uint32_t freeblk_ofs = 0;

    memset(r, 0, sizeof(*r));
    for (uint32_t i = 0; i < rte_array_size(inode->i_bitmap); i++) {
        if (inode->i_bitmap[i] != UINT32_MAX) {
            uint32_t ofs = 0;
            uint32_t base = i << 5;

            while (ofs < 32) {
                if (base + ofs >= XIPFS_MAX_INODES)
                    goto _next;

                if (!(inode->i_bitmap[i] & BIT(ofs))) {
                    /* Recording the first free block */
                    if (freeblks == 0)
                        freeblk_ofs = base + ofs;

                    freeblks++;
                } else {
                    if (freeblks > 0) {
                        r->areas[r->count].blkofs = freeblk_ofs;
                        r->areas[r->count].blknum = freeblks;
                        r->count++;
                    }
                    freeblk_ofs = freeblks = 0;
                }
                ofs++;
            }
        }
    }

_next:
    if (freeblks > 0) {
        r->areas[r->count].blkofs = freeblk_ofs;
        r->areas[r->count].blknum = freeblks;
        r->count++;
    }
}

static int match_freeblock(const struct scan_result *r, size_t reqblksz, 
    size_t *freespace) {
    int best_matched = -1;
    uint16_t min_diff = UINT16_MAX;
    size_t blksz = 0;

    /*
     * We use best fit to minimize data movement
     */
    for (uint16_t i = 0; i < r->count; i++) {
        if (r->areas[i].blknum >= reqblksz) {
            uint32_t diff = r->areas[i].blknum - reqblksz;
            if (diff < min_diff) {
                min_diff = diff;
                best_matched = r->areas[i].blkofs;
            }
        }
        blksz += r->areas[i].blknum;
    }

    if (freespace)
        *freespace = blksz;

    return best_matched;
}

static struct fmeta_data *find_fmeta_by_blkidx(struct xipfs_inode *inode, 
    uint32_t blkidx) {
    for (int i = 0; i < (int)rte_array_size(inode->meta); i++) {
        struct fmeta_data *pmeta = &inode->meta[i];

        if ((uint16_t)blkidx >= pmeta->blkofs &&
            (uint16_t)blkidx < pmeta->blkofs + pmeta->blknum) {
            return pmeta;
        }
    }
    return NULL;
}

static int move_data_block(struct disk_device *dd, char *blkbuf, 
    uint32_t dst_ofs, uint32_t src_ofs) {
    int ret;

    ret = disk_device_read(dd, blkbuf, CONFIG_XIPFS_BLKSIZE, src_ofs);
    if (ret < 0) {
        pr_err("%s: read offset(0x%x) failed\n", __func__, src_ofs);
        return ret;
    }

    ret = disk_device_erase(dd, dst_ofs, CONFIG_XIPFS_BLKSIZE);
    if (ret < 0) {
        pr_err("%s: erase offset(0x%x) failed\n", __func__, dst_ofs);
        return ret;
    }

    ret = disk_device_write(dd, blkbuf, CONFIG_XIPFS_BLKSIZE, dst_ofs);
    if (ret < 0) {
        pr_err("%s: write offset(0x%x) failed\n", __func__, dst_ofs);
        return ret;
    }

    return 0;
}

static int file_block_move(struct xipfs_context *ctx, uint32_t first_dstidx, 
    uint32_t first_srcidx, uint32_t idxcnt, char *blkbuf) {
    struct fmeta_data *pmeta;
    uint32_t srcofs, dstofs;
    uint32_t end;
    int err;

    /* Data blocks can only be moved from back to front */
    assert(first_dstidx < first_srcidx);

    /*
     * Ensure that the block does not belong to any files
     */
    pmeta = find_fmeta_by_blkidx(&ctx->inode, first_dstidx);
    if (pmeta != NULL) {
        pr_err("The block(%d) is not free\n", first_dstidx);
        return -EINVAL;
    }

    pmeta = find_fmeta_by_blkidx(&ctx->inode, first_srcidx);
    if (pmeta == NULL) {
        pr_err("Block(%d) corresponding file not found\n", first_srcidx);
        return -ENODATA;
    }

    /* 
     * Ensure that the block corresponds to the same file information 
     */
    if (pmeta->blkofs != first_srcidx ||
        pmeta->blknum != idxcnt) {
        pr_err("File information is not right\n");
        return -EINVAL;
    }

    /*
     * Move file data
     */
    for (uint32_t i = 0; i < idxcnt; i++) {
        dstofs = idx_to_offset(ctx, first_dstidx, &end);
        srcofs = idx_to_offset(ctx, first_srcidx, &end);
        err = move_data_block(ctx->dd, blkbuf, dstofs, srcofs);
        if (err < 0)
            return err;
        
        bitvec_move(ctx->inode.i_bitmap, first_dstidx, first_srcidx);
        first_dstidx++;
        first_srcidx++;
    }

    pmeta->blkofs = first_dstidx - idxcnt;

    return 0;
}

static int check_free_space(struct xipfs_context *ctx, size_t reqblksz) {
    struct scan_result r;
    size_t freesz;
    int ret;

    /*
     * Gathering information about the space available 
     * on the file system
     */
    scan_area(&ctx->inode, &r);

    ret = match_freeblock(&r, reqblksz, &freesz);
    if (ret >= 0) {
        /* 
         * If we have found the right block then return 
         */
        return ret;
    }

    /* 
     * If there is no enough free space, return error code 
     */
    if (freesz < reqblksz)
        return -EFBIG;

    /*
     * No suitable and contiguous free area is found, 
     * we must move the disk data
     */
    char *blkbuf = cache_blk_alloc(CONFIG_XIPFS_BLKSIZE);
    if (blkbuf == NULL) 
        return -ENOMEM;

    blkdev_sync();

    for (uint16_t i = 1; i < r.count; i++) {
        const struct free_area *parea = &r.areas[i - 1];
        uint32_t fblk_start = parea->blkofs + parea->blknum;

        assert(r.areas[i].blkofs > fblk_start);
        ret = file_block_move(ctx, 
            parea->blkofs,
            fblk_start, 
            r.areas[i].blkofs - fblk_start,
            blkbuf
            );
        if (ret < 0)
            break;

        /* Update area information */
        r.areas[i].blkofs -= parea->blknum;
        r.areas[i].blknum += parea->blknum;

        /*
         * If we have enough free space then return
         */
        if (r.areas[i].blknum >= reqblksz) {
            ret = r.areas[i].blkofs;
            pr_dbg("merge finished (new-index: %d)\n", ret);
            break;
        }
    }

    cache_blk_free(blkbuf);
    
    return ret;
}
