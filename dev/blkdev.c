/*
 * Copyright 2022 wtcat
 *
 * The simple block device buffer implement
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define CONFIG_ASSERT_DISABLE
#define pr_fmt(fmt) "<blkdev>: "fmt
#include <assert.h>
#include <string.h>

#include "basework/assert.h"
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
    char *buffer;
    size_t blkno; /* block number */
    enum bh_state state;
    long hold;
#ifndef CONFIG_ASSERT_DISABLE
    struct disk_device *dd;
#endif
};

struct bh_statitics {
    long cache_hits;
    long cache_missed;
};

struct block_device {
    os_mutex_t mtx;
    struct disk_device *dd;
    struct rte_list cached_list;
    struct rte_list dirty_list;
    struct rte_list link;
    struct bh_desc *bh;
    char *buffer;
    struct bh_statitics stat;
};

static os_timer_t bh_timer;
static RTE_LIST(created_list);
static os_mutex_t bdev_mutex;

static inline void 
bh_enqueue_dirty(struct block_device *bdev, struct bh_desc *bh) {
    bh->hold = CONFIG_BLKDEV_HOLD_TIME;
    rte_list_add_tail(&bh->link, &bdev->dirty_list);
}

static inline void 
bh_enqueue_cached(struct block_device *bdev, struct bh_desc *bh) {
    rte_list_add_tail(&bh->link, &bdev->cached_list);
}

static int 
blkdev_sync_locked(struct block_device *bdev, long expired, 
    int max_blks, bool invalid) {
    struct rte_list *pos, *next;
    struct disk_device *dd;
    struct bh_desc *bh;
    uint32_t ofs;
    int err = 0;

    rte_list_foreach_safe(pos, next, &bdev->dirty_list) {
        if (--max_blks < 0)
            break;

        bh = rte_container_of(pos, struct bh_desc, link);
        if (bh->hold > expired) {
            bh->hold -= expired;
            continue;
        }
        
        dd = bdev->dd;
        rte_assert(bdev->dd == bh->dd);

        ofs = dd->blk_size * bh->blkno;
		pr_dbg("blkdev_sync_locked(%ld) blkno(%d) offset(0x%x) size(%d)\n", expired, 
			bh->blkno, ofs, dd->blk_size);
        err = disk_device_erase(dd, ofs, dd->blk_size);
        if (err) {
			pr_err("erase disk failed(offset: 0x%x size:0x%x)\n", ofs, dd->blk_size);
            break;
        }
        err = disk_device_write(dd, bh->buffer, dd->blk_size, ofs);
        if (err < 0) {
			pr_err("write disk failed(offset: 0x%x size:0x%x)\n", ofs, dd->blk_size);
            break;
        }

        rte_list_del(&bh->link);
        if (rte_unlikely(invalid))
            bh->state = BH_STATE_INVALD;
        else
            bh->state = BH_STATE_CACHED;
        bh_enqueue_cached(bdev, bh);
    }

    return 0;
}

static int 
bdev_sync(long expired, int max_blks, bool invalid) {
    struct rte_list *pos;
    int err = 0;

    guard(mutex)(&bdev_mutex);
    rte_list_foreach(pos, &created_list) {
        struct block_device *bdev = rte_container_of(pos, 
            struct block_device, link);
        os_mtx_lock(&bdev->mtx);
        err |= blkdev_sync_locked(bdev, expired, max_blks, invalid);
        err |= disk_device_ioctl(bdev->dd, DISK_SYNC, NULL);
        os_mtx_unlock(&bdev->mtx);
    }
    return err;
}

static void 
bh_check_cb(os_timer_t timer, void *arg) {
    (void) arg;
    int err = bdev_sync(CONFIG_BLKDEV_SWAP_PERIOD, 3, false);
    if (err)
        pr_warn("flush data failed(%d)\n", err);
    os_timer_mod(timer, CONFIG_BLKDEV_SWAP_PERIOD);
}

