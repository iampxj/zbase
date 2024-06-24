/*
 * Copyright 2023 wtcat
 */
#ifndef BASEWORK_UI_LVGL_ENCODER_H_
#define BASEWORK_UI_LVGL_ENCODER_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

// struct _lv_indev_t;
typedef struct _lv_indev_t lv_indev_t;

/*
 * lvgl_encoder_push - Push encoder state to fifo
 *
 * @diff: encoder difference
 * @pressed: press state
 */
void lvgl_encoder_push(int diff, bool pressed);

/*
 * lvgl_indev_encoder_get - Get input encoder device for lvgl
 *
 * return dev pointer if success
 */
lv_indev_t *lvgl_indev_encoder_get(void);

/*
 * lvgl_encoder_init - Register encoder device for lvgl
 *
 * return 0 if success
 */
int lvgl_encoder_init(void);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_UI_LVGL_ENCODER_H_ */
