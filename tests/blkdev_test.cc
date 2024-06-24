/*
 * Copyright 2022 wtcat
 */
#include <string.h>

#include "basework/dev/blkdev.h"
#include "basework/dev/device.h"
#include "basework/dev/disk.h"
#include "basework/lib/disklog.h"
#include "basework/dev/partition.h"
#include "basework/log.h"

#include "gtest/gtest.h"

#if defined(__ZEPHYR__)
#define DISK_DEV "spi_flash"
#else
#define DISK_DEV "virtual-flash"
#endif

static const char copyright[] = {
    "// Copyright 2006, Google Inc.\n"
    "// All rights reserved.\n"
    "//\n"
    "// Redistribution and use in source and binary forms, with or without\n"
    "// modification, are permitted provided that the following conditions are\n"
    "// met:\n"
    "//\n"
    "//     * Redistributions of source code must retain the above copyright\n"
    "// notice, this list of conditions and the following disclaimer.\n"
    "//     * Redistributions in binary form must reproduce the above\n"
    "// copyright notice, this list of conditions and the following disclaimer\n"
    "// in the documentation and/or other materials provided with the\n"
    "// distribution.\n"
    "//     * Neither the name of Google Inc. nor the names of its\n"
    "// contributors may be used to endorse or promote products derived from\n"
    "// this software without specific prior written permission.\n"
    "//\n"
    "// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n"
    "// \"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n"
    "// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n"
    "// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\n"
    "// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n"
    "// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n"
    "// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n"
    "// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n"
    "// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n"
    "// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n"
    "// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"
    " @defgroup rtems_bdbuf Block Device Buffer Management\n"
    "\n"
    " @ingroup rtems_libblock\n"
    "\n"
    " The Block Device Buffer Management implements a cache between the disk\n"
    " devices and file systems.  The code provides read-ahead and write queuing to\n"
    " the drivers and fast cache look-up using an AVL tree.\n"
    "\n"
    " The block size used by a file system can be set at runtime and must be a\n"
    " multiple of the disk device block size.  The disk device's physical block\n"
    " size is called the media block size.  The file system can set the block size\n"
    " it uses to a larger multiple of the media block size.  The driver must be\n"
    " able to handle buffers sizes larger than one media block.\n"
    "\n"
    " The user configures the amount of memory to be used as buffers in the cache,\n"
    " and the minimum and maximum buffer size.  The cache will allocate additional\n"
    " memory for the buffer descriptors and groups.  There are enough buffer\n"
    " descriptors allocated so all the buffer memory can be used as minimum sized\n"
    " buffers.\n"
    "\n"
    " The cache is a single pool of buffers.  The buffer memory is divided into\n"
    " groups where the size of buffer memory allocated to a group is the maximum\n"
    " buffer size.  A group's memory can be divided down into small buffer sizes\n"
    " that are a multiple of 2 of the minimum buffer size.  A group is the minimum\n"
    " allocation unit for buffers of a specific size.  If a buffer of maximum size\n"
    " is request the group will have a single buffer.  If a buffer of minimum size\n"
    " is requested the group is divided into minimum sized buffers and the\n"
    " remaining buffers are held ready for use.  A group keeps track of which\n"
    " buffers are with a file system or driver and groups who have buffer in use\n"
    " cannot be realloced.  Groups with no buffers in use can be taken and\n"
    " realloced to a new size.  This is how buffers of different sizes move around\n"
    " the cache.\n"
    " The buffers are held in various lists in the cache.  All buffers follow this\n"
    " state machine:\n"
    "\n"
    " @dot\n"
    " digraph state {\n"
    " }\n"
    " @enddot\n"
    "\n"
    " Empty or cached buffers are added to the LRU list and removed from this\n"
    " queue when a caller requests a buffer.  This is referred to as getting a\n"
    " buffer in the code and the event get in the state diagram.  The buffer is\n"
    " assigned to a block and inserted to the AVL based on the block/device key.\n"
    " If the block is to be read by the user and not in the cache it is transfered\n"
    " from the disk into memory.  If no buffers are on the LRU list the modified\n"
    " list is checked.  If buffers are on the modified the swap out task will be\n"
    " woken.  The request blocks until a buffer is available for recycle.\n"
    "\n"
    " A block being accessed is given to the file system layer and not accessible\n"
    " to another requester until released back to the cache.  The same goes to a\n"
    " buffer in the transfer state.  The transfer state means being read or\n"
    " written.  If the file system has modified the block and releases it as\n"
    " modified it placed on the cache's modified list and a hold timer\n"
    " initialised.  The buffer is held for the hold time before being written to\n"
    " disk.  Buffers are held for a configurable period of time on the modified\n"
    " list as a write sets the state to transfer and this locks the buffer out\n"
    " from the file system until the write completes.  Buffers are often accessed\n"
    " and modified in a series of small updates so if sent to the disk when\n"
    " released as modified the user would have to block waiting until it had been\n"
    " written.  This would be a performance problem.\n"
    "\n"
    " The code performs multiple block reads and writes.  Multiple block reads or\n"
    " read-ahead increases performance with hardware that supports it.  It also\n"
    " helps with a large cache as the disk head movement is reduced.  It however\n"
    " is a speculative operation so excessive use can remove valuable and needed\n"
    " blocks from the cache.  The read-ahead is triggered after two misses of\n"
    " ascending consecutive blocks or a read hit of a block read by the\n"
    " most-resent read-ahead transfer.  The read-ahead works per disk, but all\n"
    " transfers are issued by the read-ahead task.\n"
    "\n"
    " The cache has the following lists of buffers:\n"
    "  - LRU: Accessed or transfered buffers released in least recently used\n"
    "  order.  Empty buffers will be placed to the front.\n"
    "  - Modified: Buffers waiting to be written to disk.\n"
    "  - Sync: Buffers to be synchronized with the disk.\n"
    "\n"
    " A cache look-up will be performed to find a suitable buffer.  A suitable\n"
    " buffer is one that matches the same allocation size as the device the buffer\n"
    " is for.  The a buffer's group has no buffers in use with the file system or\n"
    " driver the group is reallocated.  This means the buffers in the group are\n"
    " invalidated, resized and placed on the LRU queue.  There is a performance\n"
    " issue with this design.  The reallocation of a group may forced recently\n"
    " accessed buffers out of the cache when they should not.  The design should be\n"
    " change to have groups on a LRU list if they have no buffers in use.\n"
};

