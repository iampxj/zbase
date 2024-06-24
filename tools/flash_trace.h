/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_TOOLS_FLASH_TRACE_H_
#define BASEWORK_TOOLS_FLASH_TRACE_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

int flash_trace_erase(uint32_t addr, uint32_t size);
int flash_trace_write(uint32_t addr, uint32_t size);
int flash_trace_init(void);
void flash_trace_dump(bool all);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_TOOLS_FLASH_TRACE_H_ */
