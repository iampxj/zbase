/*
 * Copyright 2023 wtcat
 */
#ifndef BASEWORK_UI_INPUT_ENCODER_H_
#define BASEWORK_UI_INPUT_ENCODER_H_

#include <stdbool.h>
#include <stdint.h>

#include "basework/compiler.h"
#include "basework/errno.h"


#ifdef __cplusplus
extern "C"
#endif

#ifndef rte_likely
# define rte_likely(x) x
#endif

enum encoder_policy {
    ENCODER_LVGL_GROUP_POLICY,
    ENCODER_TP_SIMULATE_POLICY,
    ENCODER_POLICY_MAX
};

struct encoder_inops {
    void (*push)(int diff, bool pressed);
    int policy;
};

struct encoder_context {
    const struct encoder_inops *e_ops;
    int (*policy_switch)(struct encoder_context *ctx, int policy);
    uint32_t wkcnt;
    bool disabled;
};

#ifdef CONFIG_ENCODER_INPUT_FRAMEWORK
extern struct encoder_context _curr_encoder_context;

#define ENCODER_PUSH(_diff, _pressed) \
    input_encoder_push(&_curr_encoder_context, _diff, _pressed)

#define ENCODER_SWITCH_POLICY(_policy) \
    input_encoder_switch(&_curr_encoder_context, _policy)

#define ENCODER_INPUT_ENABLE(_en) \
    input_encoder_enable(&_curr_encoder_context, _en)

#define ENCODER_GET_WORKCNT() \
    input_encoder_get_wkcnt(&_curr_encoder_context)

static inline uint32_t input_encoder_get_wkcnt(struct encoder_context *ctx) {
    return ctx->wkcnt;
}

static inline void input_encoder_enable(struct encoder_context *ctx, bool en) {
    ctx->disabled = !en;
}

static inline void input_encoder_push(struct encoder_context *ctx, 
    int diff, bool pressed) {
    if (rte_likely(!ctx->disabled))
        ctx->e_ops->push(diff, pressed);
    ctx->wkcnt++;
}

static inline int input_encoder_switch(struct encoder_context *ctx,
    int policy) {
    if (ctx->policy_switch)
        return ctx->policy_switch(ctx, policy);
    return -ENOSYS;
}

#else
#define ENCODER_PUSH(...) (void)0
#define ENCODER_SWITCH_POLICY(...) (void)0
#define ENCODER_INPUT_ENABLE(...)  (void)0
#define ENCODER_GET_WORKCNT() (0)

#endif /* CONFIG_ENCODER_INPUT_FRAMEWORK */

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_UI_INPUT_ENCODER_H_ */