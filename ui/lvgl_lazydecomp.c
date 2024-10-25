/*
 * Copyright 2024 wtcat
 * 
 * Note: All APIs has no thread-safe
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <string.h>

#include "basework/generic.h"
#include "basework/assert.h"
#include "basework/container/list.h"
#include "basework/ui/lvgl_lazydecomp.h"

#ifdef CONFIG_LAZYCACHE_MAX_NODES
#define MAX_CACHE_NODES CONFIG_LAZYCACHE_MAX_NODES
#else
#define MAX_CACHE_NODES 64
#endif

#ifndef CONFIG_LAZYCACHE_ALIGN_SIZE
#define CONFIG_LAZYCACHE_ALIGN_SIZE 64
#endif

#ifndef __rte_always_inline
#define STATIC_INLINE static inline
#else
#define STATIC_INLINE static __rte_always_inline
#endif

#define CACHE_ALIGNED   CONFIG_LAZYCACHE_ALIGN_SIZE
#define CACHE_LIMIT(n)  (size_t)(((n) * 9) / 10)

#define ALIGNED_UP_ADD(p, size, align) \
	(char *)(((uintptr_t)p + size + align - 1) & ~(align - 1))
#define ALIGNED_UP(v, a) (((v) + ((a) - 1)) & ~((a) - 1))


struct image_node {
	uint32_t         key;	 /* Picture offset address */
	const void      *data;   /* Point to picture data */
	size_t           size;	 /* Picture data size */
	struct rte_list  lru;
	struct rte_hnode node;
};

struct cache_mempool {
	char            *area;
	char            *start;
	char            *end;
	char            *freeptr;
	void            (*release)(void *p);
};

struct aux_mempool {
#define AUX_MEMPOOL_SIZE 10
	struct cache_mempool slots[AUX_MEMPOOL_SIZE];
	uint16_t             count;
	uint16_t             avalible_idx;
};

struct lazy_cache {
#define HASH_LOG 4
#define HASH_SIZE (1 << HASH_LOG)
#define HASH_MASK (HASH_SIZE - 1)
#define KEY_HASH(key) (((uintptr_t)(key) ^ ((uintptr_t)(key) >> 8)) & HASH_MASK)
	struct rte_list   lru;
	struct rte_hlist  slots[HASH_SIZE];
	struct image_node inodes[MAX_CACHE_NODES];
	size_t            cache_limited;
	uint16_t          policy;
	uint16_t          cache_hits;
	uint16_t          cache_misses;
	uint16_t          cache_resets;
	uint16_t          node_misses;
	uint8_t           invalid_pending;
	uint8_t           cache_dirty;
	char             *first_avalible;

	/*
	 * Main memory pool 
	 */
	struct cache_mempool main_mempool;

#ifdef CONFIG_LVGL_LAZYDECOMP_AUXMEM 
	/*
	 * Aux memory pool
	 */
	struct aux_mempool   aux_mempool;
#endif

	/*
	 * Hardware accelarator 
	 */
	void             (*wait_complete)(void *context, int devid);
};

static struct lazy_cache cache_controller;

#ifdef CONFIG_LVGL_LAZYDECOMP_AUXMEM
STATIC_INLINE void aux_mempool_reset(struct aux_mempool *aux) {
	for (uint16_t i = 0; i < aux->count; i++)
		aux->slots[i].freeptr = aux->slots[i].start;
	aux->avalible_idx = 0;
}
#endif

STATIC_INLINE void cache_mempool_init(struct cache_mempool *pool, void *area, 
	size_t size, void (*release)(void *p)) {		 
	pool->area     = area;
	pool->start    = ALIGNED_UP_ADD(area, 0, CACHE_ALIGNED);
	pool->end      = (char *)area + size;
	pool->freeptr  = pool->start;
	pool->release  = release;
}

STATIC_INLINE void cache_mempool_uninit(struct cache_mempool *pool) {
	if (pool->area != NULL && pool->release != NULL) {
		pool->release(pool->area);
		pool->area    = NULL;
		pool->start   = NULL;
		pool->end     = NULL;
		pool->freeptr = NULL;
	}
}

STATIC_INLINE void *alloc_cache_buffer(struct lazy_cache *cache, size_t size, 
	void ***phead) {
	struct cache_mempool *pool = &cache->main_mempool;
	void *p;

	size = ALIGNED_UP(size, CACHE_ALIGNED);
	
	if (pool->end - pool->freeptr >= size) {
		*phead = (void **)&pool->freeptr;
		p = pool->freeptr;
		pool->freeptr = ALIGNED_UP_ADD(p, size, CACHE_ALIGNED);
		return p;
	}

#ifdef CONFIG_LVGL_LAZYDECOMP_AUXMEM
	struct aux_mempool *aux = &cache->aux_mempool;
	while (aux->avalible_idx < aux->count) {
		pool = &aux->slots[aux->avalible_idx];
		if ((long)(pool->end - pool->freeptr) >= size) {
			void *ptr = pool->freeptr;
			*phead = (void **)&pool->freeptr;
			pool->freeptr = ALIGNED_UP_ADD(ptr, size, 64);
			return ptr;
		}

		aux->avalible_idx++;
	}
#endif /* CONFIG_LVGL_LAZYDECOMP_AUXMEM */

	return NULL;
}

