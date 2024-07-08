/*
 * Copyright 2022 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<vfs>: " fmt
#include <string.h>

#include "basework/assert.h"
#include "basework/log.h"
#include "basework/os/osapi.h"
#include "basework/os/osapi_config.h"
#include "basework/os/osapi_fs.h"


#ifdef _WIN32
#define rte_unlikely(x) (x)
#endif

#ifdef CONFIG_VFS_PARAM_CHECKER
#define FD2VFS(fd, vfs, err) \
	do { \
		rte_assert(fd != NULL); \
		(vfs) = (fd)->vfs; \
        rte_assert(vfs != NULL); \
		VFS_CHECK_VALID(fd, err); \
	} while (0)

#else /* CONFIG_VFS_PARAM_CHECKER_DISABLE */
#define FD2VFS(fd, vfs, err) \
	do { \
		(vfs) = (fd)->vfs; \
		VFS_CHECK_VALID(fd, err); \
	} while (0)

#endif /* !CONFIG_VFS_PARAM_CHECKER_DISABLE */

#define VFSSETFD(fd, vfs) (fd)->vfs = (vfs)

static STAILQ_HEAD(vfs_list, file_class) vfs_head = 
	STAILQ_HEAD_INITIALIZER(vfs_head);

static struct file_class *vfs_match(const char *path) {
	struct file_class *best_matched = NULL;
	struct file_class *vfs;
#ifndef _WIN32
	bool compare = true;
	const char *pend;
	if (path[0] != '/' || !(pend = strchr(path + 1, '/')))
		compare = false;
#endif /* !_WIN32 */

	STAILQ_FOREACH(vfs, &vfs_head, link) {
		if (!vfs->mntpoint) {
			best_matched = vfs;
			continue;
		}
#ifndef _WIN32
		if (compare && !strncmp(vfs->mntpoint, path, pend - path)) {
			best_matched = vfs;
			break;
		}
#endif /* !_WIN32 */
	}

	return best_matched;
}

int vfs_open(os_file_t *fd, const char *path, int flags, ...) {
	struct file_class *vfs;
	os_file_t fp;
	va_list ap;
	int err;

	if (fd == NULL || path == NULL)
		return -EINVAL;

	if (!(vfs = vfs_match(path)))
		return -EINVAL;

	fp = os_obj_allocate(&vfs->avalible_fds);
	rte_assert(fp != NULL);

	memset(fp, 0, sizeof(*fp));
	VFSSETFD(fp, vfs);
	va_start(ap, flags);
	err = vfs->open(fp, path, flags, ap);
	va_end(ap);
	if (err) {
		pr_err("Open file failed(%d)\n", err);
		os_obj_free(&vfs->avalible_fds, fp);
		return err;
	}

	if (fp->read == NULL)
		fp->read = vfs->read;
	if (fp->write == NULL)
		fp->write = vfs->write;
	if (fp->ioctl == NULL)
		fp->ioctl = vfs->ioctl;
	if (fp->lseek == NULL)
		fp->lseek = vfs->lseek;

	fp->marker = VFS_MARKER;
	*fd = fp;
	return 0;
}

int vfs_close(os_file_t fd) {
	struct file_class *vfs;
	int err;

    FD2VFS(fd, vfs, -EINVAL);
	err = vfs->close(fd);
	if (err) {
		pr_err("File close failed (%d)\n", err);
		return err;
	}
	os_obj_free(&vfs->avalible_fds, fd);
	return 0;
}

off_t vfs_tell(os_file_t fd) {
	struct file_class *vfs;

    FD2VFS(fd, vfs, -EINVAL);
    if (vfs->tell)
	    return vfs->tell(fd);
    
    return -ENOSYS;
}

int vfs_flush(os_file_t fd) {
	struct file_class *vfs;

    FD2VFS(fd, vfs, -EINVAL);
	if (vfs->flush)
		return vfs->flush(fd);

	return -ENOSYS;
}

int vfs_getdents(os_file_t fd, struct dirent *dirp, size_t nbytes) {
	struct file_class *vfs;
#ifdef CONFIG_VFS_PARAM_CHECKER
	if (dirp == NULL)
		return -EINVAL;
#endif
    FD2VFS(fd, vfs, -EINVAL);
	if (vfs->getdents)
		return vfs->getdents(fd, dirp, nbytes);

	return -ENOSYS;
}

int vfs_ftruncate(os_file_t fd, off_t length) {
	struct file_class *vfs;

    FD2VFS(fd, vfs, -EINVAL);
	if (vfs->truncate)
		return vfs->truncate(fd, length);

	return -ENOSYS;
}

int vfs_opendir(const char *path, VFS_DIR *dirp) {
	struct file_class *vfs;
	os_file_t fp;
	int err;

	if (path == NULL || dirp == NULL)
		return -EINVAL;

	if (!(vfs = vfs_match(path)))
		return -EINVAL;

	fp = os_obj_allocate(&vfs->avalible_fds);
	rte_assert(fp != NULL);

	VFSSETFD(fp, vfs);
	fp->marker = VFS_MARKER;
	dirp->fd = fp;

	err = vfs->opendir(path, dirp);
	if (err) {
		pr_err("Open directory failed(%d)\n", err);
		os_obj_free(&vfs->avalible_fds, fp);
		return err;
	}

	return 0;
}

