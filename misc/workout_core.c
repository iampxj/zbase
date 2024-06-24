/*
 * Copyright 2024 wtcat 
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<workout>: "fmt
#include <errno.h>
#include <assert.h>
#include <string.h>

#include "basework/log.h"
#include "basework/malloc.h"
#include "basework/misc/workout_core.h"

static TAILQ_HEAD(,workout_class) domain_list = 
    TAILQ_HEAD_INITIALIZER(domain_list);

static void add_child(struct workout_class *parent, struct workout_class *child) {
    TAILQ_INSERT_TAIL(&parent->list, child, node);
    parent->child_cnt++;
    child->parent = parent;
}

static void remove_node(struct workout_class *parent, struct workout_class *wkc) {
    struct workout_class *pos, *next;

    /* Remove children */
    TAILQ_FOREACH_SAFE(pos, &wkc->list, node, next) {
        wkc->remove(wkc, pos);
    }

    /* Remove self */
    if (parent != NULL) {
        TAILQ_REMOVE(&parent->list, wkc, node);
        parent->child_cnt--;
    }
    general_free(wkc);
}

static void domain_remove_node(struct workout_class *parent, 
    struct workout_class *wkc) {
    TAILQ_REMOVE(&domain_list, wkc, node);
    remove_node(parent, wkc);
}

static struct workout_class *_workout_next(struct workout_class* wkc) {
    struct workout_class *parent = wkc->parent;
    struct workout_class *next;

    if (parent->flags == WORKOUT_GROUP_F) {
        struct workout_group *grp = (struct workout_group *)parent;
        if (TAILQ_NEXT(wkc, node) == NULL) {
            if (grp->remain_count > 0) {
                grp->remain_count--;
                if (grp->remain_count == 0)
                    next = TAILQ_NEXT(wkc, node);
                else
                    next = TAILQ_FIRST(&parent->list);
            } else
                next = TAILQ_NEXT(wkc, node);
        } else {
            next = TAILQ_NEXT(wkc, node);
        }
    } else {
        next = TAILQ_NEXT(wkc, node);
    }

    return next;
}

static void walk_around(struct workout_class *parent, struct workout *wks[], 
    int level, workout_vistor_t iter, void *arg) {
    struct workout_class *wkc;

    TAILQ_FOREACH(wkc, &parent->list, node) {
        wks[level] = (struct workout *)wkc;
        iter(wks, level, wkc->flags == WORKOUT_GROUP_F, arg);
        walk_around(wkc, wks, level+1, iter, arg);
    }
}

void *workout_class_create(struct workout_class *parent, const char *name, 
    const char *help, size_t objsize, size_t extsize, unsigned int flags) {
    struct workout_class *wkc;
    size_t namelen;
    size_t tiplen;

    namelen = name? strlen(name) + 1: 0;
    tiplen  = help? strlen(help) + 1: 0;
    wkc = general_calloc(1, namelen + tiplen + objsize + extsize);
    if (wkc == NULL) {
        pr_err("Allocate workout-class item(%s) failed", name);
        return NULL;
    }

    if (extsize > 0) {
        wkc->priv = (char *)wkc + objsize;
        wkc->name = (char *)((char *)wkc->priv + extsize);
    } else {
        wkc->name = (char *)wkc + objsize;
    }
    
    if (namelen > 0)
        strcpy(wkc->name, name);
    else
        wkc->name = NULL;

    if (tiplen > 0) {
        wkc->help = wkc->name + namelen;
        strcpy(wkc->help, help);
    }

    TAILQ_INIT(&wkc->list);
    wkc->flags     = flags;
    wkc->add_child = add_child;
    wkc->remove    = remove_node;

    /* Append to list */
    if (parent != NULL)
        workout_add_child(parent, wkc);

    return wkc;
}

struct workout_domain *domain_workout_create(const char *title, const char *help, 
    size_t extsize) {
    struct workout_domain *domain;

    if (title == NULL)
        return NULL;

    domain = workout_class_create(NULL, title, help, 
        sizeof(struct workout_domain), extsize, WORKOUT_DOMAIN_F);
    if (domain != NULL) {
        domain->super.remove = domain_remove_node;
        TAILQ_INSERT_TAIL(&domain_list, &domain->super, node);
    }
    return domain;
}

int workout_delete(struct workout_class *wkc) {
    if (wkc == NULL)
        return -EINVAL;

    wkc->remove(wkc->parent, wkc);
    return 0;
}

struct workout *workout_first_child(struct workout_class *parent) {
    return (struct workout *)TAILQ_FIRST(&parent->list);
}

struct workout *workout_next(struct workout *curr) {
    return (struct workout *)_workout_next(&curr->super);
}

int workout_domain_iterate(struct workout_domain *domain,
    workout_vistor_t iter, void *arg) {
    struct workout *wks[16];

    if (domain == NULL || iter == NULL)
        return -EINVAL;

    walk_around(&domain->super, wks, 0, iter, arg);
    return 0;
}

struct workout_domain *workout_domain_first(void) {
    return (struct workout_domain *)TAILQ_FIRST(&domain_list);
}

struct workout_domain *workout_domain_next(struct workout_domain *domain) {
    return (struct workout_domain *)TAILQ_NEXT(&domain->super, node);
}