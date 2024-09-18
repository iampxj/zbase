/*
 * Copyright 2024 wtcat
 */
#include <stdio.h>
#include "basework/idr.h"

#include "gtest/gtest.h"

IDR_DEFINE(test, 10, 3)

TEST(idr, test) {
    uint32_t varray[4] = {1, 2, 3, 4};
    int ids[4] = {0};
    struct idr *idr_inst = nullptr;

    //Initialize
	idr_inst = test_idr_create();
    ASSERT_NE(idr_inst, nullptr);

    //Allocate
    ids[0] = idr_alloc(idr_inst, &varray[0]);
    ASSERT_GT(ids[0], 0);
    ids[1] = idr_alloc(idr_inst, &varray[1]);
    ASSERT_GT(ids[1], 0);
    ids[2] = idr_alloc(idr_inst, &varray[2]);
    ASSERT_GT(ids[2], 0);
    ASSERT_LT(idr_alloc(idr_inst, &varray[3]), 0);

    //Find
    ASSERT_NE(idr_find(idr_inst, ids[0]), nullptr);
    ASSERT_NE(idr_find(idr_inst, ids[1]), nullptr);
    ASSERT_NE(idr_find(idr_inst, ids[2]), nullptr);
    ASSERT_EQ(idr_find(idr_inst, ids[3]), nullptr);

    //Remove
    ASSERT_EQ(idr_remove(idr_inst, ids[0]), 0);
    ASSERT_EQ(idr_remove(idr_inst, ids[1]), 0);
    ASSERT_EQ(idr_remove(idr_inst, ids[2]), 0);
    ASSERT_NE(idr_remove(idr_inst, ids[3]), 0);

    //Find
    ASSERT_EQ(idr_find(idr_inst, ids[0]), nullptr);
    ASSERT_EQ(idr_find(idr_inst, ids[1]), nullptr);
    ASSERT_EQ(idr_find(idr_inst, ids[2]), nullptr);
    ASSERT_EQ(idr_find(idr_inst, ids[3]), nullptr);
}