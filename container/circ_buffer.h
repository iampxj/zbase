/****************************************************************************
 * include/nuttx/mm/circbuf.h
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.  The
 * ASF licenses this file to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance with the
 * License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.  See the
 * License for the specific language governing permissions and limitations
 * under the License.
 *
 ****************************************************************************/

#ifndef BASEWORK_CONTAINER_CIRCLEBUF_H_
#define BASEWORK_CONTAINER_CIRCLEBUF_H_

#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct circ_buffer {
	void *base;	   /* The pointer to buffer space */
	size_t size;   /* The size of buffer space */
	size_t head;   /* The head of buffer space */
	size_t tail;   /* The tail of buffer space */
	bool external; /* The flag for external buffer */
};

/****************************************************************************
 * Name: circbuf_init
 *
 * Description:
 *   Initialize a circular buffer.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   base  - A pointer to circular buffer's internal buffer. It can be
 *           provided by caller because sometimes the creation of buffer
 *           is special or needs to preallocated, eg: DMA buffer.
 *           If NULL, a buffer of the given size will be allocated.
 *   bytes - The size of the internal buffer.
 *
 * Returned Value:
 *   Zero on success; A negated errno value is returned on any failure.
 *
 ****************************************************************************/

int circbuf_init(struct circ_buffer *circ, void *base, size_t bytes);

/****************************************************************************
 * Name: circbuf_resize
 *
 * Description:
 *   Resize a circular buffer (change buffer size).
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   bytes - The size of the internal buffer.
 *
 * Returned Value:
 *   Zero on success; A negated errno value is returned on any failure.
 *
 ****************************************************************************/

int circbuf_resize(struct circ_buffer *circ, size_t bytes);

/****************************************************************************
 * Name: circbuf_uninit
 *
 * Description:
 *   Free the circular buffer.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

void circbuf_uninit(struct circ_buffer *circ);

/****************************************************************************
 * Name: circbuf_reset
 *
 * Description:
 *   Remove the entire circular buffer content.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

void circbuf_reset(struct circ_buffer *circ);

/****************************************************************************
 * Name: circbuf_is_init
 *
 * Description:
 *   Return true if the circular buffer had been initialized.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

bool circbuf_is_init(struct circ_buffer *circ);

/****************************************************************************
 * Name: circbuf_is_full
 *
 * Description:
 *   Return true if the circular buffer is full.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

bool circbuf_is_full(struct circ_buffer *circ);

/****************************************************************************
 * Name: circbuf_is_empty
 *
 * Description:
 *   Return true if the circular buffer is empty.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

bool circbuf_is_empty(struct circ_buffer *circ);

/****************************************************************************
 * Name: circbuf_size
 *
 * Description:
 *   Return size of the circular buffer.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

size_t circbuf_size(struct circ_buffer *circ);

/****************************************************************************
 * Name: circbuf_used
 *
 * Description:
 *   Return the used bytes of the circular buffer.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

size_t circbuf_used(struct circ_buffer *circ);

/****************************************************************************
 * Name: circbuf_space
 *
 * Description:
 *   Return the remaining space of the circular buffer.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 ****************************************************************************/

size_t circbuf_space(struct circ_buffer *circ);

/****************************************************************************
 * Name: circbuf_peekat
 *
 * Description:
 *   Get data specified position from the circular buffer without removing
 *
 * Note :
 *   That with only one concurrent reader and one concurrent writer,
 *   you don't need extra locking to use these api.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   pos   - Position to read.
 *   dst   - Address where to store the data.
 *   bytes - Number of bytes to get.
 *
 * Returned Value:
 *   The bytes of get data is returned if the peek data is successful;
 *   A negated errno value is returned on any failure.
 ****************************************************************************/

ssize_t circbuf_peekat(struct circ_buffer *circ, size_t pos, void *dst, size_t bytes);

/****************************************************************************
 * Name: circbuf_peek
 *
 * Description:
 *   Get data from the circular buffer without removing
 *
 * Note :
 *   That with only one concurrent reader and one concurrent writer,
 *   you don't need extra locking to use these api.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   dst   - Address where to store the data.
 *   bytes - Number of bytes to get.
 *
 * Returned Value:
 *   The bytes of get data is returned if the peek data is successful;
 *   A negated errno value is returned on any failure.
 ****************************************************************************/

