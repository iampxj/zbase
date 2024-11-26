/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_LIBENV_H_
#define BASEWORK_LIBENV_H_

#include <stddef.h>
#include "basework/compiler.h"
#include "basework/assert.h"

#ifdef __cplusplus
extern "C"{
#endif

struct env_context;
struct env_alloctor {
    void *(*alloc)(size_t size);
    void *(*realloc)(void *p, size_t size);
    void  (*release)(void *p);
};

extern struct env_context *_sys_env__context;

/*
 * Default Interface
 */
#define rte_getenv(name) __getenv(_sys_env__context, name)
#define rte_unset(name)  __unsetenv(_sys_env__context, name)
#define rte_setenv(name, val, rewrite) \
    __setenv(_sys_env__context, name, val, rewrite)
#define rte_dumpenv() __dumpenv(_sys_env__context)
#define rte_initenv(_alloctor) \
    ({\
        _sys_env__context = __allocenv(_alloctor); \
        rte_assert0(_sys_env__context != NULL); 0;\
    })

/*
 * Low-level interface
 */
char *__getenv(struct env_context *env, const char *name);
int   __setenv(struct env_context *env, const char *name, 
    const char *value, int rewrite);
int   __unsetenv(struct env_context *env, const char *name);
struct env_context *__allocenv(const struct env_alloctor *allocator);
void __dumpenv(struct env_context *env);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_LIBENV_H_ */
