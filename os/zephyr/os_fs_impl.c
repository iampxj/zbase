/*
 * Copyright 2022 wtcat
 *
 * File system implement for zephyr
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<os_fs_impl>" fmt
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <init.h>
#include <fs/fs.h>

#ifdef CONFIG_FAT_FILESYSTEM_ELM
#include "diskio.h"		/* FatFs lower layer API */
#endif

#include "basework/os/osapi_config.h"
#include "basework/os/osapi_fs.h"
#include "basework/os/osapi_timer.h"
#include "basework/os/osapi_obj.h"
#include "basework/log.h"
#include "basework/malloc.h"

struct file {
	struct vfs_file super;
    struct fs_file_t zfp;
};

extern size_t strlcpy (char *dst, const char *src, size_t dsize);


static int zephyr_fs_open(os_file_t fd, const char *path, int flags, va_list ap) {
    struct file *fp = (struct file *)fd;
    int orw = flags & VFS_O_MASK;
    int ff = 0;

    (void) ap;
    switch (orw) {
    case VFS_O_RDONLY:
        ff |= FS_O_READ;
        break;
    case VFS_O_WRONLY:
        ff |= FS_O_WRITE;
        break;
    case VFS_O_RDWR:
        ff |= FS_O_RDWR;
        break;
    default:
        return -EINVAL;
    }
    if (flags & VFS_O_APPEND)
        ff |= FS_O_APPEND;
    if (flags & VFS_O_CREAT)
        ff |= FS_O_CREATE;

    pr_dbg("%s: %d path(%s) mode(0x%x)\n", __func__, __LINE__, path, ff);
    fs_file_t_init(&fp->zfp);
    return fs_open(&fp->zfp, path, ff);
}

static int zephyr_fs_close(os_file_t fd) {
    struct file *fp = (struct file *)fd;
    return fs_close(&fp->zfp);
}

static int zephyr_fs_ioctl(os_file_t fd, int cmd, void *args) {
    (void) fd;
    (void) cmd;
    (void) args;
    return -ENOSYS;
}

static int zephyr_fs_read(os_file_t fd, void *buf, size_t len) {
    struct file *fp = (struct file *)fd;
    return fs_read(&fp->zfp, buf, len);
}

static int zephyr_fs_write(os_file_t fd, const void *buf, size_t len) {
    struct file *fp = (struct file *)fd;
    return fs_write(&fp->zfp, buf, len);
}

static off_t zephyr_fs_tell(os_file_t fd) {
    struct file *fp = (struct file *)fd;
    return fs_tell(&fp->zfp);
}

static int zephyr_fs_flush(os_file_t fd) {
    struct file *fp = (struct file *)fd;
    return fs_sync(&fp->zfp);
}

static int zephyr_fs_lseek(os_file_t fd, off_t offset, int whence) {
    struct file *fp = (struct file *)fd;
    return fs_seek(&fp->zfp, offset, whence);
}

static int zephyr_fs_rename(os_filesystem_t fs, const char *oldpath, 
    const char *newpath) {
    (void) fs;
    return fs_rename(oldpath, newpath);
}

static int zephyr_fs_ftruncate(os_file_t fd, off_t length) {
    struct file *fp = (struct file *)fd;
    return fs_truncate(&fp->zfp, length);
}

static int zephyr_fs_unlink(os_filesystem_t fs, const char *path) {
    (void) fs;
    return fs_unlink(path);
}

static int zephyr_fs_opendir(const char *path, VFS_DIR *dirp) {
    struct fs_dir_t *zdp;
    int err;

    zdp = general_calloc(1, sizeof(struct fs_dir_t));
    if (!zdp)
        return -ENOMEM;
    err = fs_opendir(zdp, path);
    if (!err) {
        dirp->data = zdp;
        return 0;
    }
    general_free(zdp);
    return err;
}

static int zephyr_fs_readir(VFS_DIR *dirp, struct vfs_dirent *entry) {
    struct fs_dir_t *zdp = dirp->data;
    struct fs_dirent dirent;
    int err;

    err = fs_readdir(zdp, &dirent);
    if (!err) {
        entry->d_type = dirent.type == FS_DIR_ENTRY_FILE? DT_REG: DT_DIR;
        strlcpy(entry->d_name, dirent.name, sizeof(entry->d_name));
        return 0;
    }
    return err;
}

static int zephyr_fs_closedir(VFS_DIR *dirp) {
    struct fs_dir_t *zdp = dirp->data;
    int err;

    err = fs_closedir(zdp);
    if (!err)
        general_free(zdp);
    return err;
}

static int zephyr_fs_mkdir(os_filesystem_t fs, const char *path) {
    (void) fs;
    return fs_mkdir(path);
}

static int zephyr_fs_stat(os_filesystem_t fs, const char *path, 
    struct vfs_stat *buf) {
    struct fs_dirent stat;
    int err;

    err = fs_stat(path, &stat);
    if (!err) {
        buf->st_blksize = 0;
        buf->st_blocks = 0;
        buf->st_size = stat.size;
    }

    return err;
}

//TODO: This just only for fatfs mountpoint: "/UD0:"
static int zephyr_fs_sync(os_filesystem_t fs) {
    (void) fs;
#ifdef CONFIG_DISKIO_CACHE
	diskio_cache_flush("UD0");
#endif
#ifdef CONFIG_FAT_FILESYSTEM_ELM
	return disk_access_ioctl("UD0", DISK_IOCTL_CTRL_SYNC, NULL);
#else
    return -ENOSYS;
#endif
}

static struct file os_files[CONFIG_OS_MAX_FILES];
static struct file_class zephyr_file_class = {
	.mntpoint    = NULL,
	.fds_buffer  = os_files,
	.fds_size    = sizeof(os_files),
	.fd_size     = sizeof(os_files[0]),

    .open = zephyr_fs_open,
    .close = zephyr_fs_close,
    .ioctl = zephyr_fs_ioctl,
    .read = zephyr_fs_read,
    .write = zephyr_fs_write,
    .tell = zephyr_fs_tell,
    .flush = zephyr_fs_flush,
    .lseek = zephyr_fs_lseek,
    .truncate = zephyr_fs_ftruncate,
    .opendir = zephyr_fs_opendir,
    .readdir = zephyr_fs_readir,
    .closedir = zephyr_fs_closedir,
    .fssync = zephyr_fs_sync,
    .mkdir = zephyr_fs_mkdir,
    .unlink = zephyr_fs_unlink,
    .stat = zephyr_fs_stat,
    .rename = zephyr_fs_rename,
};

static int __rte_notrace zephyr_fs_init(const struct device *dev __rte_unused) {
	return vfs_register(&zephyr_file_class);
}

SYS_INIT(zephyr_fs_init, APPLICATION, 0);
