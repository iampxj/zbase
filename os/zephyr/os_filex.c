/*
 * Copyright 2025 wtcat
 */

#include <errno.h>
#include <sys/types.h>

#include <init.h>
#include <fs/fs.h>
#include "fx_api.h"

#ifndef CONFIG_FILEX_INSTANCE_NUM
#define CONFIG_FILEX_INSTANCE_NUM 2
#endif

struct filex_mnt {
	struct fs_mount_t fs;
	FX_MEDIA media;
	FX_LOGDEVICE logdev;
    unsigned char used;
};

static struct filex_mnt filex_instance[CONFIG_FILEX_INSTANCE_NUM];

static int filex_do_mount(struct filex_mnt *fx, const char *mntp,
	const char *devname, off_t offset, size_t size) {
	if (!fx || !mntp || !devname || !size)
		return -EINVAL;
	
	fx->logdev.part_offset = offset;
	fx->logdev.part_size = size;
	fx->logdev.name = devname;

	fx->fs.mnt_point = mntp;
	fx->fs.fs_data = &fx->media;
	fx->fs.storage_dev = &fx->logdev;
	fx->fs.type = FS_FILEXFS;

	return fs_mount(&fx->fs);
}

int filex_mount(const char *mntp, const char *devname, 
    off_t offset, size_t size, int *id) {
    if (id == NULL)
        return -EINVAL;

    for (size_t i = 0; i < ARRAY_SIZE(filex_instance); i++) {
        struct filex_mnt *fx = filex_instance + i;
        if (!fx->used) {
            int err = filex_do_mount(fx, mntp, devname, offset, size);
            if (err == 0) {
                fx->used = 1;
                *id = i;
            }
            return err;
        }
    }

    return -ENOMEM; 
}

int filex_unmount(int filex_id) {
    if (filex_id >= CONFIG_FILEX_INSTANCE_NUM)
        return -EINVAL;
    if (!filex_instance[filex_id].used)
        return -EINVAL;

    return fs_unmount(&filex_instance[filex_id].fs);
}
