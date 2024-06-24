/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_LIB_STRING_H_
#define BASEWORK_LIB_STRING_H_

#include <stddef.h>

#ifdef __cplusplus
extern "C"{
#endif

size_t strlcpy(char * __restrict dst, const char * __restrict src, size_t dsize);
size_t strlcat(char * __restrict dst, const char * __restrict src, size_t dsize);
size_t strnlen(const char *s, size_t maxlen);
int strsplit(char *string, int stringlen, char **tokens, int maxtokens, char delim);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_LIB_STRING_H_ */