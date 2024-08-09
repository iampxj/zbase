/*
 * Copyright 2024 wtcat
 */
#include <errno.h>
#include <init.h>
#include <drivers/flash.h>

#include "basework/dev/bcache.h"

struct flash_context {
	const struct device *dev;
	uint32_t start;
	uint32_t size;
	uint32_t blksize;
};

static inline int
flash_io_request(struct flash_context *ctx, struct bcache_request *r, size_t blksize) {
	struct bcache_sg_buffer *sg = r->bufs;
	uint32_t offset;
	int err;

	switch (r->req) {
	case BCACHE_DEV_REQ_READ:
		for (uint32_t i = 0; i < r->bufnum; i++, sg++) {
			offset = ctx->start + 
			err = flash_read(ctx->dev, sg->block * blksize, sg->buffer, sg->length);
			if (err)
				break;
		}
		break;
	case BCACHE_DEV_REQ_WRITE:
		for (uint32_t i = 0; i < r->bufnum; i++, sg++) {
			err = flash_erase(ctx->dev, sg->block * blksize, blksize);
			if (rte_unlikely(err))
				break;

			err = flash_write(ctx->dev, sg->block * blksize), sg->buffer, sg->length);
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
	static struct flash_context nand;

	(void) dev;

	nand.dev = device_get_binding("spi_nand");
	if (nand.dev) {

	bcache_dev_create("spi_nand", 2048,
		sizeof(ramdisk_inst.area) / 512, 
		ramdisk_driver_entry,
		&ramdisk_inst,
		&dd
	);
	}
}

SYS_INIT(flash_driver_init, APPLICATION, 50);
