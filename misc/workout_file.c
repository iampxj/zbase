/*
 * Copyright 2024 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<workout_file>: "fmt
#include <string.h>

#include "basework/os/osapi_fs.h"
#include "basework/log.h"
#include "basework/malloc.h"
#include "basework/lib/string.h"
#include "basework/misc/workout_parser.h"

struct fileiter_param {
#define MAX_FILES   20
#define MAX_NAMELEN 48
    char filename[MAX_FILES][MAX_NAMELEN];
    int  count;
    bool collect_file;
};

static bool file_iterator(struct vfs_dirent *dirent, void *arg) {
    if (dirent->d_type == DT_REG) {
        struct fileiter_param *fp = (struct fileiter_param *)arg;
        size_t len;

        /* 
         * Filter json file 
         */
        len = strlen(dirent->d_name);
        if (len <= 5 || strncmp(&dirent->d_name[len - 5], ".json", 5))
            return false;
        
        if (fp->collect_file) {
            if (len >= MAX_NAMELEN) {
                pr_err("File name(%s) is too long\n", dirent->d_name);
                return true;
            }
            strcpy(fp->filename[fp->count], dirent->d_name);
        }
        fp->count++;
    }
    return false;
}

int workout_load_files(const char *dir) {
    struct fileiter_param param = {0};
    size_t csize = 0;
    void *buf = NULL;
    os_file_t fd;
    int err;

    /*
     * Collect workout files
     */
    param.collect_file = true;
    err = vfs_dir_foreach(dir, file_iterator, &param);
    if (err) {
        pr_err("Scan directory(%s) failed(%d)\n", dir, err);
        return err;
    }

    for (int i = 0; i < param.count; i++) {
        struct vfs_stat stat;
        char path[64] = {0};
        size_t len;

        /*
         * General absolute path
         */
#ifndef _WIN32
        len = strlcpy(path, dir, sizeof(path));
        len = strlcpy(path+len, param.filename[i], sizeof(path) - len);
        if (len >= sizeof(path) - 1) {
            pr_err("The path(%s) is too long\n", path);
            continue;
        }
#else  /* _WIN32 */
        len = strlcpy(path, param.filename[i], sizeof(path));
#endif /* !_WIN32 */

        /*
         * Get file size
         */
        err = vfs_stat(path, &stat);
        if (err) {
            pr_err("Not found file(%s)\n", path);
            goto _exit;
        }

        /*
         * Allocate file buffer
         */
        if (csize < stat.st_size) {
            csize = stat.st_size;
            if (buf != NULL)
                general_free(buf);

            buf = general_calloc(1, csize);
            if (buf == NULL) {
                err = -ENOMEM;
                goto _exit;
            }
        }

        err = vfs_open(&fd, path, VFS_O_RDONLY);
        if (err) {
            pr_err("Open file(%s) failed\n", path);
            goto _free;
        }

        err = vfs_read(fd, buf, stat.st_size);
        if (err != stat.st_size) {
            pr_err("Read file(%s) failed\n", path);
            goto _close;
        }

        vfs_close(fd);

        /*
         * Parse workout file
         */
        pr_dbg("Parsing workout(%s, %d bytes)\n", path, stat.st_size);
        if (workout_parse(buf, stat.st_size, NULL))
            pr_err("Parse file(%s) failed\n", path);
    }

    general_free(buf);
    return 0;

_close:
    vfs_close(fd);
_free:
    general_free(buf);
_exit:
    return err;
}
