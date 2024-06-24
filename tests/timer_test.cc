/*
 * Copyright 2022 wtcat
 */
#if defined(__ZEPHYR__)
#define restrict
#include <posix/unistd.h>
#include <posix/semaphore.h>
#else
#include <semaphore.h>
#endif /* __ZEPHYR__ */

#ifdef _WIN32
#include <time.h>
#include <thread>
#endif

#include "basework/os/osapi_timer.h"
#include "gtest/gtest.h"

static volatile int timer_cb_1_counter;

static void timer_cb_1(os_timer_t timer, void *arg) {
    if (timer_cb_1_counter < 100) {
        timer_cb_1_counter++;
        os_timer_mod(timer, 10);
    } else {
        sem_post((sem_t *)arg);
    }
}

TEST(timer, run) {
    struct timespec tv;
    os_timer_t timer;
    sem_t sem;
    int err;

    sem_init(&sem, 0, 0);
    err = os_timer_create(&timer, timer_cb_1, &sem, false);
    ASSERT_EQ(err, 0);

    err = os_timer_add(timer, 1000);
    ASSERT_EQ(err, 0);

#ifdef _WIN32
    timespec_get(&tv, TIME_UTC);
#else
    clock_gettime(CLOCK_REALTIME, &tv);
#endif //_WIN32
    tv.tv_sec += 3;
    err = sem_timedwait(&sem, &tv);
    EXPECT_EQ(err, 0);

#ifdef _WIN32
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
#else
    usleep(200);
#endif
    ASSERT_EQ(timer_cb_1_counter, 100);

    os_timer_destroy(timer);
}
