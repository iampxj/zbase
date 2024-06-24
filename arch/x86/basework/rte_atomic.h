/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015 RehiveTech. All rights reserved.
 */

#ifndef BASEWORK_RTE_ATOMIC_ARM_H_
#define BASEWORK_RTE_ATOMIC_ARM_H_

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define RTE_FORCE_INTRINSICS

#ifndef RTE_FORCE_INTRINSICS
#  error Platform must be built with RTE_FORCE_INTRINSICS
#endif

#ifdef __cplusplus
extern "C" {
#endif

#include "basework/arch/generic/rte_atomic.h"

#define	rte_mb()  __rte_atomic_thread_fence(rte_memory_order_seq_cst)

#define	rte_wmb() __rte_atomic_thread_fence(rte_memory_order_seq_cst)

#define	rte_rmb() __rte_atomic_thread_fence(rte_memory_order_seq_cst)

#define rte_io_mb() rte_mb()

#define rte_io_wmb() rte_wmb()

#define rte_io_rmb() rte_rmb()

static __rte_always_inline void
rte_atomic_thread_fence(rte_memory_order memorder)
{
	__rte_atomic_thread_fence(memorder);
}

#ifdef __cplusplus
}
#endif

#endif /* BASEWORK_RTE_ATOMIC_ARM_H_ */
