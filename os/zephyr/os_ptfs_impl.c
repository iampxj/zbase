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
#include "basework/dev/ptfs.h"
#include "basework/generic.h"
#include "basework/os/osapi_fs.h"
#include "basework/log.h"
#include "basework/malloc.h"
#include "basework/lib/string.h"

#define READ_BUFFER_SIZE  4096
#define FLASH_ERASE_BLKSZ 4096
#define FLASH_ALIGNED_UP(x) \
    (((x) + FLASH_ERASE_BLKSZ - 1) & ~(FLASH_ERASE_BLKSZ - 1))

struct file {
	struct vfs_file super;
    struct ptfs_file *fp;
};

#define PTFS &ptfs_context

static struct ptfs_context ptfs_context __rte_section(".ram.noinit");

const char *ptfs_get_filename(const char *name) {
    if (name[0] != '/')
        return NULL;
    const char *p = strchr(name, ':');
    if (!p || p[1] != '/')
        return NULL;
    return p + 2;
}

static int ptfs_impl_open(os_file_t fd, const char *path, 
    int flags, va_list ap) {
    struct file *fp = (struct file *)fd;
    const char *fname;
    int err = 0;

    fname = ptfs_get_filename(path);
    if (!fname)
        return -EINVAL;
    
    (void) ap;
    fp->fp = ptfile_ll_open(PTFS, fname, flags, &err);

    return err;
}

static int ptfs_impl_close(os_file_t fd) {
    struct file *fp = (struct file *)fd;
    return ptfile_ll_close(PTFS, fp->fp);
}

static int ptfs_impl_ioctl(os_file_t fd, int cmd, void *args) {
    (void) fd;
    (void) cmd;
    (void) args;
    return -ENOSYS;
}

static ssize_t ptfs_impl_read(os_file_t fd, void *buf, size_t len) {
    struct file *fp = (struct file *)fd;
    return ptfile_ll_read(PTFS, fp->fp, buf, len);
}

static ssize_t ptfs_impl_write(os_file_t fd, const void *buf, size_t len) {
    struct file *fp = (struct file *)fd;
    return ptfile_ll_write(PTFS, fp->fp, buf, len);    
}

static int ptfs_impl_flush(os_file_t fd) {
    (void) fd;
    return -ENOSYS;
}

static int ptfs_impl_lseek(os_file_t fd, off_t offset, int whence) {
    struct file *fp = (struct file *)fd;
    return ptfile_ll_seek(PTFS, fp->fp, offset, whence); 
}


static int ptfs_impl_opendir(const char *path, VFS_DIR *dirp) {
    const char *dir = ptfs_get_filename(path);

    if (dir[-1] == '/' && dir[0] == '\0') {
        dirp->data = (void *)0;
        return 0;
    }
    return -EINVAL;
}

static int ptfs_impl_readir(VFS_DIR *dirp, struct vfs_dirent *dirent) {
    const char *pname;
    int idx;

    idx = (int)dirp->data;
    pname = ptfile_get_fname(PTFS, &idx);
    if (pname) {
        dirp->data = (void *)idx;
        dirent->d_type = DT_REG;
        strlcpy(dirent->d_name, pname, sizeof(dirent->d_name));
        return 0;
    }
    return -ENODATA;
}

static int ptfs_impl_closedir(VFS_DIR *dirp) {
    (void) dirp;
    return 0;
}

static int ptfs_impl_rename(os_filesystem_t fs, const char *oldpath, 
    const char *newpath) {
    (void) fs;
    (void) oldpath;
    (void) newpath;
    return -ENOSYS;
}

static int ptfs_impl_ftruncate(os_file_t fd, off_t length) {
    (void) fd;
    (void) length;
    return -ENOSYS;
}

static int ptfs_impl_unlink(os_filesystem_t fs, const char *path) {
    (void) fs;
    (void) path;
    const char *fname = ptfs_get_filename(path);
    if (!fname)
        return -EINVAL;
    return ptfile_ll_unlink(PTFS, fname);
}

static int ptfs_impl_stat(os_filesystem_t fs, const char *name, 
    struct vfs_stat *buf) {
    const char *fname = ptfs_get_filename(name);
    (void) fs;
    return ptfile_ll_stat(PTFS, fname, buf); 
}

static int ptfs_impl_reset(os_filesystem_t fs) {
    (void) fs;
    ptfile_ll_reset(PTFS);
    return 0;
}

static struct file os_files[CONFIG_OS_MAX_FILES] __rte_section(".ram.noinit");
static struct file_class ptfs_class = {
	.mntpoint    = "/PTFS:",
	.fds_buffer  = os_files,
	.fds_size    = sizeof(os_files),
	.fd_size     = sizeof(os_files[0]),
    .fs_priv     = &ptfs_context,

    .open = ptfs_impl_open,
    .close = ptfs_impl_close,
    .ioctl = ptfs_impl_ioctl,
    .read = ptfs_impl_read,
    .write = ptfs_impl_write,
    .flush = ptfs_impl_flush,
    .lseek = ptfs_impl_lseek,
    .truncate = ptfs_impl_ftruncate,
    .opendir = ptfs_impl_opendir,
    .readdir = ptfs_impl_readir,
    .closedir = ptfs_impl_closedir,
    .mkdir = NULL,
    .unlink = ptfs_impl_unlink,
    .stat = ptfs_impl_stat,
    .rename = ptfs_impl_rename,
    .reset = ptfs_impl_reset,
};

static int __rte_unused ptfs_impl_register(const struct device *dev) {
    uint32_t offset;
    int err;
    (void) dev;

    const struct partition_entry *parti;
    parti = parition_get_entry2(STORAGE_ID_NOR, PARTITION_FILE_ID_UDISK);
    if (!parti) {
        pr_err("Not found udisk parition\n");
        return -ENOENT;
    }

    if (parti->size < CONFIG_PTFS_SIZE) {
        pr_err("ptfs partition is too large (%d)\n", CONFIG_PTFS_SIZE);
        return -EINVAL;
    }

    offset = parti->offset;

    memset(os_files, 0, sizeof(os_files));
    memset(&ptfs_context, 0, sizeof(ptfs_context));
    switch (parti->storage_id) {
    case STORAGE_ID_NOR:
        err = ptfile_ll_init(PTFS, "spi_flash", offset);
        break;
    case STORAGE_ID_NAND:
        err = ptfile_ll_init(PTFS, "spinand_flash", offset);
        break;
    case STORAGE_ID_DATA_NOR:
        err = ptfile_ll_init(PTFS, "spi_flash_2", offset);
        break;
    default:
        err = -EINVAL;
        os_panic();
        break;
    }
    if (err)
        return err;
    pr_notice("## partition filesystem registed start(0x%x) size(%d)\n", 
        offset, CONFIG_PTFS_SIZE);

    return vfs_register(&ptfs_class);
}

#ifndef CONFIG_CARD_READER_APP
SYS_INIT(ptfs_impl_register, APPLICATION, 
    CONFIG_APPLICATION_INIT_PRIORITY);
#endif
