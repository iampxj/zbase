/*
 * Copyright 2022 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif


#include <stdbool.h>
#include <errno.h>

#include "basework/assert.h"
#include "basework/dev/disk.h"
#include "basework/dev/partition.h"
#include "basework/dev/buffer_io.h"
#include "basework/dev/blkdev.h"
#include "basework/lib/printer.h"
#include "basework/lib/disklog.h"
#include "basework/log.h"
#include "basework/minmax.h"
#include "basework/malloc.h"
#include "basework/system.h"

struct disk_log {
    uint32_t magic;
#define DISKLOG_MAGIC 0xabdcebcf
    uint32_t start;
    uint32_t end;
    uint32_t size;
    uint32_t wr_ofs;
    uint32_t rd_ofs;
    uint32_t d_size;
};

struct disklog_ctx {
    struct disk_log file;
    os_mutex_t mtx;
    os_timer_t timer;
#ifdef CONFIG_DISKLOG_BIO
    struct buffered_io *bio;
#else
    struct disk_device *dd;
#endif /* CONFIG_DISKLOG_BIO */

    uint32_t offset;
    uint32_t len;
    size_t blksz;
    struct observer_base obs;
    bool upload_pending;
    bool read_locked;
    bool log_dirty;
    bool should_stop;
};


#ifndef MIN
#define MIN(a, b) ((a) < (b)? (a): (b))
#endif

#ifndef CONFIG_DISLOG_SWAP_PERIOD
#define DISLOG_SWAP_PERIOD (12 * 3600 * 1000ul) //12 hour
#endif

#define MTX_LOCK_TIMED() os_mtx_timedlock(&logctx.mtx, 5000)
#define MTX_TRYLOCK()    os_mtx_trylock(&logctx.mtx)
#define MTX_LOCK()       os_mtx_lock(&logctx.mtx)
#define MTX_UNLOCK()     os_mtx_unlock(&logctx.mtx)
#define MTX_INIT()       os_mtx_init(&logctx.mtx, 0)


static struct disklog_ctx logctx;

static void disklog_reset_locked(void) {
    struct disk_log *filp = &logctx.file;
    size_t blksz = logctx.blksz;
    
    filp->magic  = DISKLOG_MAGIC;
    filp->start  = blksz;
    filp->size   = ((logctx.len - blksz) / blksz) * blksz;
    filp->end    = filp->start + filp->size;
    filp->rd_ofs = filp->start;
    filp->wr_ofs = filp->start;
    filp->d_size = 0;
}

static void disklog_sync(bool wait) {
    if (logctx.log_dirty) {
    	int err;
    
	    if (wait) {
	        err = MTX_LOCK_TIMED();
	    } else {
	    	err = MTX_TRYLOCK();
		}

		if (err == 0) {
	        if (logctx.log_dirty) {
	            logctx.log_dirty = false;
	            disklog_flush();
	        }
	        MTX_UNLOCK();
	    }
    }
}

static int disklog_sync_listen(struct observer_base *nb,
	unsigned long action, void *data) {
    (void) data;
    (void) nb;
    (void) action;
    logctx.should_stop = true;
    disklog_sync(true);
    return 0;
}

static void disklog_swap(os_timer_t timer, void *arg) {
    (void) arg;
    disklog_sync(false);
    os_timer_mod(timer, DISLOG_SWAP_PERIOD);
}

void disklog_reset(void) {
    MTX_LOCK();
    logctx.read_locked = false;
    disklog_reset_locked();
    MTX_UNLOCK();
}

void disklog_flush(void) {
#ifdef CONFIG_DISKLOG_BIO
	rte_assert(logctx.bio != NULL);
	if (logctx.bio->panic) {
		buffered_flush_locked(logctx.bio);
		disk_device_erase(logctx.bio->dd, logctx.offset, logctx.blksz);
		disk_device_write(logctx.bio->dd, &logctx.file, 
			sizeof(logctx.file), logctx.offset);
	} else {
	    int ret = buffered_write(logctx.bio, &logctx.file, 
	        sizeof(logctx.file), logctx.offset);
	    rte_assert(ret > 0);
	    (void) ret;
		buffered_ioflush(logctx.bio, false);
	}
#else /* CONFIG_DISKLOG_BIO */
    rte_assert(logctx.dd != NULL);
    blkdev_write(logctx.dd, &logctx.file, sizeof(logctx.file),
        logctx.offset);
    blkdev_sync();
#endif /* CONFIG_DISKLOG_BIO */
}

