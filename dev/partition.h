/*
 * Copyright 2022 wtcat
 *
 * Borrowed from rt-thread
 */
#ifndef BASEWORK_LIB_PARTITION_H_
#define BASEWORK_LIB_PARTITION_H_

#include "basework/generic.h"

#ifdef __cplusplus
extern "C"{
#endif
struct disk_device;

struct disk_partition {
    const char *name;
    const char *parent;
    long offset;
    size_t len;
    const struct disk_partition *child;
};


/*
 * Static partition table define
 */

#define PARTITION_TABLE_DECLARE(name) \
    extern struct disk_partition name[]
#define PARTITION_TABLE_DEFINE(name) \
    static struct disk_partition name[] __rte_used = 
#define PARTITION_ENTRY(_name, _parent, _offset, _size) \
    { \
        .name = _name, \
        .parent = _parent, \
        .offset = _offset, \
        .len = _size, \
        .child = NULL \
    }
#define PARTITION_TERMINAL {NULL, NULL, 0, 0} 

/*
 * disk_device_find_by_part - Find disk device by partition handle
 *
 * @part: pointer to partition handle
 * return disk device if success
 */
struct disk_device *disk_device_find_by_part(const struct disk_partition *part);

/*
 * disk_partition_find - Find partition by name
 *
 * @name: partition name
 * return NULL if failed
 */
const struct disk_partition *disk_partition_find(const char *name);

/*
 * disk_partition_register - Register partition table
 *
 * @pt: partition table address
 * @len: partition length
 * return 0 if success
 */
int disk_partition_register(const struct disk_partition *pt, size_t len);

/*
 * disk_partition_read - Read data from partition
 *
 * @part: partition
 * @addr: relative address for partition
 * @buf: read buffer
 * @size: read size
 * return >= 0: successful read data size
 *           -1: error
 */
int disk_partition_read(const struct disk_partition *part, uint32_t addr, 
    void *buf, size_t size);

/*
 * disk_partition_write - Write data to partition
 *
 * @part: partition
 * @addr: relative address for partition
 * @buf: write buffer
 * @size: write size
 * return >= 0: successful write data size
 *           -1: error
 */
int disk_partition_write(const struct disk_partition *part, uint32_t addr, 
    const void *buf, size_t size);

/*
 * lgpt_read - Read data from partition (with cache)
 *
 * @part: partition
 * @offset: relative address for partition
 * @buf: read buffer
 * @size: read size
 * return >= 0: successful read data size
 *           -1: error
 */
int lgpt_read(const struct disk_partition *part, uint32_t offset, 
    void *buf, size_t size);

/*
 * lgpt_write - Write data to partition（no need to erase）
 *
 * @part: partition
 * @offset: relative address for partition
 * @buf: write buffer
 * @size: write size
 * return >= 0: successful write data size
 *           -1: error
 */
int lgpt_write(const struct disk_partition *part, uint32_t offset, 
    const void *buf, size_t size);

/*
 * lgpt_get_block_size - Get the block size of disk device
 *
 * @part: partition
 * @blksz: device block size
 * return 0 if success
 */
int lgpt_get_block_size(const struct disk_partition *part, size_t *blksz);

/*
 * disk_partition_erase - Erase partition data
 *
 * @part: partition
 * @addr: relative address for partition
 * @size: erase size
 * return >= 0: successful erased data size
 *           -1: error
 */
int disk_partition_erase(const struct disk_partition *part, uint32_t addr, 
    size_t size);

/*
 * disk_partition_erase_all - Erase partition all data
 *
 * @part: partition
 * return >= 0: successful erased data size
 *           -1: error
 */
int disk_partition_erase_all(const struct disk_partition *part);


/*
 * disk_partition_dump - Show all partition information
 */
void disk_partition_dump(void);
void __disk_partition_dump(const char *title, const struct disk_partition *dp, 
    size_t len);

/*
 * partitions_configure_build - Initialize partition configuration table
 *
 * @base_addr: the base addresss of partition table
 * @size: the size of partition table
 * @phydev: disk device name
 * @sequence: 
 *
 * return 0 if success
 */
int partitions_configure_build(long base_addr, size_t size, 
    const char *phydev, bool sequence);

/*
 * logic_partitions_create - Create a logic parition from the parent partition
 *
 * @ppt: parent partition name
 * @sublist: the sub-partition table
 * return 0 if success
 */
int logic_partitions_create(const char *ppt, struct disk_partition *sublist);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_LIB_PARTITION_H_ */
