/*
 * Copyright 2023 wtcat
 */
#ifndef BASEWORK_DEV_XIPFS_H_
#define BASEWORK_DEV_XIPFS_H_

#include <stddef.h>
#include <stdint.h>

#include "basework/os/osapi.h"

#ifdef __cplusplus
extern "C"{
#endif

#ifndef CONFIG_XIPFS_SIZE
#define CONFIG_XIPFS_SIZE       (3 * 1024 * 1024ul)
#endif

#ifndef CONFIG_XIPFS_BLKSIZE
#define CONFIG_XIPFS_BLKSIZE    (32 * 1024ul)
#endif

#ifndef CONFIG_XIPFS_MAXFILES
#define CONFIG_XIPFS_MAXFILES   15
#endif

#define XIPFS_MAX_INODES (CONFIG_XIPFS_SIZE / CONFIG_XIPFS_BLKSIZE)
#define XIPFS_IMAP_SIZE  ((XIPFS_MAX_INODES + 31) / 32)
#define XIPFS_FMAP_SIZE  ((CONFIG_XIPFS_MAXFILES + 31) / 32)

struct xip_file;
struct fmeta_data {
#define MAX_XIPFS_FILENAME 32 
    char name[MAX_XIPFS_FILENAME];
    size_t size;
    uint16_t blkofs;  /* The first block index */
    uint16_t blknum;  /* Block numbers */
    uint16_t used;
    uint16_t chksum;
};

struct xipfs_inode {
	uint32_t i_bitmap[XIPFS_IMAP_SIZE];
    struct fmeta_data meta[CONFIG_XIPFS_MAXFILES];
    uint16_t hcrc;
    uint16_t nfiles;
	uint32_t magic;
};

struct xipfs_context {
    struct xipfs_inode inode;
    os_mutex_t mtx;
    struct disk_device *dd;
    os_timer_t timer;
    uint32_t offset; /* content offset */
    bool dirty;
};

struct xip_file *fxip_ll_open(struct xipfs_context *ctx,
    const char *name, int mode);
ssize_t fxip_ll_read(struct xipfs_context *ctx, struct xip_file *filp, 
    void *buffer, size_t size);
ssize_t fxip_ll_write(struct xipfs_context *ctx, struct xip_file *filp, 
    const void *buffer, size_t size);
int fxip_ll_close(struct xipfs_context *ctx, struct xip_file *filp);
int fxip_ll_seek(struct xipfs_context *ctx, struct xip_file *filp, 
    off_t offset, int whence);
int fxip_ll_unlink(struct xipfs_context *ctx, const char *name);
int fxip_ll_stat(struct xipfs_context *ctx, const char *name, 
    struct vfs_stat *buf);
const char *fxip_get_fname(struct xipfs_context *ctx, int *idx);
int fxip_ll_init(struct xipfs_context *ctx, const char *dev, uint32_t start);
int fxip_ll_ioctl(struct xipfs_context *ctx, struct xip_file *filp,
    uint32_t cmd, void *arg);
int fxip_ll_check(const char *dev, uint32_t offset);
void fxip_ll_reset(struct xipfs_context *ctx);
uint32_t fxip_ll_map(struct xipfs_context *ctx, struct xip_file *filp, 
    size_t *size);
    
#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEV_XIPFS_H_ */

