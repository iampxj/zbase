/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Prevent the compiler from merging or refetching reads or writes. The
 * compiler is also forbidden from reordering successive instances of
 * READ_ONCE and WRITE_ONCE, but only when the compiler is aware of some
 * particular ordering. One way to make the compiler aware of ordering is to
 * put the two invocations of READ_ONCE or WRITE_ONCE in different C
 * statements.
 *
 * These two macros will also work on aggregate data types like structs or
 * unions.
 *
 * Their two major use cases are: (1) Mediating communication between
 * process-level code and irq/NMI handlers, all running on the same CPU,
 * and (2) Ensuring that the compiler does not fold, spindle, or otherwise
 * mutilate accesses that either do not require ordering or that interact
 * with an explicit memory barrier or atomic instruction that provides the
 * required ordering.
 */
#ifndef BASEWORK_GENERIC_RWONCE_H_
#define BASEWORK_GENERIC_RWONCE_H_

#ifndef __ASSEMBLY__

#include "basework/compiler_types.h"
// #include <linux/kasan-checks.h>
// #include <linux/kcsan-checks.h>

/*
 * Yes, this permits 64-bit accesses on 32-bit architectures. These will
 * actually be atomic in some cases (namely Armv7 + LPAE), but for others we
 * rely on the access being split into 2x32-bit accesses for a 32-bit quantity
 * (e.g. a virtual address) and a strong prevailing wind.
 */
#define rte_compiletime_assert_rwonce_type(t)					\
	rte_compiletime_assert(__rte_native_word(t) || sizeof(t) == sizeof(long long),	\
		"Unsupported access size for {READ,WRITE}_ONCE().")

/*
 * Use __READ_ONCE() instead of READ_ONCE() if you do not require any
 * atomicity. Note that this may result in tears!
 */
#ifndef __RTE_READ_ONCE
#define __RTE_READ_ONCE(x)	(*(const volatile __rte_unqual_scalar_typeof(x) *)&(x))
#endif

#define RTE_READ_ONCE(x)							\
({									\
	rte_compiletime_assert_rwonce_type(x);				\
	__RTE_READ_ONCE(x);							\
})

#define __RTE_WRITE_ONCE(x, val)						\
do {									\
	*(volatile typeof(x) *)&(x) = (val);				\
} while (0)

#define RTE_WRITE_ONCE(x, val)						\
do {									\
	rte_compiletime_assert_rwonce_type(x);				\
	__RTE_WRITE_ONCE(x, val);						\
} while (0)

#endif /* __ASSEMBLY__ */
#endif	/* BASEWORK_GENERIC_RWONCE_H_ */
