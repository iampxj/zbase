/*
 * Copyright 2023 wtcat
 */
#ifndef BASEWORK_MON_H_
#define BASEWORK_MON_H_

#include <stddef.h>
#include "basework/errno.h"

#ifdef __cplusplus
extern "C"{
#endif

#define MON_TASK_MAX 10

typedef void (*mon_show_t)(void *h, const char *, ...);

struct imon_class {
#define IMON_CLICK_EVT 1
    void (*show)(void *h, mon_show_t cb, void *arg);
    void (*cmd)(int code);
    void *arg;
    unsigned int period;
};

#ifndef __MON_DECLARE__
# define __MON_DECLARE__ extern
#else
# define _MON_DECLARE
#endif

__MON_DECLARE__ const struct imon_class *_mon_tasks[MON_TASK_MAX];
__MON_DECLARE__ size_t _mon_allocidx;

static inline int monitor_register(const struct imon_class *mon) {
    if (!mon || !mon->show)
        return -EINVAL;
    if (_mon_allocidx >= MON_TASK_MAX)
        return -ENOMEM;
    _mon_tasks[_mon_allocidx++] = mon;
    return 0;
}

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_MON_H_ */
