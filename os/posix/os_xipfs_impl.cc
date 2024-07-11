/*
 * Copyright 2022 wtcat
 */
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "basework/dev/xipfs.h"
#include "basework/log.h"
#include "basework/os/osapi_fs.h"

#include "basework/ccinit.h"

struct file {
	struct vfs_file super;
	struct xip_file *fp;
};

#define XIPFS &xipfs_context

static struct xipfs_context xipfs_context;

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
	return fxip_ll_ioctl(XIPFS, fp->fp, cmd, args);
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
	(void)fd;
	return -ENOSYS;
}

static int xipfs_impl_lseek(os_file_t fd, off_t offset, int whence) {
	struct file *fp = (struct file *)fd;
	return fxip_ll_seek(XIPFS, fp->fp, offset, whence);
}

static int xipfs_impl_rename(os_filesystem_t fs, const char *oldpath,
							 const char *newpath) {
	(void)fs;
	(void)oldpath;
	(void)newpath;
	return -ENOSYS;
}

static int xipfs_impl_ftruncate(os_file_t fd, off_t length) {
	(void)fd;
	(void)length;
	return -ENOSYS;
}

static int xipfs_impl_unlink(os_filesystem_t fs, const char *path) {
	(void)fs;
	(void)path;
	const char *fname = xipfs_get_filename(path);
	if (!fname)
		return -EINVAL;
	return fxip_ll_unlink(XIPFS, fname);
}

static struct file os_files[CONFIG_OS_MAX_FILES];
static struct file_class xipfs_class = {
	.mntpoint = "/XIPFS:",
	.fds_buffer = os_files,
	.fds_size = sizeof(os_files),
	.fd_size = sizeof(os_files[0]),

	.open = xipfs_impl_open,
	.close = xipfs_impl_close,
	.ioctl = xipfs_impl_ioctl,
	.read = xipfs_impl_read,
	.write = xipfs_impl_write,
	.flush = xipfs_impl_flush,
	.lseek = xipfs_impl_lseek,
	.truncate = xipfs_impl_ftruncate,
	.mkdir = NULL,
	.unlink = xipfs_impl_unlink,
	.stat = NULL,
	.rename = xipfs_impl_rename,

};

int xipfs_impl_init(void) {
	fxip_ll_init(XIPFS, "virtual-flash", UINT32_MAX);
	return vfs_register(&xipfs_class);
}

CC_INIT(posix_xipfs_init, kDeviceOrder, 20) {
	int err = xipfs_impl_init();
	assert(err == 0);
	return 0;
}