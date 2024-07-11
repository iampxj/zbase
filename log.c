/*
 * Copyright 2024 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <errno.h>
#include <stdint.h>

#include "basework/generic.h"
#include "basework/log.h"
#include "basework/lib/printer.h"

#ifndef RTE_WRITE_ONCE
#define RTE_WRITE_ONCE(x, val) (x) = (val)
#endif

#ifndef RTE_READ_ONCE
#define RTE_READ_ONCE(x) (x)
#endif

#define LOG_MASK(prio)  (0x1 << (prio))
#define LOG_LEVEL(prio) ((0x1 << (prio + 1)) - 1)

static uint16_t syslog_mask = LOG_LEVEL(LOGLEVEL_INFO);
static struct printer *sys_printer; 

void rte_syslog(int prio, const char *fmt, ...) {
    struct printer *pr;
    va_list ap;

    if (!(LOG_MASK(prio) & syslog_mask))
        return;

    pr = RTE_READ_ONCE(sys_printer);
    va_start(ap, fmt);
    pr->format(pr->context, fmt, ap);
    va_end(ap);
}

int rte_syslog_set_level(int prio) {
    if ((unsigned int)prio > LOGLEVEL_DEBUG)
        return -EINVAL;

    syslog_mask = (uint16_t)LOG_LEVEL(prio);
    return 0;
}

int rte_syslog_redirect(struct printer *printer) {
    RTE_WRITE_ONCE(sys_printer, printer);
    return 0;
}
