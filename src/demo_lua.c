/**
 * @file demo_lua.c
 * @brief Lua 5.4 Scripting Engine Demonstration.
 */

#include <stdio.h>
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

int main(void) {
    printf("====================================================\n");
    printf("Lua 5.4 Embedded Scripting Engine Demo\n");
    printf("====================================================\n");

    lua_State *L = luaL_newstate();
    if (!L) {
        fprintf(stderr, "Failed to initialize Lua state\n");
        return 1;
    }
    luaL_openlibs(L);

    printf("Lua Version: %s\n\n", LUA_RELEASE);

    const char *script = 
        "function compute_stats(name, score)\n"
        "    local msg = string.format('Entity: %s | Computed Power: %d', name, score * 2)\n"
        "    return msg\n"
        "end\n"
        "result = compute_stats('ZigCrossCompiler', 42)\n";

    if (luaL_dostring(L, script) != LUA_OK) {
        fprintf(stderr, "Lua error: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return 1;
    }

    lua_getglobal(L, "result");
    if (lua_isstring(L, -1)) {
        printf("[Script Output] %s\n", lua_tostring(L, -1));
    }

    lua_close(L);
    printf("\nLua VM executed and closed cleanly.\n");
    return 0;
}
