/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 * Copyright (c) 2023 wtcat
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file lvgl_vertical_imglabel.c
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include <sys/util.h>
#include "lvgl_vertical_imglabel.h"

/*********************
 *      DEFINES
 *********************/
#define MY_CLASS &lvgl_vertical_imglabel_class

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lvgl_vertical_imglabel_release(lvgl_vertical_imglabel_t *label, int i);
static void lvgl_vertical_imglabel_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj);
static void lvgl_vertical_imglabel_destructor(const struct _lv_obj_class_t * class_p, struct _lv_obj_t * obj);
static void lvgl_vertical_imglabel_event(const lv_obj_class_t * class_p, lv_event_t * e);
static void draw_label(lv_event_t * e);

static void refresh_area(lv_obj_t * obj);
static void refresh_text(lv_obj_t * obj);
static void refresh_text_nocache(lv_obj_t * obj);

/**********************
 *  STATIC VARIABLES
 **********************/
const lv_obj_class_t lvgl_vertical_imglabel_class = {
	.constructor_cb = lvgl_vertical_imglabel_constructor,
	.destructor_cb = lvgl_vertical_imglabel_destructor,
	.event_cb = lvgl_vertical_imglabel_event,
	.width_def = LV_SIZE_CONTENT,
	.height_def = LV_SIZE_CONTENT,
	.instance_size = sizeof(lvgl_vertical_imglabel_t),
	.base_class = &lv_obj_class
};

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

lv_obj_t * lvgl_vertical_imglabel_create(lv_obj_t * parent)
{
	LV_LOG_INFO("begin");
	lv_obj_t * obj = lv_obj_class_create_obj(MY_CLASS, parent);
	lv_obj_class_init_obj(obj);
	return obj;
}

void lvgl_vertical_imglabel_set_align(lv_obj_t *obj, lv_align_t align)
{
	lvgl_vertical_imglabel_t * label = (lvgl_vertical_imglabel_t *)obj;

	if (align > LV_ALIGN_RIGHT_MID)
		return;

	if (label->align != align) {
		label->align = align;
		label->refresh(obj);
	}
}

void lvgl_vertical_imglabel_set_height(lv_obj_t *obj, uint16_t height)
{
	lvgl_vertical_imglabel_t * label = (lvgl_vertical_imglabel_t *)obj;
	label->height = height;
}

void lvgl_vertical_imglabel_set_src(lv_obj_t *obj, const lv_img_dsc_t * chars, uint8_t cnt)
{
	lvgl_vertical_imglabel_t * label = (lvgl_vertical_imglabel_t *)obj;

	label->src_chars = chars;
	label->num_chars = cnt;
	label->refresh = refresh_text;
	label->refresh(obj);
}

void lvgl_vertical_imglabel_set_src_nocache(lv_obj_t *obj, const lv_img_dsc_t * chars, uint8_t cnt,
	const imglabel_loader_t *ops)
{
	lvgl_vertical_imglabel_t * label = (lvgl_vertical_imglabel_t *)obj;

	label->loader = *ops;
	label->src_chars = chars;
	label->num_chars = cnt;
	label->refresh = refresh_text_nocache;
	label->refresh(obj);
}

void lvgl_vertical_imglabel_set_text(lv_obj_t * obj, const uint8_t * indices, uint8_t cnt)
{
	lvgl_vertical_imglabel_t * label = (lvgl_vertical_imglabel_t *)obj;
	uint8_t new_count = LV_MIN(cnt, IMG_VERTICAL_LABEL_MAX_COUNT);

	//Unload picture resource
	if (label->loader.ops) {
		for (int i = 0, j; i < (int)label->count; i++) {
			for (j = 0; j < new_count; j++) {
				if (label->indices[i] == indices[j])
					break;
			}
			if (j == new_count)
				lvgl_vertical_imglabel_release(label, i);
		}
	}
	label->count = new_count;
	memcpy(label->indices, indices, sizeof(*indices) * label->count);
	label->refresh(obj);
}

