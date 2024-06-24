/*
 * Copyright 2022 wtcat
 */
#ifndef BASEWORK_MALLOC_H_
#define BASEWORK_MALLOC_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

#if defined(__clang__)
# define __malloc_family __attribute((ownership_returns(malloc)))
#else
# define __malloc_family
#endif 

#if defined(_WIN32) || defined(__linux__)
#include <stdlib.h>
#define general_malloc(size)    malloc(size)
#define general_calloc(n, size) calloc(n, size)
#define general_realloc(ptr, size) realloc(ptr, size)
#define general_free(ptr)       free(ptr)
#define general_aligned_alloc(alignment, size) aligned_alloc(alignment, size)

#else /* Not general operation system */


#ifdef CONFIG_MEM_DEBUG
#define general_aligned_alloc(alignment, size) \
    __general_aligned_alloc_debug(alignment, size)
#define general_malloc(size) \
    __general_malloc_debug(size, __func__, __LINE__)
#define general_calloc(n, size) \
    __general_calloc_debug(n, size, __func__, __LINE__)
#define general_realloc(ptr, size) \
    __general_realloc_debug(ptr, size, __func__, __LINE__)
#define general_free(ptr) \
    __general_free(ptr)

#else /* !CONFIG_MEM_DEBUG */
#define general_aligned_alloc(alignment, size) \
    __general_aligned_alloc(alignment, size)
#define general_malloc(size) \
    __general_malloc(size)
#define general_calloc(n, size) \
    __general_calloc(n, size)
#define general_realloc(ptr, size) \
    __general_realloc(ptr, size)
#define general_free(ptr) \
    __general_free(ptr)
#endif /* CONFIG_MEM_DEBUG */

void *__general_aligned_alloc_debug(size_t alignment, size_t size, const char *func, int line);
void *__general_aligned_alloc(size_t alignment, size_t size);
void *__general_malloc_debug(size_t size, const char *func, int line);
void *__malloc_family __general_malloc(size_t size);
void *__general_calloc_debug(size_t n, size_t size, const char *func, int line);
void *__general_calloc(size_t n, size_t size);
void *__general_realloc_debug(void *ptr, size_t size, const char *func, int line);
void *__general_realloc(void *ptr, size_t size);
void  __malloc_family __general_free(void *ptr);

#endif /* _WIN32 || __linux__ */

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_MALLOC_H_ */
