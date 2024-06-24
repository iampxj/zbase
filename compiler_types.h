/* SPDX-License-Identifier: GPL-2.0 */
#ifndef BASEWORK_COMPILER_TYPES_H_
#define BASEWORK_COMPILER_TYPES_H_

#ifndef __ASSEMBLY__

#if defined(CONFIG_DEBUG_INFO_BTF) && defined(CONFIG_PAHOLE_HAS_BTF_TAG) && \
	__has_attribute(btf_type_tag)
# define BTF_TYPE_TAG(value) __attribute__((btf_type_tag(#value)))
#else
# define BTF_TYPE_TAG(value) /* nothing */
#endif

/* Indirect macros required for expanded argument pasting, eg. __LINE__. */
#define ___RTE_PASTE(a,b) a##b
#define __RTE_PASTE(a,b) ___RTE_PASTE(a,b)

/* Attributes */
#include "basework/compiler_attributes.h"

/* Builtins */

/*
 * __has_builtin is supported on gcc >= 10, clang >= 3 and icc >= 21.
 * In the meantime, to support gcc < 10, we implement __has_builtin
 * by hand.
 */
#ifndef __has_builtin
#define __has_builtin(x) (0)
#endif

/* Compiler specific macros. */
#ifdef __clang__
#include "basework/compiler-clang.h"
#elif defined(__INTEL_COMPILER)
#include "basework/compiler-intel.h"
#elif defined(__GNUC__)
/* The above compilers also define __GNUC__, so order is important here. */
#include "basework/compiler-gcc.h"
#else
#error "Unknown compiler"
#endif

/*
 * Some architectures need to provide custom definitions of macros provided
 * by linux/compiler-*.h, and can do so using asm/compiler.h. We include that
 * conditionally rather than using an asm-generic wrapper in order to avoid
 * build failures if any C compilation, which will include this file via an
 * -include argument in c_flags, occurs prior to the asm-generic wrappers being
 * generated.
 */
#ifdef CONFIG_HAVE_ARCH_COMPILER_H
#include <asm/compiler.h>
#endif

#define __rte_warn_unused_result __attribute__((warn_unused_result))

#define __rte_notrace			__attribute__((__no_instrument_function__))

/*
 * it doesn't make sense on ARM (currently the only user of __naked)
 * to trace naked functions because then mcount is called without
 * stack and frame pointer being set up and there is no chance to
 * restore the lr register to the value before mcount was called.
 */
#define __rte_naked			__attribute__((__naked__)) __rte_notrace

/*
 * Prefer gnu_inline, so that extern inline functions do not emit an
 * externally visible function. This makes extern inline behave as per gnu89
 * semantics rather than c99. This prevents multiple symbol definition errors
 * of extern inline functions at link time.
 * A lot of inline functions can cause havoc with function tracing.
 */
#define __rte_inline inline __rte_maybe_unused __rte_notrace

/*
 * gcc provides both __inline__ and __inline as alternate spellings of
 * the inline keyword, though the latter is undocumented. New kernel
 * code should only use the inline spelling, but some existing code
 * uses __inline__. Since we #define inline above, to ensure
 * __inline__ has the same semantics, we need this #define.
 *
 * However, the spelling __inline is strictly reserved for referring
 * to the bare keyword.
 */
#define __inline__ inline

/*
 * GCC does not warn about unused static inline functions for -Wunused-function.
 * Suppress the warning in clang as well by using __maybe_unused, but enable it
 * for W=1 build. This will allow clang to find unused functions. Remove the
 * __inline_maybe_unused entirely after fixing most of -Wunused-function warnings.
 */
#ifndef __rte_maybe_unused
#define __rte_maybe_unused __attribute__((__unused__))
#endif


/*
 * Rather then using noinline to prevent stack consumption, use
 * noinline_for_stack instead.  For documentation reasons.
 */
#ifndef __rte_noinline
#define __rte_noinline __attribute__((__noinline__))
#endif

#endif /* __ASSEMBLY__ */

/*
 * The below symbols may be defined for one or more, but not ALL, of the above
 * compilers. We don't consider that to be an error, so set them to nothing.
 * For example, some of them are for compiler specific plugins.
 */
#ifndef __latent_entropy
# define __latent_entropy
#endif


/*
 * Any place that could be marked with the "alloc_size" attribute is also
 * a place to be marked with the "malloc" attribute. Do this as part of the
 * __alloc_size macro to avoid redundant attributes and to avoid missing a
 * __malloc marking.
 */
