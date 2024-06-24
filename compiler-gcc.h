/* SPDX-License-Identifier: GPL-2.0 */
#ifndef BASEWORK_COMPILER_TYPES_H_
#error "Please don't include <basework/compiler-gcc.h> directly, include <linux/compiler.h> instead."
#endif

/*
 * Common definitions for all gcc versions go here.
 */
#define GCC_VERSION (__GNUC__ * 10000		\
		     + __GNUC_MINOR__ * 100	\
		     + __GNUC_PATCHLEVEL__)


#define rte_likely(x)   __builtin_expect(!!(x), 1)
#define rte_unlikely(x) __builtin_expect(!!(x), 0)

#ifdef CONFIG_RETPOLINE
#define __noretpoline __attribute__((__indirect_branch__("keep")))
#endif

#define __RTE_UNIQUE_ID(prefix) __RTE_PASTE(__RTE_PASTE(__UNIQUE_ID_, prefix), __COUNTER__)

#if defined(LATENT_ENTROPY_PLUGIN) && !defined(__CHECKER__)
#define __latent_entropy __attribute__((latent_entropy))
#endif

/*
 * calling noreturn functions, __builtin_unreachable() and __builtin_trap()
 * confuse the stack allocation in gcc, leading to overly large stack
 * frames, see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=82365
 *
 * Adding an empty inline assembly before it works around the problem
 */
#define rte_barrier_before_unreachable() asm volatile("")

/*
 * Mark a position in code as unreachable.  This can be used to
 * suppress control flow warnings after asm blocks that transfer
 * control elsewhere.
 */
#define rte_unreachable() __builtin_unreachable()


#if GCC_VERSION >= 70000
#define KASAN_ABI_VERSION 5
#else
#define KASAN_ABI_VERSION 4
#endif

#ifdef CONFIG_SHADOW_CALL_STACK
#define __rte_noscs __attribute__((__no_sanitize__("shadow-call-stack")))
#endif

/*
 * Turn individual warnings and errors on and off locally, depending
 * on version.
 */
#define __rte_diag_GCC(version, severity, s) \
	__rte_diag_GCC_ ## version(__diag_GCC_ ## severity s)

/* Severity used in pragma directives */
#define __rte_diag_GCC_ignore	ignored
#define __rte_diag_GCC_warn		warning
#define __rte_diag_GCC_error	error

#define __rte_diag_str1(s)		#s
#define __rte_diag_str(s)		__rte_diag_str1(s)
#define __rte_diag(s)		_Pragma(__rte_diag_str(GCC diagnostic s))

#if GCC_VERSION >= 80000
#define __rte_diag_GCC_8(s)		__rte_diag(s)
#else
#define __rte_diag_GCC_8(s)
#endif

#define __rte_diag_ignore_all(option, comment) \
	__rte_diag_GCC(8, ignore, option)

/*
 * Prior to 9.1, -Wno-alloc-size-larger-than (and therefore the "alloc_size"
 * attribute) do not work, and must be disabled.
 */
#if GCC_VERSION < 90100
#undef __alloc_size__
#endif
