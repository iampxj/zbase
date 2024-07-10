/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_DEV_FIFOFS_H_
#define BASEWORK_DEV_FIFOFS_H_

#include <stddef.h>
#include "basework/compiler_attributes.h"
#include "basework/generic.h"

#ifdef __cplusplus
extern "C"{
#endif

#ifdef RTE_CPU_64
#define _FIFO_BUFFER_SIZE 64
#else
#define _FIFO_BUFFER_SIZE 32
#endif

#define FIFO_FILE_STRUCT_SIZE sizeof(struct fifo_filemem)

struct fifo_filemem {
    char buffer[_FIFO_BUFFER_SIZE] __rte_aligned(sizeof(void *));
};

int fifofs_register(struct fifo_filemem fds[], size_t n);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEV_FIFOFS_H_ */
