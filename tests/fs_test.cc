/*
 * Copyright 2022 wtcat
 */
#include <fs/fs.h>

#include "basework/os/osapi_fs.h"
#include "gtest/gtest.h"

#define FILE_NAME(path) "/UD0:" path

TEST(fs, readwrite) {
    struct fs_file_t zfp;
    os_file_t fd;
    int ret;

vfs_mkdir(FILE_NAME("/user"));
    fs_file_t_init(&zfp);
    ret = fs_open(&zfp, FILE_NAME("/test.txt"), FS_O_RDWR | FS_O_CREATE);
    if (ret == 0) {
        fs_close(&zfp);
    }

    ret = vfs_open(&fd, FILE_NAME("/user/test-2.txt"), VFS_O_CREAT | VFS_O_RDWR);
    ASSERT_EQ(ret, 0);

    ret = vfs_write(fd, "hello\n", 6);
    ASSERT_EQ(ret, 6);

    ASSERT_EQ(vfs_close(fd), 0);

printf(" 1 -------------------------------------------------\n");    
    vfs_unlink(FILE_NAME("/user/test-x.txt"));
    vfs_rename(FILE_NAME("/user/test-2.txt"), FILE_NAME("/test-x.txt"));
    //vfs_rename(FILE_NAME("/test-2.txt"), FILE_NAME("/test-3.txt"));

printf(" 2 -------------------------------------------------\n"); 
    vfs_unlink(FILE_NAME("/test-x.txt"));
    vfs_rename(FILE_NAME("/test.txt"), FILE_NAME("/test-x.txt")); 
}