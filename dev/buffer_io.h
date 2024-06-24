/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_DEV_BUFFER_IO_H_
#define BASEWORK_DEV_BUFFER_IO_H_

#include "basework/os/osapi.h"

#ifdef __cplusplus
extern "C"{
#endif
struct disk_device;

struct buffered_io {
    struct disk_device *dd;
    struct buffered_io *next;
    os_mutex_t mtx;
    uint32_t   offset;
    uint32_t   size;  /* Block size */
    uint32_t   mask;  /* Block size mask */
    bool       dirty;
    bool       allocated;
    bool       wrlimit;
    bool       panic;
    uint8_t    *buf;  /* Block buffer */
};

/*
 * The following functions can only be by exception context
 */
void buffered_iopanic(void);
int  buffered_flush_locked(struct buffered_io *bio);


ssize_t buffered_read(struct buffered_io *bio, void *buf, size_t size,
    uint32_t offset);

ssize_t buffered_write(struct buffered_io *bio, const void *buf, size_t size,
    uint32_t offset);

int buffered_ioflush(struct buffered_io *bio, bool invalid);

int buffered_ioflush_all(bool invalid);

int buffered_ioinit(struct disk_device *dd, struct buffered_io *bio,
    void *blkbuf, size_t blksize, bool wrlimit);

int buffered_iocreate(struct disk_device *dd, size_t blksize, bool wrlimit,
    struct buffered_io **pbio);

int buffered_iodestroy(struct buffered_io *bio);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEV_BUFFER_IO_H_ */
