/*
 * Copyright 2023 wtcat
 */

#include <stdio.h>
#include "basework/lib/msg_storage.h"

#include "gtest/gtest.h"

extern "C" {

#define NR_MSGS 2000

static int _fixed_msg_count;
static struct msg_payload _fixed_messages[NR_MSGS];

static void message_construct(void) {
	const char *contacts[] = {
		"Tom", "Jack", "Lucy", "Lily", "Lulu"
	};
	const char *texts[] = {
		"Hello!",
		"How are you?",
		"Let's go!",
		"Oh, shit!!!",
		"What's you name?"
	};
	int start = 0; 
	int name_idx, text_idx;

	while (start < NR_MSGS) {
		if (!_fixed_messages[start].m_len)
			break;
		start++;
	}
	while (start < NR_MSGS) {
		name_idx = rand() % (sizeof(contacts) / sizeof(contacts[0]));
		text_idx = rand() % (sizeof(texts) / sizeof(texts[0]));
		_fixed_messages[start].timestamp = 0;
		_fixed_messages[start].id = start;
		_fixed_messages[start].type = rand() % 20 + 1;
		_fixed_messages[start].c_len = strlen(contacts[name_idx]);
		strncpy(_fixed_messages[start].contact, contacts[name_idx], 
			sizeof(_fixed_messages[start].contact) - 1);
		_fixed_messages[start].m_len = strlen(contacts[text_idx]);
		strncpy(_fixed_messages[start].buffer, texts[text_idx],
			sizeof(_fixed_messages[start].buffer) - 1);
		start++;
	}
	_fixed_msg_count = start;
}

}

TEST(msg_storage, save) {
    ASSERT_TRUE(msg_file_backend_init() == 0);
    message_construct();
    for (int i = 0; i < _fixed_msg_count; i++) {
        ASSERT_TRUE(msg_storage_write(&_fixed_messages[i]) == 0);
        ASSERT_GE(msg_storage_get_newidx(), 0);
    }

    ASSERT_LT(msg_storage_get_newidx(), 0);
    printf("message numbers: %d\n", (int)msg_storage_numbers());
    MSG_STORAGE_FOREACH(i) {
        struct msg_payload msg = {0};
        ASSERT_TRUE(msg_storage_read(i, &msg) == 0);
        printf("id(%d) contact(%s) text(%s) \n", i, msg.contact, msg.buffer);
    }
  
    ASSERT_TRUE(msg_storage_flush() == 0);
    msg_storage_remove(1);
    msg_storage_deinit();

    printf("Reopen message:\n");
    ASSERT_TRUE(msg_storage_init() == 0);
    MSG_STORAGE_FOREACH(i) {
        struct msg_payload msg = {0};
        ASSERT_TRUE(msg_storage_read(i, &msg) == 0);
        printf("id(%d) contact(%s) text(%s) \n", i, msg.contact, msg.buffer);
        // msg_storage_remove(i);
    }

    ASSERT_TRUE(msg_storage_clean() == 0);
    ASSERT_TRUE(msg_storage_deinit() == 0);
}
