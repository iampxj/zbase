/*
 * Copyright 2023 wtcat
 */
#ifndef BASEWORK_DEV_PTFS_H_
#define BASEWORK_DEV_PTFS_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

#ifndef CONFIG_PTFS_SIZE
#define CONFIG_PTFS_SIZE    4096 //(3 * 1024 * 1024ul)
#endif
#ifndef CONFIG_PTFS_INODE_SIZE 
#define CONFIG_PTFS_INODE_SIZE 512
#endif
#ifndef CONFIG_PTFS_BLKSIZE
#define CONFIG_PTFS_BLKSIZE 1024   //(256 * 1024ul)
#endif
#ifndef CONFIG_PTFS_MAXFILES
#define CONFIG_PTFS_MAXFILES 4
#endif

#define PTFS_MAX_INODES (CONFIG_PTFS_SIZE / CONFIG_PTFS_BLKSIZE)
#define PTFS_IMAP_SIZE  ((PTFS_MAX_INODES + 31) / 32)
#define PTFS_FMAP_SIZE  ((CONFIG_PTFS_MAXFILES + 31) / 32)

struct ptfs_file;
struct file_metadata {
#define MAX_PTFS_FILENAME 32 
    char name[MAX_PTFS_FILENAME];
    uint32_t size;
    uint32_t mtime;
    uint16_t i_frag[PTFS_MAX_INODES];
    uint16_t i_count;
    uint8_t i_meta;
    uint16_t chksum;
    uint16_t permision;
};

struct ptfs_inode {
#define MESSAGE_MAGIC BUILD_NAME('P', 'T', 'F', 'S')
	uint32_t magic;
    uint16_t hcrc;
    uint16_t nfiles;
	uint32_t i_bitmap[PTFS_IMAP_SIZE];
    uint32_t f_bitmap[PTFS_FMAP_SIZE];
    struct file_metadata meta[CONFIG_PTFS_MAXFILES];
};

struct ptfs_context {
    struct ptfs_inode inode;
    void *mtx;
    struct disk_device *dd;
    uint32_t offset; /* content offset */
    bool dirty;
};


#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEV_PTFS_H_ */

