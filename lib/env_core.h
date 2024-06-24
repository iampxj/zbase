/*
 * Copyright 2022 wtcat
 */
#ifndef BASEWORK_LIB_ENVCORE_H_
#define BASEWORK_LIB_ENVCORE_H_

#include <stdbool.h>
#include <stddef.h>

#include "basework/os/osapi.h"

#ifdef __cplusplus
extern "C"{
#endif

#ifdef _WIN32
#undef environ 
#endif

#define ENV_MAX  1024

struct env_reent {
    char *intial_env[1];
    char **environ;
    char ***p_environ;
    void *(*alloc)(size_t);
    void *(*realloc)(void *, size_t);
    void (*free)(void *);
    bool alloced;
#ifndef CONFIG_BOOTLOADER
    os_mutex_t mtx;
#endif
};

#define _ENV_DEFINE(_name, _malloc, _realloc, _free) \
    struct env_reent _name = {\
        .intial_env = {0}, \
        .environ = &_name.intial_env[0], \
        .p_environ = &_name.environ, \
        .alloc = _malloc, \
        .realloc = _realloc, \
        .free = _free, \
        .alloced = false \
    }

char *_env_get(struct env_reent *reent_ptr, const char *name);
int _env_unset(struct env_reent *reent_ptr, const char *name);
int _env_set(struct env_reent *reent_ptr, const char *name,
	const char *value, int rewrite);
int _env_load(struct env_reent *reent_ptr, 
    int (*readline_cb)(void *ctx, void *buffer, size_t max_size), 
    void *ctx);
int _env_flush(struct env_reent *reent_ptr, 
    int (*write_cb)(void *ctx, void *buffer, size_t size), 
    void *ctx);
void _env_free(struct env_reent *reent_ptr);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_LIB_ENVCORE_H_ */
