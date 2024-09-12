/*
 * Copyright 2024 wtcat
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>

#include "basework/os/osapi.h"
#include "basework/errno.h"
#include "basework/assert.h"
#include "basework/generic.h"
#include "basework/container/queue.h"
#include "basework/malloc.h"
#include "basework/params.h"


#define vcopy(d, s, l) \
do { \
    switch (l) { \
    case 1: *(uint8_t *)(d)  = *(uint8_t *)(s);  break; \
    case 2: *(uint16_t *)(d) = *(uint16_t *)(s); break; \
    case 4: *(uint32_t *)(d) = *(uint32_t *)(s); break; \
    case 8: *(uint64_t *)(d) = *(uint64_t *)(s); break; \
    default: rte_assert0(0); \
    } \
} while (0)

TAILQ_HEAD(param_list, param_node);
struct param_context {
    struct param_list head;
    os_mutex_t mtx;
    SLIST_ENTRY(param_context) link;
    bool static_allocated;
    
#define CONTEXT_MAX_NAMELEN 16
    char name[CONTEXT_MAX_NAMELEN];
};

static SLIST_HEAD(context_list, param_context) context_created = 
    SLIST_HEAD_INITIALIZER(context_created);
static struct param_context _system_params = {
    .head = TAILQ_HEAD_INITIALIZER(_system_params.head),
    .static_allocated = true,
    .name = {"sys_param"}
};
struct param_context *_system__params = &_system_params;
static os_mutex_t context_mtx;

static void *param_alloc(struct param_context *ctx, const char *name, 
    size_t size) {
    size_t len = strlen(name) + 1;
    struct param_node *p;
    
    p = general_calloc(1, sizeof(*p) + len + size);
    if (p) {
        p->val = (char *)(p + 1);
        p->name = p->val + size;

        os_mtx_lock(&ctx->mtx);
        TAILQ_INSERT_TAIL(&ctx->head, p, link);
        os_mtx_unlock(&ctx->mtx);
        return p;
    }

    return NULL;
}

static void param_free(struct param_context *ctx, struct param_node *node) {
    struct param_node *p;

    os_mtx_lock(&ctx->mtx);
    TAILQ_FOREACH(p, &ctx->head, link) {
        if (p == node) {
            TAILQ_REMOVE(&ctx->head, p, link);
            os_mtx_unlock(&ctx->mtx);
            general_free(p);
            return;
        }
    }
    os_mtx_unlock(&ctx->mtx);
}

static struct param_node *
param_serach(struct param_context *ctx, const char *name) {
    struct param_node *p;

    os_mtx_lock(&ctx->mtx);
    TAILQ_FOREACH(p, &ctx->head, link) {
        if (!strcmp(name, p->name)) {
            os_mtx_unlock(&ctx->mtx);
            return p;
        }
    }
    os_mtx_unlock(&ctx->mtx);
    return NULL;
}

int __param_context_setval(struct param_context *ctx, const char *name, 
    char *pval, size_t len) {
    size_t vlen = rte_max(sizeof(long), len);
    struct param_node *p;

    if (name == NULL || pval == NULL)
        return -EINVAL;

    if (vlen > 8)
        return -EINVAL;

    p = param_serach(ctx, name);
    if (p == NULL) {
        p = param_alloc(ctx, name, vlen);
        if (p == NULL)
            return -ENOMEM;
    }

    vcopy(p->val, pval, len);
    strcpy(p->name, name);
    return 0;
}

int __param_context_getval(struct param_context *ctx, const char *name, 
    char *pval, size_t len) {
    struct param_node *p;

    if (name == NULL || pval == NULL)
        return -EINVAL;

    p = param_serach(ctx, name);
    if (p) {
        vcopy(pval, p->val, len);
        return 0;
    }

    return -ENODATA;
}

int param_context_setstr(struct param_context *ctx, const char *name, 
    const char *str) {
    struct param_node *p;
    size_t slen;

    if (name == NULL || str == NULL)
        return -EINVAL;

    slen = strlen(str);
    p = param_serach(ctx, name);
    if (p) {
        if (strlen(p->val) < slen) {
            param_free(ctx, p);
            goto _alloc;
        }
        p->val[slen] = '\0';
        goto _copy;
    }

_alloc:
    p = param_alloc(ctx, name, slen + 1);
    if (p == NULL)
        return -ENOMEM;

_copy:
    strcpy(p->val, str);
    strcpy(p->name, name);
    return 0;
}

int param_context_getstr(struct param_context *ctx, const char *name, 
    const char **pstr) {
    struct param_node *p;

    if (name == NULL || pstr == NULL)
        return -EINVAL;

    p = param_serach(ctx, name);
    if (p) {
        *pstr = p->val;
        return 0;
    }

    return -ENODATA;
}

int param_context_remove(struct param_context *ctx, const char *name) {
    struct param_node *p, *n;

    os_mtx_lock(&ctx->mtx);
    TAILQ_FOREACH_SAFE(p, &ctx->head, link, n) {
        if (!strcmp(name, p->name)) {
            TAILQ_REMOVE(&ctx->head, p, link);
            os_mtx_unlock(&ctx->mtx);
            general_free(p);
            return 0;
        }
    }
    os_mtx_unlock(&ctx->mtx);
    return -EINVAL;
}

void param_context_clean(struct param_context *ctx) {
    struct param_node *p, *n;
    struct param_list tmp_list;

    TAILQ_INIT(&tmp_list);
    os_mtx_lock(&ctx->mtx);
    TAILQ_SWAP(&ctx->head, &tmp_list, param_node, link);
    os_mtx_unlock(&ctx->mtx);

    TAILQ_FOREACH_SAFE(p, &tmp_list, link, n) {
        TAILQ_REMOVE(&tmp_list, p, link);
        general_free(p);
    }
}

void param_context_dump(struct param_context *ctx) {
    struct param_node *p;

    os_mtx_lock(&ctx->mtx);
    pr_out("domain(%s):\n", ctx->name);
    TAILQ_FOREACH(p, &ctx->head, link) {
        if (isprint(p->val[0]) && 
            isprint(p->val[1]) &&
            isprint(p->val[2]) &&
            isprint(p->val[3])) {
            pr_out("\t%s: %s\n", p->name, p->val);
        } else {
            pr_out("\t%s: %d\n", p->name, *(int *)p->val);
        }
    }
    os_mtx_unlock(&ctx->mtx);
}

void param_dump_all(void) {
    struct param_context *ctx;

    os_mtx_lock(&context_mtx);
    SLIST_FOREACH(ctx, &context_created, link) {
        param_context_dump(ctx);
    }
    os_mtx_unlock(&context_mtx);
}

struct param_context *param_context_find(const char *name) {
    struct param_context *ctx = NULL;

    if (name == NULL)
        return NULL;

    os_mtx_lock(&context_mtx);
    SLIST_FOREACH(ctx, &context_created, link) {
        if (!strcmp(name, ctx->name))
            break;
    }
    os_mtx_unlock(&context_mtx);

    return ctx;
}

int param_context_create(const char *name, struct param_context **pctx) {
    struct param_context *ctx;

    if (name == NULL)
        return -EINVAL;

    if (strlen(name) >= CONTEXT_MAX_NAMELEN)
        return -EINVAL;

    ctx = param_context_find(name);
    if (ctx)
        return -EEXIST;

    ctx = general_calloc(1, sizeof(*ctx));
    if (ctx) {
        os_mtx_init(&ctx->mtx, 0);
        ctx->static_allocated = false;
        strcpy(ctx->name, name);
        TAILQ_INIT(&ctx->head);

        os_mtx_lock(&context_mtx);
        SLIST_INSERT_HEAD(&context_created, ctx, link);
        os_mtx_unlock(&context_mtx);

        *pctx = ctx;
        return 0;
    }

    return -ENOMEM;
}

void param_context_delete(struct param_context *ctx) {
    param_context_clean(ctx);
    if (!ctx->static_allocated) {
        os_mtx_lock(&context_mtx);
        SLIST_REMOVE(&context_created, ctx, param_context, link);
        os_mtx_unlock(&context_mtx);
        general_free(ctx);
    }
}

int param_init(void) {
    os_mtx_init(&_system__params->mtx, 0);
    os_mtx_init(&context_mtx, 0);

    os_mtx_lock(&context_mtx);
    SLIST_INSERT_HEAD(&context_created, _system__params, link);
    os_mtx_unlock(&context_mtx);
    return 0;
}
