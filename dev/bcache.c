/*
 * Disk I/O buffering
 * Buffer managment
 *
 * Copyright (C) 2001 OKTET Ltd., St.-Peterburg, Russia
 * Author: Andrey G. Ivanov <Andrey.Ivanov@oktet.ru>
 *         Victor V. Vengerov <vvv@oktet.ru>
 *         Alexander Kukuta <kam@oktet.ru>
 *
 * Copyright (C) 2008,2009 Chris Johns <chrisj@rtems.org>
 *    Rewritten to remove score mutex access. Fixes many performance
 *    issues.
 *
 * Copyright (C) 2009, 2017 embedded brains GmbH & Co. KG
 */

/*
 * Coypright 2024 wtcat modified
 */

#define BCACHE_TRACE 0
#define pr_fmt(fmt) "<bcache>: "fmt

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "basework/generic.h"
#include "basework/os/osapi.h"
#include "basework/malloc.h"
#include "basework/dev/bcache.h"
#include "basework/assert.h"
#include "basework/log.h"

#define BDBUF_INVALID_DEV NULL

#ifndef RTE_CACHE_LINE_SIZE
#define RTE_CACHE_LINE_SIZE 64
#endif

#ifndef rte_likely
#define rte_likely(x) (x)
#endif

#ifndef rte_unlikely
#define rte_unlikely(x) (x)
#endif

#ifndef __rte_always_inline
#define __rte_always_inline inline
#endif


/*
 * Bcache hashmap 
 */
#ifdef CONFIG_BCACHE_HASH_MAP
#ifndef BCACHE_TABLESIZE
#define BCACHE_TABLESIZE 128
#endif
#define BCACHE_HASH_MASK (BCACHE_TABLESIZE - 1)
#define BCACHE_HASH_SHIFT 8
#define BCACHE_HASH(dd, blk)  \
	(((((uintptr_t)(blk)) >> BCACHE_HASH_SHIFT) ^ (uintptr_t)(blk)) &  \
	 BCACHE_HASH_MASK)
#endif /* CONFIG_BCACHE_HASH_MAP */

/*
 * Simpler label for this file.
 */
#define bdbuf_config bcache_configuration

/**
 * A swapout transfer transaction data. This data is passed to a worked thread
 * to handle the write phase of the transfer.
 */
struct bcache_swapout_transfer {
	struct rte_list bds;
	struct bcache_device *dd;
	bool syncing;
	struct bcache_request write_req;
};

/**
 * Swapout worker thread. These are available to take processing from the
 * main swapout thread and handle the I/O operation.
 */
struct bcache_swapout_worker {
	struct rte_list link;
	os_completion_t swapout_sync;
	os_thread_t *thread;								
	bool enabled;
	struct bcache_swapout_transfer transfer;				
};

/**
 * Buffer waiters synchronization.
 */
struct bcache_waiters {
	unsigned count;
	os_cond_t cond_var;
};

struct bcache_devnode {
	char *name;
	struct bcache_device dev;
	struct rte_list link;
};

/**
 * The BD buffer cache.
 */
struct bcache_cache {
	os_completion_t swapout_signal;
	os_thread_t *swapout;
	bool swapout_enabled;
	struct rte_list swapout_free_workers;
	struct bcache_buffer *bds;
	void *buffers;
	size_t buffer_min_count;
	size_t max_bds_per_group;
	uint32_t flags;
	os_mutex_t lock;
	os_mutex_t sync_lock;
	bool sync_active;
	os_completion_t *sync_requester;
	struct bcache_device *sync_device;
	union bcache_tree {
		struct bcache_buffer *tree;
#ifdef CONFIG_BCACHE_HASH_MAP
		struct rte_hlist hashq[BCACHE_TABLESIZE];
#endif
	} root;

	struct rte_list lru;
	struct rte_list modified;
	struct rte_list sync;
	struct bcache_waiters access_waiters;
	struct bcache_waiters transfer_waiters;
	struct bcache_waiters buffer_waiters;
	struct bcache_swapout_transfer *swapout_transfer;
	struct bcache_swapout_worker *swapout_workers;
	size_t group_count;
	struct bcache_group *groups;

#ifdef CONFIG_BCACHE_READ_AHEAD
	os_thread_t *read_ahead_task;
	os_completion_t read_ahead;
	struct rte_list read_ahead_chain;
	bool read_ahead_enabled;
#endif

	/* Device list (struct bcache_devnode) */
	struct rte_list dev_nodes;
};

typedef enum {
	BCACHE_FATAL_CACHE_WAIT_2,
	BCACHE_FATAL_CACHE_WAIT_TO,
	BCACHE_FATAL_CACHE_WAKE,
	BCACHE_FATAL_PREEMPT_DIS,
	BCACHE_FATAL_PREEMPT_RST,
	BCACHE_FATAL_RA_WAKE_UP,
	BCACHE_FATAL_RECYCLE,
	BCACHE_FATAL_SO_WAKE_1,
	BCACHE_FATAL_SO_WAKE_2,
	BCACHE_FATAL_STATE_0,
	BCACHE_FATAL_STATE_2,
	BCACHE_FATAL_STATE_4,
	BCACHE_FATAL_STATE_5,
	BCACHE_FATAL_STATE_6,
	BCACHE_FATAL_STATE_7,
	BCACHE_FATAL_STATE_8,
	BCACHE_FATAL_STATE_9,
	BCACHE_FATAL_STATE_10,
	BCACHE_FATAL_STATE_11,
	BCACHE_FATAL_SWAPOUT_RE,
	BCACHE_FATAL_TREE_RM,
	BCACHE_FATAL_WAIT_EVNT,
	BCACHE_FATAL_WAIT_TRANS_EVNT
} bcache_ecode;

/*
 * Bcache container operations   
 */
#ifdef CONFIG_BCACHE_HASH_MAP
#define bcache_container_search(_root, _dd, _block) \
	bcache_hash_search((_root), (_dd), (_block))
#define bcache_container_insert(_root, _buf) \
	bcache_hash_insert((_root), (_buf))
#define bcache_container_remove(_root, _buf) \
	bcache_hash_remove((_root), (_buf))
#define bcache_container_gather_for_purge(_list, _dd) \
	bcache_hash_gather_for_purge((_list), (_dd))

#else /* !CONFIG_BCACHE_HASH_MAP */
#define bcache_container_search(_root, _dd, _block) \
	bcache_avl_search((_root), (_dd), (_block))
#define bcache_container_insert(_root, _buf) \
	bcache_avl_insert((_root), (_buf))
#define bcache_container_remove(_root, _buf) \
	bcache_avl_remove((_root), (_buf))
#define bcache_container_gather_for_purge(_list, _dd)                                    \
	bcache_avl_gather_for_purge((_list), (_dd))
#endif /* CONFIG_BCACHE_HASH_MAP */

static void bcache_swapout_task(void *arg);
static void bcache_read_ahead_task(void *arg);
static void bcache_transfer_done(struct bcache_request *req, int status);

/**
 * The Buffer Descriptor cache.
 */
static struct bcache_cache bdbuf_cache;

#if BCACHE_TRACE
/**
 * Return the number of items on the list.
 *
 * @param list The chain control.
 * @return uint32_t The number of items on the list.
 */
uint32_t bcache_list_count(rtems_chain_control *list) {
	rtems_chain_node *node = rtems_chain_first(list);
	uint32_t count = 0;
	while (!rtems_chain_is_tail(list, node)) {
		count++;
		node = rtems_chain_next(node);
	}
	return count;
}

/**
 * Show the usage for the bdbuf cache.
 */
void bcache_show_usage(void) {
	uint32_t group;
	uint32_t total = 0;
	uint32_t val;

	for (group = 0; group < bdbuf_cache.group_count; group++)
		total += bdbuf_cache.groups[group].users;
	printf("bdbuf:group users=%lu", total);
	val = bcache_list_count(&bdbuf_cache.lru);
	printf(", lru=%lu", val);
	total = val;
	val = bcache_list_count(&bdbuf_cache.modified);
	printf(", mod=%lu", val);
	total += val;
	val = bcache_list_count(&bdbuf_cache.sync);
	printf(", sync=%lu", val);
	total += val;
	printf(", total=%lu\n", total);
}

/**
 * Show the users for a group of a bd.
 *
 * @param where A label to show the context of output.
 * @param bd The bd to show the users of.
 */
void bcache_show_users(const char *where, struct bcache_buffer *bd) {
	const char *states[] = {"FR", "EM", "CH", "AC", "AM", "AE",
							"AP", "MD", "SY", "TR", "TP"};

	printf("bdbuf:users: %15s: [%" PRIu32 " (%s)] %td:%td = %" PRIu32 " %s\n", where,
		   bd->block, states[bd->state], bd->group - bdbuf_cache.groups,
		   bd - bdbuf_cache.bds, bd->group->users, bd->group->users > 8 ? "<<<<<<<" : "");
}
#endif /* BCACHE_TRACE == 1 */

#ifndef BCACHE_MINIMUM_STACK_SIZE
#define BCACHE_MINIMUM_STACK_SIZE 1024
#endif

/**
 * The default maximum height of 32 allows for AVL trees having between
 * 5,704,880 and 4,294,967,295 nodes, depending on order of insertion.  You may
 * change this compile-time constant as you wish.
 */
#ifdef CONFIG_BCACHE_AVL_MAX_HEIGHT
#define BCACHE_AVL_MAX_HEIGHT CONFIG_BCACHE_AVL_MAX_HEIGHT
#else
#define BCACHE_AVL_MAX_HEIGHT (32)
#endif

#define bcache_lock(mtx)      os_mtx_lock(mtx)
#define bcache_unlock(mtx)    os_mtx_unlock(mtx)
#define bcache_lock_cache()   bcache_lock(&bdbuf_cache.lock)
#define bcache_unlock_cache() bcache_unlock(&bdbuf_cache.lock)
#define bcache_lock_sync()    bcache_lock(&bdbuf_cache.sync_lock)
#define bcache_unlock_sync()  bcache_unlock(&bdbuf_cache.sync_lock)

#define bcache_fatal(err) rte_assert0((err) == 0)

static int 
bcache_create_task(const char *name, int prio,
	void (*entry)(void *), 
	void *arg, 
	os_thread_t **pthrd) {
	size_t stack_size = bdbuf_config.task_stack_size;
	os_thread_t *thr;
	int err;

	if (pthrd == NULL)
		return -EINVAL;

	thr = general_calloc(1, sizeof(*thr) + stack_size);
	if (thr == NULL)
		return -ENOMEM;

	err = os_thread_spawn(thr, name, thr + 1, stack_size, 
		prio, entry, arg);
	if (err) {
		general_free(thr);
		return err;
	}
	
	*pthrd = thr;
	return 0;
}

static void 
bcache_delete_task(os_thread_t *thread) {
	if (thread) {
		os_thread_destroy(thread);
		general_free(thread);
	}
}

static __rte_always_inline void 
bcache_group_obtain(struct bcache_buffer *bd) {
	++bd->group->users;
}

static __rte_always_inline void 
bcache_group_release(struct bcache_buffer *bd) {
	--bd->group->users;
}

static void 
bcache_fatal_with_state(bcache_buf_state state, bcache_ecode error) {
	pr_err("bcache crash state(%d) error(%d)\n", state, error);
	bcache_fatal((((uint32_t)state) << 16) | error);
}

#ifdef CONFIG_BCACHE_HASH_MAP
static __rte_always_inline struct bcache_buffer *
bcache_hash_search(union bcache_tree *root, const struct bcache_device *dd,
	bcache_num_t block) {
	struct rte_hlist *head = &root->hashq[BCACHE_HASH(dd, block)];
	struct rte_hnode *pos;

	rte_hlist_foreach(pos, head) {
		struct bcache_buffer *p = rte_container_of(pos, 
			struct bcache_buffer, hash);
		if (p->dd == dd && p->block == block)
			return p;
	}
	return NULL;
}

