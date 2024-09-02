/*
 * CopyRight 2022 wtcat
 */

#ifndef BASE_TIMER_H_
#define BASE_TIMER_H_

#include <rtems/score/watchdogimpl.h>

#ifdef __cplusplus
extern "C"{
#endif

struct timer {
	Watchdog_Control timer;
};

void timer_init(struct timer *timer, void (*adaptor)(struct timer *));
int  timer_add(struct timer *timer, uint32_t expires);
int  timer_mod(struct timer *timer, uint32_t expires);
int  timer_del(struct timer *timer);
bool timer_is_pending(struct timer *timer);


#ifdef __cplusplus
}
#endif
#endif /* BASE_TIMER_H_ */
