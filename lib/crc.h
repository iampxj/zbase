/*
 * Copyright 2022 wtcat
 */
#ifndef BASEWORK_LIB_CRC_H_
#define BASEWORK_LIB_CRC_H_

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

uint16_t lib_crc16(const uint8_t *src, size_t len);
uint16_t lib_crc16part(const uint8_t *src, size_t len, uint16_t crc16val);
uint32_t lib_crc32(const uint8_t *src, size_t len);
uint32_t lib_crc32part(const uint8_t *src, size_t len, uint32_t crc32val);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_LIB_CRC_H_ */
