/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_DEV_GPT_H_
#define BASEWORK_DEV_GPT_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

struct gp_entry {
    char     name[32];
    char     parent[16];
    uint32_t offset;
    uint32_t size;
};

/*
 * gp_load - Load parition informaton from json
 *
 * @buffer Point to json content
 * return 0 if success
 */
int gp_load(const char *buffer);

/*
 * gp_destroy - Destroy partition
 */
void gp_destroy(void);

/*
 * gp_find - Find partition entry by name
 *
 * @name parition name
 * return entry if success
 */
const struct gp_entry *gp_find(const char *name);

/*
 * gp_dump - Dump parition information
 */
void gp_dump(void);


#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEV_GPT_H_ */
