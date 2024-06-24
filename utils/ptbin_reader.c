/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<ptbin_reader>: "fmt
#include <errno.h>

#include "basework/log.h"
#include "basework/malloc.h"
#include "basework/dev/partition.h"
#include "basework/utils/binmerge.h"
#include "basework/utils/ptbin_reader.h"


struct ptbin_handle {
    const struct ptbin_ops *ops;
    void *handle;
    size_t size;
};

static void *local_parttion_open(const char *name) {
    return (void *)disk_partition_find(name);
}

static ssize_t local_partition_read(void *hdl, void *buffer, size_t size, 
    size_t offset) {
    return (ssize_t)lgpt_read(hdl, offset, buffer, size);
}

const struct ptbin_ops _ptbin_local_ops = {
    .open = local_parttion_open,
    .read = local_partition_read
};

int __ptbin_open(const char *name, void **handle, 
    const struct ptbin_ops *ops) {
    struct ptbin_handle *ph;
    struct bin_header bin;
    void *dh;

    if (!name || !ops || !ops->open)
        return -EINVAL;

    dh = ops->open(name);
    if (!dh) {
        pr_err("not found partition(%s)\n", name);
        return -ENODATA;
    }

    ops->read(dh, &bin, sizeof(bin), 0);
    if (bin.magic != FILE_HMAGIC) {
        pr_err("partition binary file data is invalid\n");
        return -EINVAL;
    }
    //TODO: verify CRC

    ph = general_malloc(sizeof(*ph));
    if (!ph) {
        pr_err("no more memory\n");
        return -ENOMEM;
    }

    ph->handle = dh;
    ph->size = bin.size;
    ph->ops = ops;
    *handle = ph;
    return 0; 
}

int ptbin_open(const char *name, void **handle) {
    return __ptbin_open(name, handle, &_ptbin_local_ops);
}

int ptbin_close(void *handle) {
    if (handle) {
        general_free(handle);
        return 0;
    }
    return -EINVAL;
}

size_t ptbin_tell(void *handle) {
    struct ptbin_handle *ph = handle;
    if (!ph)
        return 0;
    return ph->size;
}

ssize_t ptbin_read(void *handle, void *buffer, size_t size, uint32_t offset) {
    struct ptbin_handle *ph = handle;

    if (!handle || !buffer || !ph->ops->read)
        return -EINVAL;

    if (!size)
        return 0;

    return ph->ops->read(ph->handle, buffer, size, 
        offset + sizeof(struct bin_header));
}
