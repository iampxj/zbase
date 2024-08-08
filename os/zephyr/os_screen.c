/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#ifdef CONFIG_PM_DEVICE
#define pr_fmt(fmt) "<os_screen>: "fmt
#define CONFIG_LOGLEVEL LOGLEVEL_WARNING
#include <sys_standby.h>
#include <device.h>
#include <display/display_composer.h>
#include <drivers/display.h>
#include <drivers/input/input_dev.h>
#include <board_cfg.h>

#include "basework/log.h"
#include "basework/os/osapi_timer.h"
#include "basework/package/screen_manager.h"
#include "basework/system.h"


#define MIDDLE_BRIGHTNESS CONFIG_SCREEN_STANDBY_BRIGHTNESS
#define STANDBY_TIMEOUT   (CONFIG_SCREEN_STANDBY_TIMEOUT * 1000)

/* Wait for view_delete operation to complete */
#define STADNBY_EXTRA_DELAY  CONFIG_SCREEN_POST_DELAY
#define ZERO_BRIGHTNESS  0
#define USE_SCREEN_COVER 1

static os_timer_t timer;
static const struct device *screen_dev, *tp_dev;
static enum screen_state last_state;
static bool midsuper_on;
#ifdef CONFIG_FADE_LOW_POWER
static bool te_need_restore;
static bool time_update;
static bool simte_active;
#endif
static atomic_t screen_pending;
static uint16_t mid_brightness = MIDDLE_BRIGHTNESS;
#ifdef CONFIG_OLED_PROTECT
static uint32_t sleep_timestamp;
#endif

#if USE_SCREEN_COVER
static bool m_cover_status;
#endif

#ifdef CONFIG_FADE_LOW_POWER

static bool is_fade(void) {
    if (IS_ENABLED(CONFIG_TFT_FADE))
        return false;
        
    return is_screen_faded();
}

int soc_get_aod_mode(void) {
	return is_fade();
}

static void wakeup_prepare_cb(void) {
    pr_dbg("time update\n");
    time_update = true;
}

static void sleep_prepare_cb(void) {
    if (time_update)
        time_update = false;
}

static void s1_enter_prepare(void) {
//    while (display_composer_control(0, "disp_busy") == 1) {
//        pr_info("## display composer is busy!\n");
//        k_msleep(10);
//    }
}

static bool s1_need_exit(void) {
//    return display_composer_control(0, "disp_busy") == 1;
    return false;
}

static void s2_enter_cb(void) {
   if (te_need_restore) {
		if (simte_active) {
			simte_active = false;
			
			/* Waiting for dma-2d work to complete */
			// k_msleep(50);
		}
	}
}

static void s2_exit_cb(void) {
//   if (te_need_restore)
//       display_composer_control(0, "simteon");
}

static struct standby_operations standby_ops = {
    .wakeup = wakeup_prepare_cb,
    .sleep = sleep_prepare_cb,
    .s1_need_exit = s1_need_exit,
    .s1_enter_prepare = s1_enter_prepare,
    .s2_enter_after = s2_enter_cb,
    .s2_exit_before = s2_exit_cb,
    .is_fade = is_fade
};

#ifdef CONFIG_OLED_PROTECT
static bool is_sleep_timeout(void) {
    uint32_t diff = os_timer_gettime() - sleep_timestamp;
    return diff >= (2 * 3600 * 1000);
}
#endif /* CONFIG_OLED_PROTECT */
#endif /* CONFIG_FADE_LOW_POWER */

static void timer_cb(os_timer_t timer, void *arg) {
    (void) arg;
#if USE_SCREEN_COVER
	if (!m_cover_status) {
    	pr_dbg("##tp active screen\n");
    	screen_active_ext(SCREEN_ON_DEFAULT, SCREEN_EVSRC_TP);
	} else {
		m_cover_status = false;
		//screen_deactive();
	}
    atomic_set(&screen_pending, 0);	
    pr_dbg("tp notify => ACTIVE\n");
#else
    pr_dbg("tp notify => ACTIVE\n");
    atomic_set(&screen_pending, 0);
    screen_active(SCREEN_ON_DEFAULT);
#endif
}

