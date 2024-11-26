/*
 * Copyright 2024 wtcat
 */

#include "basework/lib/libenv.h"
#include "basework/malloc.h"

#include "gtest/gtest.h"

extern "C" {
static void *env_malloc(size_t size) {
    return general_malloc(size);
}

static void *env_realloc(void *ptr, size_t size) {
    return general_realloc(ptr, size);
}

static void env_release(void *ptr) {
    general_free(ptr);
}

static const struct env_alloctor env_allocator = {
    env_malloc,
    env_realloc,
    env_release
};

}

TEST(libenv, test) {
    rte_initenv(&env_allocator);

    ASSERT_EQ(rte_getenv("AppLink"), nullptr);
    ASSERT_EQ(rte_getenv("AppName"), nullptr);
    ASSERT_EQ(rte_getenv("BtName"), nullptr);
    ASSERT_EQ(rte_getenv("LanguageDefault"), nullptr);
    ASSERT_EQ(rte_getenv("SourceDevID"), nullptr);
    
    ASSERT_EQ(rte_setenv("AppLink", "cn.bing.com.xxx", 1), 0);
    ASSERT_EQ(rte_setenv("AppName", "Fitbeing", 1), 0);
    ASSERT_EQ(rte_setenv("BtName", "next-ble", 1), 0);
    ASSERT_EQ(rte_setenv("LanguageDefault", "english", 1), 0);
    ASSERT_EQ(rte_setenv("SourceDevID", "1003", 1), 0);

    ASSERT_STREQ(rte_getenv("AppLink"), "cn.bing.com.xxx");
    ASSERT_STREQ(rte_getenv("AppName"), "Fitbeing");
    ASSERT_STREQ(rte_getenv("BtName"), "next-ble");
    ASSERT_STREQ(rte_getenv("LanguageDefault"), "english");
    ASSERT_STREQ(rte_getenv("SourceDevID"), "1003");

    ASSERT_EQ(rte_unset("AppLink"), 0);
    ASSERT_EQ(rte_unset("AppName"), 0);
    ASSERT_EQ(rte_unset("BtName"), 0);
    ASSERT_EQ(rte_unset("LanguageDefault"), 0);
    ASSERT_EQ(rte_unset("SourceDevID"), 0);
 
    ASSERT_EQ(rte_getenv("AppLink"), nullptr);
    ASSERT_EQ(rte_getenv("AppName"), nullptr);
    ASSERT_EQ(rte_getenv("BtName"), nullptr);
    ASSERT_EQ(rte_getenv("LanguageDefault"), nullptr);
    ASSERT_EQ(rte_getenv("SourceDevID"), nullptr);
}
