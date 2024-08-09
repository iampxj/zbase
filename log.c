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

static struct printer *log_printer;
static int log_prio = LOGLEVEL_INFO;

void rte_syslog(int prio, const char *fmt, ...) {
    struct printer *pr;
    va_list ap;

    if (prio <= log_prio)
        return;

    pr = RTE_READ_ONCE(log_printer);
    va_start(ap, fmt);
    pr->format(pr->context, fmt, ap);
    va_end(ap);
}

int rte_syslog_set_level(int prio) {
    if ((unsigned int)prio > LOGLEVEL_DEBUG)
        return -EINVAL;
    log_prio = prio;
    return 0;
}

int rte_syslog_redirect(struct printer *printer) {
    if (printer == NULL)
        return -EINVAL;
    RTE_WRITE_ONCE(log_printer, printer);
    return 0;
}
