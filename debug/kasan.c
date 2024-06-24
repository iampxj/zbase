/*
 * Copyright wtcat 2024
 *
 * -fsanitize=kernel-addr
 * -fno-sanitize=all
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <stdint.h>
#include <string.h>

#include "basework/generic.h"
#include "basework/debug/kasan.h"
#include "basework/log.h"
#include "basework/os/osapi.h"

/*
 * Memory access mode
 */
#define ASAN_WRITE 0
#define ASAN_READ  1

#define ASAN_MEM_BEG (memory_base)
#define ASAN_MEM_END (memory_base + ASAN_MEM_SIZE)
#define IS_ADDR_VALID(addr) \
    (((uintptr_t)(addr) >= ASAN_MEM_BEG) && ((uintptr_t)(addr) < ASAN_MEM_END))

static uint8_t   shadow_area[ASAN_MEM_SIZE / 8] __rte_section(ASAN_SHADOW_SECTION);
static uintptr_t memory_base = ASAN_MEM_BASE;

static void asan_report_error(void *addr, size_t size, int access, 
    void *retip) {
    pr_out("Asan check failure: %s addr 0x%x size: %d retip(%p)\n", 
        access == ASAN_WRITE? "write": "read", addr, size, retip);
    os_panic();
}

static void __rte_noreturn __asan_report_error(void) {
    os_panic();
    for ( ; ; );
}

static __rte_always_inline uint8_t *asan_mem2shadow(void *addr) {
    uintptr_t ofs = (uintptr_t)addr - memory_base;
    return shadow_area + (ofs >> 3);
}

static __rte_always_inline void asan_poison_byte(void *addr) {
    if (IS_ADDR_VALID(addr)) {
        uint8_t *p = asan_mem2shadow(addr);
        *p |= 1 << ((uintptr_t)addr & 7);
    }
}

static __rte_always_inline void asan_clear_byte(void *addr) {
    if (IS_ADDR_VALID(addr)) {
        uint8_t *p = asan_mem2shadow(addr);
        *p &= ~(1 << ((uintptr_t)addr & 7));
    }
}

static __rte_always_inline int asan_slowpath_check(int8_t shadow, void *addr, 
    size_t size) {
    int8_t last = ((uintptr_t)addr & 7) + size;
    return last >= shadow;
}

/* 
 * Check if we are access to poisoned memory
 */
static void asan_check(void *addr, size_t size, int access, void *retip) {
    if (IS_ADDR_VALID(addr)) {
        int8_t shadow = *asan_mem2shadow(addr);
        if (rte_unlikely(shadow < 0)) {
            asan_report_error(addr, size, access, retip);
        } else if (shadow > 0) {
            if (rte_unlikely(asan_slowpath_check(shadow, addr, size)))
                asan_report_error(addr, size, access, retip);
        }
    }
}

void *asan_alloc_poison(void *ptr, size_t size) {
  /* 
   * Allocates the requested amount of memory with redzones around it.
   * The shadow values corresponding to the redzones are poisoned and the shadow values
   * for the memory region are cleared.
   */
    char *p   = (char *)ptr;
    char *q   = p + ASAN_REDZONE_SIZE;
    char *end = p + ASAN_ALLOCSIZE(size);

    /*
        * Poison red zone at the beginning
        */
    while (p < q) {
        asan_poison_byte(p);
        p++;
    }

    /*
     * Store memory size
     */
    *(size_t *)p = size;
    p += sizeof(size_t);

    /*
     * Clear valid memory
     */
    q = p + size - 1;
    while (q >= p) {
        asan_clear_byte(q);
        q--;
    }

    /*
     * Poison red zone at the end
     */
    for (q = p + size; q < end; q++)
        asan_poison_byte(q);
    
    return p;
}

void *asan_alloc_unpoison(void *ptr) {
    char  *p = (char *)ptr;
    char  *q = p - sizeof(size_t);
    size_t size;
    
    /*
     * Get memory size
     */
    size = *(size_t *)q;

    /*
     * Poison memory area
     */
    for (size_t i = 0; i < size; i++)
        asan_poison_byte(p + i);

    q -= ASAN_REDZONE_SIZE;

    return q;
}

int asan_init(void *base, size_t size) {
    if (base == NULL)
        return -EINVAL;

    if (size > ASAN_MEM_SIZE) {
        os_panic();
        return -EINVAL;
    }

    for (size_t i = 0; i < rte_array_size(shadow_area); i++)
        shadow_area[i] = 0;

    memory_base = (uintptr_t)base;

    return 0;
}

