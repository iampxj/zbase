/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_IDR_H_
#define BASEWORK_IDR_H_

#include <stddef.h>
#include "basework/compiler.h"

#ifdef __cplusplus
extern "C"{
#endif

struct idr {
	void *idr_list;
	void **idr_table;
	unsigned int idr_base;
	unsigned int idr_count;
};

#define IDR_BUFSZ(_nr) \
    (sizeof(struct idr) + (_nr) * sizeof(void *))

#define IDR_DEFINE(_name, _min, _nr) \
	static char _name##_idr_buffer[IDR_BUFSZ(_nr)] \
		__attribute__((aligned(sizeof(void *)))); \
	static struct idr *_name##_idr_create(void) { \
		struct idr *idr = (struct idr *)_name##_idr_buffer; \
		__idr_init(idr, _min, _nr); \
		return idr; \
    }

/*
 * __idr_init - Initialize ID context
 *
 * @idr: ID context address
 * @base: base ID 
 * @count: maximum number of IDs
 * return 0 if success
 */
int __idr_init(struct idr *idr, unsigned int base, 
	unsigned int count);

/*
 * idr_alloc - Allocate object ID and associate with user data
 *
 * @idr: ID context
 * @ptr: Point to user data
 * @return positive number if success
 */
int idr_alloc(struct idr *idr, void *ptr);

/*
 * idr_remove - Delete object ID
 *
 * @idr: ID context
 * @ptr: object id
 * @return 0 if success
 */
int idr_remove(struct idr *idr, unsigned int id);

/*
 * idr_find - Find user data by object id
 *
 * @idr: ID context
 * @ptr: object id
 * @return user data if success
 */
static inline void *idr_find(struct idr *idr, unsigned int id) {
    unsigned rid = id - idr->idr_base;
	if (rte_unlikely(rid >= idr->idr_count))
		return NULL;
	return !((unsigned long)idr->idr_table[rid] & 1)? 
		idr->idr_table[rid]: NULL;
}

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_IDR_H_ */
