/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif


// #define CONFIG_LOGLEVEL LOGLEVEL_DEBUG
#define pr_fmt(fmt) "<msg_storage>: " fmt
#include <errno.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>

#include "basework/os/osapi.h"
#include "basework/system.h"
#include "basework/generic.h"
#include "basework/log.h"
#include "basework/lib/msg_storage.h"
#include "basework/lib/crc.h"
#include "basework/container/kfifo.h"
#include "basework/bitops.h"

#define MESSAGE_CHECK_PERIOD   (60 * 1000)
#define MESSAGE_STATE_OFFSET   4096
#define MESSAGE_CONTENT_OFFSET 8192
#define NULL_NODE UINT16_MAX
#define INVALID_TYPE 0
#define MESSAGE_BITMAP_SIZE ((MAX_MESSAGES + 31) / 32)
#define MSG_FILENAME "msgfile.db"

struct msg_database {
#define MESSAGE_MAGIC 0x1eebcaa1
	uint32_t m_magic;
	uint32_t m_bitmap[MESSAGE_BITMAP_SIZE];
	struct msg_node m_node[MAX_MESSAGES];
    uint16_t m_first;  /* First message node index */ 
    uint16_t m_last;   /* Last message node index */ 
	uint32_t m_offset; /* The offset of content */
#ifdef CONFIG_MSG_STORAGE_CHECKSUM
    uint16_t m_crc;
#endif
};

struct msg_notifstate {
#define MESSAGE_STATE_MAGIC 0x1eebcaa0
    uint32_t   magic;
    uint8_t    state[MAX_MESSAGE_TYPES]; /* Notification state */
    bool       notif_enable;
    uint32_t   bitvect; /* User private data */
#ifdef CONFIG_MSG_STORAGE_CHECKSUM
    uint16_t   crc;
#endif
};

struct msg_context {
    struct msg_database db; /* message database */
    struct msg_notifstate state;
    const struct msg_fops *f_ops;
    struct observer_base obs;
    os_mutex_t lock;
    os_timer_t timer;
    void *fd;
    DECLARE_KFIFO(fifo, uint16_t, 32);
    message_notif_cb notify;
    void *user_data;
    bool dirty; /* mean to the message database has been modified */
    bool state_dirty;
    uint16_t new_idx;
};

#define MTX_LOCK()   (void)os_mtx_lock(&msg_context.lock)
#define MTX_UNLOCK() (void)os_mtx_unlock(&msg_context.lock)
#define MTX_INIT()   (void)os_mtx_init(&msg_context.lock, 0)


static struct msg_context msg_context;

static inline int fmsg_open(struct msg_context *ctx, const char *name) {
    assert(ctx != NULL);
    assert(ctx->f_ops->open != NULL);
    if (!name) {
        pr_err("Invalid file name or handle\n");
        return -EINVAL;
    }
    return ctx->f_ops->open(name, &ctx->fd);
}

static inline int fmsg_close(struct msg_context *ctx) {
    assert(ctx != NULL);
    if (ctx->f_ops->close)
        return ctx->f_ops->close(ctx->fd);
    return 0;
}

static inline ssize_t fmsg_write(struct msg_context *ctx, 
    const void *buf, size_t size, uint32_t offset) {
    assert(ctx != NULL);
    assert(ctx->f_ops->write != NULL);
    return ctx->f_ops->write(ctx->fd, buf, size, offset);
}

static inline ssize_t fmsg_read(struct msg_context *ctx, 
    void *buf, size_t size, uint32_t offset) {
    assert(ctx != NULL);
    assert(ctx->f_ops->read != NULL);
    return ctx->f_ops->read(ctx->fd, buf, size, offset);
}

static inline int fmsg_flush(struct msg_context *ctx) {
    assert(ctx != NULL);
    if (ctx->f_ops->flush)
        return ctx->f_ops->flush(ctx->fd);
    return 0;
}

static inline int fmsg_remove(struct msg_context *ctx, const char *name) {
    assert(ctx != NULL);
    if (ctx->f_ops->unlink)
        return ctx->f_ops->unlink(name);
    return -ENOSYS;
}

static inline uint32_t fmsg_offset(struct msg_database *mdb, int index) {
    return mdb->m_offset + index * sizeof(struct msg_payload);
}

