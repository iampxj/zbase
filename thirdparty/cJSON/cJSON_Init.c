/*
 * Copyright 2024 wtcat
 */

#include "basework/malloc.h"
#include "cJSON.h"

static void *CJSON_CDECL cjson_malloc(size_t size) {
    return general_malloc(size);
}

static void CJSON_CDECL cjson_free(void *ptr) {
    general_free(ptr);
}

static cJSON_Hooks allocator_hooks = {
    .malloc_fn = cjson_malloc,
    .free_fn   = cjson_free
};

CJSON_PUBLIC(int) cJSON_Init(void) {
    cJSON_InitHooks(&allocator_hooks);
    return 0;
}
