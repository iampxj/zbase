/*
 * Copyright 2022 wtcat
 */
#ifndef BASEWORK_DEV_BLKDEV_H_
#define BASEWORK_DEV_BLKDEV_H_

#include <stdint.h>
#include <sys/types.h>

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
ssize_t blkdev_write(struct disk_device *dd, const void *buf, size_t size, 
    uint32_t offset);

/*
 * blkdev_write - Block device read 
 *
 * @dd: disk device
 * @buf: buffer pointer
 * @size: buffer size
 * @offset: address of the disk device
 * return read bytes if success 
 */
ssize_t blkdev_read(struct disk_device *dd, void *buf, size_t size, 
    uint32_t offset);

/*
 * blkdev_sync - Force sync block device data to disk
 * return 0 if success
 */
int blkdev_sync(void);

/*
 * blkdev_invalid_sync - Force sync block device data to disk 
 * and invalid cache
 *
 * return 0 if success
 */
int blkdev_sync_invalid(void);

/*
 * blkdev_init - Initialize block device
 * return 0 if success
 */
int blkdev_init(void);

/*
 * blkdev_print - Print block device statistics information
 */
void blkdev_print(void);

/*
 * blkdev_destroy - Destroy block device
 * return 0 if success
 */
int blkdev_destroy(void);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEV_BLKDEV_H_ */