/* 
 * Below are the required stub function needed by ASAN 
 */
void __asan_report_store1(void *addr) {
    __asan_report_error();
}

void __asan_report_store2(void *addr) {
    __asan_report_error();
}

void __asan_report_store4(void *addr) {
    __asan_report_error();
}

void __asan_report_store_n(void *addr, ssize_t size) {
    __asan_report_error();
}

void __asan_report_load1(void *addr) {
    __asan_report_error();
}

void __asan_report_load2(void *addr) {
    __asan_report_error();
}

void __asan_report_load4(void *addr) {
    __asan_report_error();
}

void __asan_report_load_n(void *addr, ssize_t size) {
    __asan_report_error();
}

void __asan_stack_malloc_1(size_t size, void *addr) { 
    __asan_report_error(); 
}

void __asan_stack_malloc_2(size_t size, void *addr) {
    __asan_report_error(); 
}

void __asan_stack_malloc_3(size_t size, void *addr) { 
    __asan_report_error(); 
}

void __asan_stack_malloc_4(size_t size, void *addr) { 
    __asan_report_error();
}

void __asan_handle_no_return(void) { 
    __asan_report_error(); 
}

void __asan_option_detect_stack_use_after_return(void) { 
    __asan_report_error(); 
}

void __asan_register_globals(void *addr, ssize_t size) {
    (void) addr;
    (void) size;
    __asan_report_error(); 
}

void __asan_unregister_globals(void *addr, ssize_t size) {
    (void) addr;
    (void) size;
    __asan_report_error(); 
}

void __asan_version_mismatch_check_v8(void) { 
    __asan_report_error(); 
}

void __asan_loadN_noabort(void *addr, ssize_t size) {
#ifndef CONFIG_ASAN_LOAD_DISABLE
    while (size >= 8) {
        asan_check(addr, 8, ASAN_READ, RTE_RET_IP);
        addr = (char *)addr + 8;
        size -= 8;
    }
    if (size > 0)
        asan_check(addr, size, ASAN_READ, RTE_RET_IP);
#endif
}

void __asan_storeN_noabort(void *addr, ssize_t size) {
    while (size >= 8) {
        asan_check(addr, 8, ASAN_WRITE, RTE_RET_IP);
        addr = (char *)addr + 8;
        size -= 8;
    }
    if (size > 0)
        asan_check(addr, size, ASAN_WRITE, RTE_RET_IP);
}

void __asan_load16_noabort(void *addr) {
#ifndef CONFIG_ASAN_LOAD_DISABLE
    asan_check(addr, 8, ASAN_READ, RTE_RET_IP);
    asan_check((char *)addr + 8, 8, ASAN_READ, RTE_RET_IP);
#endif
}

void __asan_store16_noabort(void *addr) {
    asan_check(addr, 8, ASAN_WRITE, RTE_RET_IP);
    asan_check((char *)addr + 8, 8, ASAN_WRITE, RTE_RET_IP);
}

void __asan_load8_noabort(void *addr) {
#ifndef CONFIG_ASAN_LOAD_DISABLE
    asan_check(addr, 8, ASAN_READ, RTE_RET_IP);
#endif
}

void __asan_store8_noabort(void *addr) {
    asan_check(addr, 8, ASAN_WRITE, RTE_RET_IP);
}

void __asan_load4_noabort(void *addr) {
#ifndef CONFIG_ASAN_LOAD_DISABLE
    asan_check(addr, 4, ASAN_READ, RTE_RET_IP);
#endif
}

void __asan_store4_noabort(void *addr) {
    asan_check(addr, 4, ASAN_WRITE, RTE_RET_IP);
}

void __asan_load2_noabort(void *addr) {
#ifndef CONFIG_ASAN_LOAD_DISABLE
    asan_check(addr, 2, ASAN_READ, RTE_RET_IP);
#endif
}

void __asan_store2_noabort(void *addr) {
    asan_check(addr, 2, ASAN_WRITE, RTE_RET_IP);
}

void __asan_load1_noabort(void *addr) {
#ifndef CONFIG_ASAN_LOAD_DISABLE
    asan_check(addr, 1, ASAN_READ, RTE_RET_IP);
#endif
}

void __asan_store1_noabort(void *addr) {
    asan_check(addr, 1, ASAN_WRITE, RTE_RET_IP);
}

void __asan_before_dynamic_init(const void *addr) {

}

void __asan_after_dynamic_init(void) {

}
