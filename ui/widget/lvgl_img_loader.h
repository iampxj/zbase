/*
 * Copyright (c) 2020 Actions Technology Co., Ltd
 * Copyright (c) 2023
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief LVGL image loader interface
 */

#ifndef BASEWORK_UI_WIDGET_LVGL_IMG_LOADER_H_
#define BASEWORK_UI_WIDGET_LVGL_IMG_LOADER_H_

/**
 * @defgroup lvgl_image_apis LVGL Image Loader APIs
 * @ingroup lvgl_apis
 * @{
 */
#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

struct lvgl_img_loader;

typedef struct lvgl_img_loader_ops {
	/* load a image */
	int (*load)(struct lvgl_img_loader * loader, uint16_t index, 
		lv_img_dsc_t *dsc, lv_point_t * pos);
	/* (optional) unload a image */
	void (*unload)(struct lvgl_img_loader * loader, uint16_t index, 
		lv_img_dsc_t *dsc);
	/* (optional) preload a image */
	void (*preload)(struct lvgl_img_loader * loader, uint16_t index);
	/* get image count */
	uint16_t (*get_count)(struct lvgl_img_loader * loader);

	/* free all resources */
	void (*destroy)(struct lvgl_img_loader * loader);
} lvgl_img_loader_ops_t;

typedef struct lvgl_img_loader {
	const lvgl_img_loader_ops_t *i_ops;
	union {
		void *user_data;
	};

	void *r;
} lvgl_img_loader_t;


/**
 * Set the custom resource
 *
 * @param loader pointer to structure lvgl_img_loader
 * @param user_data custom data
 *
 * @retval 0 on success else negative code.
 */
static inline int 
lvgl_img_loader_set(lvgl_img_loader_t *loader,
	const lvgl_img_loader_ops_t *ops, void *user_data)
{
	memset(loader, 0, sizeof(*loader));
	loader->i_ops = ops;
	loader->user_data = user_data;
	return 0;
}

/**
 * Query the loader is initialized or not
 *
 * @param loader pointer to structure lvgl_img_loader
 *
 * @retval image count
 */
static inline bool 
lvgl_img_loader_is_inited(lvgl_img_loader_t *loader)
{
    return loader->i_ops != NULL;
}

/**
 * Get image count
 *
 * @param loader pointer to structure lvgl_img_loader
 *
 * @retval image count
 */
static inline uint16_t 
lvgl_img_loader_get_count(lvgl_img_loader_t *loader)
{
	return loader->i_ops->get_count(loader);
}

/**
 * load an image count
 *
 * @param loader pointer to structure lvgl_img_loader
 * @param index image index to load
 * @param dsc pointer to structure lv_img_dsc_t to store the image
 * @param pos pointer to structure lv_point_t to store the image position
 *
 * @retval 0 on success else negative code
 */
static inline int 
lvgl_img_loader_load(lvgl_img_loader_t *loader, uint16_t index, 
	lv_img_dsc_t *dsc, lv_point_t *pos)
{
	return loader->i_ops->load(loader, index, dsc, pos);		
}

/**
 * Unload an image count
 *
 * @param loader pointer to structure lvgl_img_loader
 * @param index image index to unload
 * @param dsc pointer to an image dsc returned by load()
 *
 * @retval N/A
 */
static inline void 
lvgl_img_loader_unload(lvgl_img_loader_t *loader, uint16_t index,
	lv_img_dsc_t *dsc)
{
	if (loader->i_ops->unload)
		loader->i_ops->unload(loader, index, dsc);		
}

/**
 * Preload an image count
 *
 * @param loader pointer to structure lvgl_img_loader
 * @param index image index to preload
 *
 * @retval N/A
 */
static inline void 
lvgl_img_loader_preload(lvgl_img_loader_t *loader, 
	uint16_t index)
{
	if (loader->i_ops->preload)
		loader->i_ops->preload(loader, index);		
}

/**
 * Destroy image loader
 *
 * @param loader pointer to structure lvgl_img_loader
 *
 * @retval N/A
 */
static inline void 
lvgl_img_loader_destroy(lvgl_img_loader_t *loader)
{
	if (loader->i_ops && loader->i_ops->destroy)
		loader->i_ops->destroy(loader);		
}

#ifdef __cplusplus
}
#endif
/**
 * @}
 */

#endif /* BASEWORK_UI_WIDGET_LVGL_IMG_LOADER_H_ */