static inline void msg_node_init(struct msg_node *node) {
    node->index = NULL_NODE;
    node->n_index = NULL_NODE;
    node->p_index = NULL_NODE;
    node->type = INVALID_TYPE;
}

static inline void msg_notify(struct msg_context *ctx, int idx, 
    enum notification_state state) {
    if (state == MSG_NOTIF_NORMAL) {
        pr_dbg("%s: message notification is normal state\n", __func__);
        if (ctx->notify)
            ctx->notify(idx, ctx->user_data);
    }
}

#ifdef CONFIG_MSG_STORAGE_CHECKSUM
static inline uint16_t msg_calculate_crc(const struct msg_database *db) {
    return lib_crc16((uint8_t *)db, offsetof(struct msg_database, m_crc));
}

static inline uint16_t msg_state_calculate_crc(const struct msg_notifstate *sta) {
    return lib_crc16((uint8_t *)sta, offsetof(struct msg_notifstate, crc));
}
#endif /* CONFIG_MSG_STORAGE_CHECKSUM */

static void msg_mark_dirty(struct msg_context *ctx) {
    os_timer_mod(ctx->timer, MESSAGE_CHECK_PERIOD);
    ctx->dirty = true;
}

static int msg_header_write(struct msg_context *ctx) {
#ifdef CONFIG_MSG_STORAGE_CHECKSUM
    ctx->db.m_crc = msg_calculate_crc(&ctx->db);
#endif 
    int ret = fmsg_write(ctx, &ctx->db, sizeof(ctx->db), 0);
    if (ret < 0) {
        pr_err("write message header failed(%d)\n", ret);
        return ret;
    }
    return 0;
}

static int msg_state_write(struct msg_context *ctx) {
#ifdef CONFIG_MSG_STORAGE_CHECKSUM
    ctx->state.crc = msg_state_calculate_crc(&ctx->state);
#endif
    int ret = fmsg_write(ctx, &ctx->state, sizeof(ctx->state), 
        MESSAGE_STATE_OFFSET);
    if (ret < 0) {
        pr_err("write message state failed(%d)\n", ret);
        return ret;
    }
    return 0;
}

static inline int msg_flush_header_locked(struct msg_context *ctx) {
    int err = 0;
    if (ctx->dirty) {
        int err;
        ctx->dirty = false;
        err = msg_header_write(ctx);
        if (err) {
            pr_err("flush message cache failed\n");
            goto _out;
        }
    }
    if (ctx->state_dirty) {
        err |= msg_state_write(ctx);
        ctx->state_dirty = false;
    }
_out:
    return err;
}

static void msg_reinit_database(struct msg_database *mdb) {
    memset(mdb->m_bitmap, 0, sizeof(mdb->m_bitmap));
    for (int i = 0; i < MAX_MESSAGES; i++) 
        msg_node_init(&mdb->m_node[i]);
    mdb->m_magic = MESSAGE_MAGIC;
    mdb->m_first = NULL_NODE;
    mdb->m_last = NULL_NODE;
    mdb->m_offset = MESSAGE_CONTENT_OFFSET;
}

static void msg_reset_locked(struct msg_database *mdb, bool force) {
    if (!force && mdb->m_magic == MESSAGE_MAGIC) {
#ifdef CONFIG_MSG_STORAGE_CHECKSUM
        if (mdb->m_crc != msg_calculate_crc(mdb))
            msg_reinit_database(mdb);
#else /* !CONFIG_MSG_STORAGE_CHECKSUM */
        int nr = 0;
        MSG_STORAGE_FOREACH(idx) {
            if (++nr > MAX_MESSAGES) {
                msg_reinit_database(mdb);
                break;
            }
        }
#endif /* CONFIG_MSG_STORAGE_CHECKSUM */
    } else {
        msg_reinit_database(mdb);
    }
}

static void msg_state_reinit(struct msg_notifstate *state) {
    memset(state, 0, sizeof(*state));
    state->magic = MESSAGE_STATE_MAGIC;
    state->notif_enable = true;
}

static void msg_state_reset_locked(struct msg_notifstate *state, bool force) {
    if (!force && state->magic == MESSAGE_STATE_MAGIC) {
#ifdef CONFIG_MSG_STORAGE_CHECKSUM
        if (state->crc != msg_state_calculate_crc(state))
            msg_state_reinit(state);
#endif
    } else {
        msg_state_reinit(state);
    }
}

