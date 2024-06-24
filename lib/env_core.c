/*
 * Copyright 2022 wtcat
 *
 * Environment variables (Borrowed from NewlibC)
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#include <errno.h>
#include <string.h>

#include "basework/compiler.h"
#include "basework/lib/env_core.h"
#include "basework/malloc.h"

#ifdef _WIN32
#undef environ
#endif

//TODO: Should use mutex lock at later
#ifndef CONFIG_BOOTLOADER
 # define ENV_LOCK   os_mtx_lock(&reent_ptr->mtx);
 # define ENV_UNLOCK os_mtx_unlock(&reent_ptr->mtx);
#else
 # define ENV_LOCK
 # define ENV_UNLOCK
 #endif /* CONFIG_BOOTLOADER */


static inline void *__rte_notrace env_malloc(struct env_reent *reent_ptr, size_t size) {
    if (reent_ptr->alloc)
        return reent_ptr->alloc(size);
    return general_malloc(size);
}

static inline void *__rte_notrace env_realloc(struct env_reent *reent_ptr, void *ptr, 
    size_t size) {
    if (reent_ptr->realloc)
        return reent_ptr->realloc(ptr, size);
    return general_realloc(ptr, size);
}

static inline void __rte_notrace env_free(struct env_reent *reent_ptr, void *ptr) {
    if (reent_ptr->free)
        reent_ptr->free(ptr);
    general_free(ptr);
}

static char *__rte_notrace env_find_locked(struct env_reent *reent_ptr, const char *name, 
    int *offset) {
    const char *c;
    char **p;
    int len;

    /* In some embedded systems, this does not get set.  This protects
    newlib from dereferencing a bad pointer.  */
    if (!*reent_ptr->p_environ)
        return NULL;

    c = name;
    while (*c && *c != '=') c++;

    /* Identifiers may not contain an '=', so cannot match if does */
    if (*c != '=') {
        len = c - name;
        for (p = *reent_ptr->p_environ; *p; ++p) {
            if (!strncmp(*p, name, len)) {
                if (*(c = *p + len) == '=') {
                    *offset = p - *reent_ptr->p_environ;
                    return (char *)(++c);
                }
            }
        }
    }

    return NULL;
}

static int __rte_notrace env_set_locked(struct env_reent *reent_ptr, const char *name,
	const char *value, int rewrite) {
    char *C;
    int l_value, offset;

    if (strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }

    l_value = strlen (value);
    if ((C = env_find_locked (reent_ptr, name, &offset))) {				
        /* find if already exists */
        if (!rewrite)
            return 0;

        if ((int)strlen (C) >= l_value) {
        	/* old larger; copy over */
            while ((*C++ = *value++) != 0);
            return 0;
        }

    } else {
    	/* create new slot */
        int cnt;
        char **P;

        for (P = *reent_ptr->p_environ, cnt = 0; *P; ++P, ++cnt);
        if (reent_ptr->alloced) {
            /* just increase size */
            *reent_ptr->p_environ = (char **)env_realloc(reent_ptr, (char *)reent_ptr->environ,
                (size_t)(sizeof(char *) * (cnt + 2)));
            if (!*reent_ptr->p_environ)
                return -1;
        } else {
            /* get new space */
            reent_ptr->alloced = true;		/* copy old entries into it */
            P = (char **)env_malloc(reent_ptr, (size_t) (sizeof (char *) * (cnt + 2)));
            if (!P) 
                return (-1);
            memcpy((char *) P,(char *) *reent_ptr->p_environ, cnt * sizeof (char *));
            *reent_ptr->p_environ = P;
        }
        (*reent_ptr->p_environ)[cnt + 1] = NULL;
        offset = cnt;
    }

    for (C = (char *) name; *C && *C != '='; ++C);	/* no `=' in name */
    if (!((*reent_ptr->p_environ)[offset] =	/* name + `=' + value */
        env_malloc(reent_ptr, (size_t) ((int) (C - name) + l_value + 2))))
        return -1;

    for (C = (*reent_ptr->p_environ)[offset]; (*C = *name++) && *C != '='; ++C);
    for (*C++ = '='; (*C++ = *value++) != 0;);

    return 0;
}

int __rte_notrace _env_set(struct env_reent *reent_ptr, const char *name,
	const char *value, int rewrite) {
    int err;

    ENV_LOCK
    err = env_set_locked(reent_ptr, name, value, rewrite);
    ENV_UNLOCK

    return err;
}

int __rte_notrace _env_unset(struct env_reent *reent_ptr, const char *name) {
    char **P;
    int offset;

    /* Name cannot be NULL, empty, or contain an equal sign.  */ 
    if (name == NULL || name[0] == '\0' || strchr(name, '=')) {
        errno = EINVAL;
        return -1;
    }

    ENV_LOCK

    while (env_find_locked(reent_ptr, name, &offset))	{/* if set multiple times */
        for (P = &(*reent_ptr->p_environ)[offset]; ; ++P)
        if (!(*P = *(P + 1)))
            break;
    }

    ENV_UNLOCK
    return 0;
}

char *__rte_notrace _env_get(struct env_reent *reent_ptr, const char *name) {
    int offset;
    char *p;

    ENV_LOCK
    p = env_find_locked(reent_ptr, name, &offset);
    ENV_UNLOCK

    return p;
}

int __rte_notrace _env_load(struct env_reent *reent_ptr, 
    int (*readline_cb)(void *ctx, void *buffer, size_t max_size), 
    void *ctx) {
    char *buffer, *pval;
    int err, ret;

    if (!readline_cb)
        return -EINVAL;

    ENV_LOCK

    buffer = env_malloc(reent_ptr, ENV_MAX);
    if (!buffer) {
        ENV_UNLOCK
        return -EINVAL;
    }

    while ((ret = readline_cb(ctx, buffer, ENV_MAX - 1)) > 0) {
        buffer[ret] = '\0';
        pval = strchr(buffer, '=');
        if (!pval)
            continue;
        *pval++ = '\0';
        if (buffer[ret - 1] == '\n')
            buffer[ret - 1] = '\0';

        err = env_set_locked(reent_ptr, buffer, pval, 0);
        if (err)
            break;
    }

    env_free(reent_ptr, buffer);
    ENV_UNLOCK
    return err;
}

int __rte_notrace _env_flush(struct env_reent *reent_ptr, 
    int (*write_cb)(void *ctx, void *buffer, size_t size), 
    void *ctx) {
    char *buffer;
    char **p;
    int ret = 0;

    if (!write_cb)
        return -EINVAL;

    ENV_LOCK

    buffer = env_malloc(reent_ptr, ENV_MAX);
    if (!buffer) {
        ENV_UNLOCK
        return -EINVAL;
    }

    for (p = *reent_ptr->p_environ; *p; ++p) {
        if (!strchr(*p, '='))
            continue;
        
        size_t len = strlen(*p);
        memcpy(buffer, *p, len);
        buffer[len++] = '\n';
        
        ret = write_cb(ctx, buffer, len);
        if (ret < 0)
            break;
    }

    env_free(reent_ptr, buffer);
    ENV_UNLOCK
    return ret;
}

void __rte_notrace _env_free(struct env_reent *reent_ptr) {
    char **p;

    ENV_LOCK

    for (p = *reent_ptr->p_environ; *p; ++p) 
        env_free(reent_ptr, *p);
    if (reent_ptr->alloced)
        env_free(reent_ptr, *reent_ptr->p_environ);
    
    reent_ptr->environ = reent_ptr->intial_env;
    reent_ptr->p_environ = &reent_ptr->environ;
    reent_ptr->alloced = false;

    ENV_UNLOCK
}
