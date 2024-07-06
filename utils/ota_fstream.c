/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "ota_fstream: " fmt
#define CONFIG_LOGLEVEL   LOGLEVEL_DEBUG
#include <errno.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#include "basework/dev/partition.h"
#include "basework/utils/binmerge.h"
#include "basework/malloc.h"
#include "basework/log.h"
#include "basework/utils/ota_fstream.h"
#include "basework/env.h"
#include "basework/lib/crc.h"

struct extract_context {
    int (*state_exec)(struct extract_context *ctx, 
        const void *buf, size_t size);
    int (*prog_notity)(const char *name, int percent);
    unsigned int (*get_value)(const char *key);
    bool (*check_env)(const struct file_header *header);
    const struct ota_fstream_ops *f_ops;
    struct file_header *f_header;
    struct file_node *f_node;
    void *fd;
    uint32_t p_ofs;
    void *buffer;
    size_t size;
    uint32_t h_ofs;
    uint32_t w_ofs;
    uint32_t writen;
    int percent;
    int err;
};

#define USE_DISKLOG

#ifdef USE_DISKLOG
# define LOG_REDIRECT disk
#else
# define LOG_REDIRECT default
#endif

#define BUF_SIZE 0
#define MIN(a, b) ((a) < (b)? (a): (b))

static int read_fileheader_state(struct extract_context *ctx, 
    const void *buf, size_t size);
static int write_new_file(struct extract_context *ctx, 
    const void *buf, size_t size);
static int write_file_state(struct extract_context *ctx,
    const void *buf, size_t size);
static int ota_default_notify(const char *name, int percent);

RTE_HOOK_INSTANCE(ota_finish, )
struct extract_context sm_context = {
    .state_exec = read_fileheader_state,
    .prog_notity = ota_default_notify
};

static int ota_default_notify(const char *name, int percent) {
	pr_out("<ota>: %s -> %d%%\n", name, percent);
	return 0;
}

static unsigned int ota_get_value(struct extract_context *ctx, 
    const char *key) {
    if (ctx->get_value)
        return ctx->get_value(key);
    return 0;
}

static void ota_file_notify(struct extract_context *ctx, 
    uint32_t bytes) {
    if (ctx->prog_notity) {
        int percent;
        ctx->writen += bytes;
        percent = (ctx->writen * 100) / ctx->f_header->size;
        if (percent != ctx->percent) {
            ctx->prog_notity(ctx->f_node->f_name, percent);
            ctx->percent = percent;
        }
    }
}

static int open_file_partition(struct extract_context *ctx, 
    const char *name) {
    if (ctx->f_ops->open) {
        ctx->fd = ctx->f_ops->open(name);
        if (ctx->fd)
            return 0;
        return -ENODATA;
    }
    return -EINVAL;
}

static void close_file_partition(struct extract_context *ctx) {
    if (ctx->f_ops->close) {
        if (ctx->fd) {
            if (ctx->f_ops->completed) {
				pr_info("%s: name(%s) size(%d)\n", __func__, ctx->f_node->f_name, 
					ctx->f_node->f_size);
                ctx->f_ops->completed(ctx->err, ctx->fd, ctx->f_node->f_name, 
                    ctx->f_node->f_size);
            }
            ctx->f_ops->close(ctx->fd);
            ctx->fd = NULL;
        }
    }
}

static int write_partition_file(struct extract_context *ctx, 
    void *fd, const void *buf, size_t size, uint32_t offset) {
    if (ctx->f_ops->write) {
        ota_file_notify(ctx, size);
        return ctx->f_ops->write(fd, buf, size, offset);
    }
    return -EINVAL;
}

