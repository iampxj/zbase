/*
 * Copyright 2023 wtcat
 */
#ifndef BASEWORK_LIB_CALLPATH_H_
#define BASEWORK_LIB_CALLPATH_H_

#include <stdint.h>
#include <stdbool.h>

#include "basework/compiler.h"

#ifdef __cplusplus
extern "C"{
#endif

struct printer;
struct callpath_node {
    void *caller;
    void *owner;
#ifdef CONFIG_PERFORMANCE_PROFILE
    uint32_t time;
#endif
};

struct callpath_max {
    struct callpath_node node;
    int top;
};

struct callpath {
#define CALLPATH_MAX_NAME 32
    struct callpath *next;
    char name[CALLPATH_MAX_NAME];
    int ovrlvl;
    int top;
    int deepth;
#ifdef CONFIG_PERFORMANCE_PROFILE
    struct callpath_max max;
#endif
    struct callpath_node stack[];
};

struct callnode_arg {
    struct printer *pr;
    int idx;
    void *buf[0];
};

#define CALLNODE_SIZE(n) \
    (sizeof(struct callnode_arg) + \
     ((n) * sizeof(void *)))

#define CALLPATH_SIZE(nitem) \
    (sizeof(struct callpath) + \
     (nitem) * sizeof(struct callpath_node))
     
struct callpath *callpath_allocate(const char *name, int deepth);
void callpath_free(struct callpath *ctx);
int callpath_push_locked(struct callpath *ctx, void *fn, 
    void *caller, uint32_t timestamp);
int callpath_pop_locked(struct callpath *ctx, void *fn, 
    void *caller, uint32_t timestamp);
int callpath_foreach(bool (*iterator)(struct callpath *ctx, void *), 
    void *arg);
int callpath_foreach_node(struct callpath *ctx,
    void (*iterator)(struct callpath_node *, void *), 
    void *arg);
int callpath_dump_locked(struct callpath *ctx, struct callnode_arg *p);
#ifdef CONFIG_FRACE_NVRAM
int callpath_nvram_dump(struct printer *pr);
#endif
int callpath_init(void) ;

/* plateform implement */
void callpath_profile_init(void);
void callpath_dump_all(struct printer *pr);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_LIB_CALLPATH_H_ */