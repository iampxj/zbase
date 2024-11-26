/*
 * Copyright 2023 wtcat
 */
#ifndef BASEWORK_UTILS_BINMERGE_H_
#define BASEWORK_UTILS_BINMERGE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"{
#endif

#define FILE_HMAGIC 0xdeebeefa
#define BIN_HEADER_BASE \
	uint32_t magic; \
	uint32_t crc; \
	uint32_t size; 

#define FILE_HEADER_BASE \
	BIN_HEADER_BASE \
	uint32_t nums;

struct file_node {
#define MAX_NAMELEN 16
	char f_name[MAX_NAMELEN];
	uint32_t f_offset;
	uint32_t f_size;
};

struct file_header_base {
	FILE_HEADER_BASE
};

struct file_header {
	FILE_HEADER_BASE
	uint16_t devid;
	uint16_t param;
	struct file_node headers[];
};

struct bin_header {
	BIN_HEADER_BASE
	char data[];
};

struct crcfile_node {
	struct file_node files;
	uint32_t crc;
};

struct copy_fnode {
	struct file_node node;
	char d_device[MAX_NAMELEN];
	char s_device[MAX_NAMELEN];
};

#define PACKAGE_HEADER(type, n) \
	struct type { \
		struct file_header base; \
		struct copy_fnode  nodes[n]; \
	}

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_UTILS_BINMERGE_H_ */
