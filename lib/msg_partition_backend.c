/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <errno.h>
#include <stdint.h>

#include "basework/dev/blkdev.h"
#include "basework/dev/partition.h"
#include "basework/lib/msg_storage.h"
 
static int msg_partition_open(const char *name, void **fd) {
    const struct disk_partition *dp;
    dp = disk_partition_find(name);
    if (dp) {
        *fd = (void *)dp;
        return 0;
    }
    return -ENODATA;
}

static int msg_partition_close(void *fd) {
    (void) 0;
    return -ENOSYS;
}

static int msg_partition_flush(void *fd) {
    blkdev_sync();
    return 0;
}

static ssize_t msg_partition_write(void *fd, const void *buf, 
    size_t size, uint32_t offset) {
    return lgpt_write(fd, offset, buf, size);
}

static ssize_t msg_partition_read(void *fd, void *buf, 
    size_t size, uint32_t offset) {
    return lgpt_read(fd, offset, buf, size);
}

static int msg_partition_remove(const char *name) {
    const struct disk_partition *dp;
    dp = disk_partition_find(name);
    if (dp)
        return disk_partition_erase_all(dp);
    return -ENODEV;
}

int msg_partition_backend_init(void) {
    static const struct msg_fops msg_partition_ops = {
        .open = msg_partition_open,
        .close = msg_partition_close,
        .flush = msg_partition_flush,
        .write = msg_partition_write,
        .read = msg_partition_read,
        .unlink = msg_partition_remove
    };
	int err;
	
    err = msg_storage_register(&msg_partition_ops);
	if (!err)
		err = msg_storage_init();
	return err;
}