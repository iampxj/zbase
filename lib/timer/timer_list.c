/*
 * CopyRight 2022 wtcat
 *
 * The software timer implemention that base on double-list
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <errno.h>
#include <stdbool.h>

#include "basework/os/osapi.h"
#include "basework/lib/timer/timer_list.h"
#include "basework/log.h"

#define TIMER_LOCK_DECLARE os_critical_declare
#define TIMER_LOCK()       os_critical_lock
#define TIMER_UNLOCK()     os_critical_unlock
#define TIMER_LIST(ptr) rte_container_of(ptr, struct timer_list, node)

os_critical_global_declare
static RTE_LIST(timer_list);

#ifdef TIMER_DEBUG
static void timer_list_dump(struct timer_list* target, long expires) {
	struct rte_list* ptr;
	pr_out("## slave timer begin: target(%p) expires(%ld)\n", target, expires);
	rte_list_foreach(ptr, &timer_list) {
		struct timer_list* tnode = TIMER_LIST(ptr);
		pr_out("timer(%p) expires(%ld)\n", tnode, tnode->expires);
	}
	pr_out("## slave timer end:\n");
}
#endif

static inline void list_insert_before(struct rte_list *newnd, 
	struct rte_list *node)	{
	__rte_list_add(newnd, node->prev, node);
}

/* Get first node from timer-list */
static inline struct timer_list *timer_first(void) {
	return !rte_list_empty(&timer_list)? 
		TIMER_LIST(timer_list.next): NULL;
}

static int timer_add_locked(struct timer_list *timer,
	long expires) {
	struct rte_list* ptr;
	if (rte_unlikely(timer->state != TIMER_STATE_IDLE))
		return -EBUSY;
	timer->state = TIMER_STATE_INSERT;
#ifdef TIMER_DEBUG
	long exp = expires;
#endif
	/*
	 * Insert the time-node to list with time order
	 */
	rte_list_foreach(ptr, &timer_list) {
		struct timer_list* tnode = TIMER_LIST(ptr);
		if (expires < tnode->expires) {
            tnode->expires -= expires;
			break;
        }
		expires -= tnode->expires;
	}
	timer->expires = expires;
	
	/* Insert before the target node */
	list_insert_before(&timer->node, ptr);
	timer->state = TIMER_STATE_ACTIVED;
#ifdef TIMER_DEBUG
	timer_list_dump(timer, exp);
#endif
	return 0;
}

static int timer_remove_locked(struct timer_list *timer) {
	struct timer_list* next_timer;
	struct rte_list* next_node;

	if (timer->state == TIMER_STATE_IDLE)
		return 0;

	/*
	 * We need to adjust time for next node when remove the node.
	 */
	next_node = timer->node.next;
	if (next_node != &timer_list) {
		next_timer = TIMER_LIST(next_node);
		next_timer->expires += timer->expires;
	}
	rte_list_del(&timer->node);
	timer->state = TIMER_STATE_IDLE;
	return 1;
}

long timer_schedule(long expires) {
	TIMER_LOCK_DECLARE
	struct timer_list* timer;
	long next_expired;

	TIMER_LOCK();

	/*
	 * If has no more work to do, then return
	 */
	if (rte_unlikely((timer = timer_first()) == NULL)) {
		next_expired = 0;
		goto _unlock;
	}

	if (timer->expires > expires) 
		goto _out;

	do {
		void (*fn)(struct timer_list*, void *);
		rte_list_del(&timer->node);
		fn = timer->expired_fn;
		expires -= timer->expires;
		timer->state = TIMER_STATE_IDLE;
		TIMER_UNLOCK();
		fn(timer, timer->arg);
		TIMER_LOCK();
		if (rte_unlikely((timer = timer_first()) == NULL)) {
			next_expired = 0;
			goto _unlock;
		}
	} while (timer->expires <= expires);

_out:
	next_expired = timer->expires - expires;
	timer->expires = next_expired;

_unlock:
	TIMER_UNLOCK();
	return next_expired;
}

void timer_init(struct timer_list *timer, 
    void (*expired_fn)(struct timer_list *, void *), void *arg) {
    timer->expired_fn = expired_fn;
    timer->expires = 0;
    timer->arg = arg;
    timer->state = 0;
}

int timer_mod(struct timer_list *timer, long expires) {
	TIMER_LOCK_DECLARE
	TIMER_LOCK();
	timer_remove_locked(timer);
	int ret = timer_add_locked(timer, expires);
	TIMER_UNLOCK();
	return ret;
}

int timer_add(struct timer_list *timer, long expires) {
	TIMER_LOCK_DECLARE
	TIMER_LOCK();
	int ret = timer_add_locked(timer, expires);
	TIMER_UNLOCK();
	return ret;
}

int timer_del(struct timer_list *timer) {
	TIMER_LOCK_DECLARE
	TIMER_LOCK();
	int pending = timer_remove_locked(timer);
	TIMER_UNLOCK();
	return pending;
}

int timer_visit(void (*visitor)(struct timer_list *)) {
    TIMER_LOCK_DECLARE
    struct rte_list *node;
    if (visitor == NULL)
        return -EINVAL;
    TIMER_LOCK();
	rte_list_foreach(node, &timer_list) {
        visitor(TIMER_LIST(node));
	}
    TIMER_UNLOCK();
    return 0;
}

