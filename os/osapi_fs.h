/*
 * Copyright 2022 wtcat
 *
 * Borrowed from rt-thread
 */
#ifndef BASEWORK_OS_OSAPI_FS_H_
#define BASEWORK_OS_OSAPI_FS_H_

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "basework/generic.h"
#include "basework/container/queue.h"
#include "basework/os/osapi_obj.h"

#ifdef _MSC_VER
#define __ssize_t_defined
typedef long int ssize_t;

# undef  rte_unlikely
# define rte_unlikely(x) (x)

# define VFS_STATIC_INLINE static inline
#else /* !_MSC_VER */
# ifdef __cplusplus
#  define rte_unlikely(x) __builtin_expect(!!(x), 0)
# endif /* __cplusplus */
# define VFS_STATIC_INLINE static __rte_always_inline
#endif /* _MSC_VER */

#ifndef MAX_FILE_NAME
#define MAX_FILE_NAME 256
#endif

struct vfs_file;
typedef void *os_filesystem_t;
typedef struct vfs_file *os_file_t;

struct stat;
struct dirent;

#define VFS_O_RDONLY 0
#define VFS_O_WRONLY 1
#define VFS_O_RDWR   2
#define VFS_O_MASK   3

#define VFS_O_CREAT    0x0200
#define VFS_O_APPEND   0x0008
#define VFS_O_TRUNC    0x0010
#define VFS_O_FIFO     0x00100000
#define VFS_O_NONBLOCK 0x00000800

#define VFS_SEEK_SET 0 /* Seek from beginning of file.  */
#define VFS_SEEK_CUR 1 /* Seek from current position.  */
#define VFS_SEEK_END 2 /* Set file pointer to EOF plus "offset" */

/*
 * File I/O control type
 */
#define VFS_IOSET_FILESIZE 0x0100

struct vfs_stat {
	unsigned long st_size; /* File size */
	unsigned long st_blksize;
	unsigned long st_blocks;
};

struct vfs_dirent {
	uint8_t d_type;					/* Type of file */
#define DT_REG 0					/* file type */
#define DT_DIR 1					/* directory type*/
	char d_name[MAX_FILE_NAME + 1]; /* File name */
};
#undef MAX_FILE_NAME

typedef struct {
	os_file_t         fd;
	struct vfs_dirent entry;
	void             *data;
} VFS_DIR;

struct file_class {
	/* Mount point */
	const char        *mntpoint;

	/* Link to sibling node */
	STAILQ_ENTRY(file_class) link;

	/* File descriptor list */
	struct os_robj     avalible_fds;
	void              *fds_buffer;
	size_t             fds_size;
	size_t             fd_size;

	/* Private data */
	void              *fs_priv;

	/* File operations */
	int     (*open)(os_file_t fd, const char *path, int flags, va_list ap);
	int     (*close)(os_file_t fd);
	int     (*ioctl)(os_file_t fd, int cmd, void *args);
	ssize_t (*read)(os_file_t fd, void *buf, size_t count);
	ssize_t (*write)(os_file_t fd, const void *buf, size_t count);
	int     (*flush)(os_file_t fd);
	int     (*lseek)(os_file_t fd, off_t offset, int whence);
	int     (*getdents)(os_file_t fd, struct dirent *dirp, uint32_t count);
	int     (*truncate)(os_file_t fd, off_t length);
	off_t   (*tell)(os_file_t fd);
	// int (*poll)     (os_file_t *fd, struct rt_pollreq *req);

	/* Directory operations */
	int     (*opendir)(const char *path, VFS_DIR *dirp);
	int     (*readdir)(VFS_DIR *dirp, struct vfs_dirent *entry);
	int     (*closedir)(VFS_DIR *dirp);

	/* XIP map*/
	void   *(*mmap)(os_file_t fd, size_t *size);

	/* filesystem operations */
	int     (*fssync)(os_filesystem_t fs);
	int     (*fssync2)(os_filesystem_t fs, const char *mount);
	int     (*mkdir)(os_filesystem_t fs, const char *pathname);
	int     (*unlink)(os_filesystem_t fs, const char *pathname);
	int     (*stat)(os_filesystem_t fs, const char *filename, struct vfs_stat *buf);
	int     (*rename)(os_filesystem_t fs, const char *oldpath, const char *newpath);

	int     (*reset)(os_filesystem_t fs);
};

struct vfs_file {
	/* File system marker */
	uintptr_t         marker;
#define VFS_MARKER    0xFDFDFDFDul

	struct file_class *vfs;

    /* The fast path */
 	ssize_t (*read)(os_file_t fd, void *buf, size_t count);
	ssize_t (*write)(os_file_t fd, const void *buf, size_t count);
    int     (*ioctl)(os_file_t fd, int cmd, void *args);
	int     (*lseek)(os_file_t fd, off_t offset, int whence);
};

#define VFS_CHECK_VALID(fd, err) \
	if (rte_unlikely((fd)->marker != VFS_MARKER)) \
		return err

/*
 * vfs_read - Read data from file
 *
 * @fd: file descriptor
 * @buf: buffer pointer
 * @len: request size
 * return read bytes if success
 */
VFS_STATIC_INLINE ssize_t 
vfs_read(os_file_t fd, void *buf, size_t len) {
	VFS_CHECK_VALID(fd, -EINVAL);
#ifdef CONFIG_VFS_PARAM_CHECKER
	if (rte_unlikely(buf == NULL))
		return -EINVAL;
	if (rte_unlikely(len == 0))
		return 0;
#endif
	return fd->read(fd, buf, len);
}

