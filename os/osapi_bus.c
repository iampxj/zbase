/*
 * Copyright 2024 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<iobus>: "fmt
#define CONFIG_LOGLEVEL  LOGLEVEL_DEBUG
#include <stdio.h>

#include "basework/rte_atomic.h"
#include "basework/os/osapi.h"
#include "basework/os/osapi_bus.h"
#include "basework/container/kfifo.h"
#include "basework/lib/string.h"
#include "basework/malloc.h"
#include "basework/log.h"

struct iobus_sq {
    os_completion_declare(completion)

    /* Device list */
    struct iobus_device *iodevs;
    int ndevs;

    /* Bus type */
    enum iobus_type type; 

    /* I/O low-level operation */
    const struct iobus_llops *llops;
    void (*bus_transfer)(struct iobus_sqe *sqe);

    /* MP -> SC */
    STRUCT_KFIFO_PTR(struct iobus_sqe *) queue;

    /* Reference count */
    rte_atomic32_t refcnt;

    char name[16];
    os_thread_t thread;
    STAILQ_ENTRY(iobus_sq) link;
};

os_critical_global_declare
static uint16_t iobus_id;
static os_mutex_t iobus_mtx;
static STAILQ_HEAD(, iobus_sq) created_list = 
    STAILQ_HEAD_INITIALIZER(created_list);
static STAILQ_HEAD(, iobus_llops) io_list = 
    STAILQ_HEAD_INITIALIZER(io_list);


static inline void 
iobus_notify(struct iobus_sq *sq, int state) {
    for (int i = 0; i < sq->ndevs; i++) {
        const struct iobus_device *iod = &sq->iodevs[i];
        if (iod->notify)
            iod->notify(iod, state);
    }
}

static void 
iobus_daemon(void *arg) {
    struct iobus_sq *sq = (struct iobus_sq *)arg;

    sq->bus_transfer = sq->llops->transfer;
    pr_dbg("%s daemon is ready\n");

    for ( ; ; ) {
        struct iobus_sqe *sqes[4];
        size_t n;

        if (kfifo_is_empty(&sq->queue)) {
            iobus_notify(sq, IOBUS_DEVSTATE_SUSPEND);
            os_completion_wait(&sq->completion);
            iobus_notify(sq, IOBUS_DEVSTATE_RESUME);
        }
        
        n = kfifo_out(&sq->queue, sqes, rte_array_size(sqes));
        pr_dbg("%s process %d tasks\n", __func__, n);

        for (size_t i = 0; i < n; i++)
            sq->bus_transfer(sqes[i]);
    }
}

static struct iobus_device *
iobus_search(const char *bus) {
    struct iobus_device *idev = NULL;
    struct iobus_sq *sq;

    os_mtx_lock(&iobus_mtx);
    STAILQ_FOREACH(sq, &created_list, link) {
        for (int i = 0; i < sq->ndevs; i++) {
            if (!strcmp(sq->iodevs[i].name, bus)) {
                idev = &sq->iodevs[i];
                break;
            }
        }
    }
    os_mtx_unlock(&iobus_mtx);
    return idev;
}

static struct iobus_llops *
iobus_find_llops(enum iobus_type type) {
    struct iobus_llops *io = NULL;

    os_mtx_lock(&iobus_mtx);
    STAILQ_FOREACH(io, &io_list, next) {
        if (io->type == type)
            break;
    }
    os_mtx_unlock(&iobus_mtx);
    return io;
}

