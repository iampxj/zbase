/*
 * Copyright 2023 wtcat
 * 
 * Simple parition filesystem low-level implement
 */
#define CONFIG_PTFS_SIZE    0x300000
#define CONFIG_PTFS_INODE_SIZE 4096
#define CONFIG_PTFS_BLKSIZE 32768 
#define CONFIG_PTFS_MAXFILES 10


#include <errno.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>

#include "basework/ilog2.h"
#include "basework/dev/disk.h"
#include "basework/bitops.h"
#include "basework/malloc.h"
#include "basework/minmax.h"
#include "ptfs.h"


struct ptfs_file {
    pthread_mutex_t mtx;
    struct file_metadata *pmeta;
    uint32_t rawofs;
    int oflags;
};

_Static_assert(sizeof(struct ptfs_inode) == 2424, "");

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
#define MTX_LOCK(_mtx)   (void)0
#define MTX_UNLOCK(_mtx) (void)0
#define MTX_INIT(_mtx)   (void)0
#define MTX_DEINIT(_mtx) (void)0

_Static_assert(PTFS_MAX_INODES >= CONFIG_PTFS_MAXFILES, "");
_Static_assert(!(CONFIG_PTFS_BLKSIZE & (CONFIG_PTFS_BLKSIZE - 1)), "");
_Static_assert(CONFIG_PTFS_INODE_SIZE < CONFIG_PTFS_BLKSIZE, "");

#define VFS_O_RDONLY 0
#define VFS_O_WRONLY 1
#define VFS_O_RDWR   2
#define VFS_O_MASK   3
#define VFS_O_CREAT  0x0200

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))
#define pr_dbg(...)

static uint32_t _ptfs_size;

ssize_t __blkdev_write(struct disk_device *dd, const void *buf, size_t size, 
    uint32_t offset) {
    char *fbuffer = (char *)dd;
    memcpy(fbuffer + offset, buf, size);
    if (offset + size > _ptfs_size)
        _ptfs_size = offset + size;
    return size;
}


static inline void do_dirty(struct ptfs_context *ctx, bool force) {
    // if (force)
    //     os_timer_mod(ctx->timer, PTFS_SYNC_PERIOD);
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
    assert(idx < (int)(n << 5));
    /* Remove from bitmap */
    idx -= 1;
    bitmap[(size_t)idx >> 5] &= ~BIT((idx & 31));
    return 0;
}

static inline int inode_allocate(struct ptfs_inode *inode) {
    return bitmap_allocate(inode->i_bitmap, 
        ARRAY_SIZE(inode->i_bitmap));
}

static inline int fnode_allocate(struct ptfs_inode *inode) {
    return bitmap_allocate(inode->f_bitmap, 
        ARRAY_SIZE(inode->f_bitmap));
}

static inline int fnode_free(struct ptfs_inode *inode, int idx) {
    return bitmap_free(inode->f_bitmap, 
        ARRAY_SIZE(inode->f_bitmap), idx);
}

//TODO: optimize performance
static struct file_metadata *file_search(struct ptfs_inode *inode, 
    const char *name) {
    for (int i = 0; i < (int)ARRAY_SIZE(inode->meta); i++) {
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
        printf("allocate file handle failed\n");
        return NULL;
    }
    MTX_INIT(filp->mtx);
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
        MTX_LOCK(ctx->mtx);
        uint16_t newidx = (uint16_t)inode_allocate(&ctx->inode);
        MTX_UNLOCK(ctx->mtx);
        if (!newidx) {
            printf("allocate inode failed\n");
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

static void ptfile_sync_check(struct ptfs_context *ctx) {
    if (ctx->dirty) {
        MTX_LOCK(ctx->mtx);
        ctx->dirty = false;
        __blkdev_write(ctx->dd, &ctx->inode, 
            sizeof(ctx->inode), ctx->offset);
        MTX_UNLOCK(ctx->mtx);
    }
}

struct ptfs_file *ptfile_ll_open(struct ptfs_context *ctx,
    const char *name, int mode) {
    struct file_metadata *pmeta;
    struct ptfs_file *filp;
    int fidx;

    pr_dbg("## ptfile_ll_open --begin\n");
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
            printf("allocate file node failed\n");
            goto _free_fp;
        }
        if (fidx > CONFIG_PTFS_MAXFILES) {
            printf("too many files\n");
            fnode_free(&ctx->inode, fidx);
            goto _free_fp;
        }

        pmeta = ctx->inode.meta + IDX_CONVERT(fidx);
        memset(pmeta, 0, sizeof(*filp->pmeta));
        pmeta->i_meta = fidx;
        strncpy(pmeta->name, name, MAX_PTFS_FILENAME-1);
    }
    MTX_UNLOCK(ctx->mtx);
    filp->pmeta = pmeta;
    filp->oflags = mode & VFS_O_MASK;
    filp->oflags |= PFLILE_O_FLAGS;
    pr_dbg("## ptfile_ll_open --end\n");
    return filp;

_free_fp:
    general_free(filp);
    MTX_UNLOCK(ctx->mtx);
    return NULL;
}