static void msg_remove_node(struct msg_database *mdb, int idx) {
    struct msg_node *node = &mdb->m_node[idx];

    /* Remove from message list */
    if (node->n_index != NULL_NODE) 
        mdb->m_node[node->n_index].p_index = node->p_index;
    if (node->p_index != NULL_NODE) 
        mdb->m_node[node->p_index].n_index = node->n_index;
    if (mdb->m_first == idx)
        mdb->m_first = mdb->m_node[idx].n_index;
    if (mdb->m_last == idx)
        mdb->m_last = mdb->m_node[idx].p_index;
    
    /* Make node invalid */
    msg_node_init(&mdb->m_node[idx]);
}

static int msg_slot_allocate(struct msg_database *mdb) {
    int index = 0;
    for (int i = 0; i < (int)rte_array_size(mdb->m_bitmap); i++) {
        uint32_t mask = mdb->m_bitmap[i];
        if (mdb->m_bitmap[i] != UINT32_MAX) {
            uint32_t ofs = ffs(~mask) - 1;
            index += ofs;
			if (index >= MAX_MESSAGES)
				break;
            mdb->m_bitmap[i] |= BIT(ofs);
			pr_dbg("allocated idx %d\n", index);
            return index;
        }
        index += 32;
    }

    /* If the message slot is full then remove the first one */
	index = mdb->m_first;
    msg_remove_node(mdb, mdb->m_first);
    pr_dbg("replace idx %d\n", index);
    return index;
}

static int msg_slot_free(struct msg_context *ctx, int index) {
    struct msg_database *mdb;
    uint16_t idx;

    if (index >= MAX_MESSAGES) {
        pr_err("Invalid slot index\n");
        return -EINVAL;
    }
    idx = (uint16_t)index;
    mdb = &ctx->db;

    /* Remove from bitmap */
    mdb->m_bitmap[idx >> 5] &= ~BIT((idx & 31));

    msg_remove_node(mdb, idx);
    msg_mark_dirty(ctx);
    return 0;
}

static int msg_content_write(struct msg_context *ctx, 
    const struct msg_payload *content, int *idx) {
    struct msg_database *mdb = &ctx->db;
    struct msg_node *node;
    int index, ret;

    /* Allocate free message slot */
    index = msg_slot_allocate(mdb);
    assert(index >= 0 && index < MAX_MESSAGES);
    node = &mdb->m_node[index];
    node->type = content->type;
    node->index = (uint16_t)index;

    /* Append message list */
    if (mdb->m_last != NULL_NODE) {
        struct msg_node *pnode = &mdb->m_node[mdb->m_last];
        pnode->n_index = node->index;
        node->p_index = pnode->index;
    } else {
        mdb->m_first = node->index;
        node->p_index = NULL_NODE;
    }
    mdb->m_last = node->index;
    node->n_index = NULL_NODE;

    /* Write message to disk */
    uint32_t offset = fmsg_offset(mdb, node->index);
    ret = fmsg_write(ctx, content, sizeof(*content), offset);
    if (ret < 0) {
        pr_err("write message content failed(%d)\n", ret);
        goto _failed;
    }
    msg_mark_dirty(ctx);
    kfifo_put(&ctx->fifo, (uint16_t)index);
    *idx = index;
    ctx->new_idx = index;
    pr_dbg("write message(idx: %d) completed\n", index);
    return 0;
_failed:
    msg_slot_free(ctx, node->index);
    return ret;
}

static void msg_check_cb(os_timer_t timer, void *arg) {
    struct msg_context *ctx = arg;
    MTX_LOCK();
    msg_flush_header_locked(ctx);
    MTX_UNLOCK();
    os_timer_mod(timer, MESSAGE_CHECK_PERIOD);
}

static int msg_storage_listen(struct observer_base *nb,
			unsigned long action, void *data) {
    struct shutdown_param *p = data;
    (void) nb;
    (void) action;
    if (p->reason != SHUTDOWN_FACTORY_RESET)
        msg_storage_deinit();
    return 0;
}

static int msg_flush_locked(void) {
    int err = msg_flush_header_locked(&msg_context);
    if (!err)
        err = fmsg_flush(&msg_context);
    return err;
}

