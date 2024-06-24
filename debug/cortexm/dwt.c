/*
 * CopyRight 2022 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <stddef.h>
#include <init.h>
#include <arch/cpu.h>

#include "basework/debug/cortexm/dwt.h"

#if defined(__CORE_CM3_H_GENERIC) || \
	defined(__CORE_CM4_H_GENERIC) || \
	defined(__CORE_CM7_H_GENERIC)
struct watchpoint_regs {
	volatile uint32_t comp;
	volatile uint32_t mask;
	volatile uint32_t function;
	uint32_t reserved;
};
#elif defined(__CORE_CM33_H_GENERIC) || \
	  defined(__CORE_CM23_H_GENERIC)
struct watchpoint_regs {
	volatile uint32_t comp;
	uint32_t reserved_1;
	volatile uint32_t function;
	uint32_t reserved_2;
};
#else
#error "Unknown CPU architecture"
#endif /* __CORE_CM_H_GENERIC */

#define WATCHPOINT_REG() WATCHPOINT_REGADDR(COMP0)
#define WATCHPOINT_REGADDR(member) \
	(struct watchpoint_regs *)(DWT_BASE + offsetof(DWT_Type, member));

#define MTX_LOCK(_lock)  (_lock) = arch_irq_lock()
#define MTX_UNLOCK(_lock) arch_irq_unlock(_lock)

static int _watchpoint_nums;

static int __cortexm_dwt_get_hwbpks(void) {
	return (DWT->CTRL & DWT_CTRL_NUMCOMP_Msk) >> DWT_CTRL_NUMCOMP_Pos;
}

static void __cortexm_dwt_access(int ena) {
#if defined(__CORE_CM7_H_GENERIC)
	uint32_t lsr = DWT->LSR;
	if ((lsr & DWT_LSR_Present_Msk) != 0) {
		if (!!ena) {
			if ((lsr & DWT_LSR_Access_Msk) != 0) 
				DWT->LAR = 0xC5ACCE55; /* unlock it */
		} else {
			if ((lsr & DWT_LSR_Access_Msk) == 0) 
				DWT->LAR = 0; /* Lock it */
		}
	}
#else
	(void) ena;
#endif
}

static int __cortexm_dwt_init(void) {
	CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
	__cortexm_dwt_access(1);
	return 0;
}

static int __cortexm_dwt_enable_monitor_exception(void) {
	 if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
	 	return -EBUSY;
#if defined(CONFIG_ARMV8_M_SE) && !defined(CONFIG_ARM_NONSECURE_FIRMWARE)
	if (!(CoreDebug->DEMCR & DCB_DEMCR_SDME_Msk))
		return -EBUSY;
#endif
	CoreDebug->DEMCR |= CoreDebug_DEMCR_MON_EN_Msk;
	return 0;
}

int _cortexm_dwt_enable(int nr, void *addr, unsigned int mode) {
	if (nr > _watchpoint_nums)
		return -EINVAL;
	struct watchpoint_regs *wp_regs = WATCHPOINT_REG();
	uint32_t func = (mode & 0xF) << DWT_FUNCTION_DATAVSIZE_Pos;
    uint32_t level;
	MTX_LOCK(level);
#if defined(__CORE_CM3_H_GENERIC) || \
	defined(__CORE_CM4_H_GENERIC) || \
	defined(__CORE_CM7_H_GENERIC)
	if (mode & MEM_READ)
		func |= (0x5 << DWT_FUNCTION_FUNCTION_Pos);
	if (mode & MEM_WRITE)
		func |= (0x6 << DWT_FUNCTION_FUNCTION_Pos);	
	wp_regs[nr].comp = (uint32_t)addr;
	wp_regs[nr].mask = 0;
	wp_regs[nr].function = func;
	
#elif defined(__CORE_CM33_H_GENERIC) || \
	  defined(__CORE_CM23_H_GENERIC)
	if (mode & MEM_READ)
		func |= (0x6 << DWT_FUNCTION_MATCH_Pos);
	if (mode & MEM_WRITE)
		func |= (0x5 << DWT_FUNCTION_MATCH_Pos);	
	func |= (0x1 << DWT_FUNCTION_ACTION_Pos);
	wp_regs[nr].comp = (uint32_t)addr;
	wp_regs[nr].function = func;
#endif
	MTX_UNLOCK(level);
    return 0;
}

int _cortexm_dwt_disable(int nr) {
	if (nr > _watchpoint_nums)
		return -EINVAL;
	struct watchpoint_regs *wp_regs = WATCHPOINT_REG();
    uint32_t level;
	MTX_LOCK(level);
	wp_regs[nr].function = 0;
	MTX_UNLOCK(level);
	return 0;
}

int _cortexm_dwt_busy(int nr) {
	struct watchpoint_regs *wp_regs = WATCHPOINT_REG();
	uint32_t func = wp_regs[nr].function;
	return func != 0;
}

int _cortexm_dwt_setup(void) {
	int nr = __cortexm_dwt_get_hwbpks();
	__cortexm_dwt_init();
	for (int i = 0; i < nr; i++)
		_cortexm_dwt_disable(i);
	__cortexm_dwt_enable_monitor_exception();
	_watchpoint_nums = nr;
	return 0;
}
