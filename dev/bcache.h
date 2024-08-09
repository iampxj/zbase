/*
 * Copyright (C) 2001 OKTET Ltd., St.-Petersburg, Russia
 * Author: Victor V. Vengerov <vvv@oktet.ru>
 *
 * Copyright (C) 2008,2009 Chris Johns <chrisj@rtems.org>
 *    Rewritten to remove score mutex access. Fixes many performance
 *    issues.
 *    Change to support demand driven variable buffer sizes.
 *
 * Copyright (C) 2009, 2012 embedded brains GmbH & Co. KG
 */

/*
 * Coypright 2024 wtcat
 */

#ifndef BASEWORK_DEV_BCACHE_H_
#define BASEWORK_DEV_BCACHE_H_

#include "basework/container/list.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t bcache_num_t;
struct bcache_request;
struct bcache_device;
struct bcache_stats;
struct bcache_group;
struct printer;

/**
 * @brief Block device request done callback function type.
 */
typedef void (*bcache_request_cb)(struct bcache_request *req, int status);

/**
 * @brief Block device IO control handler type.
 */
typedef int (*bcache_device_ioctl)(struct bcache_device *dd, uint32_t req, void *argp);

/**
 * @brief Block device request type.
 *
 * @warning The sync request is an IO one and only used from the cache. Use the
 *          Block IO when operating at the device level. We need a sync request
 *          to avoid requests looping for ever.
 */
enum bcache_request_op {
	BCACHE_DEV_REQ_READ,  /**< Read the requested blocks of data. */
	BCACHE_DEV_REQ_WRITE, /**< Write the requested blocks of data. */
	BCACHE_DEV_REQ_SYNC	  /**< Sync any data with the media. */
};

/**
 * @brief Block device scatter or gather buffer structure.
 */
struct bcache_sg_buffer {
	bcache_num_t block;
	uint32_t     length;
	void         *buffer;
	void         *user;
};

/**
 * @brief The block device transfer request is used to read or write a number
 * of blocks from or to the device.
 *
 * Transfer requests are issued to the disk device driver with the
 * @ref BCACHE_IO_REQUEST IO control.  The transfer request completion status
 * must be signalled with bcache_request_done().  This function must be
 * called exactly once per request.  The return value of the IO control will be
 * ignored for transfer requests.
 *
 */
struct bcache_request {
	enum bcache_request_op req;
	bcache_request_cb done;
	void *done_arg;
	int status;
	uint32_t bufnum;
	void *io_task;
	/*
	 * TODO: The use of these req blocks is not a great design. The req is a
	 *       struct with a single 'bufs' declared in the req struct and the
	 *       others are added in the outer level struct. This relies on the
	 *       structs joining as a single array and that assumes the compiler
	 *       packs the structs. Why not just place on a list ? The BD has a
	 *       node that can be used.
	 */

	struct bcache_sg_buffer bufs[];
};

/**
 * @brief Signals transfer request completion status.
 *
 * This function must be called exactly once per request.
 *
 * @param[in,out] req The transfer request.
 * @param[in] status The status of the operation should be
 */
static inline void 
bcache_request_done(struct bcache_request *req, int status) {
	(*req->done)(req, status);
}

/**
 * @brief The start block in a request.
 *
 * Only valid if the driver has returned the
 * @ref BCACHE_DEV_CAP_MULTISECTOR_CONT capability.
 */
#define BCACHE_DEV_START_BLOCK(req) (req->bufs[0].block)

/**
 * @name IO Control Request Codes
 */
#define BCACHE_IO_REQUEST          0
#define BCACHE_IO_GETMEDIABLKSIZE  1
#define BCACHE_IO_GETBLKSIZE       2
#define BCACHE_IO_SETBLKSIZE       3
#define BCACHE_IO_GETSIZE          4
#define BCACHE_IO_SYNCDEV          5
#define BCACHE_IO_DELETED          6
#define BCACHE_IO_CAPABILITIES     7
#define BCACHE_IO_GETDISKDEV       8
#define BCACHE_IO_PURGEDEV         9
#define BCACHE_IO_GETDEVSTATS      10
#define BCACHE_IO_RESETDEVSTATS    11