static __rte_always_inline int 
bcache_hash_insert(union bcache_tree* root, struct bcache_buffer* node) {
	struct rte_hlist *head = &root->hashq[BCACHE_HASH(node->dd, node->block)];
	rte_hlist_add_head(&node->hash, head);
	return 0;
}

static __rte_always_inline int 
bcache_hash_remove(union bcache_tree *root, struct bcache_buffer *node) {
	rte_hlist_del(&node->hash);
	return 0;
}

#else /* !CONFIG_BCACHE_HASH_MAP */

static __rte_always_inline struct bcache_buffer *
bcache_avl_search(union bcache_tree *avl, const struct bcache_device *dd,
	bcache_num_t block) {
	struct bcache_buffer *p = &avl->tree;

	while ((p != NULL) && ((p->dd != dd) || (p->block != block))) {
		if (((uintptr_t)p->dd < (uintptr_t)dd) || 
			((p->dd == dd) && (p->block < block))) {
			p = p->avl.right;
		} else {
			p = p->avl.left;
		}
	}

	return p;
}

static int 
bcache_avl_insert(union bcache_tree *avl, struct bcache_buffer *node) {
	struct bcache_buffer **root = &avl->tree;
	const struct bcache_device *dd = node->dd;
	bcache_num_t block = node->block;

	struct bcache_buffer *p = *root;
	struct bcache_buffer *q = NULL;
	struct bcache_buffer *p1;
	struct bcache_buffer *p2;
	struct bcache_buffer *buf_stack[BCACHE_AVL_MAX_HEIGHT];
	struct bcache_buffer **buf_prev = buf_stack;

	bool modified = false;

	if (p == NULL) {
		*root = node;
		node->avl.left = NULL;
		node->avl.right = NULL;
		node->avl.bal = 0;
		return 0;
	}

	while (p != NULL) {
		*buf_prev++ = p;

		if (((uintptr_t)p->dd < (uintptr_t)dd) || ((p->dd == dd) && (p->block < block))) {
			p->avl.cache = 1;
			q = p->avl.right;
			if (q == NULL) {
				q = node;
				p->avl.right = q = node;
				break;
			}
		} else if ((p->dd != dd) || (p->block != block)) {
			p->avl.cache = -1;
			q = p->avl.left;
			if (q == NULL) {
				q = node;
				p->avl.left = q;
				break;
			}
		} else {
			return -1;
		}

		p = q;
	}

	q->avl.left = q->avl.right = NULL;
	q->avl.bal = 0;
	modified = true;
	buf_prev--;

	while (modified) {
		if (p->avl.cache == -1) {
			switch (p->avl.bal) {
			case 1:
				p->avl.bal = 0;
				modified = false;
				break;

			case 0:
				p->avl.bal = -1;
				break;

			case -1:
				p1 = p->avl.left;
				if (p1->avl.bal == -1) /* simple LL-turn */
				{
					p->avl.left = p1->avl.right;
					p1->avl.right = p;
					p->avl.bal = 0;
					p = p1;
				} else /* double LR-turn */
				{
					p2 = p1->avl.right;
					p1->avl.right = p2->avl.left;
					p2->avl.left = p1;
					p->avl.left = p2->avl.right;
					p2->avl.right = p;
					if (p2->avl.bal == -1)
						p->avl.bal = +1;
					else
						p->avl.bal = 0;
					if (p2->avl.bal == +1)
						p1->avl.bal = -1;
					else
						p1->avl.bal = 0;
					p = p2;
				}
				p->avl.bal = 0;
				modified = false;
				break;

			default:
				break;
			}
		} else {
			switch (p->avl.bal) {
			case -1:
				p->avl.bal = 0;
				modified = false;
				break;

			case 0:
				p->avl.bal = 1;
				break;

			case 1:
				p1 = p->avl.right;
				if (p1->avl.bal == 1) /* simple RR-turn */
				{
					p->avl.right = p1->avl.left;
					p1->avl.left = p;
					p->avl.bal = 0;
					p = p1;
				} else /* double RL-turn */
				{
					p2 = p1->avl.left;
					p1->avl.left = p2->avl.right;
					p2->avl.right = p1;
					p->avl.right = p2->avl.left;
					p2->avl.left = p;
					if (p2->avl.bal == +1)
						p->avl.bal = -1;
					else
						p->avl.bal = 0;
					if (p2->avl.bal == -1)
						p1->avl.bal = +1;
					else
						p1->avl.bal = 0;
					p = p2;
				}
				p->avl.bal = 0;
				modified = false;
				break;

			default:
				break;
			}
		}
		q = p;
		if (buf_prev > buf_stack) {
			p = *--buf_prev;

			if (p->avl.cache == -1) {
				p->avl.left = q;
			} else {
				p->avl.right = q;
			}
		} else {
			*root = p;
			break;
		}
	};

	return 0;
}

static int 
bcache_avl_remove(union bcache_tree *avl, const struct bcache_buffer *node) {
	struct bcache_buffer **root = &avl->tree;
	const struct bcache_device *dd = node->dd;
	bcache_num_t block = node->block;

	struct bcache_buffer *p = *root;
	struct bcache_buffer *q;
	struct bcache_buffer *r;
	struct bcache_buffer *s;
	struct bcache_buffer *p1;
	struct bcache_buffer *p2;
	struct bcache_buffer *buf_stack[BCACHE_AVL_MAX_HEIGHT];
	struct bcache_buffer **buf_prev = buf_stack;

	bool modified = false;

	memset(buf_stack, 0, sizeof(buf_stack));

	while (p != NULL) {
		*buf_prev++ = p;

		if (((uintptr_t)p->dd < (uintptr_t)dd) || ((p->dd == dd) && (p->block < block))) {
			p->avl.cache = 1;
			p = p->avl.right;
		} else if ((p->dd != dd) || (p->block != block)) {
			p->avl.cache = -1;
			p = p->avl.left;
		} else {
			/* node found */
			break;
		}
	}

	if (p == NULL) {
		/* there is no such node */
		return -1;
	}

	q = p;

	buf_prev--;
	if (buf_prev > buf_stack) {
		p = *(buf_prev - 1);
	} else {
		p = NULL;
	}

	/* at this moment q - is a node to delete, p is q's parent */
	if (q->avl.right == NULL) {
		r = q->avl.left;
		if (r != NULL) {
			r->avl.bal = 0;
		}
		q = r;
	} else {
		struct bcache_buffer **t;

		r = q->avl.right;

		if (r->avl.left == NULL) {
			r->avl.left = q->avl.left;
			r->avl.bal = q->avl.bal;
			r->avl.cache = 1;
			*buf_prev++ = q = r;
		} else {
			t = buf_prev++;
			s = r;

			while (s->avl.left != NULL) {
				*buf_prev++ = r = s;
				s = r->avl.left;
				r->avl.cache = -1;
			}

			s->avl.left = q->avl.left;
			r->avl.left = s->avl.right;
			s->avl.right = q->avl.right;
			s->avl.bal = q->avl.bal;
			s->avl.cache = 1;

			*t = q = s;
		}
	}

	if (p != NULL) {
		if (p->avl.cache == -1) {
			p->avl.left = q;
		} else {
			p->avl.right = q;
		}
	} else {
		*root = q;
	}

	modified = true;

	while (modified) {
		if (buf_prev > buf_stack) {
			p = *--buf_prev;
		} else {
			break;
		}

		if (p->avl.cache == -1) {
			/* rebalance left branch */
			switch (p->avl.bal) {
			case -1:
				p->avl.bal = 0;
				break;
			case 0:
				p->avl.bal = 1;
				modified = false;
				break;

			case +1:
				p1 = p->avl.right;

				if (p1->avl.bal >= 0) /* simple RR-turn */
				{
					p->avl.right = p1->avl.left;
					p1->avl.left = p;

					if (p1->avl.bal == 0) {
						p1->avl.bal = -1;
						modified = false;
					} else {
						p->avl.bal = 0;
						p1->avl.bal = 0;
					}
					p = p1;
				} else /* double RL-turn */
				{
					p2 = p1->avl.left;

					p1->avl.left = p2->avl.right;
					p2->avl.right = p1;
					p->avl.right = p2->avl.left;
					p2->avl.left = p;

					if (p2->avl.bal == +1)
						p->avl.bal = -1;
					else
						p->avl.bal = 0;
					if (p2->avl.bal == -1)
						p1->avl.bal = 1;
					else
						p1->avl.bal = 0;

					p = p2;
					p2->avl.bal = 0;
				}
				break;

			default:
				break;
			}
		} else {
			/* rebalance right branch */
			switch (p->avl.bal) {
			case +1:
				p->avl.bal = 0;
				break;

			case 0:
				p->avl.bal = -1;
				modified = false;
				break;

			case -1:
				p1 = p->avl.left;

				if (p1->avl.bal <= 0) /* simple LL-turn */
				{
					p->avl.left = p1->avl.right;
					p1->avl.right = p;
					if (p1->avl.bal == 0) {
						p1->avl.bal = 1;
						modified = false;
					} else {
						p->avl.bal = 0;
						p1->avl.bal = 0;
					}
					p = p1;
				} else /* double LR-turn */
				{
					p2 = p1->avl.right;

					p1->avl.right = p2->avl.left;
					p2->avl.left = p1;
					p->avl.left = p2->avl.right;
					p2->avl.right = p;

					if (p2->avl.bal == -1)
						p->avl.bal = 1;
					else
						p->avl.bal = 0;
					if (p2->avl.bal == +1)
						p1->avl.bal = -1;
					else
						p1->avl.bal = 0;

					p = p2;
					p2->avl.bal = 0;
				}
				break;

			default:
				break;
			}
		}

		if (buf_prev > buf_stack) {
			q = *(buf_prev - 1);

			if (q->avl.cache == -1) {
				q->avl.left = p;
			} else {
				q->avl.right = p;
			}
		} else {
			*root = p;
			break;
		}
	}

	return 0;
}

#endif /* CONFIG_BCACHE_HASH_MAP*/


static __rte_always_inline void 
bcache_set_state(struct bcache_buffer *bd, bcache_buf_state state) {
	bd->state = state;
}

#ifndef CONFIG_BCACHE_BLOCK_POWEROF2_MEDIA_SIZE
static inline bcache_num_t 
bcache_media_block(const struct bcache_device *dd, bcache_num_t block) {
	if (rte_likely(dd->block_to_media_block_shift >= 0))
		return block << dd->block_to_media_block_shift;
	else
		/*
		 * Change the block number for the block size to the block number for the media
		 * block size. We have to use 64bit maths. There is no short cut here.
		 */
		return (bcache_num_t)((((uint64_t)block) * dd->block_size) /
								   dd->media_block_size);
}

#else /* !CONFIG_BCACHE_BLOCK_POWEROF2_MEDIA_SIZE */
#define bcache_media_block(dd, block) \
	((block) << (dd)->block_to_media_block_shift)
#endif /* CONFIG_BCACHE_BLOCK_POWEROF2_MEDIA_SIZE */

/**
 * Wait until woken. Semaphores are used so a number of tasks can wait and can
 * be woken at once. Task events would require we maintain a list of tasks to
 * be woken and this would require storage and we do not know the number of
 * tasks that could be waiting.
 *
 * While we have the cache locked we can try and claim the semaphore and
 * therefore know when we release the lock to the cache we will block until the
 * semaphore is released. This may even happen before we get to block.
 *
 * A counter is used to save the release call when no one is waiting.
 *
 * The function assumes the cache is locked on entry and it will be locked on
 * exit.
 */
static void 
bcache_anonymous_wait(struct bcache_waiters *waiters) {
	/*
	 * Indicate we are waiting.
	 */
	++waiters->count;
	os_cv_wait(&waiters->cond_var, &bdbuf_cache.lock);
	--waiters->count;
}

