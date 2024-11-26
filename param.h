/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_PARAM_H_
#define BASEWORK_PARAM_H_

#include <stdlib.h>
#include "basework/lib/libenv.h"

#ifdef __cplusplus
extern "C"{
#endif

/*
 * sys_getenv_str - Get string parameter for system
 *
 * @key: name
 * @default_val: default value
 */
static inline const char *
sys_getenv_str(const char *key, const char *default_val) {
    const char *p = rte_getenv(key);
    if (!p)
        p = default_val;
    return p;
}

/*
 * sys_getenv_val - Get integer parameter for system
 *
 * @key: key name
 * @default_val: default value
 * @base: base of the interpreted integer value
 */
static inline unsigned long
sys_getenv_val(const char *key, unsigned long default_val, int base) {
    const char *p = rte_getenv(key);
    unsigned long v = default_val;
    if (p)
        v = strtoul(p, NULL, base);
    return v;
}

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_PARAM_H_ */
