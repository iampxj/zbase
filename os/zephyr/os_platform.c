/*
 * Copyright 2022 wtcat
 *
 * OS adapt layer for zephyr
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "os_platform: " fmt
#define CONFIG_LOGLEVEL LOGLEVEL_DEBUG
#include <errno.h>
#include <assert.h>
#include <string.h>

#include <zephyr.h>
#include <init.h>
#include <device.h>
#include <fs/fs.h>
#include <drivers/flash.h>
#include <drivers/rtc.h>
#include <partition/partition.h>

#ifndef CONFIG_BOOTLOADER
#include "sys_wakelock.h"
#include "sys_manager.h"
#endif

#include "basework/malloc.h"
#include "basework/log.h"
#include "basework/dev/disk.h"
#include "basework/dev/partition.h"
#include "basework/system.h"
#include "board_cfg.h"


/*
 * Partition layout
 *
 * ---------------------------------------------------------------------------------------------------
 *| FIRMWARE(4) | SDFS(5) | NVRAM_RO(6) | NVRAM_RW(7) | SDFS_K(20) | GPT(30) | BOOTPARAM(31) | GP(36) |
 * ---------------------------------------------------------------------------------------------------
 */

// #define GMEM_DEBUG

#ifndef CONFIG_BASEWORK_MEM_SIZE
#define CONFIG_BASEWORK_MEM_SIZE (32 * 1024)
#endif

#ifdef GMEM_DEBUG
# include <stdio.h>
# include <ui_mem.h>

#include "basework/dev/blkdev.h"
#ifdef CONFIG_MEM_MONITOR
# include "basework/mon.h"
#endif

# define TRACER_LOCK   unsigned int _key = irq_lock();
# define TRACER_UNLOCK irq_unlock(_key);

# define TRACER_EXT_MEMBERS \
	size_t size; \
	void *caller;

# define TRACER_CMD \
	tnode->caller = __builtin_return_address(0); \
	tnode->size = size; \
    gmem_usage += size

# define TRACER_DELETECMD \
	gmem_usage -= tnode->size

# include "basework/debug/mb_trace.h"

struct mdump_arg {
    char *buf;
    size_t bufsz;
    size_t offset;
    int count;
};

static struct mb_tracer *_g_mtracer;
static size_t gmem_usage;
static TRACER_MEM_DEFINE(_tracer_buf, 1000);

static bool __rte_unused tracer_dump(struct mb_node *node, void *arg) {
    struct mdump_arg *p = arg;

	printk("<%3d>: <%p>@ %p (%d)\n", 
        p->count, node->caller, node->key, node->size);
    if (p->offset < p->bufsz) {
        int len = snprintf(&p->buf[p->offset], p->bufsz - p->offset,
            "<%3d>: <%p>@ %p (%d)\n", 
            p->count, node->caller, node->key, node->size);
        p->offset += len;
    }
    p->count++;
    return true;
}

static void __rte_unused tracer_init(void) {
    _g_mtracer = mb_tracer_init(_tracer_buf);
    assert(_g_mtracer != NULL);
}

#define TRACER_INSERT(_ptr, size) \
do { \
    if (_ptr) \
        TRACER_ADD(_g_mtracer, _ptr, TRACER_CMD); \
    else {\
        struct mdump_arg _dparg = {0}; \
        _dparg.buf = ui_mem_alloc(MEM_RES, 16*1024, NULL); \
        if (_dparg.buf) \
            _dparg.bufsz = 16*1024; \
        printk("Generic-Malloc(%u) failed <caller(%p) used(%u)>:\n", \
            size, __builtin_return_address(0), gmem_usage); \
        TRACER_DUMP(_g_mtracer, tracer_dump, &_dparg);\
        if (_dparg.offset) {\
            npr_err(disk, "Generic-Malloc(%u) failed<caller(%p)>:\n" \
                "%s\nused: %u\n", \
                size, __builtin_return_address(0), _dparg.buf, gmem_usage); \
            blkdev_sync(); \
            ui_mem_free(MEM_RES, _dparg.buf); \
        } \
    } \
} while (0)

#define TRACER_REMOVE(_ptr) \
    TRACER_DEL(_g_mtracer, _ptr, TRACER_DELETECMD)


#ifdef CONFIG_MEM_MONITOR
static void gmem_show(void *h, mon_show_t show, void *arg) {
    (void) arg;
    show(h, "gmem: %u", (gmem_usage * 100) / CONFIG_BASEWORK_MEM_SIZE);
}
#endif

