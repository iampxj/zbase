/* SPDX-License-Identifier: GPL-2.0 */
#ifndef BASE_MINMAX_H_
#define BASE_MINMAX_H_

#ifndef __RTE_UNIQUE_ID
#define __RTE_UNIQUE_ID(n) n
#endif

#ifndef __rte_is_constexpr
#define __rte_is_constexpr(x) __builtin_constant_p(x)
#endif

/*
 * rte_min()/rte_max()/rte_clamp() macros must accomplish three things:
 *
 * - avoid multiple evaluations of the arguments (so side-effects like
 *   "x++" happen only once) when non-constant.
 * - perform strict type-checking (to generate warnings instead of
 *   nasty runtime surprises). See the "unnecessary" pointer comparison
 *   in __rte_typecheck().
 * - retain result as a constant expressions when called with only
 *   constant expressions (to avoid tripping VLA warnings in stack
 *   allocation usage).
 */
#define __rte_typecheck(x, y) \
	(!!(sizeof((typeof(x) *)1 == (typeof(y) *)1)))

#define __rte_no_side_effects(x, y) \
		(__rte_is_constexpr(x) && __rte_is_constexpr(y))

#define __rte_safe_cmp(x, y) \
		(__rte_typecheck(x, y) && __rte_no_side_effects(x, y))

#define __rte_cmp(x, y, op)	((x) op (y) ? (x) : (y))

#define __rte_cmp_once(x, y, unique_x, unique_y, op) ({	\
		typeof(x) unique_x = (x);		\
		typeof(y) unique_y = (y);		\
		__rte_cmp(unique_x, unique_y, op); })

#define __rte_careful_cmp(x, y, op) \
	__builtin_choose_expr(__rte_safe_cmp(x, y), \
		__rte_cmp(x, y, op), \
		__rte_cmp_once(x, y, __RTE_UNIQUE_ID(__x), __RTE_UNIQUE_ID(__y), op))

/**
 * rte_min - return minimum of two values of the same or compatible types
 * @x: first value
 * @y: second value
 */
#ifndef rte_min
#define rte_min(x, y)	__rte_careful_cmp(x, y, <)
#endif

/**
 * rte_max - return maximum of two values of the same or compatible types
 * @x: first value
 * @y: second value
 */
#ifndef rte_max
#define rte_max(x, y)	__rte_careful_cmp(x, y, >)
#endif

/**
 * rte_min3 - return minimum of three values
 * @x: first value
 * @y: second value
 * @z: third value
 */
#define rte_min3(x, y, z) rte_min((typeof(x))rte_min(x, y), z)

/**
 * rte_max3 - return maximum of three values
 * @x: first value
 * @y: second value
 * @z: third value
 */
#define rte_max3(x, y, z) rte_max((typeof(x))rte_max(x, y), z)

/**
 * rte_min_not_zero - return the minimum that is _not_ zero, unless both are zero
 * @x: value1
 * @y: value2
 */
#define rte_min_not_zero(x, y) ({			\
	typeof(x) __x = (x);			\
	typeof(y) __y = (y);			\
	__x == 0 ? __y : ((__y == 0) ? __x : rte_min(__x, __y)); })

/**
 * rte_clamp - return a value clamped to a given range with strict typechecking
 * @val: current value
 * @lo: lowest allowable value
 * @hi: highest allowable value
 *
 * This macro does strict typechecking of @lo/@hi to make sure they are of the
 * same type as @val.  See the unnecessary pointer comparisons.
 */
#define rte_clamp(val, lo, hi) rte_min((typeof(val))rte_max(val, lo), hi)

/*
 * ..and if you can't take the strict
 * types, you can specify one yourself.
 *
 * Or not use rte_min/rte_max/rte_clamp at all, of course.
 */

/**
 * rte_min_t - return minimum of two values, using the specified type
 * @type: data type to use
 * @x: first value
 * @y: second value
 */
#define rte_min_t(type, x, y)	__rte_careful_cmp((type)(x), (type)(y), <)

/**
 * rte_max_t - return maximum of two values, using the specified type
 * @type: data type to use
 * @x: first value
 * @y: second value
 */
#define rte_max_t(type, x, y)	__rte_careful_cmp((type)(x), (type)(y), >)

/**
 * rte_clamp_t - return a value clamped to a given range using a given type
 * @type: the type of variable to use
 * @val: current value
 * @lo: minimum allowable value
 * @hi: maximum allowable value
 *
 * This macro does no typechecking and uses temporary variables of type
 * @type to make all the comparisons.
 */
#define rte_clamp_t(type, val, lo, hi) rte_min_t(type, rte_max_t(type, val, lo), hi)

/**
 * rte_clamp_val - return a value clamped to a given range using val's type
 * @val: current value
 * @lo: minimum allowable value
 * @hi: maximum allowable value
 *
 * This macro does no typechecking and uses temporary variables of whatever
 * type the input argument @val is.  This is useful when @val is an unsigned
 * type and @lo and @hi are literals that will otherwise be assigned a signed
 * integer type.
 */
#define rte_clamp_val(val, lo, hi) rte_clamp_t(typeof(val), val, lo, hi)

/**
 * rte_swap - rte_swap values of @a and @b
 * @a: first value
 * @b: second value
 */
#define rte_swap(a, b) \
	do { typeof(a) __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

#define rte_swap_t(t, a, b) \
	do { t __tmp = (a); (a) = (b); (b) = __tmp; } while (0)

#endif	/* BASE_MINMAX_H_ */