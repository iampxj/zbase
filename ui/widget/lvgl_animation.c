/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 * Copyright (c) 2023 wtcat
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lvgl_animation.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include <assert.h>
#include <string.h>

#include "basework/ui/widget/lvgl_animation.h"

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS &animation_class

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void animation_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void animation_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj);

static void animation_exec_cb(void *obj, int32_t value);

static void prepare_anim(lv_obj_t *obj);
static bool load_image(lv_obj_t *obj, uint16_t index);

/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t animation_class = {
	.constructor_cb = animation_constructor,
	.destructor_cb = animation_destructor,
	.instance_size = sizeof(animation_t),
	.base_class = &lv_img_class,
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * animation_create(lv_obj_t * parent)
{
	LV_LOG_INFO("begin");
	lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
	lv_obj_class_init_obj(obj);
	return obj;
}

void animation_clean(lv_obj_t * obj)
{
	animation_t *animimg = (animation_t *)obj;

	/* stop current animation */
	animation_stop(obj);

	/* unload resource */
	if (animimg->src_dsc.data != NULL) {
		lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
		lvgl_img_loader_unload(&animimg->loader, animimg->src_idx, &animimg->src_dsc);
		animimg->src_dsc.data = NULL;
	}
}

/*=====================
 * Setter functions
 *====================*/

void animation_set_src(lv_obj_t *obj, const lvgl_img_loader_ops_t *fn, 
	void *user_data)
{
	animation_t *animimg = (animation_t *)obj;

	/* cleanup loader */
	animation_clean(obj);
	/* initialize loader */
	lvgl_img_loader_set(&animimg->loader, fn, user_data);
	/* prepare the animation */
	prepare_anim(obj);
}

void animation_set_duration(lv_obj_t *obj, uint16_t delay, uint16_t duration)
{
	animation_t *animimg = (animation_t *)obj;

	lv_anim_set_delay(&animimg->anim, delay);
	lv_anim_set_time(&animimg->anim, duration);
}

void animation_set_repeat(lv_obj_t *obj, uint16_t delay, uint16_t count)
{
	animation_t *animimg = (animation_t *)obj;

	lv_anim_set_repeat_delay(&animimg->anim, delay);
	lv_anim_set_repeat_count(&animimg->anim, count);
}

void animation_set_preload(lv_obj_t *obj, bool en)
{
	animation_t *animimg = (animation_t *)obj;

	animimg->preload_en = en;
}

void animation_set_ready_cb(lv_obj_t *obj, animation_ready_cb_t ready_cb, void *arg)
{
	animation_t *animimg = (animation_t *)obj;

	animimg->ready_cb = ready_cb;
	animimg->arg = arg;
}

/*=====================
 * Getter functions
 *====================*/

uint16_t animation_get_src_count(lv_obj_t *obj)
{
	animation_t *animimg = (animation_t *)obj;

	return animimg->src_count;
}

bool animation_get_running(lv_obj_t *obj)
{
	animation_t *animimg = (animation_t *)obj;

	return animimg->started;
}

/*=====================
 * Other functions
 *====================*/

void animation_play_static(lv_obj_t *obj, uint16_t idx)
{
	animation_t *animimg = (animation_t *)obj;

	if (animimg->started || !lvgl_img_loader_is_inited(&animimg->loader)) {
		return;
	}

	idx = LV_MIN(idx, animimg->src_count - 1);
	load_image(obj, idx);
}

void animation_start(lv_obj_t *obj, bool continued)
{
	animation_t *animimg = (animation_t *)obj;

	if (animimg->started || !lvgl_img_loader_is_inited(&animimg->loader)) {
		return;
	}

	if (continued && animimg->src_idx > 0) {
		animimg->start_idx = LV_MIN(animimg->src_idx, animimg->src_count - 1);
	} else {
		animimg->start_idx = 0;
	}

	animimg->started = 1;
	
	lv_anim_start(&animimg->anim);
}

void animation_set_value(lv_obj_t *obj, int32_t start, int32_t end)
{
	animation_t *animimg = (animation_t *)obj;

	lv_anim_set_values(&animimg->anim, start, LV_MIN(end, animimg->src_count - 1));
}

void animation_stop(lv_obj_t *obj)
{
	animation_t *animimg = (animation_t *)obj;

	if (animimg->started) {
		animimg->started = 0;
		lv_anim_del(NULL, animation_exec_cb);

		/* Destory loader */
		lvgl_img_loader_destroy(&animimg->loader);
	}
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void animation_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
	LV_UNUSED(class_p);
	LV_TRACE_OBJ_CREATE("begin");

	animation_t *animimg = (animation_t *)obj;

	animimg->src_idx = -1;
	animimg->preload_en = 1;
	lv_anim_init(&animimg->anim);
	lv_anim_set_early_apply(&animimg->anim, true);
	lv_anim_set_var(&animimg->anim, obj);
	lv_anim_set_exec_cb(&animimg->anim, animation_exec_cb);
	lv_anim_set_repeat_count(&animimg->anim, LV_ANIM_REPEAT_INFINITE);

	lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);

	LV_TRACE_OBJ_CREATE("finished");
}

static void animation_destructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
	animation_clean(obj);
}

static void prepare_anim(lv_obj_t *obj)
{
	animation_t *animimg = (animation_t *)obj;

	if (animimg->preload_en) {
		lvgl_img_loader_preload(&animimg->loader, 0);
	}

	animimg->src_idx = -1;
	animimg->src_count = lvgl_img_loader_get_count(&animimg->loader);
}

static bool load_image(lv_obj_t *obj, uint16_t index)
{
	animation_t *animimg = (animation_t *)obj;
	lv_point_t pos = { obj->coords.x1, obj->coords.y1 };
	lv_img_dsc_t next_src;

	if (animimg->src_idx == index && animimg->src_dsc.data != NULL)
		return true;

	if (lvgl_img_loader_load(&animimg->loader, index, &next_src, &pos) == 0) {

		animimg->src_idx = index;
		memcpy(&animimg->src_dsc, &next_src, sizeof(next_src));

		lv_img_cache_invalidate_src(&animimg->src_dsc);
		lv_img_set_src(obj, &animimg->src_dsc);
		if (lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN))
			lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
		//lv_obj_set_pos(obj, pos.x - obj->parent->coords.x1, pos.y - obj->parent->coords.y1);
		return true;
	}

	return false;
}

static void animation_exec_cb(void *var, int32_t value)
{
	lv_obj_t *obj = var;
	animation_t *animimg = (animation_t *)obj;

	/* value range can be exceed src_count - 1 */
	if (value >= animimg->src_count) {
		value -= animimg->src_count;
	}

	/* load current image */
	load_image(obj, value);

	/* preload next image */
	if (animimg->preload_en) {
		uint16_t preload_idx = value + 1;

		if (preload_idx >= animimg->src_count)
			preload_idx = 0;

		lvgl_img_loader_preload(&animimg->loader, preload_idx);
	}

	if (animimg->ready_cb && value == animimg->start_idx + animimg->src_count - 1) {
		animimg->ready_cb(obj, animimg->arg);
	}
}
