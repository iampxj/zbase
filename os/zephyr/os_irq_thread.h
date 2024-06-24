/*
 * Copyright 2023 wtcat
 */
#ifndef ZEPHYR_IRQ_THREAD_H_
#define ZEPHYR_IRQ_THREAD_H_

#include <assert.h>
#include <zephyr.h>

#include "sys_wakelock.h"

#ifdef __cplusplus
extern "C"{
#endif

#define IRQ_THREAD_PRIO (-3)

struct irq_thread_arg {
	struct k_sem *sync;
	void (*task)(void *arg);
	void *arg;
};

static inline void irq_daemon_thread(void *arg) {
	struct irq_thread_arg *p = arg;
	assert(p != NULL);
	assert(p->sync != NULL);
	assert(p->task != NULL);
	for ( ; ; ) {
		k_sem_take(p->sync, K_FOREVER);
        sys_wake_lock_ext(PARTIAL_WAKE_LOCK, APP_WAKE_LOCK_USER);
		p->task(p->arg);
        sys_wake_unlock_ext(PARTIAL_WAKE_LOCK, APP_WAKE_LOCK_USER);
	}
}

#define MTX_DEFINE(name) K_MUTEX_DEFINE(name)
#define mtx_lock(x)   k_mutex_lock(x, K_FOREVER)
#define mtx_unlock(x) k_mutex_unlock(x)
    
#define irq_thread_wakeup(_handle) \
	k_sem_give((struct k_sem *)_handle)

#define irq_thread_new(_name, _stacksize, _entry, _arg) \
	_irq_thread_new(_name, IRQ_THREAD_PRIO, _stacksize, _entry, _arg)

#define _irq_thread_new(_name, _prio, _stacksize, _entry, _arg) \
	({\
		static K_SEM_DEFINE(_name##sem, 0, 1); \
		static K_THREAD_STACK_DEFINE(_name##_thread_stack, _stacksize); \
		static struct k_thread _name##_thread_data; \
		static struct irq_thread_arg _name##thread_param = { \
			.sync = &_name##sem, \
			.task = _entry, \
			.arg = _arg \
		}; \
		k_thread_create(&_name##_thread_data, _name##_thread_stack, \
				K_THREAD_STACK_SIZEOF(_name##_thread_stack), \
				(k_thread_entry_t)irq_daemon_thread, &_name##thread_param, \
				NULL, NULL, _prio, 0, K_NO_WAIT); \
		k_thread_name_set(&_name##_thread_data, #_name "@IRQ"); \
		&_name##sem; \
	})



#ifdef __cplusplus
}
#endif

#endif /* ZEPHYR_IRQ_THREAD_H_ */