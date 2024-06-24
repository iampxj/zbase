/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<lvgl_encoder>: "fmt
// #define CONFIG_LOGLEVEL LOGLEVEL_DEBUG
#include <errno.h>
#include <lvgl.h>

#include "basework/generic.h"
#include "basework/log.h"
#include "basework/container/kfifo.h"
#include <msgbox_cache.h>
struct encoder_input {
	int16_t diff;
	uint16_t state;
};

static DEFINE_KFIFO(fifo, struct encoder_input, 64);
static lv_indev_t *indev_encoder;

int input_encoder_device_init(void);

static void lvgl_encoder_read_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
	struct encoder_input enc;

	if (kfifo_get(&fifo, &enc)) {
		data->continue_reading = !kfifo_is_empty(&fifo);
		data->enc_diff = enc.diff;
		data->state = enc.state;
		pr_dbg("## encoder read: diff(%d) state(%d)\n", enc.diff, enc.state);
	} else {
		data->continue_reading = false;
		data->enc_diff = 0;
	
		data->state = LV_INDEV_STATE_RELEASED;
	}
}

void lvgl_encoder_push(int diff, bool pressed) {
	struct encoder_input enc;

	if(msgbox_cache_num_popup_get())
		return;
	
	enc.diff = (int16_t)diff;
	enc.state = pressed? LV_INDEV_STATE_PRESSED: LV_INDEV_STATE_RELEASED;
	kfifo_put(&fifo, enc);
}

lv_indev_t *lvgl_indev_encoder_get(void) {
	return indev_encoder;
}

int lvgl_encoder_init(void) {
	static lv_indev_drv_t drv;

	lv_indev_drv_init(&drv);
	drv.type = LV_INDEV_TYPE_ENCODER;
	drv.read_cb = lvgl_encoder_read_cb;
	indev_encoder = lv_indev_drv_register(&drv);
	if (indev_encoder == NULL) {
		pr_err("Failed to register input device.\n");
		return -ENOMEM;
	}
	pr_dbg("Encoder input device intialized\n");
	return 0;
}

int input_encoder_device_init(void) __rte_alias(lvgl_encoder_init);