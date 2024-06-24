/*
 * Copyright 2024 wtcat
 */
 
#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <stdarg.h>
#include <string.h>

#include "basework/os/osapi.h"
#include "basework/os/osapi_fs.h"
#include "basework/errno.h"
#include "basework/lib/string.h"
#include "basework/ilog2.h"
#include "basework/container/kfifo.h"
#include "basework/container/queue.h"
#include "basework/malloc.h"
#include "basework/dev/fifofs.h"


struct fifo_node {
    STAILQ_ENTRY(fifo_node) link;
    os_sem_t rdsem;
    struct kfifo fifo;
    int refcnt;
    char name[];
};

struct fifo_file {
    struct vfs_file fs;
    struct fifo_node *node;
    int flags;
};

STATIC_ASSERT(FIFO_FILE_STRUCT_SIZE == sizeof(struct fifo_file), "");

static STAILQ_HEAD(, fifo_node) fifo_list = 
    STAILQ_HEAD_INITIALIZER(fifo_list);
static os_mutex_t fifo_mtx;

static ssize_t fifo_write_noblock(os_file_t fd, const void *buf, size_t count);
static ssize_t fifo_write_block(os_file_t fd, const void *buf, size_t count);
static ssize_t fifo_read_noblock(os_file_t fd, void *buf, size_t count);
static ssize_t fifo_read_block(os_file_t fd, void *buf, size_t count);

static int fifo_open(os_file_t fd, const char *path, int flags, va_list ap) {
    struct fifo_file *ff = (struct fifo_file *)fd;
    struct fifo_node *fifo;
    int err;

    os_mtx_lock(&fifo_mtx);
    STAILQ_FOREACH(fifo, &fifo_list, link) {
        if (!strcmp(path, fifo->name)) {
            fifo->refcnt++;
            goto _out;
        }
    }
    
    if (flags & VFS_O_CREAT) {
        size_t size = va_arg(ap, size_t);
        size_t namelen;

        if (size == 0 || ((size & (size - 1)) != 0)) {
            err = -EINVAL;
            goto _failed;
        }

        namelen = strlen(path) + 1;
        fifo = general_calloc(1, sizeof(*fifo) + namelen);
        if (fifo == NULL) {
            err = -ENOMEM;
            goto _failed;
        }

        err = kfifo_alloc(&fifo->fifo, size);
        if (err)
            goto _free;

        if (!(flags & VFS_O_NONBLOCK)) {
            err = os_sem_doinit(&fifo->rdsem, 0);
            if (err)
                goto _freefifo;
        }

        strlcpy(fifo->name, path, namelen);
        fifo->refcnt = 1;
        STAILQ_INSERT_TAIL(&fifo_list, fifo, link);
    } else {
        err = -EFAULT;
        goto _freefifo;
    }
    
_out:
    os_mtx_unlock(&fifo_mtx);
    if (flags & VFS_O_NONBLOCK) {
        ff->fs.read = fifo_read_noblock;
        ff->fs.write = fifo_write_noblock;
    } else {
        ff->fs.read = fifo_read_block;
        ff->fs.write = fifo_write_block;
    }
    ff->node = fifo;
    ff->flags = flags;
    return 0;

_freefifo:
    kfifo_free(&fifo->fifo);
_free:
    general_free(fifo);
_failed:
    os_mtx_unlock(&fifo_mtx);
    return err;
}

static int fifo_close(os_file_t fd) {
    struct fifo_file *ff = (struct fifo_file *)fd;
    struct fifo_node *fifo = ff->node;

    os_mtx_lock(&fifo_mtx);
    if (fifo->refcnt > 0) {
        fifo->refcnt--;
        if (fifo->refcnt == 0) {
            kfifo_free(&fifo->fifo);
            general_free(fifo);
        }
    }
    os_mtx_unlock(&fifo_mtx);

    return 0;
}

static ssize_t fifo_read_block(os_file_t fd, void *buf, size_t count) {
    struct fifo_file *ff = (struct fifo_file *)fd;
#ifndef CONFIG_FIFOFS_CHECKER_DISABLE
    if (rte_unlikely((ff->flags & VFS_O_MASK) == VFS_O_WRONLY))
        return -EOPNOTSUPP;
#endif

    struct fifo_node *fifo = ff->node;
    size_t size;

    do {
        size = kfifo_out(&fifo->fifo, buf, count);
        if (size > 0)
            break;
        os_sem_wait(&fifo->rdsem);
    } while (1);

    return size;
}

static ssize_t fifo_read_noblock(os_file_t fd, void *buf, size_t count) {
    struct fifo_file *ff = (struct fifo_file *)fd;

#ifndef CONFIG_FIFOFS_CHECKER_DISABLE
    if (rte_unlikely((ff->flags & VFS_O_MASK) == VFS_O_WRONLY))
        return -EOPNOTSUPP;
#endif
    return kfifo_out(&ff->node->fifo, buf, count);
}

static ssize_t fifo_write_block(os_file_t fd, const void *buf, size_t count) {
    struct fifo_file *ff = (struct fifo_file *)fd;
    size_t size;

#ifndef CONFIG_FIFOFS_CHECKER_DISABLE
    if (rte_unlikely((ff->flags & VFS_O_MASK) == VFS_O_RDONLY))
        return -EOPNOTSUPP;
#endif

    size = kfifo_in(&ff->node->fifo, buf, count);
    os_sem_post(&ff->node->rdsem);

    return size;
}

static ssize_t fifo_write_noblock(os_file_t fd, const void *buf, size_t count) {
    struct fifo_file *ff = (struct fifo_file *)fd;
#ifndef CONFIG_FIFOFS_CHECKER_DISABLE
    if (rte_unlikely((ff->flags & VFS_O_MASK) == VFS_O_RDONLY))
        return -EOPNOTSUPP;
#endif
    return kfifo_in(&ff->node->fifo, buf, count);
}

static ssize_t fifo_write(os_file_t fd, const void *buf, size_t count) {
    struct fifo_file *ff = (struct fifo_file *)fd;

    if (!(ff->flags & VFS_O_NONBLOCK))
        return fifo_write_noblock(fd, buf, count);

    return fifo_write_block(fd, buf, count);
}

static ssize_t fifo_read(os_file_t fd, void *buf, size_t count) {
    struct fifo_file *ff = (struct fifo_file *)fd;

    if (!(ff->flags & VFS_O_NONBLOCK))
        return fifo_read_noblock(fd, buf, count);

    return fifo_read_block(fd, buf, count);
}

static struct file_class fifofs_class = {
	.mntpoint      = "/fifo:",
	.fds_buffer    = NULL,
	.fds_size      = 0,
	.fd_size       = 0,
	.fs_priv       = NULL,

	.open          = fifo_open,
	.close         = fifo_close,
	.ioctl         = NULL,
	.read          = fifo_read,
	.write         = fifo_write,
	.flush         = NULL,
	.lseek         = NULL,
	.truncate      = NULL,
	.opendir       = NULL,
	.readdir       = NULL,
	.closedir      = NULL,
	.mkdir         = NULL,
	.unlink        = NULL,
	.stat          = NULL,
	.rename        = NULL,
	.reset         = NULL,
};

int fifofs_register(struct fifo_filemem fds[], size_t n) {
    os_mtx_init(&fifo_mtx, 0);
    fifofs_class.fds_buffer = fds;
    fifofs_class.fd_size    = sizeof(struct fifo_file);
    fifofs_class.fds_size   = fifofs_class.fd_size * n;
    return vfs_register(&fifofs_class);
}
