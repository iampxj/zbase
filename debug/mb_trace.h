/*
 * Copyright 2022 wtcat
 * Copyright 2023 wtcat (Modified)
 */
#ifndef DEBUG_MB_TRACE_H_
#define DEBUG_MB_TRACE_H_

#include <assert.h>
#include <string.h>

#include "basework/errno.h"
#include "basework/container/list.h"

#ifdef __cplusplus
extern "C"{
#endif

#ifndef TRACER_LOCK
#define TRACER_LOCK
#endif

#ifndef TRACER_UNLOCK
#define TRACER_UNLOCK
#endif

#ifndef TRACER_EXT_MEMBERS
#define TRACER_EXT_MEMBERS
#endif

#ifndef MTRACE_LOG_SIZE
#define MTRACE_LOG_SIZE   8
#endif


struct mb_info {
#define MB_TRACER_MARK 0xdeafbcea
    uint32_t magic;
    uint32_t size;
    size_t maxitems;
};

struct mb_node {
    struct rte_hnode node;
    const void *key;
    TRACER_EXT_MEMBERS
};

struct mb_tracer {
#define MTRACE_HASHSIZE   (1 << MTRACE_LOG_SIZE)
#define MTRACE_MASK  (MTRACE_HASHSIZE - 1)
#define MTRACE_HASH(addr) \
    (((uintptr_t)(addr) ^ ((uintptr_t)(addr) >> MTRACE_LOG_SIZE)) & MTRACE_MASK)

    struct rte_hlist slots[MTRACE_HASHSIZE];
    void (*free)(void *);
    char *freelist;
    size_t size;
    struct mb_node freends[];
};

#define TRACER_ADD(_tracer, _ptr, _cmd_exec) \
do { \
    TRACER_LOCK \
    struct mb_node *tnode; \
    int __err = _mb_tracer_add(_tracer, _ptr, &tnode); \
    if (!__err) {\
        _cmd_exec; \
    } \
    TRACER_UNLOCK \
} while (0)

#define TRACER_DEL(_tracer, _ptr, _cmd_exec) \
do {\
    TRACER_LOCK \
    struct mb_node *tnode = _mb_tracer_find(_tracer, _ptr); \
    if (tnode) { \
        _cmd_exec; \
        _mb_tracer_del(_tracer, tnode); \
    } \
    TRACER_UNLOCK \
} while (0)

#define TRACER_DUMP(_tracer, _visitor, _arg) \
do { \
    TRACER_LOCK \
    _mb_tracer_visit(_tracer, _visitor, _arg); \
    TRACER_UNLOCK \
} while (0)

#define TRACER_FIND(_tracer, _key) \
({\
		struct mb_node *__tnode; \
    TRACER_LOCK \
    __tnode = _mb_tracer_find(_tracer, _key); \
    TRACER_UNLOCK \
		__tnode; \
})

#define TRACER_SIZE(_maxitems) \
    (((_maxitems) * sizeof(struct mb_node)) + sizeof(struct mb_tracer))

#define TRACER_MEM_DEFINE(_tracer, _maxitems) \
    union _tracer_##_tracer { \
        char _tracer[TRACER_SIZE(_maxitems)] __rte_aligned(4);\
        struct mb_info info; \
    } _tracer[1] = {\
        { \
            .info = {\
                .magic = MB_TRACER_MARK, \
                .size = TRACER_SIZE(_maxitems), \
                .maxitems = _maxitems \
            } \
        } \
    }

/*
 * mb_node_alloc - allocate a new <struct mb_node>
 * @tracer: tracer object
 */
static inline struct mb_node *
__mb_node_alloc(struct mb_tracer *tracer) {
    char *newnode = tracer->freelist;
    if (newnode) 
        tracer->freelist = *(char **)newnode;
    assert(newnode != NULL);
    return (struct mb_node *)newnode;
}

/*
 * mb_node_alloc - free node<struct mb_node>
 * @tracer: tracer object
 * @node: record node
 */
static inline void 
__mb_node_free(struct mb_tracer *tracer, struct mb_node *node) {
    char *p = (char *)node;
    *(char **)p = tracer->freelist;
    tracer->freelist = p;
}

