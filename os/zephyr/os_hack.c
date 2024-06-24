/*
 * Copyright 2023 wtcat
 */
#define CONFIG_LOGLEVEL   LOGLEVEL_INFO
#define pr_fmt(fmt) "<os_hack>: "fmt
#include <string.h>

#include <init.h>
#include <kernel.h>
#include <os_common_api.h>
#include <ui_manager.h>

#include "basework/os/osapi_timer.h"
#include "basework/log.h"


#if 0
struct thrd_node {
	const char *name;
	struct k_thread *thrd;
};

extern void z_thread_install_hook(void (*hook)(struct k_thread *, uint32_t));
extern int system_standby_get(void);

static uint32_t _last_switched_out;
static struct thrd_node _ui_thread = {
	.name = "ui_service"
};

static void _ui_thread_switch_monitor(struct k_thread *curr, uint32_t now) {
	if (unlikely(!_ui_thread.thrd)) {
		if (strcmp(_ui_thread.name, curr->name))
			return;

		_ui_thread.thrd = curr;
	}
	if (curr != _ui_thread.thrd) {

		/* If we run in S1 state, reset time */
		if (system_standby_get() == 1) {
			_last_switched_out = os_timer_gettime();
			return;
		}

		uint32_t diff_ms = (uint32_t)(os_timer_gettime() - _last_switched_out);
		/* 
		 * If the ui-service is not running more than 5 seconds than
		 * we throw a exception
		 */
		if (diff_ms > 5000) {
			__ASSERT_NO_MSG(0);
		}

	} else 
		_last_switched_out = os_timer_gettime();
}

int _os_hack_init(void) {
	z_thread_install_hook(_ui_thread_switch_monitor);
	return 0;
}

#else

static int uisrv_ackcnt, uisrv_reqcnt;
static os_timer_t mon_timer;
static tid_node_t uisrv_handle = {
	.name = "ui_service"
};

static void uisrv_ack(struct app_msg *msg, int cmd, void *arg) {
	(void) msg;
	(void) arg;
	(void) cmd;
	uisrv_ackcnt++;
	pr_dbg("uisrv_ack: %d\n", uisrv_ackcnt);
}

static bool uisrv_heartick_request(void) {
	struct app_msg msg = {
		.type = MSG_UI_EVENT,
		.cmd = 0,
		.callback = uisrv_ack,
	};

	if (uisrv_reqcnt - uisrv_ackcnt > 3) {
		pr_err("Some errors has occurred with ui-service!\n");
		k_panic();
		return -2;
	}
	int err = msg_manager_send_async_msg_tid(&uisrv_handle, &msg);
	if (err == -ESRCH) {
		pr_dbg("UI service has been killed\n");
		return false;
	}

	uisrv_reqcnt += !err;
	pr_dbg("uisrv_heartick_request: %d\n", uisrv_reqcnt);
	return true;
}

static void uisrv_monitor_timercb(os_timer_t timer, void *arg) {
	(void)arg;
	if (uisrv_heartick_request())
		os_timer_mod(timer, 2000);
}

int _os_hack_init(void) {
	os_timer_create(&mon_timer, uisrv_monitor_timercb, NULL, false);
	os_timer_add(mon_timer, 60*1000);
	return 0;
}
#endif
