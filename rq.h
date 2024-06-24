/*
 * Copyright 2022 wtcat
 */
#ifndef BASEWORK_RQ_H_
#define BASEWORK_RQ_H_

#include "basework/os/osapi.h"
#include "basework/container/list.h"

#ifdef __cplusplus
extern "C"{
#endif

// #define USE_RQ_DEBUG

typedef void (*rq_exec_t)(void *);

struct rq_context {
    struct rte_list frees;   /* Free list */
    struct rte_list pending; /* Pending list */
    os_completion_declare(completion)
    void *buffer;
    rq_exec_t fn_executing;
    uint16_t nq;
    uint16_t nr; /* queued count */
    os_thread_t thread;
};

struct rq_node {
    struct rte_list node;
    rq_exec_t exec; /* User function */
    void *arg;
    void **refp;
#ifdef USE_RQ_DEBUG
	union {
		const char *name;
		const void *pfn;
	};
#endif
    bool dofree;
};

#define RQ_BUFFER_SIZE(_nitem, _itemsize) \
    ((_nitem) * rte_roundup(sizeof(struct rq_node) + (_itemsize), 4))

#define RQ_BUFFER_DEFINE(_name, _nitem, _attr) \
    char _name[RQ_BUFFER_SIZE(_nitem, sizeof(struct rq_node))] __rte_aligned(4) _attr; \

#define RQ_NODE_DEFINE(_name, _fn, _arg) \
    struct rq_node _name = {\
        .exec = _fn, \
        .arg = _arg, \
    }

/*
 * RQ_DEFINE - Define a runqueue context
 *
 * @_name: Object name
 * @_nitem: The maximum length of queue
 * @_itemsize: The maximum size of queue-item
 * @_attr: variable attribute
 */
#define RQ_DEFINE(_name, _nitem, _attr) \
    static RQ_BUFFER_DEFINE(_name ## _buffer, _nitem, _attr); \
    struct rq_context _name __rte_used _attr = {    \
        .buffer = _name ## _buffer,   \
        .nq = _nitem,  \
    }

extern struct rq_context *_system_rq;

/*
 * _rq_static_init - Initialze run-queue context that defined by macro 'RQ_DEFINE()'
 *
 * @rq: run-queue context
 */
void _rq_static_init(struct rq_context *rq);

/*
 * _rq_init - Initialze run-queue context that defined by user
 *
 * @rq: Run-queue context
 * @buffer: Queue buffer
 * @size: The size of queue buffer
 * return 0 if success
 */
int _rq_init(struct rq_context *rq, void *buffer, size_t size);
    
/*
 * _rq_node_delete - Delete runq node (just only for timer component)
 *
 * @pnode: run queue node
 * return 0 if success
 */
void _rq_node_delete(struct rq_context *rq, struct rq_node *pnode);

/*
 * _rq_submit - Submit a work-entry to run-queue
 *
 * @rq: Run-queue context
 * @rn: work entry
 * @prepend: urgent call
 * return 0 if success
 */
int _rq_submit_entry(struct rq_context *rq, struct rq_node *rn, bool prepend);

/*
 * _rq_submit - Submit a user-work to run-queue
 *
 * @rq: Run-queue context
 * @exec: user function
 * @data: user parameter
 * @prepend: urgent call
 * return 0 if success
 */
#ifdef USE_RQ_DEBUG
int _rq_submit(struct rq_context *rq, void (*exec)(void *), void *arg, 
	bool prepend, const void *pfn);
#else
int _rq_submit(struct rq_context *rq, void (*exec)(void *), void *arg, 
	bool prepend);
#endif

/*
 * _rq_submit_ext - Submit a user-work to run-queue
 *
 * @rq: Run-queue context
 * @exec: user function
 * @data: user parameter
 * @prepend: urgent call
 * @pnode: queue node
 * return 0 if success
 */
#ifdef USE_RQ_DEBUG
int _rq_submit_ext(struct rq_context *rq, void (*exec)(void *), void *arg, 
    bool prepend, void **pnode, const void *pfn);
#else
int _rq_submit_ext(struct rq_context *rq, void (*exec)(void *), void *arg, 
    bool prepend, void **pnode);
#endif

/*
 * _rq_submit_with_copy - Submit a work and copy buffer
 *
 * @rq: Run-queue context
 * @exec: user function
 * @data: buffer address
 * @size: buffer size
 * @prepend: urgent call
 * return 0 if success
 */
#ifdef USE_RQ_DEBUG
int _rq_submit_with_copy(struct rq_context *rq, void (*exec)(void *, size_t), 
	void *data, size_t size, bool prepend, const void *pfn);
#else
int _rq_submit_with_copy(struct rq_context *rq, void (*exec)(void *, size_t), 
	void *data, size_t size, bool prepend);
#endif


/*
 * _rq_schedule - Schedule run-queue (Asynchronous execute user function).
 * This should be called in a task context
 *
 * @rq: Run-queue context
 */
void _rq_schedule(struct rq_context *rq);

/*
 * _rq_new_thread - Create a new run-queue
 *
 * @rq: run queue context
 * @stack: thread stack address
 * @size: thread stack size
 * @prio: thread priority
 * return 0 if success
 */
int _rq_new_thread(struct rq_context *rq, void *stack, size_t size, int prio);

/*
 * _rq_change_prio - Change thread priority for runqueue
 *
 * @rq: run queue context
 * @prio: thread priority
 * return 0 if success
 */
int _rq_change_prio(struct rq_context *rq, int prio);

/*
 * _rq_cancel_thread - Aboart a run-queue thread
 *
 * @rq: run queue context
 * return 0 if success
 */
int _rq_cancel_thread(struct rq_context *rq);

/*
 * _rq_visitor - Walk around schedule queue
 *
 * @rq: run queue context
 * @iterator: iterator callback function
 * @arg: user data
 */
void _rq_visitor(struct rq_context *rq, 
	void (*iterator)(struct rq_node *n, void *arg), void *arg);

/*
 * _rq_dump - Dump Runqueue information
 *
 * @rq: run queue context
 */
void _rq_dump(struct rq_context *rq);

/*
 * Default run-queue public interface
 */
static inline int rq_init(struct rq_context *rq) {
    if (!rq || !rq->buffer || !rq->nq)
        return -EINVAL;
    _rq_static_init(rq);
    if (!_system_rq)
        _system_rq = rq;
    return 0;
}

/*
 * Submit a user work to run-queue and pass argument by value.
 * That is mean to the data will be copy
 */
#ifdef USE_RQ_DEBUG
# define rq_submit_cp(_exec, _data, _size) \
	_rq_submit_with_copy(_system_rq, _exec, _data, _size, false, (void *)#_exec)
# define rq_submit_urgent_cp(_exec, _data, _size) \
	_rq_submit_with_copy(_system_rq, _exec, _data, _size, true, (void *)#_exec)
#else
# define rq_submit_cp(_exec, _data, _size) \
	_rq_submit_with_copy(_system_rq, _exec, _data, _size, false)
# define rq_submit_urgent_cp(_exec, _data, _size) \
	_rq_submit_with_copy(_system_rq, _exec, _data, _size, true)
#endif


/*
 * Submit a user work to run-queue and pass argument by pointer
 */
#ifdef USE_RQ_DEBUG
# define rq_submit_arg(_exec, _data, _name_or_address) \
	_rq_submit(_system_rq, _exec, _data, false, (const void *)_name_or_address)
# define rq_submit(_exec, _data) \
	_rq_submit(_system_rq, _exec, _data, false, (void *)#_exec)
# define rq_submit_urgent(_exec, _data, ...) \
	_rq_submit(_system_rq, _exec, _data, true, (void *)#_exec)

# define rq_submit_ext_arg(_exec, _data, _pnode, _name_or_address) \
    _rq_submit_ext(_system_rq, _exec, _data, false, &(_pnode), (const void *)_name_or_address)
# define rq_submit_ext(_exec, _data, _pnode) \
	_rq_submit_ext(_system_rq, _exec, _data, false, &(_pnode), (void *)#_exec)
# define rq_submit_ext_urgent(_exec, _data, _pnode, ...) \
	_rq_submit_ext(_system_rq, _exec, _data, true, &(_pnode), (void *)#_exec)

#else
# define rq_submit_arg rq_submit
# define rq_submit(_exec, _data) \
	_rq_submit(_system_rq, _exec, _data, false)
# define rq_submit_urgent(_exec, _data) \
	_rq_submit(_system_rq, _exec, _data, true)

# define rq_submit_ext_arg(_exec, _data, _pnode, ...) \
    _rq_submit_ext(_system_rq, _exec, _data, false, &(_pnode))
# define rq_submit_ext(_exec, _data, _pnode) \
	_rq_submit_ext(_system_rq, _exec, _data, false, &(_pnode))
# define rq_submit_ext_urgent(_exec, _data, _pnode, ...) \
	_rq_submit(_system_rq, _exec, _data, true, &(_pnode))

#endif /* USE_RQ_DEBUG */

#define rq_submit_entry(_rn) \
    _rq_submit_entry(_system_rq, _rn, false)
#define rq_submit_entry_urgent(_rn) \
    _rq_submit_entry(_system_rq, _rn, true)


static inline size_t _rq_get_nodes(struct rq_context *rq) {
    return rq->nr;
}

static inline void rq_schedule(void) {
    _rq_schedule(_system_rq);
}

static inline void* _rq_current_executing(struct rq_context *ctx) {
    return (void *)ctx->fn_executing;
}

static inline void _rq_set_executing(struct rq_context *ctx, void *ptr) {
    ctx->fn_executing = (rq_exec_t)ptr;
}

static inline void *rq_current_executing(void) {
    return _rq_current_executing(_system_rq);
}

static inline void rq_set_executing(void *ptr) {
    _rq_set_executing(_system_rq, ptr);
}

static inline int rq_change_prio(int prio) {
    return _rq_change_prio(_system_rq, prio);
}

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_RQ_H_ */
