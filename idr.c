/*
 * Copyright 2024 wtcat
 */
#include <errno.h>
#include <stddef.h>
#include <stdint.h>

#include "basework/os/osapi.h"
#include "basework/idr.h"

os_critical_global_declare

int __idr_init(struct idr *idr, unsigned int base, 
	unsigned int count) {
	if (idr == NULL)
		return -EINVAL;
	if (count == 0)
		return -EINVAL;

	char *p = (char *)(idr + 1);
	idr->idr_list = NULL;
    idr->idr_count = count;
    idr->idr_base  = base;
    idr->idr_table = (void *)(idr + 1);
	
	while (count > 0) {
		*(char **)p = (char *)((unsigned long)idr->idr_list + 1);
		idr->idr_list = p;
		p += sizeof(void *);
		count--;
	}
	return 0;
}

int idr_alloc(struct idr *idr, void *ptr) {
	os_critical_declare
	void *p;

	os_critical_lock
	p = (void *)(((unsigned long)idr->idr_list >> 1) << 1);
	if (p) {
		idr->idr_list = *(void **)p;
		*(void **)p = ptr;
		os_critical_unlock
		return (unsigned long)((void **)p - idr->idr_table) + idr->idr_base;
	}
	os_critical_unlock
	return -1;
}

int idr_remove(struct idr *idr, unsigned int id) {
	os_critical_declare
    unsigned rid = id - idr->idr_base;
	if (rte_unlikely(rid >= idr->idr_count))
		return -EINVAL;
	
	os_critical_lock
	idr->idr_table[rid] = (void *)((unsigned long)idr->idr_list | 1);
	idr->idr_list = (void *)&idr->idr_table[rid];
	os_critical_unlock
	return 0;
}
