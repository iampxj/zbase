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

#include "basework/dev/disk.h"
#include "basework/generic.h"
#include "basework/log.h"
#include "basework/malloc.h"
#include "basework/dev/buffer_io.h"

struct line_region {
    uint32_t beg;
    uint32_t end;
};

#define WRITE_BLOCK_SIZE  256
#define WRITE_BLOCK_MASK  (WRITE_BLOCK_SIZE - 1)

#define MTX_INIT(mtx)   os_mtx_init(mtx, 0)
#define MTX_LOCK(mtx)   os_mtx_lock(mtx)
#define MTX_UNLOCK(mtx) os_mtx_unlock(mtx)

static struct buffered_io *buffered_created;
static os_mutex_t list_mutex;

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

int buffered_flush_locked(struct buffered_io *bio) {
    if (bio->dirty) {
        if (bio->wrlimit) {
            size_t blknum = bio->size / WRITE_BLOCK_SIZE;
            size_t ofs = 0;
            int err;

            err = disk_device_erase(bio->dd, bio->offset, bio->size);
            if (err < 0)
                return err;
            
            for (size_t i = 0; i < blknum; i++) {
                if (is_wrblock_dirty(bio->buf, ofs, WRITE_BLOCK_SIZE)) {
                    err = disk_device_write(bio->dd, &bio->buf[ofs], WRITE_BLOCK_SIZE, 
                        bio->offset + ofs);
                    if (err < 0)
                        return err;
                }
                ofs += WRITE_BLOCK_SIZE;
            }
        } else {
            int err = write_eraseable_block(bio->dd, bio->buf, 
                bio->size, bio->offset);
            if (err < 0)
                return err;
        }

        bio->dirty = false;
    }
    return 0;
}

ssize_t buffered_read(struct buffered_io *bio, void *buf, size_t size, 
    uint32_t offset) {
    int err = disk_device_read(bio->dd, buf, size, offset);
    if (err < 0) {
        pr_err("Read disk failed(offset: 0x%x size: %d)\n", offset, size);
        return err;
    }

    /*
     * If there is some data in the cache, We have to 
     * read it again from the cache
     */
    if (bio->dirty) {
        if (rte_likely(!bio->panic))
            MTX_LOCK(&bio->mtx);

        /* 
         * We'll have to read it again because other threads 
         * may have been modified 
         */
        if (bio->dirty) {
            struct line_region reader = {
                .beg = offset,
                .end = offset + size - 1
            };
            struct line_region cache = {
                .beg = bio->offset,
                .end = bio->offset + bio->size - 1
            };
            struct line_region out;

            if (region_intersect(&reader, &cache, &out)) {
                memcpy(
                    (char *)buf + out.beg - offset, 
                    bio->buf + out.beg - cache.beg, 
                    out.end - out.beg + 1
                );
            }
        }
        if (rte_likely(!bio->panic))
            MTX_UNLOCK(&bio->mtx);
    }

    return size;
}

ssize_t buffered_write(struct buffered_io *bio, const void *buf, size_t size, 
    uint32_t offset) {
    const char *pbuf = (const char *)buf;
    size_t bytes = size;
    uint32_t aligned_ofs;
    uint32_t blkin_ofs;
    size_t cplen;
    size_t remain;
    int ret = 0;

    while (size > 0) {
        blkin_ofs = offset & bio->mask;

        /*
         * Write to whole eraseable block
         */
        if (rte_unlikely(blkin_ofs == 0 && size >= bio->size)) {
            ret = write_eraseable_block(bio->dd, pbuf, bio->size, 
                offset);
            if (ret < 0)
                goto _exit;
            
            offset += bio->size;
            pbuf += bio->size;
            size -= bio->size;
            continue;
        }

        /* Calculator block algined address */
        aligned_ofs = offset & ~bio->mask;

        if (rte_likely(!bio->panic))
            MTX_LOCK(&bio->mtx);

        /*
         * If the offset not in the cache range then
         * synchronize the cached data to disk 
         */
        if (bio->offset != aligned_ofs) {
            ret = buffered_flush_locked(bio);
            if (ret)
                goto _unlock;

            /* Read a block of data into a buffer */
            ret = disk_device_read(bio->dd, bio->buf, bio->size,
                aligned_ofs);
            if (ret < 0)
                goto _unlock;

            bio->offset = aligned_ofs;
        }

        /* Copy data to write buffer */
        remain = bio->size - blkin_ofs;
        cplen  = rte_min(size, remain);
        memcpy(bio->buf + blkin_ofs, pbuf, cplen);
        bio->dirty = true;

        if (rte_likely(!bio->panic))
            MTX_UNLOCK(&bio->mtx);

        /* Update loop condition */
        pbuf += cplen;
        size -= cplen;
        offset += cplen;
    }

    return bytes;
_unlock:
    MTX_UNLOCK(&bio->mtx);
_exit:
    return ret;
}

