/*
 * Copyright 2022 wtcat
 */
#ifndef BASE_GENERIC_H_
#define BASE_GENERIC_H_

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#ifndef _MSC_VER
#include "basework/compiler.h"
#endif

#if __cplusplus >= 201103L
# define STATIC_ASSERT( _cond, _msg ) static_assert( _cond, _msg )
#elif __STDC_VERSION__ >= 201112L
  #define STATIC_ASSERT( _cond, _msg ) _Static_assert( _cond, _msg )
#else
# define STATIC_ASSERT( _cond, _msg ) \
    struct _STATIC_ASSERT_NAME(__COUNTER__)  \
      { int _static_assert ## _msg : ( _cond ) ? 1 : -1; }
# define _STATIC_ASSERT_NAME(_name) RTE_JOIN(_static_assert, _name)
#endif

#if UINTPTR_MAX  == UINT64_MAX
#define RTE_CPU_64 1
#else
#define RTE_CPU_32 1
#endif

#if defined(__clang__) || defined(__GNUC__)
# define RTE_RET_IP __builtin_return_address(0)
#else
# define RTE_RET_IP (void *)(0)
#endif

#define _RTE_CACHE_GUARD_HELPER2(unique) \
	char cache_guard_ ## unique[RTE_CACHE_LINE_SIZE] __rte_cache_aligned
#define _RTE_CACHE_GUARD_HELPER1(unique) _RTE_CACHE_GUARD_HELPER2(unique)
/**
 * Empty cache lines, to guard against false sharing-like effects
 * on systems with a next-N-lines hardware prefetcher.
 *
 * Use as spacing between data accessed by different lcores,
 * to prevent cache thrashing on hardware with speculative prefetching.
 */
#define RTE_CACHE_GUARD _RTE_CACHE_GUARD_HELPER1(__COUNTER__)


#define RTE_TOSTRING(s) #s

#define RTE_JOIN(a, b)  _RTE_JOIN(a, b)
#define _RTE_JOIN(a, b) a ## b


/*
 * Check for macro definition in compiler-visible expressions
 *
 *     if (RTE_IS_ENABLED(CONFIG_FOO)) {
 *             do_something_with_foo
 *     }
 */
#define RTE_IS_ENABLED(config_macro) RTE_IS_ENABLED1(config_macro)

/* This is called from IS_ENABLED(), and sticks on a "_XXXX" prefix,
 * it will now be "_XXXX1" if config_macro is "1", or just "_XXXX" if it's
 * undefined.
 *   ENABLED:   RTE_IS_ENABLED2(_XXXX1)
 *   DISABLED   RTE_IS_ENABLED2(_XXXX)
 */
#define RTE_IS_ENABLED1(config_macro) RTE_IS_ENABLED2(_XXXX##config_macro)

/* Here's the core trick, we map "_XXXX1" to "_YYYY," (i.e. a string
 * with a trailing comma), so it has the effect of making this a
 * two-argument tuple to the preprocessor only in the case where the
 * value is defined to "1"
 *   ENABLED:    _YYYY,    <--- note comma!
 *   DISABLED:   _XXXX
 */
#define _XXXX1 _YYYY,

/* Then we append an extra argument to fool the gcc preprocessor into
 * accepting it as a varargs macro.
 *                         arg1   arg2  arg3
 *   ENABLED:   RTE_IS_ENABLED3(_YYYY,    1,    0)
 *   DISABLED   RTE_IS_ENABLED3(_XXXX 1,  0)
 */
#define RTE_IS_ENABLED2(one_or_two_args) RTE_IS_ENABLED3(one_or_two_args 1, 0)

/* And our second argument is thus now cooked to be 1 in the case
 * where the value is defined to 1, and 0 if not:
 */
#define RTE_IS_ENABLED3(ignore_this, val, ...) val


/**
 * add a byte-value offset to a pointer
 */
#define RTE_PTR_ADD(ptr, x) ((void*)((uintptr_t)(ptr) + (x)))

/**
 * subtract a byte-value offset from a pointer
 */
#define RTE_PTR_SUB(ptr, x) ((void *)((uintptr_t)(ptr) - (x)))

/**
 * get the difference between two pointer values, i.e. how far apart
 * in bytes are the locations they point two. It is assumed that
 * ptr1 is greater than ptr2.
 */
#define RTE_PTR_DIFF(ptr1, ptr2) ((uintptr_t)(ptr1) - (uintptr_t)(ptr2))

/**
 * Workaround to cast a const field of a structure to non-const type.
 */
#define RTE_CAST_FIELD(var, field, type) \
	(*(type *)((uintptr_t)(var) + offsetof(typeof(*(var)), field)))
  
/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no higher than the first parameter. Second parameter
 * must be a power-of-two value.
 */
#define RTE_PTR_ALIGN_FLOOR(ptr, align) \
	((typeof(ptr))RTE_ALIGN_FLOOR((uintptr_t)(ptr), align))

/**
 * Macro to align a value to a given power-of-two. The resultant value
 * will be of the same type as the first parameter, and will be no
 * bigger than the first parameter. Second parameter must be a
 * power-of-two value.
 */
#define RTE_ALIGN_FLOOR(val, align) \
	(typeof(val))((val) & (~((typeof(val))((align) - 1))))

/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 */
#define RTE_PTR_ALIGN_CEIL(ptr, align) \
	RTE_PTR_ALIGN_FLOOR((typeof(ptr))RTE_PTR_ADD(ptr, (align) - 1), align)

/**
 * Macro to align a value to a given power-of-two. The resultant value
 * will be of the same type as the first parameter, and will be no lower
 * than the first parameter. Second parameter must be a power-of-two
 * value.
 */
#define RTE_ALIGN_CEIL(val, align) \
	RTE_ALIGN_FLOOR(((val) + ((typeof(val)) (align) - 1)), align)

/**
 * Macro to align a pointer to a given power-of-two. The resultant
 * pointer will be a pointer of the same type as the first parameter, and
 * point to an address no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 * This function is the same as RTE_PTR_ALIGN_CEIL
 */
#define RTE_PTR_ALIGN(ptr, align) RTE_PTR_ALIGN_CEIL(ptr, align)

/**
 * Macro to align a value to a given power-of-two. The resultant
 * value will be of the same type as the first parameter, and
 * will be no lower than the first parameter. Second parameter
 * must be a power-of-two value.
 * This function is the same as RTE_ALIGN_CEIL
 */
#define RTE_ALIGN(val, align) RTE_ALIGN_CEIL(val, align)

/**
 * Macro to align a value to the multiple of given value. The resultant
 * value will be of the same type as the first parameter and will be no lower
 * than the first parameter.
 */
#define RTE_ALIGN_MUL_CEIL(v, mul) \
	((((v) + (typeof(v))(mul) - 1) / ((typeof(v))(mul))) * (typeof(v))(mul))

/**
 * Macro to align a value to the multiple of given value. The resultant
 * value will be of the same type as the first parameter and will be no higher
 * than the first parameter.
 */
#define RTE_ALIGN_MUL_FLOOR(v, mul) \
	(((v) / ((typeof(v))(mul))) * (typeof(v))(mul))

/**
 * Macro to align value to the nearest multiple of the given value.
 * The resultant value might be greater than or less than the first parameter
 * whichever difference is the lowest.
 */
#define RTE_ALIGN_MUL_NEAR(v, mul)				\
	({							\
		typeof(v) ceil = RTE_ALIGN_MUL_CEIL(v, mul);	\
		typeof(v) floor = RTE_ALIGN_MUL_FLOOR(v, mul);	\
		(ceil - (v)) > ((v) - floor) ? floor : ceil;	\
	})

/**
 * Triggers an error at compilation time if the condition is true.
 */
#define RTE_BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

#define RTE_FOR16_EXECUTE(_n,  _exec_cmd) \
do {                                      \
	long __n = (long)_n;                  \
    long __nc = rte_div_roundup(__n, 16); \
    switch ((__n) & 15) {                 \
    case 0: do {    _exec_cmd             \
    case 15:        _exec_cmd             \
    case 14:        _exec_cmd             \
    case 13:        _exec_cmd             \
    case 12:        _exec_cmd             \
    case 11:        _exec_cmd             \
    case 10:        _exec_cmd             \
    case 9:         _exec_cmd             \
    case 8:         _exec_cmd             \
    case 7:         _exec_cmd             \
    case 6:         _exec_cmd             \
    case 5:         _exec_cmd             \
    case 4:         _exec_cmd             \
    case 3:         _exec_cmd             \
    case 2:         _exec_cmd             \
    case 1:         _exec_cmd             \
            } while (--__nc > 0);         \
    }                                     \
} while (0)

#define rte_array_size(a) (sizeof(a) / sizeof((a)[0]))
#define rte_div_roundup(x, y)  (((x) + ((y) - 1)) / (y))
#define rte_roundup(x, y)  ((((x) + ((y) - 1)) / (y)) * (y))
#define rte_powerof2(x)    ((((x) - 1) & (x)) == 0)

/**
 * add a byte-value offset to a pointer
 */
#define rte_ptr_add(ptr, x) ((void*)((uintptr_t)(ptr) + (x)))

/**
 * subtract a byte-value offset from a pointer
 */
#define rte_ptr_sub(ptr, x) ((void *)((uintptr_t)(ptr) - (x)))

/**
 * get the difference between two pointer values, i.e. how far apart
 * in bytes are the locations they point two. It is assumed that
 * ptr1 is greater than ptr2.
 */
#define rte_ptr_diff(ptr1, ptr2) ((uintptr_t)(ptr1) - (uintptr_t)(ptr2))

/**
 * Get the size of a field in a structure.
 *
 * @param type
 *   The type of the structure.
 * @param field
 *   The field in the structure.
 * @return
 *   The size of the field in the structure, in bytes.
 */
#define rte_sizeof_field(type, field) (sizeof(((type *)0)->field))

/*
 * Note: DISABLE_BRANCH_PROFILING can be used by special lowlevel code
 * to disable branch tracing on a per file basis.
 */
#define rte_container_of(_m, _type, _member) \
	( (_type *) ( (uintptr_t) ( _m ) - offsetof( _type, _member ) ) )


#define rte_sysinit(func, prio) \
  static void __attribute__((constructor(prio), used)) func(void)


#ifndef __cplusplus
#include "basework/minmax.h"
#endif
#endif /* BASE_GENERIC_H_ */
