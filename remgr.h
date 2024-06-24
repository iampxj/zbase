/*
 * Copyright 2023 wtcat
 *
 * Resource manger implement
 */
#ifndef BASEWORK_REMGR_H_
#define BASEWORK_REMGR_H_

#include "basework/container/list.h"

#ifdef __cplusplus
extern "C"{
#endif

struct remgr_base;
typedef void (*remgr_release_t)(struct remgr_base *);
typedef void *(*remgr_alloc_t)(void *, size_t);

/* resource manager node */
struct remgr_base {
    struct rte_list node;
    remgr_release_t release;
};

/* memory resource node */
struct remgr_mem {
    struct remgr_base base;
    size_t size;
    char data[];
};


static inline void __remgr_destroy(struct rte_list *head, 
    bool (*filter)(const struct remgr_base *, const void *key),
    void *key) {
    struct rte_list *pos, *n;
    rte_list_foreach_safe(pos, n, head) {
        struct remgr_base *rb = rte_container_of(pos, struct remgr_base, node);
        if (filter && !filter(rb, key))
            continue;
        rte_list_del(pos);
        if (rb->release)
            rb->release(rb);
    }
}

static inline void remgr_add(struct rte_list *head, struct remgr_base *node, 
    remgr_release_t release) {
    node->release = release;
    rte_list_add_tail(&node->node, head);
}

static inline void remgr_destroy(struct rte_list *head) {
    __remgr_destroy(head, NULL, NULL);
}

static inline void *remgr_malloc(struct rte_list *head,
    remgr_release_t release_fn, remgr_alloc_t alloc,
    size_t size, void *ctx) { 
    struct remgr_mem *r = alloc(ctx, size + sizeof(struct remgr_mem));
    if (r) {
        r->size = size;
        remgr_add(head, &r->base, release_fn);
        return r->data;
    }
    return NULL;
}

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_REMGR_H_ */
