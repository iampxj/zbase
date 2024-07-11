/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <errno.h>
#include <lvgl.h>

#include "basework/generic.h"
#define __MON_DECLARE__
#include "basework/mon.h"
#include "src/core/lv_obj.h"


static lv_point_t origin_point = {
    .x = 10,
    .y = 300 / 2
};

static void sched_monitor_event(int code) {
    for (size_t i = 0; i < _mon_allocidx; i++) {
        const struct imon_class *task = _mon_tasks[i];
        if (task->cmd)
            task->cmd(code);
    }
}

static void sched_monitor_cb(struct _lv_timer_t* timer) {
    lv_obj_t* label = (lv_obj_t *)timer->user_data;
    int i = (int)lv_obj_get_user_data(label);
    const struct imon_class *task;

    task = _mon_tasks[i];
    if (task)
        task->show(label, (mon_show_t)lv_label_set_text_fmt, task->arg);
}

static void timer_delete_cb(lv_event_t* e) {
    lv_timer_t* timer = (lv_timer_t*)e->user_data;
    if (timer)
        lv_timer_del(timer);
}

static lv_obj_t *mon_text_create(lv_obj_t *parent, void (*refr_cb)(struct _lv_timer_t*), 
    uint32_t period, void *user_data) {
    lv_obj_t *label = lv_label_create(parent);
    if (label) {
        lv_timer_t* timer = lv_timer_create(refr_cb, period, label);
        if (timer == NULL) {
            lv_obj_del(label);
            return NULL;
        }
        lv_obj_set_user_data(label, user_data);
        lv_obj_add_event_cb(parent, timer_delete_cb,
            LV_EVENT_DELETE, timer);
        lv_obj_set_style_bg_color(label, lv_color_hex(0x17171c), LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_white(), 0);
        lv_obj_set_style_pad_top(label, 3, 0);
        lv_obj_set_style_pad_bottom(label, 3, 0);
        lv_obj_set_style_pad_left(label, 3, 0);
        lv_obj_set_style_pad_right(label, 3, 0);
        return label;
    }
    return NULL;
}

static void track_event_cb(lv_event_t *e) {
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t point;
    if (indev) {
        lv_obj_t *obj = lv_event_get_target(e);
        lv_indev_get_vect(indev, &point);
        lv_coord_t x = lv_obj_get_x(obj) + point.x;
        lv_coord_t y = lv_obj_get_y(obj) + point.y;
        origin_point.x = x;
        origin_point.y = y;
        lv_obj_set_pos(obj, x, y);
    }
}

static void panel_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    switch (code) {
    case LV_EVENT_PRESSING:
        track_event_cb(e);
        break;
    case LV_EVENT_CLICKED:
        sched_monitor_event(IMON_CLICK_EVT);
        break;
    default:
        break;
    }
}

int _monitor_pannel_init(void *display) {
    if (!display || !_mon_allocidx)
        return -EINVAL;

    lv_obj_t* parent = lv_disp_get_layer_top((lv_disp_t*)display);
    lv_obj_t* panel = lv_btn_create(parent);
    lv_obj_add_event_cb(panel, panel_event_cb,
        LV_EVENT_ALL, panel);
    lv_obj_set_size(panel, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_flex_cross_place(panel, LV_FLEX_ALIGN_START, 0);
    lv_obj_set_pos(panel, origin_point.x, origin_point.y);
    lv_obj_set_style_radius(panel, 5, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_50, 0);
    lv_obj_set_style_bg_color(panel, lv_color_white(), 0);

    for (int i = 0; i < (int)_mon_allocidx; i++) {
        if (!_mon_tasks[i])
            break;
        mon_text_create(panel, sched_monitor_cb, 
            _mon_tasks[i]->period, (void *)i);
    }
    return 0;
}