STATIC_INLINE struct image_node *alloc_node(struct lazy_cache *cache) {
	char *p = cache->first_avalible;
	if (p != NULL)
		cache->first_avalible = *(char **)p;
	return (struct image_node *)p;
}

static void lazy_cache_reset(struct lazy_cache *cache) {
	if (cache->cache_dirty) {
		cache->first_avalible = NULL;
		cache->cache_dirty    = 0;
		cache->cache_hits     = 0;
		cache->cache_misses   = 0;
		cache->node_misses    = 0;
		cache->cache_resets++;

		cache->main_mempool.freeptr = cache->main_mempool.start;
#ifdef CONFIG_LVGL_LAZYDECOMP_AUXMEM
		aux_mempool_reset(&cache->aux_mempool);
#endif

		char *p = (char *)cache->inodes;
		for (size_t i = 0; i < MAX_CACHE_NODES; i++) {
			*(char **)p = cache->first_avalible;
			cache->first_avalible = p;
			p += sizeof(struct image_node);
		}

		RTE_INIT_LIST(&cache->lru);
		memset(cache->slots, 0, sizeof(cache->slots));
	}
}

static void lazy_cache_add(struct lazy_cache *cache, struct image_node *img, 
	uint32_t key, const void *data) {
	cache->cache_dirty = 1;
	img->key           = key;
	img->data          = data;

	rte_list_add_tail(&img->lru, &cache->lru);
	rte_hlist_add_head(&img->node, &cache->slots[KEY_HASH(key)]);
}

static void *lazy_cache_get(struct lazy_cache *cache, uint32_t key) {
	struct rte_hnode *pos;
	uint32_t offset;

	if (!cache->cache_dirty)
		return NULL;

	offset = KEY_HASH(key);

	rte_hlist_foreach(pos, &cache->slots[offset]) {
		struct image_node *img = rte_container_of(pos, struct image_node, node);
		if (img->key == key) {

			/* 
			 * Move node to the tail of the list when the cache is hit 
			 */
			rte_list_del(&img->lru);
			rte_list_add_tail(&img->lru, &cache->lru);
			cache->cache_hits++;

			return (void *)img->data;
		}
	}

	cache->cache_misses++;
	return NULL;
}

static void cache_internal_init(struct lazy_cache *cache, void *area, size_t size,
								void (*release)(void *p)) {
	cache->policy          = LAZY_CACHE_LRU;
	cache->cache_limited   = CACHE_LIMIT(size);
	cache->cache_dirty     = 1;
	cache->invalid_pending = 0;
	cache->cache_resets    = 0;

	cache_mempool_init(&cache->main_mempool, area, size, release);

#ifdef CONFIG_LVGL_LAZYDECOMP_AUXMEM
	memset(&cache->aux_mempool, 0, sizeof(cache->aux_mempool));
#endif

	cache->wait_complete   = NULL;

	lazy_cache_reset(cache);
}