#ifndef __rte_alloc_size
# ifdef __alloc_size__
#  define __alloc_size(x, ...)	__alloc_size__(x, ## __VA_ARGS__) __malloc
# else
#  define __rte_alloc_size(x, ...)	__malloc
# endif
#endif /* __alloc_size */

#define __rte_asm_volatile_goto(x...) asm goto(x)

#ifdef CONFIG_CC_HAS_ASM_INLINE
#define __rte_asm_inline asm __inline
#else
#define __rte_asm_inline asm
#endif

/* Are two types/vars the same type (ignoring qualifiers)? */
#define __rte_same_type(a, b) __builtin_types_compatible_p(__typeof(a), __typeof(b))
#define __rte_is_constexpr(x) __builtin_constant_p(x)

/*
 * __rte_unqual_scalar_typeof(x) - Declare an unqualified scalar type, leaving
 *			       non-scalar types unchanged.
 */
/*
 * Prefer C11 _Generic for better compile-times and simpler code. Note: 'char'
 * is not type-compatible with 'signed char', and we define a separate case.
 */
#define __rte_scalar_type_to_expr_cases(type)				\
		unsigned type:	(unsigned type)0,			\
		signed type:	(signed type)0

#define __rte_unqual_scalar_typeof(x) __typeof(				\
		_Generic((x),						\
			 char:	(char)0,				\
			 __rte_scalar_type_to_expr_cases(char),		\
			 __rte_scalar_type_to_expr_cases(short),		\
			 __rte_scalar_type_to_expr_cases(int),		\
			 __rte_scalar_type_to_expr_cases(long),		\
			 __rte_scalar_type_to_expr_cases(long long),	\
			 default: (x)))

/* Is this type a native word size -- useful for atomic operations */
#define __rte_native_word(t) \
	(sizeof(t) == sizeof(char) || sizeof(t) == sizeof(short) || \
	 sizeof(t) == sizeof(int) || sizeof(t) == sizeof(long))

#ifdef __OPTIMIZE__
# define __rte_compiletime_assert(condition, msg, prefix, suffix)		\
	do {								\
		/*							\
		 * __noreturn is needed to give the compiler enough	\
		 * information to avoid certain possibly-uninitialized	\
		 * warnings (regardless of the build failing).		\
		 */							\
		__rte_noreturn extern void prefix ## suffix(void)		\
			__rte_compiletime_error(msg);			\
		if (!(condition))					\
			prefix ## suffix();				\
	} while (0)
#else
# define __rte_compiletime_assert(condition, msg, prefix, suffix) do { } while (0)
#endif

#define _rte_compiletime_assert(condition, msg, prefix, suffix) \
	__rte_compiletime_assert(condition, msg, prefix, suffix)

/**
 * compiletime_assert - break build and emit msg if condition is false
 * @condition: a compile-time constant condition to check
 * @msg:       a message to emit if condition is false
 *
 * In tradition of POSIX assert, this macro will break the build if the
 * supplied condition is *false*, emitting the supplied error message if the
 * compiler has support to do so.
 */
#define rte_compiletime_assert(condition, msg) \
	_rte_compiletime_assert(condition, msg, rte_compiletime_assert_, __COUNTER__)

#define rte_compiletime_assert_atomic_type(t)				\
	rte_compiletime_assert(__rte_native_word(t),				\
		"Need native word sized stores/loads for atomicity.")

/* Helpers for emitting diagnostics in pragmas. */
#ifndef __rte_diag
#define __rte_diag(string)
#endif

#ifndef __rte_diag_GCC
#define __rte_diag_GCC(version, severity, string)
#endif

#define __rte_diag_push()	__rte_diag(push)
#define __rte_diag_pop()	__rte_diag(pop)

#define __rte_diag_ignore(compiler, version, option, comment) \
	__rte_diag_ ## compiler(version, ignore, option)
#define __rte_diag_warn(compiler, version, option, comment) \
	__rte_diag_ ## compiler(version, warn, option)
#define __rte_diag_error(compiler, version, option, comment) \
	__rte_diag_ ## compiler(version, error, option)

#ifndef __rte_diag_ignore_all
#define __rte_diag_ignore_all(option, comment)
#endif

#endif /* BASEWORK_COMPILER_TYPES_H_ */
