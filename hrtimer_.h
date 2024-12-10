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

typedef void (*hrtimer_routine_t)(struct hrtimer *);

enum hrtimer_state {
	HRTIMER_SCHEDULED_BLACK,
	HRTIMER_SCHEDULED_RED,
	HRTIMER_INACTIVE,
	HRTIMER_PENDING
};

struct hrtimer_context {
	RBTree_Control tree;

	/*
	 * The scheduled watchdog with the earliest expiration
	 * time or NULL in case no watchdog is scheduled.
	 */
	RBTree_Node *first;

	/* Reload hardware timer to new value */
	uint32_t (*reload)(struct hrtimer_context *header, uint64_t ns);

	uint64_t jiffies;
#ifdef HRTIME_CONTEXT_EXTENSION
	HRTIME_CONTEXT_EXTENSION
#endif
};

#define HRTIMER_CONTEXT_DEFINE(_name, _reload_fn) \
	struct hrtimer_context _name = { \
		.tree = RBTREE_INITIALIZER_EMPTY(_name.tree), \
		.first = NULL, \
		.reload = _reload_fn \
	}

struct hrtimer {
	RBTree_Node node;
	hrtimer_routine_t routine;
	uint64_t expire;
};

static inline struct hrtimer *
_hrtimer_first(const struct hrtimer_context *header) {
	return (struct hrtimer *)header->first;
}

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

#ifdef USE_HRTIMER_SOURCE_CODE
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
	if (new_first == &timer->node)
		header->reload(header, new_first->expire);

	return 0;
}

void _hrtimer_remove(struct hrtimer_context *header, struct hrtimer *timer) {
	if (_hrtimer_pending(timer)) {
		if (header->first == &timer->node)
			_hrtimer_next_first(header, timer);

		_RBTree_Extract(&header->tree, &timer->node);
		_hrtimer_set_state(timer, HRTIMER_INACTIVE);
	}
}

void _hrtimer_expire(struct hrtimer_context *header, struct hrtimer *first, 
	uint64_t now) {
	do {
		if (first->expire <= now) {
			hrtimer_routine_t routine;

			_hrtimer_next_first(header, first);
			_RBTree_Extract(&header->tree, &first->node);
			_hrtimer_set_state(first, HRTIMER_INACTIVE);
			routine = first->routine;
			// _ISR_lock_Release_and_ISR_enable(lock, lock_context);
			(*routine)(first);
			// _ISR_lock_ISR_disable_and_acquire(lock, lock_context);
		} else {
			break;
		}

		first = _hrtimer_first(header);
	} while (first != NULL);
}

#else /* !USE_HRTIMER_SOURCE_CODE */
int _hrtimer_insert(struct hrtimer_context *header, struct hrtimer *timer,
	uint64_t expire);
void _hrtimer_remove(struct hrtimer_context *header, struct hrtimer *timer);
void _hrtimer_expire(struct hrtimer_context *header, struct hrtimer *first, 
	uint64_t now);
#endif /* USE_HRTIMER_SOURCE_CODE */

static inline uint64_t 
_hrtimer_cancel(struct hrtimer_context *header, struct hrtimer *timer, 
	uint64_t now) {
	uint64_t remaining;

	uint64_t expire = timer->expire;
	if (now < expire)
		remaining = expire - now;
	else
		remaining = 0;
	_hrtimer_remove(header, timer);
	return remaining;
}

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_HRTIMER__H_ */