static int ota_file_check(struct extract_context *ctx) {
    struct file_header *h = ctx->f_header;
    uint32_t devid = ota_get_value(ctx, "devid");
    int err = -EINVAL;

    if (ota_get_value(ctx, "devid") != h->devid) {
        npr_err(default, "Error***: The device(%d) does not support this ota-firmware(%d)!\n", 
            devid, h->devid);
        return -EINVAL;
    }

    if (ctx->check_env) {
        if (!ctx->check_env(h)) {
            npr_err(disk, "Error***: OTA package does not match current firmware\n");
            return -EINVAL;
        }
    }

    if (h) {
        npr_info(default, "ota file header: magic(0x%x) crc(0x%x) size(%d) num(%d)\n",
            h->magic, h->crc, h->size, h->nums);
        npr_info(LOG_REDIRECT, "ota file header: magic(0x%x) crc(0x%x) size(%d) num(%d)\n",
            h->magic, h->crc, h->size, h->nums);
        for (size_t i = 0; i < h->nums; i++) {
            npr_info(default, " => file(%s) offset(%d) size(%d)\n", 
                h->headers[i].f_name,
                h->headers[i].f_offset,
                h->headers[i].f_size);
            npr_info(LOG_REDIRECT, " => file(%s) offset(%d) size(%d)\n", 
                h->headers[i].f_name,
                h->headers[i].f_offset,
                h->headers[i].f_size);

            err = open_file_partition(ctx, h->headers[i].f_name);
            if (err) {
                npr_err(default, "invalid file(%s)\n", h->headers[i].f_name);
                npr_err(LOG_REDIRECT, "invalid file(%s)\n", h->headers[i].f_name);
                break;
            }
        }
    }
    return err;
}

static inline int run_state_machine(struct extract_context *ctx, 
    int (*state)(struct extract_context *, const void *, size_t),
    const void *buf, 
    size_t size) {
    ctx->state_exec = state;
    return ctx->state_exec(ctx, buf, size);
}

static void reset_state_machine(struct extract_context *ctx) {
    const struct ota_fstream_ops *f_ops = ctx->f_ops;
    if (ctx->f_header) {
        pr_dbg("ota free memory\n");
        general_free(ctx->f_header);
    }
    memset(&ctx->f_header, 0, 
        sizeof(*ctx) - offsetof(struct extract_context, f_header));
    ctx->state_exec = read_fileheader_state;
    ctx->f_ops = f_ops;
}

static int end_state(struct extract_context *ctx) {
    int err = ctx->err;

    if (ctx->state_exec != read_fileheader_state) {
        RTE_HOOK(ota_finish, ctx->f_header, err);
        close_file_partition(ctx);
        reset_state_machine(ctx);
    }
    assert(ctx->f_header == NULL);
    pr_dbg("ota write end with error(%d)\n", err);
    return err;
}

static int read_fileheader_state(struct extract_context *ctx, 
    const void *buf, size_t size) {
    if (ctx->h_ofs == 0) {
        const struct file_header *fh;
        size_t hsize, bytes;

        if (size < sizeof(struct file_header)) {
            npr_err(LOG_REDIRECT, "buffer size is too small\n");
            return -EINVAL;
        }

        fh = (const struct file_header *)buf;
        if (fh->magic != FILE_HMAGIC) {
            npr_err(LOG_REDIRECT, "Invalid file\n");
            return -EINVAL;
        }
        hsize = fh->nums * sizeof(struct file_node) + sizeof(struct file_header);
        if (size < hsize) {
            npr_err(LOG_REDIRECT, "buffer size is less than the size of file_head\n");
            return -EINVAL;
        }
        
        ctx->f_header = general_malloc(hsize + BUF_SIZE);
        if (!ctx->f_header) {
            npr_err(LOG_REDIRECT, "no more memory\n");
            return -ENOMEM;
        }

        ctx->f_node = ctx->f_header->headers;
        ctx->buffer = (char *)ctx->f_header + hsize;
        ctx->w_ofs = 0;
        bytes = MIN(size, hsize);
        memcpy(ctx->f_header, buf, bytes);
        ctx->h_ofs += bytes;
        size -= bytes;

        if (ota_file_check(ctx)) {
            npr_err(LOG_REDIRECT, "file check failed\n");
            reset_state_machine(ctx);
            return -ENOENT;
        }
        if (size > 0) {
            ctx->f_node--;
            return run_state_machine(ctx, write_new_file, 
                (char *)buf + bytes, size);
        }
    }
    npr_err(LOG_REDIRECT, "ota file header is invalid\n");
    return -EINVAL;
}