int disklog_init(void) {
    const struct disk_partition *dp_dev;
    struct disk_device *dd;
    static bool initialized;
    int ret;

    if (initialized)
        return 0;

    MTX_INIT();
    dp_dev = disk_partition_find("syslog");
    rte_assert(dp_dev != NULL);
    dd = disk_device_find_by_part(dp_dev);

    MTX_LOCK();
#ifdef CONFIG_DISKLOG_BIO
    ret = buffered_iocreate(dd, dd->blk_size, false, &logctx.bio);
    if (ret) {
        rte_assert(0);
        goto _unlock;
    }
#else
    logctx.dd = dd;
#endif /* CONFIG_DISKLOG_BIO */

    lgpt_get_block_size(dp_dev, &logctx.blksz);
    logctx.offset = dp_dev->offset;
    logctx.len = dp_dev->len;

#ifdef CONFIG_DISKLOG_BIO
    ret = buffered_read(logctx.bio, &logctx.file, 
        sizeof(logctx.file), logctx.offset);
#else
    ret = blkdev_read(logctx.dd, &logctx.file, 
        sizeof(logctx.file), logctx.offset);
#endif /* CONFIG_DISKLOG_BIO */

    if (ret < 0) {
        MTX_UNLOCK();
        pr_err("read disklog file failed(%d)\n", ret);
        rte_assert(0);
        goto _unlock;
    }
    if (logctx.file.magic != DISKLOG_MAGIC)
        disklog_reset_locked();

    os_timer_create(&logctx.timer, disklog_swap, 
        NULL, false);
    
    /* Register observer for system shutdown/reoot */
    logctx.obs.update = disklog_sync_listen;
    logctx.obs.priority = 10;
    system_add_observer(&logctx.obs);

	logctx.log_dirty = false;
    initialized = true;
    ret = 0;
    os_timer_add(logctx.timer, DISLOG_SWAP_PERIOD);

_unlock:
    MTX_UNLOCK();
    return ret;
}

int disklog_input(const char *buf, size_t size) {
#ifdef CONFIG_DISKLOG_BIO
    struct buffered_io *bio;
#else
    struct disk_device *dd;
#endif
    size_t remain = size;
    uint32_t wr_ofs, bytes;
    int ret;

    if (buf == NULL || size == 0)
        return -EINVAL;

    if (logctx.read_locked) 
        return -EBUSY;

#ifdef CONFIG_DISKLOG_BIO
	bio = logctx.bio;
	if (rte_likely(!bio->panic)) {
	    ret = MTX_TRYLOCK();
	    if (ret) {
	        if (logctx.upload_pending)
	            return -EBUSY;
	        MTX_LOCK();
	    }
	}
#else /* !CONFIG_DISKLOG_BIO */
    dd = logctx.dd;
    ret = MTX_TRYLOCK();
    if (ret) {
        if (logctx.upload_pending)
            return -EBUSY;
        MTX_LOCK();
    }
#endif /* CONFIG_DISKLOG_BIO */

    struct disk_log *filp = &logctx.file;
    wr_ofs = filp->wr_ofs;

    while (remain > 0) {
        bytes = MIN(filp->end - wr_ofs, remain);
#ifdef CONFIG_DISKLOG_BIO
        ret = buffered_write(bio, buf, bytes, 
            logctx.offset + wr_ofs);
#else
        ret = blkdev_write(dd, buf, bytes, 
            logctx.offset + wr_ofs);
#endif /* CONFIG_DISKLOG_BIO */
        if (ret < 0) 
            goto _next;
        
        remain -= bytes;
        wr_ofs += bytes;
        buf += bytes;
        if (wr_ofs >= filp->end)
            wr_ofs = filp->start;
    }

    filp->d_size += size - remain;
    if (filp->d_size > filp->size) {
        filp->d_size = filp->size;
        filp->rd_ofs = wr_ofs;
    }
    ret = 0;
_next:
    filp->wr_ofs = wr_ofs;
	logctx.log_dirty = true;
#ifdef CONFIG_DISKLOG_BIO
	if (rte_likely(!bio->panic))
    	MTX_UNLOCK();
#else
    MTX_UNLOCK();
#endif /* CONFIG_DISKLOG_BIO */
    return ret;
}

