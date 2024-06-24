/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<os_callpath>: "fmt
#define CONFIG_LOGLEVEL LOGLEVEL_INFO
#include <assert.h>
#include <string.h>
#include <zephyr.h>

#include "basework/malloc.h"
#include "basework/lib/callpath.h"
#include "basework/log.h"

// #define USE_IRQPATH_MONITOR

#ifdef USE_IRQPATH_MONITOR
static struct callpath *irq_callpath;
#endif
static bool callpath_ready;

void __rte_notrace __cyg_profile_func_enter(void *this_fn, void *call_site) {
    if (rte_unlikely(!callpath_ready))
        return;
#ifdef USE_IRQPATH_MONITOR
    if (k_is_in_isr()) {
        unsigned int key = irq_lock();
        callpath_push_locked(irq_callpath, this_fn, 
            call_site, k_cycle_get_32());
        irq_unlock(key);
        return;
    }
#endif
#ifdef CONFIG_THREAD_CUSTOM_DATA
    k_tid_t curr = k_current_get();
    if (curr->custom_data) {
        callpath_push_locked(curr->custom_data, this_fn, 
            call_site, k_cycle_get_32());
    }
#endif
}

void __rte_notrace __cyg_profile_func_exit (void *this_fn, void *call_site) {
    if (rte_unlikely(!callpath_ready))
        return;
#ifdef USE_IRQPATH_MONITOR
    if (k_is_in_isr()) {
        unsigned int key = irq_lock();
        callpath_pop_locked(irq_callpath, this_fn, 
            call_site, k_cycle_get_32());
        irq_unlock(key);
        return;
    }
#endif
#ifdef CONFIG_THREAD_CUSTOM_DATA
    k_tid_t curr = k_current_get();
    if (curr->custom_data) {
        callpath_pop_locked(curr->custom_data, this_fn, 
            call_site, k_cycle_get_32());
    }
#endif
}

#ifdef CONFIG_THREAD_CUSTOM_DATA
static void __rte_notrace thread_user_cb(const struct k_thread *thread,
    void *user_data) {
    struct k_thread *dcthread = (void *)thread;
    int deepth = 40;

    assert(thread->custom_data == NULL);
    if (!strcmp(thread->name, "ui_service") ||
        !strcmp(thread->name, "res_loader") ||
        !strcmp(thread->name, "RunQueue")) {
        deepth = 80;
        goto _attach;
    }
    if (!strcmp(thread->name, "gsensor@IRQ") ||
        !strcmp(thread->name, "bluetooth"))
        goto _attach;

    return;
_attach:
    dcthread->custom_data = callpath_allocate(thread->name, deepth);
    assert(dcthread->custom_data != NULL);
}

static void __rte_notrace thread_iterate_cb(const struct k_thread *thread,
    void *user_data) {
    if (thread->custom_data) 
        callpath_dump_locked((void *)thread->custom_data, user_data);
}

#endif /* CONFIG_THREAD_CUSTOM_DATA */

void __rte_notrace callpath_profile_init(void) {
    callpath_init();
#ifdef CONFIG_THREAD_CUSTOM_DATA
    k_sched_lock();
    k_thread_foreach(thread_user_cb, NULL);
    k_sched_unlock();
#endif /* CONFIG_THREAD_CUSTOM_DATA */

#ifdef USE_IRQPATH_MONITOR
    irq_callpath = callpath_allocate("IRQ", 50);
    assert(irq_callpath != NULL);
#endif /* USE_IRQPATH_MONITOR */
    callpath_ready = true;
}

void __rte_notrace callpath_dump_all(struct printer *pr) {
    struct callnode_arg *p;
    
    if (!pr)
        return;
    p = general_calloc(1, CALLNODE_SIZE(80));
    if (p) {
        p->pr = pr;
        k_thread_foreach(thread_iterate_cb, p);
        general_free(p);
    }
}
