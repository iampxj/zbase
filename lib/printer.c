/*
 * Copyright 2022 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <assert.h>
#include <stdio.h>

#include "basework/lib/printer.h"
#include "basework/circ_buffer.h"
#include "basework/lib/iovpr.h"
#include "basework/lib/disklog.h"

#define LOG_MAX_PRINTER 1

#ifndef _WIN32
struct disklog_context {
    char buffer[128];
    int ptr;
};

struct queue_context {
    struct circ_buffer ccb;
    size_t size;
};

static struct queue_context queue_context;
struct printer *__log_disk_printer;
#endif /* _WIN32  */

struct printer* __log_default_printer;

static int 
stdio_format(void* context, const char* fmt, va_list ap) {
    (void)context;
    return vprintf(fmt, ap);
}

#ifndef _WIN32
static void 
cqueue_put_char(int c, void *arg) {
    struct queue_context *qc = arg;
    struct circ_buffer *ccb = &qc->ccb;
    int in = ccb->head;

    ccb->b_buf[in] = (uint8_t)c;
    ccb->head = (in + 1) & (qc->size - 1);
    if (ccb->head == ccb->tail)
        ccb->tail++;
}

static int 
cqueue_format(void *context, const char *fmt, va_list ap) {
    return _IO_Vprintf(cqueue_put_char, context, fmt, ap);
}

#ifndef CONFIG_BOOTLOADER
static void 
disklog_put_char(int c, void *arg) {
    struct disklog_context *dc = (struct disklog_context *)arg;
    int ptr = dc->ptr;
    dc->buffer[ptr] = (char)c;
    dc->ptr = ptr + 1;
    if (dc->ptr == sizeof(dc->buffer)) {
        disklog_input(dc->buffer, dc->ptr);
        dc->ptr = 0;
    }
}

static int 
disklog_format(void *context, const char *fmt, va_list ap) {
    struct disklog_context dc;
    int len;
    (void) context;
    dc.ptr = 0;
    len = _IO_Vprintf(disklog_put_char, &dc, fmt, ap);
    if (dc.ptr > 0)
        disklog_input(dc.buffer, dc.ptr);
    return len;
}

void __rte_notrace 
disklog_format_init(struct printer *pr) {
    assert(pr != NULL);
    pr->format = disklog_format;
    pr->context = NULL;
    disklog_init();
}
#endif /* CONFIG_BOOTLOADER */

void __rte_notrace 
queue_format_init(struct printer* pr, void* buffer, size_t size) {
    assert(pr != NULL);
    assert(rte_powerof2(size));
    circ_buffer_reset(&queue_context.ccb);
    queue_context.size = size;
    pr->format = cqueue_format;
    pr->context = &queue_context;
}

#endif /* _WIN32 */

void __rte_notrace 
printf_format_init(struct printer *pr) {
    assert(pr != NULL);
    pr->format = stdio_format;
    pr->context = NULL;
}
