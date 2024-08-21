/*
 *  Copyright 2022 wtcat
 */

#ifndef BASEWORK_SYSTEM_H_
#define BASEWORK_SYSTEM_H_

#include <errno.h>
#include <stdint.h>
#include <stdbool.h>

#include "basework/container/observer.h"
#include "basework/lib/callpath.h"

#ifdef __cplusplus
extern "C"{
#endif

OBSERVER_CLASS_DECLARE(system)

/*
 * Shutdown type
 */
#define SYS_REPORT_REBOOT 1
#define SYS_REPORT_POWOFF 2

/*
 * Shutdown reason
 */
#define SHUTDOWN_NORMAL        0
#define SHUTDOWN_OTA           1
#define SHUTDOWN_FACTORY_RESET 2
#define SHUTDOWN_RECOVERY      3

/*
 * System eveluate level
 */
#define SYSTEM_EVEL_NORMAL   0
#define SYSTEM_EVEL_WARNING  1
#define SYSTEM_EVEL_FATAL    2

struct nvram_desc {
#define NVRAM_MAGIC 0x5abcdefa
    uint32_t magic;
    uint32_t reserved;
#ifdef _WIN32
    char __data_begin[1];
#else
    char __data_begin[0];
#endif
    /* system crash var */
    char    env_ram[32];
    uint8_t time_ram[20];
    uint8_t fault_time[20];
#if CONFIG_PSRAM_SIZE > 0
    char    fault_ram[8192];
#else
    char    fault_ram[2048];
#endif
    struct {
        uint32_t header;
        uint32_t start_time;
        uint16_t count;
        uint16_t state;
#define CRASH_STATE_NORMAL         0
#define CRASH_STATE_RECOVER        1
#define CRASH_STATE_RECOVER_POST   2
#define CRASH_STATE_FATAL          3
    } crash;

    /* callpath region */
    struct {
#define CALLPATH_NVRAM_SIZE 1
#define CALLPATH_NVRAM_DEEPTH 1
        char mem[CALLPATH_NVRAM_SIZE][CALLPATH_SIZE(CALLPATH_NVRAM_DEEPTH)];
        int idx;
    } callpath __rte_aligned(4);

    char __data_end[0];
};

// _Static_assert(sizeof(struct nvram_desc) <= CONFIG_BACKUP_RAM_SIZE, "");


struct shutdown_param {
    int reason;
    void *ptr;
};

/*
 * Global system absract interface
 */
struct system_operations {
    /* return milliseconds */
    uint32_t (*get_time_since_boot)(void);
    void (*reboot)(int reason);
    void (*shutdown)(void);
    void (*enter_transport)(void);

#define SCREEN_DEFAULT_TIMEOUT -1
    int (*screen_on)(int sec);
    int (*screen_off)(void);
    bool (*is_screen_up)(void);

    uint32_t (*get_utc)(void);
};

/*
 * sys_shutdown - Make system shutdown or reboot
 *
 * @reason: The reason that shutdown or reboot
 * @reboot: enable reboot
 */
void sys_shutdown(int reason, bool reboot);

/*
 * sys_enter_tranport - Make system enter transport mode
 */
void sys_enter_tranport(void);

/*
 * sysops_register - Register system operations
 *
 * @ops: operations pointer
 * return 0 if success
 */
int sysops_register(const struct system_operations *ops);

/*
 * sys_screen_up - Screen on
 *
 * @sec: timeout with seconds(if the paramter(sec) is -1 then use default timeout)
 * return 0 if success
 */
int sys_screen_up(int sec);

/*
 * sys_screen_down - Screen off
 *
 * return 0 if success
 */
int sys_screen_down(void);

/*
 * sys_is_screen_up 
 *
 * return true if the screen has been power-on
 */
bool sys_is_screen_up(void);

/*
 * sys_is_firmware_okay - Determine if the firmware is OK
 *
 * return true if firmware is okay
 */
bool sys_is_firmware_okay(void);

/*
 * sys_firmware_evaluate - Eveluate system status
 *
 */
int sys_firmware_evaluate(void);

/*
 * sys_crash_up -  Increate crash count
 */
void sys_crash_up(void);

/*
 * sys_crash_up -  Clear crash count
 */
void sys_crash_clear(void);

/*
 * sys_startup -  System component initialize
 */
int sys_startup(void);

/*
 * sys_uptime_get - Get system time since boot
 *
 * return milliseconds
 */
uint32_t sys_uptime_get(void);
/*
 * sys_nvram_get - Get nvram buffer
 */
struct nvram_desc *sys_nvram_get(void);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_SYSTEM_H_ */