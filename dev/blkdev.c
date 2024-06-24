/*
 * Copyright 2022 wtcat
 *
 * The simple block device buffer implement
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "blkdev: "fmt
#include <assert.h>
#include <string.h>

#include "basework/container/list.h"
#include "basework/dev/disk.h"
#include "basework/dev/partition.h"
#include "basework/log.h"
#include "basework/dev/blkdev.h"
#include "basework/malloc.h"
#include "basework/os/osapi_timer.h"
#include "basework/os/osapi.h"


#ifndef CONFIG_BLKDEV_NR_BUFS
#define CONFIG_BLKDEV_NR_BUFS 4
#endif
#ifndef CONFIG_BLKDEV_MAX_BLKSZ
#define CONFIG_BLKDEV_MAX_BLKSZ 4096
#endif
#ifndef CONFIG_BLKDEV_SWAP_PERIOD
#define CONFIG_BLKDEV_SWAP_PERIOD 2000 //Unit: ms
#endif
#ifndef CONFIG_BLKDEV_HOLD_TIME
#define CONFIG_BLKDEV_HOLD_TIME 10000 //Unit: ms
#endif

#define NR_BLKS CONFIG_BLKDEV_NR_BUFS

#ifndef _WIN32
_Static_assert(CONFIG_BLKDEV_NR_BUFS >= 1 && CONFIG_BLKDEV_NR_BUFS < 20, "");
#endif
enum bh_state {
    BH_STATE_INVALD = 0,
    BH_STATE_DIRTY,
    BH_STATE_CACHED
};

struct bh_desc {
    struct rte_list link;
    struct disk_device *dd;
    char *buffer;
    size_t blkno; /* block number */
    enum bh_state state;
    long hold;
};

struct bh_statitics {
    long cache_hits;
    long cache_missed;
};

#define MTX_LOCK()   os_mtx_lock(&bh_mtx)
#define MTX_UNLOCK() os_mtx_unlock(&bh_mtx)
#define MTX_LOCK_INIT() os_mtx_init(&bh_mtx, 0)


static os_mutex_t bh_mtx;
static os_timer_t bh_timer;
static void *bh_buffer;
static bool bh_initialized;
static struct bh_statitics bh_statistics;
static RTE_LIST(cached_list);
static RTE_LIST(dirty_list);

static inline void bh_enqueue_dirty(struct bh_desc *bh) {
    bh->hold = CONFIG_BLKDEV_HOLD_TIME;
    rte_list_add_tail(&bh->link, &dirty_list);
}

static inline void bh_enqueue_cached(struct bh_desc *bh) {
    rte_list_add_tail(&bh->link, &cached_list);
}

static int blkdev_sync_locked(long expired, int max_blks, bool invalid) {
    struct rte_list *pos, *next;
    struct disk_device *dd;
    struct bh_desc *bh;
    uint32_t ofs;
    int err = 0;

    rte_list_foreach_safe(pos, next, &dirty_list) {
        bh = rte_container_of(pos, struct bh_desc, link);
        if (bh->hold > expired) {
            bh->hold -= expired;
            continue;
        }
        
        max_blks--;
        if (max_blks < 0)
            break;

        dd = bh->dd;
        ofs = bh->dd->blk_size * bh->blkno;
		pr_dbg("blkdev_sync_locked(%ld) blkno(%d) offset(0x%x) size(%d)\n", expired, 
			bh->blkno, ofs, dd->blk_size);
        err = disk_device_erase(bh->dd, ofs, dd->blk_size);
        if (err) {
			pr_err("erase disk failed(offset: 0x%x size:0x%x)\n", ofs, dd->blk_size);
            break;
        }
        err = disk_device_write(bh->dd, bh->buffer, bh->dd->blk_size, ofs);
        if (err < 0) {
			pr_err("write disk failed(offset: 0x%x size:0x%x)\n", ofs, dd->blk_size);
            break;
        }

        rte_list_del(&bh->link);
        if (rte_unlikely(invalid)) {
            bh->dd = NULL;
            bh->state = BH_STATE_INVALD;
        } else {
            bh->state = BH_STATE_CACHED;
        }
        bh_enqueue_cached(bh);
    }

    return 0;
}

