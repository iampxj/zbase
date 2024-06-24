/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#ifndef BASEWORK_RTE_PAUSE_ARM_H_
#define BASEWORK_RTE_PAUSE_ARM_H_

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#ifndef CONFIG_C11_MEM_MODEL
#include "basework/arch/generic/rte_pause.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

static inline void rte_pause(void)
{
}

#ifdef __cplusplus
}
#endif

#endif /* BASEWORK_RTE_PAUSE_ARM_H_ */
