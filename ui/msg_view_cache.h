/*
 * Copyright 2023 wtcat
 */
#ifndef MSG_VIEW_CACHE_H_
#define MSG_VIEW_CACHE_H_

#include <stdbool.h>
#include <stddef.h>

#include "basework/ui/msg_view_presenter.h"

#ifdef __cplusplus
extern "C" {
#endif

int _message_view_cache_get(const struct message_presenter *p, int idx, 
    struct message_content** pmsg, bool force_update);
int _message_view_cache_remove(const struct message_presenter* p, int idx);
void _message_view_cache_free(const struct message_presenter* p, bool erase);
bool _message_view_is_cache_full(const struct message_presenter* p);
size_t _message_view_cache_size(void);

#ifdef __cplusplus
}
#endif
#endif /* MSG_VIEW_CACHE_H_ */
