/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_DEV_GPT_H_
#define BASEWORK_DEV_GPT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

typedef void (*gpt_updated_cb)(void);

struct gpt_entry {
    char     name[32];
    char     parent[16];
    char     fname[36];
    uint32_t offset;
    uint32_t size;
};

/*
 * gpt_load - Load parition informaton from json
 *
 * @buffer Point to json content
 * return 0 if success
 */
int gpt_load(const char *buffer);

/*
 * gpt_destroy - Destroy partition
 */
void gpt_destroy(void);

/*
 * gpt_find - Find partition entry by name
 *
 * @name parition name
 * return entry if success
 */
const struct gpt_entry *gpt_find(const char *name);

/*
 * gpt_find_by_filename - Find partition entry by file name
 *
 * @name parition file name
 * return entry if success
 */
const struct gpt_entry* gpt_find_by_filename(const char *fname);

/*
 * gpt_dump - Dump parition information
 */
void gpt_dump(void);

/*
 * gpt_signature - Generate partition signature
 */
int gpt_signature(
    int (*signature)(const void *buf, uint32_t size, void *ctx), 
    void *ctx);

/*
 * gpt_register_update_cb - Register update callback
 */
int gpt_register_update_cb(gpt_updated_cb cb);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEV_GPT_H_ */
