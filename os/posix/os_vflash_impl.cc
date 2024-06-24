/*
 * Copyright 2022 wtcat
 */
#include <errno.h>
#include <string.h>
#include <assert.h>

#include "basework/dev/disk.h"
#include "basework/dev/partition.h"
#include "basework/ccinit.h"

#define FLASH_DEVNAME "virtual-flash"
#define FLASH_PGSZ 4096
#define FLASH_CAPACITY (5 *1024 * 1024)

static char virtual_flash_memory[FLASH_CAPACITY];

static const char *virt_flash_get_name(device_t dd) {
    (void) dd;
    return FLASH_DEVNAME;
}

static int virtual_flash_read(device_t dd, void *buf, size_t size, long offset) {
    (void) dd;
    if (offset + size > FLASH_CAPACITY)
        return -EINVAL;
    memcpy(buf, virtual_flash_memory + offset, size);
    return size;
}

static int virtual_flash_write(device_t dd, const void *buf, size_t size, long offset) {
    (void) dd;
    if (offset + size > FLASH_CAPACITY)
        return -EINVAL;
    memcpy(virtual_flash_memory + offset, buf, size);
    return size;
}

static int virtual_flash_erase(device_t dd, long offset, size_t size) {
    (void) dd;
    if (offset + size > FLASH_CAPACITY)
        return -EINVAL;
    if (offset % FLASH_PGSZ)
        return -EINVAL;
    if (size % FLASH_PGSZ)
        return -EINVAL;
    memset(virtual_flash_memory + offset, 0xFF, size);
    return 0;
}

static int virtual_flash_ioctl(device_t dd, long cmd, void *arg) {
    (void) dd;
    (void) cmd;
    (void) arg;
    return -ENOSYS;
}

CC_INIT(posix_vflash, kDeviceOrder, 10) {
    static struct disk_device vdisk;
    vdisk.blk_size = FLASH_PGSZ;
    vdisk.len = FLASH_CAPACITY;
    vdisk.dev = (device_t)1;
    vdisk.get_name = virt_flash_get_name;
    vdisk.read = virtual_flash_read;
    vdisk.write = virtual_flash_write;
    vdisk.erase = virtual_flash_erase;
    vdisk.ioctl = virtual_flash_ioctl;
    int err = disk_device_register(&vdisk);
    assert(err == 0);
    err = partitions_configure_build(0, FLASH_CAPACITY, FLASH_DEVNAME, false);
    assert(err == 0);
    return err;
}
