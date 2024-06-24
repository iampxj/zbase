/*
 * Copyright 2024 wtcat
 */

#ifdef CONFIG_HEADER_FILE
#include CONFIG_HEADER_FILE
#endif

#define pr_fmt(fmt) "<lua>: "fmt
#define CONFIG_LOGLEVEL LOGLEVEL_DEBUG
#include <errno.h>

#include "basework/thirdparty/lua/lua_api.h"
#include "basework/log.h"


int lua_exec_script(const char *buf, const struct lua_lib *lib,
    bool is_file) {
    lua_State *lvm;
    int err;

    pr_dbg("Create lua virtual-machine\n");

    /* Create lua virtual machine */
    lvm = luaL_newstate();
    if (lvm == NULL) {
        pr_err("No more memory to create lua-vm\n");
        return -ENOMEM;
    }

    pr_dbg("Open libraries\n");

    /* Open base library */
    luaL_openlibs(lvm);

    /* Register custom library */
    while (lib && lib->mod) {
        if (lib->open) {
            pr_dbg("Register module: %s\n", lib->mod);
            luaL_requiref(lvm, lib->mod, lib->open, 1);
        }
        lib++;
    }

    pr_dbg("Loading lua script...");
    if (!is_file)
        err = luaL_loadstring(lvm, buf);
    else
        err = luaL_loadfile(lvm, buf);
    pr_dbg("with error(%d)\n", err);
    
    if (err == LUA_OK) {
        err = lua_pcall(lvm, 0, LUA_MULTRET, 0);
        pr_err("\nExecute scirpt with error(%d)\n", err);
    }

    lua_close(lvm);

    return 0;
}