/**
 * @name Block Device Driver Capabilities
 */

/**
 * @brief Only consecutive multi-sector buffer requests are supported.
 *
 * This option means the cache will only supply multiple buffers that are
 * inorder so the ATA multi-sector command for example can be used. This is a
 * hack to work around the current ATA driver.
 */
#define BCACHE_DEV_CAP_MULTISECTOR_CONT (1 << 0)

/**
 * @brief The driver will accept a sync call.
 *
 * A sync call is made to a driver after a bdbuf cache sync has finished.
 */
#define BCACHE_DEV_CAP_SYNC (1 << 1)

/** @} */

/**
 * @brief Common IO control primitive.
 *
 * Use this in all block devices to handle the common set of IO control
 * requests.
 */
int bcache_ioctl(struct bcache_device *dd, uint32_t req, void *argp);

/**
 * @brief Creates a block device.
 *
 * The block size is set to the media block size.
 *
 * @param[in] device The path for the new block device.
 * @param[in] media_block_size The media block size in bytes.  Must be positive.
 * @param[in] media_block_count The media block count.  Must be positive.
 * @param[in] handler The block device IO control handler.  Must not be @c NULL.
 * @param[in] driver_data The block device driver data.
 *
 * @retval 0 Successful operation.
 */
int bcache_dev_create(const char *device, uint32_t media_block_size,
					  bcache_num_t media_block_count, bcache_device_ioctl handler,
					  void *driver_data, struct bcache_device **dd);

struct bcache_device* bcache_dev_find(const char* device);


/**
 * @brief Prints the block device statistics.
 */
void bcache_print_stats(const struct bcache_stats *stats, uint32_t media_block_size,
						uint32_t media_block_count, uint32_t block_size,
						struct printer *printer);


/**
 * @brief Trigger value to disable further read-ahead requests.
 */
#define BCACHE_READ_AHEAD_NO_TRIGGER ((bcache_num_t)-1)

/**
 * @brief Size value to set number of blocks based on config and disk size.
 */
#define BCACHE_READ_AHEAD_SIZE_AUTO (0)

/**
 * @brief Block device read-ahead control.
 */
struct bcache_read_ahead {
	/**
	 * @brief Chain node for the read-ahead request queue of the read-ahead task.
	 */
	struct rte_list node;

	/**
	 * @brief Block value to trigger the read-ahead request.
	 *
	 * A value of @ref BCACHE_READ_AHEAD_NO_TRIGGER will disable further
	 * read-ahead requests (except the ones triggered by @a bcache_peek)
	 * since no valid block can have this value.
	 */
	bcache_num_t trigger;

	/**
	 * @brief Start block for the next read-ahead request.
	 *
	 * In case the trigger value is out of range of valid blocks, this value my
	 * be arbitrary.
	 */
	bcache_num_t next;

	/**
	 * @brief Size of the next read-ahead request in blocks.
	 *
	 * A value of @ref BCACHE_READ_AHEAD_SIZE_AUTO will try to read the rest
	 * of the disk but at most the configured max_read_ahead_blocks.
	 */
	uint32_t nr_blocks;
};

/**
 * @brief Block device statistics.
 *
 * Integer overflows in the statistic counters may happen.
 */
struct bcache_stats {
	uint32_t read_hits;
	uint32_t read_misses;
	uint32_t read_ahead_transfers;
	uint32_t read_ahead_peeks;
	uint32_t read_blocks;
	uint32_t read_errors;
	uint32_t write_transfers;
	uint32_t write_blocks;
	uint32_t write_errors;
};

