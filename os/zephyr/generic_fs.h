/*
 * Copyright 2025 wtcat
 */
#ifndef GENERIC_FS_H_
#define GENERIC_FS_H_

#include <fs/fs.h>

#ifdef __cplusplus
extern "C"{
#endif

/*
 * Generic filesystem mount function
 *
 * Required:
 *   dev@name=%s or dev@id=%d 
 *   rawbufsz=%u
 * 
 * Options:
 *   rawbuf=0x%x
 *   offset=0x%x
 *   capacity=0x%x
 *
 * For example:
 *   generic_fs_mount(FS_FILEXFS, "/usr", "dev@id=50 rawbufsz=8192");
 *   generic_fs_mount(FS_FILEXFS, "/usr", "dev@name=partition rawbufsz=8192");
 *   generic_fs_mount(FS_FILEXFS, "/usr", 
 *      "dev@name=%s rawbufsz=8192 offset=0x%x capacity=0x%x", "spi_name", offset, capacity);
 *
 */
int generic_fs_mount(int type, const char *mnt_point, const char *dev_opt, ...)
    __attribute__((format(printf, 3, 4)));
    
int generic_fs_unmount(const char *mnt_point);
int generic_fs_flush(const char *mnt_point);

#ifdef __cplusplus
}
#endif
#endif /* GENERIC_FS_H_ */
