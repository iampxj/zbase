/*
 * Copyright 2022 wtcat
 */
#include "basework/generic.h"
#include "basework/lib/printer.h"
#include "basework/log.h"
#include "basework/ccinit.h"
#include "basework/dev/fifofs.h"

extern "C" int unix_init(void);

static struct printer log_printer;
static struct printer disk_printer;
static struct fifo_filemem fifo_fds[2];

CC_INIT(platform_log, kDeviceOrder, 0) {
	printf_format_init(&log_printer);
	pr_log_init(&log_printer);
    return 0;
}

CC_INIT(posix_platform, kApplicationOrder, 0) {
	disklog_format_init(&disk_printer);
	pr_disklog_init(&disk_printer);
	fifofs_register(fifo_fds, sizeof(fifo_fds)/sizeof(fifo_fds[0]));
	unix_init();
    return 0;
}