/**
 * @brief Description of a disk device (logical and physical disks).
 *
 * An array of pointer tables to struct bcache_device structures is maintained.
 * The first table will be indexed by the major number and the second table
 * will be indexed by the minor number.  This allows quick lookup using a data
 * structure of moderated size.
 */
struct bcache_device {
	uint32_t dev;
	struct bcache_device *phys_dev;
	uint32_t capabilities;
	char *name;
	unsigned uses;
	bcache_num_t start;
	bcache_num_t size;
	uint32_t media_block_size;
	uint32_t block_size;
	uint32_t block_size_shift;
	bcache_num_t block_count;
	uint32_t media_blocks_per_block;
	int block_to_media_block_shift;
	size_t bds_per_group;
	bcache_device_ioctl ioctl;
	void *driver_data;
	bool deleted;
	struct bcache_stats stats;
#ifdef CONFIG_BCACHE_READ_AHEAD
	struct bcache_read_ahead read_ahead;
#endif
};

/**
 * @name Disk Device Data
 */
static inline void *
bcache_disk_get_driver_data(const struct bcache_device *dd) {
	return dd->driver_data;
}

static inline uint32_t 
bcache_disk_get_media_block_size(const struct bcache_device *dd) {
	return dd->media_block_size;
}

static inline uint32_t 
bcache_disk_get_block_size(const struct bcache_device *dd) {
	return dd->block_size;
}

static inline uint32_t 
bcache_disk_get_block_logsize(const struct bcache_device *dd) {
	return dd->block_size_shift;
}

static inline bcache_num_t 
bcache_disk_get_block_begin(const struct bcache_device *dd) {
	return dd->start;
}

static inline bcache_num_t 
bcache_disk_get_block_count(const struct bcache_device *dd) {
	return dd->size;
}

/* Internal function, do not use */
int bcache_disk_init_phys(struct bcache_device *dd, uint32_t block_size,
						bcache_num_t block_count,
						bcache_device_ioctl handler,
						void *driver_data);

/* Internal function, do not use */
int bcache_disk_init_log(struct bcache_device *dd, 
						struct bcache_device *phys_dd,
						bcache_num_t block_begin,
						bcache_num_t block_count);


/**
 * @brief State of a buffer of the cache.
 *
 * The state has several implications.  Depending on the state a buffer can be
 * in the AVL tree, in a list, in use by an entity and a group user or not.
 *
 * <table>
 *   <tr>
 *     <th>State</th><th>Valid Data</th><th>AVL Tree</th>
 *     <th>LRU List</th><th>Modified List</th><th>Synchronization List</th>
 *     <th>Group User</th><th>External User</th>
 *   </tr>
 *   <tr>
 *     <td>FREE</td><td></td><td></td>
 *     <td>X</td><td></td><td></td><td></td><td></td>
 *   </tr>
 *   <tr>
 *     <td>EMPTY</td><td></td><td>X</td>
 *     <td></td><td></td><td></td><td></td><td></td>
 *   </tr>
 *   <tr>
 *     <td>CACHED</td><td>X</td><td>X</td>
 *     <td>X</td><td></td><td></td><td></td><td></td>
 *   </tr>
 *   <tr>
 *     <td>ACCESS CACHED</td><td>X</td><td>X</td>
 *     <td></td><td></td><td></td><td>X</td><td>X</td>
 *   </tr>
 *   <tr>
 *     <td>ACCESS MODIFIED</td><td>X</td><td>X</td>
 *     <td></td><td></td><td></td><td>X</td><td>X</td>
 *   </tr>
 *   <tr>
 *     <td>ACCESS EMPTY</td><td></td><td>X</td>
 *     <td></td><td></td><td></td><td>X</td><td>X</td>
 *   </tr>
 *   <tr>
 *     <td>ACCESS PURGED</td><td></td><td>X</td>
 *     <td></td><td></td><td></td><td>X</td><td>X</td>
 *   </tr>
 *   <tr>
 *     <td>MODIFIED</td><td>X</td><td>X</td>
 *     <td></td><td>X</td><td></td><td>X</td><td></td>
 *   </tr>
 *   <tr>
 *     <td>SYNC</td><td>X</td><td>X</td>
 *     <td></td><td></td><td>X</td><td>X</td><td></td>
 *   </tr>
 *   <tr>
 *     <td>TRANSFER</td><td>X</td><td>X</td>
 *     <td></td><td></td><td></td><td>X</td><td>X</td>
 *   </tr>
 *   <tr>
 *     <td>TRANSFER PURGED</td><td></td><td>X</td>
 *     <td></td><td></td><td></td><td>X</td><td>X</td>
 *   </tr>
 * </table>
 */
