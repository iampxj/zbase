/*
 * Copyright 2024 wtcat
 */
#ifndef FLASHDB_NVRAM_KV_H_
#define FLASHDB_NVRAM_KV_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C"{
#endif

struct fdb_kvdb;

/*
 * _nvram_kv_set - Set key-value pairs to nvram
 *
 * @db  Pointer to nvram context
 * @key The key that should be associate with user buffer
 * @buf Pointer to user buffer
 * @len Buffer length
 *
 * return 0 if success
 */
int _nvram_kv_set(struct fdb_kvdb *db, const char *key, const char *buf, 
    size_t len);

/*
 * _nvram_kv_get - Get the value that associated with the key
 *
 * @db  Pointer to nvram context
 * @key The key associated with buffer
 * @buf Pointer to user buffer
 * @maxlen Buffer length
 *
 * return actual length if success
 */
ssize_t _nvram_kv_get(struct fdb_kvdb *db, const char *key, char *buf, 
    size_t maxlen);

/*
 * _nvram_kv_del - Delete key-value pair
 *
 * @db  Pointer to nvram context
 * @key The key associated with value
 *
 * return 0 if success
 */
int _nvram_kv_del(struct fdb_kvdb *db, const char *key);

/*
 * _nvram_kv_sync - Flush cache data to storage media
 *
 * @db  Pointer to nvram context
 *
 * return 0 if success
 */
int _nvram_kv_sync(struct fdb_kvdb *db);

extern char _nvram_db_default;

static inline int 
nvram_kv_set(const char *key, const char *buf, size_t len) {
    return _nvram_kv_set((struct fdb_kvdb *)&_nvram_db_default, key, buf, len);
}

static inline ssize_t 
nvram_kv_get(const char *key, char *buf, size_t maxlen) {
    return _nvram_kv_get((struct fdb_kvdb *)&_nvram_db_default, key, buf, maxlen);
}

static inline int 
nvram_kv_del(const char *key) {
    return _nvram_kv_del((struct fdb_kvdb *)&_nvram_db_default, key);
}

static inline int 
nvram_kv_sync(void) {
    return _nvram_kv_sync((struct fdb_kvdb *)&_nvram_db_default);
}

#ifdef __cplusplus
}
#endif
#endif /* FLASHDB_NVRAM_KV_H_ */
