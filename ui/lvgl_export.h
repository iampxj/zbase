/*
 * Copyright 2023 wtcat
 */
#ifndef BASEWORK_UI_LVGL_EXPORT_H_
#define BASEWORK_UI_LVGL_EXPORT_H_

#include <lvgl.h>

#ifdef __cplusplus
extern "C"{
#endif

/*
 * lvgl_encoder_attach - Attach encoder device to lvgl-object
 *
 * @indev: encoder device
 * @obj: target object
 * @cb: encoder input event callback(use default if it is null)
 * @arg: user argument
 * return group object if success
 */
lv_group_t* lvgl_encoder_attach(lv_indev_t* indev, lv_obj_t* obj, 
    lv_event_cb_t cb, void* arg);

/*
 * lvgl_encoder_detach - Detach encoder device from lvgl-object
 *
 * @indev: encoder device
 * @g: group object
 */
void lvgl_encoder_detach(lv_indev_t* indev, lv_group_t* g);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_UI_LVGL_EXPORT_H_ */
