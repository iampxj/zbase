/*
 * Copyright 2022 wtcat
 */
#include "basework/os/osapi_fs.h"
#include "gtest/gtest.h"

#define FILE_NAME(path) "/XIPFS:" path
#define TEXT_LEN(name) (sizeof(name) - 1)
static const char test_text_1[] = {
    "If there any previous publications allow the subscriber to read 1111"
};
static const char test_text_2[] = {
    "//If there any previous publications allow the subscriber to read 2222"
};
static const char test_text_3[] = {
    "##If there any previous publications allow the subscriber to read 3333"
};
static const char test_text_4[] = {
    "##If there any previous publications allow the subscriber to read 4444 abcdefghij 123456789 !@#$ks"
};

extern "C" void gtest_breakpoint(void) {
    puts("\n");
}

extern "C" void gtest_breakpoint_1(void) {
    puts("\n");
}

extern "C" void gtest_breakpoint_2(void) {
    puts("\n");
}

TEST(xipfs, readwrite) {
    os_file_t fd;
    char buffer[128];

    memset(buffer, 0, sizeof(buffer));
    ASSERT_EQ(vfs_open(&fd, FILE_NAME("/test-1.txt"), VFS_O_CREAT | VFS_O_RDWR), 0);
    ASSERT_EQ(vfs_ioctl(fd, VFS_IOSET_FILESIZE, (void *)TEXT_LEN(test_text_1)), 0);
    ASSERT_EQ(vfs_write(fd, test_text_1, TEXT_LEN(test_text_1)), (ssize_t)TEXT_LEN(test_text_1));
    ASSERT_EQ(vfs_lseek(fd, 0, VFS_SEEK_SET), 0);
    ASSERT_EQ(vfs_read(fd, buffer, sizeof(buffer)), TEXT_LEN(test_text_1));
    ASSERT_STREQ(buffer, test_text_1);
    ASSERT_EQ(vfs_close(fd), 0);

    memset(buffer, 0, sizeof(buffer));
    ASSERT_EQ(vfs_open(&fd, FILE_NAME("/test-1.txt"), VFS_O_CREAT | VFS_O_RDWR), 0);
    ASSERT_EQ(vfs_read(fd, buffer, sizeof(buffer)), TEXT_LEN(test_text_1));
    ASSERT_STREQ(buffer, test_text_1);
    ASSERT_EQ(vfs_close(fd), 0);

    memset(buffer, 0, sizeof(buffer));
    ASSERT_EQ(vfs_open(&fd, FILE_NAME("/test-2.txt"), VFS_O_CREAT | VFS_O_RDWR), 0);
    ASSERT_EQ(vfs_ioctl(fd, VFS_IOSET_FILESIZE, (void *)TEXT_LEN(test_text_2)), 0);
    ASSERT_EQ(vfs_write(fd, test_text_2, TEXT_LEN(test_text_2)), (ssize_t)TEXT_LEN(test_text_2));
    ASSERT_EQ(vfs_lseek(fd, 0, VFS_SEEK_SET), 0);
    ASSERT_EQ(vfs_read(fd, buffer, sizeof(buffer)), TEXT_LEN(test_text_2));
    ASSERT_STREQ(buffer, test_text_2);
    ASSERT_EQ(vfs_close(fd), 0);

    memset(buffer, 0, sizeof(buffer));
    ASSERT_EQ(vfs_open(&fd, FILE_NAME("/test-3.txt"), VFS_O_CREAT | VFS_O_RDWR), 0);
    ASSERT_EQ(vfs_ioctl(fd, VFS_IOSET_FILESIZE, (void *)TEXT_LEN(test_text_3)), 0);
    ASSERT_EQ(vfs_write(fd, test_text_3, TEXT_LEN(test_text_3)), (ssize_t)TEXT_LEN(test_text_3));
    ASSERT_EQ(vfs_lseek(fd, 0, VFS_SEEK_SET), 0);
    ASSERT_EQ(vfs_read(fd, buffer, sizeof(buffer)), TEXT_LEN(test_text_3));
    ASSERT_STREQ(buffer, test_text_3);
    ASSERT_EQ(vfs_close(fd), 0);

    ASSERT_EQ(vfs_unlink(FILE_NAME("/test-2.txt")), 0);
    ASSERT_NE(vfs_open(&fd, FILE_NAME("/test-2.txt"), VFS_O_RDWR), 0);

    ASSERT_EQ(vfs_open(&fd, FILE_NAME("/test-4.txt"), VFS_O_CREAT | VFS_O_RDWR), 0);
    ASSERT_EQ(vfs_ioctl(fd, VFS_IOSET_FILESIZE, (void *)TEXT_LEN(test_text_4)), 0);

    gtest_breakpoint();
    memset(buffer, 0, sizeof(buffer));
    ASSERT_EQ(vfs_write(fd, test_text_4, TEXT_LEN(test_text_4)), (ssize_t)TEXT_LEN(test_text_4));
    ASSERT_EQ(vfs_lseek(fd, 0, VFS_SEEK_SET), 0);
    ASSERT_EQ(vfs_read(fd, buffer, sizeof(buffer)), TEXT_LEN(test_text_4));
    ASSERT_STREQ(buffer, test_text_4);
    ASSERT_EQ(vfs_close(fd), 0);

    memset(buffer, 0, sizeof(buffer));
    gtest_breakpoint_1();
    ASSERT_EQ(vfs_open(&fd, FILE_NAME("/test-1.txt"),  VFS_O_RDWR), 0);
    ASSERT_EQ(vfs_read(fd, buffer, sizeof(buffer)), TEXT_LEN(test_text_1));
    ASSERT_STREQ(buffer, test_text_1);
    ASSERT_EQ(vfs_close(fd), 0);

    memset(buffer, 0, sizeof(buffer));
    gtest_breakpoint_1();
    ASSERT_EQ(vfs_open(&fd, FILE_NAME("/test-3.txt"),  VFS_O_RDWR), 0);
    ASSERT_EQ(vfs_read(fd, buffer, sizeof(buffer)), TEXT_LEN(test_text_3));
    ASSERT_STREQ(buffer, test_text_3);
    ASSERT_EQ(vfs_close(fd), 0);
}