/*
 * Copyright 2022 wtcat
 *
 * RTOS abstract layer
 */

#ifndef BASEWORK_OS_OSAPI_H_
#define BASEWORK_OS_OSAPI_H_

typedef void (*os_thread_entry_t)(void *);

#include "os_base_impl.h"
#include "basework/os/osapi_timer.h"
#include "basework/os/osapi_fs.h"

#ifdef __cplusplus
extern "C"{
#endif

#ifndef os_panic
#define os_panic(...) for(;;)
#endif

#ifndef os_in_isr
#define os_in_isr() 0
#endif

/* Critical lock */
#ifndef os_critical_global_declare
#define os_critical_global_declare
#endif
#ifndef os_critical_declare
#define os_critical_declare
#endif
#ifndef os_critical_lock
#define os_critical_lock
#endif
#ifndef os_critical_unlock
#define os_critical_unlock
#endif

/* */
#ifndef os_completion_declare
#define os_completion_declare(_proc)  
#endif
#ifndef os_completion_reinit
#define os_completion_reinit(_proc) (void)(_proc)
#endif
#ifndef os_completion_wait
#define os_completion_wait(_proc)   (void)(_proc)
#endif
#ifndef os_completed
#define os_completed(_proc)         (void)(_proc)
#endif


/* 
 * Thread Class API 
 */
#ifndef OS_THREAD_API
#define OS_THREAD_API
#endif
#ifndef OS_MTX_API
#define OS_MTX_API
#endif
#ifndef OS_CV_API
#define OS_CV_API
#endif
#ifndef OS_SEM_API
#define OS_SEM_API
#endif

#ifndef os_thread_spawn
#define os_thread_spawn(thr, name, stack, size, prio, entry, arg) \
    _os_thread_spawn(thr, name, stack, size, prio, entry, arg)
OS_THREAD_API int _os_thread_spawn(os_thread_t *thread, const char *name, 
    void *stack, size_t size, 
    int prio, os_thread_entry_t entry, void *arg);
#endif

#ifndef os_thread_destroy
#define os_thread_destroy(thr) \
    _os_thread_destroy(thr)
OS_THREAD_API int _os_thread_destroy(os_thread_t *thread);
#endif

#ifndef os_thread_change_prio
#define os_thread_change_prio(thr, newprio, oldprioptr) \
    _os_thread_change_prio(thr, newprio, oldprioptr)
OS_THREAD_API int _os_thread_change_prio(os_thread_t *thread, int newprio, 
    int *oldprio);
#endif

#ifndef os_thread_setaffinity
#define os_thread_setaffinity(thr, cpusetsize, cpuset) \
    _os_thread_setaffinity(thr, cpusetsize, cpuset)
OS_THREAD_API int _os_thread_setaffinity(os_thread_t *thread, size_t cpusetsize, 
    const cpu_set_t *cpuset);
#endif

#ifndef os_thread_getaffinity
#define os_thread_getaffinity(thr, cpusetsize, cpuset) \
    _os_thread_getaffinity(thr, cpusetsize, cpuset)
OS_THREAD_API int _os_thread_getaffinity(os_thread_t *thread, size_t cpusetsize, 
    cpu_set_t *cpuset);
#endif

#ifndef os_thread_sleep
#define os_thread_sleep(milliseconds) \
    _os_thread_sleep(milliseconds)
OS_THREAD_API int   _os_thread_sleep(uint32_t ms);
#endif

#ifndef os_thread_yield
#define os_thread_yield() \
    _os_thread_yield()
OS_THREAD_API void  _os_thread_yield(void);
#endif

#ifndef os_thread_self
#define os_thread_self() \
    _os_thread_self()
OS_THREAD_API void  *_os_thread_self(void);
#endif

#ifndef os_thread_exit
#define os_thread_exit() \
    _os_thread_destroy(os_thread_self())
OS_THREAD_API int _os_thread_destroy(os_thread_t *thread);
#endif

/* 
 * Thread sync 
 */
#ifndef os_mtx_init
#define os_mtx_init(mtx, type) \
    _os_mtx_init(mtx, type)
OS_MTX_API int _os_mtx_init(os_mutex_t *mtx, int type);
#endif

#ifndef os_mtx_destroy
#define os_mtx_destroy(mtx) \
    _os_mtx_destroy(mtx)
OS_MTX_API int _os_mtx_destroy(os_mutex_t *mtx);
#endif

#ifndef os_mtx_lock
#define os_mtx_lock(mtx) \
    _os_mtx_lock(mtx)
OS_MTX_API int _os_mtx_lock(os_mutex_t *mtx);
#endif

#ifndef os_mtx_unlock
#define os_mtx_unlock(mtx) \
    _os_mtx_unlock(mtx)
OS_MTX_API int _os_mtx_unlock(os_mutex_t *mtx);
#endif

#ifndef _os_mtx_timedlock
#define _os_mtx_timedlock(mtx, timeout) \
    _os_mtx_timedlock(mtx, timeout)
OS_MTX_API int _os_mtx_timedlock(os_mutex_t *mtx, uint32_t timeout);
#endif

#ifndef os_mtx_trylock
#define os_mtx_trylock(mtx) \
    _os_mtx_trylock(mtx)
OS_MTX_API int _os_mtx_trylock(os_mutex_t *mtx);
#endif

/*
 * Condition variable (Optional)
 */
#ifndef os_cv_init
#define os_cv_init(cv, data) \
    _os_cv_init(dv, data)
OS_CV_API int _os_cv_init(os_cond_t *cv, void *data);
#endif

#ifndef os_cv_signal
#define os_cv_signal(cv) \
    _os_cv_signal(cv)
OS_CV_API int _os_cv_signal(os_cond_t *cv);
#endif

#ifndef os_cv_broadcast
#define os_cv_broadcast(cv) \
    _os_cv_broadcast(cv)
OS_CV_API int _os_cv_broadcast(os_cond_t *cv);
#endif

#ifndef os_cv_wait
#define os_cv_wait(cv, mtx) \
    _os_cv_wait(cv, mtx)
OS_CV_API int _os_cv_wait(os_cond_t *cv, os_mutex_t *mtx);
#endif

/*
 * Semaphore (Optional)
 */
#ifdef OS_SEMAPHORE_IMLEMENT

#ifndef os_sem_doinit
#define os_sem_doinit(sem, val) \
    _os_sem_init(sem, val)
OS_SEM_API int _os_sem_init(os_sem_t *sem, unsigned int value);
#endif

#ifndef os_sem_timedwait
#define os_sem_timedwait(sem, timeout) \
    _os_sem_timedwait(sem, timeout)
OS_SEM_API int _os_sem_timedwait(os_sem_t *sem, int64_t timeout_ns);
#endif

#ifndef os_sem_wait
#define os_sem_wait(sem) \
    _os_sem_wait(sem)
OS_SEM_API int _os_sem_wait(os_sem_t *sem);
#endif

#ifndef os_sem_trywait
#define os_sem_trywait(sem) \
    _os_sem_trywait(sem)
OS_SEM_API int _os_sem_trywait(os_sem_t *sem);
#endif

#ifndef os_sem_post
#define os_sem_post(sem) \
    _os_sem_post(sem)
OS_SEM_API int _os_sem_post(os_sem_t *sem);
#endif

#endif /* OS_SEMAPHORE_IMLEMENT */

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_OS_OSAPI_H_ */