int 
iobus_create(const char *buses[], enum iobus_type type, int qsize, 
    int prio, void *stack, int stacksize) {
    struct iobus_device *iodev;
    struct iobus_llops *io;
    struct iobus_sq *sq;
    size_t sqsize;
    size_t reqsize;
    int err;
    int n;

    if (buses == NULL)
        return -EINVAL;

    for (n = 0; buses[n] != NULL; n++) {
        iodev = iobus_search(buses[n]);
        if (iodev != NULL) {
            pr_err("iobus %s is exist\n", buses[n]);
            return -EBUSY;
        }
    }

    io = iobus_find_llops(type);
    if (io == NULL)
        return -ENODEV;

    sqsize = RTE_ALIGN(sizeof(*sq), sizeof(void *));
    reqsize = sqsize + qsize * sizeof(void *) + n * sizeof(*iodev);
    sq = general_calloc(1, reqsize);
    if (sq == NULL) {
        err = -ENOMEM;
        goto _exit;
    }

    err = kfifo_init(&sq->queue, (char *)sq + sqsize, qsize);
    if (err)
        goto _free_sq;

    sq->iodevs = (void *)((char *)sq + sqsize + qsize * sizeof(void *));
    sq->ndevs = n;
    sq->type = type;
    sq->llops = io;
    for (int i = 0; i < n; i++) {
        strlcpy(sq->iodevs[i].name, buses[i], sizeof(sq->iodevs[i].name));
        io->devopen(buses[i], &sq->iodevs[i].dev);
        sq->iodevs[i].sq  = sq;
        if (sq->iodevs[i].dev == NULL) {
            pr_err("Not found %s device\n", buses[i]);
            goto _free_sq;
        }
    }

    rte_atomic32_init(&sq->refcnt);
    os_completion_reinit(&sq->completion);
    snprintf(sq->name, sizeof(sq->name), "iobus-%d", iobus_id);
    err = os_thread_spawn(&sq->thread, sq->name, stack, 
        stacksize, prio, iobus_daemon, sq);
    if (err) {
        pr_err("create thread failed(%d)\n", err);
        goto _free_sq;
    }

    os_mtx_lock(&iobus_mtx);
    iobus_id++;
    STAILQ_INSERT_TAIL(&created_list, sq, link);
    os_mtx_unlock(&iobus_mtx);

    return 0;

_free_sq:
    general_free(sq);
_exit:
    return err;
}

int 
iobus_destroy(struct iobus_sq *sq) {
    if (sq == NULL)
        return -EINVAL;
    
    if (rte_atomic32_read(&sq->refcnt) > 0)
        return -EBUSY;

    os_thread_destroy(&sq->thread);
    os_mtx_lock(&iobus_mtx);
    STAILQ_REMOVE(&created_list, sq, iobus_sq, link);
    os_mtx_unlock(&iobus_mtx);
    general_free(sq);
    return 0;
}

struct iobus_device *
iobus_request(const char *bus) {
    struct iobus_device *iod;

    if (bus == NULL)
        return NULL;
    iod = iobus_search(bus);
    if (iod)
        rte_atomic32_add(&iod->sq->refcnt, 1);
    return iod;
}

int 
iobus_release(struct iobus_device *iod) {
    if (iod == NULL)
        return -EINVAL;
    
    rte_atomic32_sub(&iod->sq->refcnt, 1);
    return 0;
}

int 
__iobus_burst_submit(struct iobus_sq *sq, struct iobus_sqe **sqes, 
    size_t n) {
    os_critical_declare

    os_critical_lock
    if (rte_likely(kfifo_avail(&sq->queue) >= n)) {
        kfifo_in(&sq->queue, sqes, n);
        os_completed(&sq->completion);
        os_critical_unlock
        return 0;
    }
    os_critical_unlock

    return -ENOMEM;
}

static void 
iobus_done(struct iobus_sqe *sqe) {
    os_completed(sqe->arg);
} 

int 
iobus_burst_submit_wait(struct iobus_device *iodev, struct iobus_sqe **sqes, 
    size_t n) {
    os_completion_declare(completion)
    struct iobus_sq *sq = iodev->sq;
    int err;

    sqes[n - 1]->arg  = &completion;
    sqes[n - 1]->done = iobus_done;
    os_completion_reinit(&completion);
    err = __iobus_burst_submit(sq, sqes, n);
    if (!err)
        err = os_completion_wait(&completion);

    return err;
}

int 
iobus_init(void) {
    os_mtx_init(&iobus_mtx, 0);
    return 0;
}

int 
iobus_register_llops(struct iobus_llops *ops) {
    struct iobus_llops *io;
    int err = 0;

    if (ops == NULL || ops->transfer == NULL ||
        ops->devopen == NULL)
        return -EINVAL;

    os_mtx_lock(&iobus_mtx);
    STAILQ_FOREACH(io, &io_list, next) {
        if (io->type == ops->type) {
            err = -EBUSY;
            goto _unlock;
        }
    }
    STAILQ_INSERT_TAIL(&io_list, ops, next);
_unlock:
    os_mtx_unlock(&iobus_mtx);
    return err;
}

void 
iobus_dump(void) {
    struct iobus_sq *sq;

    pr_out("\n********** iobus information **********\n");
    os_mtx_lock(&iobus_mtx);
    STAILQ_FOREACH(sq, &created_list, link) {
        pr_out("+ %s\n", sq->name);
        for (int i = 0; i < sq->ndevs; i++)
            pr_out("- - - - -%s\n", sq->iodevs[i].name);
    }
    pr_out("\n");
    os_mtx_unlock(&iobus_mtx);
}
