/*
 * Copyright 2024 wtcat
 */
#include <stdbool.h>
#include <stdint.h>

#include "basework/generic.h"
#include "basework/os/osapi.h"
#include "basework/assert.h"
#include "basework/debug/kasan.h"
#include "basework/container/queue.h"

#define KASAN_INIT_VALUE 0xDEADCAFE

struct kasan_region {
	SLIST_ENTRY(kasan_region) link;
	uintptr_t begin;
	uintptr_t end;
	uintptr_t shadow[1];
};

STATIC_ASSERT(sizeof(struct kasan_region) == KASAN_REGION_STRUCT_SIZE, "");

static SLIST_HEAD(kasan_list, kasan_region) kasan_list = 
	SLIST_HEAD_INITIALIZER(kasan_list);
static uintptr_t kasan_state;

static __rte_always_inline uintptr_t *
kasan_mem_to_shadow(const void *ptr, size_t size, unsigned int *bit) {
	struct kasan_region *region;
	uintptr_t addr = (uintptr_t)ptr;

	if (rte_unlikely(kasan_state != KASAN_INIT_VALUE))
		return NULL;

	SLIST_FOREACH(region, &kasan_list, link) {
		if (addr >= region->begin && addr < region->end) {
			rte_assert(addr + size <= region->end);
			addr -= region->begin;
			addr /= KASAN_SHADOW_SCALE;
			*bit = addr % KASAN_BITS_PER_WORD;
			return &region->shadow[addr / KASAN_BITS_PER_WORD];
		}
	}

	return NULL;
}

static void kasan_report(const void *addr, size_t size, bool is_write,
						 void *return_address) {
	static int recursion;

	if (++recursion == 1) {
		pr_out("kasan detected a %s access error, address at %p,"
			   "size is %zu, return address: %p\n",
			   is_write ? "write" : "read", addr, size, return_address);
		rte_assert0(0);
	}

	--recursion;
}

static bool kasan_is_poisoned(const void *addr, size_t size) {
	uintptr_t *p;
	unsigned int bit = 0;

	p = kasan_mem_to_shadow((char *)addr + size - 1, 1, &bit);
	return p && ((*p >> bit) & 1);
}

static __rte_always_inline void 
kasan_set_poison(const void *addr, size_t size, bool poisoned) {
	os_critical_declare
	unsigned int bit = 0, nbit;
	uintptr_t mask, *p;

	os_critical_lock
	p = kasan_mem_to_shadow(addr, size, &bit);
	rte_assert(p != NULL);
	nbit = KASAN_BITS_PER_WORD - bit % KASAN_BITS_PER_WORD;
	mask = KASAN_FIRST_WORD_MASK(bit);

	size /= KASAN_SHADOW_SCALE;
	while (size >= nbit) {
		if (poisoned)
			*p++ |= mask;
		else
			*p++ &= ~mask;
		
		bit += nbit;
		size -= nbit;

		nbit = KASAN_BITS_PER_WORD;
		mask = UINTPTR_MAX;
	}

	if (size) {
		mask &= KASAN_LAST_WORD_MASK(bit + size);
		if (poisoned)
			*p |= mask;
		else
			*p &= ~mask;
	}
	os_critical_unlock
}

static inline void 
kasan_check_report(const void *addr, size_t size, bool is_write,
	void *return_address) {
	if (kasan_is_poisoned(addr, size))
		kasan_report(addr, size, is_write, return_address);
}

void kasan_poison(const void *addr, size_t size) {
	kasan_set_poison(addr, size, true);
}

void kasan_unpoison(const void *addr, size_t size) {
	kasan_set_poison(addr, size, false);
}

void kasan_register(void *addr, size_t *size) {
	os_critical_declare
	struct kasan_region *region;

	region = (struct kasan_region *)((char *)addr + *size - KASAN_REGION_SIZE(*size));

	region->begin = (uintptr_t)addr;
	region->end = region->begin + *size;

	os_critical_lock
	SLIST_INSERT_HEAD(&kasan_list, region, link);
	kasan_state = KASAN_INIT_VALUE;
	os_critical_unlock

	kasan_poison(addr, *size);
	*size -= KASAN_REGION_SIZE(*size);
}

/* 
 * Exported functions called from the compiler generated code 
 */
void __sanitizer_annotate_contiguous_container(const void *beg, const void *end,
	const void *old_mid, const void *new_mid) {
	/* Shut up compiler complaints */
}

void __asan_before_dynamic_init(const void *module_name) {
	/* Shut up compiler complaints */
}

void __asan_after_dynamic_init(void) {
	/* Shut up compiler complaints */
}

void __asan_handle_no_return(void) {
	/* Shut up compiler complaints */
}

void __asan_report_load_n_noabort(void *addr, size_t size) {
	kasan_report(addr, size, false, RTE_RET_IP);
}

void __asan_report_store_n_noabort(void *addr, size_t size) {
	kasan_report(addr, size, true, RTE_RET_IP);
}

void __asan_loadN_noabort(void *addr, size_t size) {
	kasan_check_report(addr, size, false, RTE_RET_IP);
}

void __asan_storeN_noabort(void *addr, size_t size) {
	kasan_check_report(addr, size, true, RTE_RET_IP);
}

void __asan_loadN(void *addr, size_t size) {
	kasan_check_report(addr, size, false, RTE_RET_IP);
}

void __asan_storeN(void *addr, size_t size) {
	kasan_check_report(addr, size, true, RTE_RET_IP);
}

#define DEFINE_ASAN_LOAD_STORE(size)                                                     \
	void __asan_report_load##size##_noabort(void *addr) {                                \
		kasan_report(addr, size, false, RTE_RET_IP);                              \
	}                                                                                    \
	void __asan_report_store##size##_noabort(void *addr) {                               \
		kasan_report(addr, size, true, RTE_RET_IP);                               \
	}                                                                                    \
	void __asan_load##size##_noabort(void *addr) {                                       \
		kasan_check_report(addr, size, false, RTE_RET_IP);                        \
	}                                                                                    \
	void __asan_store##size##_noabort(void *addr) {                                      \
		kasan_check_report(addr, size, true, RTE_RET_IP);                         \
	}                                                                                    \
	void __asan_load##size(void *addr) {                                                 \
		kasan_check_report(addr, size, false, RTE_RET_IP);                        \
	}                                                                                    \
	void __asan_store##size(void *addr) {                                                \
		kasan_check_report(addr, size, true, RTE_RET_IP);                         \
	}

DEFINE_ASAN_LOAD_STORE(1)
DEFINE_ASAN_LOAD_STORE(2)
DEFINE_ASAN_LOAD_STORE(4)
DEFINE_ASAN_LOAD_STORE(8)
DEFINE_ASAN_LOAD_STORE(16)
