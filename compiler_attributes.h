/* SPDX-License-Identifier: GPL-2.0 */
#ifndef BASEWORK_COMPILER_ATTRIBUTES_H_
#define BASEWORK_COMPILER_ATTRIBUTES_H_

/*
 * The attributes in this file are unconditionally defined and they directly
 * map to compiler attribute(s), unless one of the compilers does not support
 * the attribute. In that case, __has_attribute is used to check for support
 * and the reason is stated in its comment ("Optional: ...").
 *
 * Any other "attributes" (i.e. those that depend on a configuration option,
 * on a compiler, on an architecture, on plugins, on other attributes...)
 * should be defined elsewhere (e.g. compiler_types.h or compiler-*.h).
 * The intention is to keep this file as simple as possible, as well as
 * compiler- and version-agnostic (e.g. avoiding GCC_VERSION checks).
 *
 * This file is meant to be sorted (by actual attribute name,
 * not by #define identifier). Use the __attribute__((__name__)) syntax
 * (i.e. with underscores) to avoid future collisions with other macros.
 * Provide links to the documentation of each supported compiler, if it exists.
 */

/*
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-alias-function-attribute
 */
#define __rte_alias(symbol)                 __attribute__((__alias__(#symbol)))

/*
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-aligned-function-attribute
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html#index-aligned-type-attribute
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Variable-Attributes.html#index-aligned-variable-attribute
 */
#define __rte_aligned(x)                    __attribute__((__aligned__(x)))
#define __rte_aligned_largest               __attribute__((__aligned__))

/*
 * Note: do not use this directly. Instead, use __alloc_size() since it is conditionally
 * available and includes other attributes.
 *
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-alloc_005fsize-function-attribute
 * clang: https://clang.llvm.org/docs/AttributeReference.html#alloc-size
 */
// #define __alloc_size__(x, ...)		__attribute__((__alloc_size__(x, ## __VA_ARGS__)))

/*
 * Note: users of __always_inline currently do not write "inline" themselves,
 * which seems to be required by gcc to apply the attribute according
 * to its docs (and also "warning: always_inline function might not be
 * inlinable [-Wattributes]" is emitted).
 *
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-always_005finline-function-attribute
 * clang: mentioned
 */
#define __rte_always_inline                 inline __attribute__((__always_inline__))

/*
 * The second argument is optional (default 0), so we use a variadic macro
 * to make the shorthand.
 *
 * Beware: Do not apply this to functions which may return
 * ERR_PTRs. Also, it is probably unwise to apply it to functions
 * returning extra information in the low bits (but in that case the
 * compiler should see some alignment anyway, when the return value is
 * massaged by 'flags = ptr & 3; ptr &= ~3;').
 *
 * Optional: not supported by icc
 *
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-assume_005faligned-function-attribute
 * clang: https://clang.llvm.org/docs/AttributeReference.html#assume-aligned
 */
#if __has_attribute(__assume_aligned__)
# define __rte_assume_aligned(a, ...)       __attribute__((__assume_aligned__(a, ## __VA_ARGS__)))
#else
# define __rte_assume_aligned(a, ...)
#endif

/*
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-cold-function-attribute
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Label-Attributes.html#index-cold-label-attribute
 */
#define __rte_cold                          __attribute__((__cold__))
#define __rte_hot                           __attribute__((__hot__))

/*
 * Note the long name.
 *
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-const-function-attribute
 */
#define __rte_attribute_const              __attribute__((__const__))

/*
 * Optional: only supported since gcc >= 9
 * Optional: not supported by clang
 * Optional: not supported by icc
 *
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-copy-function-attribute
 */
#if __has_attribute(__copy__)
# define __rte_copy(symbol)                 __attribute__((__copy__(symbol)))
#else
# define __rte_copy(symbol)
#endif

/*
 * Optional: not supported by gcc
 * Optional: only supported since clang >= 14.0
 * Optional: not supported by icc
 *
 * clang: https://clang.llvm.org/docs/AttributeReference.html#diagnose_as_builtin
 */
#if __has_attribute(__diagnose_as_builtin__)
# define __rte_diagnose_as(builtin...)	__attribute__((__diagnose_as_builtin__(builtin)))
#else
# define __rte_diagnose_as(builtin...)
#endif

