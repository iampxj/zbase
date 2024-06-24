/*
 * Copyright 2024 wtcat
 */
#ifndef BASEWORK_LIB_LUA_API_H_
#define BASEWORK_LIB_LUA_API_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif
#include "basework/thirdparty/lua/src/lua.h"
#include "basework/thirdparty/lua/src/lualib.h"
#include "basework/thirdparty/lua/src/lauxlib.h"

struct lua_lib {
    const char   *mod;
    lua_CFunction open;
};

int lua_exec_script(const char *buf, const struct lua_lib *lib,
    bool is_file);

#ifdef __cplusplus
}
#endif
#endif /* BASEWORK_LIB_LUA_API_H_ */