static void 
bcache_wait(struct bcache_buffer *bd, struct bcache_waiters *waiters) {
	bcache_group_obtain(bd);
	++bd->waiters;
	bcache_anonymous_wait(waiters);
	--bd->waiters;
	bcache_group_release(bd);
}

/**
 * Wake a blocked resource. The resource has a counter that lets us know if
 * there are any waiters.
 */
static inline void 
bcache_wake(struct bcache_waiters *waiters) {
	if (waiters->count > 0) 
		os_cv_broadcast(&waiters->cond_var);
}

static __rte_always_inline void 
bcache_wake_swapper(void) {
	os_completed(&bdbuf_cache.swapout_signal);
}

static __rte_always_inline bool 
bcache_has_buffer_waiters(void) {
	return bdbuf_cache.buffer_waiters.count;
}

static void 
bcache_remove_from_tree(struct bcache_buffer *bd) {
	if (bcache_container_remove(&bdbuf_cache.root, bd) != 0)
		bcache_fatal_with_state(bd->state, BCACHE_FATAL_TREE_RM);
}

static void 
bcache_remove_from_tree_and_lru_list(struct bcache_buffer *bd) {
	switch (bd->state) {
	case BCACHE_STATE_FREE:
		break;
	case BCACHE_STATE_CACHED:
		bcache_remove_from_tree(bd);
		break;
	default:
		bcache_fatal_with_state(bd->state, BCACHE_FATAL_STATE_10);
	}
	rte_list_del(&bd->link);
}

static void 
bcache_make_free_and_add_to_lru_list(struct bcache_buffer *bd) {
	bcache_set_state(bd, BCACHE_STATE_FREE);
	rte_list_add(&bd->link, &bdbuf_cache.lru);
}

static inline void 
bcache_make_empty(struct bcache_buffer *bd) {
	bcache_set_state(bd, BCACHE_STATE_EMPTY);
}

static void 
bcache_make_cached_and_add_to_lru_list(struct bcache_buffer *bd) {
	bcache_set_state(bd, BCACHE_STATE_CACHED);
	rte_list_add_tail(&bd->link, &bdbuf_cache.lru);
}

static void bcache_discard_buffer(struct bcache_buffer *bd) {
	bcache_make_empty(bd);
	if (bd->waiters == 0) {
		bcache_remove_from_tree(bd);
		bcache_make_free_and_add_to_lru_list(bd);
	}
}

static void 
bcache_add_to_modified_list_after_access(struct bcache_buffer *bd) {
	if (bdbuf_cache.sync_active && bdbuf_cache.sync_device == bd->dd) {
		bcache_unlock_cache();

		/*
		 * Wait for the sync lock.
		 */
		bcache_lock_sync();
		bcache_unlock_sync();
		bcache_lock_cache();
	}

	/*
	 * Only the first modified release sets the timer and any further user
	 * accesses do not change the timer value which should move down. This
	 * assumes the user's hold of the buffer is much less than the time on the
	 * modified list. Resetting the timer on each access which could result in a
	 * buffer never getting to 0 and never being forced onto disk. This raises a
	 * difficult question. Is a snapshot of a block that is changing better than
	 * nothing being written? We have tended to think we should hold changes for
	 * only a specific period of time even if still changing and get onto disk
	 * and letting the file system try and recover this position if it can.
	 */
	if (bd->state == BCACHE_STATE_ACCESS_CACHED ||
		bd->state == BCACHE_STATE_ACCESS_EMPTY)
		bd->hold_timer = bdbuf_config.swap_block_hold;

	bcache_set_state(bd, BCACHE_STATE_MODIFIED);
	rte_list_add_tail(&bd->link, &bdbuf_cache.modified);

	if (bd->waiters)
		bcache_wake(&bdbuf_cache.access_waiters);
	else if (bcache_has_buffer_waiters())
		bcache_wake_swapper();
}

static void 
bcache_add_to_lru_list_after_access(struct bcache_buffer *bd) {
	bcache_group_release(bd);
	bcache_make_cached_and_add_to_lru_list(bd);
	if (bd->waiters)
		bcache_wake(&bdbuf_cache.access_waiters);
	else
		bcache_wake(&bdbuf_cache.buffer_waiters);
}

/**
 * Compute the number of BDs per group for a given buffer size.
 *
 * @param size The buffer size. It can be any size and we scale up.
 */
static size_t bcache_bds_per_group(size_t size) {
	size_t bufs_per_size;
	size_t bds_per_size;

	if (size > bdbuf_config.buffer_max)
		return 0;

	bufs_per_size = ((size - 1) / bdbuf_config.buffer_min) + 1;
	for (bds_per_size = 1; 
		bds_per_size < bufs_per_size; 
		bds_per_size <<= 1)
		;

	return bdbuf_cache.max_bds_per_group / bds_per_size;
}

static void bcache_discard_buffer_after_access(struct bcache_buffer *bd) {
	bcache_group_release(bd);
	bcache_discard_buffer(bd);

	if (bd->waiters)
		bcache_wake(&bdbuf_cache.access_waiters);
	else
		bcache_wake(&bdbuf_cache.buffer_waiters);
}

/**
 * Reallocate a group. The BDs currently allocated in the group are removed
 * from the ALV tree and any lists then the new BD's are prepended to the ready
 * list of the cache.
 *
 * @param group The group to reallocate.
 * @param new_bds_per_group The new count of BDs per group.
 * @return A buffer of this group.
 */
static struct bcache_buffer *
bcache_group_realloc(struct bcache_group *group, size_t new_bds_per_group) {
	struct bcache_buffer *bd;
	size_t bufs_per_bd;
	size_t b;

	pr_dbg("bdbuf:realloc: %tu: %zd -> %zd\n", group - bdbuf_cache.groups,
			group->bds_per_group, new_bds_per_group);

	bufs_per_bd = bdbuf_cache.max_bds_per_group / group->bds_per_group;
	for (b = 0, bd = group->bdbuf; b < group->bds_per_group; b++, bd += bufs_per_bd)
		bcache_remove_from_tree_and_lru_list(bd);

	group->bds_per_group = new_bds_per_group;
	bufs_per_bd = bdbuf_cache.max_bds_per_group / new_bds_per_group;
	for (b = 1, bd = group->bdbuf + bufs_per_bd; b < group->bds_per_group;
		 b++, bd += bufs_per_bd)
		bcache_make_free_and_add_to_lru_list(bd);

	if (b > 1)
		bcache_wake(&bdbuf_cache.buffer_waiters);

	return group->bdbuf;
}

static void 
bcache_setup_empty_buffer(struct bcache_buffer *bd, struct bcache_device *dd, 
	bcache_num_t block) {
	bd->dd = dd;
	bd->block = block;
	bd->avl.left = NULL;
	bd->avl.right = NULL;
	bd->waiters = 0;

	if (bcache_container_insert(&bdbuf_cache.root, bd) != 0)
		bcache_fatal(BCACHE_FATAL_RECYCLE);

	bcache_make_empty(bd);
}

static struct bcache_buffer *
bcache_get_buffer_from_lru_list(struct bcache_device *dd, bcache_num_t block) {
	struct rte_list *pos, *next;

	rte_list_foreach_safe(pos, next, &bdbuf_cache.lru) {
		struct bcache_buffer *bd = rte_container_of(pos, struct bcache_buffer, link);
		struct bcache_buffer *empty_bd = NULL;

		pr_dbg("bdbuf:next-bd: %tu (%td:%" PRId32 ") %zd -> %zd\n",
				bd - bdbuf_cache.bds, bd->group - bdbuf_cache.groups, bd->group->users,
				bd->group->bds_per_group, dd->bds_per_group);

		/*
		 * If nobody waits for this BD, we may recycle it.
		 */
		if (bd->waiters == 0) {
			if (bd->group->bds_per_group == dd->bds_per_group) {
				bcache_remove_from_tree_and_lru_list(bd);
				empty_bd = bd;
			} else if (bd->group->users == 0)
				empty_bd = bcache_group_realloc(bd->group, dd->bds_per_group);
		}

		if (empty_bd != NULL) {
			bcache_setup_empty_buffer(empty_bd, dd, block);
			return empty_bd;
		}
	}

	return NULL;
}

static struct bcache_swapout_transfer *
bcache_swapout_transfer_alloc(void) {
	/*
	 * @note chrisj The struct bcache_request and the array at the end is a hack.
	 * I am disappointment at finding code like this in RTEMS. The request should
	 * have been a rtems_chain_control. Simple, fast and less storage as the node
	 * is already part of the buffer structure.
	 */
	size_t transfer_size =
		sizeof(struct bcache_swapout_transfer) +
		(bdbuf_config.max_write_blocks * sizeof(struct bcache_sg_buffer));
	return general_calloc(1, transfer_size);
}

static void 
bcache_swapout_transfer_init(struct bcache_swapout_transfer *transfer,
	os_completion_t *completion) {
	RTE_INIT_LIST(&transfer->bds);
	transfer->dd = BDBUF_INVALID_DEV;
	transfer->syncing = false;
	transfer->write_req.req = BCACHE_DEV_REQ_WRITE;
	transfer->write_req.done = bcache_transfer_done;
	transfer->write_req.io_task = completion;
}

static size_t bcache_swapout_worker_size(void) {
	return sizeof(struct bcache_swapout_worker) +
		   (bdbuf_config.max_write_blocks * sizeof(struct bcache_sg_buffer));
}

static void bcache_swapout_worker_task(void * arg);

static int bcache_swapout_workers_create(void) {
	int sc = 0;
	size_t w;
	size_t worker_size;
	char *worker_current;

	worker_size = bcache_swapout_worker_size();
	worker_current = general_calloc(1, bdbuf_config.swapout_workers * worker_size);
	if (worker_current == NULL)
		return -ENOMEM;

	bdbuf_cache.swapout_workers = (struct bcache_swapout_worker *)worker_current;

	for (w = 0; sc == 0 && w < bdbuf_config.swapout_workers;
		 w++, worker_current += worker_size) {
		struct bcache_swapout_worker *worker = (struct bcache_swapout_worker *)worker_current;
		os_completion_reinit(&worker->swapout_sync);
		bcache_swapout_transfer_init(&worker->transfer, &worker->swapout_sync);
		rte_list_add_tail(&worker->link, &bdbuf_cache.swapout_free_workers);
		worker->enabled = true;
		sc = bcache_create_task("swap-worker", bdbuf_config.swapout_worker_priority,
								bcache_swapout_worker_task,
								(void *)worker,
								&worker->thread);
	}

	return sc;
}

static size_t 
bcache_read_request_size(uint32_t transfer_count) {
	return sizeof(struct bcache_request) + 
		sizeof(struct bcache_sg_buffer) * transfer_count;
}

