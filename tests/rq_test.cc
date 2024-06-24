/*
 * Copyright 2022 wtcat
 */
#include <stdio.h>
#include <string.h>
#if defined(__ZEPHYR__)
#define restrict
#include <posix/unistd.h>
#include <posix/semaphore.h>
#else
#include <semaphore.h>
#endif /* __ZEPHYR__ */

#include "basework/rq.h"
#include "basework/malloc.h"
#include "gtest/gtest.h"

static int test_counter;

static const char *test_text[] = {
    "The home for all developers",
    "Welcome to your personal dashboard, where you can find an introduction to how GitHub works",
    "See Documentation/core-api/circular-buffers.rst for more information",
    "/* SPDX-License-Identifier: GPL-2.0 */"
};

static void work_1(void *arg) {
    test_counter++;
    sem_post((sem_t *)arg);
}

static void work_2(void *arg) {
    printf("hello, world\n");
}
static struct rq_node rqnode;

TEST(runqueue, submit) {
    sem_t sync;

    rqnode.exec = work_2;
    rqnode.name = "TEST";
    rqnode.arg = NULL;
    rq_submit_entry(&rqnode);
    rq_submit_entry(&rqnode);


    test_counter = 0;
    sem_init(&sync, 0, 0); 
    for (int i = 0; i < 100; i++) {
        rq_submit(work_1, &sync);
        sem_wait(&sync);
    }
    usleep(20000);
    ASSERT_EQ(test_counter, 100);
}


static void work_2(void *data, size_t size) {
    static const char **p = test_text;
    static int i = 0;

    char buffer[256] = {0};
    memcpy(buffer, data, size);
    ASSERT_EQ(memcmp(buffer, p[i++], size), 0);
}

static void work_3(void *data, size_t size) {
    ASSERT_EQ(size, (size_t)12);
    ASSERT_STREQ((const char *)data, "abcdeghijkl");
}

TEST(runqueue, submit_cp) {
    for (size_t i = 0; i < sizeof(test_text)/sizeof(test_text[0]); i++) {
        rq_submit_cp(work_2, (void *)test_text[i], strlen(test_text[i]));
    }
    usleep(50000);

    char buffer[12] = {"abcdeghijkl"};
    for (int i = 0; i < 50; i++) {
        ASSERT_EQ(rq_submit_cp(work_3, (void *)buffer, sizeof(buffer)), 0);
        usleep(50000);
    }
}
