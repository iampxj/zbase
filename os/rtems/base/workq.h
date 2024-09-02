/*
 * CopyRight 2022 wtcat
 */
#ifndef BASE_WORKQUEUE_H_
#define BASE_WORKQUEUE_H_

#include <rtems.h>
#include <rtems/scheduler.h>
#include <bsp/linker-symbols.h>
#include "base/timer.h"

#ifdef __cplusplus
extern "C"{
#endif

#if defined(RTEMS_SMP)
#define SMP_ALIGNMENT CPU_STRUCTURE_ALIGNMENT
#else
#define SMP_ALIGNMENT
#endif

#define WQ_MSEC(n) RTEMS_MILLISECONDS_TO_TICKS(n)

struct workqueue {
#if defined(RTEMS_SMP)
    rtems_interrupt_lock lock;
#endif
    Chain_Control entries;
    rtems_id thread;
} SMP_ALIGNMENT;

struct work_struct {
    Chain_Node node;
    void (*handler)(struct work_struct *work);
};

struct delayed_work {
    struct work_struct work;
    struct timer timer;
    struct workqueue *wq;
};

struct sysworkq_attr {
    int prio;
    int mode;
    size_t stacksize;
};

#ifdef __CONFIGURATION_TEMPLATE_h
# ifndef CONFIGURE_WORKQ_PRIORITY
#  define CONFIGURE_WORKQ_PRIORITY  14
# endif
# ifndef CONFIGURE_WORKQ_STACKSIZE
#  define CONFIGURE_WORKQ_STACKSIZE 4096
# endif
# ifndef CONFIGURE_WORKQ_TASKMODE
#  define CONFIGURE_WORKQ_TASKMODE  (RTEMS_PREEMPT | RTEMS_NO_ASR | RTEMS_NO_TIMESLICE)
# endif
    const struct sysworkq_attr _system_workqueue_attr = {
        .prio = CONFIGURE_WORKQ_PRIORITY,
        .mode = CONFIGURE_WORKQ_TASKMODE,
        .stacksize = CONFIGURE_WORKQ_STACKSIZE
    };

    struct workqueue _system_workqueue[_CONFIGURE_MAXIMUM_PROCESSORS] BSP_FAST_DATA_SECTION;
#else
    extern struct workqueue _system_workqueue[];
    extern const struct sysworkq_attr _system_workqueue_attr;
#endif

#define _system_wq  (&_system_workqueue[rtems_scheduler_get_processor()])
#define WORK_INITIALIZER(_handler) \
    {{NULL, NULL}, _handler}

void work_init(struct work_struct *work, 
    void (*handler)(struct work_struct *));
void delayed_work_init(struct delayed_work *work,
    void (*handler)(struct work_struct *));
int schedule_work_to_queue(struct workqueue *wq, 
    struct work_struct *work);
int cancel_queue_work(struct workqueue *wq, struct work_struct *work, 
    bool wait);
int schedule_delayed_work_to_queue(struct workqueue *wq, 
    struct delayed_work *work, uint32_t ticks);
int cancel_queue_delayed_work(struct workqueue *wq, 
    struct delayed_work *work, bool wait);
int workqueue_create(struct workqueue *wq, int cpu_index, uint32_t prio, 
    size_t stksize, uint32_t modes, uint32_t attributes);
int workqueue_destory(struct workqueue *wq);
int workqueue_set_cpu_affinity(struct workqueue *wq, const cpu_set_t *affinity, 
    size_t size, uint32_t prio);


static inline struct delayed_work *to_delayed_work(struct work_struct *work) {
	return RTEMS_CONTAINER_OF(work, struct delayed_work, work);
}

static inline int schedule_work(struct work_struct *work) {
    return schedule_work_to_queue(_system_wq, work);
}

static inline int schedule_delayed_work(struct delayed_work *work, 
    uint32_t ticks) {
    return schedule_delayed_work_to_queue(_system_wq, work, ticks);
}

static inline int cancel_work_sync(struct work_struct *work) {
    return cancel_queue_work(_system_wq, work, true);
}

static inline int cancel_work_async(struct work_struct *work) {
    return cancel_queue_work(_system_wq, work, false);
}

static inline int cancel_delayed_work_sync(struct delayed_work *work) {
    return cancel_queue_delayed_work(_system_wq, work, true);
}

static inline int cancel_delayed_work(struct delayed_work *work) {
    return cancel_queue_delayed_work(_system_wq, work, false);
}

#ifdef __cplusplus
}
#endif
#endif /* BASE_WORKQUEUE_H_ */
