/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_DEV_FIFOFS_H_
#define BASEWORK_DEV_FIFOFS_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

#define FIFO_FILE_STRUCT_SIZE sizeof(struct fifo_filemem)

struct fifo_filemem {
    char buffer[sizeof(void *) == 8? 56: 32];
};

int fifofs_register(struct fifo_filemem fds[], size_t n);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEV_FIFOFS_H_ */
