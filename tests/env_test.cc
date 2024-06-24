/*
 * Copyright 2022 wtcat
 */
#include <stdio.h>
#include <string.h>

#include "basework/env.h"
#include "gtest/gtest.h"

#define __restrict 

static const char *list_envs[] = {
    "runtime=20\n",
    "update=yes\n",
    NULL
};

static char env_storage[4096];

struct rd_context {
    int offset;
};

static int env_readline(void *ctx, void *buffer, size_t size) {
    struct rd_context *rdc = (struct rd_context *)ctx;
    size_t rlen;
    if (list_envs[rdc->offset] == NULL)
        return 0;
    rlen = strlen(list_envs[rdc->offset]);
    if (rlen > size)
        rlen = size;
    strncpy((char *)buffer, list_envs[rdc->offset], rlen);
    rdc->offset++;
    return rlen;
}

TEST(environent_var, api_test) {

    ASSERT_EQ(env_set("os", "posix", 1), 0);
    ASSERT_EQ(env_set("path", "/home", 1), 0);
    ASSERT_EQ(env_set("ip", "192.168.1.1", 1), 0);

    ASSERT_STREQ(env_get("os"), "posix");
    ASSERT_STREQ(env_get("path"), "/home");
    ASSERT_STREQ(env_get("ip"), "192.168.1.1");

    struct rd_context ctx = {0};
    ASSERT_EQ(env_load(env_readline, &ctx), 0);
    ASSERT_STREQ(env_get("runtime"), "20");
    ASSERT_STREQ(env_get("update"), "yes");

    ASSERT_EQ(env_unset("ip"), 0);
    ASSERT_EQ(env_get("ip"), nullptr);

    // env_flush(
    //     [](void *ctx, void *buffer, size_t size) -> int {
    //         printf("%s", (const char *)buffer);
    //         return size;
    //     }, 
    // nullptr);
}

TEST(environment_var, ram) {
    ASSERT_EQ(env_set("os", "posix", 0), 0);
    ASSERT_EQ(env_set("path", "/home", 0), 0);
    ASSERT_EQ(env_set("ip", "192.168.1.1", 0), 0);
    ASSERT_EQ(env_set("runtime", "20", 0), 0);
    ASSERT_EQ(env_set("update", "yes", 0), 0);
    ASSERT_GT(env_flush_ram(env_storage, sizeof(env_storage)), 0);
    env_reset();

    ASSERT_EQ(env_get("os"), nullptr);
    ASSERT_EQ(env_get("path"), nullptr);
    ASSERT_EQ(env_get("ip"), nullptr);
    ASSERT_EQ(env_get("runtime"), nullptr);
    ASSERT_EQ(env_get("update"), nullptr);

    ASSERT_EQ(env_load_ram(env_storage, sizeof(env_storage)), 0);
    ASSERT_STREQ(env_get("os"), "posix");
    ASSERT_STREQ(env_get("path"), "/home");
    ASSERT_STREQ(env_get("ip"), "192.168.1.1");   
    ASSERT_STREQ(env_get("runtime"), "20");
    ASSERT_STREQ(env_get("update"), "yes");
}