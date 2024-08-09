/*
 * Copyright 2023 wtcat
 */
#define CONFIG_LOGLEVEL LOGLEVEL_DEBUG
#define pr_fmt(fmt) "<os_logdisk>: "fmt
#include <errno.h>
#include <string.h>

#include <drivers/disk.h>
#include <board_cfg.h>
#include <fs_manager.h>
#include <partition/partition.h>

#include "basework/dev/partition.h"
#include "basework/malloc.h"
#include "basework/log.h"

struct fstab_info {
    const char *name;
    const char *type;
    const char *dev;
    const char *mp;
    size_t limit_size;
};

static const struct fstab_info fs_table[] = {
    {"filesystem",      "littlefs", "UD0", "/UD0:", UINT32_MAX},
    {"firmware",        "littlefs", "UD1", "/UD1:", 0x1c2000},  /* Limit to 1.8MB */
#if !defined(CONFIG_PTFS) && \
	!defined(CONFIG_PAGEFS) && \
	!defined(CONFIG_XIPFS)
    {"udisk",          "fatfs", "UD2", "/UD2:", UINT32_MAX},  /* just only for  */
#endif
};

enum {
    FS_NUMBER        = ARRAY_SIZE(fs_table),
    LITTLEFS_NUMBER  = 2,
    FATFS_NUMBER     = FS_NUMBER - LITTLEFS_NUMBER
};

#define NOR_LOG_SIZE 9 
#define NOR_SECTOR_SIZE (1 << NOR_LOG_SIZE)


static struct fs_mount_t mount_entry[FS_NUMBER];

#ifdef CONFIG_FILE_SYSTEM_LITTLEFS
#include <fs/littlefs.h>

FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(liitefs_storage_1);
FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(liitefs_storage_2);
static struct fs_littlefs *littlefs_dat[LITTLEFS_NUMBER] = {
    &liitefs_storage_1,
    &liitefs_storage_2
};

static struct flash_area fs_flash_map[LITTLEFS_NUMBER];
const struct flash_area *flash_map = fs_flash_map;
const int flash_map_entries = LITTLEFS_NUMBER;
#endif /* !CONFIG_FILE_SYSTEM_LITTLEFS */

#ifdef CONFIG_FAT_FILESYSTEM_ELM
extern struct disk_operations disk_nor_operation;
static FATFS fat_data[FATFS_NUMBER];

/*
 * Create logic device base on norflash device
 */
static int platform_nor_logdisk_create(const char *name, 
    uint32_t offset, uint32_t size) {
    struct disk_info *di;

    if (!name) {
        pr_err("Invalid physic device(null)\n");
        return -EINVAL;
    }
    if (offset + size > CONFIG_SPI_FLASH_CHIP_SIZE) {
        pr_err("Invalid offset or size\n");
        return -EINVAL;
    }
    if (size < NOR_SECTOR_SIZE) {
        pr_err("Invalid size\n");
        return -EINVAL;
    }

    di = general_calloc(1, sizeof(*di) + strlen(name) + 1);
    if (!di) {
        pr_err("No more memory\n");
        return -ENOMEM;
    }

    pr_dbg("Create logic device(%s) offset(0x%x) size(0x%x)\n", name, offset, size);
    di->name = (char *)(di + 1);
    strcpy(di->name, name);
    di->sector_size = NOR_SECTOR_SIZE;
    di->sector_offset = offset >> NOR_LOG_SIZE;
    di->sector_cnt = size >> NOR_LOG_SIZE;
    di->ops = &disk_nor_operation;
    return disk_access_register(di);
}
#endif /* CONFIG_FILE_SYSTEM_LITTLEFS */

/*
 * Mount fatfs to filesystem parition
 */
int __rte_notrace platform_extend_filesystem_init(int fs_type) {
    const struct partition_entry *parti;
    struct fs_mount_t *mp;
    const struct disk_partition *fpt;
    struct disk_partition dpobj;
		size_t limit;
    int err = -EINVAL;
    
    (void) fs_type;
    for (int i = 0, lfs = 0, fat = 0; i < FS_NUMBER; i++) {
        fpt = disk_partition_find(fs_table[i].name);
        if (!fpt) { 
            if (strcmp(fs_table[i].name, "udisk")) {
                pr_err("Not found filesystem partition(%s)\n", fs_table[i].name);
                continue;
            }
            parti = parition_get_entry2(STORAGE_ID_NOR, 
                PARTITION_FILE_ID_UDISK);
            dpobj.parent = CONFIG_SPI_FLASH_NAME;
            dpobj.offset = parti->offset;
            dpobj.len = parti->size;
            fpt = &dpobj;
        }

        mp = &mount_entry[i];
        mp->mnt_point = fs_table[i].mp;
		limit = fs_table[i].limit_size;

#ifdef CONFIG_FILE_SYSTEM_LITTLEFS
        if (!strcmp(fs_table[i].type, "littlefs") && lfs < LITTLEFS_NUMBER) {
            fs_flash_map[lfs].fa_id = (uint8_t)i;
            fs_flash_map[lfs].fa_device_id = (uint8_t)i;
            fs_flash_map[lfs].fa_off = fpt->offset;
            fs_flash_map[lfs].fa_size = rte_min(fpt->len, limit);
            fs_flash_map[lfs].fa_dev_name =fpt->parent;

            mp->type = FS_LITTLEFS;
            mp->fs_data = littlefs_dat[lfs++];
            mp->storage_dev = (void *)i;
            err |= fs_mount(mp);
            continue;
        }
#endif /* !CONFIG_FILE_SYSTEM_LITTLEFS */
        (void)fat;
#ifdef CONFIG_FAT_FILESYSTEM_ELM
        if (!strcmp(fs_table[i].type, "fatfs") && fat < FATFS_NUMBER) {
            err = platform_nor_logdisk_create(fs_table[i].dev, 
                fpt->offset, fpt->len);
            if (err)
                return err;
            if (disk_access_init(fs_table[i].dev) != 0) 
                return -ENODEV;

            mp->type = FS_FATFS;
            mp->fs_data = &fat_data[fat++];
            err = fs_mount(mp);
            if (err)
                pr_err("Mount fatfs failed(%d) partition(%x, %x)\n", err, fpt->offset, fpt->len);
            continue;
        }
#endif /* CONFIG_FILE_SYSTEM_LITTLEFS */

    }
    return err;
}