ssize_t ptfile_ll_write(struct ptfs_context *ctx, struct ptfs_file *filp, 
    const void *buffer, size_t size) {
    const char *pbuffer = buffer;
    size_t osize = size;
    uint32_t offset, start;
    uint32_t end;
    int ret;

    pr_dbg("## ptfile_ll_write --begin: size(%d)\n", size);
    MTX_LOCK(filp->mtx);
    if ((filp->oflags & VFS_O_MASK) == VFS_O_RDONLY) {
        ret = -EACCES;
        goto _unlock;
    }

    offset = filp->rawofs;
    ret = curr_avaliable_space(ctx, filp, offset, &start, &end);
    if (ret < 0)
        goto _unlock;

    pr_dbg("## ptfile_ll_write --first start(0x%x) end(0x%x)\n", start, end);
    while (size > 0) {
        size_t bytes = min_t(size_t, end - start, size);
        if (bytes > 0) {
            ret = __blkdev_write(ctx->dd, pbuffer, bytes, start);
            if (ret < 0)
                goto _unlock;
            size -= bytes;
            pbuffer += bytes;
            start += bytes;
            offset += bytes;
            pr_dbg("## ptfile_ll_write bytes(%d)\n", bytes);
        } else {
            ret = curr_avaliable_space(ctx, filp, offset, &start, &end);
            if (ret < 0)
                goto _unlock;
            pr_dbg("## ptfile_ll_write start(0x%x) end(0x%x)\n", start, end);
        }
    }
    filp->pmeta->size = offset;
    filp->rawofs = offset;
    ctx->dirty = true;
    ret = osize - size;

_unlock:
pr_dbg("## ptfile_ll_write --end: size(%d)\n", ret);
    MTX_UNLOCK(filp->mtx);
    return ret;
}

int ptfile_ll_close(struct ptfs_context *ctx, struct ptfs_file *filp) {
    if (!ctx || !filp)
        return -EINVAL;
    if (!(filp->oflags & PFLILE_O_FLAGS))
        return -EINVAL;
    pr_dbg("## ptfile_ll_close --begin\n");
    MTX_LOCK(filp->mtx);
    filp->oflags &= ~PFLILE_O_FLAGS;
    do_dirty(ctx, ctx->dirty);
    MTX_UNLOCK(filp->mtx);
    ptfile_sync_check(ctx);
    general_free(filp);
    pr_dbg("## ptfile_ll_close --end\n");
    return 0;
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

int ptfile_ll_init(struct ptfs_context *ctx, const char *name, 
    uint32_t start) {
    if (!ctx || !name)
        return -EINVAL;

    MTX_INIT(ctx->mtx);
    ctx->offset = start;
    ptfile_ll_reset(ctx);
    return 0;
}

static void show_usage(void) {
    printf("\nmkptfs -i [input files] -o [output file]\n");
}

int main(int argc, char **argv) {
#define RD_BUFSIZE   0x200000
#define PTFS_BUFSIZE 0x300000
    struct ptfs_context ctx;
    void *fbuffer, *rdbuffer;
    const char *inlist[10] = {NULL};
    const char *str, *outfile;
    int nfiles = 0, ch, err;
    FILE *fp, *rfp;
    struct ptfs_file *pfp;
    size_t rdsize;

    while ((ch = getopt(argc, argv, "i:o:")) != -1) {
        switch (ch) {
        case 'i':
            str = optarg;
            break;
        case 'o':
            outfile = optarg;
            break;
        default:
            show_usage();
            return -2;
        }
    }

    while (*str != '\0' && *str != '-') {
        if (*str == ' ') {
            str++;
            continue;
        }
        inlist[nfiles++] = str;
        while (*str != '\0' && *str != ' ')
            str++;
    }

    printf("Input files:\n");
    for (int i = 0; i < nfiles; i++) 
        printf(" %s\n", inlist[i]);
    printf("\n");
    printf("Output file:\n%s\n", outfile);

    fp = fopen(outfile, "wb");
    if (!fp) {
        printf("Open %s failed\n", outfile);
        err = errno;
        goto _fclose;
    }

    fbuffer = malloc(PTFS_BUFSIZE + RD_BUFSIZE);
    if (!fbuffer) {
        printf("No more memory\n");
        err = -ENOMEM;
        goto _fclose;
    }

    rdbuffer = (char *)fbuffer + PTFS_BUFSIZE;
    memset(&ctx, 0, sizeof(ctx));
    memset(fbuffer, 0, PTFS_BUFSIZE);
    ctx.dd = fbuffer;
    ptfile_ll_init(&ctx, "", 0);
    for (int i = 0; i < nfiles; i++) {
        pfp = ptfile_ll_open(&ctx, inlist[i], 
            VFS_O_CREAT | VFS_O_WRONLY);
        if (!pfp) {
            printf("PTFS Open %s failed\n", inlist[i]);
            err = -EIO;
            break;
        }

        rfp = fopen(inlist[i], "rb");
        if (!rfp) {
            ptfile_ll_close(&ctx, pfp);
            break;
        }
        fseek(rfp, 0, SEEK_END);
        rdsize = ftell(rfp);
        fseek(rfp, 0, SEEK_SET);
        if (fread(rdbuffer, rdsize, 1, rfp) < 0) {
            err = -EIO;
            fclose(rfp);
            ptfile_ll_close(&ctx, pfp);
            break;
        }
        err = ptfile_ll_write(&ctx, pfp, rdbuffer, rdsize);
        if (err < 0) {
            fclose(rfp);
            ptfile_ll_close(&ctx, pfp);
            break;
        }
        err = 0;
        fclose(rfp);
        ptfile_ll_close(&ctx, pfp);
    }

    if (err == 0) {
        fwrite(fbuffer, _ptfs_size, 1, fp);
        printf("mkptfs (%s) successul(size: %u)\n", outfile, _ptfs_size);
    } else {
        printf("mkptfs failed with error(%d)\n", err);
    }
    free(fbuffer);
_fclose:
    fclose(fp);
    return err;
}