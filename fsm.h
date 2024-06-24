/*
 * Copyright 2021 The Chromium OS Authors
 * Copyright 2024 wtcat
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef BASEWORK_FSM_H_
#define BASEWORK_FSM_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

struct fsm_context;

struct fsm_state {
    const char *name;

	/*
	 * Optional method that will be run repeatedly during state machine
	 * loop.
	 */
	void (*run)(struct fsm_context *ctx);

	/* Optional method that will be run when this state is entered */
	void (*entry)(struct fsm_context *ctx);

	/* Optional method that will be run when this state exists */
	void (*exit)(struct fsm_context *ctx);

#ifdef CONFIG_HFSM_SUPPORT
	/*
	 * Optional parent state that contains common entry/run/exit
	 *	implementation among various child states.
	 *	entry: Parent function executes BEFORE child function.
	 *	run:   Parent function executes AFTER child function.
	 *	exit:  Parent function executes AFTER child function.
	 *
	 *	Note: When transitioning between two child states with a shared parent,
	 *	that parent's exit and entry functions do not execute.
	 */
	const struct fsm_state *parent;
#endif
};


struct fsm_context {
	/* Current state the state machine is executing. */
	const struct fsm_state *current;

	/* Previous state the state machine executed */
	const struct fsm_state *previous;

	/*
	 * This value is set by the set_terminate function and
	 * should terminate the state machine when its set to a
	 * value other than zero when it's returned by the
	 * run_state function.
	 */
	int result;

#ifdef CONFIG_HFSM_SUPPORT
	bool new_state;
	bool handled;
#endif
	bool terminate;
	bool exit;
};

#ifdef CONFIG_HFSM_SUPPORT
#define FSM_STATE(_state, _run, _entry, _exit, _parent) \
    [_state] = { \
        .name  = #_state, \
        .entry  = _entry, \
        .run    = _run,   \
        .exit   = _exit,  \
        .parent = _parent \
    }

#else /* !CONFIG_HFSM_SUPPORT */
#define FSM_STATE(_state, _run, _entry, _exit, ...) \
    [_state] = { \
        .name  = #_state, \
        .entry = _entry, \
        .run   = (_run != NULL)? _run: _fsm_default_state,   \
        .exit  = _exit   \
    }
#endif /* CONFIG_HFSM_SUPPORT */

#ifdef CONFIG_HFSM_SUPPORT
static inline void fsm_set_handled(struct fsm_context *ctx) {
	ctx->handled = true;
}
#else
#define fsm_set_handled(ctx) (void)ctx
#endif /* CONFIG_HFSM_SUPPORT */

static inline void fsm_set_terminate(struct fsm_context *ctx, int val) {
	ctx->terminate = true;
	ctx->result    = val;
}
/*
 * _fsm_default_state - private function
 */
void _fsm_default_state(struct fsm_context *ctx);

/*
 * fsm_init - Initialize a state machine
 *
 * @ctx   point to the context of state machine 
 * @state point to intialize state
 */
void fsm_init(struct fsm_context *ctx, const struct fsm_state *state);

/*
 * fsm_switch - Switch to target state from current state
 *
 * @ctx   point to the context of state machine 
 * @state point to target state
 */
void fsm_switch(struct fsm_context *ctx, const struct fsm_state *target);

/*
 * fsm_execute - Execute state machine
 *
 * @ctx   point to the context of state machine 
 * return 0 if success
 */
int  fsm_execute(struct fsm_context *ctx);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_FSM_H_ */
