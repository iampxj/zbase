/*
 * Copyright 2024 wtcat
 */

#include <zephyr.h>
#include <partition/partition.h>

#include "basework/thirdparty/FlashDB/nvram_kv.h"
#include "basework/thirdparty/FlashDB/flashdb.h"

#include <board_cfg.h>

#define _nvram_db ((struct fdb_kvdb *)(&_nvram_db_default))

static K_MUTEX_DEFINE(nvram_mtx);
static bool kv_okay;

static void nvram_lock(fdb_db_t db) {
    k_mutex_lock((struct k_mutex *)db->user_data, K_FOREVER);
}

static void nvram_unlock(fdb_db_t db) {
    k_mutex_unlock((struct k_mutex *)db->user_data);
}

int _nvram_kv_init(const char *name, int file_id, const char *path) {
    if (!kv_okay) {
        int err = 0;
        if (file_id > 0) {
            static struct disk_partition dpart;
            const struct partition_entry *parti = parition_get_entry(file_id);
            if (parti == NULL)
                return -ENODEV;

            dpart.name = "nvram";
            dpart.offset = parti->offset;
            dpart.len = parti->len;

            switch (parti->storage_id) {
            case STORAGE_ID_NOR:
                err = disk_device_open("spi_flash", 
                    &nvram_db.parent.storage.f_part.dev);
                break;
            case STORAGE_ID_NAND:
                err = disk_device_open("spinand", 
                    &nvram_db.parent.storage.f_part.dev);
                break;
            case STORAGE_ID_DATA_NOR:
                err = disk_device_open("spi_flash_2", 
                    &nvram_db.parent.storage.f_part.dev);
                break;
            default:
                rte_assert0(0);
            }
            _nvram_db->parent.storage.f_part.dp = &dpart;
        }
        if (!err) {
            fdb_kvdb_control(_nvram_db, FDB_KVDB_CTRL_SET_LOCK, (void *)nvram_lock);
            fdb_kvdb_control(_nvram_db, FDB_KVDB_CTRL_SET_UNLOCK, (void *)nvram_unlock);
            err = fdb_kvdb_init(_nvram_db, name, path, NULL, &nvram_mtx);
            if (!err) {
                kv_okay = true;
                return 0;
            }
        }
        return -(int)err;
    }
    return 0;
}