int vfs_readdir(VFS_DIR *dirp, struct vfs_dirent *dirent) {
	struct file_class *vfs;

#ifdef CONFIG_VFS_PARAM_CHECKER
	if (dirp == NULL || dirent == NULL)
		return -EINVAL;
#endif
    FD2VFS(dirp->fd, vfs, -EINVAL);
	if (vfs->readdir)
		return vfs->readdir(dirp, dirent);

	return -ENOSYS;
}

int vfs_closedir(VFS_DIR *dirp) {
	struct file_class *vfs;
	int err;

#ifdef CONFIG_VFS_PARAM_CHECKER
	if (dirp == NULL)
		return -EINVAL;
#endif
	FD2VFS(dirp->fd, vfs, -EINVAL);
	err = vfs->closedir(dirp);
	if (err) {
		pr_err("Directory close failed (%d)\n", err);
		return err;
	}

	os_obj_free(&vfs->avalible_fds, dirp->fd);
	return 0;
}

int vfs_rename(const char *oldpath, const char *newpath) {
	struct file_class *vfs;

	if (oldpath == NULL || newpath == NULL)
		return -EINVAL;

	// TODO: check newpath
	if (!(vfs = vfs_match(oldpath)))
		return -EINVAL;

	if (vfs->rename)
	    return vfs->rename(vfs->fs_priv, oldpath, newpath);

    return -ENOSYS;
}

int vfs_mkdir(const char *path) {
	struct file_class *vfs;

	if (path == NULL)
		return -EINVAL;

	if (!(vfs = vfs_match(path)))
		return -EINVAL;

	if (vfs->mkdir)
	    return vfs->mkdir(vfs->fs_priv, path);

    return -ENOSYS;
}

int vfs_unlink(const char *path) {
	struct file_class *fp;

#ifdef CONFIG_VFS_PARAM_CHECKER
	if (path == NULL)
		return -EINVAL;
#endif
	if (!(fp = vfs_match(path)))
		return -EINVAL;

	if (fp->unlink)
	    return fp->unlink(fp->fs_priv, path);

    return -ENOSYS;
}

int vfs_stat(const char *path, struct vfs_stat *buf) {
	struct file_class *fp;

	if (path == NULL || buf == NULL)
		return -EINVAL;

	if (!(fp = vfs_match(path)))
		return -EINVAL;

	if (fp->stat)
	    return fp->stat(fp->fs_priv, path, buf);

    return -ENOSYS;
}

int vfs_sync(void) {
	struct file_class *vfs;
	int err = 0;

	STAILQ_FOREACH(vfs, &vfs_head, link) {
		if (vfs->fssync)
			err |= vfs->fssync(vfs->fs_priv);
	}
	return err;
}

int vfs_dir_foreach(const char *path, 
	bool (*iterator)(struct vfs_dirent *dirent, void *),
	void *arg) {
	struct vfs_dirent dirent;
	VFS_DIR dirp;
	int err;

	if (!iterator)
		return -EINVAL;

	err = vfs_opendir(path, &dirp);
	if (err)
		return err;

	while (!(err = vfs_readdir(&dirp, &dirent))) {
		if (iterator(&dirent, arg))
			break;
	}

	return vfs_closedir(&dirp);
}

void *vfs_mmap(os_file_t fd, size_t *size) {
	struct file_class *vfs;

    FD2VFS(fd, vfs, NULL);
	if (vfs->mmap)
		return vfs->mmap(fd, size);

	return NULL;
}

int vfs_reset(const char *mpt) {
	struct file_class *vfs;

	if (mpt == NULL)
		return -EINVAL;

	if (!(vfs = vfs_match(mpt)))
		return -EINVAL;

	if (vfs->reset)
		return vfs->reset(vfs->fs_priv);

	return -ENOSYS;
}

int vfs_register(struct file_class *cls) {
	struct file_class *vfs;
	int err;

	if (cls == NULL || cls->open == NULL || cls->close == NULL) {
		pr_err("invalid vfs node\n");
		err = -EINVAL;
		goto _out;
	}

	if (cls->fds_buffer == NULL) {
		pr_err("No file descriptor buffer\n");
		err = -EINVAL;
		goto _out;
	}

	if (cls->fds_size < cls->fd_size || 
		cls->fd_size == 0 || 
		cls->fds_size == 0) {
		pr_err("Invalid file descriptor parameters\n");
		err = -EINVAL;
		goto _out;
	}

	if (cls->mntpoint) {
		if (cls->mntpoint[0] != '/' || strchr(cls->mntpoint + 1, '/')) {
			pr_err("invalid vfs mount point(%s)\n", cls->mntpoint);
			err = -EINVAL;
			goto _out;
		}
	}

	STAILQ_FOREACH(vfs, &vfs_head, link) {
		if (cls->mntpoint == vfs->mntpoint || 
			(cls->mntpoint != NULL &&
			vfs->mntpoint  != NULL &&
			!strcmp(cls->mntpoint, vfs->mntpoint))) {
			pr_err("vfs node has exist\n");
			err = -EEXIST;
			goto _out;
		}
	}

	err = os_obj_initialize(&cls->avalible_fds, cls->fds_buffer, 
		cls->fds_size, cls->fd_size);					
	if (!err)
		STAILQ_INSERT_TAIL(&vfs_head, cls, link);

_out:
	rte_assert(err == 0);
	return err;
}