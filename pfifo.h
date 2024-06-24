/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */
#include <stdint.h>
#ifndef _WIN32
# include <stdatomic.h>
#endif
/*
 * Fifo struct mapped in a shared memory. It describes a circular buffer FIFO
 * Write and read should wrap around. Fifo is empty when write == read
 * Writing should never overwrite the read position
 */
struct pfifo {
	unsigned write;              /**< Next position to be written*/
	unsigned read;               /**< Next position to be read */
	unsigned len;                /**< Circular buffer length */
	unsigned elem_size;          /**< Pointer size - for 32/64 bit OS */
	void *volatile buffer[];     /**< The buffer contains mbuf pointers */
};


/**
 * @internal when c11 memory model enabled use c11 atomic memory barrier.
 * when under non c11 memory model use rte_smp_* memory barrier.
 *
 * @param src
 *   Pointer to the source data.
 * @param dst
 *   Pointer to the destination data.
 * @param value
 *   Data value.
 */
#ifndef __PFIFO_LOAD
#define __PFIFO_LOAD(src) ({                         \
		__atomic_load_n((src), __ATOMIC_ACQUIRE);           \
	})
#endif
#ifndef __PFIFO_STORE
#define __PFIFO_STORE(dst, value) do {               \
		__atomic_store_n((dst), value, __ATOMIC_RELEASE);   \
	} while(0)
#endif

/**
 * Initializes the kni fifo structure
 */
static inline int
pfifo_init(struct pfifo *fifo, unsigned size)
{
	/* Ensure size is power of 2 */
	if (size & (size - 1))
		return -1;

	fifo->write = 0;
	fifo->read = 0;
	fifo->len = size;
	fifo->elem_size = sizeof(void *);
	return 0;
}

/**
 * Adds num elements into the fifo. Return the number actually written
 */
static inline unsigned
pfifo_put(struct pfifo *fifo, void **data, unsigned num)
{
	unsigned i = 0;
	unsigned fifo_write = fifo->write;
	unsigned new_write = fifo_write;
	unsigned fifo_read = __PFIFO_LOAD(&fifo->read);

	for (i = 0; i < num; i++) {
		new_write = (new_write + 1) & (fifo->len - 1);

		if (new_write == fifo_read)
			break;
		fifo->buffer[fifo_write] = data[i];
		fifo_write = new_write;
	}
	__PFIFO_STORE(&fifo->write, fifo_write);
	return i;
}

/**
 * Get up to num elements from the fifo. Return the number actually read
 */
static inline unsigned
pfifo_get(struct pfifo *fifo, void **data, unsigned num)
{
	unsigned i = 0;
	unsigned new_read = fifo->read;
	unsigned fifo_write = __PFIFO_LOAD(&fifo->write);

	for (i = 0; i < num; i++) {
		if (new_read == fifo_write)
			break;

		data[i] = fifo->buffer[new_read];
		new_read = (new_read + 1) & (fifo->len - 1);
	}
	__PFIFO_STORE(&fifo->read, new_read);
	return i;
}

/**
 * Get the num of elements in the fifo
 */
static inline uint32_t
pfifo_count(struct pfifo *fifo)
{
	unsigned fifo_write = __PFIFO_LOAD(&fifo->write);
	unsigned fifo_read = __PFIFO_LOAD(&fifo->read);
	return (fifo->len + fifo_write - fifo_read) & (fifo->len - 1);
}

/**
 * Get the num of available elements in the fifo
 */
static inline uint32_t
pfifo_free_count(struct pfifo *fifo)
{
	uint32_t fifo_write = __PFIFO_LOAD(&fifo->write);
	uint32_t fifo_read = __PFIFO_LOAD(&fifo->read);
	return (fifo_read - fifo_write - 1) & (fifo->len - 1);
}