static int bcache_do_init(void) {
	struct bcache_group *group;
	struct bcache_buffer *bd;
	uint8_t *buffer;
	size_t b;
	int sc = -ENOMEM;

	pr_dbg("bdbuf:init\n");

	/*
	 * Check the configuration table values.
	 */
	if ((bdbuf_config.buffer_max % bdbuf_config.buffer_min) != 0)
		return -EINVAL;

	if (bcache_read_request_size(bdbuf_config.max_read_ahead_blocks) >
		BCACHE_MINIMUM_STACK_SIZE / 8U)
		return -EINVAL;

	bdbuf_cache.sync_device = NULL;
	RTE_INIT_LIST(&bdbuf_cache.swapout_free_workers);
	RTE_INIT_LIST(&bdbuf_cache.lru);
	RTE_INIT_LIST(&bdbuf_cache.modified);
	RTE_INIT_LIST(&bdbuf_cache.sync);
#ifdef CONFIG_BCACHE_READ_AHEAD
	RTE_INIT_LIST(&bdbuf_cache.read_ahead_chain);
#endif
	RTE_INIT_LIST(&bdbuf_cache.dev_nodes);

	os_mtx_init(&bdbuf_cache.lock, 0);
	os_mtx_init(&bdbuf_cache.sync_lock, 0);
	os_cv_init(&bdbuf_cache.access_waiters.cond_var, NULL);
	os_cv_init(&bdbuf_cache.transfer_waiters.cond_var, NULL);
	os_cv_init(&bdbuf_cache.buffer_waiters.cond_var, NULL);

	bcache_lock_cache();

	/*
	 * Compute the various number of elements in the cache.
	 */
	bdbuf_cache.buffer_min_count = bdbuf_config.size / bdbuf_config.buffer_min;
	bdbuf_cache.max_bds_per_group = bdbuf_config.buffer_max / bdbuf_config.buffer_min;
	bdbuf_cache.group_count =
		bdbuf_cache.buffer_min_count / bdbuf_cache.max_bds_per_group;

	/*
	 * Allocate the memory for the buffer descriptors.
	 */
	bdbuf_cache.bds = general_calloc(sizeof(struct bcache_buffer), 
		bdbuf_cache.buffer_min_count);
	if (!bdbuf_cache.bds)
		goto error;

	/*
	 * Allocate the memory for the buffer descriptors.
	 */
	bdbuf_cache.groups = general_calloc(sizeof(struct bcache_group), 
		bdbuf_cache.group_count);
	if (!bdbuf_cache.groups)
		goto error;

	/*
	 * Allocate memory for buffer memory. The buffer memory will be cache
	 * aligned. It is possible to general_free the memory allocated by
	 * rtems_cache_aligned_malloc() with general_free().
	 */
	bdbuf_cache.buffers = general_aligned_alloc(
		RTE_CACHE_LINE_SIZE, 
		bdbuf_cache.buffer_min_count * bdbuf_config.buffer_min);
	if (bdbuf_cache.buffers == NULL)
		goto error;

	/*
	 * The cache is empty after opening so we need to add all the buffers to it
	 * and initialise the groups.
	 */
	for (b = 0, group = bdbuf_cache.groups, bd = bdbuf_cache.bds,
		buffer = bdbuf_cache.buffers;
		 b < bdbuf_cache.buffer_min_count; b++, bd++, buffer += bdbuf_config.buffer_min) {
		bd->dd = BDBUF_INVALID_DEV;
		bd->group = group;
		bd->buffer = buffer;

		rte_list_add_tail(&bd->link, &bdbuf_cache.lru);
		if ((b % bdbuf_cache.max_bds_per_group) == (bdbuf_cache.max_bds_per_group - 1))
			group++;
	}

	for (b = 0, group = bdbuf_cache.groups, bd = bdbuf_cache.bds;
		 b < bdbuf_cache.group_count; b++, group++, bd += bdbuf_cache.max_bds_per_group) {
		group->bds_per_group = bdbuf_cache.max_bds_per_group;
		group->bdbuf = bd;
	}

	/*
	 * Create and start swapout task.
	 */
	bdbuf_cache.swapout_transfer = bcache_swapout_transfer_alloc();
	if (!bdbuf_cache.swapout_transfer)
		goto error;

	os_completion_reinit(&bdbuf_cache.swapout_signal);
	bcache_swapout_transfer_init(bdbuf_cache.swapout_transfer, 
		&bdbuf_cache.swapout_signal);
	bdbuf_cache.swapout_enabled = true;
	sc = bcache_create_task("swap-thread", bdbuf_config.swapout_priority,
							bcache_swapout_task,
							(void *)bdbuf_cache.swapout_transfer,
							&bdbuf_cache.swapout);
	if (sc != 0)
		goto error;

	if (bdbuf_config.swapout_workers > 0) {
		sc = bcache_swapout_workers_create();
		if (sc != 0)
			goto error;
	}

#ifdef CONFIG_BCACHE_READ_AHEAD
	if (bdbuf_config.max_read_ahead_blocks > 0) {
		bdbuf_cache.read_ahead_enabled = true;
		os_completion_reinit(&bdbuf_cache.read_ahead);
		sc = bcache_create_task("read-ahead", bdbuf_config.read_ahead_priority,
								bcache_read_ahead_task, NULL,
								&bdbuf_cache.read_ahead_task);
		if (sc != 0)
			goto error;
	}
#endif /* CONFIG_BCACHE_READ_AHEAD */

	bcache_unlock_cache();

	return 0;

error:
#ifdef CONFIG_BCACHE_READ_AHEAD
	bcache_delete_task(bdbuf_cache.read_ahead_task);
#endif

	bcache_delete_task(bdbuf_cache.swapout);

	if (bdbuf_cache.swapout_workers) {
		char *worker_current = (char *)bdbuf_cache.swapout_workers;
		size_t worker_size = bcache_swapout_worker_size();
		size_t w;

		for (w = 0; w < bdbuf_config.swapout_workers;
			 w++, worker_current += worker_size) {
			struct bcache_swapout_worker *worker =
				(struct bcache_swapout_worker *)worker_current;
			bcache_delete_task(worker->thread);
		}
	}

	general_free(bdbuf_cache.buffers);
	general_free(bdbuf_cache.groups);
	general_free(bdbuf_cache.bds);
	general_free(bdbuf_cache.swapout_transfer);
	general_free(bdbuf_cache.swapout_workers);
	bcache_unlock_cache();

	return sc;
}

int bcache_init(void) {
	static bool ready;

	if (!ready) {
		ready = true;
		return bcache_do_init();
	}
	return 0;
}

static void 
bcache_wait_for_access(struct bcache_buffer *bd) {
	while (true) {
		switch (bd->state) {
		case BCACHE_STATE_MODIFIED:
			bcache_group_release(bd);
			/* Fall through */
		case BCACHE_STATE_CACHED:
			rte_list_del(&bd->link);
			/* Fall through */
		case BCACHE_STATE_EMPTY:
			return;
		case BCACHE_STATE_ACCESS_CACHED:
		case BCACHE_STATE_ACCESS_EMPTY:
		case BCACHE_STATE_ACCESS_MODIFIED:
		case BCACHE_STATE_ACCESS_PURGED:
			bcache_wait(bd, &bdbuf_cache.access_waiters);
			break;
		case BCACHE_STATE_SYNC:
		case BCACHE_STATE_TRANSFER:
		case BCACHE_STATE_TRANSFER_PURGED:
			bcache_wait(bd, &bdbuf_cache.transfer_waiters);
			break;
		default:
			bcache_fatal_with_state(bd->state, BCACHE_FATAL_STATE_7);
		}
	}
}

static void 
bcache_request_sync_for_modified_buffer(struct bcache_buffer *bd) {
	bcache_set_state(bd, BCACHE_STATE_SYNC);
	rte_list_del(&bd->link);
	rte_list_add_tail(&bd->link, &bdbuf_cache.sync);
	bcache_wake_swapper();
}

/**
 * @brief Waits until the buffer is ready for recycling.
 *
 * @retval @c true Buffer is valid and may be recycled.
 * @retval @c false Buffer is invalid and has to searched again.
 */
static bool 
bcache_wait_for_recycle(struct bcache_buffer *bd) {
	while (true) {
		switch (bd->state) {
		case BCACHE_STATE_FREE:
			return true;
		case BCACHE_STATE_MODIFIED:
			bcache_request_sync_for_modified_buffer(bd);
			break;
		case BCACHE_STATE_CACHED:
		case BCACHE_STATE_EMPTY:
			if (bd->waiters == 0)
				return true;
			else {
				/*
				 * It is essential that we wait here without a special wait count and
				 * without the group in use.  Otherwise we could trigger a wait ping
				 * pong with another recycle waiter.  The state of the buffer is
				 * arbitrary afterwards.
				 */
				bcache_anonymous_wait(&bdbuf_cache.buffer_waiters);
				return false;
			}
		case BCACHE_STATE_ACCESS_CACHED:
		case BCACHE_STATE_ACCESS_EMPTY:
		case BCACHE_STATE_ACCESS_MODIFIED:
		case BCACHE_STATE_ACCESS_PURGED:
			bcache_wait(bd, &bdbuf_cache.access_waiters);
			break;
		case BCACHE_STATE_SYNC:
		case BCACHE_STATE_TRANSFER:
		case BCACHE_STATE_TRANSFER_PURGED:
			bcache_wait(bd, &bdbuf_cache.transfer_waiters);
			break;
		default:
			bcache_fatal_with_state(bd->state, BCACHE_FATAL_STATE_8);
		}
	}
}

static void 
bcache_wait_for_sync_done(struct bcache_buffer *bd) {
	while (true) {
		switch (bd->state) {
		case BCACHE_STATE_CACHED:
		case BCACHE_STATE_EMPTY:
		case BCACHE_STATE_MODIFIED:
		case BCACHE_STATE_ACCESS_CACHED:
		case BCACHE_STATE_ACCESS_EMPTY:
		case BCACHE_STATE_ACCESS_MODIFIED:
		case BCACHE_STATE_ACCESS_PURGED:
			return;
		case BCACHE_STATE_SYNC:
		case BCACHE_STATE_TRANSFER:
		case BCACHE_STATE_TRANSFER_PURGED:
			bcache_wait(bd, &bdbuf_cache.transfer_waiters);
			break;
		default:
			bcache_fatal_with_state(bd->state, BCACHE_FATAL_STATE_9);
		}
	}
}

static void 
bcache_wait_for_buffer(void) {
	if (!rte_list_empty(&bdbuf_cache.modified))
		bcache_wake_swapper();

	bcache_anonymous_wait(&bdbuf_cache.buffer_waiters);
}

static void 
bcache_sync_after_access(struct bcache_buffer *bd) {
	bcache_set_state(bd, BCACHE_STATE_SYNC);
	rte_list_add_tail(&bd->link, &bdbuf_cache.sync);
	if (bd->waiters)
		bcache_wake(&bdbuf_cache.access_waiters);

	bcache_wake_swapper();
	bcache_wait_for_sync_done(bd);

	/*
	 * We may have created a cached or empty buffer which may be recycled.
	 */
	if (bd->waiters == 0 &&
		(bd->state == BCACHE_STATE_CACHED || bd->state == BCACHE_STATE_EMPTY)) {
		if (bd->state == BCACHE_STATE_EMPTY) {
			bcache_remove_from_tree(bd);
			bcache_make_free_and_add_to_lru_list(bd);
		}
		bcache_wake(&bdbuf_cache.buffer_waiters);
	}
}

static struct bcache_buffer *
bcache_get_buffer_for_read_ahead(struct bcache_device *dd, bcache_num_t block) {
	struct bcache_buffer *bd = NULL;

	bd = bcache_container_search(&bdbuf_cache.root, dd, block);
	if (bd == NULL) {
		bd = bcache_get_buffer_from_lru_list(dd, block);
		if (bd != NULL)
			bcache_group_obtain(bd);
	} else
		/*
		 * The buffer is in the cache.  So it is already available or in use, and
		 * thus no need for a read ahead.
		 */
		bd = NULL;

	return bd;
}

static struct bcache_buffer *
bcache_get_buffer_for_access(struct bcache_device *dd, bcache_num_t block) {
	struct bcache_buffer *bd = NULL;

	do {
		bd = bcache_container_search(&bdbuf_cache.root, dd, block);
		if (bd != NULL) {
			if (bd->group->bds_per_group != dd->bds_per_group) {
				if (bcache_wait_for_recycle(bd)) {
					bcache_remove_from_tree_and_lru_list(bd);
					bcache_make_free_and_add_to_lru_list(bd);
					bcache_wake(&bdbuf_cache.buffer_waiters);
				}
				bd = NULL;
			}
		} else {
			bd = bcache_get_buffer_from_lru_list(dd, block);
			if (bd == NULL)
				bcache_wait_for_buffer();
		}
	} while (bd == NULL);

	bcache_wait_for_access(bd);
	bcache_group_obtain(bd);
	return bd;
}

