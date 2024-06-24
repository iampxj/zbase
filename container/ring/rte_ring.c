/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2010-2015 Intel Corporation
 * Copyright (c) 2007,2008 Kip Macy kmacy@freebsd.org
 * All rights reserved.
 * Derived from FreeBSD's bufring.h
 * Used as BSD-3 Licensed with permission from Kip Macy.
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <errno.h>
#include <assert.h>

#include "basework/generic.h"
#include "basework/log.h"
#include "basework/malloc.h"
#include "basework/lib/string.h"
#include "basework/assert.h"

#include "basework/container/queue.h"
#include "basework/container/ring/rte_ring.h"
#include "basework/container/ring/rte_ring_elem.h"


#define rte_mcfg_tailq_read_lock()
#define rte_mcfg_tailq_read_unlock()
#define rte_mcfg_tailq_write_lock()
#define rte_mcfg_tailq_write_unlock()


#define RTE_MEMZONE_NAMESIZE 16
#define RTE_CACHE_LINE_MASK (RTE_CACHE_LINE_SIZE - 1)
#define rte_errno errno


/** dummy structure type used by the rte_tailq APIs */
struct rte_tailq_entry {
	TAILQ_ENTRY(rte_tailq_entry) next; /**< Pointer entries for a tailq list */
	void *data; /**< Pointer to the data referenced by this tailq entry */
};
/** dummy */
TAILQ_HEAD(rte_tailq_entry_head, rte_tailq_entry);

/**
 * The structure defining a tailq header entry for storing
 * in the rte_config structure in shared memory. Each tailq
 * is identified by name.
 * Any library storing a set of objects e.g. rings, mempools, hash-tables,
 * is recommended to use an entry here, so as to make it easy for
 * a multi-process app to find already-created elements in shared memory.
 */
struct rte_tailq_head {
#define RTE_TAILQ_NAMESIZE 16
	struct rte_tailq_entry_head tailq_head; /**< NOTE: must be first element */
	char name[RTE_TAILQ_NAMESIZE];
};

struct rte_tailq_elem {
	/**
	 * Reference to head in shared mem, updated at init time by
	 * rte_eal_tailqs_init()
	 */
	struct rte_tailq_head *head;
	TAILQ_ENTRY(rte_tailq_elem) next;
	const char name[RTE_TAILQ_NAMESIZE];
};

/**
 * Return the first tailq entry cast to the right struct.
 */
#define RTE_TAILQ_CAST(tailq_entry, struct_name) \
	(struct struct_name *)&(tailq_entry)->tailq_head


static TAILQ_HEAD(rte_ring_list, rte_tailq_entry) ring_tailq = 
	TAILQ_HEAD_INITIALIZER(ring_tailq);


#define RTE_LOG(level, type, fmt, args...) pr_notice(fmt, __func__, ## args)
	
/* mask of all valid flag values to ring_create() */
#define RING_F_MASK (RING_F_SP_ENQ | RING_F_SC_DEQ | RING_F_EXACT_SZ | \
		     RING_F_MP_RTS_ENQ | RING_F_MC_RTS_DEQ |	       \
		     RING_F_MP_HTS_ENQ | RING_F_MC_HTS_DEQ | RING_F_ALLOCTED)

/* true if x is a power of 2 */
#define POWEROF2(x) ((((x)-1) & (x)) == 0)

/* by default set head/tail distance as 1/8 of ring capacity */
#define HTD_MAX_DEF	8


static inline uint32_t rte_combine32ms1b(uint32_t x) {
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x;
}

static inline uint32_t rte_align32pow2(uint32_t x) {
	x--;
	x = rte_combine32ms1b(x);
	return x + 1;
}

/* return the size of memory occupied by a ring */
ssize_t
rte_ring_get_memsize_elem(unsigned int esize, unsigned int count) {
	ssize_t sz;

	/* Check if element size is a multiple of 4B */
	if (esize % 4 != 0) {
		RTE_LOG(ERR, RING, "element size is not a multiple of 4\n");

		return -EINVAL;
	}

	/* count must be a power of 2 */
	if ((!POWEROF2(count)) || (count > RTE_RING_SZ_MASK )) {
		RTE_LOG(ERR, RING,
			"Requested number of elements is invalid, must be power of 2, and not exceed %u\n",
			RTE_RING_SZ_MASK);

		return -EINVAL;
	}

	sz = sizeof(struct rte_ring) + (ssize_t)count * esize;
	sz = RTE_ALIGN(sz, RTE_CACHE_LINE_SIZE);
	return sz;
}

/* return the size of memory occupied by a ring */
ssize_t
rte_ring_get_memsize(unsigned int count) {
	return rte_ring_get_memsize_elem(sizeof(void *), count);
}

/*
 * internal helper function to reset prod/cons head-tail values.
 */
