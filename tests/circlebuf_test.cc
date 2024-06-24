/*
 * Copyright 2024 wtcat
 */

#include "basework/container/circ_buffer.h"
#include "gtest/gtest.h"


TEST(circlebuf, operation) {
    struct circ_buffer circ = {0};
    char buf[17] = {0};
    const char str[] = {
        "0123456789abcdef"
    };
    
    ASSERT_EQ(circbuf_init(&circ, nullptr, 16), 0);
    ASSERT_EQ(circbuf_write(&circ, str, 16), 16);
    ASSERT_LE(circbuf_write(&circ, &str[0], 1), 0);
    ASSERT_EQ(circbuf_read(&circ, buf, 16), 16);
    ASSERT_STREQ(str, buf);
    ASSERT_EQ(circbuf_read(&circ, buf, 16), 0);
    circbuf_uninit(&circ);
}