static inline int 
bcache_get_media_block(const struct bcache_device *dd, bcache_num_t block,
	bcache_num_t *media_block_ptr) {
	int sc = 0;

	if (rte_likely(block < dd->block_count)) {
		/*
		 * Compute the media block number. Drivers work with media block number not
		 * the block number a BD may have as this depends on the block size set by
		 * the user.
		 */
		*media_block_ptr = bcache_media_block(dd, block) + dd->start;
	} else {
		sc = -EINVAL;
	}

	return sc;
}

int 
bcache_get(struct bcache_device *dd, bcache_num_t block,
	struct bcache_buffer **bd_ptr) {
	int sc = 0;
	struct bcache_buffer *bd = NULL;
	bcache_num_t media_block;

	bcache_lock_cache();

	sc = bcache_get_media_block(dd, block, &media_block);
	if (rte_likely(sc == 0)) {
		/*
		 * Print the block index relative to the physical disk.
		 */
		pr_dbg("bdbuf:get: %" PRIu32 " (%" PRIu32 ") (dev = %08x)\n", media_block,
				block, (unsigned)dd->dev);

		bd = bcache_get_buffer_for_access(dd, media_block);

		switch (bd->state) {
		case BCACHE_STATE_CACHED:
			bcache_set_state(bd, BCACHE_STATE_ACCESS_CACHED);
			break;
		case BCACHE_STATE_EMPTY:
			bcache_set_state(bd, BCACHE_STATE_ACCESS_EMPTY);
			break;
		case BCACHE_STATE_MODIFIED:
			/*
			 * To get a modified buffer could be considered a bug in the caller
			 * because you should not be getting an already modified buffer but
			 * user may have modified a byte in a block then decided to seek the
			 * start and write the whole block and the file system will have no
			 * record of this so just gets the block to fill.
			 */
			bcache_set_state(bd, BCACHE_STATE_ACCESS_MODIFIED);
			break;
		default:
			bcache_fatal_with_state(bd->state, BCACHE_FATAL_STATE_2);
			break;
		}

#if (BCACHE_TRACE)
		bcache_show_users("get", bd);
		bcache_show_usage();
#endif
	}

	bcache_unlock_cache();

	*bd_ptr = bd;

	return sc;
}

/**
 * Call back handler called by the low level driver when the transfer has
 * completed. This function may be invoked from interrupt handler.
 *
 * @param arg Arbitrary argument specified in block device request
 *            structure (in this case - pointer to the appropriate
 *            block device request structure).
 * @param status I/O completion status
 */
static void 
bcache_transfer_done(struct bcache_request *req, int status) {
	req->status = status;
	os_completed(req->io_task);
}

static int 
bcache_execute_transfer_request(struct bcache_device *dd, 
	struct bcache_request *req, bool cache_locked) {
	int sc = 0;
	uint32_t transfer_index = 0;
	bool wake_transfer_waiters = false;
	bool wake_buffer_waiters = false;

	if (cache_locked)
		bcache_unlock_cache();

	/* The return value will be ignored for transfer requests */
	dd->ioctl(dd->phys_dev, BCACHE_IO_REQUEST, req);

	/* Wait for transfer request completion */
	os_completion_wait(req->io_task);
	sc = req->status;

	bcache_lock_cache();

	/* Statistics */
	/* Statistics */
	if (req->req == BCACHE_DEV_REQ_READ) {
		dd->stats.read_blocks += req->bufnum;
		if (sc != 0)
			++dd->stats.read_errors;
	} else {
		dd->stats.write_blocks += req->bufnum;
		++dd->stats.write_transfers;
		if (sc != 0)
			++dd->stats.write_errors;
	}

	for (transfer_index = 0; transfer_index < req->bufnum; ++transfer_index) {
		struct bcache_buffer *bd = req->bufs[transfer_index].user;
		bool waiters = bd->waiters;

		if (waiters)
			wake_transfer_waiters = true;
		else
			wake_buffer_waiters = true;

		bcache_group_release(bd);

		if (sc == 0 && bd->state == BCACHE_STATE_TRANSFER)
			bcache_make_cached_and_add_to_lru_list(bd);
		else
			bcache_discard_buffer(bd);

#if (BCACHE_TRACE)
		bcache_show_users("transfer", bd);
#endif
	}

	if (wake_transfer_waiters)
		bcache_wake(&bdbuf_cache.transfer_waiters);

	if (wake_buffer_waiters)
		bcache_wake(&bdbuf_cache.buffer_waiters);

	if (!cache_locked)
		bcache_unlock_cache();

	return sc;
}

static int 
bcache_execute_read_request(struct bcache_device *dd,
	struct bcache_buffer *bd, uint32_t transfer_count) {
	struct bcache_request *req = NULL;
	bcache_num_t media_block = bd->block;
	uint32_t media_blocks_per_block = dd->media_blocks_per_block;
	uint32_t block_size = dd->block_size;
	uint32_t transfer_index = 1;
	os_completion_t done;

	/*
	 * TODO: This type of request structure is wrong and should be removed.
	 */
#if defined(__GNUC__) || defined(__clang__)
#define bdbuf_alloc(size) __builtin_alloca(size)
#else
	char buffer[4096];
#define bdbuf_alloc(size) (void *)buffer
	rte_assert(bcache_read_request_size(transfer_count) <= sizeof(buffer));
#endif

	os_completion_reinit(&done);
	req = bdbuf_alloc(bcache_read_request_size(transfer_count));
	req->req = BCACHE_DEV_REQ_READ;
	req->done = bcache_transfer_done;
	req->io_task = &done;
	req->bufnum = 0;

	bcache_set_state(bd, BCACHE_STATE_TRANSFER);

	req->bufs[0].user = bd;
	req->bufs[0].block = media_block;
	req->bufs[0].length = block_size;
	req->bufs[0].buffer = bd->buffer;

#if (BCACHE_TRACE)
	bcache_show_users("read", bd);
#endif

	while (transfer_index < transfer_count) {
		media_block += media_blocks_per_block;

		bd = bcache_get_buffer_for_read_ahead(dd, media_block);
		if (bd == NULL)
			break;

		bcache_set_state(bd, BCACHE_STATE_TRANSFER);
		req->bufs[transfer_index].user = bd;
		req->bufs[transfer_index].block = media_block;
		req->bufs[transfer_index].length = block_size;
		req->bufs[transfer_index].buffer = bd->buffer;

#if (BCACHE_TRACE)
		bcache_show_users("read", bd);
#endif
		++transfer_index;
	}

	req->bufnum = transfer_index;
	return bcache_execute_transfer_request(dd, req, true);
}

#ifdef CONFIG_BCACHE_READ_AHEAD
static inline bool 
bcache_is_read_ahead_active(const struct bcache_device *dd) {
	return dd->read_ahead.node.next != NULL;
}

static void 
bcache_read_ahead_cancel(struct bcache_device *dd) {
	if (bcache_is_read_ahead_active(dd))
		rte_list_del(&dd->read_ahead.node);
}

static void 
bcache_read_ahead_reset(struct bcache_device *dd) {
	bcache_read_ahead_cancel(dd);
	dd->read_ahead.trigger = BCACHE_READ_AHEAD_NO_TRIGGER;
}

static void 
bcache_read_ahead_add_to_chain(struct bcache_device *dd) {
	struct rte_list *list = &bdbuf_cache.read_ahead_chain;
	if (rte_list_empty(list))
		os_completed(&bdbuf_cache.read_ahead);
	rte_list_add_tail(&dd->read_ahead.node, list);
}

static void 
bcache_check_read_ahead_trigger(struct bcache_device *dd, bcache_num_t block) {
	if (bdbuf_cache.read_ahead_task != 0 && 
		dd->read_ahead.trigger == block &&
		!bcache_is_read_ahead_active(dd)) {
		dd->read_ahead.nr_blocks = BCACHE_READ_AHEAD_SIZE_AUTO;
		bcache_read_ahead_add_to_chain(dd);
	}
}

static void 
bcache_set_read_ahead_trigger(struct bcache_device *dd, bcache_num_t block) {
	if (dd->read_ahead.trigger != block) {
		bcache_read_ahead_cancel(dd);
		dd->read_ahead.trigger = block + 1;
		dd->read_ahead.next = block + 2;
	}
}
#endif /* CONFIG_BCACHE_READ_AHEAD */

int 
bcache_read(struct bcache_device *dd, bcache_num_t block,
	struct bcache_buffer **bd_ptr) {
	int sc = 0;
	struct bcache_buffer *bd = NULL;
	bcache_num_t media_block;

	bcache_lock_cache();

	sc = bcache_get_media_block(dd, block, &media_block);
	if (sc == 0) {
		pr_dbg("bdbuf:read: %" PRIu32 " (%" PRIu32 ") (dev = %08x)\n", media_block,
				block, (unsigned)dd->dev);

		bd = bcache_get_buffer_for_access(dd, media_block);
		
		switch (bd->state) {
		case BCACHE_STATE_CACHED:
			++dd->stats.read_hits;
			bcache_set_state(bd, BCACHE_STATE_ACCESS_CACHED);
			break;
		case BCACHE_STATE_MODIFIED:
			++dd->stats.read_hits;
			bcache_set_state(bd, BCACHE_STATE_ACCESS_MODIFIED);
			break;
		case BCACHE_STATE_EMPTY:
			++dd->stats.read_misses;
#ifdef CONFIG_BCACHE_READ_AHEAD
			bcache_set_read_ahead_trigger(dd, block);
#endif
			sc = bcache_execute_read_request(dd, bd, 1);
			if (sc == 0) {
				bcache_set_state(bd, BCACHE_STATE_ACCESS_CACHED);
				rte_list_del(&bd->link);
				bcache_group_obtain(bd);
			} else {
				bd = NULL;
			}
			break;
		default:
			bcache_fatal_with_state(bd->state, BCACHE_FATAL_STATE_4);
			break;
		}
#ifdef CONFIG_BCACHE_READ_AHEAD
		bcache_check_read_ahead_trigger(dd, block);
#endif
	}

	bcache_unlock_cache();
	*bd_ptr = bd;
	return sc;
}

void 
bcache_peek(struct bcache_device *dd, bcache_num_t block,
	uint32_t nr_blocks) {
#ifdef CONFIG_BCACHE_READ_AHEAD
	bcache_lock_cache();
	if (bdbuf_cache.read_ahead_enabled && nr_blocks > 0) {
		bcache_read_ahead_reset(dd);
		dd->read_ahead.next = block;
		dd->read_ahead.nr_blocks = nr_blocks;
		bcache_read_ahead_add_to_chain(dd);
	}
	bcache_unlock_cache();
#endif /* CONFIG_BCACHE_READ_AHEAD */
}

static int 
bcache_check_bd_and_lock_cache(struct bcache_buffer *bd, const char *kind) {
	if (bd == NULL)
		return -EINVAL;
#if (BCACHE_TRACE)
	pr_dbg("bdbuf:%s: %" PRIu32 "\n", kind, bd->block);
	bcache_show_users(kind, bd);
#endif
	bcache_lock_cache();
	return 0;
}

int 
bcache_release(struct bcache_buffer *bd) {
	int sc = 0;

	sc = bcache_check_bd_and_lock_cache(bd, "release");
	if (sc != 0)
		return sc;

	switch (bd->state) {
	case BCACHE_STATE_ACCESS_CACHED:
		bcache_add_to_lru_list_after_access(bd);
		break;
	case BCACHE_STATE_ACCESS_EMPTY:
	case BCACHE_STATE_ACCESS_PURGED:
		bcache_discard_buffer_after_access(bd);
		break;
	case BCACHE_STATE_ACCESS_MODIFIED:
		bcache_add_to_modified_list_after_access(bd);
		break;
	default:
		bcache_fatal_with_state(bd->state, BCACHE_FATAL_STATE_0);
		break;
	}

#if (BCACHE_TRACE)
	bcache_show_usage();
#endif
	bcache_unlock_cache();
	return 0;
}

