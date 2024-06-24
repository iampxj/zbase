/*
 * Copyright 2024 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include "basework/dev/rte_timer.h"

void rte_timer_insert(struct rte_timer_header *header,
    struct rte_timer *timer, uint64_t expire) {
    RBTree_Node **link;
    RBTree_Node  *parent;
    RBTree_Node  *old_first;
    RBTree_Node  *new_first;

    rte_assert(rte_timer_get_state(timer) == RTE_TIMER_INACTIVE);

    link = _RBTree_Root_reference(&header->tree);
    parent = NULL;
    old_first = header->first;
    new_first = &timer->node;

    timer->expire = expire;

    while ( *link != NULL ) {
        struct rte_timer *parent_timer;

        parent = *link;
        parent_timer = (struct rte_timer *)parent;
        if ( expire < parent_timer->expire ) {
            link = _RBTree_Left_reference(parent);
        } else {
            link = _RBTree_Right_reference(parent);
            new_first = old_first;
        }
    }

    header->first = new_first;
    _RBTree_Initialize_node(&timer->node);
    _RBTree_Add_child(&timer->node, parent, link);
    _RBTree_Insert_color(&header->tree, &timer->node);
}

void rte_timer_remove(struct rte_timer_header *header,
    struct rte_timer *timer) {
    if (rte_timer_is_scheduled(timer)) {
        if (header->first == &timer->node)
            rte_timer_next_first(header, timer);

        _RBTree_Extract(&header->tree, &timer->node);
        rte_timer_set_state(timer, RTE_TIMER_INACTIVE);
    }
}
