/*
 * Copyright 2023 wtcat
 */
#ifndef BASEWORK_DEV_PTFS_H_
#define BASEWORK_DEV_PTFS_H_

#include <stddef.h>
#include <stdint.h>

#include "basework/os/osapi.h"

#ifdef __cplusplus
extern "C"{
#endif

struct pt_inode;
struct file_metadata;
struct buffered_io;

struct pt_file {
    struct file_metadata *pmeta;
    uint32_t              rawofs;
    int                   oflags;
    bool                  written;
};

struct ptfs_class {
    /*
     * File manage data 
     */
    struct pt_inode      *p_inode;
    struct file_metadata **f_meta;
    uint32_t             *i_bitmap;
    uint32_t             *f_bitmap;
    uint16_t              i_bitmap_count;
    uint16_t              f_bitmap_count;
    uint32_t              p_inode_size;
    uint32_t              log2_blksize;

    os_mutex_t            mtx;
    struct disk_device   *dd;
    struct buffered_io   *bio;
    void                 *buffer;
    os_timer_t            timer;
    size_t                maxfiles;
    size_t                inodes;
    size_t                blksize;
    size_t                size;
    uint32_t              offset; /* content offset */
    bool                  dirty;

    /* 
     * Read data from block device 
     */
    ssize_t (*read)(struct ptfs_class *ctx, void *buf, size_t size, 
        uint32_t offset);

    /* 
     * Write data to block device 
     */
    ssize_t (*write)(struct ptfs_class *ctx, const void *buf, size_t size, 
        uint32_t offset);

    /*
     * Clear cache data
     */
    int (*flush)(struct ptfs_class *ctx);
};

int pt_file_open(struct ptfs_class *ctx, struct pt_file *filp,
    const char *name, int mode);

ssize_t pt_file_read(struct ptfs_class *ctx, struct pt_file *filp, 
    void *buffer, size_t size);

ssize_t pt_file_write(struct ptfs_class *ctx, struct pt_file *filp, 
    const void *buffer, size_t size);

int pt_file_close(struct ptfs_class *ctx, struct pt_file *filp);

int pt_file_seek(struct ptfs_class *ctx, struct pt_file *filp, 
    off_t offset, int whence);

int pt_file_unlink(struct ptfs_class *ctx, const char *name);

int pt_file_stat(struct ptfs_class *ctx, const char *name, 
    struct vfs_stat *buf);

const char *pt_file_getname(struct ptfs_class *ctx, int *idx);

void pt_file_reset(struct ptfs_class *ctx);

int pt_file_init(struct ptfs_class *ctx, const char *name, uint32_t start, 
    size_t size, size_t blksize, size_t maxfiles, uint32_t maxlimit, bool bio);



#ifdef _VFS_PTFS_IMPLEMENT
#include <errno.h>
#include "basework/lib/string.h"

struct _pt_file {
    struct vfs_file super;
    struct pt_file file;
};

#define FILE2FS(fp) (struct ptfs_class *)(fp)->super.vfs->fs_priv

static const char *filename_get(const char *name) {
    if (name[0] != '/')
        return NULL;
    const char *p = strchr(name, ':');
    if (!p || p[1] != '/')
        return NULL;
    return p + 2;
}

static int ptfs_open(os_file_t fd, const char *path, int flags, va_list ap) {
    struct _pt_file *fp = (struct _pt_file *)fd;
    const char *fname = filename_get(path);
    if (!fname)
        return -EINVAL;
    
    (void) ap;
    return pt_file_open(FILE2FS(fp), &fp->file, fname, flags);
}

static int ptfs_close(os_file_t fd) {
    struct _pt_file *fp = (struct _pt_file *)fd;
    return pt_file_close(FILE2FS(fp), &fp->file);
}

static int ptfs_ioctl(os_file_t fd, int cmd, void *args) {
    (void) fd;
    (void) cmd;
    (void) args;
    return -ENOSYS;
}

static ssize_t ptfs_read(os_file_t fd, void *buf, size_t len) {
    struct _pt_file *fp = (struct _pt_file *)fd;
    return pt_file_read(FILE2FS(fp), &fp->file, buf, len);
}

static ssize_t ptfs_write(os_file_t fd, const void *buf, size_t len) {
    struct _pt_file *fp = (struct _pt_file *)fd;
    return pt_file_write(FILE2FS(fp), &fp->file, buf, len);    
}

static int ptfs_flush(os_file_t fd) {
    (void) fd;
    return -ENOSYS;
}

static int ptfs_lseek(os_file_t fd, off_t offset, int whence) {
    struct _pt_file *fp = (struct _pt_file *)fd;
    return pt_file_seek(FILE2FS(fp), &fp->file, offset, whence); 
}

static int ptfs_opendir(const char *path, VFS_DIR *dirp) {
    const char *dir = filename_get(path);
    if (dir[-1] == '/' && dir[0] == '\0') {
        dirp->data = (void *)0;
        return 0;
    }
    return -EINVAL;
}

static int ptfs_readir(VFS_DIR *dirp, struct vfs_dirent *dirent) {
    struct _pt_file *fp = (struct _pt_file *)dirp->fd;
    const char *pname;
    int idx;

    idx = (int)(uintptr_t)dirp->data;
    pname = pt_file_getname(FILE2FS(fp), &idx);
    if (pname) {
        dirp->data = (void *)(uintptr_t)idx;
        dirent->d_type = DT_REG;
        strlcpy(dirent->d_name, pname, sizeof(dirent->d_name));
        return 0;
    }
    return -ENODATA;
}

static int ptfs_closedir(VFS_DIR *dirp) {
    (void) dirp;
    return 0;
}

static int ptfs_rename(os_filesystem_t fs, const char *oldpath, 
    const char *newpath) {
    (void) fs;
    (void) oldpath;
    (void) newpath;
    return -ENOSYS;
}

static int ptfs_ftruncate(os_file_t fd, off_t length) {
    (void) fd;
    (void) length;
    return -ENOSYS;
}

static int ptfs_unlink(os_filesystem_t fs, const char *path) {
    (void) fs;
    (void) path;
    const char *fname = filename_get(path);
    if (!fname)
        return -EINVAL;
    return pt_file_unlink((struct ptfs_class *)fs, fname);
}

static int ptfs_stat(os_filesystem_t fs, const char *name, 
    struct vfs_stat *buf) {
    const char *fname = filename_get(name);
    (void) fs;
    return pt_file_stat((struct ptfs_class *)fs, fname, buf); 
}

static int ptfs_reset(os_filesystem_t fs) {
    (void) fs;
    pt_file_reset((struct ptfs_class *)fs);
    return 0;
}
#endif /* _VFS_PTFS_IMPLEMENT */

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEV_PTFS_H_ */

