/*
 * Copyright 2022 wtcat
 */
#ifndef BASEWORK_DEV_DISK_H_
#define BASEWORK_DEV_DISK_H_

#include <stddef.h>
#include <stdint.h>

#include "basework/dev/device.h"
#include "basework/container/queue.h"

#ifdef __cplusplus
extern "C"{
#endif

struct disk_device {
    device_t dev;
    SLIST_ENTRY(disk_device) next;
    char name[32];

    /* flash device start address and len  */
    uint32_t addr;
    size_t len;
    size_t blk_size; /* the block size in the flash for erase minimum granularity */

    void *bdev; /* Pointer to block device */

    int (*read)(device_t dd, void *buf, size_t size, long offset);
    int (*write)(device_t dd, const void *buf, size_t size, long offset);
    int (*erase)(device_t dd, long offset, size_t size);
    int (*ioctl)(device_t dd, long cmd, void *arg);
};

/*
 * Disk commands
 */
#define DISK_GETBLKSIZE  0x10
#define DISK_GETCAPACITY 0x11
#define DISK_SYNC        0x12

struct disk_device *disk_device_next(struct disk_device *dd);
struct disk_device *disk_device_find(const char *name);
int disk_device_open(const char *name, struct disk_device **dd);
int disk_device_close(struct disk_device *dd);
int disk_device_write(struct disk_device *dd, const void *buf, size_t size, long offset);
int disk_device_read(struct disk_device *dd, void *buf, size_t size, long offset);
int disk_device_erase(struct disk_device *dd, long offset, size_t size);
int disk_device_ioctl(struct disk_device *dd, long cmd, void *arg);
int disk_device_register(struct disk_device *dd);

static inline size_t 
disk_device_get_block_size(const struct disk_device *dd) {
    return dd->blk_size;
}

static inline const char * 
disk_device_get_name(const struct disk_device *dd) {
    return dd->name;
}

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEV_DISK_H_ */
