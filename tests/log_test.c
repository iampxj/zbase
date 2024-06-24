/*
 * Copyright 2022 wtcat(wt1454246140@163.com)
 */

#define CONFIG_LOGLEVEL LOGLEVEL_NOTICE
#define pr_fmt(fmt) "log_test: " fmt

#include <init.h>
#include "basework/log.h"

static void log_test_case(void) {
    pr_dbg("This is a dbg-level message!");
    pr_info("This is a info-level message!");
    pr_notice("This is a notice-level message!");
    pr_warn("This is a warn-level message!");
    pr_err("This is a err-level message!");
    pr_emerg("This is a fatal-level message!");
}

static int log_test(const struct device *dev __rte_unused) {
    log_test_case();
    return 0;
}

SYS_INIT(log_test, APPLICATION, 30);