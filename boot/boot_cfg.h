/*
 * Copyright 2022 wtcat
 */
#ifndef BASEWORK_BOOT_BOOT_CFG_H_
#define BASEWORK_BOOT_BOOT_CFG_H_

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define BOOT_CW06//BOOT_CW06//BOOT_3085E

#if defined(BOOT_CW07X)
# define FIRMWARE_SIZE    _KB(2700) //For cw06x
#elif defined(BOOT_CW06)
# define FIRMWARE_SIZE    _KB(2280) //For cw06x
#elif defined(BOOT_3085E)
# define FIRMWARE_SIZE    _KB(2216) //For ats3085e
#else
# define FIRMWARE_SIZE    _KB(2048) //Default
#endif

#define _KB(x) ((x) * 1024)

#ifdef BOOT_CW07X
#define LOAD_DEVICE_SIZE 0x1800000 /* 16M */
#else
#define LOAD_DEVICE_SIZE 0x1000000 /* 16M */
#endif
#define RUN_DEVICE_BASE  0x24000

#define FW_LOAD_DEVICE    "spi_flash"
#define FW_INFO_OFFSET(x)  ((x) - 0x1000)
#define FW_LOAD_OFFSET    (FW_INFO_OFFSET(LOAD_DEVICE_SIZE)   - FIRMWARE_SIZE)

#define FW_RUN_DEVICE   "spi_flash"
#define FW_RUN_OFFSET  RUN_DEVICE_BASE

#ifndef FW_FLASH_BLKSIZE
#define FW_FLASH_BLKSIZE 4096
#endif

#endif /* BASEWORK_BOOT_BOOT_CFG_H_ */