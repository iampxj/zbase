/*
 * Copyright 2022 wtcat
 */
#ifndef BASEWORK_LIB_ENVCORE_H_
#define BASEWORK_LIB_ENVCORE_H_

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

/*
 * env_set - Create or overwrite a environment variable 
 *
 * @name: variable name
 * @value: variable value
 * @overwrite: if it exist then overwirte
 * return 0 if success
 */
int env_set(const char *name, const char *value, int overwrite);

/*
 * env_unset - Delete a environment variable 
 *
 * @name: variable name
 * return 0 if success
 */
int env_unset(const char *name);

/*
 * env_get - Get a environment variable 
 *
 * @name: variable name
 * return a string if success else null
 */
char *env_get(const char *name);

/*
 * env_load - Load environment variables from media
 *
 * @readline_cb: the read callback that read a line per time
 * @ctx: callback parameter
 * return 0 if success
 */
int env_load(int (*readline_cb)(void *ctx, void *buffer, size_t max_size),
    void *ctx);

/*
 * env_flush - Flush all environment variables 
 *
 * @write_cb: the write callback function
 * @ctx: callback parameter
 * return 0 if success
 */
int env_flush(int (*write_cb)(void *ctx, void *buffer, size_t size),
    void *ctx);

/*
 * env_reset - Clear all environment variables
 */
void env_reset(void);

/*
 * env_streq - Compare whether the key-value is equal 
 *
 * @key: environment key
 * @s: string value
 * return true if equal
 */
bool env_streq(const char *key, const char *s);

/*
 * env_getul - Get key-value and convert to unsigned long value
 *
 * @key: environment key
 * @base:
 * return the value of key
 */
unsigned long env_getul(const char *key, int base);

/*
 * env_getul - Get key-value and convert to long value
 *
 * @key: environment key
 * @@base:
 * return the value of key
 */
long env_getl(const char *key, int base);

/*
 * env_setint - Set integer value to key
 *
 * @key: environment key
 * @v: value
 * @base:
 * return 0 if success
 */
int env_setint(const char *key, int v, int base);

/*
 * env_flush_ram - Flush environment variables to ram
 *
 * @buffer: buffer pointer
 * @maxlen: buffer size
 * return great than 0 if success
 */
int env_flush_ram(void *buffer, size_t maxlen);

/*
 * env_load_ram - Load environment variables from ram
 *
 * @input: buffer pointer
 * @size: buffer size
 * return  0 if success
 */
int env_load_ram(void *input, size_t size);

/*
 * env_dump - Dump environment information
 *
 * return  0 if success
 */
int env_dump(void);

/*
 * env_init - Initialize environment variable
 *
 * return  0 if success
 */
int env_init(void);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_LIB_ENVCORE_H_ */
