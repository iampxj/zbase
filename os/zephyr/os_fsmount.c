/*
 * Copyright 2025 wtcat
 */

#include <ctype.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <kernel.h>
#include <fs/fs.h>
#ifdef CONFIG_FILE_SYSTEM_LITTLEFS
#include <fs/littlefs.h>
#endif
#include <fx_api.h>
#include <drivers/flash.h>

#include <partition/partition.h>
#include <mem_manager.h>

#define USE_CUSTOM_PARTITION 0

struct dev_options {
    const struct device *dev;
    off_t  offset;
    size_t len;
};

struct fs_devopt {
    struct dev_options base;
    void   *rawbuf;
    size_t  rawbufsz;
};

struct fs_node {
    struct fs_mount_t  fs;
    struct dev_options opt;
    struct fs_node    *next;
    int (*fs_flush)(struct fs_node *node);
};

#define foreach_fs_mounted(pnode) \
    for (struct fs_node **pnode = &fs_manager_list; \
        (*pnode) != NULL; \
        pnode = &(*pnode)->next)

#define ALIGNED_UP_ADD(p, size, align) \
	(typeof(p))(((uintptr_t)p + size + align - 1) & ~(align - 1))
#define FSLOG(fmt, ...) printk("[fs_mount]: "fmt, ##__VA_ARGS__)

static K_MUTEX_DEFINE(fs_manager_mutex);
static struct fs_node *fs_manager_list;

static inline void 
media_pages_layout(const struct device *dev, 
    const struct flash_pages_layout **layout, 
    size_t *layout_size) {
	const struct flash_driver_api *api =
		(const struct flash_driver_api *)dev->api;
    if (api->page_layout)
	    api->page_layout(dev, layout, layout_size);
}

static bool 
parse_param(const char *cfg, const char *key, char *dst, 
    size_t maxsize, unsigned long *pval) {
    const char *src = strstr(cfg, key);
    
    if (src != NULL) {
        char *pdst = dst;

        src += strlen(key);
        while (*src && *src == ' ') src++;

        while (maxsize > 1 && *src && *src != ' ') {
            *pdst++ = *src++;
            maxsize--;
        }
        if (pdst - dst > 0) {
            *pdst = '\0';
            if (isdigit((int)(*dst)) && pval)
                *pval = strtoul(dst, NULL, 0);
        }
        return true;
    }
    return false;
}

static int 
parse_device_options(const char *options, struct fs_devopt *ptr) {
    char buffer[64];
    unsigned long value;

    FSLOG("%s: %s\n", __func__, options);
    
    if (!parse_param(options, "rawbufsz=", buffer, sizeof(buffer), 
        (void *)&ptr->rawbufsz))
        ptr->rawbufsz = 4096;

    if (!parse_param(options, "rawbuf=", buffer, sizeof(buffer), 
        (void *)&ptr->rawbuf))
        ptr->rawbuf = NULL;

    if (parse_param(options, "dev@id=", buffer, sizeof(buffer), &value)) {
        const struct partition_entry *parti;
        for(int i = 0; i < STORAGE_ID_MAX; i++){
            parti = partition_get_stf_part(i, (u8_t)value);
            if (parti) {
                ptr->base.dev = partition_get_storage_dev(parti);
                if (ptr->base.dev) {
                    ptr->base.offset = parti->offset;
                    ptr->base.len = parti->size;
                    return 0;
                }
            }
        }
        return -EINVAL;
    } 
    if (parse_param(options, "dev@name=", buffer, sizeof(buffer), NULL)) {
#if USE_CUSTOM_PARTITION
        const struct gpt_entry *ge = gpt_find(buffer);
        if (ge) {
            ptr->dev = device_get_binding(ge->parent);
            ptr->offset = ge->offset;
            ptr->len = ge->size;
            return 0;
        }

        const struct disk_partition *pt = disk_partition_find(buffer);
        if (pt) {
            ptr->dev = device_get_binding(pt->parent);
            ptr->offset = pt->offset;
            ptr->len = pt->size;
            return 0;
        }
#endif /* USE_CUSTOM_PARTITION == 1 */
        ptr->base.dev = device_get_binding(buffer);
        if (ptr->base.dev) {
            const struct flash_pages_layout *pages = NULL;
            size_t layout_size = 0;
            media_pages_layout(ptr->base.dev, &pages, &layout_size);
            if (layout_size > 0) {
                if (!parse_param(options, "offset=", buffer, sizeof(buffer), 
                    (void *)&ptr->base.offset))
                    ptr->base.offset = 0;

                if (!parse_param(options, "capacity=", buffer, sizeof(buffer), 
                    (void *)&ptr->base.len))
                    ptr->base.len = pages->pages_size * pages->pages_count;

                if (ptr->rawbufsz % pages->pages_size)
                    ptr->rawbufsz = pages->pages_size;
            }
            return 0;
        }
    }

    return -EINVAL;
}

