#include <stdio.h>
#include <unistd.h>


#include "basework/fsm.h"


static void state_machine_test(void);

int main(int argc, char *argv[]) {

    state_machine_test();

    return 0;
}

/* List of demo states */
enum demo_state { PARENT, S0, S1, S2 };

/* User defined object */
struct s_object {
    /* This must be first */
    struct fsm_context ctx;

    /* Other state specific data add here */
} s_obj;

/* Forward declaration of state table */
static const struct fsm_state demo_states[];

/* Parent State */
static void parent_entry(struct fsm_context *ctx)
{
    printf("%s\n", __func__);
}

static void parent_exit(struct fsm_context *ctx)
{
    printf("%s\n", __func__);
}

/* State S0 */
static void s0_run(struct fsm_context *ctx)
{
    printf("%s\n", __func__);
    fsm_switch(ctx, &demo_states[S1]);
}

/* State S1 */
static void s1_run(struct fsm_context *ctx)
{
    printf("%s\n", __func__);
    fsm_switch(ctx, &demo_states[S2]);
}

/* State S2 */
static void s2_run(struct fsm_context *ctx)
{
    printf("%s\n", __func__);
    fsm_switch(ctx, &demo_states[S0]);
}

/* Populate state table */
static const struct fsm_state demo_states[] = {
    /* Parent state does not have a run action */
    FSM_STATE(PARENT, NULL, parent_entry, parent_exit, NULL),

    /* Child states do not have entry or exit actions */
    FSM_STATE(S0, s0_run, NULL, NULL, &demo_states[PARENT]),
    FSM_STATE(S1, s1_run, NULL, NULL, &demo_states[PARENT]),

    /* State S2 do ot have entry or exit actions and no parent */
    FSM_STATE(S2, s2_run, NULL, NULL, NULL),
};


static void state_machine_test(void) 
{
    fsm_init(&s_obj.ctx, &demo_states[S0]);

    while (fsm_execute(&s_obj.ctx) == 0)
        usleep(1000000);
}