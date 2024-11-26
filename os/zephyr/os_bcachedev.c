/*
 * Copyright 2024 wtcat
 */

#define pr_fmt(fmt) "<bdev>: "fmt
#define CONFIG_LOGLEVEL LOGLEVEL_DEBUG

#include <errno.h>
#include <device.h>
#include <drivers/flash.h>

#include "basework/generic.h"
#include "basework/malloc.h"
#include "basework/dev/bcache.h"
#include "basework/dev/partition.h"
#include "basework/dev/disk.h"
#include "basework/log.h"

struct flash_context {
	const struct device *dev;
	uint32_t start;
	uint32_t size;
	uint32_t blksize;
};

static __rte_always_inline int
bdev_io_request(struct flash_context *ctx, struct bcache_request *r, 
	size_t blksize) {
	struct bcache_sg_buffer *sg = r->bufs;
	int err;

	switch (r->req) {
	case BCACHE_DEV_REQ_READ:
		pr_dbg("%s read %d blocks (blksize: %d)\n", __func__, r->bufs, blksize);
		for (uint32_t i = 0; i < r->bufnum; i++, sg++) {
			uint32_t offset = ctx->start + sg->block * blksize;
			err = flash_read(ctx->dev, offset, sg->buffer, sg->length);
			if (err)
				break;
		}
		break;
	case BCACHE_DEV_REQ_WRITE:
		pr_dbg("%s write %d blocks (blksize: %d)\n", __func__, r->bufs, blksize);
		for (uint32_t i = 0; i < r->bufnum; i++, sg++) {
			uint32_t offset = ctx->start + sg->block * blksize;
			err = flash_erase(ctx->dev, offset, blksize);
			if (rte_unlikely(err))
				break;

			err = flash_write(ctx->dev, offset, sg->buffer, sg->length);
			if (rte_unlikely(err))
				break;
		}
		break;
	case BCACHE_DEV_REQ_SYNC:
		flash_flush(ctx->dev);
		break;
	default:
		return -EINVAL;
	}

	bcache_request_done(r, err);
	return err;
}

static int 
bdev_driver_handler(struct bcache_device *dd, uint32_t req, void *argp) {
	struct flash_context *ctx = bcache_disk_get_driver_data(dd);
	if (rte_likely(req == BCACHE_IO_REQUEST))
		return bdev_io_request(ctx, argp, dd->block_size);

	if (req == BCACHE_IO_CAPABILITIES) {
		*(uint32_t *)argp = BCACHE_DEV_CAP_SYNC;
		return 0;
	}

	return bcache_ioctl(dd, req, argp);
}

int
platform_bdev_register(struct disk_device *dd) {
	struct flash_context *media;

	media = general_malloc(sizeof(*media));
	if (media) {
		media->dev = (struct device *)dd->dev;
		media->start = dd->addr;
		media->size = dd->len;
		media->blksize = dd->blk_size;
		int err = bcache_blkdev_create(disk_device_get_name(dd),
			media->blksize,
			media->size / media->blksize, 
			bdev_driver_handler,
			media,
			(struct bcache_device **)&dd->bdev
		);
		if (err) {
			pr_err("create bcache device(start: 0x%x size:0x%x blksize:%x) failed(%d)\n", 
				media->start, media->size, media->blksize, err);
			general_free(media);
			return err;
		}
		
		return 0;
	}
	
	return -ENOMEM;
}

const struct bcache_config bcache_configuration = {
	.max_read_ahead_blocks = 0,
	.max_write_blocks = 16, 
	.swapout_priority = 4,
	.swapout_period = 2000,
	.swap_block_hold = 16*1000,
	.swapout_workers = 0,
	.swapout_worker_priority = 2,
	.task_stack_size = 4096,
	.size = 32*1024,
	.buffer_min = 2048,
	.buffer_max = 4096,
	.read_ahead_priority = 8
};
