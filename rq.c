/*
 * Copyright 2022 wtcat
 * 
 * The simple async-queue implement (rq: run-queue)
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<rq>: "fmt

#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>

#define __USE_GNU
#include "basework/hook.h"
#include "basework/rq.h"
#include "basework/container/list.h"
#include "basework/malloc.h"
#include "basework/log.h"

struct iter_param {
    int idx;
};
#ifdef _WIN32
#pragma comment(lib,"pthreadVC2.lib")
#endif

#define RQ_PANIC(_rq, _msg) \
do { \
    struct iter_param __param = {0}; \
    pr_err("%s caller(%p) rq_executing(%p)\n", _msg, \
        __builtin_return_address(0), (_rq)->fn_executing); \
    _rq_visitor(rq, _rq_dump_iterator, &__param); \
    os_panic(); \
} while (0)

#define BUFFER_HDR_SIZE(sz) ((sz) + sizeof(struct buffer_hdr))

struct buffer_hdr {
    struct rq_context *rq;
    void (*cb)(void *arg, size_t);
    size_t len;
    char data[];
};


os_critical_global_declare
struct rq_context *_system_rq;

void RTE_WEAKHOOK(_rq_execute_prepare, struct rq_context *rq) {
    (void) rq;
}

void RTE_WEAKHOOK(_rq_execute_post, struct rq_context *rq) {
    (void) rq;
}

static void _rq_dump_iterator(struct rq_node *n, void *arg) {
    struct iter_param *param = arg;
#ifdef USE_RQ_DEBUG
    const char *p = n->name;

	if (!isprint((int)p[0]) ||
		!isprint((int)p[1]) ||
		!isprint((int)p[2]) ||
		!isprint((int)p[3])) 
		pr_notice("[%d] %p\n", param->idx, n->pfn);
	else
		pr_notice("[%d] %s<%p>\n", param->idx, n->name, n->pfn);
#else
    pr_notice("[%d] exec<%p>\n", param->idx, n->exec);
#endif
	param->idx++;
}

static void exec_adaptor_cb(void *arg) {
    struct buffer_hdr *bh = (struct buffer_hdr *)arg;
    _rq_set_executing(bh->rq, bh->cb);
    bh->cb(bh->data, bh->len);
    pr_dbg("free buffer: %p\n", bh);
    general_free(bh);
}

void _rq_static_init(struct rq_context *rq) {
    struct rq_node *p = (struct rq_node *)rq->buffer;

    RTE_INIT_LIST(&rq->frees);
    RTE_INIT_LIST(&rq->pending);
    os_completion_reinit(&rq->completion);
    for (int i = 0; i < rq->nq; i++) {
        rte_list_add_tail(&p->node, &rq->frees);
        p = (struct rq_node *)((char *)p + sizeof(struct rq_node));
    }
}

int _rq_init(struct rq_context *rq, void *buffer, size_t size) {
    if (!rq || !buffer || ((uintptr_t)buffer & 3))
        return -EINVAL;

    if (size < sizeof(struct rq_node))
        return -EINVAL;

    rq->nq = (uint16_t)(size / sizeof(struct rq_node));
    rq->buffer = buffer;
    _rq_static_init(rq);
    return 0;
}

static inline void rq_put_locked(struct rq_context *rq, 
    struct rq_node *rn) {
    rte_list_add(&rn->node, &rq->frees);
}

static struct rq_node *rq_get_first_locked(struct rte_list *head) {
    if (!rte_list_empty(head)) {
        struct rq_node *rn = rte_container_of(head->next, struct rq_node, node);
        rte_list_del(&rn->node);
        return rn;
    }
    return NULL;
}

static void rq_add_node_locked(struct rq_context *rq, struct rq_node *rn, 
    bool prepend) {
    int need_wakeup;
    /*
     * When the pending list is empty that the run-queue is sleep,
     * So we need to wake up it at later.
     */
    need_wakeup = rte_list_empty(&rq->pending);
    if (prepend)
        rte_list_add(&rn->node, &rq->pending);
    else
        rte_list_add_tail(&rn->node, &rq->pending);
    rq->nr++;
    /* We will wake up the schedule queue if necessary */
    if (need_wakeup)
        os_completed(&rq->completion);
} 

