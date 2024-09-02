/*
 * Copyright 2023 wtcat
 */
#include <rtems/printer.h>
#include <rtems/sysinit.h>

#include "basework/log.h"

struct printer *_log_default_printer;
struct printer __log_kernel_printer, __log_stout_printer;

static void os_platform_init(void) {
    rtems_print_printer_printk((rtems_printer *)&__log_kernel_printer);
    rtems_print_printer_printf((rtems_printer *)&__log_stout_printer);
    pr_log_init(&__log_kernel_printer);
}

RTEMS_SYSINIT_ITEM(os_platform_init,
    RTEMS_SYSINIT_BSP_START,
    RTEMS_SYSINIT_ORDER_LAST
);