size_t msg_storage_numbers(void) {
    size_t n = 0;
    MTX_LOCK();
    MSG_STORAGE_FOREACH(idx) {
        n++;
    }
    MTX_UNLOCK();
    pr_dbg("message numbers: %d\n", n);
    return n;
}

int msg_storage_read(int id, struct msg_payload *payload) {
    struct msg_context *ctx;
    uint32_t offset;
    uint16_t idx = (uint16_t)id;
    int err;

    if (payload == NULL) {
        pr_err("invalid paramters\n");
        return -EINVAL;
    }
    if (idx > MAX_MESSAGES || idx == NULL_NODE) {
        pr_err("invalid message id(%d)\n", id);
        return -EINVAL;
    }

    MTX_LOCK();
    ctx = &msg_context;
    if (ctx->db.m_node[idx].type == INVALID_TYPE) {
        MTX_UNLOCK();
        pr_err("invalid message type\n");
        return -EINVAL;
    }
    offset = fmsg_offset(&ctx->db, idx);
    MTX_UNLOCK();
    err = fmsg_read(ctx, payload, sizeof(*payload), offset);
    if (err < 0) {
        pr_err("read message failed (index:%d  offset:%d)\n", idx, offset);
        return err;
    }
    return 0;
}

int msg_storage_first(void) {
    uint16_t idx = msg_context.db.m_first;
    if (idx == NULL_NODE)
        return -EINVAL;
    return idx;
}

int msg_storage_next(int id) {
    if (id < 0) {
        pr_warn("invalid message id(%d)\n", id);
        return -EINVAL;
    }
    if (id < MAX_MESSAGES) {
        int idx = msg_context.db.m_node[id].n_index;
        if (idx == NULL_NODE)
            return -EINVAL;
        return idx;
    }
    return -EINVAL;
}

int msg_storage_remove(int id) {
    assert(id >= 0);
    MTX_LOCK();
    int err = msg_slot_free(&msg_context, id);
    MTX_UNLOCK();
    return err;
}

int msg_storage_write(const struct msg_payload *content) {
    struct msg_context *ctx;
    int err, idx;

    if (!content) {
        pr_err("invalid message pointer\n");
        return -EINVAL;
    }
    if (content->type >= MAX_MESSAGE_TYPES) {
        pr_err("%s: invalid message type(%d)\n", content->type);
        return -EINVAL;
    }
    ctx = &msg_context;
    MTX_LOCK();
    int type = content->type;
    enum notification_state state = ctx->state.state[type];
    if (ctx->state.notif_enable && 
        state != MSG_NOTIF_DISABLE) {
        err = msg_content_write(ctx, content, &idx);
        if (!err) {
            pr_dbg("%s: message notification(type: %d)\n", __func__, type);
            msg_notify(ctx, idx, state);
        }
    } else {
        err = 0;
    }
    MTX_UNLOCK();
    return err;
}

int __rte_notrace msg_storage_register(const struct msg_fops *fops) {
    if (fops != NULL) {
        msg_context.f_ops = fops;
        return 0;
    }
    pr_err("invalid message file operation\n");
    return -EINVAL;
}

int msg_storage_flush(void) {
    MTX_LOCK();
    int err = msg_flush_locked();
    MTX_UNLOCK();
    return err;
}

int msg_storage_get_newidx(void) {
    struct msg_context *ctx = &msg_context;
    uint16_t val;

    if (!kfifo_get(&ctx->fifo, &val))
        return -ENODATA;
    return val;
}

int msg_storage_get_node_newidx(void) {
    struct msg_context *ctx = &msg_context;

    return ctx->new_idx;
}

int msg_storage_clean(void) {
    struct msg_context *ctx = &msg_context;
    int err = 0;
    MTX_LOCK();
    fmsg_close(ctx);
    if (fmsg_remove(ctx, MSG_FILENAME)) {
        pr_dbg("clear msgfile.db failed\n");
        msg_context.db.m_magic = 0;
        msg_context.dirty = true;
        err = msg_flush_locked();
    } else {
        err = fmsg_open(ctx, MSG_FILENAME);
        assert(err == 0);
        msg_reset_locked(&ctx->db, true);
        pr_dbg("clear msgfile.db success\n");
    }
    MTX_UNLOCK();
    return err;
}