uint32_t lvgl_vertical_imglabel_get_size(lv_obj_t *obj)
{
	lvgl_vertical_imglabel_t * label = (lvgl_vertical_imglabel_t *)obj;

	return label->count;
}

void lvgl_vertical_imglabel_split_src(lv_obj_t *obj, lv_img_dsc_t * chars,
		const lv_img_dsc_t * src, uint8_t cnt, bool vertical)
{
	uint32_t data_size = src->data_size / cnt;
	uint16_t w = vertical ? src->header.w : (src->header.w / cnt);
	uint16_t h = vertical ? (src->header.h / cnt) : src->header.h;
	const uint8_t *data = src->data;

	for (int i = 0; i < cnt; i++) {
		chars[i].header.cf = src->header.cf;
		chars[i].header.always_zero = 0;
		chars[i].header.w = w;
		chars[i].header.h = h;
		chars[i].data_size = data_size;
		chars[i].data = data;

		data += data_size;
	}
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

static void lvgl_vertical_imglabel_constructor(const lv_obj_class_t * class_p, lv_obj_t * obj)
{
	LV_UNUSED(class_p);
	LV_TRACE_OBJ_CREATE("begin");

	lvgl_vertical_imglabel_t *label = (lvgl_vertical_imglabel_t *)obj;

	lv_obj_clear_flag(obj, LV_OBJ_FLAG_CLICKABLE);
	label->align = LV_ALIGN_CENTER;

	LV_TRACE_OBJ_CREATE("finished");
}

static void lvgl_vertical_imglabel_destructor(const struct _lv_obj_class_t * class_p, struct _lv_obj_t * obj) 
{
	LV_UNUSED(class_p);

	lvgl_vertical_imglabel_t *label = (lvgl_vertical_imglabel_t *)obj;

	if (label->loader.ops) {
		for (int i = 0; i < label->count; i++)
			lvgl_vertical_imglabel_release(label, i);
	}
}

static void lvgl_vertical_imglabel_release(lvgl_vertical_imglabel_t *label, int i)
{
	int no = label->indices[i];
	lv_img_dsc_t *src = (void *)&label->src_chars[no];
	if (src->data) {
		label->loader.ops->unload(&label->loader, no);
		src->data = NULL;
	}
}

static void lvgl_vertical_imglabel_event(const lv_obj_class_t * class_p, lv_event_t * e)
{
	LV_UNUSED(class_p);

	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * obj = lv_event_get_target(e);
	lvgl_vertical_imglabel_t *label = (lvgl_vertical_imglabel_t *)obj;

	/*Ancestor events will be called during drawing*/
	if (code != LV_EVENT_COVER_CHECK && code != LV_EVENT_DRAW_MAIN && code != LV_EVENT_DRAW_POST) {
		/*Call the ancestor's event handler*/
		lv_res_t res = lv_obj_event_base(MY_CLASS, e);
		if(res != LV_RES_OK) return;
	}

	if (code == LV_EVENT_SIZE_CHANGED) {
		lv_area_t *ori = lv_event_get_param(e);

		if (lv_area_get_width(ori) != lv_obj_get_width(obj) ||
			lv_area_get_height(ori) != lv_obj_get_height(obj)) {
			refresh_area(obj);
		}
	} else if (code == LV_EVENT_GET_SELF_SIZE) {
		lv_point_t * p = lv_event_get_param(e);
		p->x = lv_area_get_width(&label->area);
		p->y = lv_area_get_height(&label->area);
	} else if (code == LV_EVENT_DRAW_MAIN || code == LV_EVENT_DRAW_POST || code == LV_EVENT_COVER_CHECK) {
		draw_label(e);
	}
}

static void draw_label(lv_event_t * e)
{
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t * obj = lv_event_get_target(e);
	lvgl_vertical_imglabel_t *label = (lvgl_vertical_imglabel_t *)obj;
	lv_area_t abs_area;

	lv_area_set(&abs_area,
		label->area.x1 + obj->coords.x1, label->area.y1 + obj->coords.y1,
		label->area.x2 + obj->coords.x1, label->area.y2 + obj->coords.y1);

	if (code == LV_EVENT_COVER_CHECK) {
		lv_cover_check_info_t * info = lv_event_get_param(e);
		if (info->res == LV_COVER_RES_MASKED) return;

		if (label->src_chars == NULL ) {
			info->res = LV_COVER_RES_NOT_COVER;
			return;
		}

		if (label->src_chars[0].header.cf != LV_IMG_CF_TRUE_COLOR && label->src_chars[0].header.cf != LV_IMG_CF_RAW) {
			info->res = LV_COVER_RES_NOT_COVER;
			return;
		}

		if (_lv_area_is_in(info->area, &abs_area, 0) == false) {
			info->res = LV_COVER_RES_NOT_COVER;
			return;
		}

		info->res = LV_COVER_RES_COVER;
	} else if (code == LV_EVENT_DRAW_MAIN) {
		if (label->src_chars == NULL)
			return;

		lv_draw_ctx_t * draw_ctx = lv_event_get_draw_ctx(e);
		lv_area_t clip, area;

		if (_lv_area_intersect(&clip, &obj->coords, draw_ctx->clip_area) == false)
			return;

		lv_draw_img_dsc_t img_dsc;
		lv_draw_img_dsc_init(&img_dsc);
		img_dsc.opa = lv_obj_get_style_img_opa(obj, LV_PART_MAIN);
		img_dsc.recolor = lv_obj_get_style_img_recolor(obj, LV_PART_MAIN);
		img_dsc.recolor_opa = lv_obj_get_style_img_recolor_opa(obj, LV_PART_MAIN);

		

		if(label->align == LV_ALIGN_TOP_RIGHT)
		{
			area.y1 =(lv_coord_t)( abs_area.y1 + label->height - (label->src_chars[label->indices[0]].header.h) * (label->count)) ;
			for (int i = 0; i < label->count; i++) {
				if (label->indices[i] >= label->num_chars)
					continue;

				const lv_img_dsc_t* src = &label->src_chars[label->indices[i]];
				area.y2 = (lv_coord_t)(area.y1  +  src->header.h - 1);
				/* align to the middle of area */
				area.x1 = (lv_coord_t)((abs_area.x1 + abs_area.x2 + 1 - src->header.w) / 2);
				area.x2 = (lv_coord_t)(area.x1 + src->header.w - 1);
				lv_draw_img(draw_ctx, &img_dsc, &area, src);

				area.y1 =(lv_coord_t)(area.y1+ src->header.h);
			}
		}
		else
		{
			area.y1 = abs_area.y1;
			for (int i = 0; i < label->count; i++) {
				if (label->indices[i] >= label->num_chars)
					continue;

				const lv_img_dsc_t* src = &label->src_chars[label->indices[i]];
				area.y2 = (lv_coord_t)(area.y1 + src->header.h - 1);
				/* align to the middle of area */
				area.x1 = (lv_coord_t)((abs_area.x1 + abs_area.x2 + 1 - src->header.w) / 2);
				area.x2 =(lv_coord_t)( area.x1 + src->header.w - 1);

				lv_draw_img(draw_ctx, &img_dsc, &area, src);

				area.y1 = (lv_coord_t)area.y1+src->header.h;
			}
		}
	}
}

static void refresh_area(lv_obj_t * obj)
{
	lvgl_vertical_imglabel_t *label = (lvgl_vertical_imglabel_t *)obj;

	//lv_area_align(&obj->coords, &label->area, label->align, 0, 0);
	lv_area_move(&label->area, -obj->coords.x1, -obj->coords.y1);
}

static void ll_refresh_text(lv_obj_t * obj, lv_coord_t w, lv_coord_t h)
{
	lvgl_vertical_imglabel_t *label = (lvgl_vertical_imglabel_t *)obj;
	lv_area_t abs_area;

	lv_area_set(&abs_area,
		label->area.x1 + obj->coords.x1, label->area.y1 + obj->coords.y1,
		label->area.x2 + obj->coords.x1, label->area.y2 + obj->coords.y1);
	lv_obj_invalidate_area(obj, &abs_area);

	lv_area_set(&label->area, 0, 0, w - 1, h - 1);

	lv_area_align(&obj->coords, &label->area, LV_ALIGN_TOP_LEFT, 0, 0);
	lv_area_move(&label->area, -obj->coords.x1, -obj->coords.y1);

	lv_area_set(&abs_area,
		label->area.x1 + obj->coords.x1, label->area.y1 + obj->coords.y1,
		label->area.x2 + obj->coords.x1, label->area.y2 + obj->coords.y1);

	lv_obj_refresh_self_size(obj);
	lv_obj_invalidate_area(obj, &abs_area);
}

static void refresh_text(lv_obj_t * obj)
{
	lvgl_vertical_imglabel_t *label = (lvgl_vertical_imglabel_t *)obj;
	lv_coord_t w = 0, h = 0;

	if (label->num_chars == 0 || label->count == 0) {
		return;
	}

	//label->same_height = 1;

	for (int i = 0; i < label->count; i++) {
		if (label->indices[i] >= label->num_chars)
			continue;

		const lv_img_dsc_t *src = &label->src_chars[label->indices[i]];

		/*if (w > 0 && w != src->header.w)
			label->same_height = 0;*/

		h += src->header.h;
		w = LV_MAX(w, src->header.w);
	}

	ll_refresh_text(obj, w, h);
}

static void refresh_text_nocache(lv_obj_t * obj)
{
	lvgl_vertical_imglabel_t *label = (lvgl_vertical_imglabel_t *)obj;
	lv_coord_t w = 0, h = 0;

	if (label->num_chars == 0 || label->count == 0) {
		return;
	}

	//label->same_height = 1;

	for (int i = 0; i < label->count; i++) {
		uint8_t no = label->indices[i];
		if (no >= label->num_chars)
			continue;

		const lv_img_dsc_t *src = &label->src_chars[no];
		if (!src->data) {
			//Load picture resource
			if (label->loader.ops->load(&label->loader, no, (void *)src))
				return;
		}

		/*if (h > 0 && h != src->header.h)
			label->same_height = 0;*/

		h += src->header.h;
		w = LV_MAX(w, src->header.w);
	}

	ll_refresh_text(obj, w, h);
}

#if 0
/*
 * split a decimal integer into digits
 *
 * @param value decimal integer value
 * @param buf buffer to store the digits
 * @param len buffer len
 * @param min_w minimum split digits (may pad 0 if integer too small)
 *
 * @retval number of split digits
 */
static int split_decimal(uint32_t value, uint8_t *buf, int len, int min_w)
{
	uint32_t tmp_value = value;
	int n_digits = 0;

	/* compute digit count of value */
	do {
		n_digits++;
		tmp_value /= 10;
	} while (tmp_value > 0);

	/* return digit count if buf not available */
	if (buf == NULL || len <= 0) {
		return n_digits;
	}

	/* compute filled digit count */
	if (n_digits >= len) {
		n_digits = len;
	}

	if (min_w >= len) {
		min_w = len;
	} else {
		len = MAX(n_digits, min_w);
	}

	/* fill digits of value */
	buf += len - 1;
	min_w -= n_digits;

	for (; n_digits > 0; n_digits--) {
		*buf-- = value % 10;
		value /= 10;
	}

	/* pad zero */
	for (; min_w > 0; min_w--) {
		*buf-- = 0;
	}

	return len;
}
#endif