static int
check_valid(const struct fs_devopt *opt) {
    k_mutex_lock(&fs_manager_mutex, K_FOREVER);
    foreach_fs_mounted(mnt) {
        struct dev_options *ptr = &(*mnt)->opt;
        if (ptr->dev == opt->base.dev) {
            size_t p_max = ptr->offset + ptr->len;
            size_t p_min = ptr->offset;
            if ((p_max > opt->base.offset && p_max <= opt->base.offset + opt->base.len) ||
                (p_min >= opt->base.offset && p_min < opt->base.offset + opt->base.len))
                return -EEXIST;
        }
    }
    k_mutex_unlock(&fs_manager_mutex);
    return 0;
}

#ifdef CONFIG_FILE_SYSTEM_LITTLEFS
#define LITTLEFS_BUFSIZE \
    (CONFIG_FS_LITTLEFS_READ_SIZE + \
    CONFIG_FS_LITTLEFS_PROG_SIZE + \
    CONFIG_FS_LITTLEFS_LOOKAHEAD_SIZE)

static int
littlefs_prepare(const struct fs_devopt *opt, struct fs_node **pnode) {
    size_t bufsz = CONFIG_FS_LITTLEFS_CACHE_SIZE;
    size_t lfs_size = ALIGNED_UP_ADD(sizeof(struct fs_littlefs), 0, sizeof(void *));
    size_t alloc_size = sizeof(struct fs_node) + lfs_size;
    struct fs_node *node;
    struct fs_littlefs *lfs;

    if (opt->rawbuf && opt->rawbufsz < LITTLEFS_BUFSIZE) {
        FSLOG("rawbuf is too small (%u bytes)\n", opt->rawbufsz);
    }
        
    alloc_size += sizeof(struct fs_littlefs);
    if (opt->rawbuf == NULL)
        alloc_size += bufsz * 2 + CONFIG_FS_LITTLEFS_LOOKAHEAD_SIZE;
    node = mem_malloc(alloc_size);
    if (!node)
        return -ENOMEM;

    memset(node, 0, alloc_size);
    lfs = (struct fs_littlefs *)(node + 1);
    lfs->cfg.read_size  = bufsz;
    lfs->cfg.prog_size  = bufsz;
    lfs->cfg.cache_size = CONFIG_FS_LITTLEFS_CACHE_SIZE;
    lfs->cfg.lookahead_size = CONFIG_FS_LITTLEFS_LOOKAHEAD_SIZE;
    if (opt->rawbuf == NULL) {
        lfs->cfg.read_buffer = (void *)((char *)lfs + lfs_size);
        lfs->cfg.prog_buffer = ((char *)lfs->cfg.read_buffer + bufsz);
        lfs->cfg.lookahead_buffer = ((char *)lfs->cfg.prog_buffer + bufsz);
    } else {
        lfs->cfg.read_buffer = opt->rawbuf;
        lfs->cfg.prog_buffer = ((char *)lfs->cfg.read_buffer + CONFIG_FS_LITTLEFS_READ_SIZE);
        lfs->cfg.lookahead_buffer = ((char *)lfs->cfg.prog_buffer + CONFIG_FS_LITTLEFS_PROG_SIZE);
    }

    lfs->lfs_dev.offset = opt->base.offset;
    lfs->lfs_dev.size   = opt->base.len;
    lfs->lfs_dev.dev    = (struct device *)opt->base.dev;
    *pnode = node;
    return 0;
}
#endif /* CONFIG_FILE_SYSTEM_LITTLEFS */

static int
filex_flush(struct fs_node *node) {
    return fx_media_flush(node->fs.fs_data);
}

