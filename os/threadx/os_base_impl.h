/*
 * Copyright 2022 wtcat
 *
 * RTOS abstract layer
 */
#ifndef BASEWORK_OS_THREADX_OS_BASE_H_
#define BASEWORK_OS_THREADX_OS_BASE_H_

#include <stdint.h>
#include <assert.h>
#include <errno.h>

#include "tx_user.h"
#include "tx_api.h"
#include "basework/compiler_attributes.h"

#ifdef __cplusplus
extern "C"{
#endif

#define TX_CALLERR(fn, arg...) 0 - (int)fn(arg) 

#define os_in_isr()
#define os_panic(...)

/* Critical lock */
#define os_critical_global_declare
#define os_critical_declare TX_INTERRUPT_SAVE_AREA
#define os_critical_lock    TX_DISABLE
#define os_critical_unlock  TX_RESTORE

/* */
#ifndef CONFIG_BOOTLOADER
typedef TX_SEMAPHORE os_completion_t;
#define os_completion_declare(_cp) TX_SEMAPHORE _cp;
#define os_completion_reinit(_cp)  \
    do { \
        TX_MEMSET((_cp), 0, (sizeof(TX_SEMAPHORE))); \
        (_cp)->tx_semaphore_name = (CHAR *)__func__; \
        (_cp)->tx_semaphore_id = 0x53454D41; \
    } while (0)
#define os_completion_wait(_cp)    tx_semaphore_get((_cp), TX_WAIT_FOREVER)
#define os_completed(_cp)          tx_semaphore_ceiling_put((_cp), 1)
#define os_completion_timedwait(_cp, _to) tx_semaphore_get((_cp), TX_MSEC(_to))

#endif /* CONFIG_BOOTLOADER */


/*
  * Zephyr platform 
 */
#define OS_THREAD_API static inline
#define OS_MTX_API    static __rte_always_inline
#define OS_CV_API     static __rte_always_inline
#define OS_SEM_API    static __rte_always_inline
#define OS_EVENT_API  static __rte_always_inline

typedef void* cpu_set_t;
typedef struct {
    TX_THREAD thread;
} os_thread_t;

typedef struct {
    TX_MUTEX mtx;
} os_mutex_t;

typedef struct {
    TX_SEMAPHORE cond_semaphore;
    INT type;
    INT in_use;
} os_cond_t;

typedef struct {
    TX_SEMAPHORE sem;
} os_sem_t;

typedef struct {
    TX_EVENT_FLAGS_GROUP event;
} os_event_t;

OS_THREAD_API int 
_os_thread_spawn(os_thread_t *thread, const char *name, 
    void *stack, size_t size, 
    int prio, os_thread_entry_t entry, void *arg) {
    return TX_CALLERR(tx_thread_create, &thread->thread, (CHAR *)name, (VOID (*)(ULONG))(void *)entry, 
        (ULONG)arg, stack, size, prio, prio, TX_NO_TIME_SLICE, TX_TRUE);         
}

OS_THREAD_API int 
_os_thread_destroy(os_thread_t *thread) {
    return TX_CALLERR(tx_thread_delete, &thread->thread);
}

OS_THREAD_API int 
_os_thread_change_prio(os_thread_t *thread, int newprio, 
    int *oldprio) {
    return TX_CALLERR(tx_thread_priority_change, &thread->thread, (UINT)newprio, 
        (UINT *)oldprio);
}

#define os_thread_preemption_change(t, n, o) \
    _os_thread_preemption_change(t, n, o)
OS_THREAD_API int 
_os_thread_preemption_change(os_thread_t *thread, int new_threshold, 
    int *old_threshold) {
    return TX_CALLERR(tx_thread_preemption_change, &thread->thread, (UINT)new_threshold, 
        (UINT *)old_threshold);
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
    return TX_CALLERR(_tx_thread_sleep, TX_MSEC(ms));
}

OS_THREAD_API void 
_os_thread_yield(void) {
    tx_thread_relinquish();
}

OS_THREAD_API void*
_os_thread_self(void) {
    return (void *)tx_thread_identify();
}

OS_MTX_API int 
_os_mtx_init(os_mutex_t *mtx, int type) {
    (void) type;
    return TX_CALLERR(tx_mutex_create, &mtx->mtx, "mutex", TX_INHERIT);
}

OS_MTX_API int 
_os_mtx_destroy(os_mutex_t *mtx) {
    return TX_CALLERR(tx_mutex_delete, &mtx->mtx);
}

OS_MTX_API int 
_os_mtx_lock(os_mutex_t *mtx) {
    return TX_CALLERR(tx_mutex_get, &mtx->mtx, TX_WAIT_FOREVER);
}

OS_MTX_API int 
_os_mtx_unlock(os_mutex_t *mtx) {
    return TX_CALLERR(tx_mutex_put, &mtx->mtx);
}

OS_MTX_API int 
_os_mtx_timedlock(os_mutex_t *mtx, uint32_t timeout) {
    return TX_CALLERR(tx_mutex_get, &mtx->mtx, timeout);
}

OS_MTX_API int 
_os_mtx_trylock(os_mutex_t *mtx) {
    return TX_CALLERR(tx_mutex_get, &mtx->mtx, TX_NO_WAIT);
}

OS_CV_API int _os_cv_init(os_cond_t *cv, void *data) {
    (void)data;
    cv->in_use = TX_TRUE;
    return TX_CALLERR(tx_semaphore_create, &cv->cond_semaphore, "csem", 0);
}

OS_CV_API int _os_cv_signal(os_cond_t* cv) {
    TX_SEMAPHORE *semaphore_ptr = &cv->cond_semaphore;

    if (semaphore_ptr->tx_semaphore_suspended_count) {
        UINT status = tx_semaphore_prioritize(semaphore_ptr);
        if (status != TX_SUCCESS)
            return -EINVAL;
        status = tx_semaphore_put(semaphore_ptr);
        if (status != TX_SUCCESS)
            return -EINVAL;
    }

    return 0;
}

OS_CV_API int _os_cv_broadcast(os_cond_t *cv) {
    TX_SEMAPHORE *semaphore_ptr;
    TX_THREAD *thread;
    UINT status;
    ULONG sem_count;
    UINT old_threshold, dummy;

    semaphore_ptr = &cv->cond_semaphore;
    sem_count = semaphore_ptr->tx_semaphore_suspended_count;
    if (!sem_count)
        return 0;
    
    status = tx_semaphore_prioritize(semaphore_ptr);
    if (status != TX_SUCCESS)
        return -EINVAL;

    /* get to know about current thread */
    thread = tx_thread_identify();

    /* Got the current thread , now raise its preemption threshold */
    /* that way the current thread does not get descheduled when   */
    /* threads with higher priority are activated */
    tx_thread_preemption_change(thread,0,&old_threshold); 
    while (sem_count) {
        status = tx_semaphore_put(semaphore_ptr);
        if (status != TX_SUCCESS) {
            /* restore changed preemption threshold */
            tx_thread_preemption_change(thread,old_threshold,&dummy);
            return -EINVAL;
        }
        sem_count--;
    }

    /* restore changed preemption threshold */
    tx_thread_preemption_change(thread,old_threshold,&dummy);
    return 0;
}

OS_CV_API int _os_cv_wait(os_cond_t *cv, os_mutex_t *mtx) {
    TX_SEMAPHORE *semaphore_ptr;
    TX_THREAD *thread;
    UINT status;
    UINT old_threshold, dummy;
    
    thread = tx_thread_identify();
    tx_thread_preemption_change(thread,0,&old_threshold); 
    _os_mtx_unlock(mtx);

    semaphore_ptr = &cv->cond_semaphore;;
    status = tx_semaphore_get(semaphore_ptr, TX_WAIT_FOREVER);
    tx_thread_preemption_change(thread, old_threshold, &dummy);
    if (status != TX_SUCCESS)
        return -EINVAL;

    status = tx_semaphore_prioritize(semaphore_ptr);
    if (status != TX_SUCCESS)
        return -EINVAL;
    _os_mtx_lock(mtx);

    return 0;
}

#define OS_SEMAPHORE_IMLEMENT
OS_SEM_API int 
_os_sem_init(os_sem_t *sem, unsigned int value) {
    return TX_CALLERR(tx_semaphore_create, &sem->sem, "sem", value);
}

OS_SEM_API int 
_os_sem_timedwait(os_sem_t *sem, int64_t timeout) {
    return TX_CALLERR(tx_semaphore_get, &sem->sem, (ULONG)timeout);
}

OS_SEM_API int 
_os_sem_wait(os_sem_t *sem) {
    return TX_CALLERR(tx_semaphore_get, &sem->sem, TX_WAIT_FOREVER);
}

OS_SEM_API int 
_os_sem_trywait(os_sem_t *sem) {
    return TX_CALLERR(tx_semaphore_get, &sem->sem, TX_NO_WAIT);
}

OS_SEM_API int 
_os_sem_post(os_sem_t *sem) {
    return TX_CALLERR(tx_semaphore_put, &sem->sem);
}

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_OS_THREADX_OS_BASE_H_ */
