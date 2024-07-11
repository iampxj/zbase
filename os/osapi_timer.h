/*
 * Copyright 2022 wtcat
 *
 * Timer API for OS
 */
#ifndef BASEWORK_OS_TIMER_H_
#define BASEWORK_OS_TIMER_H_

#include <errno.h>
#include <stdbool.h>
#include <assert.h>

#include "basework/compiler.h"

#ifdef __cplusplus
extern "C"{
#endif

typedef void* os_timer_t;

#if !defined(__GNUC__) && !defined(__clang__)
#define __timer_type_check(timer) 1
#define ___timer_type_check(timer, type) 1
#define OS_TIMER_CHECKER 0
#else
#define ___timer_type_check(timer, type) \
    rte_compiletime_assert(__rte_same_type(timer, (type)0), "")
#define __timer_type_check(timer) \
    rte_compiletime_assert(__rte_same_type(timer, (os_timer_t)0), "")
#define OS_TIMER_CHECKER 1
#endif

#ifndef CONFIG_OS_TIMER_DISABLE
/*
 * os_timer_create - Initialize timer object
 * @timer: timer pointer
 * @expired_fn: timeout callback function
 * @arg: callback argument
 * @isr_context: execute with interrupt context
 * return 0 if success
 */
int __os_timer_create(os_timer_t *timer, 
    void (*expired_fn)(os_timer_t, void *), void *arg, bool isr_context);
#if OS_TIMER_CHECKER
#define os_timer_create(timer, expired_fn, arg, isr_context) \
({ \
    ___timer_type_check(timer, os_timer_t*); \
    int __err = __os_timer_create(timer, expired_fn, arg, isr_context); \
    assert(__err == 0); \
    __err; \
})
#else 
#define os_timer_create __os_timer_create
#endif

/*
 * os_timer_mod - modify timer expiration time and re-add to timer list
 * @timer: timer pointer
 * @expires: expiration time (unit: ms)
 * 
 * return 0 if success
 */
int __os_timer_mod(os_timer_t timer, long expires);
#if OS_TIMER_CHECKER
#define os_timer_mod(timer, expires) \
({ \
    __timer_type_check(timer); \
    int __err = __os_timer_mod(timer, expires); \
    assert(__err == 0); \
    __err; \
})
#else
#define os_timer_mod __os_timer_mod
#endif

/*
 * os_timer_add - add a new timer to timer list
 * @timer: timer pointer
 * @expires: expiration time (unit: ms)
 * 
 * return 0 if success
 */
int __os_timer_add(os_timer_t timer, long expires);
#if OS_TIMER_CHECKER
#define os_timer_add(timer, expires) \
({ \
    __timer_type_check(timer); \
    int __err = __os_timer_add(timer, expires); \
    assert(__err == 0); \
    __err; \
})
#else
#define os_timer_add __os_timer_add
#endif

/*
 * os_timer_del - delete a timer from timer list
 * @timer: timer pointer
 * 
 * return 1 if timer has pending
 */
int __os_timer_del(os_timer_t timer);
#if OS_TIMER_CHECKER
#define os_timer_del(timer) \
({ \
    __timer_type_check(timer); \
    int __err = __os_timer_del(timer); \
    assert(__err == 0); \
    __err; \
})
#else
#define os_timer_del __os_timer_del
#endif

/*
 * This function just only can be called when timer is stop or 
 * in timer callback function
 *
 * os_timer_update_handler - update timer callback function
 *
 * @timer: timer object pointer
 * @fn: callback function for timeout
 * @arg: user argument
 * return 0 if success
 */
int __os_timer_update_handler(os_timer_t timer, 
    void (*fn)(os_timer_t, void *), void *arg);
#if OS_TIMER_CHECKER
#define os_timer_update_handler(timer, fn, arg) \
({ \
    __timer_type_check(timer); \
    int __err = __os_timer_update_handler(timer, fn, arg); \
    assert(__err == 0); \
    __err; \
})
#else
#define os_timer_update_handler __os_timer_update_handler
#endif

/*
 * os_timer_destroy - delete a timer from timer list and free it
 * @timer: timer pointer
 * 
 * return 0 if success
 */
int __os_timer_destroy(os_timer_t timer);
#if OS_TIMER_CHECKER
#define os_timer_destroy(timer) \
({ \
    __timer_type_check(timer); \
    int __err = __os_timer_destroy(timer); \
    assert(__err == 0); \
    __err; \
})
#else
#define os_timer_destroy __os_timer_destroy
#endif

/*
 * __os_timer_gettime - Get the time since startup
 * 
 * return time base on milliseconds
 */
unsigned int __os_timer_gettime(void);
#ifndef os_timer_gettime
# define os_timer_gettime __os_timer_gettime
#endif

#ifdef CONFIG_OS_TIMER_TRACER
/*
 * os_timer_trace - Enable or disable timer tracer
 * @timer: timer pointer
 * @enable: function enable 
 *
 * return 0 if success
 */
int os_timer_trace(os_timer_t timer, bool enable);
#endif

#else /* CONFIG_OS_TIMER_DISABLE */

static inline int os_timer_create(os_timer_t *timer, 
    void (*expired_fn)(os_timer_t, void *), void *arg, bool isr_context) {
    return -ENOSYS;
}

static inline int os_timer_mod(os_timer_t timer, long expires) {
    return -ENOSYS;
}

static inline int os_timer_add(os_timer_t timer, long expires) {
    return -ENOSYS;
}

static inline int os_timer_del(os_timer_t timer) {
    return -ENOSYS;
}

static inline int os_timer_update_handler(os_timer_t timer, 
    void (*fn)(os_timer_t, void *), void *arg)  {
    return -ENOSYS;
}

static inline int os_timer_destroy(os_timer_t timer) {
    return -ENOSYS;
}

static inline unsigned int os_timer_gettime(void) {
    return -ENOSYS;
}

#ifdef CONFIG_OS_TIMER_TRACER
static inline int os_timer_trace(os_timer_t timer, bool enable) {
    return -ENOSYS;
}
#endif /* CONFIG_OS_TIMER_TRACER */

#endif /* !CONFIG_OS_TIMER_DISABLE */

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_OS_TIMER_H_ */