static char str_buffer[sizeof(copyright)+1];

class cc_blkdev_test : public testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
    static void SetUpTestCase() {}
    static void TearDownTestCase() {
        blkdev_destroy();
    }
    struct disk_device *get_disk() const {
        struct disk_device *dd = nullptr;
        disk_device_open(DISK_DEV, &dd);
        return dd;
    }

private:
    static struct disk_device vflash_disk_;
};

struct disk_device cc_blkdev_test::vflash_disk_;

TEST_F(cc_blkdev_test, blkdev_rw) {
    struct disk_device *dd = get_disk();
    ASSERT_NE(dd, nullptr);
    ASSERT_GT(dd->blk_size, (size_t)0);
    ASSERT_GT(dd->len, (size_t)0);
    ASSERT_GE(blkdev_write(dd, copyright, sizeof(copyright), 0), 0);
    ASSERT_GE(blkdev_read(dd, str_buffer, sizeof(copyright), 0), 0);
    ASSERT_STREQ(copyright, str_buffer);
    ASSERT_EQ(blkdev_sync(), 0);
    // ASSERT_STREQ(copyright, vflash_memory);
    blkdev_print();
}


struct log_context {
    int ofs;
};
static bool log_output(void *ctx, char *buf, size_t size) {
    struct log_context *lctx = (struct log_context *)ctx;
    buf[size] = '\0';
    assert(strncmp(buf, copyright+lctx->ofs, size) == 0);
    lctx->ofs += size;
    return true;
}

TEST_F(cc_blkdev_test, disklog) {
    struct log_context ctx = {0};
    ASSERT_EQ(disklog_init(), 0);
    ASSERT_EQ(disklog_input(copyright, sizeof(copyright)), 0);
    ASSERT_EQ(disklog_ouput(log_output, &ctx, 0), 0);
    ctx.ofs = 0;
    ASSERT_EQ(disklog_ouput(log_output, &ctx, 0), 0);
    blkdev_print();

    disklog_reset();
    npr_info(disk, "%s", copyright);
    ctx.ofs = 0;
    ASSERT_EQ(disklog_ouput(log_output, &ctx, 0), 0);
    blkdev_print();

    int offset = 0;
    bool first = true;
    memset(str_buffer, 0, sizeof(str_buffer));
    
    do {
        offset = disklog_read(str_buffer + offset, sizeof(str_buffer) - offset, first);
        ASSERT_GE(offset, 0);
        first = false;
    } while (offset > 0);
    ASSERT_STREQ(copyright, str_buffer);

}