static void tp_notify(struct device *dev, struct input_value *val) {
    (void) dev;
    (void) val;
#if USE_SCREEN_COVER
	if (val->point.gesture == 0xAA) {
		system_standby_ioevent_post();
		m_cover_status = true;
	}
#endif
#ifdef CONFIG_FADE_LOW_POWER
    if (time_update && te_need_restore)
        return;
#endif
    if (atomic_cas(&screen_pending, 0, 1))
        os_timer_mod(timer, 50);
}

static void tpscr_pm_control(bool lowerpower, uint8_t wksrc) {
    if (!midsuper_on)
        return;
		
    if (lowerpower) {
        // display_composer_control(0, "idle");
        input_dev_control(tp_dev, 0, "idle");
    } else {
        // display_composer_control(0, "exit");
        input_dev_control(tp_dev, 0, "exit");
        if (wksrc != 0)
            input_dev_control(tp_dev, wksrc, NULL);
    }
}

static int screen_sleep_set(const struct screen_eventsrc *src, 
    enum screen_state state, uint8_t brightness) {
    struct screen_param sp;

    sp.brightness = brightness;
    switch (state) {
    case SCREEN_STATE_NORMAL:
        pr_dbg("normal state\n");
        if (last_state != SCREEN_STATE_NORMAL) {
#if defined(CONFIG_FADE_LOW_POWER) && defined(CONFIG_OLED_PROTECT)
            sp.v = midsuper_on? (uintptr_t)is_sleep_timeout(): 0;
            sleep_timestamp = os_timer_gettime();
#endif
            screen_manager_notify_locked(SCREEN_NOTIF_ON, &sp);
            tpscr_pm_control(false, src->active_src);
        }
#ifdef CONFIG_FADE_LOW_POWER
        if (midsuper_on) {
            time_update = false;

            /* Restore auto-standby time to avoid screen flicker */
            system_standby_set_fade(false, UINT32_MAX);

            /* Reset wakeup-lock idle time */
            sys_screen_up(0);
        }
#endif
        last_state = state;
        break;
				
    case SCREEN_STATE_MIDDLE:
        pr_dbg("middle state\n");
        sp.brightness = mid_brightness; //TODO: 
        if (last_state == SCREEN_STATE_NORMAL) {
#ifdef CONFIG_FADE_LOW_POWER
            /* We must change the standby time in order to go to sleep*/
            system_standby_set_fade(true, 
                STANDBY_TIMEOUT + STADNBY_EXTRA_DELAY);
#endif
            tpscr_pm_control(true, src->deactive_src);
            screen_manager_notify_locked(SCREEN_NOTIF_OFF, &sp);
            mid_brightness = sp.brightness;
        }
        last_state = state;
        break;
				
    case SCREEN_STATE_SLEEP:
        pr_dbg("sleep state\n");
        sp.brightness = ZERO_BRIGHTNESS;
        if (last_state != SCREEN_STATE_SLEEP)
            screen_manager_notify_locked(SCREEN_NOTIF_SLEEP, NULL);
        last_state = state;
        break;
				
    case SCREEN_STATE_KEEP:
        sp.brightness = brightness > ZERO_BRIGHTNESS? (uint8_t)brightness: ZERO_BRIGHTNESS;
        break;
		
    case SCREEN_STATE_POST:
        screen_manager_post_locked(&sp, true);
        pr_dbg("screen sleep post { midsuper }\n");
        break;
		
    default:
        return -EINVAL;
    }
    
#ifdef CONFIG_FADE_LOW_POWER
    if (time_update)
        return 0;
#endif

    pr_dbg("screen_sleep_set brightness(%d)\n", 
        (int)sp.brightness);
    return display_set_brightness(screen_dev, sp.brightness);
}

