/*
 * Copyright 2023 wtcat
 */
#if defined(CONFIG_BOARD_ATS3085C_CW01)
# define BACKUP_USERDATA_ENABLE
#endif
#define BACKUP_GPTDATA_ENABLE

#include <errno.h>
#include <zephyr.h>
#include <device.h>
#include <drivers/flash.h>
#include <init.h>

#include <ui_mem.h>
#include <partition/partition.h>
#include "basework/rq.h"
#include "basework/malloc.h"
#include "basework/system.h"
#include "basework/dev/partition.h"
#include "board_cfg.h"
struct partition_table {
	u32_t magic;
	u16_t version;
	u16_t table_size;
	u16_t part_cnt;
	u16_t part_entry_size;
	u8_t reserved1[4];

	struct partition_entry parts[MAX_PARTITION_COUNT];
	u8_t Reserved2[4];
	u32_t table_crc;
};

struct partition_local {
    const char *name;
    uint32_t offset;
};

#define WATCHDOG_FEED() watchdog_clear()
#define MEM_ALLOCATOR_ALLOC(size) general_malloc(size)
#define MEM_ALLOCATOR_FREE(ptr)   general_free(ptr)

#define PT_BUFFER_SIZE   (32 * 1024)
#define GPT_ADDRESS      0x1000
#define GPT_SIZE         0x1000
#define FLASH_PAGE_SIZE  0x1000
#define FLASH_PAGE_MASK  (FLASH_PAGE_SIZE - 1)

#define BACKUP_PARTITION_OFFSET (MB(16) - BACKUP_PARTITION_SIZE)
#define BACKUP_PARTITION_SIZE   MB(2)

extern void watchdog_clear(void);
extern char __partition_image_begin[];
extern char __partition_image_end[];

static const uint8_t file_array[] = {
    5,  /* fw0_sdfs */
    6,  /* nvram_factory */
    7,  /* nvram_factory_rw*/
    8,  /* nvram_user */
    20, /* sdfs_k */
};

#ifdef BACKUP_USERDATA_ENABLE
/*
    ==================== disk partition table ====================
    | name          | device    |   offset   |    length  |
    -------------------------------------------------------------
    | firmware_cur  | spi_flash | 0x00fff000 | 0x00001000 |
    | firmware      | spi_flash | 0x00dff000 | 0x00200000 |
    | filesystem    | spi_flash | 0x00bff000 | 0x00200000 |
    | syslog        | spi_flash | 0x00be6000 | 0x00019000 |
    | usrdata       | spi_flash | 0x00bd2000 | 0x00014000 |
    | msgfile.db    | spi_flash | 0x00bc5000 | 0x0000d000 |
    | watchface     | spi_flash | 0x00bc4000 | 0x00001000 |
    | dial_local    | spi_flash | 0x00b8d000 | 0x00037000 |
    | usrdata2      | spi_flash | 0x00b8b000 | 0x00002000 |
    | call_recent   | spi_flash | 0x00b89000 | 0x00002000 |
    | phone_book    | spi_flash | 0x00b5f000 | 0x0002a000 |
    | offline_agnss | spi_flash | 0x00b40000 | 0x0001f000 |
    =============================================================
*/
static const struct partition_local old_partition[] = {
    {"usrdata",       0x00bd2000},
    {"watchface",     0x00bc4000},
    {"dial_local",    0x00b8d000},
    {"usrdata2",      0x00b8b000},
    {"offline_agnss", 0x00b40000}
};

