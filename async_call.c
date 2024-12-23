/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include "basework/rq.h"
#include "basework/async_call.h"

static void rq_async_execute(void *arg, size_t size) {
    struct call_param *cp = (struct call_param *)arg;
    (void) size;
    cp->result = cp->fn(cp->a1, cp->a2, cp->a3, cp->a4, cp->a5);
    if (cp->done)
        cp->done(cp);
}

int __async_call0(async_fn_t fn, bool urgent) {
    if (fn == NULL)
        return -EINVAL;
    if (urgent)
        return rq_submit_urgent((void (*)(void *))(void *)fn, NULL);
    return rq_submit((void (*)(void *))(void *)fn, NULL);
}

int __async_call1(async_fn_t fn, uintptr_t a1, bool urgent) {
    if (fn == NULL)
        return -EINVAL;
    if (urgent)
        return rq_submit_urgent((void (*)(void *))(void *)fn, (void *)a1);
    return rq_submit((void (*)(void *))(void *)fn, (void *)a1);
}

int __async_call(async_fn_t fn, uintptr_t a1, uintptr_t a2, 
    uintptr_t a3, uintptr_t a4, uintptr_t a5, async_done_t done, bool urgent) {
    struct call_param param;
    if (fn == NULL)
        return -EINVAL;
    param.fn = fn;
    param.a1 = a1;
    param.a2 = a2;
    param.a3 = a3;
    param.a4 = a4;
    param.a5 = a5;
    param.result = 0;
    param.done = done;
    if (urgent)
        return rq_submit_urgent_cp(rq_async_execute, &param, sizeof(param));
    return rq_submit_cp(rq_async_execute, &param, sizeof(param));
}

int ___async_call(struct call_param *param, async_fn_t fn, uintptr_t a1, uintptr_t a2, 
    uintptr_t a3, uintptr_t a4, uintptr_t a5, async_done_t done) {
    if (fn == NULL)
        return -EINVAL;
    param->fn = fn;
    param->a1 = a1;
    param->a2 = a2;
    param->a3 = a3;
    param->a4 = a4;
    param->a5 = a5;
    param->result = 0;
    param->done = done;
    return rq_submit((void *)rq_async_execute, param);
}
