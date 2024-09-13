/*
 * Copyright 2022 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "partitons_cfg: "fmt
#include <errno.h>

#include "basework/dev/partition.h"
#include "basework/log.h"

#define KB(n) ((n) * 1024)
#define MB(n) (KB(n) * 1024ul)

#define PT_ENTRY(name, size) \
    PARTITION_ENTRY(name, NULL, -1, size)

/*
 * Partition configure table
 */
PARTITION_TABLE_DEFINE(partitions_configure) {
#ifdef CONFIG_CUSTOM_PT_TABLE
    #include CONFIG_MAJOR_PT_FILE
#else
    #include "config/major_ptcfg_default.h"
#endif
    PARTITION_TERMINAL
};


int partitions_configure_build(long base_addr, size_t size, 
    const char *phydev, bool sequence) {
    struct disk_partition *dp = partitions_configure;
    uint32_t offset;
    int err;

    if (sequence) {
        if (dp->offset == -1)
            dp->offset = base_addr;
        offset = dp->offset;
        while (dp->name) {
            if (dp->offset == -1)
                dp->offset = offset;
            if (!dp->parent)
                dp->parent = phydev;
            offset += dp->len;
            if (offset > base_addr + size) {
                pr_err("Paritiion(%s) size exceeds limit(overlap: %x)\n", 
                    dp->name, offset - base_addr+size);
                return -EINVAL;
            }
            dp++;
        }
    } else {
        if (dp->offset == -1)
            dp->offset = base_addr + size - dp->len;
        offset = dp->offset;
        while (dp->name) {
    _repeat:
            if (dp->offset == -1)
                dp->offset = offset;
            if (!dp->parent)
                dp->parent = phydev;
			pr_notice("##%s: partition(%s) base(0x%x) size(0x%x)\n", 
				__func__, dp->name, dp->offset, dp->len);
            dp++;
            if (dp->name) {
                offset -= dp->len;
                if ((long)offset < base_addr) {
                    pr_err("Paritiion(%s) size(0x%x) below lower limit"
						"(overlap: base(0x%x) offset(0x%x))\n", 
                        dp->name, dp->len, base_addr, offset);
                    return -EINVAL;
                }
                goto _repeat;
            }
            break;
        }
    }

    err = disk_partition_register(partitions_configure, 
        dp - partitions_configure);
    if (err) {
        pr_err("partition register failed: %d\n", err);
        return err;
    }
    
    disk_partition_dump();
    return 0;
}

int logic_partitions_create(const char *ppt, struct disk_partition *sublist) {
    struct disk_partition *dp = sublist;
    struct disk_partition *parent;
    uint32_t offset;
    uint32_t base_addr;

    if (!ppt || !sublist)
        return -EINVAL;

    parent = (struct disk_partition *)disk_partition_find(ppt);
    if (!parent)
        return -ENODEV;
    
    if (parent->child)
        return -EEXIST;

    base_addr = parent->offset;
    if (dp->offset == -1)
        dp->offset = base_addr;
    offset = dp->offset;

    while (dp->name) {
        if (dp->offset == -1)
            dp->offset = offset;
        dp->parent = parent->parent;
        offset += dp->len;
        if (offset > base_addr + parent->len)
            return -EINVAL;
        dp++;
    }

    parent->child = sublist;
    __disk_partition_dump("logic partition table", sublist, dp - sublist);
    return 0;
}

int partitions_configure_rebuild(long base_addr, size_t size, 
    const char *phydev, bool sequence) {
    struct disk_partition *dp = partitions_configure;

    while (dp && dp->name) {
        dp->offset = -1;
        dp++;
    }
    return partitions_configure_build(base_addr, size, 
        phydev, sequence);
}