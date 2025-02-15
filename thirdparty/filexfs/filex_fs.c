/*
 * Copyright 2025 wtcat
 */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/__assert.h>

#include <kernel.h>
#include <device.h>
#include <init.h>
#include <fs/fs.h>
#include <fs/fs_sys.h>
#include <drivers/flash.h>

#include "fx_api.h"

#ifndef CONFIG_FS_FILEX_NUM_DIRS
#define CONFIG_FS_FILEX_NUM_DIRS 3
#endif

#ifndef CONFIG_FS_FILEX_NUM_FILES
#define CONFIG_FS_FILEX_NUM_FILES 3
#endif

#ifndef CONFIG_FS_FILEX_MEDIA_BUFFER_SIZE
#define CONFIG_FS_FILEX_MEDIA_BUFFER_SIZE 4096
#endif

#ifndef CONFIG_FS_FILEX_NUM
#define CONFIG_FS_FILEX_NUM 3
#endif

struct dir_private {
#ifndef FX_NO_LOCAL_PATH
    FX_LOCAL_PATH local_path;
#endif
    CHAR *default_path;
    bool first;
};

#ifndef __ELASTERROR
#define __ELASTERROR (2000)
#endif

#define FX_PATH(_name) ((CHAR *)(_name) + fs->mountp_len)
#define FX_ERR(_err)   (((int)(_err) > 0)? _FX_ERR(_err): err)
#define _FX_ERR(_err) -(__ELASTERROR + (int)(_err))


/* Memory pool for FatFs directory objects */
K_MEM_SLAB_DEFINE(filexfs_dirp_pool, sizeof(struct dir_private),
			CONFIG_FS_FILEX_NUM_DIRS, 4);

/* Memory pool for FatFs file objects */
K_MEM_SLAB_DEFINE(filexfs_filep_pool, sizeof(FX_FILE),
			CONFIG_FS_FILEX_NUM_FILES, 4);

static char media_buffer[CONFIG_FS_FILEX_MEDIA_BUFFER_SIZE] __aligned(32);

extern UINT _fx_partition_offset_calculate(void  *partition_sector, UINT partition,
                                     ULONG *partition_start, ULONG *partition_size);

#ifdef FILEX_DEBUG
static void filex_mbr_dump(const uint8_t *prec) {
    printk("\\n*MBR dump:");
    for (int i = 0; i < 512; i++) {
        if (i % 16 == 0)
            printk("\n");
        printk("%02x ", prec[i]);
    }
}
#endif /* FILEX_DEBUG */

static int filex_media_read(FX_MEDIA *media_ptr, ULONG start_blkno, ULONG nblks) {
    if (start_blkno + nblks > 
        media_ptr->fx_media_sector_base_ + media_ptr->fx_media_sector_num_) {
        return -EINVAL;
    }

    ULONG bytes_per_sector = media_ptr->fx_media_bytes_per_sector;
    start_blkno += media_ptr->fx_media_sector_base_;
    return flash_read(media_ptr->fx_media_device_, start_blkno * bytes_per_sector,
        media_ptr->fx_media_driver_buffer, nblks * bytes_per_sector);
}

static int filex_media_write(FX_MEDIA *media_ptr, ULONG start_blkno, ULONG nblks) {
    if (start_blkno + nblks > 
        media_ptr->fx_media_sector_base_ + media_ptr->fx_media_sector_num_) {
        return -EINVAL;
    }

    ULONG bytes_per_sector = media_ptr->fx_media_bytes_per_sector;
    start_blkno += media_ptr->fx_media_sector_base_;
    int err = flash_erase(media_ptr->fx_media_device_, start_blkno * bytes_per_sector, 
        nblks * bytes_per_sector);
    if (err == 0) {
        err = flash_write(media_ptr->fx_media_device_, start_blkno * bytes_per_sector,
            media_ptr->fx_media_driver_buffer, nblks * bytes_per_sector);
    }
    return err;
}

