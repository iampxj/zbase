/*
 * Copyright 2022 wtcat
 */
#ifndef BASEWORK_DEV_PARTITION_FILE_H_
#define BASEWORK_DEV_PARTITION_FILE_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C"{
#endif

enum sfile_partition {
    USR_PARTITION_NORMAL,
    USR_PARTITION_SPEC
};

struct sfile;

#define SFILE_METADATA_SIZE 12
#define SFILE_SIZE(n) ((n) + SFILE_METADATA_SIZE)

/*
 * usr_sfile_init - Initialize partition file
 *
 * return 0 if success
 */
int usr_sfile_init(void);

/*
 * usr_sfile_open - Open a partition file
 *
 * @name: file name
 * @p: point to file handle pointer
 * return 0 if success
 */
int usr_sfile_open(const char *name, struct sfile **p);

/*
 * usr_sfile_close - Close partition file
 *
 * @fp: file handle
 * return 0 if success
 */
int usr_sfile_close(struct sfile *fp);

/*
 * usr_sfile_invalid - Clear file content
 *
 * @fp: file handle
 * return 0 if success
 */
int usr_sfile_invalid(struct sfile *fp);

/*
 * usr_sfile_invalid - Set offset for file pointer
 *
 * @fp: file handle
 * @offset: the offset of file pointer
 * return 0 if success
 */
int usr_sfile_setoffset(struct sfile *fp, off_t offset);

/*
 * usr_sfile_invalid - Write file
 *
 * @fp: file handle
 * @buffer: data buffer
 * @size: data size
 * return the writen size if success else less then 0
 */
ssize_t usr_sfile_write(struct sfile *fp, const void *buffer ,size_t size);

/*
 * usr_sfile_invalid - Read file
 *
 * @fp: file handle
 * @buffer: data buffer
 * @size: data size
 * return the read size if success else less then 0
 */
ssize_t usr_sfile_read(struct sfile *fp, void *buffer ,size_t size);

/*
 * usr_sfile_size - Get file size
 *
 * @fp: file handle
 * return the file size
 */
size_t usr_sfile_size(struct sfile *fp);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_DEV_PARTITION_FILE_H_ */
