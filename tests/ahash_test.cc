/*
 * Copyright 2024 wtcat
 */
#include "basework/container/ahash.h"
#include "basework/log.h"
#include "basework/tools/flash_trace.h"
#include "gtest/gtest.h"

#define HASH_LOGSZ 8
#define HASH_ELEM_NUM 1000

extern "C" {
struct flash_node {
    AHASH_NODE_BASE
    uint32_t address;
};

static int foreach_count;

static bool iterator(struct hash_node *node, void *arg) {
    // struct flash_node *p = (struct flash_node *)node;
    foreach_count++;
    return true;
}

}

TEST(ahash, operation) {
    uint32_t hash_buffer[(AHASH_CALC_BUFSZ(HASH_ELEM_NUM, HASH_LOGSZ, sizeof(struct flash_node)) + 3) / 4];
    struct hash_header header;
    struct flash_node *p;

    ASSERT_EQ(ahash_init(&header, hash_buffer, sizeof(hash_buffer), sizeof(flash_node), HASH_LOGSZ), 0);

    for (size_t i = 0; i < HASH_ELEM_NUM; i++) {
        ASSERT_EQ(ahash_add(&header, (const void *)(0x10000000 + i), (struct hash_node **)&p), 0);
        p->address = i;
    }

    ahash_visit(&header, iterator, NULL);
    ASSERT_EQ(foreach_count, HASH_ELEM_NUM);
    foreach_count = 0;

    for (size_t i = 0; i < HASH_ELEM_NUM; i++) {
        struct flash_node *node = (struct flash_node *)ahash_find(&header, (void *)(0x10000000 + i));
        ASSERT_NE(node, nullptr);
        ASSERT_EQ(node->address, i);
        ahash_del(&header, (struct hash_node *)node);
    }

    ahash_visit(&header, iterator, NULL);
    ASSERT_EQ(foreach_count, 0);
}


static int erase_test(uint32_t addr, size_t n) {
    int err;

    for (size_t i = 0; i < n; i++) {
        err = flash_trace_erase(addr, 4096);
        if (err)
            break;
    }
    return err;
}

TEST(ahash, flash_trace) {
    ASSERT_EQ(flash_trace_init(), 0);

    //Erase
    ASSERT_EQ(erase_test(0x80000000, 30), 0);
    ASSERT_EQ(erase_test(0x80002000, 50), 0);
    ASSERT_EQ(erase_test(0x80008000, 100), 0);
    ASSERT_EQ(erase_test(0x80028000, 200), 0);
    ASSERT_EQ(erase_test(0x81028000, 5), 0);
    ASSERT_EQ(erase_test(0x82028000, 40), 0);
    ASSERT_EQ(erase_test(0x83028000, 7), 0);
    ASSERT_EQ(erase_test(0x83048000, 69), 0);

    flash_trace_dump(true);
}