static int 
bh_release_modified(struct block_device *bdev, struct bh_desc *bh) {
    bh->state = BH_STATE_DIRTY;
    bh_enqueue_dirty(bdev, bh);
    return 0;
}

static int 
bh_release(struct block_device *bdev, struct bh_desc *bh) {
    if (bh->state == BH_STATE_DIRTY)
        bh_enqueue_dirty(bdev, bh);
    else
        bh_enqueue_cached(bdev, bh);
    return 0;
}

/*
 * The cache block search use list is simplest but not fit lots of blocks
 * If so should use balance binrary tree
 */
static struct bh_desc *bh_search_locked(struct block_device *bdev, uint32_t blkno) {
    struct rte_list *pos, *next;
    struct bh_desc *bh;

    rte_list_foreach_safe(pos, next, &bdev->cached_list) {
        bh = rte_container_of(pos, struct bh_desc, link);
        if (bh->blkno == blkno) {
            rte_assert(bdev->dd == bh->dd);
            bdev->stat.cache_hits++;
            rte_list_del(&bh->link);
            return bh;
        }
    }
    rte_list_foreach_safe(pos, next, &bdev->dirty_list) {
        bh = rte_container_of(pos, struct bh_desc, link);
        if (bh->blkno == blkno) {
            rte_assert(bdev->dd == bh->dd);
            bdev->stat.cache_hits++;
            rte_list_del(&bh->link);
            return bh;
        }
    }
    if (rte_list_empty(&bdev->cached_list)) 
        blkdev_sync_locked(bdev, CONFIG_BLKDEV_HOLD_TIME, 1, false);
    
    bdev->stat.cache_missed++;
    bh = rte_container_of(bdev->cached_list.next, struct bh_desc, link);
    rte_list_del(&bh->link);
    bh->blkno = blkno;
    bh->state = BH_STATE_INVALD;
    return bh;
}

static int 
bh_get_locked(struct block_device *bdev, uint32_t blkno, struct bh_desc **pbh) {
    struct bh_desc *bh;

    bh = bh_search_locked(bdev, blkno);
    assert(bh != NULL);
    *pbh = bh;
    return 0;
}

static int 
bh_read_locked(struct block_device *bdev, uint32_t blkno, struct bh_desc **pbh) {
    struct bh_desc *bh;
    uint32_t ofs;
    int err;

    bh = bh_search_locked(bdev, blkno);
    if (bh->state != BH_STATE_INVALD) {
        *pbh = bh;
        return 0;
    }

    ofs = bdev->dd->blk_size * bh->blkno;
    err = disk_device_read(bdev->dd, bh->buffer, bdev->dd->blk_size, ofs);
    if (err < 0) {
        bh_release(bdev, bh);
        pr_err("Read disk failed(offset: 0x%x size: %d)\n", ofs, bdev->dd->blk_size);
        return err;
    }

    bh->state = BH_STATE_CACHED;
    *pbh = bh;
    return 0;
}

static int 
bdev_init(struct disk_device *dd, int nrblk) {
    struct block_device *bdev;
    size_t bdev_size;
    size_t size;

    if (dd == NULL || nrblk == 0)
        return -EINVAL;

    if (dd->bdev)
        return -EEXIST;

    size = dd->blk_size + sizeof(struct bh_desc);
    bdev_size = RTE_ALIGN(sizeof(*bdev), RTE_CACHE_LINE_SIZE);
    bdev = general_malloc(bdev_size + size * nrblk);
    assert(bdev != NULL);

    memset(bdev, 0, sizeof(*bdev));
    os_mtx_init(&bdev->mtx, 0);
    RTE_INIT_LIST(&bdev->cached_list);
    RTE_INIT_LIST(&bdev->dirty_list);
    bdev->bh = (struct bh_desc *)((char *)bdev + bdev_size);
    bdev->dd = dd;
    dd->bdev = bdev;
    
    for (int i = 0; i < nrblk; i++) {
        bdev->bh->blkno = (size_t)-1;
        bdev->bh->buffer = (char *)(bdev->bh + 1);
        bdev->bh->state = BH_STATE_INVALD;
#ifndef CONFIG_ASSERT_DISABLE
        bdev->bh->dd = dd;
#endif
        rte_list_add(&bdev->bh->link, &bdev->cached_list);
        bdev->bh = (void *)((char *)bdev->bh + size);
    }

    os_mtx_lock(&bdev_mutex);
    rte_list_add_tail(&bdev->link, &created_list);
    os_mtx_unlock(&bdev_mutex);
    return 0;
}

