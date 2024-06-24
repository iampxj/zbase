/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_DEBUG_KASAN_H_
#define BASEWORK_DEBUG_KASAN_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

#ifdef CONFIG_ASAN_SHADOW_SECTION
#define ASAN_SHADOW_SECTION CONFIG_ASAN_SHADOW_SECTION
#else
#define ASAN_SHADOW_SECTION ".bss.shadow.mem"
#endif

#ifdef CONFIG_ASAN_REDZONE_SIZE
#define ASAN_REDZONE_SIZE CONFIG_ASAN_REDZONE_SIZE
#else
#define ASAN_REDZONE_SIZE 8
#endif

#ifdef CONFIG_ASAN_MEM_BASE
#define ASAN_MEM_BASE CONFIG_ASAN_MEM_BASE
#else
#define ASAN_MEM_BASE 0
#endif
#ifdef CONFIG_ASAN_MEM_SIZE
#define ASAN_MEM_SIZE CONFIG_ASAN_MEM_SIZE
#else
#define ASAN_MEM_SIZE ASAN_REDZONE_SIZE
#endif


#define ASAN_ALLOCSIZE(size) \
    ((size) + (ASAN_REDZONE_SIZE << 1) + sizeof(size_t))

/*
 * asan_alloc_poison - Poisoning memory that be dynamic allocated
 *
 * @ptr   point to memory address
 * @size  memory size
 * return user memory area
 */
void *asan_alloc_poison(void *ptr, size_t size);

/*
 * asan_alloc_unpoison - Unpoisioning memory
 *
 * @ptr  point to memory address
 * return user memory area
 */
void *asan_alloc_unpoison(void *ptr);

/*
 * asan_init - Initilaize address sanitizer
 *
 * @base point to memory base address
 * @size memory size
 * return 0 if success
 */
int  asan_init(void *base, size_t size);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEBUG_KASAN_H_ */