static int _gmem_monitor_init(const struct device *dev) {
#ifdef CONFIG_MEM_MONITOR
    static const struct imon_class gmem_monitor = {
        .show = gmem_show,
        .period = 500
    };
    monitor_register(&gmem_monitor);
#endif /* CONFIG_MEM_MONITOR */
    tracer_init();
    return 0;
}
SYS_INIT(_gmem_monitor_init, PRE_KERNEL_1, 50);

#else

# define TRACER_INSERT(...)  (void)0 
# define TRACER_REMOVE(...) (void)0
#endif /* GMEM_DEBUG */

static const struct device *rtc_dev __rte_unused;

/*
 * Memory allocate implement
 */		
#ifndef CONFIG_SMALL_MEM_MODEL		
static char mem_buffer[MAX(CONFIG_BASEWORK_MEM_SIZE, Z_HEAP_MIN_SIZE)] __rte_aligned(8);
static STRUCT_SECTION_ITERABLE(k_heap, mem_heap) = {
    .heap = {
        .init_mem = mem_buffer,
        .init_bytes = sizeof(mem_buffer),
    }
};

void *__general_aligned_alloc(size_t alignment, size_t size) {
    void *ptr = k_heap_aligned_alloc(&mem_heap, alignment, size, K_NO_WAIT);
    __ASSERT_NO_MSG(ptr != NULL);
    return ptr;
}

void *__general_aligned_alloc_debug(size_t alignment, size_t size, 
    const char *func, int line) {
    pr_dbg("# aligned-allocate(%s@%d): %d bytes\n", func, line, size);
    return __general_aligned_alloc(alignment, size);
}

void *__general_malloc(size_t size) {
    void *ptr = k_heap_aligned_alloc(&mem_heap, sizeof(void *), size, K_NO_WAIT);
    TRACER_INSERT(ptr, size);
    __ASSERT_NO_MSG(ptr != NULL);
    return ptr;
}

void *__general_malloc_debug(size_t size, const char *file, int line) {
    pr_dbg("# allocate(%s@%d): %d bytes\n", file, line, size);
    return __general_malloc(size);
}

void *__general_calloc(size_t n, size_t size) {
    size_t alloc_size = n * size;
    void *ptr = k_heap_aligned_alloc(&mem_heap, sizeof(void *), alloc_size, K_NO_WAIT);
    __ASSERT_NO_MSG(ptr != NULL);
    TRACER_INSERT(ptr, alloc_size);
    memset(ptr, 0, alloc_size);
    return ptr;
}

void *__general_calloc_debug(size_t n, size_t size, const char *file, int line) {
    pr_dbg("# callocate(%s@%d): %d bytes\n", file, line, n*size);
    return __general_calloc(n, size);
}

void  __general_free(void *ptr) {
    TRACER_REMOVE(ptr);
    k_heap_free(&mem_heap, ptr);
}

void *__general_realloc(void *ptr, size_t size) {
#ifndef GMEM_DEBUG
    k_spinlock_key_t key = k_spin_lock(&mem_heap.lock);
    void *p = sys_heap_aligned_realloc(&mem_heap.heap, ptr, sizeof(void *), size);
    k_spin_unlock(&mem_heap.lock, key);
    __ASSERT_NO_MSG(ptr != NULL);
#else /* GMEM_DEBUG */
    void *p = __general_malloc(size);
    if (p && ptr) {
        memcpy(p, ptr, size);
        __general_free(ptr);
    }
#endif /* GMEM_DEBUG */
    return p;
}

void *__general_realloc_debug(void *ptr, size_t size, const char *func, int line) {
    pr_dbg("# reallocate(%s@%d): %d bytes\n", func, line, size);
    return __general_realloc(ptr, size);
}

#else /* CONFIG_SMALL_MEM_MODEL */

void *__general_aligned_alloc(size_t alignment, size_t size) {
    void *ptr = k_aligned_alloc(alignment, size);
    __ASSERT_NO_MSG(ptr != NULL);
    return ptr;
}

void *__general_aligned_alloc_debug(size_t alignment, size_t size, 
    const char *func, int line) {
    pr_info("# aligned-allocate(%s@%d): %d bytes\n", func, line, size);
    return __general_aligned_alloc(alignment, size);
}

void *__general_malloc(size_t size) {
    void *ptr = k_malloc(size);
    TRACER_INSERT(ptr, size);
    __ASSERT_NO_MSG(ptr != NULL);
    return ptr;
}

