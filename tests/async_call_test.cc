/*
 * Copyright 2023 wtcat
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "basework/async_call.h"
#include "gtest/gtest.h"


#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wcast-function-type"
#endif

static void print_hello(void) {
    printf("Hello World\n");
}

static int calc_add3(int a, int b, int c) {
    return a + b + c;
}

static void call_done(const struct call_param *p) {
    printf("caluc-result: %d\n", (int)p->result);
}

TEST(async_call, api_test) {
    ASSERT_EQ(async_call0(print_hello), 0);
    ASSERT_EQ(async_done_call3(calc_add3, call_done, 1, 2, 3), 0);
    usleep(1000000);
}