int 
simple_blkdev_sync(void) {
    return bdev_sync(CONFIG_BLKDEV_HOLD_TIME, INT32_MAX, false);
}

int 
simple_blkdev_sync_invalid(void) {
    return bdev_sync(CONFIG_BLKDEV_HOLD_TIME, INT32_MAX, true);
}

ssize_t 
simple_blkdev_write(struct disk_device *dd, const void *buf, size_t size, 
    uint32_t offset) {
    const char *src = (const char *)buf;
    struct block_device *bdev = dd->bdev;
    struct bh_desc *bh;
    size_t remain = size;
    size_t bytes;
    uint32_t blkno;
    uint32_t blkofs;
    int err;

    blkno = offset / dd->blk_size;
    blkofs = offset % dd->blk_size;

    guard(mutex)(&bdev->mtx);

    while (remain > 0) {
        if (blkofs == 0 && remain >= dd->blk_size)
            err = bh_get_locked(bdev, blkno, &bh);
        else
            err = bh_read_locked(bdev, blkno, &bh);
        if (err)
            return err;

        bytes = dd->blk_size - blkofs;
        if (bytes > remain)
            bytes = remain;

        memcpy(bh->buffer + blkofs, src, bytes);
        bh_release_modified(bdev, bh);
        remain -= bytes;
        src += bytes;
        blkofs = 0;
        blkno++;
    }

    return size - remain;
}

ssize_t 
simple_blkdev_read(struct disk_device *dd, void *buf, size_t size, 
    uint32_t offset) {
    struct block_device *bdev = dd->bdev;
    char *dst = (char *)buf;
    struct bh_desc *bh;
    size_t remain = size;
    size_t bytes;
    uint32_t blkno;
    uint32_t blkofs;
    int err;

    blkno = offset / dd->blk_size;
    blkofs = offset % dd->blk_size;

    guard(mutex)(&bdev->mtx);

    while (remain > 0) {
        err = bh_read_locked(bdev, blkno, &bh);
        if (err)
            return err;

        bytes = dd->blk_size - blkofs;
        if (bytes > remain)
            bytes = remain;

        memcpy(dst, bh->buffer + blkofs, bytes);
        bh_release(bdev, bh);
        dst += bytes;
        remain -= bytes;
        blkofs = 0;
        blkno++;
    }

    return size - remain;
}

int 
simple_blkdev_init(struct disk_device *dd) {
    if (!bh_timer) {
        int err = os_timer_create(&bh_timer, bh_check_cb, NULL, false);
        if (err) {
            pr_err("create timer failed(%d)\n", err);
            return err;
        }

        os_mtx_init(&bdev_mutex, 0);
        err = os_timer_add(bh_timer, CONFIG_BLKDEV_SWAP_PERIOD*10);
        assert(err == 0);
        (void) err;
    }

    return bdev_init(dd, CONFIG_BLKDEV_NR_BUFS);
}

void 
simple_blkdev_print(void) {
    struct rte_list *pos;

    guard(mutex)(&bdev_mutex);
    rte_list_foreach(pos, &created_list) {
        struct block_device *bdev = rte_container_of(pos, 
            struct block_device, link);
        struct bh_statitics *stat = &bdev->stat;

        pr_out("\n=========== %s Statistics ==========\n", 
            disk_device_get_name(bdev->dd));
        pr_out("Cache Hits: %ld\nCache Missed: %ld\nCache Hit-Rate: %ld%%\n\n",
            stat->cache_hits,
            stat->cache_missed,
            (stat->cache_hits * 100) / (stat->cache_hits + stat->cache_missed)
        );
    }
}
