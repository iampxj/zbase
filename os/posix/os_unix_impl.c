/*
 * Copyright 2022 wtcat
 */
#include "basework/lib/string.h"
#include "basework/os/osapi_fs.h"
#include "basework/log.h"
#undef DT_REG
#undef DT_DIR

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>


struct file {
    struct vfs_file super;
    int fd;
};

static int unix_open(os_file_t fd, const char *path, 
    int flags, va_list ap) {
    struct file *fp = (struct file *)fd;
    int orw = flags & VFS_O_MASK;
    int oflags;

    (void) ap;
    switch (orw) {
    case VFS_O_RDONLY:
        oflags = O_RDONLY;
        break;
    case VFS_O_WRONLY:
        oflags = O_WRONLY;
        break;
    case VFS_O_RDWR:
        oflags = O_RDWR;
        break;
    default:
        return -EINVAL;
    }

    if (flags & VFS_O_CREAT)
        oflags |= O_CREAT;

    fp->fd = open(path, oflags, 0666);
    if (fp->fd < 0)
        return errno;
    return 0;
}

static int unix_close(os_file_t fd) {
    struct file *fp = (struct file *)fd;
    return close(fp->fd);
}

static int unix_ioctl(os_file_t fd, int cmd, void *args) {
    (void) fd;
    (void) cmd;
    (void) args;
    return -ENOSYS;
}

static ssize_t unix_read(os_file_t fd, void *buf, size_t len) {
    struct file *fp = (struct file *)fd;
    return read(fp->fd, buf, len);
}

static ssize_t unix_write(os_file_t fd, const void *buf, size_t len) {
    struct file *fp = (struct file *)fd;
    return write(fp->fd, buf, len);
}

static int unix_flush(os_file_t fd) {
    // struct file *fp = (struct file *)fd;
    return -ENOSYS;
}

static int unix_lseek(os_file_t fd, off_t offset, int whence) {
    struct file *fp = (struct file *)fd;
    return lseek(fp->fd, offset, whence);
}

static int unix_rename(os_filesystem_t fs, const char *oldpath, 
    const char *newpath) {
    (void) fs;
    return rename(oldpath, newpath);
}

static int unix_ftruncate(os_file_t fd, off_t length) {
    (void) fd;
    (void) length;
    return -ENOSYS;
}

static int unix_unlink(os_filesystem_t fs, const char *path) {
    (void) fs;
    if (path == NULL)
        return -ENOSYS;

    return unlink(path);
}


static int unix_opendir(const char* path, VFS_DIR* dirp) {
    DIR *dir;
    
    dir = opendir(path);
    if (dir == NULL)
        return errno;

    dirp->data = dir;
    return 0;
}

static int unix_readir(VFS_DIR* dirp, struct vfs_dirent* dirent) {
    struct dirent *dir;

_repeat:
    dir = readdir((DIR *)dirp->data);
    if (dir == NULL)
        return -ENODATA;
    
    switch (dir->d_type) {
    case DT_REG:
        dirent->d_type = 0;
        break;
    case DT_DIR:    
        dirent->d_type = 1;
        break;
    default:
        goto _repeat; 
    }

    strlcpy(dirent->d_name, dir->d_name, sizeof(dirent->d_name));
    return 0;
}

static int unix_closedir(VFS_DIR* dirp) {
    closedir((DIR *)dirp->data);
    return 0;
}

static off_t unix_tell(os_file_t fd) {
    // struct file* fp = (struct file*)fd;
    return -ENOSYS;
}

static int unix_stat(os_filesystem_t fs, const char* filename, 
    struct vfs_stat* buf) {
    struct stat st;
    int err;

    (void) fs;
    err = stat(filename, &st);
    if (!err) {
        buf->st_size = st.st_size;
        buf->st_blksize = st.st_blksize;
        buf->st_blocks = st.st_blocks;
    }
    return err;
}

static struct file os_files[10];
static struct file_class unix_file_class = {
    .mntpoint   = NULL,
	.fds_buffer = os_files,
	.fds_size   = sizeof(os_files),
	.fd_size    = sizeof(os_files[0]),
    .fs_priv    = NULL,
    .open = unix_open,
    .close = unix_close,
    .ioctl = unix_ioctl,
    .read = unix_read,
    .write = unix_write,
    .flush = unix_flush,
    .lseek = unix_lseek,
    .tell = unix_tell,
    .truncate = unix_ftruncate,
    .opendir = unix_opendir,
    .readdir = unix_readir,
    .closedir = unix_closedir,
    .mkdir = NULL,
    .unlink = unix_unlink,
    .stat = unix_stat,
    .rename = unix_rename,
};

int unix_init(void) {
	return vfs_register(&unix_file_class);
}
