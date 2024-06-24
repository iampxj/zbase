/*
 * Copyright 2022 wtcat
 *
 */
#ifndef BASEWORK_OS_OBJ_H_
#define BASEWORK_OS_OBJ_H_

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

struct os_robj {
    void *freechain;
};

/*
 * os_obj_ready - Determine if the object has been initialized
 * robj: pointer to resource object
 *
 * return true is ok
 */
bool os_obj_ready(struct os_robj *robj);

/*
 * os_obj_allocate - Allocate a new object
 * robj: pointer to resource object
 *
 * return object pointer if success 
 */
void *os_obj_allocate(struct os_robj *robj);

/*
 * os_obj_free - Free a object
 * robj: Pointer to resource object
 *
 */
void os_obj_free(struct os_robj *robj, void *p);

/*
 * os_obj_initialize - Initialize object table
 * robj: pointer to resource object
 * buffer: start address
 * size: buffer size
 * isize: the size of single object
 *
 * return 0 if success
 */
int os_obj_initialize(struct os_robj *robj, void *buffer, size_t size, 
    size_t isize);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_OS_OBJ_H_ */