static void
reset_headtail(void *p) {
	struct rte_ring_headtail *ht;
	struct rte_ring_hts_headtail *ht_hts;
	struct rte_ring_rts_headtail *ht_rts;

	ht = p;
	ht_hts = p;
	ht_rts = p;

	switch (ht->sync_type) {
	case RTE_RING_SYNC_MT:
	case RTE_RING_SYNC_ST:
		ht->head = 0;
		ht->tail = 0;
		break;
	case RTE_RING_SYNC_MT_RTS:
		ht_rts->head.raw = 0;
		ht_rts->tail.raw = 0;
		break;
	case RTE_RING_SYNC_MT_HTS:
		ht_hts->ht.raw = 0;
		break;
	default:
		/* unknown sync mode */
		RTE_ASSERT(0);
	}
}

void
rte_ring_reset(struct rte_ring *r) {
	reset_headtail(&r->prod);
	reset_headtail(&r->cons);
}

/*
 * helper function, calculates sync_type values for prod and cons
 * based on input flags. Returns zero at success or negative
 * errno value otherwise.
 */
static int
get_sync_type(uint32_t flags, enum rte_ring_sync_type *prod_st,
	enum rte_ring_sync_type *cons_st) {
	static const uint32_t prod_st_flags =
		(RING_F_SP_ENQ | RING_F_MP_RTS_ENQ | RING_F_MP_HTS_ENQ);
	static const uint32_t cons_st_flags =
		(RING_F_SC_DEQ | RING_F_MC_RTS_DEQ | RING_F_MC_HTS_DEQ);

	switch (flags & prod_st_flags) {
	case 0:
		*prod_st = RTE_RING_SYNC_MT;
		break;
	case RING_F_SP_ENQ:
		*prod_st = RTE_RING_SYNC_ST;
		break;
	case RING_F_MP_RTS_ENQ:
		*prod_st = RTE_RING_SYNC_MT_RTS;
		break;
	case RING_F_MP_HTS_ENQ:
		*prod_st = RTE_RING_SYNC_MT_HTS;
		break;
	default:
		return -EINVAL;
	}

	switch (flags & cons_st_flags) {
	case 0:
		*cons_st = RTE_RING_SYNC_MT;
		break;
	case RING_F_SC_DEQ:
		*cons_st = RTE_RING_SYNC_ST;
		break;
	case RING_F_MC_RTS_DEQ:
		*cons_st = RTE_RING_SYNC_MT_RTS;
		break;
	case RING_F_MC_HTS_DEQ:
		*cons_st = RTE_RING_SYNC_MT_HTS;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int
rte_ring_init(struct rte_ring *r, const char *name, unsigned int count,
	unsigned int flags) {
	int ret;

	/* compilation-time checks */
	RTE_BUILD_BUG_ON((sizeof(struct rte_ring) &
			  RTE_CACHE_LINE_MASK) != 0);
	RTE_BUILD_BUG_ON((offsetof(struct rte_ring, cons) &
			  RTE_CACHE_LINE_MASK) != 0);
	RTE_BUILD_BUG_ON((offsetof(struct rte_ring, prod) &
			  RTE_CACHE_LINE_MASK) != 0);

	RTE_BUILD_BUG_ON(offsetof(struct rte_ring_headtail, sync_type) !=
		offsetof(struct rte_ring_hts_headtail, sync_type));
	RTE_BUILD_BUG_ON(offsetof(struct rte_ring_headtail, tail) !=
		offsetof(struct rte_ring_hts_headtail, ht.pos.tail));

	RTE_BUILD_BUG_ON(offsetof(struct rte_ring_headtail, sync_type) !=
		offsetof(struct rte_ring_rts_headtail, sync_type));
	RTE_BUILD_BUG_ON(offsetof(struct rte_ring_headtail, tail) !=
		offsetof(struct rte_ring_rts_headtail, tail.val.pos));

	/* future proof flags, only allow supported values */
	if (flags & ~RING_F_MASK) {
		RTE_LOG(ERR, RING,
			"Unsupported flags requested %#x\n", flags);
		return -EINVAL;
	}

	/* init the ring structure */
	memset(r, 0, sizeof(*r));
	ret = strlcpy(r->name, name, sizeof(r->name));
	if (ret < 0 || ret >= (int)sizeof(r->name))
		return -ENAMETOOLONG;
	r->flags = flags;
	ret = get_sync_type(flags, &r->prod.sync_type, &r->cons.sync_type);
	if (ret != 0)
		return ret;

	if (flags & RING_F_EXACT_SZ) {
		r->size = rte_align32pow2(count + 1);
		r->mask = r->size - 1;
		r->capacity = count;
	} else {
		if ((!POWEROF2(count)) || (count > RTE_RING_SZ_MASK)) {
			RTE_LOG(ERR, RING,
				"Requested size is invalid, must be power of 2, and not exceed the size limit %u\n",
				RTE_RING_SZ_MASK);
			return -EINVAL;
		}
		r->size = count;
		r->mask = count - 1;
		r->capacity = r->mask;
	}

	/* set default values for head-tail distance */
	if (flags & RING_F_MP_RTS_ENQ)
		rte_ring_set_prod_htd_max(r, r->capacity / HTD_MAX_DEF);
	if (flags & RING_F_MC_RTS_DEQ)
		rte_ring_set_cons_htd_max(r, r->capacity / HTD_MAX_DEF);

	return 0;
}

/* create the ring for a given element size */
struct rte_ring *
rte_ring_create_elem(const char *name, unsigned int esize, unsigned int count,
		int socket_id, unsigned int flags) {
	struct rte_ring *r;
	struct rte_tailq_entry *te;
	ssize_t ring_size;
	struct rte_ring_list* ring_list = NULL;
	const unsigned int requested_count = count;

	ring_list = &ring_tailq;

	/* for an exact size ring, round up from count to a power of two */
	if (flags & RING_F_EXACT_SZ)
		count = rte_align32pow2(count + 1);

	ring_size = rte_ring_get_memsize_elem(esize, count);
	if (ring_size < 0) {
		rte_errno = -ring_size;
		return NULL;
	}

	te = general_calloc(1, sizeof(*te));
	if (te == NULL) {
		pr_err("Cannot reserve memory for tailq\n");
		rte_errno = ENOMEM;
		return NULL;
	}

	rte_mcfg_tailq_write_lock();
	/* reserve a memory zone for this ring. If we can't get rte_config or
	 * we are secondary process, the memzone_reserve function will set
	 * rte_errno for us appropriately - hence no check in this function
	 */
	r = general_aligned_alloc(RTE_CACHE_LINE_SIZE, ring_size);
	if (r != NULL) {
		/* no need to check return value here, we already checked the
		 * arguments above */
		rte_ring_init(r, name, requested_count, flags | RING_F_ALLOCTED);
		te->data = (void *)r;
		TAILQ_INSERT_TAIL(ring_list, te, next);
	} else {
		general_free(te);
		pr_err("No more memory\n");
	}

	rte_mcfg_tailq_write_unlock();

	return r;
}

/* create the ring */
struct rte_ring *
rte_ring_create(const char *name, unsigned int count, int socket_id,
		unsigned int flags)
{
	return rte_ring_create_elem(name, sizeof(void *), count, socket_id,
		flags);
}

/* free the ring */
void
rte_ring_free(struct rte_ring *r)
{
	struct rte_ring_list *ring_list = NULL;
	struct rte_tailq_entry *te;

	if (r == NULL)
		return;

	/*
	 * Ring was not created with rte_ring_create,
	 * therefore, there is no memzone to free.
	 */
	if (!(r->flags & RING_F_ALLOCTED)) {
		RTE_LOG(ERR, RING,
			"Cannot free ring, not created with rte_ring_create()\n");
		return;
	}

	ring_list = &ring_tailq;
	rte_mcfg_tailq_write_lock();

	/* find out tailq entry */
	TAILQ_FOREACH(te, ring_list, next) {
		if (te->data == (void *) r)
			break;
	}

	if (te == NULL) {
		rte_mcfg_tailq_write_unlock();
		return;
	}

	TAILQ_REMOVE(ring_list, te, next);

	rte_mcfg_tailq_write_unlock();

	general_free(r);
	general_free(te);
}

/* dump the status of the ring on the console */
void
rte_ring_dump(struct printer *pr, const struct rte_ring *r) {
	virt_format(pr, "ring <%s>@%p\n", r->name, r);
	virt_format(pr, "  flags=%x\n", r->flags);
	virt_format(pr, "  size=%"PRIu32"\n", r->size);
	virt_format(pr, "  capacity=%"PRIu32"\n", r->capacity);
	virt_format(pr, "  ct=%"PRIu32"\n", r->cons.tail);
	virt_format(pr, "  ch=%"PRIu32"\n", r->cons.head);
	virt_format(pr, "  pt=%"PRIu32"\n", r->prod.tail);
	virt_format(pr, "  ph=%"PRIu32"\n", r->prod.head);
	virt_format(pr, "  used=%u\n", rte_ring_count(r));
	virt_format(pr, "  avail=%u\n", rte_ring_free_count(r));
}

/* dump the status of all rings on the console */
void
rte_ring_list_dump(struct printer *pr)
{
	const struct rte_tailq_entry *te;
	struct rte_ring_list *ring_list;

	ring_list = &ring_tailq;

	rte_mcfg_tailq_read_lock();

	TAILQ_FOREACH(te, ring_list, next) {
		rte_ring_dump(pr, (struct rte_ring *) te->data);
	}

	rte_mcfg_tailq_read_unlock();
}

/* search a ring from its name */
struct rte_ring *
rte_ring_lookup(const char *name)
{
	struct rte_tailq_entry *te;
	struct rte_ring *r = NULL;
	struct rte_ring_list *ring_list;

	ring_list = &ring_tailq;

	rte_mcfg_tailq_read_lock();

	TAILQ_FOREACH(te, ring_list, next) {
		r = (struct rte_ring *) te->data;
		if (strncmp(name, r->name, RTE_RING_NAMESIZE) == 0)
			break;
	}

	rte_mcfg_tailq_read_unlock();

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}

	return r;
}
