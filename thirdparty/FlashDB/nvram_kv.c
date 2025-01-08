/*
 * Copyright (c) 2024 wtcat
 *
 * Key-value based flashdb
 */

#include <errno.h>

#include "basework/thirdparty/FlashDB/flashdb.h"

struct fdb_kvdb _nvram_db_default;

int _nvram_kv_set(fdb_kvdb_t db, const char *key, const char *buf, 
    size_t len) {
    struct fdb_blob blob;

    if (db == NULL || key == NULL)
        return -EINVAL;

    return -(int)fdb_kv_set_blob(db, key, 
        fdb_blob_make(&blob, buf, len));
}

ssize_t _nvram_kv_get(fdb_kvdb_t db, const char *key, char *buf, 
    size_t maxlen) {
    struct fdb_blob blob;

     if (db == NULL || key == NULL)
        return -EINVAL;

    return fdb_kv_get_blob(db, key, 
        fdb_blob_make(&blob, buf, maxlen));
}

int _nvram_kv_del(fdb_kvdb_t db, const char *key) {
     if (db == NULL || key == NULL)
        return -EINVAL;

    return -(int)fdb_kv_del(db, key);
}