static void bh_check_cb(os_timer_t timer, void *arg) {
    (void) arg;
    MTX_LOCK();
    int err = blkdev_sync_locked(CONFIG_BLKDEV_SWAP_PERIOD, 3, false);
    MTX_UNLOCK();
    if (err) {
        pr_warn("flush data failed(%d)\n", err);
    }
    os_timer_mod(timer, CONFIG_BLKDEV_SWAP_PERIOD);
}

static int bh_release_modified(struct bh_desc *bh) {
    bh->state = BH_STATE_DIRTY;
    bh_enqueue_dirty(bh);
    MTX_UNLOCK();
    return 0;
}

static int bh_release(struct bh_desc *bh) {
    if (bh->state == BH_STATE_DIRTY)
        bh_enqueue_dirty(bh);
    else
        bh_enqueue_cached(bh);
    MTX_UNLOCK();
    return 0;
}

/*
 * The cache block search use list is simplest but not fit lots of blocks
 * If so should use balance binrary tree
 */
static struct bh_desc *bh_search_locked(struct disk_device *dd, uint32_t blkno) {
    struct rte_list *pos, *next;
    struct bh_desc *bh;

    rte_list_foreach_safe(pos, next, &cached_list) {
        bh = rte_container_of(pos, struct bh_desc, link);
        if (bh->dd == dd && bh->blkno == blkno) {
            bh_statistics.cache_hits++;
            rte_list_del(&bh->link);
            return bh;
        }
    }
    rte_list_foreach_safe(pos, next, &dirty_list) {
        bh = rte_container_of(pos, struct bh_desc, link);
        if (bh->dd == dd && bh->blkno == blkno) {
            bh_statistics.cache_hits++;
            rte_list_del(&bh->link);
            return bh;
        }
    }
    if (rte_list_empty(&cached_list)) 
        blkdev_sync_locked(CONFIG_BLKDEV_HOLD_TIME, 1, false);
    
    bh_statistics.cache_missed++;
    bh = rte_container_of(cached_list.next, struct bh_desc, link);
    rte_list_del(&bh->link);
    bh->blkno = blkno;
    bh->dd = dd;
    bh->state = BH_STATE_INVALD;
    return bh;
}

static int bh_get(struct disk_device *dd, uint32_t blkno, struct bh_desc **pbh) {
    struct bh_desc *bh;

    MTX_LOCK();
    bh = bh_search_locked(dd, blkno);
    assert(bh != NULL);
    *pbh = bh;
    return 0;
}

static int bh_read(struct disk_device *dd, uint32_t blkno, struct bh_desc **pbh) {
    struct bh_desc *bh;
    uint32_t ofs;
    int err;

    MTX_LOCK();
    bh = bh_search_locked(dd, blkno);
    if (bh->state != BH_STATE_INVALD) {
        *pbh = bh;
        return 0;
    }

    ofs = dd->blk_size * bh->blkno;
    err = disk_device_read(dd, bh->buffer, dd->blk_size, ofs);
    if (err < 0) {
        bh_release(bh);
        pr_err("Read disk failed(offset: 0x%x size: %d)\n", ofs, dd->blk_size);
        return err;
    }

    bh->state = BH_STATE_CACHED;
    *pbh = bh;
    return 0;
}

int blkdev_sync(void) {
    MTX_LOCK();
    int err = blkdev_sync_locked(CONFIG_BLKDEV_HOLD_TIME, 
        INT32_MAX, false);
    MTX_UNLOCK();
    return err;
}