static int
filex_prepare(const struct fs_devopt *opt, struct fs_node **pnode, 
    FX_LOGDEVICE *fxdev) {
    size_t bufsz = opt->rawbufsz;
    size_t media_size = ALIGNED_UP_ADD(sizeof(FX_MEDIA), 0, sizeof(void *));
    size_t alloc_size = sizeof(struct fs_node) + media_size;
    struct fs_node *node;
    
    if (opt->rawbuf == NULL)
        alloc_size += bufsz;
    node = mem_malloc(alloc_size);
    if (!node)
        return -ENOMEM;

    memset(node, 0, alloc_size);
    fxdev->part_offset  = opt->base.offset;
    fxdev->part_size    = opt->base.len;
    fxdev->name         = opt->base.dev->name;
    if (opt->rawbuf == NULL)
        fxdev->media_buffer = (void *)((char *)(node + 1) + media_size);
    else
        fxdev->media_buffer = opt->rawbuf;
    fxdev->media_buffer_size = bufsz;
    node->fs.storage_dev = fxdev;
    node->fs_flush = filex_flush;
    *pnode = node;
    return 0;
}

int generic_fs_mount(int type, const char *mnt_point, const char *dev_opt, ...) {
    struct fs_node *node;
    struct fs_devopt opt;
    union {
        FX_LOGDEVICE fxdev;
    } priv;
    char optbuf[128];
    va_list ap;
    int err;

    if (mnt_point == NULL || dev_opt == NULL)
        return -EINVAL;

    /* Format device options */
    va_start(ap, dev_opt);
    vsnprintk(optbuf, sizeof(optbuf), dev_opt, ap);
    va_end(ap);

    /* Parse option parameters */
    err = parse_device_options(optbuf, &opt);
    if (err)
        return err;

    FSLOG("%s: name(%s) offset(0x%lx) size(0x%x) buf(%p) bufsz(%u)\n",
        __func__, 
        opt.base.dev->name, 
        opt.base.offset, 
        opt.base.len,
        opt.rawbuf,
        opt.rawbufsz);

    /* Check whether the parameter is valid */
    err = check_valid(&opt);
    if (err)
        return err;
    
    /*
     * Fill file system private data
     */
    switch (type) {
#ifdef CONFIG_FILE_SYSTEM_LITTLEFS
    case FS_LITTLEFS: 
        err = littlefs_prepare(&opt, &node);
        if (err)
            return err;
        break;
#endif /* CONFIG_FILE_SYSTEM_LITTLEFS */

    case FS_FILEXFS:
        err = filex_prepare(&opt, &node, &priv.fxdev);
        if (err)
            return err;
        break;

    default:
        return -EINVAL;
    }

    /*
     * Mount filesystem to special device
     */
    node->fs.mnt_point = mnt_point;
    node->fs.type = type;
    node->fs.fs_data = (node + 1);
    err = fs_mount(&node->fs);
    if (err) {
        mem_free(node);
        return err;
    }

    /* Save option param to fs node */
    node->opt = opt.base;

    /* Insert to list head */
    k_mutex_lock(&fs_manager_mutex, K_FOREVER);
    node->next = fs_manager_list;
    fs_manager_list = node;
    k_mutex_unlock(&fs_manager_mutex);
    return 0;
}

int generic_fs_unmount(const char *mnt_point) {
    if (mnt_point == NULL)
        return -EINVAL;

    int err = -ENODATA;
    k_mutex_lock(&fs_manager_mutex, K_FOREVER);
    foreach_fs_mounted(node) {
        if (!strcmp(mnt_point, (*node)->fs.mnt_point)) {
            err = fs_unmount(&(*node)->fs);
            if (!err) {
                struct fs_node *ptr = *node;
                *node = (*node)->next;
                mem_free(ptr);
            }
            break;
        }
    }
    k_mutex_unlock(&fs_manager_mutex);
    return err;
}

int generic_fs_flush(const char *mnt_point) {
    if (mnt_point == NULL)
        return -EINVAL;

    int err = -ENODATA;
    k_mutex_lock(&fs_manager_mutex, K_FOREVER);
    foreach_fs_mounted(node) {
        if (!strcmp(mnt_point, (*node)->fs.mnt_point)) {
            if ((*node)->fs_flush)
                err = (*node)->fs_flush(*node);
            break;
        }
    }
    k_mutex_unlock(&fs_manager_mutex);
    return err;
}
