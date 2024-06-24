/*
 * Copyright 2024 wtcat
 */
#include <unistd.h>

#include "basework/lib/lua/lua_api.h"
#include "gtest/gtest.h"


extern "C" {

static int add(lua_State *l) {
    lua_Integer a = lua_tointeger(l, 1);
    lua_Integer b = lua_tointeger(l, 2);
    lua_pushinteger(l, a + b);
    return 1;
}

static const luaL_Reg lua_userlib[] = {
    {"add", add},
    {NULL, NULL}
};

static int luaopen_extlib(lua_State*lvm) {
	luaL_newlib(lvm, lua_userlib);
	return 1;
}

static const struct lua_lib user_libs[] = {
    {"user", luaopen_extlib},
    {NULL, NULL}
};

}


static const char script[] = {
    "print(\"3 + 5 = \", user.add(3, 5))\n"
    "print(\"hello\")\n"
};


TEST(lua, first) {
    lua_exec_script(script, user_libs, false);
}
