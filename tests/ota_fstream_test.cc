/*
 * Copyright 2023 wtcat
 */
#include "basework/utils/ota_fstream.h"

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include <fstream>
#include "gtest/gtest.h"

extern "C" {

static void* file_open(const char *name) {
    char path[128];
    int len;
    FILE *fp;

    len = snprintf(path, sizeof(path) - 1, "out/%s", name);
    path[len] = '\0';
    fp = fopen(path, "wb+");
    if (fp == NULL)
        printf("open failed: %d\n", errno);
    return (void *)fp;
}

static void file_close(void *fp) {
    if (fp)
        fclose((FILE *)fp);
}

static int file_write(void *fp, const void *buf, size_t size, 
    uint32_t offset) {
    fseek((FILE *)fp, offset, SEEK_SET);
    if (fwrite(buf, size, 1, (FILE *)fp) > 0)
        return size;
    return -1;
}

static const ota_fstream_ops ota_fops = {
    file_open,
    file_close,
    file_write
};

}

TEST(ota_fstream, download) {
    ota_fstream_set_ops(&ota_fops);
    if (access("out",  F_OK))
        mkdir("out", 0666);
    
    std::fstream fd("image.ota", std::ios::binary | std::ios::in);
    ASSERT_TRUE(fd.is_open());

    char buffer[256];
    fd.seekg(0, std::ios::beg);
    while (true) {
        fd.read(buffer, sizeof(buffer));
        if (ota_fstream_write(buffer, fd.gcount()) < 0) {
            printf("ota write error\n");
            break;
        }
        if (fd.eof())
            break;
    }
    
    ASSERT_TRUE(fd.eof());
    ota_fstream_finish();
}