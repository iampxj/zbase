/* SPDX-License-Identifier: GPL-2.0 */
#ifndef BASEWORK_COMPILER_TYPES_H_
#error "Please don't include <basework/compiler-clang.h> directly, include <basework/compiler.h> instead."
#endif

/* Compiler specific definitions for Clang compiler */
#define rte_likely(x)   __builtin_expect(!!(x), 1)
#define rte_unlikely(x) __builtin_expect(!!(x), 0)

/* same as gcc, this was present in clang-2.6 so we can assume it works
 * with any version that can compile the kernel
 */
#define __RTE_UNIQUE_ID(prefix) __RTE_PASTE(__RTE_PASTE(__UNIQUE_ID_, prefix), __COUNTER__)

/*
 * Turn individual warnings and errors on and off locally, depending
 * on version.
 */
#define __rte_diag_clang(version, severity, s) \
	__rte_diag_clang_ ## version(__diag_clang_ ## severity s)

/* Severity used in pragma directives */
#define __rte_diag_clang_ignore	ignored
#define __rte_diag_clang_warn	warning
#define __rte_diag_clang_error	error

#define __rte_diag_str1(s)		#s
#define __rte_diag_str(s)		__rte_diag_str1(s)
#define __rte_diag(s)		_Pragma(__rte_diag_str(clang diagnostic s))

#if CONFIG_CLANG_VERSION >= 110000
#define __rte_diag_clang_11(s)	__rte_diag(s)
#else
#define __rte_diag_clang_11(s)
#endif

#define __rte_diag_ignore_all(option, comment) \
	__rte_diag_clang(11, ignore, option)