int 
bcache_release_modified(struct bcache_buffer *bd) {
	int sc = 0;

	sc = bcache_check_bd_and_lock_cache(bd, "release modified");
	if (sc != 0)
		return sc;

	switch (bd->state) {
	case BCACHE_STATE_ACCESS_CACHED:
	case BCACHE_STATE_ACCESS_EMPTY:
	case BCACHE_STATE_ACCESS_MODIFIED:
		bcache_add_to_modified_list_after_access(bd);
		break;
	case BCACHE_STATE_ACCESS_PURGED:
		bcache_discard_buffer_after_access(bd);
		break;
	default:
		bcache_fatal_with_state(bd->state, BCACHE_FATAL_STATE_6);
		break;
	}

#if (BCACHE_TRACE)
	bcache_show_usage();
#endif
	bcache_unlock_cache();
	return 0;
}

int 
bcache_sync(struct bcache_buffer *bd) {
	int sc = 0;

	sc = bcache_check_bd_and_lock_cache(bd, "sync");
	if (sc != 0)
		return sc;

	switch (bd->state) {
	case BCACHE_STATE_ACCESS_CACHED:
	case BCACHE_STATE_ACCESS_EMPTY:
	case BCACHE_STATE_ACCESS_MODIFIED:
		bcache_sync_after_access(bd);
		break;
	case BCACHE_STATE_ACCESS_PURGED:
		bcache_discard_buffer_after_access(bd);
		break;
	default:
		bcache_fatal_with_state(bd->state, BCACHE_FATAL_STATE_5);
		break;
	}

#if (BCACHE_TRACE)
	bcache_show_usage();
#endif
	bcache_unlock_cache();
	return 0;
}

int 
bcache_syncdev(struct bcache_device *dd) {
	os_completion_t completion;

	pr_dbg("bdbuf:syncdev: %08x\n", (unsigned)dd->dev);
	/*
	 * Take the sync lock before locking the cache. Once we have the sync lock we
	 * can lock the cache. If another thread has the sync lock it will cause this
	 * thread to block until it owns the sync lock then it can own the cache. The
	 * sync lock can only be obtained with the cache unlocked.
	 */
	bcache_lock_sync();
	bcache_lock_cache();

	/*
	 * Set the cache to have a sync active for a specific device and let the swap
	 * out task know the id of the requester to wake when done.
	 *
	 * The swap out task will negate the sync active flag when no more buffers
	 * for the device are held on the "modified for sync" queues.
	 */
	os_completion_reinit(&completion);
	bdbuf_cache.sync_active = true;
	bdbuf_cache.sync_requester = &completion;
	bdbuf_cache.sync_device = dd;

	bcache_wake_swapper();
	bcache_unlock_cache();
	os_completion_wait(&completion);
	bcache_unlock_sync();

	return 0;
}

/**
 * Swapout transfer to the driver. The driver will break this I/O into groups
 * of consecutive write requests is multiple consecutive buffers are required
 * by the driver. The cache is not locked.
 *
 * @param transfer The transfer transaction.
 */
static void bcache_swapout_write(struct bcache_swapout_transfer *transfer) {
	struct rte_list *node;

	pr_dbg("bdbuf:swapout transfer: %08x\n", (unsigned)transfer->dd->dev);
	/*
	 * If there are buffers to transfer to the media transfer them.
	 */
	if (!rte_list_empty(&transfer->bds)) {
		/*
		 * The last block number used when the driver only supports
		 * continuous blocks in a single request.
		 */
		uint32_t last_block = 0;

		struct bcache_device *dd = transfer->dd;
		uint32_t media_blocks_per_block = dd->media_blocks_per_block;
		bool need_continuous_blocks =
			(dd->phys_dev->capabilities & BCACHE_DEV_CAP_MULTISECTOR_CONT) != 0;

		/*
		 * Take as many buffers as configured and pass to the driver. Note, the
		 * API to the drivers has an array of buffers and if a chain was passed
		 * we could have just passed the list. If the driver API is updated it
		 * should be possible to make this change with little effect in this
		 * code. The array that is passed is broken in design and should be
		 * removed. Merging members of a struct into the first member is
		 * trouble waiting to happen.
		 */
		transfer->write_req.status = -1;
		transfer->write_req.bufnum = 0;

		while ((node = transfer->bds.next) != NULL) {
			struct bcache_buffer *bd = (struct bcache_buffer *)node;
			bool write = false;

			rte_list_del(node);
			/*
			 * If the device only accepts sequential buffers and this is not the
			 * first buffer (the first is always sequential, and the buffer is not
			 * sequential then put the buffer back on the transfer chain and write
			 * the committed buffers.
			 */
			pr_dbg("bdbuf:swapout write: bd:%" PRIu32 ", bufnum:%" PRIu32
					" mode:%s\n",
					bd->block, transfer->write_req.bufnum,
					need_continuous_blocks ? "MULTI" : "SCAT");

			if (need_continuous_blocks && transfer->write_req.bufnum &&
				bd->block != last_block + media_blocks_per_block) {
				rte_list_add(&bd->link, &transfer->bds);
				write = true;
			} else {
				struct bcache_sg_buffer *buf;
				buf = &transfer->write_req.bufs[transfer->write_req.bufnum];
				transfer->write_req.bufnum++;
				buf->user = bd;
				buf->block = bd->block;
				buf->length = dd->block_size;
				buf->buffer = bd->buffer;
				last_block = bd->block;
			}

			/*
			 * Perform the transfer if there are no more buffers, or the transfer
			 * size has reached the configured max. value.
			 */

			if (rte_list_empty(&transfer->bds) ||
				(transfer->write_req.bufnum >= bdbuf_config.max_write_blocks))
				write = true;

			if (write) {
				bcache_execute_transfer_request(dd, &transfer->write_req, false);
				transfer->write_req.status = -1;
				transfer->write_req.bufnum = 0;
			}
		}

		/*
		 * If sync'ing and the deivce is capability of handling a sync IO control
		 * call perform the call.
		 */
		if (transfer->syncing && 
			(dd->phys_dev->capabilities & BCACHE_DEV_CAP_SYNC)) {
			 dd->ioctl(dd->phys_dev, BCACHE_DEV_REQ_SYNC, NULL);
		}
	}
}

/**
 * Process the modified list of buffers. There is a sync or modified list that
 * needs to be handled so we have a common function to do the work.
 *
 * @param dd_ptr Pointer to the device to handle. If BDBUF_INVALID_DEV no
 * device is selected so select the device of the first buffer to be written to
 * disk.
 * @param chain The modified chain to process.
 * @param transfer The chain to append buffers to be written too.
 * @param sync_active If true this is a sync operation so expire all timers.
 * @param update_timers If true update the timers.
 * @param timer_delta It update_timers is true update the timers by this
 *                    amount.
 */
static void 
bcache_swapout_modified_processing(struct bcache_device **dd_ptr,
	struct rte_list *chain, 
	struct rte_list *transfer,
	bool sync_active, 
	bool update_timers,
	uint32_t timer_delta) {

	if (!rte_list_empty(chain)) {
		struct rte_list *node = chain->next;
		bool sync_all;

		/*
		 * A sync active with no valid dev means sync all.
		 */
		if (sync_active && (*dd_ptr == BDBUF_INVALID_DEV))
			sync_all = true;
		else
			sync_all = false;

		while (node != chain /* !rtems_chain_is_tail(chain, node) */) {
			struct bcache_buffer *bd = (struct bcache_buffer *)node;

			/*
			 * Check if the buffer's hold timer has reached 0. If a sync is active
			 * or someone waits for a buffer written force all the timers to 0.
			 *
			 * @note Lots of sync requests will skew this timer. It should be based
			 *       on TOD to be accurate. Does it matter ?
			 */
			if (sync_all || 
				(sync_active && (*dd_ptr == bd->dd)) ||
				bcache_has_buffer_waiters())
				bd->hold_timer = 0;

			if (bd->hold_timer) {
				if (update_timers) {
					if (bd->hold_timer > timer_delta)
						bd->hold_timer -= timer_delta;
					else
						bd->hold_timer = 0;
				}

				if (bd->hold_timer) {
					node = node->next;
					continue;
				}
			}

			/*
			 * This assumes we can set it to BDBUF_INVALID_DEV which is just an
			 * assumption. Cannot use the transfer list being empty the sync dev
			 * calls sets the dev to use.
			 */
			if (*dd_ptr == BDBUF_INVALID_DEV)
				*dd_ptr = bd->dd;

			if (bd->dd == *dd_ptr) {
				struct rte_list *next_node = node->next;
				struct rte_list *tnode = transfer->prev;

				/*
				 * The blocks on the transfer list are sorted in block order. This
				 * means multi-block transfers for drivers that require consecutive
				 * blocks perform better with sorted blocks and for real disks it may
				 * help lower head movement.
				 */
				bcache_set_state(bd, BCACHE_STATE_TRANSFER);
				rte_list_del(node);

				while (node && tnode != transfer) {
					struct bcache_buffer *tbd = (struct bcache_buffer *)tnode;

					if (bd->block > tbd->block) {
						__rte_list_add(node, tnode, tnode->next);
						node = NULL;
					} else
						tnode = tnode->prev;
				}

				if (node)
					rte_list_add(node, transfer);

				node = next_node;
			} else {
				node = node->next;
			}
		}
	}
}

/**
 * Process the cache's modified buffers. Check the sync list first then the
 * modified list extracting the buffers suitable to be written to disk. We have
 * a device at a time. The task level loop will repeat this operation while
 * there are buffers to be written. If the transfer fails place the buffers
 * back on the modified list and try again later. The cache is unlocked while
 * the buffers are being written to disk.
 *
 * @param timer_delta It update_timers is true update the timers by this
 *                    amount.
 * @param update_timers If true update the timers.
 * @param transfer The transfer transaction data.
 *
 * @retval true Buffers where written to disk so scan again.
 * @retval false No buffers where written to disk.
 */
static bool 
bcache_swapout_processing(unsigned long timer_delta, bool update_timers,
	struct bcache_swapout_transfer *transfer) {
	struct bcache_swapout_worker *worker = NULL;
	bool transfered_buffers = false;
	bool sync_active;

	bcache_lock_cache();

	/*
	 * To set this to true you need the cache and the sync lock.
	 */
	sync_active = bdbuf_cache.sync_active;

	/*
	 * If a sync is active do not use a worker because the current code does not
	 * cleaning up after. We need to know the buffers have been written when
	 * syncing to release sync lock and currently worker threads do not return to
	 * here. We do not know the worker is the last in a sequence of sync writes
	 * until after we have it running so we do not know to tell it to release the
	 * lock. The simplest solution is to get the main swap out task perform all
	 * sync operations.
	 */
	if (!sync_active) {
		if (!rte_list_empty(&bdbuf_cache.swapout_free_workers)) {
			worker =
				(struct bcache_swapout_worker *)bdbuf_cache.swapout_free_workers.next;
			rte_list_del(&worker->link);
			transfer = &worker->transfer;
		}
	}

	RTE_INIT_LIST(&transfer->bds);
	transfer->dd = BDBUF_INVALID_DEV;
	transfer->syncing = sync_active;

	/*
	 * When the sync is for a device limit the sync to that device. If the sync
	 * is for a buffer handle process the devices in the order on the sync
	 * list. This means the dev is BDBUF_INVALID_DEV.
	 */
	if (sync_active)
		transfer->dd = bdbuf_cache.sync_device;

	/*
	 * If we have any buffers in the sync queue move them to the modified
	 * list. The first sync buffer will select the device we use.
	 */
	bcache_swapout_modified_processing(&transfer->dd, &bdbuf_cache.sync,
											&transfer->bds, true, false, timer_delta);

	/*
	 * Process the cache's modified list.
	 */
	bcache_swapout_modified_processing(&transfer->dd, &bdbuf_cache.modified,
											&transfer->bds, sync_active, update_timers,
											timer_delta);

	/*
	 * We have all the buffers that have been modified for this device so the
	 * cache can be unlocked because the state of each buffer has been set to
	 * TRANSFER.
	 */
	bcache_unlock_cache();

	/*
	 * If there are buffers to transfer to the media transfer them.
	 */
	if (!rte_list_empty(&transfer->bds)) {
		if (worker)
			os_completed(&worker->swapout_sync);
		else
			bcache_swapout_write(transfer);
		transfered_buffers = true;
	}

	if (sync_active && !transfered_buffers) {
		os_completion_t *sync_requester;

		bcache_lock_cache();
		sync_requester = bdbuf_cache.sync_requester;
		bdbuf_cache.sync_active = false;
		bdbuf_cache.sync_requester = NULL;
		bcache_unlock_cache();
		if (sync_requester)
			os_completed(sync_requester);
	}

	return transfered_buffers;
}