typedef enum {
	/**
	 * @brief Free.
	 */
	BCACHE_STATE_FREE = 0,

	/**
	 * @brief Empty.
	 */
	BCACHE_STATE_EMPTY,

	/**
	 * @brief Cached.
	 */
	BCACHE_STATE_CACHED,

	/**
	 * @brief Accessed by upper layer with cached data.
	 */
	BCACHE_STATE_ACCESS_CACHED,

	/**
	 * @brief Accessed by upper layer with modified data.
	 */
	BCACHE_STATE_ACCESS_MODIFIED,

	/**
	 * @brief Accessed by upper layer with invalid data.
	 */
	BCACHE_STATE_ACCESS_EMPTY,

	/**
	 * @brief Accessed by upper layer with purged data.
	 */
	BCACHE_STATE_ACCESS_PURGED,

	/**
	 * @brief Modified by upper layer.
	 */
	BCACHE_STATE_MODIFIED,

	/**
	 * @brief Scheduled for synchronization.
	 */
	BCACHE_STATE_SYNC,

	/**
	 * @brief In transfer by block device driver.
	 */
	BCACHE_STATE_TRANSFER,

	/**
	 * @brief In transfer by block device driver and purged.
	 */
	BCACHE_STATE_TRANSFER_PURGED
} bcache_buf_state;

/**
 * To manage buffers we using buffer descriptors (BD). A BD holds a buffer plus
 * a range of other information related to managing the buffer in the cache. To
 * speed-up buffer lookup descriptors are organized in AVL-Tree. The fields
 * 'dd' and 'block' are search keys.
 */
struct bcache_buffer {
	struct rte_list link;

	union {
		struct bcache_avl_node {
			struct bcache_buffer *left;
			struct bcache_buffer *right;
			signed char cache; /**< Cache */
			signed char bal;   /**< The balance of the sub-tree */
		} avl;
		struct rte_hnode hash;
	};

	struct bcache_device *dd;
	bcache_num_t block;
	unsigned char *buffer;
	bcache_buf_state state;
	uint32_t waiters;
	struct bcache_group *group;
	uint32_t hold_timer;
	int references;
	void *user;
};

/**
 * A group is a continuous block of buffer descriptors. A group covers the
 * maximum configured buffer size and is the allocation size for the buffers to
 * a specific buffer size. If you allocate a buffer to be a specific size, all
 * buffers in the group, if there are more than 1 will also be that size. The
 * number of buffers in a group is a multiple of 2, ie 1, 2, 4, 8, etc.
 */
struct bcache_group {
	struct rte_list link;	   /**< Link the groups on a LRU list if they
								* have no buffers in use. */
	size_t bds_per_group;
	uint32_t users;			   /**< How many users the block has. */
	struct bcache_buffer *bdbuf; /**< First BD this block covers. */
};

/**
 * Buffering configuration definition. See confdefs.h for support on using this
 * structure.
 */
struct bcache_config {
	uint32_t max_read_ahead_blocks;
	uint32_t max_write_blocks;
	int swapout_priority;
	uint32_t swapout_period;
	uint32_t swap_block_hold;
	size_t swapout_workers;
	int swapout_worker_priority;
	size_t task_stack_size;
	size_t size;  /* Size of memory in the cache */
	uint32_t buffer_min;
	uint32_t buffer_max;
	int read_ahead_priority;
	void *(*stack_alloc)(size_t size);
	void *(*stack_free)(void *ptr);
};

