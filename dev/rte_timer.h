/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_DEV_RTE_TIMER_H_
#define BASEWORK_DEV_RTE_TIMER_H_

#include <stdbool.h>

#include "basework/compiler.h"
#include "basework/container/rb.h"
#include "basework/assert.h"

#ifdef __cplusplus
extern "C"{
#endif

enum rte_timer_state {
    RTE_TIMER_SCHEDULED_BLACK,
    RTE_TIMER_SCHEDULED_RED,
    RTE_TIMER_INACTIVE,
};

struct rte_timer_header {
    RBTree_Control tree;
    RBTree_Node   *first;
};

struct rte_timer {
    RBTree_Node node;
    uint64_t    expire;
    void        (*fn)(struct rte_timer *timer, void *arg);
    void        *arg;
};

#ifndef RTE_TIMER_LOCK_CONTEXT
#define RTE_TIMER_LOCK_CONTEXT(...)   (void)0
#endif

#ifndef RTE_TIMER_UNLOCK_CONTEXT
#define RTE_TIMER_UNLOCK_CONTEXT(...) (void)0
#endif

#ifndef rte_unlikely
#define rte_unlikely(x) x
#endif

static inline void
rte_timer_set_state(struct rte_timer *timer, enum rte_timer_state state) {
    RB_COLOR(&timer->node, Node) = state;
}

static inline int 
rte_timer_get_state(const struct rte_timer *timer) {
    return RB_COLOR(&timer->node, Node);
}

static inline bool 
rte_timer_is_scheduled(const struct rte_timer *timer) {
    return rte_timer_get_state(timer) < RTE_TIMER_INACTIVE;
}

static inline struct rte_timer *
rte_timer_first(struct rte_timer_header *header) {
    return (struct rte_timer *)header->first;
}

static inline void
rte_timer_next_first(struct rte_timer_header *header, struct rte_timer *first) {
    RBTree_Node *right;

   /*
    * This function uses the following properties of red-black trees:
    *
    * 1. Every leaf (NULL) is black.
    *
    * 2. If a node is red, then both its children are black.
    *
    * 3. Every simple path from a node to a descendant leaf contains the same
    *    number of black nodes.
    *
    * The first node has no left child.  So every path from the first node has
    * exactly one black node (including leafs).  The first node cannot have a
    * non-leaf black right child.  It may have a red right child.  In this case
    * both children must be leafs.
    */
    rte_assert(header->first == &first->node);
    rte_assert(_RBTree_Left( &first->node) == NULL);
    right = _RBTree_Right( &first->node);

    if ( right != NULL ) {
        rte_assert(RB_COLOR(right, Node) == RB_RED);
        rte_assert(_RBTree_Left(right) == NULL);
        rte_assert(_RBTree_Right(right) == NULL);
        header->first = right;
    } else {
        header->first = _RBTree_Parent(&first->node);
    }
}

static inline void
rte_timer_init(struct rte_timer *timer, void (*fn)(struct rte_timer *, void *), 
    void *arg) {
    rte_assert(fn != NULL);
    rte_timer_set_state(timer, RTE_TIMER_INACTIVE);
    timer->fn  = fn;
    timer->arg = arg;
}

#define RTE_TIMER_PROCESS(header, now, ...) \
do {  \
    struct rte_timer *_first = rte_timer_first((header));    \
    if (rte_unlikely(_first == NULL))                        \
        return;                                              \
    do {                                                     \
        if ( _first->expire <= (now) ) {                     \
            void (*fn)(struct rte_timer *timer, void *arg);  \
                                                             \
            rte_timer_next_first((header), _first);          \
            _RBTree_Extract(&(header)->tree, &_first->node); \
            rte_timer_set_state(_first, RTE_TIMER_INACTIVE); \
            fn = _first->fn;                                 \
                                                             \
            RTE_TIMER_UNLOCK_CONTEXT(__VA_ARGS__);           \
            fn(_first, _first->arg);                         \
            RTE_TIMER_LOCK_CONTEXT(__VA_ARGS__);             \
        } else {                                             \
            break;                                           \
        }                                                    \
                                                             \
        _first = rte_timer_first(header);                    \
    } while (_first != NULL);                                \
} while (0)

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEV_RTE_TIMER_H_ */