void *__general_malloc_debug(size_t size, const char *file, int line) {
    pr_info("# allocate(%s@%d): %d bytes\n", file, line, size);
    return __general_malloc(size);
}

void *__general_calloc(size_t n, size_t size) {
    size_t alloc_size = n * size;
    void *ptr = k_malloc(n * size);
    __ASSERT_NO_MSG(ptr != NULL);
    TRACER_INSERT(ptr, alloc_size);
    memset(ptr, 0, alloc_size);
    return ptr;
}

void *__general_calloc_debug(size_t n, size_t size, const char *file, int line) {
    pr_info("# callocate(%s@%d): %d bytes\n", file, line, n*size);
    return __general_calloc(n, size);
}

void  __general_free(void *ptr) {
    TRACER_REMOVE(ptr);
    k_free(ptr);
}

void *__general_realloc(void *ptr, size_t size) {
    void *p = __general_malloc(size);
    if (p && ptr) {
        memcpy(p, ptr, size);
        __general_free(ptr);
    }
    return p;
}

void *__general_realloc_debug(void *ptr, size_t size, const char *func, int line) {
    pr_info("# reallocate(%s@%d): %d bytes\n", func, line, size);
    return __general_realloc(ptr, size);
}
#endif /* !CONFIG_SMALL_MEM_MODEL */ 


/*
 * Log printer register 
 */
static int __rte_notrace printk_format(void *context, const char *fmt, va_list ap) {
    (void) context;
    vprintk(fmt, ap);
    return 0;
}

static struct printer log_printer = {
    .format = printk_format
};

static int os_early_platform_init(const struct device *dev __rte_unused) {
    pr_log_init(&log_printer);
    return 0;
}
SYS_INIT(os_early_platform_init, PRE_KERNEL_1, 1);

/*
 * Platform Device
 */
static const char *platform_flash_getname(device_t dev) {
    const struct device *zdev = (const struct device *)dev;
    return zdev->name;
}

static int platform_flash_read(device_t dd, void *buf, size_t size, long offset) {
    return flash_read(dd, offset, buf, size);
}

static int platform_flash_write(device_t dd, const void *buf, size_t size, long offset) {
    return flash_write(dd, offset, buf, size);
}

static int platform_flash_erase(device_t dd, long offset, size_t size) {
    return flash_erase(dd, offset, size);
}

static int __rte_notrace platform_flash_ioctl(device_t dd, long cmd, void *arg) {
    (void) dd;
    (void) cmd;
    (void) arg;
    return -ENOSYS;
}

static int __rte_unused __rte_notrace platform_flash_init(void) {
    const struct flash_pages_layout *layout;
    const struct flash_driver_api *api;
    static struct disk_device *dd;
    const struct device *zdev;
    const char *devnames[] = {
        CONFIG_SPI_FLASH_NAME,
        CONFIG_SPI_FLASH_1_NAME,
        CONFIG_SPI_FLASH_2_NAME
#ifdef CONFIG_SPINAND_FLASH_NAME
        ,
        CONFIG_SPINAND_FLASH_NAME
#endif

#ifdef CONFIG_BLOCK_DEV_FLASH_NAME
        ,
        CONFIG_BLOCK_DEV_FLASH_NAME
#endif
    };
    
    for (int i = 0; i < ARRAY_SIZE(devnames); i++) {
        zdev = device_get_binding(devnames[i]);
        pr_dbg("<%s>: device_name(%s) handle(%p)\n", __func__, devnames[i], zdev);
        if (zdev) {
            api = (const struct flash_driver_api *)zdev->api;
            __ASSERT_NO_MSG(api->page_layout != NULL);
            pr_dbg("%s: layout(%p)\n", zdev->name, api->page_layout);
            api->page_layout(zdev, &layout, NULL);

            dd = general_calloc(1, sizeof(*dd));
            if (dd == NULL) {
                pr_err("No more memory\n");
                return -ENOMEM;
            }

            dd->dev = (void *)zdev;
            dd->addr = 0;
            dd->blk_size = layout->pages_size;
            dd->len = layout->pages_count * layout->pages_size;
            dd->read = platform_flash_read;
            dd->write = platform_flash_write;
            dd->erase = platform_flash_erase;
            dd->ioctl = platform_flash_ioctl;
            dd->get_name = platform_flash_getname;
            int err = disk_device_register(dd);
            if (err) {
                pr_err("Register disk device failed: %d\n", err);
                return err;
            }
        }
    }

    return -EINVAL;
}

