/*
 * Copyright 2022 wtcat
 *
 * RTOS abstract layer
 */
#ifndef BASEWORK_OS_ZEPHYR_OS_BASE_H_
#define BASEWORK_OS_ZEPHYR_OS_BASE_H_

#include "basework/compiler_attributes.h"
#include <assert.h>
#include <kernel.h>

#ifndef CONFIG_BOOTLOADER
#include <posix/pthread.h>
#endif

#ifdef __cplusplus
extern "C"{
#endif

#define os_in_isr() k_is_in_isr()
#define os_panic(...) k_panic()

/* Critical lock */
#define os_critical_global_declare
#define os_critical_declare int ___lock;
#define os_critical_lock   (___lock) = irq_lock();
#define os_critical_unlock irq_unlock(___lock);

/* */
#ifndef CONFIG_BOOTLOADER
typedef struct k_sem os_completion_t;
#define os_completion_declare(_cp) struct k_sem _cp;
#define os_completion_reinit(_cp)  k_sem_init((_cp), 0, 1)
#define os_completion_wait(_cp)    k_sem_take((_cp), K_FOREVER)
#define os_completed(_cp)          k_sem_give((_cp))
#define os_completion_timedwait(_cp, _to) k_sem_take((_cp), K_MSEC(_to))

#endif /* CONFIG_BOOTLOADER */


/*
  * Zephyr platform 
 */
#define OS_THREAD_API static inline
#define OS_MTX_API    static __rte_always_inline
#define OS_CV_API     static __rte_always_inline
#define OS_SEM_API    static __rte_always_inline

typedef void* cpu_set_t;
typedef struct {
    struct k_thread thread;
} os_thread_t;

typedef struct {
    struct k_mutex mtx;
} os_mutex_t;

typedef struct {
    struct k_condvar cv;
} os_cond_t;

typedef struct {
    struct k_sem sem;
} os_sem_t;

OS_THREAD_API int 
_os_thread_spawn(os_thread_t *thread, const char *name, 
    void *stack, size_t size, 
    int prio, os_thread_entry_t entry, void *arg) {
    k_tid_t tid;
    tid = k_thread_create(&thread->thread, stack, size, 
        (k_thread_entry_t)(void *)entry, arg, NULL, NULL, 
        prio, 0, K_NO_WAIT);
    if (tid != NULL)
        k_thread_name_set(&thread->thread, name);
    return 0;
}

OS_THREAD_API int 
_os_thread_destroy(os_thread_t *thread) {
    k_thread_abort(&thread->thread);
    return 0;
}

OS_THREAD_API int 
_os_thread_change_prio(os_thread_t *thread, int newprio, 
    int *oldprio) {
    if (oldprio) 
        *oldprio = k_thread_priority_get(&thread->thread);
    k_thread_priority_set(&thread->thread, newprio);
    return 0;
}

OS_THREAD_API int 
_os_thread_setaffinity(os_thread_t *thread, size_t cpusetsize, 
    const cpu_set_t *cpuset) {
    return -ENOSYS;
}

OS_THREAD_API int 
_os_thread_getaffinity(os_thread_t *thread, size_t cpusetsize, 
    cpu_set_t *cpuset) {
    return -ENOSYS;
}

OS_THREAD_API int 
_os_thread_sleep(uint32_t ms) {
    return k_msleep(ms);
}

OS_THREAD_API void 
_os_thread_yield(void) {
    k_yield();
}

OS_THREAD_API void*
_os_thread_self(void) {
    return k_current_get();
}

OS_MTX_API int 
_os_mtx_init(os_mutex_t *mtx, int type) {
    (void) type;
    k_mutex_init(&mtx->mtx);
    return 0;
}

OS_MTX_API int 
_os_mtx_destroy(os_mutex_t *mtx) {
    return -ENOSYS;
}

OS_MTX_API int 
_os_mtx_lock(os_mutex_t *mtx) {
    return k_mutex_lock(&mtx->mtx, K_FOREVER);
}

OS_MTX_API int 
_os_mtx_unlock(os_mutex_t *mtx) {
    return k_mutex_unlock(&mtx->mtx);
}

OS_MTX_API int 
_os_mtx_timedlock(os_mutex_t *mtx, uint32_t timeout) {
    return k_mutex_lock(&mtx->mtx, K_TICKS(timeout));
}

OS_MTX_API int 
_os_mtx_trylock(os_mutex_t *mtx) {
    return k_mutex_lock(&mtx->mtx, K_NO_WAIT);
}

OS_CV_API int _os_cv_init(os_cond_t *cv, void *data) {
	return k_condvar_init(&cv->cv);
}

OS_CV_API int _os_cv_signal(os_cond_t* cv) {
	return k_condvar_signal(&cv->cv);
}

OS_CV_API int _os_cv_broadcast(os_cond_t *cv) {
	return k_condvar_broadcast(&cv->cv);
}

OS_CV_API int _os_cv_wait(os_cond_t *cv, os_mutex_t *mtx) {
	return k_condvar_wait(&cv->cv, &mtx->mtx, K_FOREVER);
}

#define OS_SEMAPHORE_IMLEMENT
OS_SEM_API int 
_os_sem_init(os_sem_t *sem, unsigned int value) {
    return k_sem_init(&sem->sem, value, K_SEM_MAX_LIMIT);
}

OS_SEM_API int 
_os_sem_timedwait(os_sem_t *sem, int64_t timeout) {
    int64_t us = timeout / 1000;
    return k_sem_take(&sem->sem, K_USEC(us));
}

OS_SEM_API int 
_os_sem_wait(os_sem_t *sem) {
    return k_sem_take(&sem->sem, K_FOREVER);
}

OS_SEM_API int 
_os_sem_trywait(os_sem_t *sem) {
    return k_sem_take(&sem->sem, K_NO_WAIT);
}

OS_SEM_API int 
_os_sem_post(os_sem_t *sem) {
    k_sem_give(&sem->sem);
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_OS_ZEPHYR_OS_BASE_H_ */
