/*
 * Copyright 2022 wtcat
 */
#include <unistd.h>

#include "basework/dev/clock_timer.h"

#include "gtest/gtest.h"


static int vsec;

static void clock_second_cb(void) {
    printf("Second: %d\n", vsec);
    vsec++;
}

TEST(clock_timer, second_timeout) {
    static struct clock_callout sec;

    clock_timer_init();
    clock_callout_add(&sec, clock_second_cb, CLK_SEC_TIMEOUT);
    while (vsec < 5)
        usleep(1000000);
}
