/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <errno.h>
#include <stdint.h>
#include <stdio.h>

#include "basework/lib/msg_storage.h"
 
static int msg_file_open(const char *name, void **fd) {
    FILE *fp = fopen(name, "rb+");
    if (fp == NULL) {
        fp = fopen(name, "wb+");
        if (fp == NULL)
            return -EINVAL;
    }
    *fd = (void *)fp;
    return 0;
}

static int msg_file_close(void *fd) {
    return fclose((FILE *)fd);
}

static int msg_file_flush(void *fd) {
    return fflush((FILE *)fd);
}

static ssize_t msg_file_write(void *fd, const void *buf, 
    size_t size, uint32_t offset) {
    FILE *fp = (FILE *)fd;
    int ret;

    fseek(fp, offset, SEEK_SET);
    ret = fwrite(buf, size, 1, fp);
    if (ret > 0)
        return size;
    return ret;
}

static ssize_t msg_file_read(void *fd, void *buf, 
    size_t size, uint32_t offset) {
    FILE *fp = (FILE *)fd;
    int ret;

    fseek(fp, offset, SEEK_SET);
    ret = fread(buf, size, 1, fp);
    if (ret > 0)
        return size;
    return ret;
}

static int msg_file_remove(const char *name) {
    return remove(name);
}

int msg_file_backend_init(void) {
    static const struct msg_fops msg_file_ops = {
        .open = msg_file_open,
        .close = msg_file_close,
        .flush = msg_file_flush,
        .write = msg_file_write,
        .read = msg_file_read,
        .unlink = msg_file_remove
    };

    int err = msg_storage_register(&msg_file_ops);
    if (!err)
        err = msg_storage_init();
    return err;
}