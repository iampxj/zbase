/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <init.h>

#include "basework/rq.h"
#include "basework/mon.h"

extern size_t mon_memres_get_used(void);
#ifdef CONFIG_DISPLAY_COMPOSER_DEBUG_FPS
extern int display_composer_get_fps(void);
#endif

static size_t _max_mem_usage;

static void rq_show(void *obj, mon_show_t show, void *arg) {
	show(obj, "sch: %d", _rq_get_nodes(_system_rq));
    (void) arg;
}

static void mem_show(void *obj, mon_show_t show, void *arg) {
    size_t percent = mon_memres_get_used() * 100 / CONFIG_UI_RES_MEM_POOL_SIZE;
    if (_max_mem_usage < percent)
        _max_mem_usage = percent;
    show(obj, "ui_mem(%d%%) max(%d%%)", percent, _max_mem_usage);
    (void) arg;
}

static void mem_cmd(int code) {
    if (code == IMON_CLICK_EVT)
        _max_mem_usage = 0;
}

#ifdef CONFIG_DISPLAY_COMPOSER_DEBUG_FPS
static void fps_show(void *obj, mon_show_t show, void *arg) {
	show(obj, "fps: %d", display_composer_get_fps());
    (void) arg;
}
#endif

static struct imon_class mem_mon_class = {
    .show = mem_show,
    .cmd = mem_cmd,
    .arg = NULL,
    .period = 1000
};

#ifdef CONFIG_DISPLAY_COMPOSER_DEBUG_FPS
static struct imon_class fps_mon_class = {
    .show = fps_show,
    .arg = NULL,
    .period = 500
};
#endif /* CONFIG_DISPLAY_COMPOSER_DEBUG_FPS */

static struct imon_class rq_mon_class = {
    .show = rq_show,
    .arg = NULL,
    .period = 500
};

static int _monitor_init(const struct device *device) {
    monitor_register(&mem_mon_class);
#ifdef CONFIG_DISPLAY_COMPOSER_DEBUG_FPS
    monitor_register(&fps_mon_class);
#endif
    monitor_register(&rq_mon_class);
    (void) device;
    return 0;
}

SYS_INIT(_monitor_init, APPLICATION, 
    CONFIG_KERNEL_INIT_PRIORITY_DEFAULT);
