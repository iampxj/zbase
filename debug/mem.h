/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_DEBUG_MEM_H_
#define BASEWORK_DEBUG_MEM_H_

#include <errno.h>
#include <stdint.h>

#include "basework/container/ahash.h"

#ifdef __cplusplus
extern "C"{
#endif

//
// For example
//
// void *mem_malloc(size_t size) {
// 	struct redzone_header *r;
// 	size_t orgsz = size;

// 	size = REDZONE_ALLOCATE_SIZE(orgsz);
// 	r = mem_pool_malloc(size);
// 	if (r) {
//         redzone_fill(r, orgsz);
//         return redzone_data(r);
//     }
//     return NULL;
// }

// void mem_free(void *ptr)
// {
// 	struct redzone_header *r = to_redzone(ptr);
// 	redzone_head_error(r) {
// 		DBG("XXX has been overwrite\n");
// 		assert(0);
// 	}
// 	redzone_tail_error(r) {
// 		DBG("XXX has been overwrite\n");
// 		assert(0);
// 	}
// 	ptr = r;
// 	mem_pool_free(ptr);
// }

struct redzone_header {
#define REDZONE_HEAD 0xfdfdfdfd
#define REDZONE_TAIL 0xdfdfdfdf
#define REDZONE_SIZE sizeof(uint32_t)
#define REDZONE_ALIGNED_UP(val, align) \
	((val + (align - 1)) & ~(align - 1))
#define REDZONE_ALIGN_SIZE(size) \
	REDZONE_ALIGNED_UP(size, REDZONE_SIZE)
#define REDZONE_ALLOCATE_SIZE(size) \
	REDZONE_ALIGN_SIZE(size) + sizeof(struct redzone_header) + REDZONE_SIZE;

#define _redzone_end(r) *(uint32_t *)((r)->data + (r)->size)
#define to_redzone(_ptr) ((struct redzone_header *)(_ptr) - 1)
#define redzone_head_error(r) ((r)->marker != REDZONE_HEAD)
#define redzone_tail_error(r) (_redzone_end(r) != REDZONE_TAIL)  

#define redzone_data(r) (r)->data
#define redzone_fill(r, _size)                                                           \
	do {                                                                                 \
		(r)->marker = REDZONE_HEAD;                                                      \
		(r)->size = REDZONE_ALIGN_SIZE(_size);                                           \
		_redzone_end(r) = REDZONE_TAIL;                                                  \
	} while (0)

	uint32_t marker;
	uint32_t size;
	char     data[];
};

#define REDZONE_TRACER_DEFINE(name, elem_num, logsize, struct_type) \
    static AHASH_BUF_DEFINE(name##_mem, elem_num, logsize, sizeof(struct_type)); \
    static struct hash_header name; \
                                        \
    static inline int name##_init(void) { \
        return ahash_init(&name, name##_mem, sizeof(name##_mem), \
            sizeof(struct_type), logsize); \
    } \
    static inline struct_type *name##_find(const void *key) { \
        return (struct_type *)ahash_find(&name, (void *)key); \
    } \
    static inline int name##_add(const void *key, struct_type **node) { \
        if (name##_find(key)) \
            return -EEXIST; \
        return ahash_add(&name, key, (struct hash_node **)node); \
    } \
    static inline void name##_del(const void *key) { \
        struct_type *node = (struct_type *)ahash_find(&name, (void *)key); \
        if (node) \
            ahash_del(&name, (struct hash_node *)node); \
    } \
    static inline void name##_foreach( \
        bool (*visitor)(struct hash_node *, void *), \
        void *arg) { \
        ahash_visit(&name, visitor, arg); \
    }

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEBUG_MEM_H_ */
