/*
 * Copyright 2022 wtcat
 *
 * Zephyr for Googletest 
 */
#include <stdlib.h>
#include <string.h>

#include <fs/fs.h>

char *strdup(const char *str) {
    size_t len;
    char *dup;

    if (str == NULL)
        return NULL;
    len = strlen(str);
    dup = malloc(len + 1);
    if (dup == NULL) 
        return NULL;
    dup[len] = '\0';
    return memcpy(dup, str, len);
}

/* For newlibc */
int _unlink(const char *abs_path) {
    return fs_unlink(abs_path);
}

char *getcwd(char *buf,size_t size) {
    if (buf) {
        strncpy(buf, "/zephyr", size);
        return buf;
    }
    return NULL;
}