/*
 * Copyright 2023 wtcat
 */
#ifndef BASEWORK_UTILS_OTA_FSTREAM_H_
#define BASEWORK_UTILS_OTA_FSTREAM_H_

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "basework/hook.h"

#ifdef __cplusplus
extern "C"{
#endif
struct file_header;

struct ota_fstream_ops {
    void *(*open)(const char *name, size_t fsize);
    void (*close)(void *fd);
    int  (*write)(void *fd, const void *buf, size_t size, uint32_t offset);
    void (*completed)(int err, void *fd, const char *filename, size_t size);
};

int  ota_fstream_set_ops(const struct ota_fstream_ops *ops);
int  ota_fstream_set_notify(int (*notify)(const char *, int));
int  ota_fstream_set_kvfn(unsigned int (*getv)(const char *));
int  ota_fstream_set_envchecker(bool (*check_env)(const struct file_header *header));
int  ota_fstream_write(const void *buf, size_t size);
void ota_fstream_finish(void);
uint32_t ota_fstream_get_devid(void);

struct file_header;
RTE_HOOK_DECLARE(ota_finish, const struct file_header *header, int err);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_UTILS_OTA_FSTREAM_H_ */
