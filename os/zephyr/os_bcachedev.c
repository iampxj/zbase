/*
 * Copyright 2024 wtcat
 */

#define pr_fmt(fmt) "<bdev>: "fmt
#define CONFIG_LOGLEVEL LOGLEVEL_DEBUG

#include <errno.h>
#include <init.h>
#include <device.h>
#include <drivers/flash.h>

#include "basework/generic.h"
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
flash_io_request(struct flash_context *ctx, struct bcache_request *r, 
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
	default:
		return -EINVAL;
	}

	bcache_request_done(r, err);
	return err;
}

static int 
flash_driver_entry(struct bcache_device *dd, uint32_t req, void *argp) {
	struct flash_context *ctx = bcache_disk_get_driver_data(dd);
	if (req == BCACHE_IO_REQUEST)
		return flash_io_request(ctx, argp, dd->block_size);

	return bcache_ioctl(dd, req, argp);
}

static int flash_driver_init(const struct device *dev) {
	static struct flash_context media;
	const struct disk_partition *part;
	(void) dev;

	part = disk_partition_find("firmware");
	if (!part)
		return -ENODEV;

	media.dev = device_get_binding("spi_nand");
	if (media.dev) {
		size_t blksize = disk_device_find_by_part(part)->blk_size;
		media.start = part->offset;
		media.size = part->len;
		media.blksize = (blksize < 2048)? 2048: blksize;
		return bcache_dev_create("spi_nand", 
			media.blksize,
			media.size / media.blksize, 
			flash_driver_entry,
			&media,
			NULL
		);
	}
	return -EINVAL;
}

const struct bcache_config bcache_configuration = {
	.max_read_ahead_blocks = 0,
	.max_write_blocks = 16, 
	.swapout_priority = 4,
	.swapout_period = 2000,
	.swap_block_hold = 60*1000,
	.swapout_workers = 0,
	.swapout_worker_priority = 4,
	.task_stack_size = 4096,
	.size = 64*1024,
	.buffer_min = 2048,
	.buffer_max = 4096,
	.read_ahead_priority = 8
};

SYS_INIT(flash_driver_init, APPLICATION, 50);
