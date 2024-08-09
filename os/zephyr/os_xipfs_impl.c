/*
 * Copyright 2022 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<os_ptfs>: "fmt

#include <string.h>
#include <errno.h>
#include <assert.h>
#include <init.h>
#include <board_cfg.h>
#include <partition/partition.h>
#include <drivers/nvram_config.h>

#include "basework/dev/disk.h"
#include "basework/dev/xipfs.h"
#include "basework/generic.h"
#include "basework/os/osapi_fs.h"
#include "basework/log.h"
#include "basework/malloc.h"
#include "basework/lib/string.h"

struct file {
    struct vfs_file super;
    struct xip_file *fp;
};

#define XIPFS &xipfs_context

static struct xipfs_context xipfs_context __rte_section(".ram.noinit");;

const char *xipfs_get_filename(const char *name) {
    if (name[0] != '/')
        return NULL;
    const char *p = strchr(name, ':');
    if (!p || p[1] != '/')
        return NULL;
    return p + 2;
}

static int xipfs_impl_open(os_file_t fd, const char *path, 
    int flags, va_list ap) {
    struct file *fp = (struct file *)fd;
    const char *fname = xipfs_get_filename(path);
    if (!fname)
        return -EINVAL;
    
    (void) ap;
    fp->fp = fxip_ll_open(XIPFS, fname, flags);
    if (!fp->fp)
        return -1;
    return 0;
}

static int xipfs_impl_close(os_file_t fd) {
    struct file *fp = (struct file *)fd;
    return fxip_ll_close(XIPFS, fp->fp);
}

static int xipfs_impl_ioctl(os_file_t fd, int cmd, void *args) {
    struct file *fp = (struct file *)fd;
    return fxip_ll_ioctl(XIPFS, fp->fp,
        cmd, args);
}

static ssize_t xipfs_impl_read(os_file_t fd, void *buf, size_t len) {
    struct file *fp = (struct file *)fd;
    return fxip_ll_read(XIPFS, fp->fp, buf, len);
}

static ssize_t xipfs_impl_write(os_file_t fd, const void *buf, size_t len) {
    struct file *fp = (struct file *)fd;
    return fxip_ll_write(XIPFS, fp->fp, buf, len);    
}

static int xipfs_impl_flush(os_file_t fd) {
    (void) fd;
    return -ENOSYS;
}

static int xipfs_impl_lseek(os_file_t fd, off_t offset, int whence) {
    struct file *fp = (struct file *)fd;
    return fxip_ll_seek(XIPFS, fp->fp, offset, whence); 
}


static int xipfs_impl_opendir(const char *path, VFS_DIR *dirp) {
    const char *dir = xipfs_get_filename(path);

    if (dir[-1] == '/' && dir[0] == '\0') {
        dirp->data = (void *)0;
        return 0;
    }
    return -EINVAL;
}

static int xipfs_impl_readir(VFS_DIR *dirp, struct vfs_dirent *dirent) {
    const char *pname;
    int idx;

    idx = (int)dirp->data;
    pname = fxip_get_fname(XIPFS, &idx);
    if (pname) {
        dirp->data = (void *)idx;
        dirent->d_type = DT_REG;
        strlcpy(dirent->d_name, pname, sizeof(dirent->d_name));
        return 0;
    }
    return -ENODATA;
}

static int xipfs_impl_closedir(VFS_DIR *dirp) {
    (void) dirp;
    return 0;
}

static int xipfs_impl_rename(os_filesystem_t fs, const char *oldpath, 
    const char *newpath) {
    (void) fs;
    (void) oldpath;
    (void) newpath;
    return -ENOSYS;
}

static int xipfs_impl_ftruncate(os_file_t fd, off_t length) {
    (void) fd;
    (void) length;
    return -ENOSYS;
}

static int xipfs_impl_unlink(os_filesystem_t fs, const char *path) {
    (void) fs;
    (void) path;
    const char *fname = xipfs_get_filename(path);
    if (!fname)
        return -EINVAL;
    return fxip_ll_unlink(XIPFS, fname);
}

static int xipfs_impl_stat(os_filesystem_t fs, const char *name, 
    struct vfs_stat *buf) {
    const char *fname = xipfs_get_filename(name);
    (void) fs;
    return fxip_ll_stat(XIPFS, fname, buf); 
}

static int xipfs_impl_reset(os_filesystem_t fs) {
    (void) fs;
    fxip_ll_reset(XIPFS);
    return 0;
}

static void *xipfs_impl_map(os_file_t fd, size_t *size) {
    struct file *fp = (struct file *)fd;
    uint32_t offset = fxip_ll_map(XIPFS, fp->fp, size);
    return (void *)(CONFIG_SPI_XIP_VADDR + offset);
}

static struct file os_files[CONFIG_OS_MAX_FILES] __rte_section(".ram.noinit");
static struct file_class xipfs_class = {
	.mntpoint   = "/XIPFS:",
	.fds_buffer = os_files,
	.fds_size   = sizeof(os_files),
	.fd_size    = sizeof(os_files[0]),

    .open = xipfs_impl_open,
    .close = xipfs_impl_close,
    .ioctl = xipfs_impl_ioctl,
    .read = xipfs_impl_read,
    .write = xipfs_impl_write,
    .flush = xipfs_impl_flush,
    .lseek = xipfs_impl_lseek,
    .truncate = xipfs_impl_ftruncate,
    .opendir = xipfs_impl_opendir,
    .readdir = xipfs_impl_readir,
    .closedir = xipfs_impl_closedir,
    .mmap = xipfs_impl_map,
    .mkdir = NULL,
    .unlink = xipfs_impl_unlink,
    .stat = xipfs_impl_stat,
    .rename = xipfs_impl_rename,
    .reset = xipfs_impl_reset,
};

static int __rte_unused xipfs_impl_register(const struct device *dev) {
    uint32_t offset;
    int err;
    (void) dev;

    const struct partition_entry *parti;
    parti = parition_get_entry2(STORAGE_ID_NOR, PARTITION_FILE_ID_UDISK);
    if (!parti) {
        pr_err("Not found udisk parition\n");
        return -ENOENT;
    }

    if (parti->size < CONFIG_XIPFS_SIZE) {
        pr_err("ptfs partition is too large (%d)\n", CONFIG_XIPFS_SIZE);
        return -EINVAL;
    }

    offset = parti->offset;

    memset(os_files, 0, sizeof(os_files));
    memset(&xipfs_context, 0, sizeof(xipfs_context));
    err = fxip_ll_init(XIPFS, CONFIG_SPI_FLASH_NAME, offset);
    if (err)
        return err;
    pr_notice("## XIP filesystem registed start(0x%x) size(%d)\n", 
        offset, CONFIG_XIPFS_SIZE);

    return vfs_register(&xipfs_class);
}

#ifndef CONFIG_CARD_READER_APP
SYS_INIT(xipfs_impl_register, APPLICATION, 
    CONFIG_APPLICATION_INIT_PRIORITY);
#endif
