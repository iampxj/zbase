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
    unsigned int flags;
#define PARAM_RO  0x10000 /* Readonly */

#define PARAM_TYPEMASK 0xf
#define PARAM_POINTER  0x1
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

#define RTE_EXPORT_PARAM(_var, _attr) \
    static const char _param_name__##_var[] \
    __rte_section(ROSET_SORTED_SECTION ".NAME") __rte_used = {#_var}; \
    static LINKER_ROSET_ITEM_ORDERED(param, struct param_struct, \
    _param__##_var, _var) = { \
        .name  = _param_name__##_var, \
        .flags = _attr, \
        .p = &_var \
    }

#define RTE_PARAM_WRITE(_name, _val) \
({ \
    typeof(_val) __val = _val; \
    int __err = rte_param_write(rte_param_serach(_name), (int64_t)__val); \
    __err; \
})

int rte_param_write(const struct param_struct *param, int64_t pv);
const struct param_struct *rte_param_serach(const char *name);
void rte_param_dump(void);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_PARAM_H_ */
