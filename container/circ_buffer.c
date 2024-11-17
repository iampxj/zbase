/****************************************************************************
 * mm/circbuf/circbuf.c
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

 /*
  * Copyright 2024 wtcat (modified)
  */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <assert.h>
#include <errno.h>
#include <string.h>

#include "basework/container/circ_buffer.h"
#include "basework/malloc.h"

#define _ASSERT(x)

int circbuf_init(struct circ_buffer *circ, void *base, 
    size_t bytes) {
	_ASSERT(circ);
	_ASSERT(bytes);

	if (bytes & (bytes - 1))
		return -EINVAL;

	circ->external = !!base;
	if (!base && bytes) {
		base = general_malloc(bytes);
		if (!base)
			return -ENOMEM;
	}

	circ->base = base;
	circ->size = bytes;
	circ->mask = circ->size - 1;
	circ->head = 0;
	circ->tail = 0;
	return 0;
}

int circbuf_resize(struct circ_buffer *circ, size_t bytes) {
	void *tmp = NULL;
	size_t len = 0;

	_ASSERT(circ);
	_ASSERT(!circ->external);
	if (bytes == circ->size)
		return 0;

	if (bytes) {
		tmp = general_malloc(bytes);
		if (!tmp)
			return -ENOMEM;

		len = circbuf_used(circ);
		if (bytes < len) {
			circbuf_skip(circ, len - bytes);
			len = bytes;
		}
		circbuf_read(circ, tmp, len);
	}

	general_free(circ->base);
	circ->base = tmp;
	circ->size = bytes;
	circ->head = len;
	circ->tail = 0;
	return 0;
}

void circbuf_reset(struct circ_buffer *circ) {
	_ASSERT(circ);
	circ->head = circ->tail = 0;
}

void circbuf_uninit(struct circ_buffer *circ) {
	_ASSERT(circ);

	if (!circ->external)
		general_free(circ->base);

	memset(circ, 0, sizeof(*circ));
}

size_t circbuf_size(struct circ_buffer *circ) {
	_ASSERT(circ);
	return circ->size;
}

size_t circbuf_used(struct circ_buffer *circ) {
	_ASSERT(circ);
	return circ->head - circ->tail;
}

size_t circbuf_space(struct circ_buffer *circ) {
	return circbuf_size(circ) - circbuf_used(circ);
}

bool circbuf_is_init(struct circ_buffer *circ) {
	return !!circ->base;
}

bool circbuf_is_empty(struct circ_buffer *circ) {
	return !circbuf_used(circ);
}

bool circbuf_is_full(struct circ_buffer *circ) {
	return !circbuf_space(circ);
}

ssize_t circbuf_peekat(struct circ_buffer *circ, size_t pos, void *dst, 
    size_t bytes) {
	size_t len;
	size_t off;

	_ASSERT(circ);

	if (circ->head - pos > circ->head - circ->tail)
		pos = circ->tail;

	len = circ->head - pos;
	off = pos & circ->mask;
	if (bytes > len)
		bytes = len;

	len = circ->size - off;
	if (bytes < len)
		len = bytes;

	memcpy(dst, (char *)circ->base + off, len);
	memcpy((char *)dst + len, circ->base, bytes - len);
	return bytes;
}

ssize_t circbuf_peek(struct circ_buffer *circ, void *dst, size_t bytes) {
	return circbuf_peekat(circ, circ->tail, dst, bytes);
}

ssize_t circbuf_read(struct circ_buffer *circ, void *dst, size_t bytes) {
	_ASSERT(circ);
	_ASSERT(dst || !bytes);

	bytes = circbuf_peek(circ, dst, bytes);
	circ->tail += bytes;
	return bytes;
}

ssize_t circbuf_skip(struct circ_buffer *circ, size_t bytes) {
	size_t len;

	_ASSERT(circ);

	len = circbuf_used(circ);
	if (bytes > len)
		bytes = len;
	circ->tail += bytes;
	return bytes;
}

ssize_t circbuf_write(struct circ_buffer *circ, const void *src, 
    size_t bytes) {
	size_t space;
	size_t off;

	_ASSERT(circ);
	_ASSERT(src || !bytes);

	space = circbuf_space(circ);
	off = circ->head & circ->mask;
	if (bytes > space)
		bytes = space;

	space = circ->size - off;
	if (bytes < space)
		space = bytes;

	memcpy((char *)circ->base + off, src, space);
	memcpy(circ->base, (char *)src + space, bytes - space);
	circ->head += bytes;
	return bytes;
}

ssize_t circbuf_overwrite(struct circ_buffer *circ, const void *src, 
    size_t bytes) {
	size_t overwrite = 0;
	size_t skip = 0;
	size_t space;
	size_t off;

	_ASSERT(circ);
	_ASSERT(src || !bytes);

	if (bytes > circ->size) {
		skip = bytes - circ->size;
		src = (const void *)((char *)src + skip);
		bytes = circ->size;
	}

	space = circbuf_space(circ);
	if (bytes > space) {
		overwrite = bytes - space + skip;
	}

	circ->head += skip;
	off = circ->head & circ->mask;
	space = circ->size - off;
	if (bytes < space) {
		space = bytes;
	}

	memcpy((char *)circ->base + off, src, space);
	memcpy(circ->base, (char *)src + space, bytes - space);
	circ->head += bytes;
	circ->tail += overwrite;
	return overwrite;
}

void *circbuf_get_writeptr(struct circ_buffer *circ, size_t *size) {
	size_t off;
	size_t pos;

	_ASSERT(circ);
	off = circ->head & circ->mask;
	pos = circ->tail & circ->mask;
	if (off >= pos) {
		*size = circ->size - off;
	} else {
		*size = pos - off;
	}

	return (char *)circ->base + off;
}

void *circbuf_get_readptr(struct circ_buffer *circ, size_t *size) {
	size_t off;
	size_t pos;

	_ASSERT(circ);
	off = circ->head & circ->mask;
	pos = circ->tail & circ->mask;
	if (pos > off) {
		*size = circ->size - pos;
	} else {
		*size = off - pos;
	}

	return (char *)circ->base + pos;
}

void circbuf_writecommit(struct circ_buffer *circ, size_t writtensize) {
	_ASSERT(circ);
	circ->head += writtensize;
}

void circbuf_readcommit(struct circ_buffer *circ, size_t readsize) {
	_ASSERT(circ);
	circ->tail += readsize;
}
