/*
 * Copyright (c) 2024 wtcat
 */
#ifndef BASEWORK_HRTIMER__H_
#define BASEWORK_HRTIMER__H_

#include <stdint.h>

#include "basework/assert.h"
#include "basework/container/rb.h"

#ifdef __cplusplus
extern "C" {
#endif

#define HRTIMER_DEBUG_ON

struct hrtimer_context;
struct hrtimer;

typedef void (*hrtimer_routine_t)(struct hrtimer *);

enum hrtimer_state {
	HRTIMER_SCHEDULED_BLACK,
	HRTIMER_SCHEDULED_RED,
	HRTIMER_INACTIVE,
	HRTIMER_PENDING
};

struct hrtimer {
	RBTree_Node node;
	uint64_t expire;
	hrtimer_routine_t routine;
#ifdef HRTIMER_DEBUG_ON
	const char *name;
#endif
};

static inline enum hrtimer_state 
_hrtimer_get_state(const struct hrtimer *timer) {
	return (enum hrtimer_state)RB_COLOR(&timer->node, Node);
}

static inline void 
_hrtimer_set_state(struct hrtimer *timer, enum hrtimer_state state) {
	RB_COLOR(&timer->node, Node) = state;
}

static inline bool 
_hrtimer_pending(const struct hrtimer *timer) {
	return _hrtimer_get_state(timer) < HRTIMER_INACTIVE;
}

#ifdef HRTIMER_SOURCE_CODE
struct hrtimer_context {
	RBTree_Control tree;

	/*
	 * The scheduled watchdog with the earliest expiration
	 * time or NULL in case no watchdog is scheduled.
	 */
	RBTree_Node *first;
};


static inline struct hrtimer *
_hrtimer_first(const struct hrtimer_context *header) {
	return (struct hrtimer *)header->first;
}

static inline void 
_hrtimer_next_first(struct hrtimer_context *header,
	const struct hrtimer *first) {
	RBTree_Node *right;
	rte_assert(header->first == &first->node);
	rte_assert(_RBTree_Left(&first->node) == NULL);
	right = _RBTree_Right(&first->node);

	if (right != NULL) {
		rte_assert(RB_COLOR(right, Node) == RB_RED);
		rte_assert(_RBTree_Left(right) == NULL);
		rte_assert(_RBTree_Right(right) == NULL);
		header->first = right;
	} else {
		header->first = _RBTree_Parent(&first->node);
	}
}

int _hrtimer_insert(struct hrtimer_context *header, struct hrtimer *timer,
	uint64_t expire) {
	RBTree_Node **link;
	RBTree_Node *parent;
	RBTree_Node *old_first;
	RBTree_Node *new_first;

	rte_assert(_hrtimer_get_state(timer) == HRTIMER_INACTIVE);
	link = _RBTree_Root_reference(&header->tree);
	parent = NULL;
	old_first = header->first;
	new_first = &timer->node;
	timer->expire = expire;

	while (*link != NULL) {
		struct hrtimer *parent_timer;
		parent = *link;
		parent_timer = (struct hrtimer *)parent;
		if (expire < parent_timer->expire) {
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

	return new_first == &timer->node;
}

int _hrtimer_remove(struct hrtimer_context *header, struct hrtimer *timer) {
	if (_hrtimer_pending(timer)) {
		if (header->first == &timer->node) {
			_hrtimer_next_first(header, timer);
			return 1;
		}
		_RBTree_Extract(&header->tree, &timer->node);
		_hrtimer_set_state(timer, HRTIMER_INACTIVE);
	}
	return 0;
}

#define _hrtimer_expire(header, first, now, ...) \
	do { \
		if (first->expire <= now) { \
			hrtimer_routine_t routine; \
                                         \
			_hrtimer_next_first(header, first); \
			_RBTree_Extract(&header->tree, &first->node); \
			_hrtimer_set_state(first, HRTIMER_INACTIVE); \
			routine = first->routine; \
			__VA_ARGS__ \
		} else { \
			break; \
		} \
              \
		first = _hrtimer_first(header); \
	} while (first != NULL)

#else

void hrtimer_init(struct hrtimer *timer);
int  hrtimer_stop(struct hrtimer *timer);
int  hrtimer_start(struct hrtimer *timer, uint64_t expire);
#endif /* HRTIMER_SOURCE_CODE */

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_HRTIMER__H_ */