/*
 * Don't. Just don't. See commit 771c035372a0 ("deprecate the '__deprecated'
 * attribute warnings entirely and for good") for more information.
 *
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-deprecated-function-attribute
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html#index-deprecated-type-attribute
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Variable-Attributes.html#index-deprecated-variable-attribute
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Enumerator-Attributes.html#index-deprecated-enumerator-attribute
 * clang: https://clang.llvm.org/docs/AttributeReference.html#deprecated
 */
#define __rte_deprecated  __attribute__((deprecated))

/*
 * Optional: not supported by clang
 * Optional: not supported by icc
 *
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html#index-designated_005finit-type-attribute
 */
#if __has_attribute(__designated_init__)
# define __rte_designated_init              __attribute__((__designated_init__))
#else
# define __rte_designated_init
#endif

/*
 * Optional: only supported since clang >= 14.0
 *
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-error-function-attribute
 */
#if __has_attribute(__error__)
# define __rte_compiletime_error(msg)       __attribute__((__error__(msg)))
#else
# define __rte_compiletime_error(msg)
#endif

/*
 * Optional: not supported by clang
 *
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-externally_005fvisible-function-attribute
 */
#if __has_attribute(__externally_visible__)
# define __rte_visible                      __attribute__((__externally_visible__))
#else
# define __rte_visible
#endif

/*
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-format-function-attribute
 * clang: https://clang.llvm.org/docs/AttributeReference.html#format
 */
#define __rte_printf(a, b)                  __attribute__((__format__(printf, a, b)))
#define __rte_scanf(a, b)                   __attribute__((__format__(scanf, a, b)))

/*
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-gnu_005finline-function-attribute
 * clang: https://clang.llvm.org/docs/AttributeReference.html#gnu-inline
 */
#define __rte_gnu_inline                    __attribute__((__gnu_inline__))

/*
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-malloc-function-attribute
 * clang: https://clang.llvm.org/docs/AttributeReference.html#malloc
 */
#define __rte_malloc                        __attribute__((__malloc__))

/*
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html#index-mode-type-attribute
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Variable-Attributes.html#index-mode-variable-attribute
 */
#define __rte_mode(x)                       __attribute__((__mode__(x)))

/*
 * Optional: only supported since gcc >= 7
 *
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/x86-Function-Attributes.html#index-no_005fcaller_005fsaved_005fregisters-function-attribute_002c-x86
 * clang: https://clang.llvm.org/docs/AttributeReference.html#no-caller-saved-registers
 */
#if __has_attribute(__no_caller_saved_registers__)
# define __rte_no_caller_saved_registers	__attribute__((__no_caller_saved_registers__))
#else
# define __rte_no_caller_saved_registers
#endif

/*
 * Optional: not supported by clang
 *
 *  gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-noclone-function-attribute
 */
#if __has_attribute(__noclone__)
# define __rte_noclone                      __attribute__((__noclone__))
#else
# define __rte_noclone
#endif

/*
 * Add the pseudo keyword 'fallthrough' so case statement blocks
 * must end with any of these keywords:
 *   break;
 *   fallthrough;
 *   continue;
 *   goto <label>;
 *   return [expression];
 *
 *  gcc: https://gcc.gnu.org/onlinedocs/gcc/Statement-Attributes.html#Statement-Attributes
 */
#if __has_attribute(__fallthrough__)
# define __rte_fallthrough                    __attribute__((__fallthrough__))
#else
# define __rte_fallthrough                    /* fallthrough */
#endif


/*
 * gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#Common-Function-Attributes
 * clang: https://clang.llvm.org/docs/AttributeReference.html#flatten
 */
# define __rte_flatten			__attribute__((flatten))

/*
 * Note the missing underscores.
 *
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-noinline-function-attribute
 * clang: mentioned
 */
#define __rte_noinline          __attribute__((__noinline__))


/*
 * Optional: only supported since gcc >= 8
 * Optional: not supported by clang
 * Optional: not supported by icc
 *
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Variable-Attributes.html#index-nonstring-variable-attribute
 */
#if __has_attribute(__nonstring__)
# define __rte_nonstring                    __attribute__((__nonstring__))
#else
# define __rte_nonstring
#endif

