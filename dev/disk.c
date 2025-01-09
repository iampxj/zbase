/*
 * Copyright 2022 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#else
#define CONFIG_ASSERT_DISABLE
#endif

#define pr_fmt(fmt) "<disk>: "fmt

#include <errno.h>
#include <string.h>

#include "basework/assert.h"
#include "basework/log.h"
#include "basework/dev/disk.h"
#include "basework/dev/blkdev.h"

#define DISK_FOREACH(dd) \
    SLIST_FOREACH(dd, &disk_head, next)

static SLIST_HEAD(disk_list, disk_device) disk_head;

struct disk_device *disk_device_next(struct disk_device *dd) {
    if (dd != NULL)
        return SLIST_NEXT(dd, next);
    return SLIST_FIRST(&disk_head);
}

struct disk_device *disk_device_find(const char *name) {
    struct disk_device *pd;
    if (name != NULL) {
        DISK_FOREACH(pd) {
            if (!strcmp(pd->name, name))
                return pd;
        }
    }
    return NULL;
}

int disk_device_open(const char *name, struct disk_device **dd) {
    rte_assert(dd != NULL);
    *dd = disk_device_find(name);
    if (*dd == NULL)
        return -ENODEV;

    return 0;
}

int disk_device_close(struct disk_device *dd) {
    (void) dd;
    return 0;
}

int disk_device_write(struct disk_device *dd, const void *buf, size_t size, 
    long offset) {
    rte_assert(dd != NULL);
    rte_assert(dd->write != NULL);

#ifdef CONFIG_DISK_PARAM_CHECKER
    if (rte_unlikely(buf == NULL))
        return -EINVAL;

    if (rte_unlikely(size == 0))
        return 0;

    if (rte_unlikely(offset + size > dd->len)) {
        pr_err("Disk read address(0x%08x size<0x%08x>) out of bound.\n",
            offset, size);
        return -EINVAL;
    }
#endif /* CONFIG_DISK_PARAM_CHECKER */
    return dd->write(dd->dev, buf, size, offset);
}

int disk_device_read(struct disk_device *dd, void *buf, size_t size, 
    long offset) {
    rte_assert(dd != NULL);
    rte_assert(dd->read != NULL);

#ifdef CONFIG_DISK_PARAM_CHECKER
    if (rte_unlikely(buf == NULL))
        return -EINVAL;

    if (rte_unlikely(size == 0))
        return 0;

    if (rte_unlikely(offset + size > dd->len)) {
        pr_err("Disk read address(0x%08x size<0x%08x>) out of bound.\n",
            offset, size);
        return -EINVAL;
    }
#endif /* CONFIG_DISK_PARAM_CHECKER */

    return dd->read(dd->dev, buf, size, offset);
}

int disk_device_erase(struct disk_device *dd, long offset, 
    size_t size) {
    rte_assert(dd != NULL);
    rte_assert(dd->erase != NULL);

#ifdef CONFIG_DISK_PARAM_CHECKER
    if (rte_unlikely(offset & (dd->blk_size - 1)))
        return -EINVAL;

    if (rte_unlikely(size & (dd->blk_size - 1)))
        return -EINVAL;

    if (rte_unlikely(size == 0))
        return 0;

    if (offset + size > dd->len) {
        pr_err("Disk erease address(0x%08x size<0x%08x>) out of bound\n", 
            offset, size);
        return -EINVAL;
    }
#endif /* CONFIG_DISK_PARAM_CHECKER */

    return dd->erase(dd->dev, offset, size);
}

int disk_device_ioctl(struct disk_device *dd, long cmd, void *arg) {
    rte_assert(dd != NULL);
    rte_assert(dd->ioctl != NULL);
    switch (cmd) {
    case DISK_GETBLKSIZE:
        rte_assert(arg != NULL);
        *(size_t *)arg = dd->blk_size;
        break;
    case DISK_GETCAPACITY:
        rte_assert(arg != NULL);
        *(size_t *)arg = dd->len;
        break;
    default:
        return dd->ioctl(dd->dev, cmd, arg);
    }
    return 0;
}

int disk_device_register(struct disk_device *dd) {
    struct disk_device *pd;

    if (dd == NULL) {
        pr_err("invalid parameter\n");
        return -EINVAL;
    }
    if (dd->dev == NULL || dd->len == 0) {
        pr_err("invalid device handle or disk size\n");
        return -EINVAL;
    }
    if (dd->blk_size & (dd->blk_size - 1)) {
        pr_err("invalid disk block size\n");
        return -EINVAL;
    }
    if (dd->read == NULL || 
        dd->write == NULL || 
        dd->erase == NULL) {
        pr_err("no base operations\n");
        return -EINVAL;
    }

    DISK_FOREACH(pd) {
        if (pd == dd)
            return -EEXIST;
    }

#ifndef CONFIG_BOOTLOADER   
    blkdev_init(dd);
#endif
    SLIST_INSERT_HEAD(&disk_head, dd, next);
    return 0;
}