ssize_t circbuf_peek(struct circ_buffer *circ, void *dst, size_t bytes);

/****************************************************************************
 * Name: circbuf_read
 *
 * Description:
 *   Get data from the circular buffer.
 *
 * Note :
 *   That with only one concurrent reader and one concurrent writer,
 *   you don't need extra locking to use these api.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   dst   - Address where to store the data.
 *   bytes - Number of bytes to get.
 *
 * Returned Value:
 *   The bytes of get data is returned if the read data is successful;
 *   A negated errno value is returned on any failure.
 ****************************************************************************/

ssize_t circbuf_read(struct circ_buffer *circ, void *dst, size_t bytes);

/****************************************************************************
 * Name: circbuf_skip
 *
 * Description:
 *   Skip data from the circular buffer.
 *
 * Note:
 *   That with only one concurrent reader and one concurrent writer,
 *   you don't need extra locking to use these api.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   bytes - Number of bytes to skip.
 *
 * Returned Value:
 *   The bytes of get data is returned if the skip data is successful;
 *   A negated errno value is returned on any failure.
 ****************************************************************************/

ssize_t circbuf_skip(struct circ_buffer *circ, size_t bytes);

/****************************************************************************
 * Name: circbuf_write
 *
 * Description:
 *   Write data to the circular buffer.
 *
 * Note:
 *   That with only one concurrent reader and one concurrent writer,
 *   you don't need extra locking to use these api.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   src   - The data to be added.
 *   bytes - Number of bytes to be added.
 *
 * Returned Value:
 *   The bytes of get data is returned if the write data is successful;
 *   A negated errno value is returned on any failure.
 ****************************************************************************/

ssize_t circbuf_write(struct circ_buffer *circ, const void *src, size_t bytes);

/****************************************************************************
 * Name: circbuf_overwrite
 *
 * Description:
 *   Write data to the circular buffer. It can overwrite old data when
 *   circular buffer don't have enough space to store data.
 *
 * Note:
 *   Usage circbuf_overwrite () is dangerous. It should be only called
 *   when the buffer is exclusived locked or when it is secured that no
 *   other thread is accessing the buffer.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   src   - The data to be added.
 *   bytes - Number of bytes to be added.
 *
 * Returned Value:
 *   The bytes length of overwrite is returned if it's successful;
 *   A negated errno value is returned on any failure.
 ****************************************************************************/

ssize_t circbuf_overwrite(struct circ_buffer *circ, const void *src, size_t bytes);

/****************************************************************************
 * Name: circbuf_get_writeptr
 *
 * Description:
 *   Get the write pointer of the circbuf.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   size  - Returns the maximum size that can be written consecutively.
 *
 * Returned Value:
 *   The write pointer of the circbuf.
 *
 ****************************************************************************/

void *circbuf_get_writeptr(struct circ_buffer *circ, size_t *size);

/****************************************************************************
 * Name: circbuf_get_readptr
 *
 * Description:
 *   Get the read pointer of the circbuf.
 *
 * Input Parameters:
 *   circ  - Address of the circular buffer to be used.
 *   size  - Returns the maximum size that can be read consecutively.
 *
 * Returned Value:
 *   The read pointer of the circbuf.
 *
 ****************************************************************************/

void *circbuf_get_readptr(struct circ_buffer *circ, size_t *size);

/****************************************************************************
 * Name: circbuf_writecommit
 *
 * Description:
 *   After writing data using the buf returned by circbuf_writebuf,
 *   you need to use this function to update the internal structure
 *   of cricbuf.
 *
 * Input Parameters:
 *   circ        - Address of the circular buffer to be used.
 *   writtensize - The data that has been written to the buffer.
 *
 ****************************************************************************/

void circbuf_writecommit(struct circ_buffer *circ, size_t writtensize);

/****************************************************************************
 * Name: circbuf_readcommit
 *
 * Description:
 *   After reading data using the buf returned by circbuf_readbuf,
 *   you need to use this function to update the internal structure
 *   of cricbuf.
 *
 * Input Parameters:
 *   circ     - Address of the circular buffer to be used.
 *   readsize - The data that has been read to the buffer.
 *
 ****************************************************************************/

void circbuf_readcommit(struct circ_buffer *circ, size_t readsize);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_CONTAINER_CIRCLEBUF_H_ */
