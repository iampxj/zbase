/*
 * Copyright 2021 The Chromium OS Authors
 * Copyright 2024 wtcat
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<fsm>: " fmt
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "basework/fsm.h"
#include "basework/generic.h"
#include "basework/log.h"

void _fsm_default_state(struct fsm_context *ctx) {}

#ifdef CONFIG_HFSM_SUPPORT
static bool shared_parent(const struct fsm_state *test_state,
    const struct fsm_state *target_state) {
	const struct fsm_state *state = test_state;

	while (state != NULL) {
		if (target_state == state)
			return true;

		state = state->parent;
	}

	return false;
}

static bool last_state_shared_parent(struct fsm_context *const ctx,
    const struct fsm_state *state) {
	/* 
	 * Get parent state of previous state 
	 */
	if (ctx->previous == NULL)
		return false;

	return shared_parent(ctx->previous->parent, state);
}

static const struct fsm_state *get_child_of(const struct fsm_state *states,
    const struct fsm_state *parent) {
	const struct fsm_state *tmp = states;

	do {
		if (tmp->parent == parent)
			break;

		if (tmp->parent == NULL)
			break;

		tmp = tmp->parent;
	} while (true);

	return tmp;
}

static bool fsm_ancestor_entry(struct fsm_context *const ctx,
    const struct fsm_state *target) {
	const struct fsm_state *to_execute;

	for (to_execute = get_child_of(target, NULL);
		 to_execute != NULL && to_execute != target;
		 to_execute = get_child_of(target, to_execute)) {
		/* 
		 * Execute parent state's entry 
		 */
		if (!last_state_shared_parent(ctx, to_execute) && to_execute->entry) {
			to_execute->entry(ctx);

			/* 
			 * No need to continue if terminate was set 
			 */
			if (ctx->terminate)
				return true;
		}
	}

	return false;
}

static bool fsm_ancestor_run(struct fsm_context *ctx) {
	/* 
	 * Execute all run actions in reverse order 
	 *
	 * Return if the current state switched states 
	 */
	if (ctx->new_state) {
		ctx->new_state = false;
		return false;
	}

	/* 
	 * Return if the current state terminated 
	 */
	if (ctx->terminate)
		return true;

	if (ctx->handled) {
		/* 
		 * Event was handled by this state. Stop propagating 
		 */
		ctx->handled = false;
		return false;
	}

	/* 
	 * Try to run parent run actions 
	 */
	for (const struct fsm_state *tmp = ctx->current->parent;
		tmp != NULL;
		tmp = tmp->parent) {

		if (tmp->run) {
			tmp->run(ctx);

			/* 
			 * No need to continue if terminate was set 
			 */
			if (ctx->terminate)
				return true;

			if (ctx->new_state)
				break;

			if (ctx->handled) {
				/* 
				 * Event was handled by this state. Stop propagating 
				 */
				ctx->handled = false;
				break;
			}
		}
	}

	ctx->new_state = false;

	/* 
	 * All done executing the run actions 
	 */
	return false;
}

static bool fsm_ancestor_exit(struct fsm_context *const ctx,
							  const struct fsm_state *target) {
	/* 
	 * Execute all parent exit actions in reverse order 
	 */
	for (const struct fsm_state *tmp = ctx->current->parent; tmp != NULL;
		 tmp = tmp->parent) {
		if ((target == NULL || 
            !shared_parent(target->parent, tmp)) && tmp->exit) {
			tmp->exit(ctx);

			/* 
			 * No need to continue if terminate was set
			 */
			if (ctx->terminate)
				return true;
		}
	}

	return false;
}
#endif /* CONFIG_HFSM_SUPPORT */

void fsm_init(struct fsm_context *ctx, const struct fsm_state *state) {
	ctx->exit      = false;
	ctx->terminate = false;
	ctx->current   = state;
	ctx->previous  = NULL;
	ctx->result    = 0;

#ifdef CONFIG_HFSM_SUPPORT
	ctx->new_state = false;
	if (fsm_ancestor_entry(ctx, state))
		return;
#endif

	/* 
	 * Now execute the initial state's entry action 
	 */
	if (state->entry)
		state->entry(ctx);
}

void fsm_switch(struct fsm_context *ctx, const struct fsm_state *target) {
	const struct fsm_state *curr;
	/*
	 * It does not make sense to call set_state in an exit phase of a state
	 * since we are already in a transition; we would always ignore the
	 * intended state to transition into.
	 */
	if (ctx->exit) {
		pr_err("Calling %s from exit action", __func__);
		return;
	}

	ctx->exit = true;
	curr = ctx->current;

	/* 
	 * Execute the current states exit action 
	 */
	if (curr->exit) {
		curr->exit(ctx);

		/*
		 * No need to continue if terminate was set in the
		 * exit action
		 */
		if (ctx->terminate)
			return;
	}

#ifdef CONFIG_HFSM_SUPPORT
	ctx->new_state = true;
	if (fsm_ancestor_exit(ctx, target))
		return;
#endif

	ctx->exit = false;

	/* 
	 * update the state variables 
	 */
	ctx->previous = curr;
	ctx->current  = target;

#ifdef CONFIG_HFSM_SUPPORT
	if (fsm_ancestor_entry(ctx, target))
		return;
#endif

	/*
	 * Now execute the target entry action
	 */
	if (curr->entry) {
		curr->entry(ctx);
		/*
		 * If terminate was set, it will be handled in the
		 * fsm_execute function
		 */
	}
}

int fsm_execute(struct fsm_context *ctx) {
	/*
	 * No need to continue if terminate was set
	 */
	if (rte_unlikely(ctx->terminate))
		return ctx->result;

#ifdef CONFIG_HFSM_SUPPORT
	if (ctx->current->run)
		ctx->current->run(ctx);
#else
	ctx->current->run(ctx);
#endif

#ifdef CONFIG_HFSM_SUPPORT
	if (fsm_ancestor_run(ctx))
		return ctx->result;
#endif

	return 0;
}