/**
 * The swapout worker thread body.
 *
 * @param arg A pointer to the worker thread's private data.
 * @return void Not used.
 */
static void 
bcache_swapout_worker_task(void * arg) {
	struct bcache_swapout_worker *worker = (struct bcache_swapout_worker *)arg;

	while (worker->enabled) {
		os_completion_wait(&worker->swapout_sync);
		bcache_swapout_write(&worker->transfer);

		bcache_lock_cache();
		RTE_INIT_LIST(&worker->transfer.bds);
		worker->transfer.dd = BDBUF_INVALID_DEV;
		rte_list_add_tail(&worker->link, &bdbuf_cache.swapout_free_workers);
		bcache_unlock_cache();
	}

	general_free(worker);
	os_thread_exit();
}

/**
 * Close the swapout worker threads.
 */
static void bcache_swapout_workers_close(void) {
	struct rte_list *pos;

	bcache_lock_cache();
	rte_list_foreach(pos, &bdbuf_cache.swapout_free_workers) {
		struct bcache_swapout_worker *worker;
		worker = rte_container_of(pos, struct bcache_swapout_worker, link);
		worker->enabled = false;
		os_completed(&worker->swapout_sync);
	}
	bcache_unlock_cache();
}

/**
 * Body of task which takes care on flushing modified buffers to the disk.
 *
 * @param arg A pointer to the global cache data. Use the global variable and
 *            not this.
 * @return void Not used.
 */
static void bcache_swapout_task(void * arg) {
	struct bcache_swapout_transfer *transfer = (struct bcache_swapout_transfer *)arg;
	const uint32_t period_in_msecs = bdbuf_config.swapout_period;
	uint32_t timer_delta;

	timer_delta = period_in_msecs;
	while (bdbuf_cache.swapout_enabled) {
		bool update_timers = true;
		bool transfered_buffers;

		do {
			transfered_buffers = false;

			/*
			 * Extact all the buffers we find for a specific device. The device is
			 * the first one we find on a modified list. Process the sync queue of
			 * buffers first.
			 */
			if (bcache_swapout_processing(timer_delta, update_timers, transfer))
				transfered_buffers = true;
			update_timers = false;
		} while (transfered_buffers);

		os_completion_timedwait(&bdbuf_cache.swapout_signal, period_in_msecs);
	}

	bcache_swapout_workers_close();
	general_free(transfer);
	os_thread_exit();
}

static void 
bcache_purge_list(struct rte_list *purge_list) {
	struct rte_list *pos, *next;
	bool wake_buffer_waiters = false;

	rte_list_foreach_safe(pos, next, purge_list) {
		struct bcache_buffer *bd = rte_container_of(pos, 
			struct bcache_buffer, link);

		rte_list_del(pos);
		if (bd->waiters == 0)
			wake_buffer_waiters = true;
		bcache_discard_buffer(bd);
	}

	if (wake_buffer_waiters)
		bcache_wake(&bdbuf_cache.buffer_waiters);
}

static inline void
__bcache_purge(struct rte_list* purge_list, struct bcache_buffer* cur,
	const struct bcache_device* dd) {
	if (cur->dd == dd) {
		switch (cur->state) {
		case BCACHE_STATE_FREE:
		case BCACHE_STATE_EMPTY:
		case BCACHE_STATE_ACCESS_PURGED:
		case BCACHE_STATE_TRANSFER_PURGED:
			break;
		case BCACHE_STATE_SYNC:
			bcache_wake(&bdbuf_cache.transfer_waiters);
			/* Fall through */
		case BCACHE_STATE_MODIFIED:
			bcache_group_release(cur);
			/* Fall through */
		case BCACHE_STATE_CACHED:
			rte_list_del(&cur->link);
			rte_list_add_tail(&cur->link, purge_list);
			break;
		case BCACHE_STATE_TRANSFER:
			bcache_set_state(cur, BCACHE_STATE_TRANSFER_PURGED);
			break;
		case BCACHE_STATE_ACCESS_CACHED:
		case BCACHE_STATE_ACCESS_EMPTY:
		case BCACHE_STATE_ACCESS_MODIFIED:
			bcache_set_state(cur, BCACHE_STATE_ACCESS_PURGED);
			break;
		default:
			bcache_fatal(BCACHE_FATAL_STATE_11);
		}
	}
}

#ifdef CONFIG_BCACHE_HASH_MAP
static void 
bcache_hash_gather_for_purge(struct rte_list *purge_list,
	const struct bcache_device *dd) {
	for (size_t i = 0; i < rte_array_size(bdbuf_cache.root.hashq); i++) {
		struct rte_hlist *head = &bdbuf_cache.root.hashq[i];
		struct rte_hnode *pos;
		rte_hlist_foreach(pos, head) {
			struct bcache_buffer *cur = rte_container_of(pos, 
				struct bcache_buffer, hash);
			__bcache_purge(purge_list, cur, dd);
		}
	}
}
#else /* !CONFIG_BCACHE_HASH_MAP */
static void 
bcache_avl_gather_for_purge(struct rte_list *purge_list,
	const struct bcache_device *dd) {
	struct bcache_buffer *stack[BCACHE_AVL_MAX_HEIGHT];
	struct bcache_buffer **prev = stack;
	struct bcache_buffer *cur = bdbuf_cache.root.tree;

	*prev = NULL;

	while (cur != NULL) {
		__bcache_purge(purge_list, cur, dd);
		if (cur->avl.left != NULL) {
			/* Left */
			++prev;
			*prev = cur;
			cur = cur->avl.left;
		} else if (cur->avl.right != NULL) {
			/* Right */
			++prev;
			*prev = cur;
			cur = cur->avl.right;
		} else {
			while (*prev != NULL &&
				   (cur == (*prev)->avl.right || (*prev)->avl.right == NULL)) {
				/* Up */
				cur = *prev;
				--prev;
			}
			if (*prev != NULL)
				/* Right */
				cur = (*prev)->avl.right;
			else
				/* Finished */
				cur = NULL;
		}
	}
}
#endif /* CONFIG_BCACHE_HASH_MAP */


static void 
bcache_do_purge_dev(struct bcache_device *dd) {
	struct rte_list purge_list;

	RTE_INIT_LIST(&purge_list);
#ifdef CONFIG_BCACHE_READ_AHEAD
	bcache_read_ahead_reset(dd);
#endif
	bcache_container_gather_for_purge(&purge_list, dd);
	bcache_purge_list(&purge_list);
}

void 
bcache_purge_dev(struct bcache_device *dd) {
	bcache_lock_cache();
	bcache_do_purge_dev(dd);
	bcache_unlock_cache();
}

int 
bcache_set_block_size(struct bcache_device *dd, uint32_t block_size,
	bool sync) {
	int sc = 0;

	/*
	 * We do not care about the synchronization status since we will purge the
	 * device later.
	 */
	if (sync)
		(void)bcache_syncdev(dd);

	bcache_lock_cache();
	if (block_size > 0) {
		size_t bds_per_group = bcache_bds_per_group(block_size);
		if (bds_per_group != 0) {
			uint32_t media_blocks_per_block = block_size / dd->media_block_size;
			int block_to_media_block_shift = 0;
			int block_size_shift = 0;

			while ((1ul << block_to_media_block_shift) < media_blocks_per_block)
				++block_to_media_block_shift;

			while ((1ul << block_size_shift) < block_size)
				++block_size_shift;

#ifndef CONFIG_BCACHE_BLOCK_POWEROF2_MEDIA_SIZE
			if ((dd->media_block_size << block_to_media_block_shift) != block_size)
				block_to_media_block_shift = -1;
#endif
			dd->block_size_shift = block_size_shift;
			dd->block_size = block_size;
			dd->block_count = dd->size / media_blocks_per_block;
			dd->media_blocks_per_block = media_blocks_per_block;
			dd->block_to_media_block_shift = block_to_media_block_shift;
			dd->bds_per_group = bds_per_group;
			bcache_do_purge_dev(dd);
		} else {
			sc = -EINVAL;
		}
	} else {
		sc = -EINVAL;
	}
	bcache_unlock_cache();
	return sc;
}

#ifdef CONFIG_BCACHE_READ_AHEAD
static void 
bcache_read_ahead_task(void * arg) {
	struct rte_list *list = &bdbuf_cache.read_ahead_chain;
	struct rte_list *pos, *next;

	while (bdbuf_cache.read_ahead_enabled) {
		os_completion_wait(&bdbuf_cache.read_ahead);
		bcache_lock_cache();

		rte_list_foreach_safe(pos, next, list) {
			struct bcache_device *dd = rte_container_of(pos, 
				struct bcache_device, read_ahead.node);
				
			rte_list_del(pos);

			bcache_num_t block = dd->read_ahead.next;
			bcache_num_t media_block = 0;
			int sc = bcache_get_media_block(dd, block, &media_block);
			if (sc == 0) {
				struct bcache_buffer *bd =
					bcache_get_buffer_for_read_ahead(dd, media_block);

				if (bd != NULL) {
					uint32_t transfer_count = dd->read_ahead.nr_blocks;
					uint32_t blocks_until_end_of_disk = dd->block_count - block;
					uint32_t max_transfer_count = bdbuf_config.max_read_ahead_blocks;

					if (transfer_count == BCACHE_READ_AHEAD_SIZE_AUTO) {
						transfer_count = blocks_until_end_of_disk;
						if (transfer_count >= max_transfer_count) {
							transfer_count = max_transfer_count;
							dd->read_ahead.trigger = block + transfer_count / 2;
							dd->read_ahead.next = block + transfer_count;
						} else {
							dd->read_ahead.trigger = BCACHE_READ_AHEAD_NO_TRIGGER;
						}
					} else {
						if (transfer_count > blocks_until_end_of_disk)
							transfer_count = blocks_until_end_of_disk;

						if (transfer_count > max_transfer_count)
							transfer_count = max_transfer_count;

						++dd->stats.read_ahead_peeks;
					}

					++dd->stats.read_ahead_transfers;
					bcache_execute_read_request(dd, bd, transfer_count);
				}
			} else {
				dd->read_ahead.trigger = BCACHE_READ_AHEAD_NO_TRIGGER;
			}
		}
		bcache_unlock_cache();
	}
	
	os_thread_exit();
}
#endif /* CONFIG_BCACHE_READ_AHEAD */

void 
bcache_get_device_stats(const struct bcache_device *dd,
	struct bcache_stats *stats) {
	bcache_lock_cache();
	*stats = dd->stats;
	bcache_unlock_cache();
}

void 
bcache_reset_device_stats(struct bcache_device *dd) {
	bcache_lock_cache();
	memset(&dd->stats, 0, sizeof(dd->stats));
	bcache_unlock_cache();
}