/*
 * mb_tracer_add - add a node to tracer that record memory informations
 * @tracer: tracer object
 * @key: memory address
 * @pnode: pointer to trace-node
 *
 * return 0 if success
 */
static inline int 
_mb_tracer_add(struct mb_tracer *tracer, const void *key,
    struct mb_node **pnode) {
    struct mb_node *newn;
    newn = __mb_node_alloc(tracer);
    if (newn) {
        int ofs = MTRACE_HASH(key);
        newn->key = key;
        rte_hlist_add_head(&newn->node, &tracer->slots[ofs]);
        if (pnode)
            *pnode = newn;
        return 0;
    }
    return -ENOMEM;
}

/*
 * _mb_tracer_find - find mb_node based on a given key
 * @tracer: tracer object
 * @key: memory address
 * return mb_node if success
 */
static inline struct mb_node *
_mb_tracer_find(struct mb_tracer *tracer, void *key) {
    struct rte_hlist *head = &tracer->slots[MTRACE_HASH(key)];
    for (struct rte_hnode *pos = head->first; 
        pos; pos = pos->next) {
        struct mb_node *p = rte_container_of(pos, struct mb_node, node);
        if (p->key == key)
            return p;
    }
    return NULL;
}

/*
 * mb_tracer_del - delete the node associated with ptr
 * @tracer: tracer object
 * @ptr: pointer to mb_node
 */
static inline void 
_mb_tracer_del(struct mb_tracer *tracer, struct mb_node *p) {
    rte_hlist_del(&p->node);
    __mb_node_free(tracer, p);
}

/*
 * mb_tracer_visit - iterate tracer class
 * @tracer: tracer object
 * @visitor: iterator callback
 */
static inline void 
_mb_tracer_visit(struct mb_tracer *tracer, 
    bool (*visitor)(struct mb_node *, void *), void *arg) {
    struct rte_hnode *pos;
    if (!tracer || !visitor)
        return;
    for (int i = 0; i < (int)ARRAY_SIZE(tracer->slots); i++) {
        rte_hlist_foreach(pos, &tracer->slots[i]) {
            struct mb_node *p = rte_container_of(pos, struct mb_node, node);
            if (!visitor(p, arg))
                break;
        }
    }
}

static inline struct mb_tracer *
mb_tracer_init(void *buffer) {
    struct mb_info *pinfo = (struct mb_info *)buffer;
    struct mb_tracer *tracer;
    size_t maxitems;
    char *ptr;

    if (pinfo->magic != MB_TRACER_MARK)
        return NULL;

    maxitems = pinfo->maxitems;
    tracer = buffer;
    memset(tracer, 0, pinfo->size);
    ptr = (char *)tracer->freends;
    for (size_t i = 0; i < maxitems; i++) {
        *(char **)ptr = tracer->freelist;
        tracer->freelist = ptr;
        ptr += sizeof(struct mb_node);
    }
    tracer->size = maxitems;
    return tracer;
}

static inline struct mb_tracer *
mb_tracer_create(size_t maxitems, void *(*alloc)(size_t), 
    void (*mfree)(void *)) {
    struct mb_tracer *tracer;
    size_t alloc_size;
    char *ptr;

    if (!maxitems || !alloc || !mfree) {
        errno = -EINVAL;
        return NULL;
    }
    alloc_size = sizeof(*tracer) + sizeof(struct mb_node) * maxitems;
    tracer = alloc(alloc_size);
    if (!tracer) {
        errno = -ENOMEM;
        return NULL;
    }

    /* Initialize free-node list */
    memset(tracer, 0, alloc_size);
    ptr = (char *)tracer->freends;
    for (size_t i = 0; i < maxitems; i++) {
        *(char **)ptr = tracer->freelist;
        tracer->freelist = ptr;
        ptr += sizeof(struct mb_node);
    }
    tracer->free = mfree;
    tracer->size = maxitems;
    return tracer;
}

static inline void 
mb_tracer_destory(struct mb_tracer *tracer) {
    if (tracer) 
        tracer->free(tracer);
}

#ifdef __cplusplus
}
#endif
#endif /* DEBUG_MB_TRACE_H_ */
