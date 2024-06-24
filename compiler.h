/* SPDX-License-Identifier: GPL-2.0 */
#ifndef BASEWORK_COMPILER_H_
#define BASEWORK_COMPILER_H_

#if !defined(_MSC_VER)

#include "basework/rte_cpu.h"
#include "basework/compiler_types.h"

#define RTE_BUILD_BUG(e) ((int)(sizeof(struct { int:(-!!(e)); })))

#ifndef __ASSEMBLY__

#define __rte_cache_aligned __rte_aligned(RTE_CACHE_LINE_SIZE)

/* Optimization barrier */
#ifndef rte_compile_barrier
/* The "volatile" is due to gcc bugs */
# define rte_compile_barrier() __asm__ __volatile__("": : :"memory")
#endif

#ifndef rte_barrier_data
/*
 * This version is i.e. to prevent dead stores elimination on @ptr
 * where gcc and llvm may behave differently when otherwise using
 * normal barrier(): while gcc behavior gets along with a normal
 * barrier(), llvm needs an explicit input variable to be assumed
 * clobbered. The issue is as follows: while the inline asm might
 * access any memory it wants, the compiler could have fit all of
 * @ptr into memory registers instead, and since @ptr never escaped
 * from that, it proved that the inline asm wasn't touching any of
 * it. This version works well with both compilers, i.e. we're telling
 * the compiler that the inline asm absolutely may see the contents
 * of @ptr. See also: https://llvm.org/bugs/show_bug.cgi?id=15495
 */
# define rte_barrier_data(ptr) __asm__ __volatile__("": :"r"(ptr) :"memory")
#endif

/* workaround for GCC PR82365 if needed */
#ifndef rte_barrier_before_unreachable
# define rte_barrier_before_unreachable() do { } while (0)
#endif

#ifndef rte_unreachable
# define rte_unreachable() do {		\
	__builtin_unreachable();	\
} while (0)
#endif

/* Not-quite-unique ID. */
#ifndef __RTE_UNIQUE_ID
# define __RTE_UNIQUE_ID(prefix) __RTE_PASTE(__RTE_PASTE(__UNIQUE_ID_, prefix), __LINE__)
#endif

#endif /* __ASSEMBLY__ */

/* &a[0] degrades to a pointer: a different type from an array */
#define __rte_must_be_array(a)	RTE_BUILD_BUG(__rte_same_type((a), &(a)[0]))


#include "basework/rwonce.h"

#else /* !_MSC_VER */

# define __rte_notrace
# define __rte_weak
#endif /* _MSC_VER */
#endif /* BASEWORK_COMPILER_H_ */
