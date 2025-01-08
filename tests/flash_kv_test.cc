/*
 * Copyright 2024 wtcat
 */
#include <string.h>
#include "basework/dev/partition.h"
#include "basework/log.h"
#include "basework/thirdparty/FlashDB/flashdb.h"

#include "gtest/gtest.h"

namespace {

} //namespace

TEST(flashdb, fal_mode) {
    struct fdb_kvdb fdb = {};
    struct fdb_blob blob;
    uint8_t buf[] = {1, 2, 3, 4, 5};
    uint8_t rdbuf[10] = {};

    ASSERT_EQ(fdb_kvdb_init(&fdb, "test", "firmware", nullptr, nullptr), 0);
    ASSERT_EQ(fdb_kv_set(&fdb, "platform", "linux"), 0);
    ASSERT_EQ(fdb_kv_set(&fdb, "toolchain", "gnu"), 0);
    ASSERT_EQ(fdb_kv_set(&fdb, "model", "arm"), 0);
    ASSERT_STREQ(fdb_kv_get(&fdb, "platform"), "linux");
    ASSERT_STREQ(fdb_kv_get(&fdb, "toolchain"), "gnu");
    ASSERT_STREQ(fdb_kv_get(&fdb, "model"), "arm");

    ASSERT_EQ(fdb_kv_set_blob(&fdb, "blob", fdb_blob_make(&blob, buf, sizeof(buf))), 0);
    ASSERT_EQ(fdb_kv_get_blob(&fdb, "blob", fdb_blob_make(&blob, rdbuf, sizeof(rdbuf))), sizeof(buf));
    ASSERT_EQ(memcmp(buf, rdbuf, sizeof(buf)), 0);
}

TEST(flashdb, fal_mode__) {
    struct fdb_kvdb fdb = {};
    struct fdb_blob blob;
    uint8_t buf[] = {1, 2, 3, 4, 5};
    uint8_t rdbuf[10] = {};

    const struct disk_partition *part;
    struct disk_partition vpart;

    ASSERT_NE((part = disk_partition_find("filesystem")), nullptr);
    ASSERT_EQ(disk_device_open(part->parent, &fdb.parent.storage.f_part.dev), 0);
    vpart = *part;
    fdb.parent.storage.f_part.dp = &vpart;

    ASSERT_EQ(fdb_kvdb_init(&fdb, "test", "firmware", nullptr, nullptr), 0);
    ASSERT_EQ(fdb_kv_set(&fdb, "platform", "linux"), 0);
    ASSERT_EQ(fdb_kv_set(&fdb, "toolchain", "gnu"), 0);
    ASSERT_EQ(fdb_kv_set(&fdb, "model", "arm"), 0);
    ASSERT_STREQ(fdb_kv_get(&fdb, "platform"), "linux");
    ASSERT_STREQ(fdb_kv_get(&fdb, "toolchain"), "gnu");
    ASSERT_STREQ(fdb_kv_get(&fdb, "model"), "arm");

    ASSERT_EQ(fdb_kv_set_blob(&fdb, "blob", fdb_blob_make(&blob, buf, sizeof(buf))), 0);
    ASSERT_EQ(fdb_kv_get_blob(&fdb, "blob", fdb_blob_make(&blob, rdbuf, sizeof(rdbuf))), sizeof(buf));
    ASSERT_EQ(memcmp(buf, rdbuf, sizeof(buf)), 0);
}