/*
 * Copyright 2024 wtcat
 */

#define pr_fmt(fmt) "<mod>: "fmt
#include <errno.h>
#include <string.h>

#include "basework/container/queue.h"
#include "basework/rte_cpu.h"
#include "basework/module.h"
#include "basework/os/osapi.h"
#include "basework/os/osapi_fs.h"
#include "basework/malloc.h"
#include "basework/log.h"

struct module_runtime {
    SLIST_ENTRY(module_runtime) link;
    struct module *mod;
    struct module_class *m_op;
    int refcnt;
    int state;
};

#define MODULE_LOCK_INIT() os_mtx_init(&module_mtx, 0)
#define MODULE_LOCK()      os_mtx_lock(&module_mtx)
#define MODULE_UNLOCK()    os_mtx_unlock(&module_mtx)

static os_mutex_t module_mtx;
static SLIST_HEAD(, module_runtime) module_list = 
    SLIST_HEAD_INITIALIZER(module_list);

static void module_setup(struct module *m) {
    uintptr_t base = (uintptr_t)m;

    /* Relocate addresss */
    m->bss_start += base;
    m->ldata_start += base;
    m->bss_start += base;

    memcpy((void *)m->data_start, (void *)m->ldata_start, 
        m->data_size);
    memset((void *)m->bss_start, 0, m->bss_size);
}

static int module_check(const struct module *m) {
    if (m->magic != MODULE_MAGIC)
        return -EINVAL;

    if (m->cid != MODULE_COMPILER_ID)
        return -EINVAL;

    if (m->load == NULL || m->unload == NULL)
        return -EINVAL;

    if (m->mod_size < sizeof(*m))
        return -EINVAL;

    if (m->mod_size < m->bss_start + m->bss_size)
        return -EINVAL;
    
    return 0;
}

static int __module_load(struct module_runtime *rt, struct module *m, 
    struct module_class *api) {
    int err;

    MODULE_LOCK();
    if (rt->state == MODULE_STATE_LOADED) {
        err = -EBUSY;
        goto _unlock;
    }

    err = 0;
    rt->state = MODULE_STATE_LOADING;
    module_setup(m);
    err = m->load(api);
    if (err)
        goto _unlock;
    
    rt->state = MODULE_STATE_LOADED;
    rt->m_op = api;
    rt->mod = m;
    rt->refcnt = 0;
    SLIST_INSERT_HEAD(&module_list, rt, link);

_unlock:
    MODULE_UNLOCK();
    return err;
}

static int __module_unload(struct module_runtime *rt) {
    struct module *m = rt->mod;
    int state, err = 0;

    if (rt->refcnt > 0) {
        err = -EBUSY;
        goto _unlock;
    }
    
    state = rt->state;
    rt->state = MODULE_STATE_UNLOADING;
    if (m->unload) {
        err = m->unload(rt->m_op);
        if (err) {
            rt->state = state;
            goto _unlock;
        }
    }
    rt->state = MODULE_STATE_IDLE;
    SLIST_REMOVE(&module_list, rt, module_runtime, link);
    general_free(rt->mod);

_unlock:
    return err;
}

int module_load_fromfile(const char *file, struct module_class *api, 
    struct module_runtime **prt) {
    struct module_runtime *rt;
    struct module m, *p;
    struct vfs_stat stat;
    os_file_t fd;
    size_t msize;
    int ret;

    if (file == NULL)
        return -EINVAL;

    ret = vfs_stat(file, &stat);
    if (ret < 0)
        return ret;

    if (stat.st_size < sizeof(m)) 
        return -ENODATA;

    ret = vfs_open(&fd, file, VFS_O_RDONLY);
    if (ret)
        return ret;

    ret = vfs_read(fd, &m, sizeof(m));
    if (ret < 0)
        goto _close;
    
    ret = module_check(&m);
    if (ret < 0)
        goto _close;
    
    msize = RTE_ALIGN(m.mod_size, 4);
    p = general_aligned_alloc(RTE_CACHE_LINE_SIZE, msize + sizeof(*rt));
    if (p == NULL) {
        ret = -ENOMEM;
        goto _close;
    }

    vfs_lseek(fd, 0, VFS_SEEK_SET);
    ret = vfs_read(fd, p, stat.st_size);
    if (ret < 0)
        goto _free;
    
    rt = (struct module_runtime *)((char *)p + msize);
    memset(rt, 0, sizeof(*rt));
    ret = __module_load(rt, p, api);
    if (ret < 0)
        goto _free;

    *prt = rt;
    vfs_close(fd);
    return 0;

_free:
    general_free(p);
_close:
    vfs_close(fd);
    return ret;
}

int module_load_frommem(void *code, size_t size, struct module_class *api, 
    struct module_runtime **prt) {
    struct module *p = (struct module *)code;
    struct module_runtime *rt;
    size_t msize;
    int ret;

    if (p == NULL)
        return -EINVAL;

    if (size < sizeof(*p)) 
        return -ENODATA;

    if (p->mod_size < size)
        return -EINVAL;

    ret = module_check(p);
    if (ret < 0)
        goto _out;
    
    msize = RTE_ALIGN(p->mod_size, 4);
    p = general_aligned_alloc(RTE_CACHE_LINE_SIZE, msize + sizeof(*rt));
    if (p == NULL) {
        ret = -ENOMEM;
        goto _out;
    }

    rt = (struct module_runtime *)((char *)p + msize);
    memcpy(p, code, size);
    memset(rt, 0, sizeof(*rt));
    ret = __module_load(rt, p, api);
    if (ret < 0)
        goto _free;

    *prt = rt;
    return 0;

_free:
    general_free(p);
_out:
    return ret;
}

int moudule_unload(struct module_runtime *rt) {
    int err;

    if (rt == NULL)
        return -EINVAL;

    MODULE_LOCK();
    err = __module_unload(rt);
    MODULE_UNLOCK();
    return err;
}

int module_get(struct module_runtime *rt) {
    MODULE_LOCK();
    rt->refcnt++;
    MODULE_UNLOCK();
    return 0;
}

int module_put(struct module_runtime *rt) {
    int err = 0;

    if (rt == NULL)
        return -EINVAL;

    MODULE_LOCK();
    if (rt->refcnt > 0) {
        rt->refcnt--;
        if (rt->refcnt == 0) {
            err = __module_unload(rt);
            goto _unlock;
        }
    }

_unlock:
    MODULE_UNLOCK();
    return err;
}

int module_init(void) {
    MODULE_LOCK_INIT();
    return 0;
}
