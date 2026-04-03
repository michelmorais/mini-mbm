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

#ifndef STEAM_LUA_H
#define STEAM_LUA_H

// ---------------------------------------------------------------------------
// Export macro
// ---------------------------------------------------------------------------
#if defined(__GNUC__)
    #define STEAM_PLUGIN_API __attribute__((__visibility__("default")))
#elif defined(WIN32)
    #ifdef STEAM_PLUGIN_BUILD_DLL
        #define STEAM_PLUGIN_API __declspec(dllexport)
    #else
        #define STEAM_PLUGIN_API __declspec(dllimport)
    #endif
#else
    #define STEAM_PLUGIN_API
#endif

extern "C"
{
    #include <lualib.h>
    #include <lauxlib.h>
    #include <lua.h>
}

// ---------------------------------------------------------------------------
// Plugin entry points
// Both forms are exported so that both `require "steam"` and
// `require "libsteam"` work correctly.
// ---------------------------------------------------------------------------
extern "C" STEAM_PLUGIN_API int luaopen_steam    (lua_State *lua);
extern "C" STEAM_PLUGIN_API int luaopen_libsteam (lua_State *lua);

#endif // !STEAM_LUA_H