static void filex_io_request(FX_MEDIA *media_ptr) {
	switch (media_ptr->fx_media_driver_request) {
	case FX_DRIVER_READ: {
        int err = filex_media_read(media_ptr, 
            media_ptr->fx_media_driver_logical_sector + media_ptr->fx_media_hidden_sectors,
            media_ptr->fx_media_driver_sectors);
        media_ptr->fx_media_driver_status = err;
        break;
    }

	case FX_DRIVER_WRITE: {
        int err = filex_media_write(media_ptr, 
            media_ptr->fx_media_driver_logical_sector + media_ptr->fx_media_hidden_sectors,
            media_ptr->fx_media_driver_sectors);
        media_ptr->fx_media_driver_status = err;
        break;
    }

	case FX_DRIVER_FLUSH:
		/* Return driver success.  */
        flash_flush(media_ptr->fx_media_driver_info, false);
		media_ptr->fx_media_driver_status = FX_SUCCESS;
		break;

	case FX_DRIVER_ABORT:
	case FX_DRIVER_INIT:
	case FX_DRIVER_UNINIT:
		media_ptr->fx_media_driver_status = FX_SUCCESS;
		break;

	case FX_DRIVER_BOOT_READ: {
        ULONG partition_start, partition_size;

        int err = filex_media_read(media_ptr, 0, media_ptr->fx_media_driver_sectors);
		if (err == 0) {
#ifdef FILEX_DEBUG
            filex_mbr_dump(media_ptr->fx_media_driver_buffer);
#endif
            err = _fx_partition_offset_calculate(media_ptr->fx_media_driver_buffer, 0,
                &partition_start, &partition_size);
            if (err) {
                media_ptr->fx_media_driver_status = FX_IO_ERROR;
                break;
            }
            if (partition_start) {
                /* Yes, now lets read the actual boot record.  */
                err = filex_media_read(media_ptr, partition_start, 
                    media_ptr->fx_media_driver_sectors);
            }
        }
        media_ptr->fx_media_driver_status = err;
		break;
    }

	case FX_DRIVER_BOOT_WRITE: {
        int err = filex_media_write(media_ptr, 0, media_ptr->fx_media_driver_sectors);
        media_ptr->fx_media_driver_status = err;
        break;
    }
	default: 
		/* Invalid driver request.  */
		media_ptr->fx_media_driver_status = FX_IO_ERROR;
		break;
	}
}

static int filex_open(struct fs_file_t *zfp, const char *file_name,
	fs_mode_t mode) {
    const struct fs_mount_t *fs = zfp->mp;
    bool created = false;
    void *ptr;
    UINT err;

	if (k_mem_slab_alloc(&filexfs_filep_pool, &ptr, K_NO_WAIT))
		return -ENOMEM;

    memset(ptr, 0, sizeof(FX_FILE));
    zfp->filep = ptr;

    if (mode & FS_O_CREATE) {
        err = fx_file_create(fs->fs_data, FX_PATH(file_name));
        if (err == FX_SUCCESS)
            created = true;
        else if (err != FX_ALREADY_CREATED)
            goto _failed;
    }

    int rw_flags = mode & (FS_O_WRITE | FS_O_READ);
    if (!rw_flags && !created) {
        err = -EINVAL;
        goto _failed;
    }

    err = fx_file_open(fs->fs_data, zfp->filep, FX_PATH(file_name), 
        rw_flags == FS_O_READ? FX_OPEN_FOR_READ: FX_OPEN_FOR_WRITE);
    if (err == FX_SUCCESS) {
        if ((rw_flags & FS_O_WRITE) && !created)
            fx_file_truncate(zfp->filep, 0);
        return 0;
    }

_failed:
    k_mem_slab_free(&filexfs_filep_pool, &zfp->filep);
    zfp->filep = NULL;
	return FX_ERR(err);
}

