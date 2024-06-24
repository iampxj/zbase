/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<callpath>: "fmt
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <assert.h>

#include "basework/os/osapi.h"
#include "basework/minmax.h"
#include "basework/malloc.h"
#include "basework/log.h"
#include "basework/lib/callpath.h"
#include "basework/system.h"

#ifdef CONFIG_FRACE_NVRAM
#define USE_CALLPATH_NVRAM CONFIG_FRACE_NVRAM
#endif

#define CALLPATH_MAX_DEEPTH 100

#define ADDR2LINE_CMD "arm-none-eabi-addr2line -e zephyr.elf -a -f"

#define MTX_INIT()   pthread_mutex_init(&callpath_mutex, NULL)
#define MTX_LOCK()   pthread_mutex_lock(&callpath_mutex)
#define MTX_UNLOCK() pthread_mutex_unlock(&callpath_mutex)

#ifndef USE_CALLPATH_NVRAM
# define CPATH_ALLOC(nitem) general_calloc(1, CALLPATH_SIZE(nitem))
# define CPATH_FREE(p) general_free(p)
#else
# define CPATH_ALLOC(nitem) nvram_allocate(nitem)
# define CPATH_FREE(p) nvram_free(p)
#endif

static struct callpath *callpath_list;
static pthread_mutex_t callpath_mutex;

static void __rte_notrace callpath_addr2line_print(struct callnode_arg *ca) {
    if (ca->idx > 0) {
        pr_vout(ca->pr, "\n"ADDR2LINE_CMD);
        for (int i = 0; i < ca->idx; i++)
            pr_vout(ca->pr, " %p", ca->buf[i]);
        pr_vout(ca->pr, "\n\n");
    }
}

static void __rte_notrace callpath_print_cb(struct callpath_node *node, void *arg) {
    struct callnode_arg *cn = arg;
    pr_vout(cn->pr, "=> [%d]: <%p> <- <%p>\n", 
        cn->idx, node->owner, node->caller);
    cn->buf[cn->idx] = node->owner;
    cn->idx++;
}

int __rte_notrace callpath_dump_locked(struct callpath *ctx, struct callnode_arg *p) {
    p->idx = 0;
    pr_vout(p->pr, "\n*** %s ***\n", ctx->name);
    int err = callpath_foreach_node(ctx, callpath_print_cb, p);
#ifdef CONFIG_PERFORMANCE_PROFILE
    pr_out("ExecuteUnit(%s) heavy function: %p time(%d)\n",
        ctx->name, ctx->max.owner, ctx->max.time);
#endif
    callpath_addr2line_print(p);
    return err;
}

#ifdef USE_CALLPATH_NVRAM
static void *nvram_allocate(size_t size) {
    struct nvram_desc *nvram = sys_nvram_get();
    void *p;

    (void) size;
    MTX_LOCK();
    if (nvram->callpath.idx < CALLPATH_NVRAM_SIZE) {
        p = nvram->callpath.mem[nvram->callpath.idx++];
        memset(p, 0, sizeof(nvram->callpath.mem[0]));
        MTX_UNLOCK();
        return p;
    }
    MTX_UNLOCK();
    return NULL;
}

static void nvram_free(void *p) {
    (void) p;
}

int __rte_notrace callpath_nvram_dump(struct printer *pr) {
    struct nvram_desc *nvram = sys_nvram_get();
    char buffer[CALLPATH_SIZE(CALLPATH_NVRAM_DEEPTH)];
    struct callnode_arg *ca = (void *)buffer;
    int err = 0;

    if (!pr)
        return -EINVAL;
    ca->pr = pr;
    MTX_LOCK();
    for (int i = 0; i < nvram->callpath.idx; i++) {
        struct callpath *ctx = (void *)nvram->callpath.mem[i];
        if (ctx->deepth > CALLPATH_MAX_DEEPTH) {
            pr_err("callpath stack deepth(%d) is invalid\n", ctx->deepth);
            break;
        }
        err = callpath_dump_locked(ctx, ca);
        if (err)
            break;
    }
    MTX_UNLOCK();
    return err;
}
#endif /* USE_CALLPATH_NVRAM */

struct callpath *__rte_notrace callpath_allocate(const char *name, int deepth) {
    struct callpath *ctx;

    ctx = CPATH_ALLOC(deepth);
    if (!ctx) {
        pr_err("no more memory\n");
        return NULL;
    }
#ifndef USE_CALLPATH_NVRAM
    ctx->deepth = deepth;
#else
    (void) deepth;
    ctx->deepth = CALLPATH_NVRAM_DEEPTH;
#endif
    ctx->top = ctx->deepth;
    strncpy(ctx->name, name, CALLPATH_MAX_NAME-1);
#ifdef CONFIG_PERFORMANCE_PROFILE
    ctx->max.top = ctx->deepth;
#endif
    MTX_LOCK();
    ctx->next = callpath_list;
    callpath_list = ctx;
    MTX_UNLOCK();
    return ctx;
}

void __rte_notrace callpath_free(struct callpath *ctx) {
    if (ctx) {
        struct callpath **p = &callpath_list;
        MTX_LOCK();
        while (*p) {
            if ((*p) == ctx) {
                *p = ctx->next;
                ctx->next = NULL;
                break;
            }
            p = &(*p)->next;
        }
        MTX_UNLOCK();
        CPATH_FREE(ctx);
    }
}

int __rte_notrace callpath_foreach_node(struct callpath *ctx,
    void (*iterator)(struct callpath_node *, void *), 
    void *arg) {
    if (!ctx || !iterator)
        return -EINVAL;

    int top = max(ctx->top, 0);
    while (top < ctx->deepth) {
        iterator(&ctx->stack[top], arg);
        top++;
    }
    return 0;
}

int __rte_notrace callpath_push_locked(struct callpath *ctx, void *fn, 
    void *caller, uint32_t timestamp) {
    int top = ctx->top;

    if (top > 0) {
        top--;
        ctx->stack[top].caller = caller;
        ctx->stack[top].owner = fn;
#ifdef CONFIG_PERFORMANCE_PROFILE
        ctx->stack[top].time = timestamp;
#endif
        ctx->top = top;
        return 0;
    }
    ctx->ovrlvl++;
    return -EBUSY;
}

int __rte_notrace callpath_pop_locked(struct callpath *ctx, void *fn, 
    void *caller, uint32_t timestamp) {
    int top = ctx->top;

    if (rte_unlikely(ctx->ovrlvl > 0)) {
       ctx->ovrlvl--;
       return -EINVAL;
    }
    if (top < ctx->deepth) {
#ifdef CONFIG_PERFORMANCE_PROFILE
        ctx->stack[top].time = timestamp - ctx->stack[top].time;
        if (top <= ctx->max.top && time > ctx->max.time) {
            ctx->max.top = top;
            ctx->max.node = ctx->stack[top];
        }
#endif
        ctx->top++;
        return 0;
    }
    return -EINVAL;
}

int __rte_notrace callpath_foreach(bool (*iterator)(struct callpath *ctx, void *), 
    void *arg) {
    struct callpath *p = callpath_list;

    if (!iterator)
        return -EINVAL;

    MTX_LOCK();
    while (p) {
        if (iterator(p, arg))
            break;
        p = p->next;
    }
    MTX_UNLOCK();
    return 0;
}

int __rte_notrace callpath_init(void) {
#ifdef USE_CALLPATH_NVRAM
    struct nvram_desc *nvram = sys_nvram_get();
    nvram->callpath.idx = 0;
#endif
    MTX_INIT();
    return 0;
}