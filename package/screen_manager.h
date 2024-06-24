/*
 * Copyright 2023 wtcat
 */
#ifndef SCREEN_MANAGER_H_
#define SCREEN_MANAGER_H_

#include <stdbool.h>
#include <stdint.h>

#include "basework/container/observer.h"

#ifdef __cplusplus
extern "C"{
#endif

struct screen_param {
    uint16_t brightness;
    union {
        void *p;
        uintptr_t v;
    };
};

/*
 * screen manager notify
 */
enum screen_notif {
    SCREEN_NOTIF_ON,
    SCREEN_NOTIF_OFF,
    SCREEN_NOTIF_SLEEP,
    SCREEN_NOTIF_POST,
    SCREEN_NOTIF_REPAINT
};

enum screen_state {
    SCREEN_STATE_NORMAL,
    SCREEN_STATE_SLEEP,
    SCREEN_STATE_MIDDLE,
    SCREEN_STATE_KEEP,
    SCREEN_STATE_POST
};

/*
 * Screen manager state
 */
enum scrmgr_state {
    SCRMGR_STATE_NORMAL,
    SCRMGR_STATE_MIDSUPER,
    SCRMGR_STATE_SUPER,
};

struct scrmgr_ops {
    int  (*screen_set)(enum screen_state state, int brightness);
    void (*state_switch)(enum scrmgr_state curr, enum scrmgr_state next);

};

/*
 * Timeout time
 */
#define SCREEN_ON_FOREVER INT32_MAX
#define SCREEN_ON_DEFAULT 0

/*
 * is_screen_active - Whether the screen is active
 *
 * return true if it is active state
 */
bool is_screen_active(void);

/*
 * is_screen_sleep - Whether the screen is sleep
 *
 * return true if it is sleep state
 */
bool is_screen_sleep(void);

/*
 * is_screen_faded - Whether the screen is faded
 *
 * return true if it is midsleep state
 */
bool is_screen_faded(void);

/*
 * screen_active - Active screen display
 *
 * @sec: screen on timeout in seconds
 * return 0 if success
 */
int screen_active(unsigned int sec);

/*
 * screen_deactive - Deactive screen display
 *
 * return 0 if success
 */
int screen_deactive(void);

/*
 * screen_get_activetime - Get the time of screen activation
 *
 * return milliseconds
 */
uint32_t screen_get_activetime(void);

/*
 * screen_super_enter - Keep on screen active
 *
 * return 0 if success
 */
int screen_super_enter(void);

/*
 * screen_super_enter - Keep on screen active but enter light-sleep when timeout 
 *
 * return 0 if success
 */
int screen_midsuper_enter(void);

/*
 * screen_super_exit - Resore normal state from super mode
 *
 * return 0 if success
 */
int screen_super_exit(void);

/*
 * screen_midsuper_exit - Resore normal state from midsuper mode
 *
 * return 0 if success
 */
int screen_midsuper_exit(void);

/*
 * screen_manager_set - Enable manager post process
 *
 * @post_active: enable post-event
 * return true if success
 */
void screen_manager_set(bool post_active);

/*
 * screen_manager_post - Process screen post event
 * @arg: pointer to screen parameter
 * @force: forced post
 *
 * return true if success
 */
bool screen_manager_post(struct screen_param *sp, bool force);
bool screen_manager_post_locked(struct screen_param *sp, bool force);

/*
 * screen_manager_post_active - Test whether screen-manager can post event
 *
 * return true if okay
 */
bool screen_manager_post_active(void);

/*
 * screen_manager_enable - Enable screen manager
 *
 * @en: enable flag
 */
void screen_manager_enable(bool en);

/*
 * screen_manager_init - Initialize screen manager
 *
 * return 0 if success
 */
int screen_manager_init(void);

/*
 * screen_manager_notify - Screen manager notify
 * screen_manager_notify_locked - Screen manager notify (without lock)
 *
 * @value: screen state
 * @sp: pointer to screen parameter
 *
 * return 0 if success
 */
int screen_manager_notify(unsigned long value, struct screen_param *sp);
int screen_manager_notify_locked(unsigned long value, struct screen_param *sp);

/*
 * screen_manager_add_observer - Register screen manager observer
 *
 * return 0 if success
 */
int screen_manager_add_observer(struct observer_base *obs);

/*
 * screen_manager_update_time - Update screen default time parameters
 *
 * @on_delay: the time of screen active in seconds
 * return 0 if success
 */
int screen_manager_update_time(unsigned int on_delay);

/*
 * screen_manager_set_brightness - Set brightness of the screen  
 *
 * @brightness: screen brightness
 * @active_scr: active screen
 * return 0 if success
 */
int screen_manager_set_brightness(unsigned int brightness, bool active_scr);

/*
 * screen_manager_refresh_brightness - Refresh the brightness of the screen and not save  
 *
 * @brightness: screen brightness
 * return 0 if success
 */
int screen_manager_refresh_brightness(unsigned int brightness);

/*
 * screen_register_hwops - Register screen device operations
 *
 * @hwops: device operations
 * return 0 if success
 */
int screen_manager_register_ops(const struct scrmgr_ops *ops);

/*
 * screen_manager_get_state - Get current state of the screen manager
 *
 * return sceen manager state
 */
enum scrmgr_state screen_manager_get_state(void);

#ifdef __cplusplus
}
#endif
#endif /* SCREEN_MANAGER_H_ */
