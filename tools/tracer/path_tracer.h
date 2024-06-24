/*
 * Copyright 2023 wtcat 
 */
#ifndef PATH_TRACER_H_
#define PATH_TRACER_H_

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef void (*memcheck_hook_t)(const char *dpath, void *ptr, size_t size);

void mempath_init(int min_limit, int max_limit, const char* sep);
void mempath_add_heap(const char* heap, void* addr, size_t size,
    void (*print)(void *fout, const void *));
bool mempath_add(void* addr, size_t size, void *user);
bool mempath_del(void* addr);
size_t mempath_getsize(void* addr);
void mempath_dump_std(void);
void mempath_dump_file(const char* filename);
void mempath_set_memcheck_hook(const memcheck_hook_t hook);

void uiview_heap_user_print(void *fout, const void *user);

#ifdef __cplusplus
}
#endif
#endif /* PATH_TRACER_H_ */