int disklog_ouput(bool (*output)(void *ctx, char *buf, size_t size), 
    void *ctx, size_t maxsize) {
#define LOG_SIZE 1024
    uint32_t rd_ofs, bytes;
    char *buffer;
    int ret;
	
    if (output == NULL)
        return -EINVAL;
		
    if (logctx.upload_pending)
        return -EBUSY;
		
    if (maxsize == 0)
        maxsize = LOG_SIZE;
		
    buffer = general_malloc(maxsize+1);
    if (buffer == NULL)
        return -ENOMEM;

    ret = 0;
    logctx.upload_pending = true;
    MTX_LOCK();
    struct disk_log *filp = &logctx.file;
    size_t size = filp->d_size;
    size_t remain = size;
    rd_ofs = filp->rd_ofs;
    while (remain > 0) {
        if (logctx.should_stop)
            goto _next;

        bytes = MIN(filp->end - rd_ofs, remain);
        bytes = MIN(bytes, maxsize);
#ifdef CONFIG_DISKLOG_BIO
        ret = buffered_read(logctx.bio, buffer, bytes, 
            logctx.offset + rd_ofs);
#else
        ret = blkdev_read(logctx.dd, buffer, bytes, 
            logctx.offset + rd_ofs);
#endif /* CONFIG_DISKLOG_BIO */
        if (ret <= 0)
            goto _next;
		
        if (!output(ctx, buffer, bytes)) {
            ret = -EIO;
            goto _next;
        }

        remain -= bytes;
        rd_ofs += bytes;
        if (rd_ofs >= filp->end)
            rd_ofs = filp->start;
    }
    
	filp->rd_ofs = filp->wr_ofs = rd_ofs;
	filp->d_size = 0;
	ret = 0;
_next:
    MTX_UNLOCK();
	logctx.upload_pending = false;
    general_free(buffer);
    return ret;    
}

ssize_t disklog_read(char *buffer, size_t maxlen, bool first) {
    static struct disk_log rdlog;
    uint32_t rd_ofs, bytes;
    int ret = 0;

    if (!buffer || !maxlen)
        return -EINVAL;

    ret = MTX_TRYLOCK();
    if (ret) {
        if (logctx.upload_pending)
            return -EBUSY;
        MTX_LOCK();
    }

    if (first)
        rdlog = logctx.file;

    size_t size = rdlog.d_size;
    size_t remain = MIN(maxlen, size);
    rd_ofs = rdlog.rd_ofs;
    while (remain > 0) {
        bytes = MIN(rdlog.end - rd_ofs, remain);
#ifdef CONFIG_DISKLOG_BIO
        ret = buffered_read(logctx.bio, buffer, bytes, 
            logctx.offset + rd_ofs);
#else
        ret = blkdev_read(logctx.dd, buffer, bytes, 
            logctx.offset + rd_ofs);
#endif /* CONFIG_DISKLOG_BIO */
        if (ret <= 0)
            goto _next;

        remain -= bytes;
        rd_ofs += bytes;
        size -= bytes;
        if (rd_ofs >= rdlog.end)
            rd_ofs = rdlog.start;
    }
    rdlog.rd_ofs = rd_ofs;
    rdlog.d_size = size;
_next:
    MTX_UNLOCK();
    return ret;   
}

void disklog_upload_cb(struct log_upload_class *luc) {
    if (!luc || !luc->upload)
        return;
    if (luc->begin)
        luc->begin(luc->ctx);

    int err = disklog_ouput(luc->upload, luc->ctx, luc->maxsize);

    if (luc->end)
        luc->end(luc->ctx, err);
}

int disklog_read_lock(bool enable) {
    MTX_LOCK();
    logctx.read_locked = enable;
    MTX_UNLOCK();
    return 0;
}
