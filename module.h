/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_MODULE_H_
#define BASEWORK_MODULE_H_


#ifdef __cplusplus
extern "C"{
#endif

/*
 * Compiler IDs
 */
#define COMPILER_GNU    0x01
#define COMPILER_CLANG  0x02

struct bin_module {
    const char    name[16]; /* moudule name */    
    unsigned long magic;
    unsigned long cid; /* compiler ID */
    unsigned long text_start; /* .text section */
    unsigned long text_size;
    unsigned long data_start; /* .data section */
    unsigned long data_size;
    unsigned long bss_start;  /* .bss section  */
    unsigned long bss_size;

    int (*load)(void *ctx);
    int (*unload)(void *ctx);
};

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_MODULE_H_ */
