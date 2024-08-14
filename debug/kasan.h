/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_DEBUG_KASAN_H_
#define BASEWORK_DEBUG_KASAN_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_KASAN
void kasan_poison(const void *addr, size_t size);
void kasan_unpoison(const void *addr, size_t size);
void kasan_register(void *addr, size_t *size);

#else /* CONFIG_KASAN */
#define kasan_poison(addr, size)
#define kasan_unpoison(addr, size)
#define kasan_register(addr, size)
#endif /* CONFIG_MM_KASAN */

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEBUG_KASAN_H_ */
