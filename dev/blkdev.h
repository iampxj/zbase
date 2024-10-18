/*
 * Copyright 2022 wtcat
 */
#ifndef BASEWORK_DEV_BLKDEV_H_
#define BASEWORK_DEV_BLKDEV_H_

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "basework/dev/disk.h"

#ifdef CONFIG_BCACHE
#include "basework/dev/bcache.h"
#endif

#ifdef _WIN32
#ifndef __ssize_t_defined
#define __ssize_t_defined
typedef long ssize_t;
#endif
#endif //_WIN32

#ifdef __cplusplus
extern "C"{
#endif
struct disk_device;

/*
 * blkdev_write - Block device write 
 *
 * @dd: disk device
 * @buf: buffer pointer
 * @size: buffer size
 * @offset: address of the disk device
 * return writen bytes if success 
 */
#ifdef CONFIG_BCACHE
#define blkdev_write(dd, buf, size, offset) \
    bcache_blkdev_write((struct bcache_device *)(dd)->bdev, buf, size, offset)
#else
#define blkdev_write(dd, buf, size, offset) \
    simple_blkdev_write(dd, buf, size, offset)
#endif /* CONFIG_BCACHE */

/*
 * blkdev_write - Block device read 
 *
 * @dd: disk device
 * @buf: buffer pointer
 * @size: buffer size
 * @offset: address of the disk device
 * return read bytes if success 
 */
#ifdef CONFIG_BCACHE
#define blkdev_read(dd, buf, size, offset) \
    bcache_blkdev_read((struct bcache_device *)(dd)->bdev, buf, size, offset)
#else
#define blkdev_read(dd, buf, size, offset) \
    simple_blkdev_read(dd, buf, size, offset)
#endif /* CONFIG_BCACHE */

/*
 * blkdev_sync - Force sync block device data to disk
 * return 0 if success
 */
#ifdef CONFIG_BCACHE
#define blkdev_sync() bcache_blkdev_sync(false)
#else
#define blkdev_sync() simple_blkdev_sync()
#endif /* CONFIG_BCACHE */

/*
 * blkdev_invalid_sync - Force sync block device data to disk 
 * and invalid cache
 *
 * return 0 if success
 */
#ifdef CONFIG_BCACHE
#define blkdev_sync_invalid() bcache_blkdev_sync(true)
#else
#define blkdev_sync_invalid() simple_blkdev_sync_invalid()
#endif /* CONFIG_BCACHE */



/*
 * blkdev_init - Initialize block device
 * return 0 if success
 */
#ifdef CONFIG_BCACHE
#define blkdev_init(dev) bcache_init()
#else
#define blkdev_init(dev) simple_blkdev_init(dev)
#endif /* CONFIG_BCACHE */


/*
 * blkdev_print - Print block device statistics information
 */
#ifdef CONFIG_BCACHE
#define blkdev_print() (void)0
#else
#define blkdev_print() simple_blkdev_print()
#endif /* CONFIG_BCACHE */

/*
 * blkdev_destroy - Destroy block device
 * return 0 if success
 */
#define blkdev_destroy() (void)0

/*
 * Simple block device api
 */
int  simple_blkdev_init(struct disk_device *dd);
void simple_blkdev_print(void);
int  simple_blkdev_sync(void);
int  simple_blkdev_sync_invalid(void);
ssize_t simple_blkdev_write(struct disk_device *dd, const void *buf, size_t size, 
    uint32_t offset);
ssize_t simple_blkdev_read(struct disk_device *dd, void *buf, size_t size, 
    uint32_t offset);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEV_BLKDEV_H_ */