int lazy_cache_decomp(const lv_img_dsc_t *src, lv_img_dsc_t *imgbuf, 
	void *context, int devid) {
	int err = 0;

	/*
	 * If lvgl image descriptor use lazy decompress 
	 */
	if (src->header.reserved == LV_IMG_LAZYDECOMP_MARKER) {
		struct lazy_cache *cache = &cache_controller;
		const struct lazy_decomp *ld = (struct lazy_decomp *)src->data;
		void **phead;
		void *p;

		rte_assert(ld->decompress != NULL);

		switch (cache->policy) {
		/*
		 * Does not use any caching policy
		 */
		case LAZY_CACHE_NONE:
			/*
			 * Waiting for rendering to complete
			 */
			if (cache->wait_complete)
				cache->wait_complete(context, devid);

			err = ld->decompress(ld, cache->main_mempool.start, src->data_size);
			if (!err) {
				p = cache->main_mempool.start;
				goto _next;
			}
			goto _exit;

		case LAZY_CACHE_LRU:
			if (!cache->invalid_pending) {
				/*
				 * The first, We search image cache
				 */

				p = lazy_cache_get(cache, ld->offset);
				if (p != NULL)
					goto _next;

				/*
				 * Allocate free memory for decompress new image
				 */
				if (src->data_size < cache->cache_limited) {
					struct image_node *img = NULL;
					struct rte_list *victim;

					/*
					 * Try to allocate new buffer for cache block
					 */
					p = alloc_cache_buffer(cache, src->data_size, &phead);
					if (p != NULL) {
						/*
						 * Allocate a cache node 
						 */
						img = alloc_node(cache);
						if (img != NULL) {

							/*
							 * Record memory size 
							 */
							img->size = src->data_size;
							goto _decomp_nowait;
						}

						/*
						 * If there are no free nodes, release the cache block
						 */
						*phead = p;

						/*
						 * Increase node misses count 
						 */
						cache->node_misses++;
					}

					/*
					 * Find victim node from LRU list
					 */
					rte_list_foreach(victim, &cache->lru) {
						img = rte_container_of(victim, struct image_node, lru);

						if (img->size >= src->data_size) {
							/*
							 * If we found a valid cache block then remove it from list
							 */
							rte_list_del(&img->lru);
							rte_hlist_del(&img->node);
							p = (void *)img->data;
							goto _decomp;
						}
					}

					/*
					 * If no cache blocks are found, reset the cache controller
					 */
					p = cache->main_mempool.start;
					cache->invalid_pending = 1;
					img = NULL;

_decomp:
					/*
					 * Waiting for rendering to complete
					 */
					if (cache->wait_complete)
						cache->wait_complete(context, devid);

_decomp_nowait:
					err = ld->decompress(ld, p, src->data_size);
					if (!err) {
						/*
						 * Add image data to cache
						 */
						if (img != NULL)
							lazy_cache_add(cache, img, ld->offset, p);
						goto _next;
					}
				} else {
					/*
					 * If the image size is large than limited value,
					 * we will not cache it and invalid cache controller
					 */
					cache->invalid_pending = 1;
				}
			} else {
				/*
				 * If cache controller has received invalid request, Reset it
				 */
				lazy_cache_reset(cache);
				cache->invalid_pending = 0;
			}

			/*
			 * Waiting for rendering to complete
			 */
			if (cache->wait_complete)
				cache->wait_complete(context, devid);

			p = cache->main_mempool.start;
			err = ld->decompress(ld, p, src->data_size);
			if (!err)
				goto _next;
			goto _exit;

		default:
			goto _exit;
		}

_next:
		*imgbuf = *src;
		imgbuf->data = p;
	} else {
		*imgbuf = *src;
	}

_exit:
	return err;
}

void lazy_cache_set_gpu_waitcb(void (*waitcb)(void *, int)) {
	cache_controller.wait_complete = waitcb;
}

void lazy_cache_invalid(void) {
	cache_controller.invalid_pending = 1;
}

void lazy_cache_set_policy(int policy) {
	lazy_cache_invalid();
	cache_controller.policy = (uint16_t)policy;
}

int lazy_cache_init(void *area, size_t size, void (*release)(void *p)) {
	if (area == NULL || size < 512)
		return -EINVAL;

	cache_mempool_uninit(&cache_controller.main_mempool);
	cache_internal_init(&cache_controller, area, size, release);

	return 0;
}

int lazy_cache_renew(size_t size, void* (*alloc)(size_t)) {
	struct cache_mempool *pool;
	void *area;

	if (alloc == NULL || size < 512)
		return -EINVAL;

	pool = &cache_controller.main_mempool;
	cache_mempool_uninit(pool);

	area = alloc(size);
	if (area == NULL)
		return -ENOMEM;

	cache_internal_init(&cache_controller, area, size, pool->release);

	return 0;
}

#ifdef CONFIG_LVGL_LAZYDECOMP_AUXMEM
void lazy_cache_aux_mempool_init(const struct aux_memarea *areas, size_t n) {
	struct aux_mempool *aux = &cache_controller.aux_mempool;
	size_t i;

	for (i = 0; i < n; i++) {
		struct cache_mempool *p;
		if (areas[i].area == NULL)
			break;

		p = &aux->slots[i];
		p->area = areas[i].area;
		p->start = ALIGNED_UP_ADD(p->area, 0, 64);
		p->end = p->area + areas[i].size - 1;
		p->freeptr = p->start;
		p->release = areas[i].release;
	}

	aux->count = (uint16_t)i;
	aux->avalible_idx = 0;
}

void lazy_cache_aux_mempool_uninit(void) {
	struct aux_mempool *aux = &cache_controller.aux_mempool;
	for (uint16_t i = 0; i < aux->count; i++) {
		struct cache_mempool *p = &aux->slots[i];
		if (p->release)
			p->release(p->area);
	}
	aux->count = 0;
	aux->avalible_idx = 0;

	/*
	 * The cache controller will be reset when aux memory pool
	 * is be free
	 */
	lazy_cache_invalid();
}
#endif /* CONFIG_LVGL_LAZYDECOMP_AUXMEM */

int lazy_cache_get_information(struct lazy_cache_statistics* sta) {
	struct lazy_cache *cache = &cache_controller;

	if (sta == NULL)
		return -EINVAL;

	sta->cache_hits     = cache->cache_hits;
	sta->cache_misses   = cache->cache_misses;
	sta->cache_resets   = cache->cache_resets;
	sta->node_misses    = cache->node_misses;
	sta->main_freespace =
		(uintptr_t)cache->main_mempool.end - (uintptr_t)cache->main_mempool.freeptr;

	return 0;
}