static int write_new_file(struct extract_context *ctx, 
    const void *buf, size_t size) {
	size_t remain;
	int ofs;

_loop:	
    close_file_partition(ctx);
    ctx->f_node++;
    pr_notice("Create file (%s) ctx->f_node(%p),f_size(%d),p_ofs(%d) size(%d)\n", 
        ctx->f_node->f_name, ctx->f_node,ctx->f_node->f_size, ctx->p_ofs, size);
    	
    if (open_file_partition(ctx, ctx->f_node->f_name)) {
        npr_err(default, "Create file partition(%s) failed\n", 
            ctx->f_node->f_name);
        npr_err(LOG_REDIRECT, "Create file partition(%s) failed\n", 
            ctx->f_node->f_name);
        ctx->err = -ENODATA;
        return end_state(ctx);
    }
		
    if (size > ctx->f_node->f_size) {
        remain = size - ctx->f_node->f_size;
        size = ctx->f_node->f_size;
    } else {
        remain = 0;
    }
	
    ctx->p_ofs = 0;
    ofs = write_partition_file(ctx, ctx->fd, buf, size, ctx->p_ofs);
    if (ofs > 0) {
        assert(ofs == (int)size);
        if (remain > 0) {
            if (ctx->f_node - ctx->f_header->headers >= 
                (int)ctx->f_header->nums - 1) {
                pr_err("Data size does not match the header\n");
                ofs = -EFBIG;
                goto _failed;
            }
            buf = (const char *)buf + size;
            size = remain;
            goto _loop;
        }
        ctx->p_ofs += ofs;
        ctx->state_exec = write_file_state;
        return 0;
    }

_failed:
    npr_err(LOG_REDIRECT, "%s write failed\n", __func__);
    ctx->err = ofs;
    return end_state(ctx);
}

static int write_file_state(struct extract_context *ctx,
    const void *buf, size_t size) {
    int ofs;

    /* If need create new file */
    if (ctx->p_ofs + size > ctx->f_node->f_size) {
        size_t bytes = ctx->f_node->f_size - ctx->p_ofs;
        ofs = write_partition_file(ctx, ctx->fd, buf, bytes, ctx->p_ofs);
        if (ofs < 0) {
            assert(ofs == (int)bytes);
            ctx->err = ofs;
            return end_state(ctx);
        }
        size -= bytes;
        return run_state_machine(ctx, write_new_file, 
            (char *)buf + bytes, size);
    }

    ofs = write_partition_file(ctx, ctx->fd, buf, size, ctx->p_ofs);
    if (ofs > 0) {
        assert(ofs == (int)size);
        ctx->p_ofs += ofs;

        /* Completed */
        if (ctx->p_ofs == ctx->f_node->f_size &&
            (ctx->f_node - ctx->f_header->headers) == (int)ctx->f_header->nums - 1) {
            close_file_partition(ctx);
            pr_dbg("Write finished\n");
            return end_state(ctx);
        }
        return 0;
    }

    npr_err(LOG_REDIRECT, "%s write failed\n", __func__);
    ctx->err = ofs;
    return end_state(ctx);
}

int ota_fstream_set_ops(const struct ota_fstream_ops *ops) {
    if (ops) {
        if (!ops->open || !ops->write)
            return -EINVAL;
        sm_context.f_ops = ops;
        return 0;
    }
    return -EINVAL;
}

int ota_fstream_set_notify(int (*notify)(const char *, int)) {
    if (notify) {
        sm_context.prog_notity = notify;
        return 0;
    }
    return -EINVAL; 
}

int ota_fstream_set_kvfn(unsigned int (*getv)(const char *)) {
    if (getv) {
        sm_context.get_value = getv;
        return 0;
    }
    return -EINVAL; 
}

int ota_fstream_set_envchecker(bool (*check_env)(const struct file_header *header)) {
    if (check_env) {
        sm_context.check_env = check_env;
        return 0;
    }
    return -EINVAL; 
}

int ota_fstream_write(const void *buf, size_t size) {
    if (!sm_context.f_ops) {
        npr_err(LOG_REDIRECT, "invalid file operation\n");
        return -EINVAL;
    }
    return sm_context.state_exec(&sm_context, buf, size);
}

void ota_fstream_finish(void) {
    end_state(&sm_context);
}

uint32_t ota_fstream_get_devid(void) {
    return ota_get_value(&sm_context, "devid");
}