/**
 * External reference to the configuration.
 *
 * The configuration is provided by the application.
 */
extern const struct bcache_config bcache_configuration;

/**
 * The default value for the maximum read-ahead blocks disables the read-ahead
 * feature.
 */
#define BCACHE_MAX_READ_AHEAD_BLOCKS_DEFAULT 0

/**
 * Default maximum number of blocks to write at once.
 */
#define BCACHE_MAX_WRITE_BLOCKS_DEFAULT 16

/**
 * Default swap-out task priority.
 */
#define BCACHE_SWAPOUT_TASK_PRIORITY_DEFAULT 15

/**
 * Default swap-out task swap period in milli seconds.
 */
#define BCACHE_SWAPOUT_TASK_SWAP_PERIOD_DEFAULT 250

/**
 * Default swap-out task block hold time in milli seconds.
 */
#define BCACHE_SWAPOUT_TASK_BLOCK_HOLD_DEFAULT 1000

/**
 * Default swap-out worker tasks. Currently disabled.
 */
#define BCACHE_SWAPOUT_WORKER_TASKS_DEFAULT 0

/**
 * Default swap-out worker task priority. The same as the swap-out task.
 */
#define BCACHE_SWAPOUT_WORKER_TASK_PRIORITY_DEFAULT                                 \
	BCACHE_SWAPOUT_TASK_PRIORITY_DEFAULT

/**
 * Default read-ahead task priority.  The same as the swap-out task.
 */
#define BCACHE_READ_AHEAD_TASK_PRIORITY_DEFAULT                                     \
	BCACHE_SWAPOUT_TASK_PRIORITY_DEFAULT

/**
 * Default task stack size for swap-out and worker tasks.
 */
#define BCACHE_TASK_STACK_SIZE_DEFAULT 1024

/**
 * Default size of memory allocated to the cache.
 */
#define BCACHE_CACHE_MEMORY_SIZE_DEFAULT (64 * 512)

/**
 * Default minimum size of buffers.
 */
#define BCACHE_BUFFER_MIN_SIZE_DEFAULT (512)

/**
 * Default maximum size of buffers.
 */
#define BCACHE_BUFFER_MAX_SIZE_DEFAULT (4096)

/**
 * Prepare buffering layer to work - initialize buffer descritors and (if it is
 * neccessary) buffers. After initialization all blocks is placed into the
 * ready state.
 *
 * @retval RTEMS_SUCCESSFUL Successful operation.
 * @retval RTEMS_CALLED_FROM_ISR Called from an interrupt context.
 * @retval RTEMS_INVALID_NUMBER The buffer maximum is not an integral multiple
 * of the buffer minimum.  The maximum read-ahead blocks count is too large.
 * @retval RTEMS_RESOURCE_IN_USE Already initialized.
 * @retval RTEMS_UNSATISFIED Not enough resources.
 */
int bcache_init(void);

/**
 * Get block buffer for data to be written into. The buffers is set to the
 * access or modified access state. If the buffer is in the cache and modified
 * the state is access modified else the state is access. This buffer contents
 * are not initialised if the buffer is not already in the cache. If the block
 * is already resident in memory it is returned how-ever if not in memory the
 * buffer is not read from disk. This call is used when writing the whole block
 * on a disk rather than just changing a part of it. If there is no buffers
 * available this call will block. A buffer obtained with this call will not be
 * involved in a transfer request and will not be returned to another user
 * until released. If the buffer is already with a user when this call is made
 * the call is blocked until the buffer is returned. The highest priority
 * waiter will obtain the buffer first.
 *
 * The block number is the linear block number. This is relative to the start
 * of the partition on the media.
 *
 * Before you can use this function, the bcache_init() routine must be
 * called at least once to initialize the cache, otherwise a fatal error will
 * occur.
 *
 * @param dd [in] The disk device.
 * @param block [in] Linear media block number.
 * @param bd [out] Reference to the buffer descriptor pointer.
 *
 * @retval RTEMS_SUCCESSFUL Successful operation.
 * @retval RTEMS_INVALID_ID Invalid block number.
 */
