/*
 * Copyright 2022 wtcat
 *
 * The simple object allocator implement
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "osapi_obj: "fmt
#include <errno.h>
#include <stddef.h>

#include "basework/os/osapi.h"
#include "basework/generic.h"
#include "basework/os/osapi_obj.h"
#include "basework/log.h"


os_critical_global_declare

bool os_obj_ready(struct os_robj *robj) {
    return robj->freechain != NULL;
}

void *os_obj_allocate(struct os_robj *robj) {
    os_critical_declare
    
    os_critical_lock
    void *p = robj->freechain;
    if (p) 
        robj->freechain = *(char **)p;
    os_critical_unlock
    return p;
}

void os_obj_free(struct os_robj *robj, void *p) {
    os_critical_declare

    os_critical_lock
    *(char **)p = robj->freechain;
    robj->freechain = p;
    os_critical_unlock
}

int os_obj_initialize(struct os_robj *robj, void *buffer, size_t size, 
    size_t isize) {
    os_critical_declare
    size_t n, fixed_isize = rte_roundup(isize, sizeof(void *));
    char *p, *head = NULL;

    if (os_obj_ready(robj)) {
        pr_warn("The object has been initialized\n");
        return -EBUSY;
    }

    if (!robj || !buffer || !size || !isize)
        return -EINVAL;

    if (!fixed_isize || size < fixed_isize) {
        pr_warn("object buffer is too small\n");
        return -EINVAL;
    }

    os_critical_lock

    /*
     * We must be check object again, Maybe another task just has been 
     * initialized object
     */
    if (os_obj_ready(robj)) {
        os_critical_unlock
        return 0;
    }

    for (p = buffer, n = size / fixed_isize; n > 0; n--) {
        *(char **)p = head;
        head = p;
        p = p + fixed_isize;
    }
   
    robj->freechain = head;
    os_critical_unlock

    return 0;
}