int msg_storage_set_bitvec(uint32_t setmsk, uint32_t clrmsk) {
    struct msg_context *ctx = &msg_context;
    uint32_t newval;

    MTX_LOCK();
    newval = (ctx->state.bitvect & ~clrmsk) | setmsk;
    if (ctx->state.bitvect != newval) {
        ctx->state.bitvect = newval;
        ctx->state_dirty = true;
    }
    MTX_UNLOCK();
    return 0;
}

uint32_t msg_storage_get_bitvect(void) {
    struct msg_context *ctx = &msg_context;

    MTX_LOCK();
    uint32_t val = ctx->state.bitvect;
    MTX_UNLOCK();
    return val;
}

int msg_storage_set_notif_switch(bool enable) {
    struct msg_context *ctx = &msg_context;

    MTX_LOCK();
    if (ctx->state.notif_enable != enable) {
        ctx->state.notif_enable = enable;
        ctx->state_dirty = true;
    }
    MTX_UNLOCK();
    return 0;
}

bool msg_storage_get_notif_switch(void) {
    struct msg_context *ctx = &msg_context;
    bool on;

    MTX_LOCK();
    on = ctx->state.notif_enable;
    MTX_UNLOCK();
    return on;  
}

int msg_storage_set_notification(int type, enum notification_state state) {
    struct msg_context *ctx = &msg_context;
    int err = -EINVAL;

    if (type >= MAX_MESSAGE_TYPES) {
        pr_err("invalid message type(%d)\n", type);
        return -EINVAL;
    }

    MTX_LOCK();
    if (!ctx->state.notif_enable) {
        pr_warn("message notification disabled\n");
        err = -EACCES;
        goto _unlock;
    }
    ctx->state.state[type] = state;
    ctx->state_dirty = true;
_unlock:
    MTX_UNLOCK();
    return err;
}

int msg_storage_get_notification(int type, enum notification_state *sta) {
    struct msg_context *ctx = &msg_context;
    if (!sta|| type >= MAX_MESSAGE_TYPES)
        return -EINVAL;
    MTX_LOCK();
    if (ctx->state.notif_enable)
        *sta = ctx->state.state[type];
    else
        *sta = MSG_NOTIF_DISABLE;
    MTX_UNLOCK();
    return 0;
}

int __rte_notrace msg_storage_register_notifier(message_notif_cb cb, void *arg) {
    struct msg_context *ctx = &msg_context;
    MTX_LOCK();
    ctx->notify = cb;
    ctx->user_data = arg;
    MTX_UNLOCK();
    return 0;
}

int __rte_notrace msg_storage_init(void) {
    struct msg_context *ctx = &msg_context;
    struct msg_database *mdb = &ctx->db;
    struct msg_notifstate *sta = &ctx->state;
    int err;

    err = fmsg_open(ctx, MSG_FILENAME);
    if (err) {
        pr_err("open message file failed(%d)\n", err);
        return err;
    }
    err = fmsg_read(ctx, mdb, sizeof(*mdb), 0);
    if (err < 0) {
        pr_err("read message header failed(%d)\n", err);
        return err;
    }
    err = fmsg_read(ctx, sta, sizeof(*sta), MESSAGE_STATE_OFFSET);
    if (err < 0) {
        pr_err("read message state failed(%d)\n", err);
        return err;
    }

    MTX_INIT();
    MTX_LOCK();
    msg_reset_locked(mdb, false);
    msg_state_reset_locked(sta, false);
    err = os_timer_create(&ctx->timer, msg_check_cb, 
        ctx, false);
    if (err)
        pr_err("create timer failed(%d)\n", err);

    INIT_KFIFO(ctx->fifo);
    /* Register observer for system shutdown/reoot */
    ctx->obs.update = msg_storage_listen;
    ctx->obs.priority = 10;
    err = system_add_observer(&ctx->obs);
    MTX_UNLOCK();
    return err;
}

int __rte_notrace msg_storage_deinit(void) {
    struct msg_context *ctx = &msg_context;
    MTX_LOCK();
    msg_flush_locked();
    if (ctx->timer) {
        os_timer_del(ctx->timer);
        ctx->timer = NULL;
    }
    if (ctx->fd) {
        fmsg_close(ctx);
        ctx->fd = NULL;
    }
    MTX_UNLOCK();

    return 0;
}
