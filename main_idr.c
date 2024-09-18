/*
 * Copyright 2024 wtcat
 */
#include <stddef.h>
#include <stdint.h>
#include <errno.h>
#include <stdio.h>

struct idr {
	void *idr_list;
	void **idr_table;
	unsigned int idr_base;
	unsigned int idr_count;
};

#define IDR_BUFSZ(_min, _nr) (sizeof(struct idr) + (_nr) * sizeof(void *))
#define IDR_DEFINE(_name, _min, _nr) \
	static char _name##_idr_buffer[IDR_BUFSZ(_min, _nr)] \
		__attribute__((aligned(sizeof(void *)))); \
	static struct idr *_name##_idr_create(void) { \
		struct idr *idr = (struct idr *)_name##_idr_buffer; \
		__idr_init(idr, _min, _nr); \
		return idr; \
	}

static inline int __idr_init(struct idr *idr, unsigned int base, 
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

static inline int idr_alloc(struct idr *idr, void *ptr) {
	void *p = (void *)(((unsigned long)idr->idr_list >> 1) << 1);
	if (p) {
		idr->idr_list = *(void **)p;
		*(void **)p = ptr;
		return (unsigned long)((void **)p - idr->idr_table) + idr->idr_base;
	}
	return -1;
}

static inline int idr_remove(struct idr *idr, unsigned int id) {
    unsigned rid = id - idr->idr_base;
	if (rid >= idr->idr_count)
		return -EINVAL;
	
	idr->idr_table[rid] = (void *)((unsigned long)idr->idr_list | 1);
	idr->idr_list = (void *)&idr->idr_table[rid];
	return 0;
}

static inline void *idr_find(struct idr *idr, unsigned int id) {
    unsigned rid = id - idr->idr_base;
	if (rid >= idr->idr_count)
		return NULL;
	return !((unsigned long)idr->idr_table[rid] & 1)? 
		idr->idr_table[rid]: NULL;
}


IDR_DEFINE(test, 10, 3)
static struct idr *idr_inst;


int main(int argc, char *argv[]) {

    uint32_t varray[4] = {1, 2, 3, 4};
    uint32_t ids[4] = {0};

	idr_inst = test_idr_create();
    // __idr_init(idr_inst, 10, 3);
    for (int i = 0; i < 4; i++) {
        int id = idr_alloc(idr_inst, &varray[i]);
        ids[i] = (uint32_t)id;
        printf("Allocate: %d\n", id);
    }

    for (int i = 0; i < 4; i++) {
        uint32_t *pid = idr_find(idr_inst, ids[i]);
        if (pid)
            printf("Find: %d: %p -- %u\n", ids[i], pid, *pid);
    }

    for (int i = 0; i < 4; i++) {
        idr_remove(idr_inst, ids[i]);
    }

    for (int i = 0; i < 4; i++) {
        uint32_t *pid = idr_find(idr_inst, ids[i]);
        if (pid)
            printf("Find: %d: %p -- %u\n", ids[i], pid, *pid);
    }

    return 0;
}