int bcache_get(struct bcache_device *dd, bcache_num_t block,
								  struct bcache_buffer **bd);

/**
 * Get the block buffer and if not already in the cache read from the disk. If
 * specified block already cached return. The buffer is set to the access or
 * modified access state. If the buffer is in the cache and modified the state
 * is access modified else the state is access. If block is already being read
 * from disk for being written to disk this call blocks. If the buffer is
 * waiting to be written it is removed from modified queue and returned to the
 * user. If the buffer is not in the cache a new buffer is obtained and the
 * data read from disk. The call may block until these operations complete. A
 * buffer obtained with this call will not be involved in a transfer request
 * and will not be returned to another user until released. If the buffer is
 * already with a user when this call is made the call is blocked until the
 * buffer is returned. The highest priority waiter will obtain the buffer
 * first.
 *
 * Before you can use this function, the bcache_init() routine must be
 * called at least once to initialize the cache, otherwise a fatal error will
 * occur.
 *
 * @param dd [in] The disk device.
 * @param block [in] Linear media block number.
 * @param bd [out] Reference to the buffer descriptor pointer.
 *
 * @retval RTEMS_SUCCESSFUL Successful operation.
 * @retval RTEMS_INVALID_ID Invalid block number.
 * @retval RTEMS_IO_ERROR IO error.
 */
int bcache_read(struct bcache_device *dd, bcache_num_t block,
								   struct bcache_buffer **bd);

/**
 * @brief Give a hint which blocks should be cached next.
 *
 * Provide a hint to the read ahead mechanism which blocks should be cached
 * next. This overwrites the default linear pattern. You should use it in (for
 * example) a file system to tell bdbuf where the next part of a fragmented file
 * is. If you know the length of the file, you can provide that too.
 *
 * Before you can use this function, the bcache_init() routine must be
 * called at least once to initialize everything. Otherwise you might get
 * unexpected results.
 *
 * @param dd [in] The disk device.
 * @param block [in] Linear media block number.
 * @param nr_blocks [in] Number of consecutive blocks that can be pre-fetched.
 */
void bcache_peek(struct bcache_device *dd, bcache_num_t block, uint32_t nr_blocks);

/**
 * Release the buffer obtained by a read call back to the cache. If the buffer
 * was obtained by a get call and was not already in the cache the release
 * modified call should be used. A buffer released with this call obtained by a
 * get call may not be in sync with the contents on disk. If the buffer was in
 * the cache and modified before this call it will be returned to the modified
 * queue. The buffers is returned to the end of the LRU list.
 *
 * Before you can use this function, the bcache_init() routine must be
 * called at least once to initialize the cache, otherwise a fatal error will
 * occur.
 *
 * @param bd [in] Reference to the buffer descriptor.  The buffer descriptor
 * reference must not be @c NULL and must be obtained via bcache_get() or
 * bcache_read().
 *
 * @retval RTEMS_SUCCESSFUL Successful operation.
 * @retval RTEMS_INVALID_ADDRESS The reference is NULL.
 */
int bcache_release(struct bcache_buffer *bd);