static void state_switch(enum scrmgr_state curr, enum scrmgr_state next) {
    if (curr == next)
        return;
    if (next == SCRMGR_STATE_MIDSUPER) {
#ifdef CONFIG_FADE_LOW_POWER
        /* 
         * When the system switchs to midsuper, 
         * we need to install hook 
         */
        system_standby_register_ops(&standby_ops);
#endif
        midsuper_on = true;
        return;
    }
    if (curr == SCRMGR_STATE_MIDSUPER) {
#ifdef CONFIG_FADE_LOW_POWER
        system_standby_register_ops(NULL);
#endif
        tpscr_pm_control(false, 0);
        midsuper_on = false;
        return;
    }
}

static int __rte_notrace screen_pm_init(const struct device *dev) {
    (void) dev;
    static const struct scrmgr_ops ops = {
        .screen_set = screen_sleep_set,
        .state_switch = state_switch
    };

	tp_dev = (struct device *)device_get_binding(CONFIG_TPKEY_DEV_NAME);
	if (!tp_dev) {
		pr_err("cannot found key dev tpkey\n");
		return -ENODEV;
	}

    screen_dev = device_get_binding(CONFIG_LCD_DISPLAY_DEV_NAME);
    if (!screen_dev) {
        pr_err("cannot found lcd device (%s)\n", CONFIG_LCD_DISPLAY_DEV_NAME);
        return -ENODEV;
    }

    int err = os_timer_create(&timer, timer_cb, 
        NULL, false);
    if (err) {
        pr_err("%s: Create timer failed\n", __func__);
        return err;
    }

    screen_manager_init();
    input_dev_register_notify(tp_dev, tp_notify);
    err = screen_manager_register_ops(&ops);
    pr_dbg("screen pm initialized (%d)\n", err);

#ifdef CONFIG_OS_TIMER_TRACER
    os_timer_trace(timer, true);
#endif
    return err;
}

static int screen_pm_control(const struct device *dev, 
    enum pm_device_action action) {
	switch (action) {
	case PM_DEVICE_ACTION_EARLY_SUSPEND:
    // case PM_DEVICE_ACTION_TURN_OFF:
        screen_deactive();
        screen_manager_post_locked(NULL, false);
#ifdef CONFIG_FADE_LOW_POWER
        if (!IS_ENABLED(CONFIG_TFT_FADE) && is_screen_faded()) {
#ifdef CONFIG_OLED_PROTECT
            sleep_timestamp = os_timer_gettime();
#endif
            time_update = false;
            te_need_restore = true;
            // display_composer_control(0, "teoff");
        } else 
            sys_s3_wksrc_clr(SLEEP_WK_SRC_GPIO);
#endif /* CONFIG_FADE_LOW_POWER */
        pr_dbg("screen_pm_control: suspend action(%d)\n", action);
		break;
	case PM_DEVICE_ACTION_LATE_RESUME:
#ifdef CONFIG_FADE_LOW_POWER
        if (te_need_restore) {
            te_need_restore = false;
			simte_active = true;
			// display_composer_control(0, "teon");
            if (time_update) {
                struct screen_param sp = {0};
#ifdef CONFIG_OLED_PROTECT
                sp.v = is_sleep_timeout();
#endif
                screen_manager_notify_locked(SCREEN_NOTIF_REPAINT, &sp);
                system_standby_set_fade(false, 2000);
                break;
            } 
        }
#endif /* CONFIG_FADE_LOW_POWER */
		screen_active(SCREEN_ON_DEFAULT);
        pr_dbg("screen_pm_control: resume\n");
		break;
	default:
		break;
	}
	return 0;
}

DEVICE_DEFINE(screen_dev, "screen", screen_pm_init,
			screen_pm_control, NULL, NULL, POST_KERNEL,
			61, NULL);
#endif /* CONFIG_PM_DEVICE */
