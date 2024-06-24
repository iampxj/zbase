/*
 * Copyright 2023 wtcat
 */
#ifndef MSG_VIEW_PRESENTER_H_
#define MSG_VIEW_PRESENTER_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "app_ui_view.h"
#include "middleware/user_data/app_clock/app_clock.h"

#ifdef __cplusplus
extern "C" {
#endif

struct message_content {
#define UI_MAX_MESSAGE_SIZE 256
    uint32_t timestamp;
	uint16_t id;
    uint16_t type;
    uint16_t c_len;
    char contact[32];
    uint16_t m_len;
    char buffer[UI_MAX_MESSAGE_SIZE];

	/* user extention */
	void *widget;
	char stime[32];
	bool cache_hit;
};

struct message_presenter {
#ifdef CONFIG_SIMULATOR
	app_ui_auto_t (*get_view_status_data)(void);
	uint8_t (*get_view_status)(void);
#endif
	size_t (*get_size)(void);
	int (*get_first)(void);
	int (*get_next)(int idx);
	int (*clear)(void);
	int (*remove)(int idx);
	int (*read)(int idx, struct message_content *mc);
	int (*get_newidx)(void);
	int (*convert_type)(int type);
	int (*subscribe)(int viewid);
	int (*unsubscribe)(int viewid);
	size_t (*max_limit)(void);
	int (*convert_time)(uint32_t timestamp, char* buffer, size_t maxlen);
	int (*get_enter_flag)(void);
	void (*set_enter_flag)(int);
	void (*set_read_status)(void);
	uint32_t (*convert_utctime)(uint32_t timestamp, app_clock_t *time);
    void (*return_old_view)(void);
	bool (*get_bt_status)(void);
	bool (*remind_keep_timer_creat)(uint32_t);
	void (*remind_keep_timer_del)(void);
};

static inline size_t 
ui_message_get_size(const struct message_presenter *p) {
	if (p && p->get_size)
		return p->get_size();
	return 0;
}

static inline int
ui_message_get_first(const struct message_presenter *p) {
	if (p && p->get_first)
		return p->get_first();
	return -1;
}

static inline int
ui_message_get_next(const struct message_presenter *p, int idx) {
	if (p && p->get_next)
		return p->get_next(idx);
	return -1;
}

static inline int
ui_message_clear(const struct message_presenter *p) {
	if (p && p->clear)
		return p->clear();
	return -1;
}

static inline int
ui_message_remove(const struct message_presenter *p, int idx) {
	if (p && p->remove)
		return p->remove(idx);
	return -1;
}

static inline int
ui_message_read(const struct message_presenter *p, int idx,
	struct message_content *mc) {
	if (p && p->read)
		return p->read(idx, mc);
	return -1;
}

static inline int
ui_message_get_newidx(const struct message_presenter* p) {
	if (p && p->get_newidx)
		return p->get_newidx();
	return -1;
}

static inline int
ui_message_convert_type(const struct message_presenter* p, int in_type) {
	if (p && p->convert_type)
		return p->convert_type(in_type);
	return in_type;
}

static inline int
ui_message_subscribe(const struct message_presenter* p, 
	int view, bool enable) {
	if (p) {
		if (enable && p->subscribe)
			return p->subscribe(view);
		if (!enable && p->unsubscribe)
			return p->unsubscribe(view); 
	}
	return -1;
}

static inline size_t
ui_message_max_limit(const struct message_presenter* p) {
	if (p && p->max_limit)
		return p->max_limit();
	return 100;
}

static inline size_t
ui_message_convert_time(const struct message_presenter* p,
	uint32_t timestamp, char* buffer, size_t maxlen) {
	if (p && p->convert_time)
		return p->convert_time(timestamp, buffer, maxlen);
	return -1;
}

void message_view_ui_enter(void);
	
#ifdef __cplusplus
}
#endif
#endif /* MSG_VIEW_PRESENTER_H_ */
