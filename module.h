/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_MODULE_H_
#define BASEWORK_MODULE_H_

#include <stddef.h>
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

/*
 * Define a loadable module
 */
#ifdef MODULE_LOADABLE
extern char _stext[];
extern char _etext[];
extern char _sdata[];
extern char _edata[];
extern char _sbss[];
extern char _ebss[];

#define module_install(_name, _load_fn, _unload_fn) \
    struct module _name##__module __attribute__((section(".vectors"))) = { \
        .magic         = MODULE_MAGIC, \
        .cid           = MODULE_COMPILER_ID, \
        .mod_size      = (uintptr_t)_module_end, \
        .text_start    = (uintptr_t)_stext, \
        .text_size     = (uintptr_t)(_etext - _stext), \
        .data_start    = (uintptr_t)_sdata, \
        .data_size     = (uintptr_t)(_edata - _sdata), \
        .ldata_start   = 0, \
        .bss_start     = (uintptr_t)_sbss, \
        .bss_size      = (uintptr_t)(_ebss - _sbss), \
        .load          = _load_fn, \
        .unload        = _unload_fn \
    }
#endif /* MODULE_LOADABLE */

/*
 * Public interface
 */
int module_load_fromfile(const char *file, struct module_class *api, 
    uint32_t *id);
int module_load_frommem(void *code, size_t size, struct module_class *api, 
    uint32_t *id);
int moudule_unload(uint32_t id);
int module_get(uint32_t id);
int module_put(uint32_t id);
int module_init(void);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_MODULE_H_ */
