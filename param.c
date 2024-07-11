/*
 * Copyright 2024 wtcat
 */

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "basework/os/osapi.h"
#include "basework/param.h"
#include "basework/log.h"
#include "basework/errno.h"


#define dump_fmt(param, fmt, ...) \
    pr_out("%s: %" fmt "\n", param->name, ##__VA_ARGS__) 

os_critical_global_declare
LINKER_ROSET(param, struct param_struct);

static int 
symbol_compare(const void *a, const void *b) {
    const struct param_struct *pa = a;
    const struct param_struct *pb = b;
    return strcmp(pa->name, pb->name);
}

int 
rte_param_write(const struct param_struct *param, const void *pv, int type) {
    os_critical_declare
    int ptype;

    if (param == NULL)
        return -EINVAL;

    if (!(param->flags & PARAM_WR))
        return -EACCES;
    
    ptype = param->flags & PARAM_TYPEMASK;
    if (type && type != ptype)
        return -EINVAL;
    
    os_critical_lock
    switch (ptype) {
    case PARAM_INT:
       *(int *)param->p = *(int *)pv;
        break;
    case PARAM_STRING:
        strcpy(*(char **)param->p, (const char *)pv);
        break;
    case PARAM_S64:
        *(int64_t *)param->p = *(int64_t *)pv;
        break;
    case PARAM_UINT:
        *(unsigned int *)param->p = *(unsigned int *)pv;
        break;
    case PARAM_LONG:
        *(long *)param->p = *(long *)pv;
        break;
    case PARAM_ULONG:
        *(unsigned long *)param->p = *(unsigned long *)pv;
        break;
    case PARAM_U64:
        *(uint64_t *)param->p = *(uint64_t *)pv;
        break; 
    case PARAM_U8:
        *(uint8_t *)param->p = *(uint8_t *)pv;
        break; 
    case PARAM_U16:
        *(uint16_t *)param->p = *(uint16_t *)pv;
        break; 
    case PARAM_S8:
        *(int8_t *)param->p = *(int8_t *)pv;
        break; 
    case PARAM_S16:
        *(int16_t *)param->p = *(int16_t *)pv;
        break; 
    case PARAM_S32:
        *(int32_t *)param->p = *(int32_t *)pv;
        break; 
    case PARAM_U32:
        *(uint32_t *)param->p = *(uint32_t *)pv;
        break;
    default:
        os_critical_unlock
        return -EINVAL;
    }
    os_critical_unlock
    return 0;
}

const struct param_struct *
rte_param_serach(const char *name) {
    return (const struct param_struct *)bsearch(name,
            (void *)LINKER_SET_BEGIN(param), 
            LINKER_SET_END(param) - LINKER_SET_BEGIN(param),
            sizeof(struct param_struct),
            symbol_compare);
}

void 
rte_param_dump(void) {
    const struct param_struct *param;

    pr_out("\n*** show parameters ***\n\n");
    LINKER_SET_FOREACH(param, param) {
        switch (param->flags & PARAM_TYPEMASK) {
        case PARAM_INT:
            dump_fmt(param, PRId32, *(int *)param->p);
            break;
        case PARAM_STRING:
            dump_fmt(param, "s", *(const char **)param->p);
            break;
        case PARAM_S64:
            dump_fmt(param, PRId64, *(int64_t *)param->p);
            break;
        case PARAM_UINT:
            dump_fmt(param, PRIu32, *(unsigned int *)param->p);
            break;
        case PARAM_LONG:
#ifdef RTE_CPU_64
            dump_fmt(param, PRId64, *(long *)param->p);
#else
            dump_fmt(param, PRId32, *(long *)param->p);
#endif
            break;
        case PARAM_ULONG:
#ifdef RTE_CPU_64
            dump_fmt(param, PRIu64, *(unsigned long *)param->p);
#else
            dump_fmt(param, PRIu32, *(unsigned long *)param->p);
#endif
            break;
        case PARAM_U64:
            dump_fmt(param, PRIu64, *(uint64_t *)param->p);
            break; 
        case PARAM_U8:
            dump_fmt(param, PRIu8, *(uint8_t *)param->p);
            break; 
        case PARAM_U16:
            dump_fmt(param, PRIu16, *(uint16_t *)param->p);
            break; 
        case PARAM_S8:
            dump_fmt(param, PRId8, *(int8_t *)param->p);
            break; 
        case PARAM_S16:
            dump_fmt(param, PRId16, *(int16_t *)param->p);
            break; 
        case PARAM_S32:
            dump_fmt(param, PRId32, *(int32_t *)param->p);
            break; 
        case PARAM_U32:
            dump_fmt(param, PRIu32, *(uint32_t *)param->p);
            break;
        default:
            break;
        } 
    }
}
