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

OS_THREAD_API int _os_thread_spawn(os_thread_t *thread, const char *name, 
    void *stack, size_t size, 
    int prio, os_thread_entry_t entry, void *arg);
OS_THREAD_API int _os_thread_destroy(os_thread_t *thread);
OS_THREAD_API int _os_thread_change_prio(os_thread_t *thread, int newprio, 
    int *oldprio);
OS_THREAD_API int   _os_thread_sleep(uint32_t ms);
OS_THREAD_API void  _os_thread_yield(void);
OS_THREAD_API void  *_os_thread_self(void);

#define os_thread_spawn(thr, name, stack, size, prio, entry, arg) \
    _os_thread_spawn(thr, name, stack, size, prio, entry, arg)

#define os_thread_destroy(thr) \
    _os_thread_destroy(thr)

#define os_thread_change_prio(thr, newprio, oldprioptr) \
    _os_thread_change_prio(thr, newprio, oldprioptr)

#define os_thread_sleep(milliseconds) \
    _os_thread_sleep(milliseconds)

#define os_thread_yield() \
    _os_thread_yield()

#define os_thread_self() \
    _os_thread_self()

#define os_thread_exit() \
    os_thread_destroy(os_thread_self())

/* 
 * Thread sync 
 */
OS_MTX_API int _os_mtx_init(os_mutex_t *mtx, int type);
OS_MTX_API int _os_mtx_destroy(os_mutex_t *mtx);
OS_MTX_API int _os_mtx_lock(os_mutex_t *mtx);
OS_MTX_API int _os_mtx_unlock(os_mutex_t *mtx);
OS_MTX_API int _os_mtx_timedlock(os_mutex_t *mtx, uint32_t timeout);
OS_MTX_API int _os_mtx_trylock(os_mutex_t *mtx);

#define os_mtx_init(mtx, type) \
    _os_mtx_init(mtx, type)

#define os_mtx_destroy(mtx) \
    _os_mtx_destroy(mtx)

#define os_mtx_lock(mtx) \
    _os_mtx_lock(mtx)

#define os_mtx_unlock(mtx) \
    _os_mtx_unlock(mtx)

#define _os_mtx_timedlock(mtx, timeout) \
    _os_mtx_timedlock(mtx, timeout)

#define os_mtx_trylock(mtx) \
    _os_mtx_trylock(mtx)

/*
 * Condition variable (Optional)
 */
OS_CV_API int _os_cv_init(os_cond_t *cv, void *data);
OS_CV_API int _os_cv_signal(os_cond_t *cv);
OS_CV_API int _os_cv_broadcast(os_cond_t *cv);
OS_CV_API int _os_cv_wait(os_cond_t *cv, os_mutex_t *mtx);

#define os_cv_init(cv, data) \
    _os_cv_init(dv, data)

#define os_cv_signal(cv) \
    _os_cv_signal(cv)

#define os_cv_broadcast(cv) \
    _os_cv_broadcast(cv)

#define os_cv_wait(cv, mtx) \
    _os_cv_wait(cv, mtx)


/*
 * Semaphore (Optional)
 */
#ifdef OS_SEMAPHORE_IMLEMENT
OS_SEM_API int _os_sem_init(os_sem_t *sem, unsigned int value);
OS_SEM_API int _os_sem_timedwait(os_sem_t *sem, int64_t timeout_ns);
OS_SEM_API int _os_sem_wait(os_sem_t *sem);
OS_SEM_API int _os_sem_trywait(os_sem_t *sem);
OS_SEM_API int _os_sem_post(os_sem_t *sem);

#define os_sem_doinit(sem, val) \
    _os_sem_init(sem, val)

#define os_sem_timedwait(sem, timeout) \
    _os_sem_timedwait(sem, timeout)

#define os_sem_wait(sem) \
    _os_sem_wait(sem)

#define os_sem_trywait(sem) \
    _os_sem_trywait(sem)

#define os_sem_post(sem) \
    _os_sem_post(sem)
#endif /* OS_SEMAPHORE_IMLEMENT */

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_OS_OSAPI_H_ */