#ifndef CONFIG_BOOTLOADER
/*
 * Partition table
 */
struct pt_arg {
    char name[16];
    const char *parent;
    long offset;
    size_t len;
};

extern void partition_iterate(
	void (*iterator)(const char *name, uint32_t ofs, size_t size, int storage_id, void *arg), 
	void *arg);

static void __rte_unused __rte_notrace partition_iterator(const char *name, uint32_t ofs, size_t size, 
    int storage_id, void *arg) {
    struct pt_arg *pt = (struct pt_arg *)arg;

    if (strcmp(name, "user"))
        return;
    memcpy((void *)pt->name, name, 8);
    pt->name[8] = '\0';
    pt->offset = ofs;
    pt->len = size;
    switch (storage_id) {
    case STORAGE_ID_NOR:
        pt->parent = CONFIG_SPI_FLASH_NAME;
        break;
    case STORAGE_ID_DATA_NOR:
        pt->parent = CONFIG_SPI_FLASH_2_NAME;
        break;
    case STORAGE_ID_SD:
        pt->parent = "sd";
        break;
    case STORAGE_ID_NAND:
        pt->parent = CONFIG_SPINAND_FLASH_NAME;
        break;
    default:
        pr_err("invalid partition (%s)\n", name);
        pt->parent = NULL;
        break; 
    }
}

static void gpt_update_notify(void) {
    extern int usr_partition_init(bool reinit);
    const struct gpt_entry *gpe;
    static bool first = true;

    gpe = gpt_find("reserved");
    rte_assert0(gpe != NULL);

    printk("Paritiion build: device(%s) offset(0x%x) size(%x) first(%d)\n",
        gpe->parent, (size_t)gpe->offset, gpe->size, (int)first);

    if (first) {
        err = partitions_configure_build(gpe->offset, 
            gpe->size, gpe->parent, false);
        rte_assert0(err == 0);
        rte_assert0(usr_partition_init(false) == 0);
        first = false;
    } else {
        k_sched_lock();
        err = partitions_configure_rebuild(gpe->offset, 
            gpe->size, gpe->parent, false);
        rte_assert0(err == 0);
        rte_assert0(usr_partition_init(true) == 0);
        k_sched_unlock();
    }
}

static int __rte_unused __rte_notrace platform_partition_init(void) {
    const struct partition_entry *pe;
    struct disk_device *dd = NULL;
    size_t max_buflen = 4096;
    struct bin_header *bin;
    char *buf;

    pe = parition_get_entry2(STORAGE_ID_NOR, 30);
    rte_assert0(pe != NULL);

    if (pe->storage_id == STORAGE_ID_NOR)
        disk_device_open("spi_flash", &dd);
    else if (pe->storage_id == STORAGE_ID_NAND)
        disk_device_open("spinand", &dd);
    else if (pe->storage_id == STORAGE_ID_DATA_NOR)
        disk_device_open("spi_flash_2", &dd);
    rte_assert0(dd != NULL);

    bin = general_malloc(max_buflen);
    rte_assert0(bin != NULL);
    rte_assert0(disk_device_read(dd, bin, max_buflen, pe->offset) == 0);
    rte_assert0(bin->magic == FILE_HMAGIC);
    rte_assert0(bin->size < max_buflen);
    rte_assert0(lib_crc32(bin->data, bin->size) == bin->crc);
    gpt_register_update_cb(gpt_update_notify);
    rte_assert0(gpt_load(bin->data) == 0);
    gpt_dump();
    general_free(bin);

    return 0;
}

static void platform_reboot(int reason) {
    system_power_reboot(reason);
}

static void platform_shutdown(void) {
    system_power_off();
}

static uint32_t __rte_notrace platform_gettime_since_boot(void) {
    return k_uptime_get_32();
}

/*
 * Make system enter transport mode
 *
 * 1> close charger
 * 2> close wakeup source
 * 3> disable onof function
 * 4> shutdown
 */
