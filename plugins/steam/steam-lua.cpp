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

    // Apply the engine's plugin-identity metatable (reads/creates PLUGIN_IDENTIFIER).
    mbm::plugin_stamp_userdata(lua, &PLUGIN_IDENTIFIER);

    lua_rawseti(lua, -2, 1);            // stores userdata as table[1]

    // Auto-subscribe to the engine so onSubscribe/onLoop/onDestroy are called
    const int index_plugin = lua_gettop(lua);
    mbm::plugin_doSubscribe(lua, index_plugin, "steam");

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