/*
 * CopyRight 2022 wtcat
 */
#ifndef BASE_TIMER_LIST_H
#define BASE_TIMER_LIST_H

#include "basework/container/list.h"

#ifdef __cplusplus
extern "C"{
#endif

enum timer_state {
	TIMER_STATE_IDLE,
	TIMER_STATE_INSERT,
	TIMER_STATE_ACTIVED,
	TIMER_STATE_INVALID
};

struct timer_list {
    struct rte_list node;
    void (*expired_fn)(struct timer_list *, void *);
    long expires;
    void *arg;
    enum timer_state state;
};

/*
 * timer_visit - iterate timer list
 * @visitor: callback function pointer
 * 
 * return 0 if success
 */
int timer_visit(void (*iterator)(struct timer_list *));

/*
 * timer_schedule - called by hardware interrupt service
 * @expires: expired time
 *
 * Return the next exipiration time 
 */
long timer_schedule(long expires);


/*
 * timer_init - Initialize timer object
 * @timer: timer pointer
 * @expired_fn: timeout callback function
 * @arg: callback argument
 */
void timer_init(struct timer_list *timer, 
    void (*expired_fn)(struct timer_list *, void *), void *arg);

/*
 * timer_mod - modify timer expiration time and re-add to timer list
 * @timer: timer pointer
 * @expires: expiration time (unit: ms)
 * 
 * return 0 if success
 */
int timer_mod(struct timer_list* timer, long expires);

/*
 * timer_add - add a new timer to timer list
 * @timer: timer pointer
 * @expires: expiration time (unit: ms)
 * 
 * return 0 if success
 */
int timer_add(struct timer_list* timer, long expires);

/*
 * timer_del - delete a timer from timer list
 * @timer: timer pointer
 * 
 * return 1 if timer has pending
 */
int timer_del(struct timer_list* timer);

#ifdef __cplusplus
}
#endif
#endif /* BASE_TIMER_LIST_H */
