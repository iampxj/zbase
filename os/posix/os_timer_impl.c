/*
 * Copyright 2022 wtcat
 *
 * Timer implement for POSIX
 */

#define pr_fmt(fmt) "os_timer: "fmt

#include <errno.h>
#include <assert.h>
#include <string.h>
#include <signal.h>

#include "basework/os/osapi_config.h"
#include "basework/os/osapi_timer.h"
#include "basework/log.h"
#include "basework/rq.h"


struct os_timer {
    struct sigevent sig;
    struct itimerspec time;
    timer_t id;
    void (*task)(os_timer_t, void *);
    void *arg;
};

static struct os_timer timer_objs[CONFIG_OS_MAX_TIMERS];
static struct os_robj timer_robj;

static void timer_task_wrapper(void *arg) {
    struct os_timer *timer = arg;
    pr_dbg("timer task call: %p timer->task(%p) arg(%p)\n", timer, timer->task, arg);
    assert(timer->task != NULL);
    timer->task(timer, timer->arg);
}

static void timer_signal_cb(__sigval_t val) {
    struct os_timer *timer = val.sival_ptr;
    rq_submit(timer_task_wrapper, timer);
}

int __os_timer_create(os_timer_t *timer, 
    void (*timeout_cb)(os_timer_t, void *), 
    void *arg, 
    bool isr_context) {
    struct os_timer *p;
    int err;

    if (!timer || !timeout_cb)
        return -EINVAL;
    if (!os_obj_ready(&timer_robj)) {
        int err = os_obj_initialize(&timer_robj, timer_objs, 
            sizeof(timer_objs), sizeof(timer_objs[0]));
        if (err) {
            pr_err("Initialize timer object failed (%d)\n", err);
            os_panic();
            return err;
        }
    }
    p = os_obj_allocate(&timer_robj);
    if (!p) {
        pr_err("Allocate timer object failed\n");
        return -ENOMEM;
    }

    memset(p, 0, sizeof(*p));
    *p = (struct os_timer){ 0 };
    p->arg = arg;
    p->task = timeout_cb;
    p->sig.sigev_value.sival_ptr = p;
    p->sig.sigev_notify = SIGEV_THREAD;
    p->sig.sigev_notify_function = timer_signal_cb;
    err = timer_create(CLOCK_MONOTONIC, &p->sig, &p->id);
    if (err) {
        pr_err("Create timer failed!\n");
        return err;
    }
    *timer = p;
    return 0;
}

int __os_timer_mod(os_timer_t timer, long expires) {
    assert(timer != NULL);
    struct os_timer *p = timer;
    long sec;

    sec = expires / 1000;
    expires -= sec * 1000;
    p->time.it_value.tv_sec = sec;
    p->time.it_value.tv_nsec = expires * 1000000;
    p->time.it_interval.tv_sec = 0;
    p->time.it_interval.tv_nsec = 0;
    timer_settime(p->id, 0, &p->time, NULL);
    return 0;
}

int __os_timer_add(os_timer_t timer, long expires) {
    assert(timer != NULL);
    os_timer_mod(timer, expires);
    return 0;
}

int __os_timer_del(os_timer_t timer) {
    assert(timer != NULL);
    os_timer_mod(timer, 0);
    return 0;
}

int __os_timer_destroy(os_timer_t timer) {
    assert(timer != NULL);
    struct os_timer *p = timer;
    timer_delete(p->id);
    os_obj_free(&timer_robj, p);
    return 0;
}
