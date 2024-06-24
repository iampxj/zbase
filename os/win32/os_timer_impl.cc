/*
 * Copyright 2022 wtcat
 */
#include <assert.h>

#include "basework/os/osapi_config.h"
#include "basework/os/osapi_timer.h"
#include "basework/os/osapi_obj.h"
#include "basework/lib/timer/timer_list.h"
#include "basework/log.h"
// #include "basework/rq.h"

#include <thread>

struct os_timer {
    struct timer_list timer;
};

static struct os_timer timer_objs[CONFIG_OS_MAX_TIMERS];
static struct os_robj timer_robj;

int __os_timer_create(os_timer_t *timer, 
    void (*timeout_cb)(os_timer_t, void *), 
    void *arg, 
    bool isr_context) {
    (void)isr_context;
    struct os_timer *p;

    if (!timer || !timeout_cb)
        return -EINVAL;
    if (!os_obj_ready(&timer_robj)) {
        int err = os_obj_initialize(&timer_robj, timer_objs, 
            sizeof(timer_objs), sizeof(timer_objs[0]));
        if (err) {
            pr_err("Initialize timer object failed (%d)\n", err);
            assert(0);
            return err;
        }
    }
    p = (struct os_timer *)os_obj_allocate(&timer_robj);
    if (!p) {
        pr_err("Allocate timer object failed\n");
        return -ENOMEM;
    }
    timer_init(&p->timer, 
        (void (*)(struct timer_list *, void *))timeout_cb, 
        arg
    );
    *timer = p;
    return 0;
}

int __os_timer_mod(os_timer_t timer, long expires) {
    assert(timer != NULL);
    struct os_timer *p = (struct os_timer *)timer;
    return timer_mod(&p->timer, expires);
}

int __os_timer_add(os_timer_t timer, long expires) {
    assert(timer != NULL);
    struct os_timer *p = (struct os_timer *)timer;
    return timer_add(&p->timer, expires);
}

int __os_timer_del(os_timer_t timer) {
    assert(timer != NULL);
    struct os_timer *p = (struct os_timer *)timer;
    return timer_del(&p->timer);
}

int __os_timer_destroy(os_timer_t timer) {
    assert(timer != NULL);
    struct os_timer *p = (struct os_timer *)timer;
    timer_del(&p->timer);
    os_obj_free(&timer_robj, p);
    return 0;
}

static void os_timer_thread(void) {
    int sleep_ms = 10;

    for ( ; ; ) {
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_ms));
        sleep_ms = timer_schedule(sleep_ms);
        if (sleep_ms == 0)
            sleep_ms = 10;
    }
}

std::thread timer_thread(os_timer_thread);