static int filex_close(struct fs_file_t *zfp) {
    UINT err;

    err = fx_file_close(zfp->filep);
    if (err == FX_SUCCESS) {
        k_mem_slab_free(&filexfs_filep_pool, &zfp->filep);
        return 0;
    }

    return _FX_ERR(err);
}

static int filex_unlink(struct fs_mount_t *mountp, const char *path) {
    struct fs_mount_t *fs = mountp;
    UINT attr, err;

    err = fx_file_attributes_read(fs->fs_data, FX_PATH(path), &attr);
    if (err == FX_SUCCESS)
        err = fx_file_delete(fs->fs_data, FX_PATH(path));
    else if (err == FX_NOT_A_FILE)
        err = fx_directory_delete(fs->fs_data, FX_PATH(path));
    return FX_ERR(err);
}

static int filex_rename(struct fs_mount_t *mountp, const char *from,
	const char *to) {
    struct fs_mount_t *fs = mountp;
    UINT attr, err;

    err = fx_file_attributes_read(fs->fs_data, FX_PATH(from), &attr);
    if (err == FX_SUCCESS)
        err = fx_file_rename(fs->fs_data, FX_PATH(from), FX_PATH(to));
    else if (err == FX_NOT_A_FILE)
        err = fx_directory_rename(fs->fs_data, FX_PATH(from), FX_PATH(to));
    return FX_ERR(err);
}

static ssize_t filex_read(struct fs_file_t *zfp, void *ptr, size_t size) {
    ULONG rdbytes;
    UINT err;

    err = fx_file_read(zfp->filep, ptr, size, &rdbytes);
    if (err == FX_SUCCESS)
        return (ssize_t)rdbytes;

    return _FX_ERR(err);
}

static ssize_t filex_write(struct fs_file_t *zfp, const void *ptr, size_t size) {
    UINT err;

    err = fx_file_write(zfp->filep, (VOID *)ptr, size);
    if (err == FX_SUCCESS)
        return size;

    return _FX_ERR(err);
}

static int filex_seek(struct fs_file_t *zfp, off_t offset, int whence) {
    UINT err;

    switch (whence) {
    case FS_SEEK_SET:
        err = fx_file_seek(zfp->filep, offset);
        break;
    case FS_SEEK_CUR:
        if (offset >= 0)
            err = fx_file_relative_seek(zfp->filep, offset, FX_SEEK_FORWARD);
        else
            err = fx_file_relative_seek(zfp->filep, -offset, FX_SEEK_BACK);
        break;
    case FS_SEEK_END:
        err = fx_file_relative_seek(zfp->filep, offset, FX_SEEK_END);
        break;
    default:
        return -EINVAL;
    }

    return FX_ERR(err);
}

static off_t filex_tell(struct fs_file_t *zfp) {
    FX_FILE *fxp = zfp->filep;
    return fxp->fx_file_current_file_size;
}

static int filex_truncate(struct fs_file_t *zfp, off_t length) {
    UINT err;

    err = fx_file_truncate(zfp->filep, length);
    return FX_ERR(err);
}

static int filex_sync(struct fs_file_t *zfp) {
    const struct fs_mount_t *fs = zfp->mp;
    UINT err;

    err = fx_media_flush(fs->fs_data);
    return FX_ERR(err);
}

static int filex_mkdir(struct fs_mount_t *mountp, const char *path) {
    struct fs_mount_t *fs = mountp;
    UINT err;

    err = fx_directory_create(fs->fs_data, FX_PATH(path));
    if (err == FX_ALREADY_CREATED)
        err = 0;
    return FX_ERR(err);
}

