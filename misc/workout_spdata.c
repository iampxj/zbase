/*
 * Copyright 2024 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<workout_spdata>: "
#include <string.h>

#include "basework/errno.h"
#include "basework/log.h"
#include "basework/misc/workout_spdata.h"


static STAILQ_HEAD(, spdata_object) db_list =
    STAILQ_HEAD_INITIALIZER(db_list);

struct spdata_object *workout_spdata_find(const char *name) {
    struct spdata_object *o;
    STAILQ_FOREACH(o, &db_list, node) {
        if (strcmp(name, o->name) == 0)
            return o;
    }
    return NULL;
}

int workout_spdata_load_one(const char *name, struct spdata_object **so) {
    struct spdata_object *o;
    int err = 0;

    STAILQ_FOREACH(o, &db_list, node) {
        if (!strcmp(o->name, name)) {
            if (!o->loaded) {
                err = o->load(o);
                if (!err)
                    o->loaded = 1;
            }
            if (so != NULL && o->loaded)
                *so = o;
            break;
        }
    }
    return err;
}

int workout_spdata_load(void) {
    struct spdata_object *o;
    int err = 0;
    STAILQ_FOREACH(o, &db_list, node) {
        if (!o->loaded) {
            err = o->load(o);
            if (err)
                break;
            o->loaded = 1;
        }
    }
    return err;
}

int workout_spdata_unload(void) {
    struct spdata_object *o;
    int err = 0;
    STAILQ_FOREACH(o, &db_list, node) {
        if (o->loaded) {
            err = o->unload(o);
            if (err) {
                pr_err("Unload workout data(%s) failed\n", o->name);
                continue;
            }
            o->loaded = 0;
        }
    }
    return err;
}

int workout_spdata_register(struct spdata_object *o) {
    if (o == NULL)
        return -EINVAL;

    if (o->load == NULL || o->unload == NULL)
        return -EINVAL;

    if (workout_spdata_find(o->name))
        return -EEXIST;

    STAILQ_INSERT_TAIL(&db_list, o, node);
    return 0;
}
