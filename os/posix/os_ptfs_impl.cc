/*
 * Copyright 2022 wtcat
 */
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "basework/dev/ptfs.h"
#include "basework/log.h"
#include "basework/os/osapi_config.h"
#include "basework/os/osapi_fs.h"

#include "basework/ccinit.h"

struct file {
	struct vfs_file super;
	struct ptfs_file *fp;
};

#define PTFS &ptfs_context

static struct ptfs_context ptfs_context;

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
	const char *fname = ptfs_get_filename(path);
	if (!fname)
		return -EINVAL;
	
	(void) ap;
	fp->fp = ptfile_ll_open(PTFS, fname, flags);
	if (!fp->fp)
		return -1;
	return 0;
}

static int ptfs_impl_close(os_file_t fd) {
	struct file *fp = (struct file *)fd;
	return ptfile_ll_close(PTFS, fp->fp);
}

static int ptfs_impl_ioctl(os_file_t fd, int cmd, void *args) {
	(void)fd;
	(void)cmd;
	(void)args;
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
	(void)fd;
	return -ENOSYS;
}

static int ptfs_impl_lseek(os_file_t fd, off_t offset, int whence) {
	struct file *fp = (struct file *)fd;
	return ptfile_ll_seek(PTFS, fp->fp, offset, whence);
}

static int ptfs_impl_rename(os_filesystem_t fs, const char *oldpath,
							const char *newpath) {
	(void)fs;
	(void)oldpath;
	(void)newpath;
	return -ENOSYS;
}

static int ptfs_impl_ftruncate(os_file_t fd, off_t length) {
	(void)fd;
	(void)length;
	return -ENOSYS;
}

static int ptfs_impl_unlink(os_filesystem_t fs, const char *path) {
	(void)fs;
	(void)path;
	const char *fname = ptfs_get_filename(path);
	if (!fname)
		return -EINVAL;
	return ptfile_ll_unlink(PTFS, fname);
}

static struct file os_files[CONFIG_OS_MAX_FILES];
static struct file_class ptfs_class = {
	.mntpoint = "/PTFS:",
	.fds_buffer = os_files,
	.fds_size = sizeof(os_files),
	.fd_size = sizeof(os_files[0]),

	.open = ptfs_impl_open,
	.close = ptfs_impl_close,
	.ioctl = ptfs_impl_ioctl,
	.read = ptfs_impl_read,
	.write = ptfs_impl_write,
	.flush = ptfs_impl_flush,
	.lseek = ptfs_impl_lseek,
	.truncate = ptfs_impl_ftruncate,
	.mkdir = NULL,
	.unlink = ptfs_impl_unlink,
	.stat = NULL,
	.rename = ptfs_impl_rename,

};

int ptfs_impl_init(void) {
	ptfile_ll_init(PTFS, "virtual-flash", UINT32_MAX);
	return vfs_register(&ptfs_class);
}

CC_INIT(posix_ptfs_init, kDeviceOrder, 20) {
	int err = ptfs_impl_init();
	assert(err == 0);
	return 0;
}