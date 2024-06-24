/*
 * Copyright 2023 wtcat 
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <errno.h>
#include <string.h>
#ifdef CONFIG_SIMULATOR
#include <stdio.h>
#endif
#include "basework/log.h"
#include "basework/ui/msg_view_cache.h"
#include "basework/malloc.h"


#ifdef MESSAGE_VIEW_DEBUG
# ifdef CONFIG_SIMULATOR
# define ui_dbg(fmt, ...) printf("<message_cache>: "fmt, ##__VA_ARGS__)
# else
# define ui_dbg(fmt, ...) pr_out("<message_cache>: "fmt, ##__VA_ARGS__)
# endif /* CONFIG_SIMULATOR */
#else
#define ui_dbg(fmt, ...) (void)0
#endif

#define MSG_HASH_LOG 3
#define MSG_HASH_SIZE (1 << MSG_HASH_LOG)
#define MSG_HASH_MASK (MSG_HASH_SIZE - 1)
#define MSG_HASH(idx) (((idx) ^ ((idx) >> 3)) & MSG_HASH_MASK)

struct msg_view_cache {
    struct message_content msg;
    struct msg_view_cache* next;
};

static struct msg_view_cache *_cache_head[MSG_HASH_SIZE];
static size_t _msg_cache_count;

#ifdef MESSAGE_VIEW_DEBUG
static void _message_view_cache_dump(void) {
    ui_dbg("*** Message view-cache dump:\n");
    for (int i = 0; i < MSG_HASH_SIZE; i++) {
        for (struct msg_view_cache* next, *curr = _cache_head[i]; 
            curr != NULL; curr = next) {
            next = curr->next;
            ui_dbg("\tcache offset(%d): id(%d)\n", i, curr->msg.id);
        }
    }
}
#endif

static struct msg_view_cache* _message_cache_find(int idx) {
    struct msg_view_cache* cache = _cache_head[MSG_HASH(idx)];
    while (cache) {
        if (cache->msg.id == idx)
            return cache;
        cache = cache->next;
    }
    return NULL;
}

int _message_view_cache_get(const struct message_presenter *p, int idx, 
    struct message_content** pmsg, bool force_update) {
    struct msg_view_cache* cache;
    int ofs, err;
    bool found = false;

    if (!p || !pmsg)
        return -EINVAL;

    cache = _message_cache_find(idx);
    if (cache) {
        ui_dbg("message(%d) cache hit\n", idx);
        cache->msg.cache_hit = true;
        found = true;
        if (force_update)
            goto _next;
        goto _found;
    }

    cache = general_malloc(sizeof(*cache));
    if (!cache) 
        return -ENOMEM;
    memset(cache, 0, sizeof(*cache));
    _msg_cache_count++;
    ui_dbg("malloc cache pointer: %p message(%d) count(%d)\n", 
        cache, idx, _msg_cache_count);
 #ifdef MESSAGE_VIEW_DEBUG
    if (_msg_cache_count > CONFIG_MAX_MESSAGES)
        _message_view_cache_dump();
#endif

_next:
    err = ui_message_read(p, idx, &cache->msg);
    if (err) {
        _msg_cache_count -= !found;
        general_free(cache);
        return err;
    }

    cache->msg.id = idx;
    if (!found) {
        ofs = MSG_HASH(idx);
        cache->next = _cache_head[ofs];
        _cache_head[ofs] = cache;
    }

_found:
    *pmsg = &cache->msg;
    return 0;
}

int _message_view_cache_remove(const struct message_presenter* p, int idx) {
    struct msg_view_cache** pcache = &_cache_head[MSG_HASH(idx)];
    while (*pcache) {
        if ((*pcache)->msg.id == idx) {
            struct msg_view_cache* victim = *pcache;
            *pcache = (*pcache)->next;
            general_free(victim);
            if (_msg_cache_count > 0)
                _msg_cache_count--;
            ui_dbg("free cache pointer: %p and the message(%d) has been removed (remain:%d)\n", 
                victim, idx, _msg_cache_count);
            return ui_message_remove(p, idx);
        }
        pcache = &(*pcache)->next;
    }
    return -ENODATA;
}

void _message_view_cache_free(const struct message_presenter* p, bool erase) {
    for (int i = 0; i < MSG_HASH_SIZE; i++) {
        for (struct msg_view_cache* next, *curr = _cache_head[i]; 
            curr != NULL; curr = next) {
            next = curr->next;
            ui_dbg("free cache pointer: %p\n", curr);
            general_free(curr);
        }
        _cache_head[i] = NULL;
    }
    _msg_cache_count = 0;
    if (erase)
        ui_message_clear(p);
    ui_dbg("message cache has been clear\n");
}

bool _message_view_is_cache_full(const struct message_presenter* p) {
    return ui_message_max_limit(p) == _msg_cache_count;
}

size_t _message_view_cache_size(void) {
    ui_dbg("## _message_view_cache_size: %d\n", _msg_cache_count);
    return _msg_cache_count;
}
