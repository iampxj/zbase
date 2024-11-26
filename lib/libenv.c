/*
 * Copyright 2024 wtcat
 */
#include <stddef.h>
#include <string.h>

#include "basework/generic.h"
#include "basework/os/osapi.h"
#include "basework/lib/libenv.h"
#include "basework/log.h"

#define ENV_CONTEXT_NR 2

struct env_context {
    const struct env_alloctor *allocator;
    os_mutex_t mtx;
};

static struct env_context _env_contexts[ENV_CONTEXT_NR];
static char *initial_env[] = {0};
static char **__environ_tbl = &initial_env[0];
static char ***__p_environ = &__environ_tbl;
struct env_context *_sys_env__context;


static inline void *
env_malloc(struct env_context *env, size_t size) {
    return env->allocator->alloc(size);
}

static inline void *
env_realloc(struct env_context *env, void *ptr, size_t size) {
    return env->allocator->realloc(ptr, size);
}

static inline void 
env_free(struct env_context *env, void *p) {
    return env->allocator->release(p);
}

static char *
findenv(struct env_context *env, register const char *name,
    int *offset) {
	int len;
	char **p;
	const char *c;

	/* In some embedded systems, this does not get set.  This protects
	   newlib from dereferencing a bad pointer.  */
	if (!*__p_environ)
		return NULL;

	c = name;
	while (*c && *c != '=')
		c++;

	/* Identifiers may not contain an '=', so cannot match if does */
	if (*c != '=') {
		len = c - name;
		for (p = *__p_environ; *p; ++p)
			if (!strncmp(*p, name, len))
				if (*(c = *p + len) == '=') {
					*offset = p - *__p_environ;
					return (char *)(++c);
				}
	}
	return NULL;
}

char *__getenv(struct env_context *env, const char *name) {
	int offset;
    guard(mutex)(&env->mtx);
	return findenv(env, name, &offset);
}

int __setenv(struct env_context *env, const char *name, const char *value,
			  int rewrite) {
	static int alloced; /* if allocated space before */
	char *C;
	int l_value, offset;

	if (strchr(name, '='))
		return -EINVAL;

	guard(mutex)(&env->mtx);

	l_value = strlen(value);
	if ((C = findenv(env, name, &offset))) { /* find if already exists */
		if (!rewrite)
			return 0;
		
		if (strlen(C) >= (size_t)l_value) { /* old larger; copy over */
			while ((*C++ = *value++) != 0);
			return 0;
		}
	} else { /* create new slot */
		int cnt;
		char **P;

		for (P = *__p_environ, cnt = 0; *P; ++P, ++cnt);
		if (alloced) { /* just increase size */
			*__p_environ = (char **)env_realloc(env, (char *)__environ_tbl,
											 (size_t)(sizeof(char *) * (cnt + 2)));
			if (!*__p_environ)
				return -1;

		} else {		 /* get new space */
			alloced = 1; /* copy old entries into it */
			P = (char **)env_malloc(env, (size_t)(sizeof(char *) * (cnt + 2)));
			if (!P)
				return (-1);
			
			memcpy((char *)P, (char *)*__p_environ, cnt * sizeof(char *));
			*__p_environ = P;
		}
		(*__p_environ)[cnt + 1] = NULL;
		offset = cnt;
	}

	for (C = (char *)name; *C && *C != '='; ++C);/* no `=' in name */
	if (!((*__p_environ)[offset] = /* name + `=' + value */
		  env_malloc(env, (size_t)((int)(C - name) + l_value + 2)))) {
		return -1;
	}

	for (C = (*__p_environ)[offset]; (*C = *name++) && *C != '='; ++C);	
	for (*C++ = '='; (*C++ = *value++) != 0;);

	return 0;
}

int __unsetenv(struct env_context *env, const char *name) {
	char **P;
	int offset;

	/* Name cannot be NULL, empty, or contain an equal sign.  */
	if (name == NULL || name[0] == '\0' || strchr(name, '='))
		return -EINVAL;

	guard(mutex)(&env->mtx);

    /* if set multiple times */
	while (findenv(env, name, &offset)) {
		for (P = &(*__p_environ)[offset];; ++P)
			if (!(*P = *(P + 1)))
				break;
	}

	return 0;
}

struct env_context *__allocenv(const struct env_alloctor *allocator) {
    if (!allocator)
        return NULL;

    if (!allocator->alloc ||
        !allocator->realloc ||
        !allocator->release)
        return NULL;

    for (size_t i = 0; i < rte_array_size(_env_contexts); i++) {
        if (!_env_contexts[i].allocator) {
            _env_contexts[i].allocator = allocator;
            os_mtx_init(&_env_contexts[i].mtx, 0);
            return &_env_contexts[i];
        }
    }

    return NULL;
}

void __dumpenv(struct env_context *env) {
	pr_out("\n\n*****Dump Environment Variables *****\n");
	guard(mutex)(&env->mtx);
	for (char **p = *__p_environ; *p; ++p)
		pr_out("%s", *p);
	pr_out("\n\n");
}
