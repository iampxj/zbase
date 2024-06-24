/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "basework/dev/blkdev.h"
#include "basework/dev/disk.h"
#include "basework/generic.h"
#include "basework/log.h"
#include "basework/os/osapi.h"

#ifdef CONFIG_BLKDEV_ERASE_BLKSZ
#define BLKDEV_ERASE_BLKSZ CONFIG_BLKDEV_ERASE_BLKSZ
#else
#define BLKDEV_ERASE_BLKSZ 4096
#endif

#if CONFIG_BLKDEV_WRSIZE > 0
#define WRITE_BLOCK_SIZE  CONFIG_BLKDEV_WRSIZE
#define WRITE_BLOCK_MASK  (WRITE_BLOCK_SIZE - 1)
#define BLOCKS_PER_BUFFER (BLKDEV_ERASE_BLKSZ / WRITE_BLOCK_SIZE)
#endif

#define BLKDEV_BLKSZ_MASK (BLKDEV_ERASE_BLKSZ - 1)

STATIC_ASSERT((BLKDEV_ERASE_BLKSZ & BLKDEV_BLKSZ_MASK) == 0, "");

struct blkdev_wrbuffer {
    struct disk_device *dd;
    os_mutex_t mtx;
    uint8_t    buf[BLKDEV_ERASE_BLKSZ];
    uint32_t   offset;
    bool       dirty;
};

struct line_region {
    uint32_t beg, end;
};

#define MTX_INIT()   os_mtx_init(&blk_buffer.mtx, 0)
#define MTX_LOCK()   os_mtx_lock(&blk_buffer.mtx)
#define MTX_UNLOCK() os_mtx_unlock(&blk_buffer.mtx)

static struct blkdev_wrbuffer blk_buffer;

#if CONFIG_BLKDEV_WRSIZE > 0
static bool is_wrblock_dirty(const uint8_t *buf, uint32_t offset, 
    size_t size) {
#define LOOP_INLINE_SIZE 16
    const uint32_t *p = (const uint32_t *)&buf[offset];
    size_t count = size >> 2;
    size_t n = (count + LOOP_INLINE_SIZE - 1) / LOOP_INLINE_SIZE;
    uint32_t val = UINT32_MAX;

    switch (count & (LOOP_INLINE_SIZE - 1)) {
    case 0: do {    val &= *p++;

#if LOOP_INLINE_SIZE > 8
    case 15:        val &= *p++;
    case 14:        val &= *p++;
    case 13:        val &= *p++;
    case 12:        val &= *p++;
    case 11:        val &= *p++;
    case 10:        val &= *p++;
    case 9:         val &= *p++;
    case 8:         val &= *p++;
#endif /* LOOP_INLINE_SIZE > 8 */

#if LOOP_INLINE_SIZE > 4
    case 7:         val &= *p++;
    case 6:         val &= *p++;
    case 5:         val &= *p++;
    case 4:         val &= *p++;
#endif /* LOOP_INLINE_SIZE > 4 */

#if LOOP_INLINE_SIZE > 2
    case 3:         val &= *p++;
    case 2:         val &= *p++;
#endif /* LOOP_INLINE_SIZE > 2 */

#if LOOP_INLINE_SIZE > 0
    case 1:         val &= *p++;
#endif /* LOOP_INLINE_SIZE > 0 */
                    if (val != UINT32_MAX)
                        return true;
            } while (--n > 0);
    }

    return false;
#undef LOOP_INLINE_SIZE
}
#endif /* CONFIG_BLKDEV_WRSIZE > 0 */

static bool region_intersect(const struct line_region *a, 
    const struct line_region *b, struct line_region *out) {
    out->beg = rte_max(a->beg, b->beg);
    out->end = rte_min(a->end, b->end);

    return out->beg <= out->end;
}

static int write_eraseable_block(struct disk_device *dd, const void *buf, 
    size_t blksize, uint32_t offset) {
    int err;
    err = disk_device_erase(dd, offset, blksize);
    if (err < 0)
        return err;
    
    err = disk_device_write(dd, buf, blksize, offset);
    if (err < 0)
        return err;

    return 0;    
}

static int flush_cache_block(struct disk_device *dd) {
    if (blk_buffer.dirty) {
#if CONFIG_BLKDEV_WRSIZE > 0
        struct blkdev_wrbuffer *bb = &blk_buffer;
        size_t ofs = 0;
        int err;

        err = disk_device_erase(dd, blk_buffer.offset, BLKDEV_ERASE_BLKSZ);
        if (err < 0)
            return err;
        
        for (size_t i = 0; i < BLOCKS_PER_BUFFER; i++) {
            if (is_wrblock_dirty(bb->buf, ofs, WRITE_BLOCK_SIZE)) {
                err = disk_device_write(dd, &bb->buf[ofs], WRITE_BLOCK_SIZE, 
                    bb->offset + ofs);
                if (err < 0)
                    return err;
            }

            ofs += WRITE_BLOCK_SIZE;
        }

#else /* CONFIG_BLKDEV_WRSIZE == 0 */
        int err = write_eraseable_block(dd, blk_buffer.buf, 
            BLKDEV_ERASE_BLKSZ, blk_buffer.offset);
        if (err < 0)
            return err;
#endif /* CONFIG_BLKDEV_WRSIZE > 0 */

        blk_buffer.dirty = false;
    }
    return 0;
}

