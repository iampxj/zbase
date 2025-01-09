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
#define USE_RQ_HISTORY

#define CRITICAL_TIME (50 * 1000) //Microseconds
#define RQ_HIGHEST_PRIORITY 5
#define RQ_LOW_CRITICAL  (CONFIG_BASEWORK_RQ_CAPACITY / 2)
#define RQ_HIGH_CRITICAL ((CONFIG_BASEWORK_RQ_CAPACITY * 2) / 3)
#define RQ_HISTORY_SIZE  8
#define RQ_HISTORY_MASK  (RQ_HISTORY_SIZE - 1)

struct rn_history {
    void *fn;
    uint32_t diff_us;
};

_Static_assert(RQ_HIGHEST_PRIORITY < CONFIG_BASEWORK_RQ_PRIORITY, "");

WK_LOCK_DEFINE(run_queue, PARTIAL_WAKE_LOCK)
static uint32_t old_timestamp __rte_section(".ram.noinit");
static int rq_curr_priority __rte_section(".ram.noinit");

#ifdef USE_RQ_HISTORY
static struct rn_history history_queue[RQ_HISTORY_SIZE];
static uint8_t queue_current_head;
static uint8_t queue_next_head;
#endif /* USE_RQ_HISTORY */

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

#ifdef USE_RQ_HISTORY
    queue_current_head = queue_next_head;
    queue_next_head = (queue_next_head + 1) & RQ_HISTORY_MASK;
    history_queue[queue_current_head].fn = rq_current_executing();
    history_queue[queue_current_head].diff_us = 0;
#endif /* USE_RQ_HISTORY */

#ifdef CONFIG_WATCHDOG
	watchdog_clear();  
#endif
}

void _rq_execute_post(struct rq_context *rq) {
    (void) rq;
#ifdef USE_TIME_MEASURE
    uint32_t now = k_cycle_get_32();
    uint32_t diff;

    if (now > old_timestamp)
        diff = now - old_timestamp;
    else
        diff = (uint64_t)now + UINT32_MAX - old_timestamp;
    uint32_t diff_us = SYS_CLOCK_HW_CYCLES_TO_NS_AVG(diff, 1000);
#ifdef USE_RQ_HISTORY
    history_queue[queue_current_head].diff_us = diff_us;
    history_queue[queue_current_head].fn = rq_current_executing();
#endif /* USE_RQ_HISTORY */

    if (diff_us > CRITICAL_TIME) {
        printk("** run-queue cost time(%u us); schedule-func(%p)\n", 
            diff_us, rq_current_executing());
    }
#endif
    sys_wake_unlock_ext(PARTIAL_WAKE_LOCK, 
        APP_WAKE_LOCK_USER);
}

#ifdef USE_RQ_HISTORY
void _rq_dump_history(void) {
    uint8_t idx = queue_current_head;
    uint8_t next = idx;
    int i = 0;

    printk("Dump run-queue recent record:\n");
    do {
        next = (next + 1) & RQ_HISTORY_MASK;
        printk("Fn@ <%p> cost_us(%d) order(%d)\n", 
            history_queue[next].fn, 
            history_queue[next].diff_us,
            RQ_HISTORY_SIZE - i
        );
        i++;
    } while (next != idx);
}
#else /* !USE_RQ_HISTORY */

void _rq_dump_history(void) {}
#endif /* USE_RQ_HISTORY */
