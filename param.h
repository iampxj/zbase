/*
 * Copyright 2024 wtcat
 */

#ifndef BASEWORK_PARAM_H_
#define BASEWORK_PARAM_H_

#include "basework/generic.h"
#include "basework/linker.h"

#ifdef __cplusplus
extern "C"{
#endif

struct param_struct {
    const char *name;
    uint32_t flags;
#define PARAM_WR  0x10000

#define PARAM_TYPEMASK 0xf
#define	PARAM_INT	   0x2
#define	PARAM_STRING   0x3
#define	PARAM_S64	   0x4
#define	PARAM_UINT	   0x6
#define	PARAM_LONG	   0x7
#define	PARAM_ULONG	   0x8
#define	PARAM_U64	   0x9
#define	PARAM_U8	   0xa
#define	PARAM_U16	   0xb
#define	PARAM_S8	   0xc
#define	PARAM_S16	   0xd
#define	PARAM_S32	   0xe
#define	PARAM_U32	   0xf
    void *const p;
};

// LINKER_ROSET(param, struct param_struct);

#define EXPORT_PARAM(_var, _attr) \
    static const char _param_name__##_var[] \
    __rte_section(ROSET_SORTED_SECTION ".NAME") __rte_used = {#_var}; \
    static LINKER_ROSET_ITEM_ORDERED(param, struct param_struct, \
    _param__##_var, _var) = { \
        .name  = _param_name__##_var, \
        .flags = _attr, \
        .p = &_var \
    }

#define __param_type(v) \
({ \
    int __type_id; \
    if (__rte_same_type(v, (int)0)) __type_id = PARAM_INT; \
    else if (__rte_same_type(v, (char *)0) || \
        __rte_same_type(v, (const char *)0)) __type_id = PARAM_STRING; \
    else if (__rte_same_type(v, (int64_t)0)) __type_id = PARAM_S64; \
    else if (__rte_same_type(v, (unsigned int)0)) __type_id = PARAM_UINT; \
    else if (__rte_same_type(v, (long)0)) __type_id = PARAM_LONG; \
    else if (__rte_same_type(v, (unsigned long)0)) __type_id = PARAM_ULONG; \
    else if (__rte_same_type(v, (uint64_t)0)) __type_id = PARAM_U64; \
    else if (__rte_same_type(v, (uint8_t)0)) __type_id = PARAM_U8; \
    else if (__rte_same_type(v, (uint16_t)0)) __type_id = PARAM_U16; \
    else if (__rte_same_type(v, (int8_t)0)) __type_id = PARAM_S8; \
    else if (__rte_same_type(v, (int16_t)0)) __type_id = PARAM_S16; \
    else if (__rte_same_type(v, (int32_t)0)) __type_id = PARAM_S32; \
    else if (__rte_same_type(v, (uint32_t)0)) __type_id = PARAM_U32; \
    else rte_compiletime_assert(0, ""); __type_id = 0; \
    __type_id; \
})


#define RTE_PARAM_WRITE(_name, _val) \
({ \
    typeof(_val) __val = _val; \
    int __err; \
    __err = rte_param_write(rte_param_serach(_name), &__val, __param_type(_val)); \
    __err; \
})

int rte_param_write(const struct param_struct *param, const void *pv, int type);
const struct param_struct *rte_param_serach(const char *name);
void rte_param_dump(void);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_PARAM_H_ */
