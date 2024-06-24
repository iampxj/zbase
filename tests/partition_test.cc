/*
 * Copyright 2022 wtcat
 */
#include "basework/dev/partition_file.h"
#include "basework/dev/partition.h"

#include "gtest/gtest.h"

extern "C" struct disk_partition *usr_partition_get(void);


#if defined(__ZEPHYR__)
#define DISK_DEV "spi_flash"
#else
#define DISK_DEV "virtual-flash"
#endif

class cc_partition_test : public testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
    static void SetUpTestCase() {
        ASSERT_EQ(logic_partitions_create("usrdata", usr_partition_get()), 0);
    }
    static void TearDownTestCase() {

    }
};


TEST_F(cc_partition_test, sfile) {
    struct sfile *fp = nullptr;
    struct sfile *fp_ii = nullptr;
    const char ibind[] = {"User bind"};
    size_t len = sizeof(ibind) - 1;
    char buffer[128];

    ASSERT_EQ(usr_sfile_open("usrdata/bindinfo", &fp), 0);
    ASSERT_NE(fp, nullptr);
    ASSERT_EQ(usr_sfile_size(fp), (size_t)0);
    ASSERT_EQ(usr_sfile_write(fp, ibind, len), (ssize_t)len);

    ASSERT_EQ(usr_sfile_open("usrdata/bindinfo", &fp_ii), 0);
    ASSERT_NE(fp_ii, nullptr);
    ASSERT_EQ(usr_sfile_size(fp_ii), len);
    ASSERT_EQ(usr_sfile_setoffset(fp_ii, 0), 0);
    ASSERT_EQ(usr_sfile_read(fp_ii, buffer, len), (ssize_t)len);
    buffer[len] = '\0';
    ASSERT_STREQ(buffer, ibind);
    ASSERT_EQ(usr_sfile_close(fp_ii), 0);
    ASSERT_EQ(usr_sfile_close(fp), 0);

    ASSERT_EQ(usr_sfile_open("usrdata/bindinfo", &fp), 0);
    ASSERT_EQ(usr_sfile_close(fp), 0);
}