/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2020 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                            |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and      |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

// steam-lua.cpp
// PLUGIN interface implementation + luaopen_ entry points.

#include "steam-lua.h"
#include "steam-impl.h"

#include <plugin-helper/plugin-helper.h>

// ---------------------------------------------------------------------------
// Plugin identifier (runtime-assigned integer that guards userdata types)
// ---------------------------------------------------------------------------
static int PLUGIN_IDENTIFIER = 1;

// ---------------------------------------------------------------------------
// Process-level Steam state
// g_steamGlobalReady: set to true on the first successful SteamAPI_InitEx.
//   Never reset — Steam is initialized once for the lifetime of the process.
// g_steamInstance: points to the active per-scene STEAM_LUA object.
//   Replaced each time require "steam" is called in a new scene.
// ---------------------------------------------------------------------------
bool        g_steamGlobalReady = false;
static STEAM_LUA *g_steamInstance  = nullptr;

namespace mbm
{
    STEAM_LUA *getSteamInstance() noexcept { return g_steamInstance; }
}

// ---------------------------------------------------------------------------
// onNewSteamLua
// Creates the plugin Lua table (mirroring the imgui-lua.cpp / tiled-lua.cpp
// pattern), places the STEAM_LUA userdata at field [1], and subscribes via
// mbm.subscribe().
// ---------------------------------------------------------------------------
static int onNewSteamLua(lua_State *lua)
{
    lua_settop(lua, 0);

    // Build the table of Lua-callable functions (defined in steam-class-lua.cpp)
    mbm::registerClassSteam(lua);

    // Allocate the STEAM_LUA instance and wrap it in Lua userdata
    auto **udata    = static_cast<STEAM_LUA **>(lua_newuserdata(lua, sizeof(STEAM_LUA *)));
    STEAM_LUA *that = new STEAM_LUA();
    *udata          = that;
    g_steamInstance = that;

    // Apply the engine's plugin-identity metatable so mbm.subscribe() can
    // validate that this userdata really is a PLUGIN instance.
    luaL_getmetatable(lua, "_usertype_plugin");
    if (lua_type(lua, -1) == LUA_TTABLE)
    {
        lua_rawgeti(lua, -1, 1);
        PLUGIN_IDENTIFIER = lua_tointeger(lua, -1);
        lua_pop(lua, 1);
    }
    else
    {
        lua_pop(lua, 1);
        mbm::lua_create_metatable_identifier(lua, "_usertype_plugin", PLUGIN_IDENTIFIER);
    }
    lua_setmetatable(lua, -2);          // sets plugin metatable on userdata

    lua_rawseti(lua, -2, 1);            // stores userdata as table[1]

    // Auto-subscribe to the engine so onSubscribe/onLoop/onDestroy are called
    bool bRegistered                       = false;
    const int index_plugin                 = lua_gettop(lua);
    unsigned int index_plugin_subscription = 0xffffffff;

    lua_getglobal(lua, "mbm");
    if (lua_type(lua, -1) == LUA_TTABLE)
    {
        lua_getfield(lua, -1, "doSubscribe");
        if (lua_isfunction(lua, -1))
        {
            lua_pushvalue(lua, index_plugin);
            constexpr int nargs    = 1;
            constexpr int nresults = 1;
            if (lua_pcall(lua, nargs, nresults, 0) == LUA_OK)
            {
                if (lua_type(lua, -1) == LUA_TNUMBER)
                {
                    index_plugin_subscription = static_cast<unsigned int>(lua_tointeger(lua, -1));
                    if (index_plugin_subscription != 0xffffffff)
                        bRegistered = true;
                }
            }
        }
    }

    if (bRegistered)
    {
        const int total_in_stack = lua_gettop(lua);
        if (total_in_stack > index_plugin)
            lua_pop(lua, total_in_stack - index_plugin);
    }
    else
    {
        lua_settop(lua, 0);
        luaL_error(lua,
            "steam plugin: could not subscribe to mbm.subscribe().\n"
            "Ensure the Steam plugin is loaded from within a running mini-mbm scene.");
    }

    that->m_lua = lua;
    return 1;
}

// ---------------------------------------------------------------------------
// Plugin entry points
// ---------------------------------------------------------------------------

int luaopen_steam(lua_State *lua)
{
    return onNewSteamLua(lua);
}

int luaopen_libsteam(lua_State *lua)
{
    return luaopen_steam(lua);
}