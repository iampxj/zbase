/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_MODULE_H_
#define BASEWORK_MODULE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

struct module_runtime;

/*
 * Compiler IDs
 */
#if defined(__clang__)
# define MODULE_COMPILER_ID 0x10
#elif defined(__GNUC__)
# define MODULE_COMPILER_ID 0x11
#else
# error "Unknown compiler"
#endif 

struct module_runtime;
struct module_class;

struct module {
#define MODULE_MAGIC 0xFDFDFDFD 
    uintptr_t  magic;
    uintptr_t  cid; /* compiler ID */
    uintptr_t  mod_size;
    uintptr_t  text_start; /* .text section */
    uintptr_t  text_size;
    uintptr_t  data_start; /* .data section */
    uintptr_t  data_size;
    uintptr_t  ldata_start;
    uintptr_t  bss_start;  /* .bss section  */
    uintptr_t  bss_size;

    int (*load)(struct module_class *api);
    int (*unload)(struct module_class *api);
};

/*
 * Moudle state
 */
#define MODULE_STATE_IDLE       0
#define MODULE_STATE_UNLOADING  1
#define MODULE_STATE_LOADING    2
#define MODULE_STATE_LOADED     3

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_MODULE_H_ */