#ifdef USE_RQ_DEBUG
int _rq_submit_with_copy(struct rq_context *rq, void (*exec)(void *, size_t), 
    void *data, size_t size, bool prepend, const void *pfn) {
#else
int _rq_submit_with_copy(struct rq_context *rq, void (*exec)(void *, size_t), 
    void *data, size_t size, bool prepend) {
#endif
    assert(rq != NULL);
    assert(exec != NULL);
    os_critical_declare
    struct buffer_hdr *bh;
    struct rq_node *rn;

    pr_dbg("allocate %d bytes\n", BUFFER_HDR_SIZE(size));
    bh = general_malloc(BUFFER_HDR_SIZE(size));
    if (rte_unlikely(!bh)) {
        RQ_PANIC(rq, "*** no more memory! ***");
        return -ENOMEM;
    }
    pr_dbg("allocated buffer: %p\n", bh);
    memcpy(bh->data, data, size);
    bh->rq = rq;
    bh->cb = exec;
    bh->len = size;

    os_critical_lock

    /* Allocate a new rq-node from free list */
    rn = rq_get_first_locked(&rq->frees);
    if (rte_unlikely(!rn)) {
        os_critical_unlock
        general_free(bh);
        RQ_PANIC(rq, "*** rq-scheduler queue is full! ***");
        return -EBUSY;
    }

    rn->exec = exec_adaptor_cb;
    rn->arg = bh;
    rn->refp = NULL;
    rn->dofree = true;
#ifdef USE_RQ_DEBUG
	rn->pfn = pfn;
#endif

    rq_add_node_locked(rq, rn, prepend); 
    os_critical_unlock
    return 0;
}

int _rq_submit_entry(struct rq_context *rq, struct rq_node *rn, 
    bool prepend) {
    assert(rq != NULL);
    assert(rn != NULL);
    os_critical_declare

    os_critical_lock
    if (!rn->exec) {
        os_critical_unlock
        return -EINVAL;
    }
    if (rn->node.next == NULL)
        rq_add_node_locked(rq, rn, prepend);
    rn->dofree = false;
    os_critical_unlock
    return 0;
}

#ifdef USE_RQ_DEBUG
int _rq_submit(struct rq_context *rq, void (*exec)(void *), void *arg, 
    bool prepend, const void *pfn) {
#else
int _rq_submit(struct rq_context *rq, void (*exec)(void *), void *arg, 
    bool prepend) {
#endif
    assert(rq != NULL);
    assert(exec != NULL);
    os_critical_declare
    struct rq_node *rn;

    os_critical_lock

    /* Allocate a new rq-node from free list */
    rn = rq_get_first_locked(&rq->frees);
    if (!rn) {
        os_critical_unlock
        RQ_PANIC(rq, "*** rq-scheduler queue is full! ***");
        return -EBUSY;
    }

    rn->exec = exec;
    rn->arg = arg;
    rn->refp = NULL;
    rn->dofree = true;
#ifdef USE_RQ_DEBUG
	rn->pfn = pfn;
#endif
    rq_add_node_locked(rq, rn, prepend); 
    os_critical_unlock

    return 0;
}

#ifdef USE_RQ_DEBUG
int _rq_submit_ext(struct rq_context *rq, void (*exec)(void *), void *arg, 
    bool prepend, void **pnode, const void *pfn) {
#else
int _rq_submit_ext(struct rq_context *rq, void (*exec)(void *), void *arg, 
    bool prepend, void **pnode) {
#endif

    assert(rq != NULL);
    assert(exec != NULL);
    assert(pnode != NULL);
    os_critical_declare
    struct rq_node *rn;

    os_critical_lock

    /* Allocate a new rq-node from free list */
    rn = rq_get_first_locked(&rq->frees);
    if (!rn) {
        os_critical_unlock
        RQ_PANIC(rq, "*** rq-scheduler queue is full! ***");
        return -EBUSY;
    }

    rn->exec = exec;
    rn->arg = arg;
#ifdef USE_RQ_DEBUG
	rn->pfn = pfn;
#endif
    rn->dofree = true;
    rn->refp = pnode;
    *pnode = rn;
    rq_add_node_locked(rq, rn, prepend);
    os_critical_unlock

    return 0;
}

void _rq_node_delete(struct rq_context *rq, struct rq_node *pnode) {
    os_critical_declare

    os_critical_lock
    if (pnode->refp) {
        *pnode->refp = NULL;
        pnode->refp = NULL;
        rq->nr--;
        rte_list_del(&pnode->node);
        rq_put_locked(rq, pnode);
    }
    os_critical_unlock
    pr_dbg("_rq_node_delete: %p caller(%p)\n", pnode, __builtin_return_address(0));
}

void _rq_schedule(struct rq_context *rq) {
    os_critical_declare
    struct rq_node *rn;
    rq_exec_t fn;
    void *arg;
    bool dofree;

    os_critical_lock

    for ( ; ; ) {
        /* Get the first rq-node from pending list */
        rn = rq_get_first_locked(&rq->pending);
        if (rte_unlikely(!rn)) {
            os_critical_unlock
            /*
             * Now, The queue is empty and we have no more work to do, So we
             * trigger task schedule and switch to other task.
             */
            os_completion_wait(&rq->completion);
            os_critical_lock
            continue;
        }
        rq->nr--;
        if (rn->refp) {
            *rn->refp = NULL;
            rn->refp = NULL;
        }
        fn = rn->exec;
        arg = rn->arg;
        dofree = rn->dofree;
        os_critical_unlock

        /* Executing user function */
        _rq_set_executing(rq, fn);
        _rq_execute_prepare(rq);
        fn(arg);
        _rq_execute_post(rq);
        _rq_set_executing(rq, NULL);

        os_critical_lock
   
        /* Free rn-node to free list */
        if (rte_likely(dofree))
            rq_put_locked(rq, rn);
    }
    os_critical_unlock
}

/*
 * The service thread of run-queue
 */
static void rq_main_thread(void *arg) {
    struct rq_context *rq = arg;
    
    _rq_schedule(rq);

    os_thread_exit();
}

int _rq_new_thread(struct rq_context *rq, void *stack, size_t size, int prio) {
    if (!rq || !rq->buffer || !rq->nq)
        return -EINVAL;

    rq_init(rq);
    return os_thread_spawn(&rq->thread, "RunQueue", stack, size, 
        prio, rq_main_thread, rq);
}

int _rq_cancel_thread(struct rq_context *rq) {
#ifndef _WIN32
    if (!rq)
        return -EINVAL;
#endif
    os_thread_destroy(&rq->thread);
    return 0;
}

int _rq_change_prio(struct rq_context *rq, int prio) {
    return os_thread_change_prio(&rq->thread, prio, NULL);
}

void __rte_notrace _rq_visitor(struct rq_context *rq, 
	void (*iterator)(struct rq_node *n, void *arg), void *arg) {
	os_critical_declare
	struct rte_list head, *p;

	if (!rq || !iterator)
		return;

	RTE_INIT_LIST(&head);
	os_critical_lock
	if (rte_list_empty(&rq->pending)) {
		os_critical_unlock
		return;
	}
	rte_list_splice_tail_init(&rq->pending, &head);
	os_critical_unlock
	
	rte_list_foreach(p, &head) {
		struct rq_node *rn = rte_container_of(p, struct rq_node, node);
		iterator(rn, arg);
	}

	os_critical_lock
	bool wakeup = rte_list_empty(&rq->pending);
	rte_list_splice(&head, &rq->pending);
	os_critical_unlock
	if (wakeup)
		os_completed(&rq->completion);
}

void __rte_notrace _rq_dump(struct rq_context *rq) {
    struct iter_param __param = {0};
    pr_err("rq_executing(%p)\n", rq->fn_executing);
    _rq_visitor(rq, _rq_dump_iterator, &__param);
}
