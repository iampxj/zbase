/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_WORKOUT_CORE_H_
#define BASEWORK_WORKOUT_CORE_H_

#include <stddef.h>
#include <stdint.h>

#include "basework/generic.h"
#include "basework/container/queue.h"

#ifdef __cplusplus
extern "C"{
#endif

struct workout_class {
    TAILQ_ENTRY(workout_class) node;
    TAILQ_HEAD(,workout_class) list;
    struct workout_class *parent;
    char         *name;
    char         *help;
    void         *priv;
    int           child_cnt;
    unsigned int  flags;
#define WORKOUT_ITEM_F   0x01
#define WORKOUT_GROUP_F  0x02
#define WORKOUT_DOMAIN_F 0x04

    void (*add_child)(struct workout_class *parent, struct workout_class *child);
    void (*remove)(struct workout_class *parent, struct workout_class *curr);
};

struct workout {
    struct workout_class super;
};

struct workout_group {
    struct workout_class super;
    int8_t  loop_count;
    int8_t  remain_count;
};

struct workout_domain {
    struct workout_class super;
};

#define WORKOUT_DOMAIN_FOREACH(d, next) \
    for ((d) = workout_domain_first(); \
        (d) && ((next) = workout_domain_next(d), 1); \
        (d) = (next))

typedef void (*workout_vistor_t)(struct workout *wks[], int level, 
    bool group, void *arg);

static inline int is_workout_group(struct workout *wk) {
    return wk->super.flags == WORKOUT_GROUP_F;
}

static inline void group_workout_reset_repeats(struct workout_group *grp) {
    grp->remain_count = grp->loop_count;
}

static inline int group_workout_get_repeats(struct workout_group *grp) {
    return grp->loop_count;
}

static inline int group_workout_get_walks(struct workout_group *grp) {
    return grp->loop_count - grp->remain_count;
}

static inline int group_workout_get_children(struct workout_group *grp) {
    return grp->super.child_cnt;
}

static inline void workout_add_child(struct workout_class *parent, 
    struct workout_class *child) {
    parent->add_child(parent, child);
}

static inline void *workoutclass_private(struct workout_class *wkc) {
    return wkc->priv;
}

static inline void *workout_private(struct workout *workout) {
    return workoutclass_private(&workout->super);
}

static inline void group_workout_set_repeats(struct workout_group *grp, 
    int repeats) {
    grp->loop_count = (int8_t)repeats;
}

void *workout_class_create(struct workout_class *parent, const char *name, 
    const char *help, size_t objsize, size_t extsize, unsigned int flags);

int workout_delete(struct workout_class *wk);

struct workout *workout_first_child(struct workout_class *parent);

struct workout *workout_next(struct workout *curr);

struct workout_domain *domain_workout_create(const char *title, const char *help, 
    size_t extsize);

size_t workout_domain_get_count(void);

int workout_domain_iterate(struct workout_domain *domain,
    workout_vistor_t iter, void *arg);

struct workout_domain *workout_domain_first(void);

struct workout_domain *workout_domain_next(struct workout_domain *domain);

static inline int 
domain_workout_destroy(struct workout_domain *domain) {
    return workout_delete(&domain->super);
}

static inline struct workout *
workout_create(struct workout_class *parent, const char *name, const char *help, size_t extsize) {
    return (struct workout *)workout_class_create(parent, name, help, sizeof(struct workout), 
        extsize, WORKOUT_ITEM_F);
}

static inline struct workout_group *
group_workout_create(struct workout_class *parent, size_t extsize) {
    return (struct workout_group *)workout_class_create(parent, NULL, NULL, 
        sizeof(struct workout_group), extsize, WORKOUT_GROUP_F);
}
#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_WORKOUT_CORE_H_ */
