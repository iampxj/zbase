/*
 * Copyright 2023 wtcat
 */
#include "gtest/gtest.h"
#include "basework/container/rb.h"



struct flashop_tracer {
    RBTree_Node node;
    uint32_t key;
    // uint16_t erase_count;
    // uint16_t write_count;
};

struct flashop_tree {
    RBTree_Control root;
};

static struct flashop_tree rb_root = {
    RBTREE_INITIALIZER_EMPTY(rb_root)
};

static void *
flashop_node_map(RBTree_Node *node) {
    return (struct flashop_tracer *) node;
}

static bool 
flashop_value_less(const void *left, const RBTree_Node *right) {
    uint32_t key = *(const uint32_t *)left;
    const struct flashop_tracer *rnode = (const struct flashop_tracer *)right;

    return key < rnode->key;
}

static bool 
flashop_value_equal(const void *left, const RBTree_Node *right) {
    uint32_t key = *(const uint32_t *)left;
    const struct flashop_tracer *rnode = (const struct flashop_tracer *)right;

    return key == rnode->key;
}

static struct flashop_tracer *
flashop_find_addr(struct flashop_tree *tree, uint32_t key) {
    return (struct flashop_tracer *)_RBTree_Find_inline(
    &tree->root,
    &key,
    flashop_value_equal,
    flashop_value_less,
    flashop_node_map
    );
}

static void 
flashop_insert(struct flashop_tree *tree, struct flashop_tracer *node) {
  _RBTree_Insert_inline(
    &tree->root,
    &node->node,
    &node->key,
    flashop_value_less
  );
}

static struct flashop_tracer flash_tracers[1000];

TEST(rbtree, operation) {
    for (int i = 0; i < 1000; i++) {
        struct flashop_tracer *p = &flash_tracers[i];
        p->key = i;
        flashop_insert(&rb_root, p);
    }

    for (int i = 0; i < 1000; i++) {
        struct flashop_tracer *p = flashop_find_addr(&rb_root, i);
        ASSERT_NE(p, nullptr);
        ASSERT_EQ(p->key, i);
    }
}