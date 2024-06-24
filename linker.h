/*
 * Copyright 2022 wtcat(wt1454246140@163.com)
 */
#ifndef BASEWORK_LINKER_H_
#define BASEWORK_LINKER_H_

#include "basework/generic.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ROSET_SORTED_SECTION _XSTRING(.basework.roset)
#define RWSET_SORTED_SECTION _XSTRING(.basework.rwset)

#ifndef _XSTRING
#define _XSTRING(s) #s
#endif

#define LINKER_SET_BEGIN(set) _linker__set_##set##_begin
#define LINKER_SET_END(set) _linker__set_##set##_end
#define LINKER_SET_ALIGN(type) __aligned(sizeof(type))

/*
 * Readonly section
 */ 
#define LINKER_ROSET(set, type) \
    LINKER_SET_ALIGN(type) type const LINKER_SET_BEGIN(set)[0] \
    __section(ROSET_SORTED_SECTION "." #set ".begin") __used; \
    LINKER_SET_ALIGN(type) type const LINKER_SET_END(set)[0] \
    __section(ROSET_SORTED_SECTION "." #set ".end") __used

#define LINKER_ROSET_ITEM_ORDERED(set, type, item, order) \
    LINKER_SET_ALIGN(type) type const _Linker__set_##set##_##item \
    __section(ROSET_SORTED_SECTION "." #set ".content.0." _XSTRING(order)) __used\

/*
 * Read and write section
 */
#define LINKER_RWSET(set, type) \
    LINKER_SET_ALIGN(type) type const LINKER_SET_BEGIN(set)[0] \
    __section(RWSET_SORTED_SECTION "." #set ".begin") __used; \
    LINKER_SET_ALIGN(type) type const LINKER_SET_END(set)[0] \
    __section(RWSET_SORTED_SECTION "." #set ".end") __used

#define RTEMS_LINKER_RWSET_ITEM_ORDERED( set, type, item, order ) \
    LINKER_SET_ALIGN(type) type _Linker__set_##set##_##item \
    __section(RWSET_SORTED_SECTION "." #set ".content.0." _XSTRING(order)) __used\


static inline uintptr_t _linker_set_obfuscate(const void *ptr) {
    uintptr_t addr = (uintptr_t) ptr;
    __asm__ volatile("" : "+r" (addr));
    return addr;
}

/*
 * Foreach section
 */
#define LINKER_SET_FOREACH(set, item) \
    for (item = (void *)_linker_set_obfuscate(LINKER_SET_BEGIN(set)) ; \
        item != LINKER_SET_END( set ) ; \
        ++item \
    )
#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_LINKER_H_*/
