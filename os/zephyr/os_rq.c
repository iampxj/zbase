/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<os_rq>: "fmt
#define CONFIG_LOGLEVEL LOGLEVEL_DEBUG
#include <kernel.h>
#include <sys_wakelock.h>

#include "basework/os/osapi_timer.h"
#include "basework/rq.h"
#include "basework/log.h"

#ifdef CONFIG_WATCHDOG
#include <watchdog_hal.h>
#endif

#define USE_TIME_MEASURE
#define CRITICAL_TIME (50 * 1000) //Microseconds
#define RQ_HIGHEST_PRIORITY 5
#define RQ_LOW_CRITICAL  (CONFIG_BASEWORK_RQ_CAPACITY / 2)
#define RQ_HIGH_CRITICAL ((CONFIG_BASEWORK_RQ_CAPACITY * 2) / 3)

_Static_assert(RQ_HIGHEST_PRIORITY < CONFIG_BASEWORK_RQ_PRIORITY, "");

static uint32_t old_timestamp __rte_section(".ram.noinit");
static int rq_curr_priority __rte_section(".ram.noinit");

static void _rq_param_update(struct rq_context *rq) {
    if (unlikely(rq->nr > RQ_HIGH_CRITICAL)) {
        if (rq_curr_priority != RQ_HIGHEST_PRIORITY) {
            k_thread_priority_set(k_current_get(), RQ_HIGHEST_PRIORITY);
            rq_curr_priority = RQ_HIGHEST_PRIORITY;
        }
        pr_dbg("RunQueue task priority boost to %d (QueuedCount: %d)\n", 
            rq_curr_priority, rq->nr);
        return;
    }
    if (rq_curr_priority != CONFIG_BASEWORK_RQ_PRIORITY) {
        if (rq->nr < RQ_LOW_CRITICAL) {
            k_thread_priority_set(k_current_get(), CONFIG_BASEWORK_RQ_PRIORITY);
            rq_curr_priority = CONFIG_BASEWORK_RQ_PRIORITY;
        }
        pr_dbg("RunQueue task priority restore to %d (QueuedCount: %d)\n", 
            rq_curr_priority, rq->nr);
    }
}

void _rq_execute_prepare(struct rq_context *rq) {
    _rq_param_update(rq);
    sys_wake_lock_ext(PARTIAL_WAKE_LOCK, 
        APP_WAKE_LOCK_USER);
#ifdef USE_TIME_MEASURE
    old_timestamp = k_cycle_get_32();
#endif
#ifdef CONFIG_WATCHDOG
	watchdog_clear();  
#endif
}

void _rq_execute_post(struct rq_context *rq) {
    (void) rq;
#ifdef USE_TIME_MEASURE
    uint32_t delta = k_cyc_to_us_near32(k_cycle_get_32() - old_timestamp);
    if (delta > CRITICAL_TIME) {
        printk("** run-queue cost time(%u us); schedule-func(%p)\n", 
            delta, rq_current_executing());
    }
#endif
    sys_wake_unlock_ext(PARTIAL_WAKE_LOCK, 
        APP_WAKE_LOCK_USER);
}