#ifdef FX_NO_LOCAL_PATH
static int filex_opendir(struct fs_dir_t *zdp, const char *path) {
    const struct fs_mount_t *fs = zdp->mp;
    struct dir_private *dir;
    UINT err;

	if (k_mem_slab_alloc(&filexfs_dirp_pool, (void **)&dir, K_NO_WAIT))
		return -ENOMEM;

    memset(dir, 0, sizeof(*dir));
    zdp->dirp = dir;
    err = fx_directory_default_get(fs->fs_data, &dir->default_path);
    if (err)
        goto _free;

    err = fx_directory_default_set(fs->fs_data, FX_PATH(path));
    if (err) {
        fx_directory_default_set(fs->fs_data, dir->default_path);
        goto _free;
    }

    dir->first = true;
    return 0;

_free:
    k_mem_slab_free(&filexfs_dirp_pool, &zdp->dirp);
    return FX_ERR(err);
}

static int filex_closedir(struct fs_dir_t *zdp) {
    struct dir_private *dir = zdp->dirp;
    if (dir) {
        const struct fs_mount_t *fs = zdp->mp;
        fx_directory_default_set(fs->fs_data, dir->default_path);
        k_mem_slab_free(&filexfs_dirp_pool, &zdp->dirp);
    }

    return 0;
}
#else /* !FX_NO_LOCAL_PATH */
static int filex_opendir(struct fs_dir_t *zdp, const char *path) {
    const struct fs_mount_t *fs = zdp->mp;
    struct dir_private *dir;
    UINT err;

	if (k_mem_slab_alloc(&filexfs_dirp_pool, (void **)&dir, K_NO_WAIT))
		return -ENOMEM;

    zdp->dirp = dir;
    err = fx_directory_local_path_set(fs->fs_data, &dir->local_path, FX_PATH(path));
    if (err) 
        goto _free;
 
    dir->first = true;
    return 0;

_free:
    k_mem_slab_free(&filexfs_dirp_pool, &zdp->dirp);
    return FX_ERR(err);
}

static int filex_closedir(struct fs_dir_t *zdp) {
    struct dir_private *dir = zdp->dirp;
    if (dir) {
        const struct fs_mount_t *fs = zdp->mp;
        fx_directory_local_path_clear(fs->fs_data);
        k_mem_slab_free(&filexfs_dirp_pool, &zdp->dirp);
    }

    return 0;
}
#endif /* FX_NO_LOCAL_PATH */

static int filex_readdir(struct fs_dir_t *zdp, struct fs_dirent *entry) {
    const struct fs_mount_t *fs = zdp->mp;
    struct dir_private *dir = zdp->dirp;
    ULONG size;
    UINT attr;
    UINT err;

    if (!dir->first) {
        err = fx_directory_next_full_entry_find(fs->fs_data, (CHAR *)entry->name,
            &attr, &size, NULL, NULL, NULL, NULL, NULL, NULL);
    } else {
        dir->first = false;
        err = fx_directory_first_full_entry_find(fs->fs_data, (CHAR *)entry->name,
            &attr, &size, NULL, NULL, NULL, NULL, NULL, NULL);
    }

    if (err == FX_SUCCESS) {
        if (!(attr & FX_DIRECTORY)) {
            entry->type = FS_DIR_ENTRY_FILE;
            entry->size = (size_t)size;
        } else {
            entry->type = FS_DIR_ENTRY_DIR;
            entry->size = 0;
        }
        return 0;    
    }

    return FX_ERR(err);
}

static int filex_stat(struct fs_mount_t *mountp, const char *path, 
    struct fs_dirent *entry) {
    const struct fs_mount_t *fs = mountp;
    ULONG size;
    UINT err;

    err = fx_directory_information_get(fs->fs_data, FX_PATH(path), NULL, &size,
        NULL, NULL, NULL, NULL, NULL, NULL);
    if (err == FX_SUCCESS) {
        //TODO: file entry name
        // entry->name
        entry->size = size;
        return 0;
    }

    return _FX_ERR(err);
}

static int filex_statvfs(struct fs_mount_t *mountp, const char *path, 
    struct fs_statvfs *stat) {
    return -ENOTSUP;
}

