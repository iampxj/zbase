/*
 * Copyright 2023 wtcat
 */
#include <lvgl.h>

#include "basework/log.h"
#include "basework/ui/lvgl_encoder.h"
#include "app_ui_view.h"

static void lvgl_encoder_scroll_cb(lv_event_t* e) {
	lv_obj_t* obj = lv_event_get_target(e);
	lv_indev_data_t* data = lv_event_get_param(e);
	int diff = (int)data->enc_diff;
	
#ifdef CONFIG_SIMULATOR
	diff *= 200;
#endif
	lvgl_encoder_motor_follow_handle(e,false);
	if (data->enc_diff > 0) {
		lv_coord_t bottom = lv_obj_get_scroll_bottom(obj);
		if (bottom > 0) {
			if (diff > bottom)
				diff = bottom;
			lv_obj_scroll_by(obj, 0, -diff, LV_ANIM_OFF);
			pr_dbg("## %s: down diff(%d)\n", __func__, diff);
		} else {
			pr_dbg("## %s: down drop (diff: %d,  bottom: %d)\n", __func__, diff, bottom);
		}
	} else if (data->enc_diff < 0) {
		lv_coord_t top = lv_obj_get_scroll_top(obj);
		if (top > 0) {
			if (LV_ABS(diff) > top)
				diff = 0 - top;
			lv_obj_scroll_by(obj, 0, -diff, LV_ANIM_OFF);
			pr_dbg("## %s: up diff(%d)\n", __func__, diff);
		} else {
			pr_dbg("## %s: up drop (diff: %d,  top: %d)\n", __func__, diff, top);
		}
	}
}

lv_group_t *lvgl_encoder_attach(lv_indev_t* indev, lv_obj_t *obj, 
	lv_event_cb_t cb, void *arg) {
	lv_group_t *group = lv_group_create();

	lv_group_add_obj(group, obj);
	lv_indev_set_group(indev, group);
	if (cb) {
		lv_obj_remove_event_cb(obj, cb);
		lv_obj_add_event_cb(obj, cb, LV_EVENT_KEY, arg);
	} else {
		lv_obj_remove_event_cb(obj, lvgl_encoder_scroll_cb);
		lv_obj_add_event_cb(obj, lvgl_encoder_scroll_cb, LV_EVENT_KEY, obj);
	}
	return group;
}

void lvgl_encoder_detach(lv_indev_t* indev, lv_group_t *g) {
	if (indev->group == g)
		lv_indev_set_group(indev, NULL);
	if (g)
		lv_group_del(g);
}