static int 
pt_param_copy_userdata(const struct device *storage, 
    const struct partition_local *oldpt, size_t nitem) {
    size_t backup_ofs = 0;
    void *buffer;
    int err;

    if (oldpt == NULL || nitem == 0)
        return -EINVAL;

    printk("Move user-data to new partition from old\n");

    /* Allocate read buffer */
    buffer = MEM_ALLOCATOR_ALLOC(PT_BUFFER_SIZE);
    assert(buffer != NULL);
    
    /* Erase backup partition */
    flash_erase(storage, BACKUP_PARTITION_OFFSET, 
        BACKUP_PARTITION_SIZE);
    WATCHDOG_FEED();

    /* Backup user data to special partition */
    for (size_t i = 0; i < nitem; i++) {
        const struct disk_partition *dp = disk_partition_find(oldpt[i].name);
        assert(dp != NULL);
        size_t len = dp->len;
        size_t ofs = oldpt[i].offset;

        printk("Backup parition %s offset(0x%08x) size(0x%x)\n", 
            dp->name, (uint32_t)dp->offset, dp->len);

        while (len > 0) {
            size_t copylen = MIN(len, PT_BUFFER_SIZE);
            err = flash_read(storage, ofs, buffer, copylen);
            if (err) 
                goto _exit;

            WATCHDOG_FEED();
            err = flash_write(storage, BACKUP_PARTITION_OFFSET + backup_ofs, 
                buffer, copylen);
            if (err)
                goto _exit;

            ofs += copylen;
            len -= copylen;
            backup_ofs += copylen;
        }
    }

    /* Copy data to new partition */
    backup_ofs = 0;
    for (size_t i = 0; i < nitem; i++) {
        const struct disk_partition *dp = disk_partition_find(oldpt[i].name);
        assert(dp != NULL);
        size_t len = dp->len;
        size_t ofs = dp->offset;

        printk("Restore parition %s offset(0x%08x) size(0x%x)\n", 
            dp->name, (uint32_t)dp->offset, dp->len);

        disk_partition_erase_all(dp);
        WATCHDOG_FEED();

        while (len > 0) {
            size_t copylen = MIN(len, PT_BUFFER_SIZE);
            err = flash_read(storage, BACKUP_PARTITION_OFFSET + backup_ofs, 
                buffer, copylen);
            if (err) 
                goto _exit;

            WATCHDOG_FEED();
            err = flash_write(storage, ofs, buffer, copylen);
            if (err)
                goto _exit;

            ofs += copylen;
            len -= copylen;
            backup_ofs += copylen;
        }
    }
    err = 0;

_exit:
    MEM_ALLOCATOR_FREE(buffer);
    return err;
}
#endif /* BACKUP_USERDATA_ENABLE */

static const struct partition_entry * 
get_file_entry(u8_t stor_id, u8_t file_id) {
    const struct partition_table *ptbl = (void *)__partition_image_begin;
	for (int i = 0; i < MAX_PARTITION_COUNT; i++) {
		const struct partition_entry *part = &ptbl->parts[i];
		if (part->storage_id == stor_id && part->file_id == file_id)
			return part;
	}
    assert(0);
	return NULL;
}

static int 
pt_param_compare(const struct device *flash, const char *buf, size_t len) {
    char *buffer = MEM_ALLOCATOR_ALLOC(len);
    int err;

    if (buffer) {
        err = flash_read(flash, GPT_ADDRESS, buffer, len);
        if (err) {
            printk("flash read address(0x%x) error\n", GPT_ADDRESS);
            MEM_ALLOCATOR_FREE(buffer);
            goto _out;
        }
        err = memcmp(buffer, buf, len);
        MEM_ALLOCATOR_FREE(buffer);
    } else {
        err = -ENOMEM;
        printk("no more memory\n");
    }
_out:
    return err;
}

static int 
pt_param_write(const struct device *flash, const char *buf, size_t size) {
    size_t ofs = 0, count = size;
    int err;

    err = flash_erase(flash, GPT_ADDRESS, GPT_SIZE);
    if (err) {
        printk("flash erase address(0x%x) error\n", GPT_ADDRESS);
        return err;
    }

    /* I don't known why we can't write more than 10 bytes at once */
    while (count > 0) {
        size_t bytes = rte_min_t(size_t, 10, count);
        err = flash_write(flash, GPT_ADDRESS + ofs, 
            buf + ofs, bytes);
        if (err) {
            printk("flash write address(0x%x) error\n", GPT_ADDRESS);
            break;
        }
        ofs += bytes;
        count -= bytes;
    }
    return err;
}

static int 
pt_param_update(const struct device *flash, const char *buf, size_t size) {
    int err, retry = 3;

    do {
        err = pt_param_write(flash, buf, size);
        if (!err) {
            if (!pt_param_compare(flash, buf, size))
                return 0;
        }
        k_msleep(100);
    } while (--retry > 0);
    
    return err;
}

