/*
 * Copyright 2022 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#ifdef CONFIG_PTFILE_MOUNT_TABLE
#define _VFS_PTFS_IMPLEMENT
#define pr_fmt(fmt) "<os_ptfile>: "fmt
#include <init.h>
#include <board_cfg.h>
#include <partition/partition.h>

#include "basework/generic.h"
#include "basework/dev/partition.h"
#include "basework/dev/disk.h"
#include "basework/dev/ptfs_ext.h"
#include "basework/log.h"

struct parition_data {
    uint32_t start;
    uint32_t size;
};

static int global_partiton_find(const char *name, int file_id,
    struct parition_data *p) {
    if (name != NULL) {
        const struct disk_partition *dp = disk_partition_find(name);
        if (dp != NULL) {
            p->start = dp->offset;
            p->size  = dp->len;
            return 0;
        }
    }

    const struct partition_entry *parti;
    parti = partition_get_stf_part(STORAGE_ID_NOR, (uint8_t)file_id);
    if (!parti) {
        pr_err("Not found global parition\n");
        return -ENOENT;
    }
    p->start = parti->offset;
    p->size  = parti->size;

    return 0;
}

/*
 * PTFS_CONSTRUCT - PT filesystem define
 * @name: filesystem name
 * @maxfiles: the maximum files
 * @mnt: mount point
 * @ptname: partition name
 * @fileid: the file id of global parition table
 * @blksize: the size of file block
 * @maxlimit: the maximum size of single file
 * @bio: enable buffered i/o
 */
#define PTFS_DEFINED(name, maxfiles, mnt, ptname, fileid, maxlimit, bio) \
    PTFS_CONSTRUCT(name, maxfiles, mnt, ptname, fileid, 4096, maxlimit, bio)

#define PTFS_CONSTRUCT(name, maxfiles, mnt, ptname, fileid, blksize, maxlimit, bio) \
    static struct _pt_file RTE_JOIN(name, _pt_files)[maxfiles];     \
    static struct ptfs_class RTE_JOIN(name, _pt_context);      \
    static struct file_class RTE_JOIN(name, _pt_class) = {     \
        .mntpoint    = mnt,                                    \
        .fds_buffer  = RTE_JOIN(name, _pt_files),              \
        .fds_size    = sizeof(RTE_JOIN(name, _pt_files)),      \
        .fd_size     = sizeof(RTE_JOIN(name, _pt_files)[0]),   \
        .fs_priv     = &RTE_JOIN(name, _pt_context),           \
        .open        = ptfs_open,                              \
        .close       = ptfs_close,                             \
        .ioctl       = ptfs_ioctl,                             \
        .read        = ptfs_read,                              \
        .write       = ptfs_write,                             \
        .flush       = ptfs_flush,                             \
        .lseek       = ptfs_lseek,                             \
        .truncate    = ptfs_ftruncate,                         \
        .opendir     = ptfs_opendir,                           \
        .readdir     = ptfs_readir,                            \
        .closedir    = ptfs_closedir,                          \
        .mkdir       = NULL,                                   \
        .unlink      = ptfs_unlink,                            \
        .stat        = ptfs_stat,                              \
        .rename      = ptfs_rename,                            \
        .reset       = ptfs_reset,                             \
    };                                                         \
                                                               \
    static int RTE_JOIN(name, _ptfs_register)(const struct device *dev) {   \
        struct parition_data pt;                                                \
        if (!global_partiton_find(ptname, fileid, &pt)) {                       \
            int err = pt_file_init(&RTE_JOIN(name, _pt_context), CONFIG_SPI_FLASH_NAME, \
                pt.start, pt.size, 4096, maxfiles, maxlimit, bio);              \
            if (err) {                                                          \
                pr_err("PT filesystem initialize failed(%d)\n", err);           \
                return err;                                                     \
            }                                                                   \
            return vfs_register(&RTE_JOIN(name, _pt_class));                    \
        }                                                                       \
        pr_err("Not found parition(%s) and fileid(%d)\n", ptname, fileid);      \
        return 0;                                                               \
    }                                                                           \
    SYS_INIT(RTE_JOIN(name, _ptfs_register), APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
        
// PTFS_DEFINED(workout, 20, "/PTa:", "workout", 0, 20*1024)

#include CONFIG_PTFILE_MOUNT_HEADER
#endif /* CONFIG_PTFILE_MOUNT_TABLE */
