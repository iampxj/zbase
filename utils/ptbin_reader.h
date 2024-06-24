/*
 * Copyright 2023 wtcat
 */
#ifndef BASEWORK_UTILS_PTBIN_READER_H_
#define BASEWORK_UTILS_PTBIN_READER_H_

#include <stddef.h>
#include <stdint.h>

#include <sys/types.h>

#ifdef __cplusplus
extern "C"{
#endif

struct ptbin_ops {
    void *(*open)(const char *name);
    ssize_t (*read)(void *hdl, void *buffer, size_t size, size_t offset);
};

extern const struct ptbin_ops _ptbin_local_ops;

/*
 * ptbin_open - Open a binary file
 *
 * @name: parititon name
 * @handle: file handle
 * @ops: file operations
 *
 * return 0 if success
 */
int __ptbin_open(const char *name, void **handle, 
    const struct ptbin_ops *ops);

/*
 * ptbin_open - Open a binary file that paritition based
 *
 * @name: parititon name
 * @handle: file handle
 * return 0 if success
 */
int ptbin_open(const char *name, void **handle);

/*
 * ptbin_open - Close a binary file that paritition based
 *
 * @handle: file handle
 * return 0 if success
 */
int ptbin_close(void *handle);

/*
 * ptbin_tell - Get the size of binary file
 *
 * @handle: file handle
 * return the file size if success
 */
size_t ptbin_tell(void *handle);

/*
 * ptbin_tell - Get the size of binary file
 *
 * @handle: file handle
 * @buffer: user buffer pointer
 * @size: read size
 * @offset: file offset
 * return the size if success
 */
ssize_t ptbin_read(void *handle, void *buffer, size_t size, uint32_t offset);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_UTILS_PTBIN_READER_H_ */