static void platform_enter_transport(void) {
// #define CHG_CTL_SVCC_CHG_EN         BIT(10)
// #define WAKE_CTL_SVCC_DC5VLV_WKEN   BIT(10)
// #define WAKE_CTL_SVCC_DC5VIN_WKEN   BIT(2)
// #define WAKE_CTL_SVCC_WKEN_MASK     (WAKE_CTL_SVCC_DC5VLV_WKEN | WAKE_CTL_SVCC_DC5VIN_WKEN)
// #define WAKE_CTL_SVCC_LONG_WKEN     BIT(0)

//     unsigned int key = irq_lock();
//     /* close charger */
//     reg32_update(CHG_CTL_SVCC, CHG_CTL_SVCC_CHG_EN, 0);
//     /* close wakeup source */
//     reg32_update(WKEN_CTL_SVCC, WAKE_CTL_SVCC_WKEN_MASK, 0);
//     /* disable onof function*/
//     reg32_update(WKEN_CTL_SVCC, WAKE_CTL_SVCC_LONG_WKEN, 0);
//     irq_unlock(key);

    /* request shutdown */
    sys_shutdown(0, false);
}

static K_MUTEX_DEFINE(screen_mtx);

static bool platform_screen_is_up(void) {
extern bool system_is_screen_on(void);
    bool up;
    k_mutex_lock(&screen_mtx, K_FOREVER);
    up = system_is_screen_on();
    k_mutex_unlock(&screen_mtx);
    return up;
}

static int platform_screen_up(int sec) {
extern void system_set_autosleep_time(uint32_t timeout);
    k_mutex_lock(&screen_mtx, K_FOREVER);
    if (sec > 0) {
        pr_dbg("## update autosleep timeout: %ds\n", sec);
	    system_set_autosleep_time(sec);
    }
    system_clear_fast_standby();
	sys_wake_lock_ext(FULL_WAKE_LOCK, APP_WAKE_LOCK_USER);
	sys_wake_unlock_ext(FULL_WAKE_LOCK, APP_WAKE_LOCK_USER);
    k_mutex_unlock(&screen_mtx);
    pr_dbg("## request screen up ## caller(%p)\n", __builtin_return_address(0));
    return 0;
}

static int platform_screen_down(void) {
extern void system_request_fast_standby(void);
    k_mutex_lock(&screen_mtx, K_FOREVER);
	system_request_fast_standby();
    k_mutex_unlock(&screen_mtx);
    pr_dbg("## request screen down ##\n");
    return 0;
}

static uint32_t plateform_get_utc(void) {
    if (rtc_dev) {
        struct rtc_time tm;
        uint32_t utc;
        rtc_get_time(rtc_dev, &tm);
        rtc_tm_to_time(&tm, &utc);
        return utc;
    }
    return 0;
}

static const struct system_operations platform_ops __rte_unused = {
    .get_time_since_boot = platform_gettime_since_boot,
    .reboot = platform_reboot,
    .shutdown = platform_shutdown,
    .enter_transport = platform_enter_transport,
    .screen_on = platform_screen_up,
    .screen_off = platform_screen_down,
    .is_screen_up = platform_screen_is_up,
    .get_utc = plateform_get_utc
};
#endif /* CONFIG_BOOTLOADER */

static int __rte_notrace __rte_unused
os_platform_init(const struct device *dev __rte_unused) {
    platform_flash_init();
#if !defined(CONFIG_BOOTLOADER) && defined(CONFIG_USER_PARTITION)
    static struct printer disk_printer;

    rtc_dev = device_get_binding(CONFIG_RTC_0_NAME);
    if (rtc_dev)
        rtc_enable(rtc_dev);

    platform_partition_init();
    disklog_format_init(&disk_printer);
    pr_disklog_init(&disk_printer);
    sysops_register(&platform_ops);
    sys_startup();
#endif
    return 0;
}

SYS_INIT(os_platform_init, APPLICATION, 0);

#if !defined(CONFIG_BOOTLOADER) && defined(CONFIG_USER_PARTITION)
static int __rte_notrace __rte_unused
os_platform_init_late(const struct device *dev __rte_unused) {
extern int platform_extend_filesystem_init(int fstype);

#ifdef CONFIG_FILE_SYSTEM_LITTLEFS
    int fs_type = FS_LITTLEFS;
#else
    int fs_type = FS_FATFS;
#endif
    if (platform_extend_filesystem_init(fs_type)) 
        pr_err("User extend filesystem initialize failed!\n"); 
    return 0;
}
SYS_INIT(os_platform_init_late, APPLICATION, 80);
    
/* This is a patch for SDK */
// #ifndef CONFIG_ACTLOG
// void actlog_printk_nano(uint32_t pack_data, const char *fmt, ...) {
//     va_list ap;

//     (void) pack_data;
//     va_start(ap, fmt);
//     vprintk(fmt, ap);
//     va_end(ap);
// }
// #endif /* CONFIG_ACTLOG */

#endif /* CONFIG_BOOTLOADER */
