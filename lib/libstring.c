/*
 * Copyright 2023 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <errno.h>
#include <string.h>
#include "basework/compiler.h"

#ifdef _WIN32
#define __libc
#else
#define __libc __rte_weak __rte_notrace
#endif

int __libc 
strsplit(char *string, int stringlen,
	     char **tokens, int maxtokens, char delim)
{
	int i, tok = 0;
	int tokstart = 1; /* first token is right at start of string */

	if (string == NULL || tokens == NULL)
		goto einval_error;

	for (i = 0; i < stringlen; i++) {
		if (string[i] == '\0' || tok >= maxtokens)
			break;
		if (tokstart) {
			tokstart = 0;
			tokens[tok++] = &string[i];
		}
		if (string[i] == delim) {
			string[i] = '\0';
			tokstart = 1;
		}
	}
	return tok;

einval_error:
	return -EINVAL;
}

size_t __libc 
strnlen(const char *s, size_t maxlen)
{
	size_t len;

	for (len = 0; len < maxlen; len++, s++) {
		if (!*s)
			break;
	}
	return len;
}

size_t __libc
strlcpy(char * __restrict dst, const char * __restrict src, size_t dsize)
{
	const char *osrc = src;
	size_t nleft = dsize;

	/* Copy as many bytes as will fit. */
	if (nleft != 0) {
		while (--nleft != 0) {
			if ((*dst++ = *src++) == '\0')
				break;
		}
	}

	/* Not enough room in dst, add NUL and traverse rest of src. */
	if (nleft == 0) {
		if (dsize != 0)
			*dst = '\0';		/* NUL-terminate dst */
		while (*src++)
			;
	}

	return (src - osrc - 1);	/* count does not include NUL */
}

size_t __libc
strlcat(char * __restrict dst, const char * __restrict src, size_t dsize)
{
	const char *odst = dst;
	const char *osrc = src;
	size_t n = dsize;
	size_t dlen;

	/* Find the end of dst and adjust bytes left but don't go past end. */
	while (n-- != 0 && *dst != '\0')
		dst++;
	dlen = dst - odst;
	n = dsize - dlen;

	if (n-- == 0)
		return(dlen + strlen(src));
	while (*src != '\0') {
		if (n != 0) {
			*dst++ = *src;
			n--;
		}
		src++;
	}
	*dst = '\0';

	return (dlen + (src - osrc));	/* count does not include NUL */
}