static int __blkdev_sync(bool invalid) {
    int err = 0;
    if (blk_buffer.dirty) {
        MTX_LOCK();
        err = flush_cache_block(blk_buffer.dd);
	    if (invalid && err == 0)
			blk_buffer.offset = UINT32_MAX;
        MTX_UNLOCK();
    }
    return err;
}

ssize_t blkdev_read(struct disk_device *dd, void *buf, size_t size, 
    uint32_t offset) {
    int err = disk_device_read(dd, buf, size, offset);
    if (err < 0) {
        pr_err("Read disk failed(offset: 0x%x size: %d)\n", offset, size);
        return err;
    }

    /*
     * If there is some data in the cache, We have to 
     * read it again from the cache
     */
    if (blk_buffer.dirty) {
        MTX_LOCK();

        /* 
         * We'll have to read it again because other threads 
         * may have been modified 
         */
        if (blk_buffer.dirty) {
            struct line_region reader = {
                .beg = offset,
                .end = offset + size - 1
            };
            struct line_region cache = {
                .beg = blk_buffer.offset,
                .end = blk_buffer.offset + sizeof(blk_buffer.buf) - 1
            };
            struct line_region out;

            if (region_intersect(&reader, &cache, &out)) {
                memcpy(
                    (char *)buf + out.beg - offset, 
                    blk_buffer.buf + out.beg - cache.beg, 
                    out.end - out.beg + 1
                );
            }
        }
        MTX_UNLOCK();
    }

    return size;
}

ssize_t blkdev_write(struct disk_device *dd, const void *buf, size_t size, 
    uint32_t offset) {
    const char *pbuf = (const char *)buf;
    size_t bytes = size;
    uint32_t aligned_ofs;
    uint32_t blkin_ofs;
    size_t cplen;
    size_t remain;
    int ret = 0;

    while (size > 0) {
        blkin_ofs = offset & BLKDEV_BLKSZ_MASK;

        /*
         * Write to whole eraseable block
         */
        if (rte_unlikely(blkin_ofs == 0 && size >= BLKDEV_ERASE_BLKSZ)) {
            ret = write_eraseable_block(dd, pbuf, BLKDEV_ERASE_BLKSZ, 
                offset);
            if (ret < 0)
                goto _exit;
            
            offset += BLKDEV_ERASE_BLKSZ;
            pbuf += BLKDEV_ERASE_BLKSZ;
            size -= BLKDEV_ERASE_BLKSZ;
            continue;
        }

        /* Calculator block algined address */
        aligned_ofs = offset & ~BLKDEV_BLKSZ_MASK;

        MTX_LOCK();

        /*
         * If the offset not in the cache range then
         * synchronize the cached data to disk 
         */
        if (blk_buffer.offset != aligned_ofs) {
            ret = flush_cache_block(dd);
            if (ret)
                goto _unlock;

            /* Read a block of data into a buffer */
            ret = disk_device_read(dd, blk_buffer.buf, sizeof(blk_buffer.buf), 
                aligned_ofs);
            if (ret < 0)
                goto _unlock;

            blk_buffer.offset = aligned_ofs;
        }

        /* Copy data to write buffer */
        remain = BLKDEV_ERASE_BLKSZ - blkin_ofs;
        cplen  = rte_min(size, remain);
        memcpy(blk_buffer.buf + blkin_ofs, pbuf, cplen);
        blk_buffer.dirty = true;
        blk_buffer.dd = dd;
        MTX_UNLOCK();

        /* Update loop condition */
        pbuf += cplen;
        size -= cplen;
        offset += cplen;
    }

    return bytes;
_unlock:
    MTX_UNLOCK();
_exit:
    return ret;
}

int blkdev_sync(void) {
	return __blkdev_sync(false);
}

int blkdev_sync_invalid(void) {
	return __blkdev_sync(true);
}

int blkdev_init(void) {
    MTX_INIT();
    blk_buffer.dd     = NULL;
    blk_buffer.dirty  = false;
    blk_buffer.offset = UINT32_MAX;
    return 0;
}

void blkdev_print(void) {}
int  blkdev_destroy(void) {
    return 0;
}