static int 
pt_param_copy_from_oldpt(const struct device *dev, uint8_t storage) {
    const struct partition_entry *new_pt, *old_pt;
    char *buffer = NULL;
    size_t len, ofs;
    size_t copylen;
    size_t backup_ofs = 0;
    size_t i;
    size_t sum = 0;
    int err;

    /* 
     * Backup parition data 
     */
    printk("Backup parition data...\n");
    buffer = MEM_ALLOCATOR_ALLOC(PT_BUFFER_SIZE);
    assert(buffer != NULL);

    /* 
     * Calculator and check size for backup region 
     */
    for (i = 0; i < ARRAY_SIZE(file_array); i++) {
        old_pt = partition_get_stf_part(storage, file_array[i]);
        assert(old_pt != NULL);
        assert((old_pt->file_offset & FLASH_PAGE_MASK) == 0); 
        sum += old_pt->size;
    }

    if (sum > BACKUP_PARTITION_SIZE) {
        printk("Backup region is too small (sum = 0x%08x, dp->len = 0x%08x)\n", 
            BACKUP_PARTITION_OFFSET, BACKUP_PARTITION_SIZE);
        err = -EBADF;
        goto _exit;
    }

    /* Erase backup partition */
    printk("Erasing backup partition...\n");
    WATCHDOG_FEED();
    flash_erase(dev, BACKUP_PARTITION_OFFSET, 
        BACKUP_PARTITION_SIZE);
    WATCHDOG_FEED();

    for (i = 0; i < ARRAY_SIZE(file_array); i++) {
        old_pt = partition_get_stf_part(storage, file_array[i]);
        len = old_pt->size;
        ofs = old_pt->file_offset;

        printk("Backup %s offset(0x%08x) size(0x%x)\n", 
            old_pt->name, old_pt->offset, old_pt->size);

        while (len > 0) {
            copylen = MIN(len, PT_BUFFER_SIZE);
            err = flash_read(dev, ofs, buffer, copylen);
            if (err) 
                goto _exit;

            WATCHDOG_FEED();
            err = flash_write(dev, BACKUP_PARTITION_OFFSET + backup_ofs, 
                buffer, copylen);
            if (err)
                goto _exit;

            ofs += copylen;
            len -= copylen;
            backup_ofs += copylen;
        }
    }

    /* 
     * New partion table
     */
    printk("Restore parition data...\n");
    backup_ofs = 0;

    for (i = 0; i < ARRAY_SIZE(file_array); i++) {
        old_pt = partition_get_stf_part(storage, file_array[i]);
        new_pt = get_file_entry(storage, file_array[i]);
        assert(new_pt != NULL);

        len = old_pt->size;
        ofs = new_pt->file_offset;

        err = flash_erase(dev, new_pt->file_offset, new_pt->size);
        if (err)
            goto _exit;

        printk("Restore %s offset(0x%08x) size(0x%x)\n", 
            new_pt->name, new_pt->offset, new_pt->size);

        while (len > 0) {
            copylen = MIN(len, PT_BUFFER_SIZE);
            err = flash_read(dev, BACKUP_PARTITION_OFFSET + backup_ofs, buffer, copylen);
            if (err) 
                goto _exit;

            err = flash_write(dev, ofs, buffer, copylen);
            if (err)
                goto _exit;

            ofs += copylen;
            len -= copylen;
            backup_ofs += copylen;
        }
    }

_exit:
    MEM_ALLOCATOR_FREE(buffer);
    return err;
}

int 
gpt_param_check(const char *devname) {
    const struct device *flash = device_get_binding(devname);
    if (flash) {
        size_t len = __partition_image_end - __partition_image_begin;
        char *param;
        int err;

        if (!len || !pt_param_compare(flash, __partition_image_begin, len)) {
            printk("*** GPT does not need to be updated! ***\n");
            return 0;
        }

        printk("GPT data updating...\n");
#ifdef BACKUP_GPTDATA_ENABLE
        err = pt_param_copy_from_oldpt(flash, 0);
        if (err)
            printk("Copy parition data failed(%d)\n", err);
#endif /* BACKUP_GPTDATA_ENABLE */

#ifdef BACKUP_USERDATA_ENABLE
        err = pt_param_copy_userdata(flash, old_partition, 
            ARRAY_SIZE(old_partition));
        if (err)
            printk("Backup userdata failed(%d)\n", err);
#endif

#if defined(CONFIG_NOR_CODE_IN_RAM)|| \
	defined(CONFIG_SPI_XIP_READ)
        param = MEM_ALLOCATOR_ALLOC(len);
        if (!param) {
            printk("*** GPT no more memory ***\n");
            return -ENOMEM;
        }
        memcpy(param, __partition_image_begin, len);
#else
        param = __partition_image_begin;
#endif
        err = pt_param_update(flash, param, len);
        if (!err) {
            printk("*** GPT update successful ***\n");
#if defined(CONFIG_NOR_CODE_IN_RAM)|| \
	defined(CONFIG_SPI_XIP_READ)
            MEM_ALLOCATOR_FREE(param);
#endif
            k_msleep(2000);
            sys_shutdown(0, true);
            return 0;
        }
#if defined(CONFIG_NOR_CODE_IN_RAM)|| \
	defined(CONFIG_SPI_XIP_READ)
        MEM_ALLOCATOR_FREE(param);
#endif
        printk("*** GPT udpate failed ***\n");
        return err;
    }

    printk("Not found flash device (spi_flash)\n");
    return -ENODEV;
}
