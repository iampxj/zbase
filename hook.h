/*
 * Copyright 2023 wtcat
 */
#ifndef BASEWORK_HOOK_H_
#define BASEWORK_HOOK_H_

#include "basework/compiler.h"

#ifdef __cplusplus
extern "C"{
#endif

/*
 * Declare rte-hook interface
 *
 * @name: module name
 * @params: parameters type list
 */
#define RTE_HOOK_DECLARE(name, params...) \
    __RTE_HOOK_DECLARE(name, void, params)

#define RTE_HOOKRET_DECLARE(name, rettype, params...) \
    __RTE_HOOK_DECLARE(name, rettype, params)

/* Define hook instance */
#define RTE_HOOK_INSTANCE(name, _attr) \
    name##_hook_t __rte_##name##_hook _attr;

/* Set hook function */
#define RTE_HOOK_SET(name, hook) \
    name##_set_hook(hook)

/* Get hook function */
#define RTE_HOOK_GET(name) \
    name##_get_hook()

#define RTE_HOOK(name, ...) \
    __rte_##name##_hook(__VA_ARGS__)

#define RTE_HOOK_SAFE(name, ...) \
    (__rte_##name##_hook? __rte_##name##_hook(__VA_ARGS__): 0)

#define __RTE_HOOK_DECLARE(name, rettype, params...) \
    typedef rettype (*name##_hook_t)(params); \
    extern name##_hook_t __rte_##name##_hook; \
    static __rte_inline name##_hook_t name##_get_hook(void) { \
        return __rte_##name##_hook; \
    } \
    static __rte_inline name##_hook_t name##_set_hook(name##_hook_t fn) { \
        name##_hook_t old_fn; \
        old_fn = __rte_##name##_hook; \
        __rte_##name##_hook = fn; \
        return old_fn; \
    }

/*
 * Static hook-function definition
 */

/* Overwrite hook */
#define RTE_OVERHOOK(name, param...) name(param)

/* Default hook */
#define RTE_WEAKHOOK(name, param...)  __rte_weak name(param)

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_HOOK_H_ */