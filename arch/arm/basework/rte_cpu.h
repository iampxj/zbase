/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_RTE_CPU_ARM_H_
#define BASEWORK_RTE_CPU_ARM_H_

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#ifndef RTE_CACHE_LINE_SIZE
#ifdef CONFIG_ARM64
#define RTE_CACHE_LINE_SIZE 64
#else
#define RTE_CACHE_LINE_SIZE 32
#endif
#endif /* RTE_CACHE_LINE_SIZE */

#endif /* BASEWORK_RTE_CPU_ARM_H_ */
