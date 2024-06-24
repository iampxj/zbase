/*
 * Copyright 2024 wtcat
 */
#ifndef WORKOUT_DBASE_H_
#define WORKOUT_DBASE_H_

#include "basework/generic.h"
#include "basework/container/queue.h"

#ifdef __cplusplus
extern "C"{
#endif

struct spdata_object {
    STAILQ_ENTRY(spdata_object) node;
    const char *name;
    const char *path;
    size_t      osize;
    void       *private;
    int         loaded;

    int  (*load)(struct spdata_object *o);
    int  (*unload)(struct spdata_object *o);
    int  (*reset)(struct spdata_object *o, void *p);
};

#ifndef _WIN32
#define WORKOUT_SPDATA_REGISTER(obj) \
    rte_sysinit(__workout_spdata_##obj##__register, 200) { \
        workout_spdata_register(&obj); \
    }
#else /* _WIN32 */
#define WORKOUT_SPDATA_REGISTER(obj)
#endif /* !_WIN32 */

#if defined(__linux__) || defined(_WIN32)
#define WORKOUT_PATH(path) path
#else
#define WORKOUT_PATH(path) "/PTb:/"path
#endif

struct spdata_object *workout_spdata_find(const char *name);
int workout_spdata_load_one(const char *name, struct spdata_object **so);
int workout_spdata_load(void);
int workout_spdata_unload(void);
int workout_spdata_register(struct spdata_object *o);

#ifdef __cplusplus
}
#endif
#endif /* SPORT_BASE_H_ */
