/*
 * Copyright 2022 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "basework/os/osapi_fs.h"
#include "basework/log.h"

struct file {
	struct vfs_file super;
    FILE *fp;
};

#ifndef CONFIG_OS_MAX_FILES
#define CONFIG_OS_MAX_FILES 20
#endif

static int cstd_fs_open(os_file_t fd, const char *path, 
    int flags, va_list ap) {
    struct file *fp = (struct file *)fd;
    int orw = flags & VFS_O_MASK;
    char mode[4] = {0};

    (void) ap;
    switch (orw) {
    case VFS_O_RDONLY:
        strcpy(mode, "rb");
        break;
    case VFS_O_WRONLY:
        strcpy(mode, "wb");
        if (orw & VFS_O_APPEND)
            strcpy(mode, "ab");
        break;
    case VFS_O_RDWR:
        strcpy(mode, "wb+");
        if (orw & VFS_O_APPEND)
            strcpy(mode, "ab+");
        break;
    default:
        return -EINVAL;
    }

    fp->fp = fopen(path, mode);
    if (!fp->fp)
        return -ENXIO;
    return 0;
}

static int cstd_fs_close(os_file_t fd) {
    struct file *fp = (struct file *)fd;
    return fclose(fp->fp);
}

static int cstd_fs_ioctl(os_file_t fd, int cmd, void *args) {
    (void) fd;
    (void) cmd;
    (void) args;
    return -ENOSYS;
}

static int cstd_fs_read(os_file_t fd, void *buf, size_t len) {
    struct file *fp = (struct file *)fd;
    size_t size;

    size = fread(buf, 1, len, fp->fp);
    if (len == 0)
        return -EIO;
    return size;
}

static int cstd_fs_write(os_file_t fd, const void *buf, size_t len) {
    struct file *fp = (struct file *)fd;
    size_t size;

    size = fwrite(buf, len, 1, fp->fp);
    if (size < len)
        return -EIO;
    return size;
}

static int cstd_fs_flush(os_file_t fd) {
    struct file *fp = (struct file *)fd;
    return fflush(fp->fp);
}

static int cstd_fs_lseek(os_file_t fd, off_t offset, int whence) {
    struct file *fp = (struct file *)fd;
    return fseek(fp->fp, offset, whence);
}

static int cstd_fs_rename(os_filesystem_t fs, const char *oldpath, 
    const char *newpath) {
    (void) fs;
    (void) oldpath;
    (void) newpath;
    return -ENOSYS;
}

static int cstd_fs_ftruncate(os_file_t fd, off_t length) {
    (void) fd;
    (void) length;
    return -ENOSYS;
}

static int cstd_fs_unlink(os_filesystem_t fs, const char *path) {
    (void) fs;

    if(path == NULL){
        return -ENOSYS;
    }

    remove(path);

    return -ENOSYS;
}

#ifdef _WIN32
#define WIN32FILE_EXCLUDE_ATTR \
    (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_ENCRYPTED)
static int cstd_fs_opendir(const char* path, VFS_DIR* dirp) {
    WIN32_FIND_DATA fdata;
    HANDLE handle;
    size_t slen;

    handle = FindFirstFile(path, &fdata);
    if (!handle) {
        printf("file path(%s) is invalid\n", path);
        return -EINVAL;
    }

    dirp->data = handle;
    slen = wcslen(fdata.cFileName);
    if (!(fdata.dwFileAttributes & WIN32FILE_EXCLUDE_ATTR)) {
        memset(&dirp->entry, 0, sizeof(dirp->entry));
        WideCharToMultiByte(CP_ACP, 0, fdata.cFileName, slen,
            dirp->entry.d_name, sizeof(dirp->entry.d_name), NULL, NULL);
        dirp->entry.d_name[slen] = '\0';
        dirp->entry.d_type = DT_REG;
    }
    else {
        dirp->entry.d_name[0] = '\0';
    }
    return 0;
}

static int cstd_fs_readir(VFS_DIR* dirp, struct vfs_dirent* dirent) {
    HANDLE handle = dirp->data;
    WIN32_FIND_DATA fdata = {0};
    size_t slen;

    if (dirp->entry.d_name[0]) {
        dirp->entry.d_name[0] = '\0';
        goto _copy;
    }

    while (FindNextFile(handle, &fdata)) {
        if (fdata.dwFileAttributes & WIN32FILE_EXCLUDE_ATTR)
            continue;
_copy:
        slen = wcslen(fdata.cFileName);
        WideCharToMultiByte(CP_ACP, 0, fdata.cFileName, slen,
            dirent->d_name, sizeof(dirent->d_name), NULL, NULL);
        dirent->d_name[slen] = '\0';
        dirent->d_type = DT_REG;
        return 0;
    }

    return -ENODATA;
}

static int cstd_fs_closedir(VFS_DIR* dirp) {
    FindClose(dirp->data);
    return 0;
}

#endif /* _WIN32 */

static off_t cstd_fs_tell(os_file_t fd) {
    struct file* fp = (struct file*)fd;
    return ftell(fp->fp);
}

static int cstd_fs_stat(os_filesystem_t fs, const char* filename, 
    struct vfs_stat* buf) {
    FILE* fp = fopen(filename, "rb");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        buf->st_size = (size_t)ftell(fp);
        buf->st_blksize = 0;
        buf->st_blocks = 0;
        fclose(fp);
    }
}

static struct file os_files[CONFIG_OS_MAX_FILES];
static struct file_class cstd_file_class = {
    .mntpoint   = NULL,
	.fds_buffer = os_files,
	.fds_size   = sizeof(os_files),
	.fd_size    = sizeof(os_files[0]),

    .open = cstd_fs_open,
    .close = cstd_fs_close,
    .ioctl = cstd_fs_ioctl,
    .read = cstd_fs_read,
    .write = cstd_fs_write,
    .flush = cstd_fs_flush,
    .lseek = cstd_fs_lseek,
    .tell = cstd_fs_tell,
    .truncate = cstd_fs_ftruncate,
#ifdef _WIN32
    .opendir = cstd_fs_opendir,
    .readdir = cstd_fs_readir,
    .closedir = cstd_fs_closedir,
#endif
    .mkdir = NULL,
    .unlink = cstd_fs_unlink,
    .stat = cstd_fs_stat,
    .rename = cstd_fs_rename,
};

int cstd_fs_init(void) {
	return vfs_register(&cstd_file_class);
}