static int buffered_ioflush_locked(struct buffered_io *bio, bool invalid) {
    int err = 0;
    if (bio->dirty) {
        err = buffered_flush_locked(bio);
        if (!err && invalid)
            bio->offset = UINT32_MAX;
    }
    return err;
}

int buffered_ioflush(struct buffered_io *bio, bool invalid) {
    int err = 0;
    if (bio->dirty) {
        MTX_LOCK(&bio->mtx);
        err = buffered_ioflush_locked(bio, invalid);
        MTX_UNLOCK(&bio->mtx);
    }
    return err;
}

int buffered_ioflush_all(bool invalid) {
    int err = 0;

    MTX_LOCK(&list_mutex);
    for (struct buffered_io *iter = buffered_created; 
        iter != NULL; 
        iter = iter->next) {
        err = buffered_ioflush_locked(iter, invalid);
        if (err)
            break;
    }
    MTX_UNLOCK(&list_mutex);
    return err;
}

void buffered_iopanic(void) {
    for (struct buffered_io *iter = buffered_created; 
        iter != NULL; 
        iter = iter->next)
        iter->panic = true;
}

int buffered_ioinit(struct disk_device *dd, struct buffered_io *bio, 
    void *blkbuf, size_t blksize, bool wrlimit) {
    size_t erase_blksz;
    size_t newsize;
    int err;

    if (dd == NULL || bio == NULL)
        return -EINVAL;

    if (blksize & (blksize - 1)) 
        return -EINVAL;

    MTX_INIT(&bio->mtx);
    MTX_LOCK(&bio->mtx);

    erase_blksz = disk_device_get_block_size(dd);
    newsize = (blksize / erase_blksz) * erase_blksz;
    if (newsize == 0) {
        pr_err("Buffer size(%d) is too small\n", blksize);
        err = -EINVAL;
        goto _unlock;
    }

    bio->dd      = dd;
    bio->dirty   = false;
    bio->panic   = false;
    bio->wrlimit = wrlimit;
    bio->offset  = UINT32_MAX;
    bio->size    = newsize;
    bio->mask    = newsize - 1;
    bio->buf     = blkbuf;
    err          = 0;

    /* Add new node to created list */
    MTX_LOCK(&list_mutex);
    bio->next = buffered_created;
    buffered_created = bio;
    MTX_UNLOCK(&list_mutex);

_unlock:
    MTX_UNLOCK(&bio->mtx);
    return err;
}

int buffered_iocreate(struct disk_device *dd, size_t blksize, bool wrlimit,
    struct buffered_io **pbio) {
    struct buffered_io *bio;
    int err;

    if (blksize & (blksize - 1)) 
        return -EINVAL;

    if (dd == NULL || pbio == NULL)
        return -EINVAL;

    bio = general_calloc(1, blksize + sizeof(*bio));
    if (bio == NULL)
        return -ENOMEM;

    bio->allocated = true;
    err = buffered_ioinit(dd, bio, bio + 1, blksize, wrlimit);
    if (err)
        goto _freem;

    *pbio = bio;
    return 0;

_freem:
    general_free(bio);
    return err;
}

int buffered_iodestroy(struct buffered_io *bio) {
    if (bio == NULL)
        return -EINVAL;

    MTX_LOCK(&bio->mtx);

    /* Remove node from created list */
    MTX_LOCK(&list_mutex);
    for (struct buffered_io **iter = &buffered_created; 
        *iter != NULL; 
        iter = &(*iter)->next) {
        if (*iter == bio) {
            *iter = (*iter)->next;
            break;
        }
    }
    MTX_UNLOCK(&list_mutex);

    /* If dynamically allocated, free the memory */
    if (bio->allocated) {
        bio->allocated = false;
        general_free(bio);
    }
    MTX_UNLOCK(&bio->mtx);

    return 0;
}
