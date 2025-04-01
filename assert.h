/*
 * Copyright 2023 wtcat
 */

#ifndef BASEWORK_ASSERT_H_
#define BASEWORK_ASSERT_H_

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#ifdef __cplusplus
extern "C"{
#endif

/**
 * assert() equivalent, that is always enabled.
 */
#define rte_assert0(cond) do {                                          \
    if (!(cond)) {                                                      \
        __assert_failed(__FILE__, __LINE__, __func__, #cond);           \
    }                                                                   \
} while (0)


/**
 * assert() equivalent, that does not lie in speed critical code.
 * These asserts() thus can be enabled without fearing speed loss.
 */
#ifndef CONFIG_ASSERT_DISABLE
#define rte_assert(cond) rte_assert0(cond)
#else
#define rte_assert(cond) ((void)0)
#endif

# define RTE_ASSERT(_e) rte_assert(_e)

void __assert_failed(const char *file, int line, const char *func, 
    const char *failedexpr);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_ASSERT_H_ */
