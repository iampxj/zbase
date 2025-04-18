/*
 * Copyright (c) 2020, Armink, <armink.ztl@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief configuration file
 */

#ifndef _FDB_CFG_H_
#define _FDB_CFG_H_

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include "basework/log.h"
#include "basework/assert.h"

/* using KVDB feature */
#define FDB_USING_KVDB

#ifdef FDB_USING_KVDB
/* Auto update KV to latest default when current KVDB version number is changed. @see fdb_kvdb.ver_num */
/* #define FDB_KV_AUTO_UPDATE */
#endif

#ifdef CONFIG_FDB_KV_NAME_MAX
#define FDB_KV_NAME_MAX CONFIG_FDB_KV_NAME_MAX
#endif

#ifdef CONFIG_FDB_KV_CACHE_TABLE_SIZE
#define FDB_KV_CACHE_TABLE_SIZE CONFIG_FDB_KV_CACHE_TABLE_SIZE
#endif

#ifdef CONFIG_FDB_SECTOR_CACHE_TABLE_SIZE
#define FDB_SECTOR_CACHE_TABLE_SIZE CONFIG_FDB_SECTOR_CACHE_TABLE_SIZE
#endif

#ifdef CONFIG_FDB_FILE_CACHE_TABLE_SIZE
#define FDB_FILE_CACHE_TABLE_SIZE CONFIG_FDB_FILE_CACHE_TABLE_SIZE
#endif

/* using TSDB (Time series database) feature */
#ifdef CONFIG_FDB_USING_TSDB
#define FDB_USING_TSDB
#endif

/* Using FAL storage mode */
#if !defined(CONFIG_FDB_USING_FILE_LIBC_MODE) && \
    !defined(CONFIG_FDB_USING_FILE_POSIX_MODE)
#define FDB_USING_FAL_MODE 1
#endif


#ifdef FDB_USING_FAL_MODE
/* the flash write granularity, unit: bit
 * only support 1(nor flash)/ 8(stm32f2/f4)/ 32(stm32f1)/ 64(stm32f7)/ 128(stm32h5) */
#define FDB_WRITE_GRAN     8
#endif

/* Using file storage mode by LIBC file API, like fopen/fread/fwrte/fclose */
#ifdef CONFIG_FDB_USING_FILE_LIBC_MODE
#define FDB_USING_FILE_LIBC_MODE
#endif

/* Using file storage mode by POSIX file API, like open/read/write/close */
#ifdef CONFIG_FDB_USING_FILE_POSIX_MODE
#define FDB_USING_FILE_POSIX_MODE
#endif

/* MCU Endian Configuration, default is Little Endian Order. */
/* #define FDB_BIG_ENDIAN */ 

/* log print macro. default EF_PRINT macro is printf() */
#define FDB_PRINT(...)              pr_out(__VA_ARGS__)

/* print debug information */
// #define FDB_DEBUG_ENABLE

#define FDB_ASSERT(_expr) rte_assert(_expr);

#endif /* _FDB_CFG_H_ */
