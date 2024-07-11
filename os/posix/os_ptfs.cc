/*
 * Copyright 2022 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define _VFS_PTFS_IMPLEMENT
#include <assert.h>
#include <errno.h>
#include <string.h>

#include "basework/dev/ptfs_ext.h"
#include "basework/log.h"
#include "basework/os/osapi_fs.h"

#include "basework/ccinit.h"

static struct ptfs_class ptfs_context;
static struct _pt_file os_files[CONFIG_OS_MAX_FILES];
static struct file_class ptfs_class = {
	.mntpoint = "/PTFS:",
	.fds_buffer = os_files,
	.fds_size = sizeof(os_files),
	.fd_size = sizeof(os_files[0]),
	.fs_priv = &ptfs_context,

	.open = ptfs_open,
	.close = ptfs_close,
	.ioctl = ptfs_ioctl,
	.read = ptfs_read,
	.write = ptfs_write,
	.flush = ptfs_flush,
	.lseek = ptfs_lseek,
	.truncate = ptfs_ftruncate,
	.opendir = ptfs_opendir,
	.readdir = ptfs_readir,
	.closedir = ptfs_closedir,
	.mkdir = NULL,
	.unlink = ptfs_unlink,
	.stat = ptfs_stat,
	.rename = ptfs_rename,
	.reset = ptfs_reset,
};

int ptfs_init(void) {
	pt_file_init(&ptfs_context, "virtual-flash", 0, 0x80000, 4096, 
		8, 0, true);
	return vfs_register(&ptfs_class);
}

CC_INIT(posix_ptfs_init, kDeviceOrder, 20) {
	int err = ptfs_init();
	assert(err == 0);
	return 0;
}