int blkdev_sync_invalid(void) {
    MTX_LOCK();
    int err = blkdev_sync_locked(CONFIG_BLKDEV_HOLD_TIME, 
        INT32_MAX, true);
    MTX_UNLOCK();
    return err;
}

ssize_t blkdev_write(struct disk_device *dd, const void *buf, size_t size, 
    uint32_t offset) {
    const char *src = (const char *)buf;
    struct bh_desc *bh;
    size_t remain = size;
    size_t bytes;
    uint32_t blkno;
    uint32_t blkofs;
    int err;

    blkno = offset / dd->blk_size;
    blkofs = offset % dd->blk_size;

    while (remain > 0) {
        if (blkofs == 0 && remain >= dd->blk_size)
            err = bh_get(dd, blkno, &bh);
        else
            err = bh_read(dd, blkno, &bh);
        if (err)
            return err;

        bytes = dd->blk_size - blkofs;
        if (bytes > remain)
            bytes = remain;

        memcpy(bh->buffer + blkofs, src, bytes);
        bh_release_modified(bh);
        remain -= bytes;
        src += bytes;
        blkofs = 0;
        blkno++;
    }

    return size - remain;
}

ssize_t blkdev_read(struct disk_device *dd, void *buf, size_t size, 
    uint32_t offset) {
    char *dst = (char *)buf;
    struct bh_desc *bh;
    size_t remain = size;
    size_t bytes;
    uint32_t blkno;
    uint32_t blkofs;
    int err;

    blkno = offset / dd->blk_size;
    blkofs = offset % dd->blk_size;

    while (remain > 0) {
        err = bh_read(dd, blkno, &bh);
        if (err)
            return err;
        bytes = dd->blk_size - blkofs;
        if (bytes > remain)
            bytes = remain;

        memcpy(dst, bh->buffer + blkofs, bytes);
        bh_release(bh);
        dst += bytes;
        remain -= bytes;
        blkofs = 0;
        blkno++;
    }

    return size - remain;
}

int blkdev_init(void) {
    int err;
    if (!bh_initialized) {
        struct bh_desc *bh;
        size_t size;

        size = CONFIG_BLKDEV_MAX_BLKSZ + sizeof(struct bh_desc);
        bh = general_malloc(size * NR_BLKS);
        assert(bh != NULL);
        bh_buffer = bh;
        for (int i = 0; i < (int)NR_BLKS; i++) {
            bh->blkno = 0;
            bh->buffer = (char *)(bh + 1);
            bh->state = BH_STATE_INVALD;
            bh->dd = NULL;
            rte_list_add(&bh->link, &cached_list);
            bh = (void *)((char *)bh + size);
        }

        err = os_timer_create(&bh_timer, bh_check_cb, NULL, false);
        if (err) {
            general_free(bh);
            bh_buffer = NULL;
            pr_err("create timer failed(%d)\n", err);
            return err;
        }
        MTX_LOCK_INIT();
        bh_initialized = true;
        err = os_timer_add(bh_timer, CONFIG_BLKDEV_SWAP_PERIOD);
        assert(err == 0);
        (void) err;
    }
    return 0;
}

int blkdev_destroy(void) {
    if (!bh_initialized)
        return 0;
    MTX_LOCK();
    os_timer_destroy(bh_timer);
    RTE_INIT_LIST(&cached_list);
    RTE_INIT_LIST(&dirty_list);
    if (bh_buffer) {
        general_free(bh_buffer);
        bh_buffer = NULL;
    }
    bh_initialized = false;
    MTX_UNLOCK();
    return 0;
}

void blkdev_print(void) {
    struct bh_statitics *stat = &bh_statistics;
    pr_out("\n=========== Block Device Statistics ==========\n");
    pr_out("Cache Hits: %ld\nCache Missed: %ld\nCache Hit-Rate: %ld%%\n\n",
        stat->cache_hits,
        stat->cache_missed,
        (stat->cache_hits * 100) / (stat->cache_hits + stat->cache_missed)
    );
}