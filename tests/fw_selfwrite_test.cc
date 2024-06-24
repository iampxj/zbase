/*
 * Copyright 2022 wtcat
 */
#include <string.h>
#include "basework/dev/blkdev.h"
#include "basework/dev/partition.h"
#include "basework/log.h"
#include "basework/malloc.h"
#include "basework/boot/boot.h"

#include "gtest/gtest.h"


#define FW_XIP_BASE 0x10000000
#define FW_SIZE (1500 * 1024)

class cc_fw_test : public testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
    static void SetUpTestCase() {
        pt_ = disk_partition_find("firmware");
        ASSERT_NE(pt_, nullptr);
        fw_buf_ = general_malloc(4096);
        ASSERT_NE(fw_buf_, nullptr);
    }
    static void TearDownTestCase() {
        if (fw_buf_) {
            general_free(fw_buf_);
            fw_buf_ = nullptr;
        }
    }
    void *fw_buffer() {
        return fw_buf_;
    }
    const struct disk_partition *pt() const {
        return pt_;
    } 

private:
    static void *fw_buf_;
    static const struct disk_partition *pt_;
};

void *cc_fw_test::fw_buf_;
const struct disk_partition* cc_fw_test::pt_;


static uint32_t crc32_update(uint32_t crc, const uint8_t *data, 
    size_t len) {
	/* crc table generated from polynomial 0xedb88320 */
	static const uint32_t table[16] = {
		0x00000000U, 0x1db71064U, 0x3b6e20c8U, 0x26d930acU,
		0x76dc4190U, 0x6b6b51f4U, 0x4db26158U, 0x5005713cU,
		0xedb88320U, 0xf00f9344U, 0xd6d6a3e8U, 0xcb61b38cU,
		0x9b64c2b0U, 0x86d3d2d4U, 0xa00ae278U, 0xbdbdf21cU,
	};

	crc = ~crc;
	for (size_t i = 0; i < len; i++) {
		uint8_t byte = data[i];
		crc = (crc >> 4) ^ table[(crc ^ byte) & 0x0f];
		crc = (crc >> 4) ^ table[(crc ^ ((uint32_t)byte >> 4)) & 0x0f];
	}
	return ~crc;
}

TEST_F(cc_fw_test, save) {
    struct firmware_header fw;

    void *fwbuf = fw_buffer();
    ASSERT_NE(fwbuf, nullptr);

    ASSERT_EQ(lgpt_read(pt(), 0, &fw, sizeof(fw)), (int)sizeof(fw));
    if (fw.fh_magic == FH_MAGIC &&
        fw.fh_devid == 0x01 &&
        fw.fh_version == 0x01 &&
        fw.fh_isize == FW_SIZE)
        return;

    fw.fh_magic = FH_MAGIC;
    fw.fh_devid = 0x01;
    fw.fh_version = 0x01;
    fw.fh_dcrc = 0;
    fw.fh_comp = FH_COMP_NONE;
    fw.fh_isize = FW_SIZE;
    fw.fh_size = FW_SIZE;
    fw.fh_entry = FW_XIP_BASE;
    strncpy((char *)fw.fh_name, "zephyr", 6);
    fw.fh_name[6] = '\0';

    size_t fwsize = FW_SIZE;
    size_t ofs = sizeof(fw);
    char *src = (char *)FW_XIP_BASE;
    uint32_t crc = 0;

    while (fwsize > 0) {
        size_t bytes = fwsize < 1024? fwsize: 1024;
        ASSERT_EQ(lgpt_write(pt(), ofs, src, bytes), (int)bytes);
        crc = crc32_update(crc, (const uint8_t *)src, bytes);
        fwsize -= bytes;
        src += bytes;
        ofs += bytes;
    }

    fw.fh_dcrc = crc;
    ASSERT_EQ(lgpt_write(pt(), 0, &fw, sizeof(fw)), (int)sizeof(fw));
    ASSERT_EQ(blkdev_sync(), 0);
}
