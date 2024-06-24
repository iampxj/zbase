/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 * Copyright (c) 2023 wtcat
 
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file animation.h
 *
 */

#ifndef BASEWORK_UI_WIDGET_ANIMATION_H_
#define BASEWORK_UI_WIDGET_ANIMATION_H_

#ifdef __cplusplus
extern "C" {
#endif

/*********************
 *      INCLUDES
 *********************/
#include <lvgl.h>

#include "basework/ui/widget/lvgl_img_loader.h"


/*********************
 *      DEFINES
 *********************/

/**********************
 *      TYPEDEFS
 **********************/

/** Callback used when the animation is ready or repeat ready. */
typedef void (*animation_ready_cb_t)(lv_obj_t *, void *arg);

/** Data of anim image */
typedef struct {
	lv_img_t img;

	lv_anim_t anim;
	uint8_t started : 1;
	uint8_t preload_en : 1;

	int16_t start_idx; /* anim start src index */
	int16_t src_idx;   /* src index */
	int16_t src_count; /* src count */
	lv_img_dsc_t src_dsc;

	/* resource loader */
	lvgl_img_loader_t loader;

	animation_ready_cb_t ready_cb;
	void *arg;
} animation_t;

extern const lv_obj_class_t animation_class;

/**********************
 * GLOBAL PROTOTYPES
 **********************/

/**
 * Create an animated image object
 * @param parent pointer to an object, it will be the parent of the new object
 * @return pointer to the created animated image object
 */
lv_obj_t *animation_create(lv_obj_t *parent);

/**
 * Clean the image frame resource
 *
 * @param obj pointer to an image animation object
 */
void animation_clean(lv_obj_t *obj);

/*=====================
 * Setter functions
 *====================*/

/**
 * Set the resource image array where to get pixel map array
 *
 * @param obj pointer to an image animation object
 * @param ops image resource operations
 * @param user_data position array of the images
 */
void animation_set_src(lv_obj_t *obj, const lvgl_img_loader_ops_t *ops, void *user_data);

/**
 * Set a delay and duration the animation
 *
 * @param obj pointer to an image animation object
 * @param delay delay before the animation in milliseconds
 * @param duration duration of the animation in milliseconds
 */
void animation_set_duration(lv_obj_t *obj, uint16_t delay, uint16_t duration);

/**
 * Make the animation repeat itself.
 *
 * @param obj pointer to an image animation object
 * @param delay delay delay in milliseconds before starting the playback animation.
 * @param count repeat count or `LV_ANIM_REPEAT_INFINITE` for infinite repetition. 0: to disable repetition.
 */
void animation_set_repeat(lv_obj_t *obj, uint16_t delay, uint16_t count);

/**
 * Enable/disable the preload during the animation.
 *
 * The preload is default enabled.
 *
 * @param obj pointer to an image animation object
 * @param en enabled or not.
 */
void animation_set_preload(lv_obj_t *obj, bool en);

/**
 * Set a function call when the animation is ready.
 *
 * @param obj pointer to an image animation object
 * @param ready_cb  a function call when the animation is ready or repeat ready
 * @param arg function parameter
 */
void animation_set_ready_cb(lv_obj_t *obj, animation_ready_cb_t ready_cb, void *arg);

/*=====================
 * Getter functions
 *====================*/

/**
 * Get the image count
 *
 * @param obj pointer to an image animation object
 * @return the image count
 */
uint16_t animation_get_src_count(lv_obj_t *obj);

/**
 * Get whether the animation is running
 *
 * @param obj pointer to an image animation object
 * @return true: running; false: not running
 */
bool animation_get_running(lv_obj_t *obj);

/*=====================
 * Other functions
 *====================*/

/**
 * Play one frame of the animation
 *
 * The animation must not be running.
 *
 * @param obj pointer to an image animation object
 * @param idx the frame index to play
 */
void animation_play_static(lv_obj_t *obj, uint16_t idx);

/**
 * Start the animation
 *
 * @param obj pointer to an image animation object
 * @param continued true: continue the last played frame
 *                  false: restart from the first frame
 */
void animation_start(lv_obj_t *obj, bool continued);

/**
 * Stop the animation
 *
 * @param obj pointer to an image animation object
 * @return the current play image index
 */
void animation_stop(lv_obj_t *obj);

/**
 * Set the start and end values of an animation
 * @param obj       pointer to an initialized `animation_t` variable
 * @param start     the start value
 * @param end       the end value
 */
void animation_set_value(lv_obj_t* obj, int32_t start, int32_t end);

/**********************
 *      MACROS
 **********************/

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* BASEWORK_UI_WIDGET_ANIMATION_H_ */
