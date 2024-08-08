/*
 * Copyright 2023 wtcat
 *
 * Normal Sleep Poweroff
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<screen_manager>: "fmt
#define CONFIG_LOGLEVEL LOGLEVEL_WARNING
#include "basework/os/osapi.h"
#include "basework/system.h"
#include "basework/log.h"
#include "basework/package/screen_manager.h"


#define MILLI_SEC(n) ((n) * 1000)
#define SCREEN_ON_DELAY    CONFIG_SCREEN_ACTIVE_TIMEOUT
#define SCREEN_SLEEP_DELAY CONFIG_SCREEN_STANDBY_TIMEOUT
#define SCREEN_BRIGHTNESS_MIN 25

struct screen_context;
struct screen_operations {
    enum scrmgr_state state;
    int (*active)(struct screen_context *sc, int sec);
    int (*deactive)(struct screen_context *sc);
};

struct screen_context {
    os_mutex_t lock;
    const struct screen_operations *state;
    const struct scrmgr_ops *sm_ops;
    struct observer_base *obs_list;
    os_timer_t timer;
    unsigned int on_delay;
    unsigned int sleep_delay;
    uint32_t on_timestamp;
    uint16_t brightness;
    uint16_t midsuper_pending : 1;
    uint16_t midsuper_sleep : 1;
    uint16_t post_active : 1;
    uint16_t initialized : 1;
    struct screen_eventsrc source;
    bool disabled;
};

#define IS_STATE_EQUEL(_sc, _state) \
    ((_sc)->state == &(_state))
#define IS_STATE_NEQUEL(_sc, _state) \
    ((_sc)->state != &(_state))
#define NEXT_STATE(_sc, _state) \
do {\
    (_sc)->sm_ops->state_switch((_sc)->state->state, (_state).state); \
    (_sc)->state = &(_state); \
} while (0)

#define SCREEN_STATE_SET(_sc, _sta) \
    (_sc)->sm_ops->screen_set(&(_sc)->source, _sta, (uint8_t)(_sc)->brightness)

#define MTX_INIT()    os_mtx_init(&sc->lock, 0)
#define MTX_LOCK()    os_mtx_lock(&sc->lock) 
#define MTX_UNLOCK()  os_mtx_unlock(&sc->lock) 

static int normal_state_active(struct screen_context *sc, int sec);
static int normal_state_deactive(struct screen_context *sc);
static int sleep_state_active(struct screen_context *sc, int sec);
static int sleep_state_deactive(struct screen_context *sc);
static int poff_state_active(struct screen_context *sc, int sec);
static int poff_state_deactive(struct screen_context *sc);
static int super_state_active(struct screen_context *sc, int sec);
static int super_state_deactive(struct screen_context *sc);
static int midsuper_state_active(struct screen_context *sc, int sec);
static int midsuper_state_deactive(struct screen_context *sc);

static const struct screen_operations midsuper_state = {
    .state = SCRMGR_STATE_MIDSUPER,
    .active = midsuper_state_active,
    .deactive = midsuper_state_deactive
};
static const struct screen_operations super_state = {
    .state = SCRMGR_STATE_SUPER,
    .active = super_state_active,
    .deactive = super_state_deactive
};
static const struct screen_operations normal_state = {
    .state = SCRMGR_STATE_NORMAL,
    .active = normal_state_active,
    .deactive = normal_state_deactive
};
static const struct screen_operations sleep_state = {
    .state = SCRMGR_STATE_NORMAL,
    .active = sleep_state_active,
    .deactive = sleep_state_deactive
};
static const struct screen_operations poff_state = {
    .state = SCRMGR_STATE_NORMAL,
    .active = poff_state_active,
    .deactive = poff_state_deactive
};

static struct screen_context screen_context = {
    .state = &normal_state,
    .on_delay = SCREEN_ON_DELAY,
    .sleep_delay = SCREEN_SLEEP_DELAY,
    .post_active = false
};

static void screen_on_timeout_mid2(os_timer_t timer, void *arg) {
    struct screen_context *sc = arg;
    (void) timer;
    if (rte_unlikely(sc->disabled))
        return;
    SCREEN_STATE_SET(sc, SCREEN_STATE_POST);
}

static void screen_on_timeout_mid(os_timer_t timer, void *arg) {
    struct screen_context *sc = arg;
	int err;
    (void) timer;

    MTX_LOCK();
    if (rte_unlikely(sc->disabled))
        goto _unlock;
    sc->source.deactive_src = SCREEN_EVSRC_TIMER;
    err = SCREEN_STATE_SET(sc, SCREEN_STATE_MIDDLE);
    if (!err) {
        sc->midsuper_sleep = true;
        os_timer_update_handler(sc->timer, screen_on_timeout_mid2, sc);
        os_timer_mod(sc->timer, MILLI_SEC(sc->sleep_delay));
    } else {
        pr_err("enter midsuper mode failed(%d)\n", err);
    }
	
_unlock:
    MTX_UNLOCK();
    pr_dbg("timeout: { midsuper }\n");
}

static void screen_on_timeout(os_timer_t timer, void *arg) {
    struct screen_context *sc = arg;
	int err;
    (void) timer;
    
    MTX_LOCK();
    if (rte_unlikely(sc->disabled))
        goto _unlock;
    sc->source.deactive_src = SCREEN_EVSRC_TIMER;
    err = SCREEN_STATE_SET(sc, SCREEN_STATE_SLEEP);
    if (!err)
        NEXT_STATE(sc, sleep_state);
	
_unlock:
    MTX_UNLOCK();
    pr_dbg("timeout { X -> sleep }\n");
}

static int normal_state_active(struct screen_context *sc, int sec) {
    int err = sys_screen_up(sec + sc->sleep_delay);
    if (!err) {
        SCREEN_STATE_SET(sc, SCREEN_STATE_NORMAL);
        os_timer_mod(sc->timer, MILLI_SEC(sec));
        NEXT_STATE(sc, normal_state);
        pr_dbg("screen on with %d seconds\n", sec);
        return 0;
    }
    pr_err("screen on with error(%d)\n", err);
    return err;
}

static int normal_state_deactive(struct screen_context *sc) {
    int err = sys_screen_up(sc->sleep_delay);
    if (!err) {
        SCREEN_STATE_SET(sc, SCREEN_STATE_SLEEP);
        os_timer_del(sc->timer);
        NEXT_STATE(sc, sleep_state);
    }
    pr_dbg("screen off {normal -> sleep} with error(%d) caller(%p)\n", 
        err, __builtin_return_address(0));
    return 0;
}

static int sleep_state_active(struct screen_context *sc, int sec) {
    pr_dbg("screen on {sleep -> normal}\n");
    return normal_state_active(sc, sec);
}

static int sleep_state_deactive(struct screen_context *sc) {
    (void) sc;
    pr_dbg("screen off {sleep -> poweroff}\n");
    NEXT_STATE(sc, poff_state);
    return 0;
}

static int poff_state_active(struct screen_context *sc, int sec) {
    pr_dbg("screen on {poweroff -> normal}\n");
    return normal_state_active(sc, sec);
}

static int poff_state_deactive(struct screen_context *sc) {
    (void) sc;
    pr_dbg("screen off {poweroff -> sleep}\n");
    return 0;
}

static int super_state_active(struct screen_context *sc, int sec) {
    (void) sec;
    pr_dbg("screen on {supper -> supper}\n");
    SCREEN_STATE_SET(sc, SCREEN_STATE_KEEP);
    return 0;
}

static int super_state_deactive(struct screen_context *sc) {
    (void) sc;
    pr_dbg("screen off {supper -> supper}\n");
    return 0;
}

static void screen_timer_reset(struct screen_context *sc,
    void (*fn)(os_timer_t, void *), long expires) {
    os_timer_del(sc->timer);
    os_timer_update_handler(sc->timer, fn, sc);
    os_timer_mod(sc->timer, expires);
}

static int midsuper_state_active(struct screen_context *sc, int sec) {
    (void) sec;
    sc->midsuper_sleep = false;
    SCREEN_STATE_SET(sc, SCREEN_STATE_NORMAL);
    screen_timer_reset(sc, screen_on_timeout_mid, MILLI_SEC(sc->on_delay));
    pr_dbg("screen on {supper -> supper}\n");
    return 0;
}

static int midsuper_state_deactive(struct screen_context *sc) {
    (void) sc;
    sc->midsuper_sleep = true;
    SCREEN_STATE_SET(sc, SCREEN_STATE_MIDDLE);
    screen_timer_reset(sc, screen_on_timeout_mid2, MILLI_SEC(sc->sleep_delay));
    pr_dbg("screen off {supper -> supper}\n");
    return 0;
}

static int screen_midsuper_enter_locked(struct screen_context *sc) {
    int err = sys_screen_up(SCREEN_ON_FOREVER);
    if (!err) {
        SCREEN_STATE_SET(sc, SCREEN_STATE_NORMAL);
        screen_timer_reset(sc, screen_on_timeout_mid, MILLI_SEC(sc->on_delay));
        NEXT_STATE(sc, midsuper_state);
        pr_dbg("screen enter mid-super mode { X -> midsuper }\n");
    } 
    return err;
}

bool is_screen_active(void) {
    struct screen_context *sc = &screen_context;
    return sc->state == &normal_state ||
            sc->state == &super_state || 
            (sc->state == &midsuper_state && !sc->midsuper_sleep);
}

bool is_screen_sleep(void) {
    struct screen_context *sc = &screen_context;
    return sc->state == &sleep_state ||
        (sc->state == &midsuper_state && sc->midsuper_sleep);
}

bool is_screen_faded(void) {
    struct screen_context *sc = &screen_context;
    return sc->state == &midsuper_state && 
        sc->midsuper_sleep;
}

int __screen_active(unsigned int sec, unsigned int source) {
    struct screen_context *sc = &screen_context;
    int delay, err = -EBUSY;

    MTX_LOCK();
    if (rte_unlikely(sc->disabled))
        goto _unlock;
    delay = sec? sec: sc->on_delay;
    sc->source.active_src = (uint8_t)source;
    err = sc->state->active(sc, delay);
    if (!err)
        sc->on_timestamp = sys_uptime_get();
    pr_dbg("screen active with time(%d s) sec(%d) caller(%p)\n", 
        delay, sec, __builtin_return_address(0));
_unlock:
    MTX_UNLOCK();
    return err;
}

int __screen_deactive(unsigned int source) {
    struct screen_context *sc = &screen_context;
    int err = -EBUSY;

    MTX_LOCK();
    if (rte_unlikely(sc->disabled))
        goto _unlock;
    sc->source.deactive_src = (uint8_t)source;
    err = sc->state->deactive(sc);
    pr_dbg("screen deactive with caller(%p)\n", 
        __builtin_return_address(0));
_unlock:
    MTX_UNLOCK();
    return err;
}

int screen_super_enter(void) {
    struct screen_context *sc = &screen_context;

    MTX_LOCK();
    int err = sys_screen_up(SCREEN_ON_FOREVER);
    if (!err && !IS_STATE_EQUEL(sc, super_state)) {
        SCREEN_STATE_SET(sc, SCREEN_STATE_NORMAL);
        os_timer_del(sc->timer);
        NEXT_STATE(sc, super_state);
        pr_dbg("screen enter super mode { X -> super }\n");
    }
    MTX_UNLOCK();
    return err;
}

int screen_super_exit(void) {
    struct screen_context *sc = &screen_context;
    int err = 0;

    MTX_LOCK();
    if (IS_STATE_EQUEL(sc, super_state)) {
        if (sc->midsuper_pending) {
            err = screen_midsuper_enter_locked(sc);
            goto _unlock;
        }
        
        err = sys_screen_up(sc->on_delay + sc->sleep_delay);
        if (!err) {
            os_timer_mod(sc->timer, MILLI_SEC(sc->on_delay));
            NEXT_STATE(sc, normal_state);
            pr_dbg("screen exit super mode {super/midsuper -> normal}\n");
        }
    }

_unlock:
    MTX_UNLOCK();
    return err;
}

int screen_midsuper_enter(void) {
    struct screen_context *sc = &screen_context;
    int err = 0;

    MTX_LOCK();
    if (IS_STATE_EQUEL(sc, super_state)) {
        sc->midsuper_pending = 1;
        goto _unlock;
    }

    if (!sc->midsuper_pending) {
        err = screen_midsuper_enter_locked(sc);
        if (!err)
            sc->midsuper_pending = 1;
    }
_unlock:
    MTX_UNLOCK();
    return err;
}

int screen_midsuper_exit(void) {
    struct screen_context *sc = &screen_context;
    int err = 0;

    MTX_LOCK();
    if (IS_STATE_EQUEL(sc, midsuper_state)) {
        err = sys_screen_up(sc->on_delay + sc->sleep_delay);
        if (!err) {
            sc->midsuper_pending = 0;
            os_timer_del(sc->timer);
            os_timer_update_handler(sc->timer, screen_on_timeout, sc);
            os_timer_mod(sc->timer, MILLI_SEC(sc->on_delay));
            NEXT_STATE(sc, normal_state);
            pr_dbg("screen exit super mode {super/midsuper -> normal}\n");
        }
    } else if (IS_STATE_EQUEL(sc, super_state)) {
        sc->midsuper_pending = 0;
    }
    MTX_UNLOCK();
    return err;
}

uint32_t screen_get_activetime(void) {
    struct screen_context *sc = &screen_context;
    
    return (uint32_t)((long)sys_uptime_get() - (long)sc->on_timestamp);
}

int screen_manager_update_time(unsigned int on_delay) {
    struct screen_context *sc = &screen_context;

    if (!on_delay)
        return -EINVAL;
    MTX_LOCK();
    sc->on_delay = on_delay;
    pr_dbg("set on_delay(%d)\n", on_delay);
    MTX_UNLOCK();
    return 0;
}

int screen_manager_refresh_brightness(unsigned int brightness) {
    struct screen_context *sc = &screen_context;
    int err;

    MTX_LOCK();
    err = sc->sm_ops->screen_set(&sc->source, SCREEN_STATE_KEEP, 
        (int)brightness);
    MTX_UNLOCK();
    return err;
}

int screen_manager_set_brightness(unsigned int brightness, bool active_scr) {
    struct screen_context *sc = &screen_context;
    
    if (brightness < SCREEN_BRIGHTNESS_MIN) 
        sc->brightness = SCREEN_BRIGHTNESS_MIN;
    else
        sc->brightness = (uint16_t)brightness;
    pr_dbg("set current screen brightness: %d\n", (int)sc->brightness);

    if (active_scr)
        return screen_active(SCREEN_ON_DEFAULT);
    return 0;
}

void screen_manager_set(bool post_active) {
    struct screen_context *sc = &screen_context;

    MTX_LOCK();
    sc->post_active = post_active;
    MTX_UNLOCK();
    pr_dbg("screen manager set post-attr(%d)\n", (int)sc->post_active);
}

void screen_manager_enable(bool en) {
    struct screen_context *sc = &screen_context;

    MTX_LOCK();
    sc->disabled = !en;
    MTX_UNLOCK();
}

bool screen_manager_post_active(void) {
    struct screen_context *sc = &screen_context;

    return sc->post_active;
}

bool screen_manager_post_locked(struct screen_param *sp, bool force) {
    struct screen_context *sc = &screen_context;
    bool okay = false;

    if (sc->post_active || force) {
        screen_manager_notify_locked(SCREEN_NOTIF_POST, sp);
        okay = true;
        pr_dbg("screen manager post\n");
    }
    return okay;
}

bool screen_manager_post(struct screen_param *sp, bool force) {
    struct screen_context *sc = &screen_context;

    MTX_LOCK();
    bool okay = screen_manager_post_locked(sp, force);
    MTX_UNLOCK();
    return okay;
}

int screen_manager_init(void) {
    struct screen_context *sc = &screen_context;
    int err = 0;

    if (!sc->initialized) {
        sc->initialized = true;
        MTX_INIT();
         
        err = os_timer_create(&sc->timer, screen_on_timeout, 
            sc, false);
        if (err) {
            pr_err("Create timer failed(%d)\n", err);
            return err;
        }
        pr_dbg("screen manager initialized\n");
    }
    return err;
}

int screen_manager_notify_locked(unsigned long value, struct screen_param *sp) {
    struct screen_context *sc = &screen_context;
    return observer_notify(&sc->obs_list, value, sp);
}

int screen_manager_notify(unsigned long value, struct screen_param *sp) {
    struct screen_context *sc = &screen_context;

    MTX_LOCK();
    int err = observer_notify(&sc->obs_list, value, sp);
    MTX_UNLOCK();
    return err;
}

int screen_manager_add_observer(struct observer_base *obs) {
    struct screen_context *sc = &screen_context;
    int err;

    err = screen_manager_init();
    if (err)
        return err;
    
    MTX_LOCK();
    err = observer_cond_register(&sc->obs_list, obs);
    MTX_UNLOCK();
    return err;
}

int screen_manager_register_ops(const struct scrmgr_ops *ops) {
    if (ops) {
        screen_context.sm_ops = ops;
        return 0;
    }
    return -EINVAL;
}

enum scrmgr_state screen_manager_get_state(void) {
    struct screen_context *sc = &screen_context;
    enum scrmgr_state state;

    MTX_LOCK();
    state = sc->state->state;
    MTX_UNLOCK();

    return state;
}
