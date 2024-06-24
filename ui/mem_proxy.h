/*
 *  Copyright 2024 wtcat
 */
#ifndef UI_MEM_PROXY_H_
#define UI_MEM_PROXY_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

struct _mem_node {
	char     *area;
	char     *endptr;
	char     *freeptr;
};

struct mem_proxy {
#ifndef MEM_PROXY_SLOTS
#define MEM_PROXY_SLOTS 5
#endif
	struct _mem_node slots[MEM_PROXY_SLOTS];
	uint16_t         count;
	uint16_t         avalible_idx;

	void *(*alloc)(size_t size);
	void  (*release)(void *p);
};

#define _ALIGNED_UP_ADD(p, size, align) \
	(char *)(((uintptr_t)p + size + align - 1) & ~(align - 1))

static inline void 
mem_proxy_init(struct mem_proxy *proxy, size_t blksize, 
	void *(*alloc)(size_t), 
	void (*release)(void *)) {
	uint16_t i;

	for (i = 0; i < MEM_PROXY_SLOTS; i++) {
		void *area = alloc(blksize);
		if (area == NULL)
			return;

		proxy->slots[i].area    = area;
		proxy->slots[i].endptr  = proxy->slots[i].area + blksize;
		proxy->slots[i].freeptr = _ALIGNED_UP_ADD(area, 0, 64);
	}
	proxy->count        = i;
	proxy->avalible_idx = 0;
	proxy->alloc        = alloc;
	proxy->release      = release;
}

static inline void *
mem_proxy_take(struct mem_proxy *proxy, size_t size) {
	void *p;

	while (proxy->avalible_idx < proxy->count) {
		struct _mem_node *node = &proxy->slots[proxy->avalible_idx];
		if ((long)(node->endptr - node->freeptr) >= size) {
			p = node->freeptr;
			node->freeptr = _ALIGNED_UP_ADD(p, size, 64);
			return p;
		}
		proxy->avalible_idx++;
	}

	return NULL;
}

static inline void 
mem_proxy_deinit(struct mem_proxy* proxy) {
	for (uint16_t i = 0; i < proxy->count; i++) {
		proxy->release(proxy->slots[i].area);
		proxy->slots[i].area    = NULL;
		proxy->slots[i].endptr  = NULL;
		proxy->slots[i].freeptr = NULL;
	}
	proxy->count = proxy->avalible_idx = 0;
}

#ifdef __cplusplus
}
#endif
#endif /* UI_MEM_PROXY_H_ */
