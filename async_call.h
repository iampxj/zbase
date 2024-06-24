/*
 * Copyright 2023 wtcat
 */
#ifndef BASEWORK_ASYNC_H_
#define BASEWORK_ASYNC_H_

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

struct call_param;

typedef uintptr_t (*async_fn_t)(uintptr_t a1, uintptr_t a2, 
    uintptr_t a3, uintptr_t a4, uintptr_t a5);
typedef void (*async_done_t)(const struct call_param *cp);

struct call_param {
    async_fn_t fn;
    async_done_t done;
    uintptr_t a1;
    uintptr_t a2;
    uintptr_t a3;
    uintptr_t a4;
    uintptr_t a5;
    uintptr_t result;
};

/*
 * Async-call api without done
 */
#define async_call0(_fn) \
    async_call1(_fn, 0)
#define async_call1(_fn, _a1) \
    async_call2(_fn, _a1, 0)
#define async_call2(_fn, _a1, _a2) \
    async_call3(_fn, _a1, _a2, 0)
#define async_call3(_fn, _a1, _a2, _a3) \
    async_call4(_fn, _a1, _a2, _a3, 0)
#define async_call4(_fn, _a1, _a2, _a3, _a4) \
    async_call5(_fn, _a1, _a2, _a3, _a4, 0)        
#define async_call5(_fn, _a1, _a2, _a3, _a4, _a5) \
    __async_call((async_fn_t)_fn, \
        (uintptr_t)(_a1), \
        (uintptr_t)(_a2), \
        (uintptr_t)(_a3), \
        (uintptr_t)(_a4), \
        (uintptr_t)(_a5), \
        NULL, false)

#define async_urgent_call0(_fn) \
    async_urgent_call1(_fn, 0)
#define async_urgent_call1(_fn, _a1) \
    async_urgent_call2(_fn, _a1, 0)
#define async_urgent_call2(_fn, _a1, _a2) \
    async_urgent_call3(_fn, _a1, _a2, 0)
#define async_urgent_call3(_fn, _a1, _a2, _a3) \
    async_urgent_call4(_fn, _a1, _a2, _a3, 0)
#define async_urgent_call4(_fn, _a1, _a2, _a3, _a4) \
    async_urgent_call5(_fn, _a1, _a2, _a3, _a4, 0)        
#define async_urgent_call5(_fn, _a1, _a2, _a3, _a4, _a5) \
    __async_call((async_fn_t)_fn, \
        (uintptr_t)(_a1), \
        (uintptr_t)(_a2), \
        (uintptr_t)(_a3), \
        (uintptr_t)(_a4), \
        (uintptr_t)(_a5), \
        NULL, true)

/*
 * Async-call api with done
 */
#define async_done_call0(_fn, _done) \
    async_done_call1(_fn, _done, 0)
#define async_done_call1(_fn, _done, _a1) \
    async_done_call2(_fn, _done, _a1, 0)
#define async_done_call2(_fn, _done, _a1, _a2) \
    async_done_call3(_fn, _done, _a1, _a2, 0)
#define async_done_call3(_fn, _done, _a1, _a2, _a3) \
    async_done_call4(_fn, _done, _a1, _a2, _a3, 0)
#define async_done_call4(_fn, _done, _a1, _a2, _a3, _a4) \
    async_done_call5(_fn, _done, _a1, _a2, _a3, _a4, 0)        
#define async_done_call5(_fn, _done, _a1, _a2, _a3, _a4, _a5) \
    __async_call((async_fn_t)_fn, \
        (uintptr_t)(_a1), \
        (uintptr_t)(_a2), \
        (uintptr_t)(_a3), \
        (uintptr_t)(_a4), \
        (uintptr_t)(_a5), \
        _done, false)


int __async_call(async_fn_t fn, uintptr_t a1, uintptr_t a2, 
    uintptr_t a3, uintptr_t a4, uintptr_t a5, async_done_t done, bool urgent);

int ___async_call(struct call_param *param, async_fn_t fn, uintptr_t a1, uintptr_t a2, 
    uintptr_t a3, uintptr_t a4, uintptr_t a5, async_done_t done);
    
#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_ASYNC_H_ */