int 
bcache_ioctl(struct bcache_device *dd, uint32_t req, void *argp) {
	int rc = 0;

	switch (req) {
	case BCACHE_IO_GETMEDIABLKSIZE:
		*(uint32_t *)argp = dd->media_block_size;
		break;
	case BCACHE_IO_GETBLKSIZE:
		*(uint32_t *)argp = dd->block_size;
		break;
	case BCACHE_IO_SETBLKSIZE:
		rc = bcache_set_block_size(dd, *(uint32_t *)argp, true);
		break;
	case BCACHE_IO_GETSIZE:
		*(bcache_num_t *)argp = dd->size;
		break;
	case BCACHE_IO_SYNCDEV:
		rc = bcache_syncdev(dd);
		break;
	case BCACHE_IO_GETDISKDEV:
		*(struct bcache_device **)argp = dd;
		break;
	case BCACHE_IO_PURGEDEV:
		bcache_purge_dev(dd);
		break;
	case BCACHE_IO_GETDEVSTATS:
		bcache_get_device_stats(dd, (struct bcache_stats *)argp);
		break;
	case BCACHE_IO_RESETDEVSTATS:
		bcache_reset_device_stats(dd);
		break;
	default:
		rc = -EINVAL;
		break;
	}

	return rc;
}

void 
bcache_print_stats(const struct bcache_stats *stats, 
	uint32_t media_block_size,
	uint32_t media_block_count, 
	uint32_t block_size,
	struct printer *printer) {
	pr_vout(printer,
			"-----------------------------------------------------------------------"
			"--------\n"
			"                               DEVICE STATISTICS\n"
			"----------------------+------------------------------------------------"
			"--------\n"
			" MEDIA BLOCK SIZE     | %" PRIu32 "\n"
			" MEDIA BLOCK COUNT    | %" PRIu32 "\n"
			" BLOCK SIZE           | %" PRIu32 "\n"
			" READ HITS            | %" PRIu32 "\n"
			" READ MISSES          | %" PRIu32 "\n"
			" READ AHEAD TRANSFERS | %" PRIu32 "\n"
			" READ AHEAD PEEKS     | %" PRIu32 "\n"
			" READ BLOCKS          | %" PRIu32 "\n"
			" READ ERRORS          | %" PRIu32 "\n"
			" WRITE TRANSFERS      | %" PRIu32 "\n"
			" WRITE BLOCKS         | %" PRIu32 "\n"
			" WRITE ERRORS         | %" PRIu32 "\n"
			"----------------------+------------------------------------------------"
			"--------\n",
			media_block_size, media_block_count, block_size, stats->read_hits,
			stats->read_misses, stats->read_ahead_transfers, stats->read_ahead_peeks,
			stats->read_blocks, stats->read_errors, stats->write_transfers,
			stats->write_blocks, stats->write_errors
	);
}

int 
bcache_disk_init_phys(struct bcache_device *dd, 
	uint32_t block_size,
	bcache_num_t block_count,
	bcache_device_ioctl handler,
	void *driver_data) {

	if (block_size & (block_size - 1))
		return -EINVAL;

	dd = memset(dd, 0, sizeof(*dd));
	dd->phys_dev = dd;
	dd->size = block_count;
	dd->media_block_size = block_size;
	dd->ioctl = handler;
	dd->driver_data = driver_data;
	dd->read_ahead.trigger = BCACHE_READ_AHEAD_NO_TRIGGER;
	if (block_count > 0) {
		if ((*handler)(dd, BCACHE_IO_CAPABILITIES, &dd->capabilities)) 
			dd->capabilities = 0;
		return bcache_set_block_size(dd, block_size, false);
	}

	return -EINVAL;
}

int 
bcache_disk_init_log(struct bcache_device *dd, 
	struct bcache_device *phys_dd,
	bcache_num_t block_begin,
	bcache_num_t block_count) {
	dd = memset(dd, 0, sizeof(*dd));
	dd->phys_dev = phys_dd;
	dd->start = block_begin;
	dd->size = block_count;
	dd->media_block_size = phys_dd->media_block_size;
	dd->ioctl = phys_dd->ioctl;
	dd->driver_data = phys_dd->driver_data;
	dd->read_ahead.trigger = BCACHE_READ_AHEAD_NO_TRIGGER;

	if (phys_dd->phys_dev == phys_dd) {
		bcache_num_t phys_block_count = phys_dd->size;
		if (block_begin < phys_block_count && block_count > 0 &&
			block_count <= phys_block_count - block_begin) {
			return bcache_set_block_size(dd, phys_dd->media_block_size, false);
		}
	}
	return -EINVAL;
}

ssize_t 
bcache_dev_read(struct bcache_device *dd, void *buffer, 
	size_t count, off_t offset) {
	ssize_t remaining = (ssize_t)count;
	ssize_t block_size = dd->block_size;
	bcache_num_t block;
	ssize_t block_offset;
	char *dst = buffer;
	int rv;

	block = offset >> dd->block_size_shift;
	block_offset = offset & (dd->block_size_shift - 1);

	while (remaining > 0) {
		struct bcache_buffer *bd;
		ssize_t copy;

		rv = bcache_read(dd, block, &bd);
		if (rte_unlikely(rv))
			return -EIO;

		copy = block_size - block_offset;
		if (copy > remaining) 
			copy = remaining;

		memcpy(dst, (char *)bd->buffer + block_offset, (size_t)copy);
		rv = bcache_release(bd);
		if (rte_unlikely(rv))
			return -EIO;

		block_offset = 0;
		remaining -= copy;
		dst += copy;
		++block;
	}

	return (ssize_t)count;
}

ssize_t 
bcache_dev_write(struct bcache_device *dd, const void *buffer,
	size_t count, off_t offset) {
	ssize_t block_size = dd->block_size;
	ssize_t remaining = count;
	ssize_t block_offset;
	bcache_num_t block;
	const char *src = buffer;
	int rv;

	block = offset >> dd->block_size_shift;
	block_offset = offset & (dd->block_size_shift - 1);

	while (remaining > 0) {
		struct bcache_buffer *bd;
		ssize_t copy;

		if (block_offset == 0 && remaining >= block_size)
			rv = bcache_get(dd, block, &bd);
		else
			rv = bcache_read(dd, block, &bd);
		if (rte_unlikely(rv))
			return -EIO;

		copy = block_size - block_offset;
		if (copy > remaining) 
			copy = remaining;
			
		memcpy((char *)bd->buffer + block_offset, src, (size_t)copy);
		rv = bcache_release_modified(bd);
		if (rte_unlikely(rv))
			return -EIO;

		block_offset = 0;
		remaining -= copy;
		src += copy;
		++block;
	}

	return (ssize_t)count;
}

int 
bcache_dev_ioctl(struct bcache_device *dd, unsigned int request,
	void *buffer) {
	if (request != BCACHE_IO_REQUEST)
		return dd->ioctl(dd, request, buffer);
	return -EINVAL;
}

int 
bcache_dev_create(const char* device, uint32_t media_block_size,
	bcache_num_t media_block_count, bcache_device_ioctl handler,
	void* driver_data, struct bcache_device **dd) {
	struct bcache_devnode *devn;
	int err;

	if (device == NULL || handler == NULL)
		return -EINVAL;

	if (!media_block_size || !media_block_count)
		return -EINVAL;

	if (media_block_size & (media_block_size- 1))
		return -EINVAL;

	devn = general_calloc(1, sizeof(*devn) + strlen(device) + 1);
	if (devn == NULL)
		return -ENOMEM;

	err = bcache_disk_init_phys(&devn->dev, media_block_size, 
		media_block_count, handler, driver_data);
	if (err) {
		general_free(devn);
		return err;
	}

	devn->name = (char *)(devn + 1);
	strcpy(devn->name, device);
	bcache_lock_cache();
	rte_list_add_tail(&devn->link, &bdbuf_cache.dev_nodes);
	bcache_unlock_cache();
	if (dd)
		*dd = &devn->dev;

	return 0;
}

struct bcache_device* 
bcache_dev_find(const char* device) {
	struct rte_list *pos;

	if (device == NULL)
		return NULL;

	bcache_lock_cache();
	rte_list_foreach(pos, &bdbuf_cache.dev_nodes) {
		struct bcache_devnode *devn = rte_container_of(pos, 
			struct bcache_devnode, link);
		if (!strcmp(devn->name, device)) {
			bcache_unlock_cache();
			return &devn->dev;
		}
	}
	bcache_unlock_cache();
	return NULL;
}





//TODO:
const struct bcache_config bcache_configuration = {
	.max_read_ahead_blocks = 0,
	.max_write_blocks = 32, 
	.swapout_priority = 10,
	.swapout_period = 1000,
	.swap_block_hold = 1000,
	.swapout_workers = 1,
	.swapout_worker_priority = 10,
	.task_stack_size = 8192,
	.size = 64*1024,
	.buffer_min = 512,
	.buffer_max = 4096,
	.read_ahead_priority = 9
};

struct ramdisk {
	char area[1024*1024];
	size_t block_size;
};

static int 
ramdisk_read(struct ramdisk *rd, struct bcache_request *req) {
	struct bcache_sg_buffer *sg = req->bufs;
	uint8_t *from = (uint8_t *)rd->area;

	for (uint32_t i = 0; i < req->bufnum; i++, sg++) {
		memcpy(sg->buffer, from + (sg->block * rd->block_size), sg->length);
	}
	bcache_request_done(req, 0);
	return 0;
}

static int 
ramdisk_write(struct ramdisk *rd, struct bcache_request *req) {
	uint8_t *to = (uint8_t *)rd->area;
	struct bcache_sg_buffer *sg = req->bufs;

	for (uint32_t i = 0; i < req->bufnum; i++, sg++) {
		memcpy(to + (sg->block * rd->block_size), sg->buffer, sg->length);
	}
	bcache_request_done(req, 0);
	return 0;
}

static int 
ramdisk_ioctl(struct bcache_device *dd, uint32_t req, void *argp) {
	struct ramdisk *rd = bcache_disk_get_driver_data(dd);
	switch (req) {
	case BCACHE_IO_REQUEST: {
		struct bcache_request *r = argp;
		switch (r->req) {
		case BCACHE_DEV_REQ_READ:
			return ramdisk_read(rd, r);
		case BCACHE_DEV_REQ_WRITE:
			return ramdisk_write(rd, r);
		default:
			errno = EINVAL;
			return -1;
		}
		break;
	}
	case BCACHE_IO_DELETED:
		break;
	default:
		return bcache_ioctl(dd, req, argp);
		break;
	}
	errno = EINVAL;
	return -1;
}

static struct bcache_device *ramdisk_init(void) {
	static struct ramdisk ramdisk_inst = {
		.block_size = 512
	};
	struct bcache_device *dd;

	bcache_dev_create("ramdisk", 
		ramdisk_inst.block_size,
		sizeof(ramdisk_inst.area) / ramdisk_inst.block_size, 
		ramdisk_ioctl,
		&ramdisk_inst,
		&dd
	);
	return dd;
}

void ramdisk_test(void) {
	struct bcache_device *dd;
	//char buffer_4k[4096];
	char buffer_512b[512];
	size_t delta = 512;
	size_t count = 120;

	bcache_init();
	dd = ramdisk_init();

	for (size_t i = 0; i < count; i++) {
		memset(buffer_512b, 0, sizeof(buffer_512b));
		sprintf(buffer_512b, "bcache test text: %d\n", (int)i);
		bcache_dev_write(dd, buffer_512b, sizeof(buffer_512b), delta * i);
	}

	for (size_t i = 0; i < count; i++) {
		memset(buffer_512b, 0, sizeof(buffer_512b));
		bcache_dev_read(dd, buffer_512b, 200, delta * i);
		printf("Read(%d): %s\n", 0, buffer_512b);
	}

	bcache_syncdev(dd);
	bcache_purge_dev(dd);
}