/**
 * Release the buffer allocated with a get or read call placing it on the
 * modified list.  If the buffer was not released modified before the hold
 * timer is set to the configuration value. If the buffer had been released
 * modified before but not written to disk the hold timer is not updated. The
 * buffer will be written to disk when the hold timer has expired, there are
 * not more buffers available in the cache and a get or read buffer needs one
 * or a sync call has been made. If the buffer is obtained with a get or read
 * before the hold timer has expired the buffer will be returned to the user.
 *
 * Before you can use this function, the bcache_init() routine must be
 * called at least once to initialize the cache, otherwise a fatal error will
 * occur.
 *
 * @param bd [in] Reference to the buffer descriptor.  The buffer descriptor
 * reference must not be @c NULL and must be obtained via bcache_get() or
 * bcache_read().
 *
 * @retval RTEMS_SUCCESSFUL Successful operation.
 * @retval RTEMS_INVALID_ADDRESS The reference is NULL.
 */
int bcache_release_modified(struct bcache_buffer *bd);

/**
 * Release the buffer as modified and wait until it has been synchronized with
 * the disk by writing it. This buffer will be the first to be transfer to disk
 * and other buffers may also be written if the maximum number of blocks in a
 * requests allows it.
 *
 * @note This code does not lock the sync mutex and stop additions to the
 *       modified queue.
 *
 * Before you can use this function, the bcache_init() routine must be
 * called at least once to initialize the cache, otherwise a fatal error will
 * occur.
 *
 * @param bd [in] Reference to the buffer descriptor.  The buffer descriptor
 * reference must not be @c NULL and must be obtained via bcache_get() or
 * bcache_read().
 *
 * @retval RTEMS_SUCCESSFUL Successful operation.
 * @retval RTEMS_INVALID_ADDRESS The reference is NULL.
 */
int bcache_sync(struct bcache_buffer *bd);

/**
 * Synchronize all modified buffers for this device with the disk and wait
 * until the transfers have completed. The sync mutex for the cache is locked
 * stopping the addition of any further modified buffers. It is only the
 * currently modified buffers that are written.
 *
 * @note Nesting calls to sync multiple devices will be handled sequentially. A
 * nested call will be blocked until the first sync request has complete.
 *
 * Before you can use this function, the bcache_init() routine must be
 * called at least once to initialize the cache, otherwise a fatal error will
 * occur.
 *
 * @param dd [in] The disk device.
 *
 * @retval RTEMS_SUCCESSFUL Successful operation.
 */
int bcache_syncdev(struct bcache_device *dd);

/**
 * @brief Purges all buffers corresponding to the disk device @a dd.
 *
 * This may result in loss of data.  The read-ahead state of this device is reset.
 *
 * Before you can use this function, the bcache_init() routine must be
 * called at least once to initialize the cache, otherwise a fatal error will
 * occur.
 *
 * @param dd [in] The disk device.
 */
void bcache_purge_dev(struct bcache_device *dd);

/**
 * @brief Sets the block size of a disk device.
 *
 * This will set the block size derived fields of the disk device.  If
 * requested the disk device is synchronized before the block size change
 * occurs.  Since the cache is unlocked during the synchronization operation
 * some tasks may access the disk device in the meantime.  This may result in
 * loss of data.  After the synchronization the disk device is purged to ensure
 * a consistent cache state and the block size change occurs.  This also resets
 * the read-ahead state of this disk device.  Due to the purge operation this
 * may result in loss of data.
 *
 * Before you can use this function, the bcache_init() routine must be
 * called at least once to initialize the cache, otherwise a fatal error will
 * occur.
 *
 * @param dd [in, out] The disk device.
 * @param block_size [in] The new block size in bytes.
 * @param sync [in] If @c true, then synchronize the disk device before the
 * block size change.
 *
 * @retval RTEMS_SUCCESSFUL Successful operation.
 * @retval RTEMS_INVALID_NUMBER Invalid block size.
 */
int bcache_set_block_size(struct bcache_device *dd, uint32_t block_size,
											 bool sync);

/**
 * @brief Returns the block device statistics.
 */
void bcache_get_device_stats(const struct bcache_device *dd, struct bcache_stats *stats);

/**
 * @brief Resets the block device statistics.
 */
void bcache_reset_device_stats(struct bcache_device *dd);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* BASEWORK_DEV_BCACHE_H_ */