static int filex_mount(struct fs_mount_t *mountp) {
    const struct fs_mount_t *fs = mountp;
    FX_LOGDEVICE *ldev = fs->storage_dev;
    const struct flash_driver_api *api;
    const struct flash_pages_layout *layout;
    const struct device *dev;
    size_t layout_size;
    UINT  err;

    if (!ldev || !ldev->name || ldev->part_offset == 0)
        return -EINVAL;

    dev = device_get_binding(ldev->name);
    if (dev == NULL)
        return -ENODEV;

    api = (const struct flash_driver_api *)dev->api;
    __ASSERT_NO_MSG(api->page_layout != NULL);
    api->page_layout(dev, &layout, &layout_size);
    (void) layout_size;

    if (ldev->part_size < layout->pages_size || 
        (ldev->part_offset & (layout->pages_size - 1)))
        return -EINVAL;
    
    FX_MEDIA *media_ptr = fs->fs_data;
    media_ptr->fx_media_device_ = (void *)dev;
    media_ptr->fx_media_sector_base_ = ldev->part_offset / layout->pages_size;
    media_ptr->fx_media_sector_num_  = ldev->part_size / layout->pages_size;
    media_ptr->fx_media_bytes_per_sector = layout->pages_size;

    printk("@%s: sector_base(%d) sector_num(%d)\n", 
        __func__, 
        (int)media_ptr->fx_media_sector_base_, 
        (int)media_ptr->fx_media_sector_num_
    );
    err = fx_media_open(media_ptr, (CHAR *)dev->name, filex_io_request, 
        (VOID *)dev, media_buffer, sizeof(media_buffer));
    if (err == FX_MEDIA_INVALID || err == FX_BOOT_ERROR) {
        err = fx_media_format(media_ptr,               // Media instance
                        filex_io_request,              // Driver entry
                        (VOID *)dev,          // Device driver handle
                        (UCHAR *)media_buffer,     // Media buffer pointer
                        sizeof(media_buffer),     // Media buffer size
                        "FileX",                  // Volume Name
                        1,                     // Number of FATs
                        32,                 // Directory Entries
                        0,                     // Hidden sectors
                        media_ptr->fx_media_sector_num_,    // Total sectors
                        layout->pages_size,  // Sector size
                        32,               // Sectors per cluster
                        1,                              // Heads
                        1);                 // Sectors per track
        if (err == FX_SUCCESS) {
            err = fx_media_open(media_ptr, (CHAR *)dev->name, filex_io_request, 
                (VOID *)dev, media_buffer, sizeof(media_buffer));
            if (err == 0)
                err = fx_media_flush(media_ptr);
        }
    }

    return FX_ERR(err);
}

static int filex_unmount(struct fs_mount_t *mountp) {
    const struct fs_mount_t *fs = mountp;
    UINT err;

    err = fx_media_close(fs->fs_data);
    return FX_ERR(err);
}

/* File system interface */
static const struct fs_file_system_t filex_fs = {
	.open     = filex_open,
	.close    = filex_close,
	.read     = filex_read,
	.write    = filex_write,
	.lseek    = filex_seek,
	.tell     = filex_tell,
	.truncate = filex_truncate,
	.sync     = filex_sync,
	.opendir  = filex_opendir,
	.readdir  = filex_readdir,
	.closedir = filex_closedir,
	.mount    = filex_mount,
	.unmount  = filex_unmount,
	.unlink   = filex_unlink,
	.rename   = filex_rename,
	.mkdir    = filex_mkdir,
	.stat     = filex_stat,
	.statvfs  = filex_statvfs,
};

static int filex_init(const struct device *dev) {
	ARG_UNUSED(dev);
	return fs_register(FS_FILEXFS, &filex_fs);
}

SYS_INIT(filex_init, POST_KERNEL, 98);
