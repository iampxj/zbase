/*
 * Copyright 2022 wtcat(wt1454246140@163.com)
 */
#ifndef BASEWORK_LINKER_H_
#define BASEWORK_LINKER_H_

#include "basework/generic.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * KEEP(*(SORT_BY_NAME(.basework.roset*)))
 */

#ifdef CONFIG_BASEWORK_ROSET
#define ROSET_SORTED_SECTION CONFIG_BASEWORK_ROSET
#else
#define ROSET_SORTED_SECTION ".basework.roset"
#endif /* CONFIG_BASEWORK_ROSET */

#ifdef CONFIG_BASEWORK_RWSET
#define RWSET_SORTED_SECTION CONFIG_BASEWORK_RWSET
#else
#define RWSET_SORTED_SECTION ".basework.rwset"
#endif /* CONFIG_BASEWORK_RWSET */

#ifndef _XSTRING
#define _XSTRING(s) #s
#endif

#define LINKER_SET_BEGIN(set) _linker__set_##set##_begin
#define LINKER_SET_END(set) _linker__set_##set##_end
#define LINKER_SET_ALIGN(type) //__rte_aligned(sizeof(type))

/*
 * Readonly section
 */ 
#define LINKER_ROSET(set, type) \
    LINKER_SET_ALIGN(type) type const LINKER_SET_BEGIN(set)[0] \
    __rte_section(ROSET_SORTED_SECTION "." #set ".begin") __rte_used; \
    LINKER_SET_ALIGN(type) type const LINKER_SET_END(set)[0] \
    __rte_section(ROSET_SORTED_SECTION "." #set ".end") __rte_used

#define LINKER_ROSET_ITEM_ORDERED(set, type, item, order) \
    LINKER_SET_ALIGN(type) type const _Linker__set_##set##_##item \
    __rte_section(ROSET_SORTED_SECTION "." #set ".content.0." _XSTRING(order)) __rte_used\

/*
 * Read and write section
 */
#define LINKER_RWSET(set, type) \
    LINKER_SET_ALIGN(type) type const LINKER_SET_BEGIN(set)[0] \
    __rte_section(RWSET_SORTED_SECTION "." #set ".begin") __rte_used; \
    LINKER_SET_ALIGN(type) type const LINKER_SET_END(set)[0] \
    __rte_section(RWSET_SORTED_SECTION "." #set ".end") __rte_used

#define LINKER_RWSET_ITEM_ORDERED( set, type, item, order ) \
    LINKER_SET_ALIGN(type) type _Linker__set_##set##_##item \
    __rte_section(RWSET_SORTED_SECTION "." #set ".content.0." _XSTRING(order)) __rte_used\

/*
 * Foreach section
 */
#define LINKER_SET_FOREACH(set, item, type) \
    for (volatile type *item = (volatile type *)LINKER_SET_BEGIN(set); \
        item < (volatile type *)LINKER_SET_END(set); \
        ++item \
    )


#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_LINKER_H_*/
