/*
 * Copyright 2022 wtcat
 *
 * This is a simple partition table implement
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<pt>: " fmt
#define CONFIG_LOGLEVEL LOGLEVEL_INFO
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "basework/dev/partition.h"
#include "basework/dev/disk.h"
#include "basework/log.h"
#include "basework/malloc.h"
#include "basework/dev/blkdev.h"


#ifndef DISK_DEV_NAME_MAX
#define DISK_DEV_NAME_MAX 24
#endif

static const struct disk_partition *partition_table;
static struct disk_device **parition_disk_cache;
static size_t partition_table_len;

static const struct disk_partition *partition_match(const char *in) {
    const struct disk_partition *dp, *dpii;
    char *p = strchr(in, '/');
    for (size_t i = 0; i < partition_table_len; i++) {
        dp = &partition_table[i];
        if (!p) {
           if (!strcmp(in, dp->name))
                return dp;
            continue;
        }
        if (!strncmp(in, dp->name, p - in)) {
            p++;
            dpii = dp->child;
            while (dpii && dpii->name) {
                if (!strcmp(p, dpii->name))
                    return dpii;
                dpii++;
            }
        }
    }
    return NULL;
}

struct disk_device *disk_device_find_by_part(const struct disk_partition *part) {
    assert(part >= partition_table);
    assert(part <= &partition_table[partition_table_len - 1]);
    return parition_disk_cache[part - partition_table];
}

const struct disk_partition *disk_partition_find(const char *name) {
    return partition_match(name);
}

const struct disk_partition *disk_get_partition_table(size_t *len) {
    if (len != NULL)
        *len = partition_table_len;
    return partition_table;
}

int lgpt_read(const struct disk_partition *part, uint32_t addr, 
    void *buf, size_t size) {
    struct disk_device *dd = NULL;
    int ret = 0;

    assert(part != NULL);
    assert(buf != NULL);
    if (addr + size > part->len) {
        pr_err("Partition address out of bound.");
        return -EINVAL;
    }
    dd = disk_device_find_by_part(part);
    if (dd == NULL) {
        pr_err("Not found flash device(%s) of the partition(%s).", 
            part->parent, part->name);
        return -EINVAL;
    }
    ret = blkdev_read(dd, buf, size, part->offset + addr);
    if (ret < 0) {
        pr_err("Flash device(%s) read error!", part->parent);
    }

    return ret;
}

int lgpt_write(const struct disk_partition *part, uint32_t addr, 
    const void *buf, size_t size) {
    struct disk_device *dd = NULL;
    int ret = 0;

    assert(part != NULL);
    assert(buf != NULL);
    if (addr + size > part->len) {
        pr_err("Partition address out of bound.");
        return -EINVAL;
    }
    dd = disk_device_find_by_part(part);
    if (dd == NULL) {
        pr_err("Not found flash device(%s) of the partition(%s).", 
            part->parent, part->name);
        return -EINVAL;
    }
    ret = blkdev_write(dd, buf, size, part->offset + addr);
    if (ret < 0)  {
        pr_err("Flash device(%s) write error!", part->parent);
    }

    return ret;
}

int lgpt_get_block_size(const struct disk_partition *part, size_t *blksz) {
    struct disk_device *dd = NULL;
    assert(part != NULL);

    if (blksz == NULL)
        return -EINVAL;
    dd = disk_device_find_by_part(part);
    if (dd == NULL) {
        pr_err("Not found flash device(%s) of the partition(%s).", 
            part->parent, part->name);
        return -EINVAL;
    }

    *blksz = dd->blk_size;
    return 0;
}

int disk_partition_read(const struct disk_partition *part, uint32_t addr, 
    void *buf, size_t size) {
    struct disk_device *dd = NULL;
    int ret = 0;

    assert(part != NULL);
    assert(buf != NULL);
    if (addr + size > part->len) {
        pr_err("Partition read error! Partition address out of bound.");
        return -EINVAL;
    }
    dd = disk_device_find_by_part(part);
    if (dd == NULL) {
        pr_err("Partition read error! Don't found flash device(%s) of the partition(%s).", 
            part->parent, part->name);
        return -EINVAL;
    }
    ret = disk_device_read(dd, buf, size, part->offset + addr);
    if (ret < 0) {
        pr_err("Partition read error! Flash device(%s) read error!", part->parent);
    }

    return ret;
}

int disk_partition_write(const struct disk_partition *part, uint32_t addr, 
    const void *buf, size_t size) {
    struct disk_device *dd = NULL;
    int ret = 0;

    assert(part != NULL);
    assert(buf != NULL);
    if (addr + size > part->len) {
        pr_err("Partition write error! Partition address out of bound.");
        return -EINVAL;
    }
    dd = disk_device_find_by_part(part);
    if (dd == NULL) {
        pr_err("Partition write error!  Don't found flash device(%s) of the partition(%s).", 
            part->parent, part->name);
        return -EINVAL;
    }
    ret = disk_device_write(dd, buf, size, part->offset + addr);
    if (ret < 0)  {
        pr_err("Partition write error! Flash device(%s) write error!", part->parent);
    }

    return ret;
}

int disk_partition_erase(const struct disk_partition *part, uint32_t addr, size_t size) {
    struct disk_device *dd;
    int ret = 0;

    assert(part != NULL);
    if (addr + size > part->len) {
        pr_err("Partition erase error! Partition address out of bound.");
        return -EINVAL;
    }
    dd = disk_device_find_by_part(part);
    if (dd == NULL) {
        pr_err("Partition erase error! Don't found flash device(%s) of the partition(%s).", 
            part->parent, part->name);
        return -EINVAL;
    }
    assert((long)(part->offset + addr) > 0);
    ret = disk_device_erase(dd, part->offset + addr, size);
    if (ret < 0) {
        pr_err("Partition erase error! Flash device(%s) erase error!", part->parent);
    }

    return ret;
}

int disk_partition_erase_all(const struct disk_partition *part) {
    return disk_partition_erase(part, 0, part->len);
}

void __disk_partition_dump(const char *title, 
    const struct disk_partition *dp, size_t len) {
    char *item1 = "name", *item2 = "device";
    size_t i, part_name_max, flash_dev_name_max;
    const struct disk_partition *part;

    part_name_max = strlen(item1);
    flash_dev_name_max = strlen(item2);
    if (len) {
        for (i = 0; i < len; i++) {
            part = &dp[i];
            if (strlen(part->name) > part_name_max)
                part_name_max = strlen(part->name);
            if (strlen(part->parent) > flash_dev_name_max)
                flash_dev_name_max = strlen(part->parent);
        }
    }

    pr_notice("==================== %s ====================\n", title);
    pr_notice("| %-*.*s | %-*.*s |   offset   |    length  |\n", 
        part_name_max, 
        DISK_DEV_NAME_MAX, 
        item1, 
        flash_dev_name_max,
        DISK_DEV_NAME_MAX, 
        item2
    );

    pr_notice("-------------------------------------------------------------\n");
    for (i = 0; i < len; i++) {
        part = &dp[i];
        pr_notice("| %-*.*s | %-*.*s | 0x%08lx | 0x%08x |\n", 
            part_name_max, 
            DISK_DEV_NAME_MAX, 
            part->name, 
            flash_dev_name_max,
            DISK_DEV_NAME_MAX, 
            part->parent, 
            part->offset, 
            part->len
        );
    }
    pr_notice("=============================================================\n");
}

void disk_partition_dump(void) {
    __disk_partition_dump("disk partition table", 
        partition_table, partition_table_len);
}

int disk_partition_register(const struct disk_partition *pt, size_t len) {
    const struct disk_partition *dp;
    struct disk_device **caches;
    struct disk_device *dd;
    size_t i;

    if (!pt || !len) {
        pr_warn("invalid parameters!\n");
        return -EINVAL;
    }

    caches = (struct disk_device **)general_malloc(sizeof(caches) * len);
    if (!caches) {
        pr_warn("no more memory!\n");
        return -ENOMEM;
    }

    for (i = 0, dp = pt; i < len; i++) {
        if (disk_device_open(dp->parent, &dd)) {
            pr_err("Not found device(%s)\n", dp->parent);
            goto _free;
        }
        caches[i] = dd;
    }

    if (parition_disk_cache)
        general_free(parition_disk_cache);

    parition_disk_cache = caches;
    partition_table = pt;
    partition_table_len = len;
    return 0;

_free:
    general_free(caches);
    return -EIO;
}

