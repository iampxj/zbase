/*
 * Copyright 2024 wtcat
 */
#include <unistd.h>
#include "basework/os/osapi.h"

#include "gtest/gtest.h"


static void *fifo_reader(void *arg) {
    int *should_stop = (int *)arg;
    os_file_t fd;
    char buffer[128];
    int ret;

    ret = vfs_open(&fd, "/fifo:/pipe", VFS_O_RDWR);
    if (ret)
        goto _exit;

    printf("fifo reader start\n");

    while (!*should_stop) {
        memset(buffer, 0, sizeof(buffer));
        ret = vfs_read(fd, buffer, 27);
        printf("Read(%d bytes): %s\n", ret, buffer);
    }

    vfs_close(fd);
    
_exit:
    printf("reader thread exited\n");
    pthread_exit((void *)(uintptr_t)ret);
    return NULL;
}

TEST(fifofs, read_write) {
    const char text[] = {"fifo thread: hello, world\n"};
    os_file_t fd;
    pthread_t thr;
    int thrstop = 0;

    ASSERT_EQ(vfs_open(&fd, "/fifo:/pipe", VFS_O_RDWR|VFS_O_CREAT, 128), 0);
    ASSERT_EQ(pthread_create(&thr, NULL, fifo_reader, &thrstop), 0);
    usleep(10000);

    for (int i = 0; i < 10; i++) {
        printf("%d> send %lu bytes\n", i, sizeof(text));
        EXPECT_EQ(vfs_write(fd, text, sizeof(text)), (ssize_t)sizeof(text));
        usleep(1000000);
    }

    thrstop = 1;
    ASSERT_EQ(vfs_write(fd, text, 16), 16);
    ASSERT_EQ(pthread_join(thr, nullptr), 0);
    ASSERT_EQ(vfs_close(fd), 0);
}