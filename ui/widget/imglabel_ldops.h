/*
 * 
 */
#ifndef BASEWORK_UI_WIDGET_IMGLABEL_LOADER_OPS_H_
#define BASEWORK_UI_WIDGET_IMGLABEL_LOADER_OPS_H_

#ifdef __cplusplus
extern "C" {
#endif

struct imglabel_loader;

typedef struct {
	int (*load)(struct imglabel_loader *loader, int no, void* dsc);
	int (*unload)(struct imglabel_loader *loader, int no);
} imglabel_loadops_t;

typedef struct imglabel_loader {
	const imglabel_loadops_t *ops;
	void *context;
} imglabel_loader_t;

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_UI_WIDGET_IMGLABEL_LOADER_OPS_H_ */