/*
 * vfs_write - Write data to file
 *
 * @fd: file descriptor
 * @buf: buffer pointer
 * @len: buffer size
 * return writen bytes if success
 */
VFS_STATIC_INLINE ssize_t 
vfs_write(os_file_t fd, const void *buf, size_t len) {
	VFS_CHECK_VALID(fd, -EINVAL);
#ifdef CONFIG_VFS_PARAM_CHECKER
	if (rte_unlikely(buf == NULL))
		return -EINVAL;
	if (rte_unlikely(len == 0))
		return 0;
#endif
	return fd->write(fd, buf, len);
}

/*
 * vfs_ioctl - send command to file
 *
 * @fd: file descriptor
 * @cmd: command code
 * @arg: pointer to argument
 * return 0 if success
 */
VFS_STATIC_INLINE int 
vfs_ioctl(os_file_t fd, int cmd, void *args) {
	VFS_CHECK_VALID(fd, -EINVAL);
	if (fd->ioctl)
		return fd->ioctl(fd, cmd, args);
	return -ENOSYS;
}

/*
 * vfs_lseek - Set offset for file pointer
 *
 * @fd: file descriptor
 * @offset: file offset
 * @whence: base position
 * return 0 if success
 */
VFS_STATIC_INLINE int 
vfs_lseek(os_file_t fd, off_t offset, int whence) {
	VFS_CHECK_VALID(fd, -EINVAL);
	return fd->lseek(fd, offset, whence);
}

/*
 * vfs_open - Open or create a regular file
 *
 * @fd: file descriptor
 * @path: file path
 * @flags: open mode (VFS_O_XXXXXX)
 * return 0 if success
 */
int vfs_open(os_file_t *fd, const char *path, int flags, ...);

/*
 * vfs_close - Close a regular file
 *
 * @fd: file descritpor
 * return 0 if success
 */
int vfs_close(os_file_t fd);

/*
 * vfs_read - Read data from file
 *
 * @fd: file descriptor
 * @buf: buffer pointer
 * @len: request size
 * return read bytes if success
 */
ssize_t vfs_read(os_file_t fd, void *buf, size_t len);

/*
 * vfs_write - Write data to file
 *
 * @fd: file descriptor
 * @buf: buffer pointer
 * @len: buffer size
 * return writen bytes if success
 */
ssize_t vfs_write(os_file_t fd, const void *buf, size_t len);

/*
 * vfs_tell - Obtain the position of file pointer
 *
 * @fd: file descriptor
 * return the offset of file pointer
 */
off_t vfs_tell(os_file_t fd);

/*
 * vfs_flush - Sync file cache to disk
 *
 * @fd: file descriptor
 * return 0 if success
 */
int vfs_flush(os_file_t fd);

/*
 * vfs_ftruncate - Change file size
 *
 * @fd: file descriptor
 * @length: the new size for file
 * return 0 if success
 */
int vfs_ftruncate(os_file_t fd, off_t length);

/*
 * vfs_opendir - Open a directory
 *
 * @path: directory path
 * @dirp: directory object pointer
 * return 0 if success
 */
int vfs_opendir(const char *path, VFS_DIR *dirp);

/*
 * vfs_readdir - Read a directory
 *
 * @dirp: directory object pointer
 * @entry: directory content pointer
 * return 0 if success
 */
int vfs_readdir(VFS_DIR *dirp, struct vfs_dirent *entry);

/*
 * vfs_closedir - Close a directory
 *
 * @dirp: directory object pointer
 * return 0 if success
 */
int vfs_closedir(VFS_DIR *dirp);

/*
 * vfs_mkdir - Create a directory
 *
 * @path: directory path
 * return 0 if success
 */
int vfs_mkdir(const char *path);

/*
 * vfs_unlink - Delete a file or directory
 *
 * @path: directory path
 * return 0 if success
 */
int vfs_unlink(const char *path);

/*
 * vfs_rename - Rename a file
 *
 * @oldpath: current name
 * @newpath: new name
 * return 0 if success
 */
int vfs_rename(const char *oldpath, const char *newpath);

/*
 * vfs_stat - Get file status
 *
 * @path: file path
 * @buf: status handle
 * return 0 if success
 */
int vfs_stat(const char *path, struct vfs_stat *buf);

/*
 * vfs_dir_foreach - Iterate through the file directory
 *
 * @path: file path
 * @iterator: user callback (if need break and return true)
 * @arg: user parameter
 * return 0 if success
 */
int vfs_dir_foreach(const char *path, bool (*iterator)(struct vfs_dirent *dirent, void *),
					void *arg);

/*
 * vfs_sync - Flush filesystem cache to disk
 *
 * return 0 if success
 */
int vfs_sync(void);

/*
 * vfs_sync_cache - Flush filesystem cache to disk
 *
 * @mnt_point: mount point
 * return 0 if success
 */
int vfs_sync_cache(const char *mnt_point);

/*
 * vfs_reset - Reset filesystem data
 * Warnning: This operation will cause all files to be deleted
 *
 * @mpt: mount point
 * return 0 if success
 */
int vfs_reset(const char *mpt);

/*
 * vfs_mmap - Mapping file to memory address
 *
 * @fd: file descriptor
 * @size: mapped size
 * return memory address if success
 */
void *vfs_mmap(os_file_t fd, size_t *size);

/*
 * vfs_register - Register a filesystem
 */
int vfs_register(struct file_class *vfs);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_OS_OSAPI_FS_H_ */