/*
 * Optional: only supported since GCC >= 7.1, clang >= 13.0.
 *
 *      gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-no_005fprofile_005finstrument_005ffunction-function-attribute
 *    clang: https://clang.llvm.org/docs/AttributeReference.html#no-profile-instrument-function
 */
#if __has_attribute(__no_profile_instrument_function__)
# define __rte_no_profile                  __attribute__((__no_profile_instrument_function__))
#else
# define __rte_no_profile
#endif

/*
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-noreturn-function-attribute
 * clang: https://clang.llvm.org/docs/AttributeReference.html#noreturn
 * clang: https://clang.llvm.org/docs/AttributeReference.html#id1
 */
#define __rte_noreturn                      __attribute__((__noreturn__))

/*
 * Optional: not supported by gcc.
 * Optional: not supported by icc.
 *
 * clang: https://clang.llvm.org/docs/AttributeReference.html#overloadable
 */
#if __has_attribute(__overloadable__)
# define __rte_overloadable			__attribute__((__overloadable__))
#else
# define __rte_overloadable
#endif

/*
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html#index-packed-type-attribute
 * clang: https://gcc.gnu.org/onlinedocs/gcc/Common-Variable-Attributes.html#index-packed-variable-attribute
 */
#define __rte_packed                        __attribute__((__packed__))

/*
 * Note: the "type" argument should match any __builtin_object_size(p, type) usage.
 *
 * Optional: not supported by gcc.
 * Optional: not supported by icc.
 *
 * clang: https://clang.llvm.org/docs/AttributeReference.html#pass-object-size-pass-dynamic-object-size
 */
#if __has_attribute(__pass_object_size__)
# define __pass_object_size(type)	__attribute__((__pass_object_size__(type)))
#else
# define __pass_object_size(type)
#endif

/*
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-pure-function-attribute
 */
#define __rte_pure                          __attribute__((__pure__))


/*
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-section-function-attribute
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Variable-Attributes.html#index-section-variable-attribute
 * clang: https://clang.llvm.org/docs/AttributeReference.html#section-declspec-allocate
 */
#define __rte_section(section)              __attribute__((__section__(section)))

/*
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-unused-function-attribute
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Type-Attributes.html#index-unused-type-attribute
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Variable-Attributes.html#index-unused-variable-attribute
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Label-Attributes.html#index-unused-label-attribute
 * clang: https://clang.llvm.org/docs/AttributeReference.html#maybe-unused-unused
 */
#define __rte_always_unused                 __attribute__((__unused__))
#define __rte_maybe_unused                  __attribute__((__unused__))

/*
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-used-function-attribute
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Variable-Attributes.html#index-used-variable-attribute
 */
#define __rte_used                          __attribute__((__used__))
#define __rte_unused                        __attribute__((__unused__))

/*
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-warn_005funused_005fresult-function-attribute
 * clang: https://clang.llvm.org/docs/AttributeReference.html#nodiscard-warn-unused-result
 */
#define __rte_must_check                    __attribute__((__warn_unused_result__))

/*
 * Optional: only supported since clang >= 14.0
 *
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-warning-function-attribute
 */
#if __has_attribute(__warning__)
# define __rte_compiletime_warning(msg)     __attribute__((__warning__(msg)))
#else
# define __rte_compiletime_warning(msg)
#endif

/*
 * Optional: only supported since clang >= 14.0
 *
 * clang: https://clang.llvm.org/docs/AttributeReference.html#disable-sanitizer-instrumentation
 *
 * disable_sanitizer_instrumentation is not always similar to
 * no_sanitize((<sanitizer-name>)): the latter may still let specific sanitizers
 * insert code into functions to prevent false positives. Unlike that,
 * disable_sanitizer_instrumentation prevents all kinds of instrumentation to
 * functions with the attribute.
 */
#if __has_attribute(disable_sanitizer_instrumentation)
# define __rte_disable_sanitizer_instrumentation \
	 __attribute__((disable_sanitizer_instrumentation))
#else
# define __rte_disable_sanitizer_instrumentation
#endif

/*
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Function-Attributes.html#index-weak-function-attribute
 *   gcc: https://gcc.gnu.org/onlinedocs/gcc/Common-Variable-Attributes.html#index-weak-variable-attribute
 */
#define __rte_weak                          __attribute__((__weak__))

#endif /* BASEWORK_COMPILER_ATTRIBUTES_H_ */
