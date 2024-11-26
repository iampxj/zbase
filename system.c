/*
 * Copyright 2022 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<system>: "fmt
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "basework/os/osapi.h"
#include "basework/generic.h"
#include "basework/dev/blkdev.h"
#include "basework/log.h"
#include "basework/lib/crc.h"


#define OBSERVER_CLASS_DEFINE
#include "basework/system.h"

#ifndef SYS_RECOVERY_THRESHOLD
#define SYS_RECOVERY_THRESHOLD 2
#endif

#ifdef CONFIG_SYS_NVRAM_ATTRIBUTE
#define _NVRAM_ATTRIBUTE __rte_section(CONFIG_SYS_NVRAM_ATTRIBUTE)
#else
#define _NVRAM_ATTRIBUTE __rte_section(".noinit.system.nvram")
#endif

static struct nvram_desc nvram_region _NVRAM_ATTRIBUTE;

#define CRASH_HEADER 0xaccedcaf
#define CRASH_TIMEOUT 60  /* seconds */ 
#define MILLISECONDS(n) ((n) * 1000)
#define MINUTES(n) ((n) * 60)

OBSERVER_CLASS_INSTANCE(system)
static const struct system_operations *_system_operation;
static os_timer_t crash_timer;
static uint8_t expires_cnt;
static os_mutex_t mutex;
os_critical_global_declare

/*
 * If the system runs more than 10 minutes, 
 * We consider the firmware to be normal
 */
static void crash_timer_cb(os_timer_t timer, void *arg) {
    (void) timer;
    (void) arg;
    if (++expires_cnt < 10) {
        os_timer_mod(crash_timer, MILLISECONDS(CRASH_TIMEOUT));
        return;
    }
    expires_cnt = 0;
    sys_crash_clear();
}

static uint16_t crash_crc(struct nvram_desc *nvram) {
    return lib_crc16((uint8_t *)&nvram->crash, offsetof(struct nvram_crash, crc));
}

const struct system_operations * __rte_notrace sys_operation_get(void) {
    return _system_operation;
}

uint32_t sys_uptime_get(void) {
    return _system_operation->get_time_since_boot();
}

struct nvram_desc *__rte_notrace sys_nvram_get(void) {
    //TODO: fix critical problem (use atomic variable)
    struct nvram_desc *nvram = &nvram_region;

    if (nvram->magic != NVRAM_MAGIC) {
        nvram->magic = NVRAM_MAGIC;
        memset(nvram->__data_begin, 0, 
            nvram->__data_end - nvram->__data_begin);
    }
    return nvram;
}

bool sys_is_firmware_okay(void) {
    return sys_firmware_evaluate() != CRASH_STATE_FATAL;
}

int sys_firmware_evaluate(void) {
    struct nvram_desc *nvram = sys_nvram_get();
    struct nvram_crash *crash = &nvram->crash;

    pr_notice("crash count: %d, crash_firsttime: %u\n", 
        crash->count, crash->start_time);
    
    if (crash->header != CRASH_HEADER ||
        crash->crc != crash_crc(nvram)) {
        crash->header = CRASH_HEADER;
        crash->start_time = _system_operation->get_utc();
        crash->count = 0;
        crash->crc = crash_crc(nvram);
        pr_warn("*** Crash detector is invalid ***\n");
    }

    /* starting the recovery timer */
    os_timer_mod(crash_timer, MILLISECONDS(CRASH_TIMEOUT));
    
	if (crash->count == 2)
		return CRASH_STATE_RECOVER;
	
	if (crash->count >= 4)
		return CRASH_STATE_FATAL;
	
	return CRASH_STATE_NORMAL;
}

void sys_crash_up(void) {
     struct nvram_desc *nvram = sys_nvram_get();
    os_critical_declare

    os_critical_lock
	nvram->crash.count++;
    os_critical_unlock
    nvram->crash.crc = crash_crc(nvram);
}

void sys_crash_clear(void) {
    struct nvram_desc *nvram = sys_nvram_get();
    os_critical_declare

    os_critical_lock
    nvram->crash.header = 0;
    nvram->crash.count = 0;
    os_critical_unlock
}

static void notify_locked(bool reboot, int reason, void *ret_ip) {
    static bool notified;
    if (!notified) {
        struct shutdown_param param;

        notified = true;
        param.reason = reason;
        param.ptr = ret_ip;
        pr_notice("system notify %s with RET_IP(%p)\n", 
            reboot? "SYS_REPORT_REBOOT": "SYS_REPORT_POWOFF",
            ret_ip
        );
        system_notify(reboot? SYS_REPORT_REBOOT: SYS_REPORT_POWOFF, &param);
    }
}

void sys_stop_notify(int reason, bool reboot) {
    guard(mutex)(&mutex);
    notify_locked(reboot, reason, RTE_RET_IP);
}

void sys_shutdown(int reason, bool reboot) {
    const struct system_operations *ops = _system_operation; 

    assert(ops != NULL);
    scoped_guard(mutex, &mutex) {
        sys_crash_clear();
        
        /* Notify application */
        notify_locked(reboot, reason, RTE_RET_IP);

        /* Sync data to disk */
        blkdev_sync();
        
        if (reboot)
            ops->reboot(reason);
        else
            ops->shutdown();
    }

    pr_err("Never reached here!\n");
    while (1);
}

void sys_enter_tranport(void) {
    const struct system_operations *ops = _system_operation; 
    assert(ops != NULL);
    assert(ops->enter_transport != NULL);
    
    ops->enter_transport();
}

int __rte_notrace sysops_register(const struct system_operations *ops) {
    assert(ops != NULL);
    assert(ops->get_time_since_boot != NULL);
    assert(ops->reboot != NULL);
    assert(ops->shutdown != NULL);
    
    if (_system_operation)
        return -EBUSY;

    _system_operation = ops;
    return 0;
}

int __rte_notrace sys_screen_up(int sec) {
    const struct system_operations *ops = _system_operation; 
    if (ops->screen_on)
        return ops->screen_on(sec);
    return -ENOSYS;
}

int __rte_notrace sys_screen_down(void) {
    const struct system_operations *ops = _system_operation; 
    if (ops->screen_off)
        return ops->screen_off();
    return -ENOSYS;
}

bool __rte_notrace sys_is_screen_up(void) {
    const struct system_operations *ops = _system_operation; 
    if (ops->is_screen_up)
        return ops->is_screen_up();
    return false;
}

int __rte_notrace sys_startup(void) {
    os_mtx_init(&mutex, 0);
    return os_timer_create(&crash_timer, crash_timer_cb, 
        NULL, true);
}
