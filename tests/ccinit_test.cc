/*
 * Copyright 2022 wtcat
 */
#if defined(__ZEPHYR__)
#define restrict
#include <posix/unistd.h>
#include <posix/time.h>
#else
#include <unistd.h>
#endif /* __ZEPHYR__ */
#include <time.h>
#include "basework/ccinit.h"
#include "gtest/gtest.h"

static int64_t time_array[4];
static int time_index;

static int64_t get_time(void) {
    struct timespec tm;
    clock_gettime(CLOCK_REALTIME, &tm);
    return tm.tv_sec * 1000 + tm.tv_nsec / 1000000;
}

CC_INIT(test_device_init, kDeviceOrder, 0) {
    time_array[time_index++] = get_time();
    usleep(100000);
    return 0;
}

CC_INIT(test_device_init_10, kDeviceOrder, 10) {
    time_array[time_index++] = get_time();
    usleep(100000);
    return 0;
}

CC_INIT(test_app_init, kApplicationOrder, 0) {
    time_array[time_index++] = get_time();
    usleep(100000);
    return 0;
}

CC_INIT(test_app_init_10, kApplicationOrder, 10) {
    time_array[time_index++] = get_time();
    usleep(100000);
    return 0;
}

TEST(test_ccinit, order) {
    ASSERT_EQ(time_index, 0);

    base::cc_initializer_run(kDeviceOrder, kDeviceOrder);
    ASSERT_EQ(time_index, 2);
    ASSERT_GT(time_array[0], 0);
    ASSERT_GT(time_array[1], time_array[0]);
    ASSERT_EQ(time_array[2], 0);
    ASSERT_EQ(time_array[3], 0);
    
    base::cc_initializer_run(kApplicationOrder, kApplicationOrder);
    ASSERT_EQ(time_index, 4);
    ASSERT_GT(time_array[2], 0);
    ASSERT_GT(time_array[3], time_array[2]);
    ASSERT_GT(time_array[2], time_array[1]);
}

