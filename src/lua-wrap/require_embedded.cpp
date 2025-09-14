/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
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

#include <string.h>
#include <lua-wrap/require_embedded.h>

//Basically Plugins Android, workaround however could be in other platforms as well
#if defined USE_LSQLITE3
extern "C" 
{
    #include <lsqlite3/lsqlite3.h>
}
#ifndef REQUIRE_EMBEDDED
    #define REQUIRE_EMBEDDED
#endif
#endif

#if defined USE_BOX2D_LIQUID_FUN
    #include <box2d-liquid-fun-lua/box2d-liquid-fun-lua.h>
#ifndef REQUIRE_EMBEDDED
    #define REQUIRE_EMBEDDED
#endif
#endif

#if defined USE_BOX2D
    #include <box2d/box2d-lua.h>
#ifndef REQUIRE_EMBEDDED
    #define REQUIRE_EMBEDDED
#endif
#endif

#if defined USE_IMGUI
    #include <imGui/imgui-lua.h>
#ifndef REQUIRE_EMBEDDED
    #define REQUIRE_EMBEDDED
#endif
#endif

#if defined REQUIRE_EMBEDDED
    #pragma message("REQUIRE_EMBEDDED is enabled, one or more library is linked, usually this is for ANDROID")
    #pragma message("plugins for android (https://developer.android.com/about/versions/nougat/android-7.0-changes.html#ndk) will be linked (workaround)")
    //# plugins for android (https://developer.android.com/about/versions/nougat/android-7.0-changes.html#ndk) will be linked (workaround)
    int __luaB_require_embedded(lua_State *lua)
    {
        const char* name     = luaL_checkstring(lua,1);
        (void)name;
        #if defined USE_LSQLITE3
            #pragma message("Using USE_LSQLITE3 embeeded, usually this is for ANDROID")
            if(strcmp(name,"lsqlite3") == 0)
                return luaopen_lsqlite3(lua);
        #endif

        #if defined USE_BOX2D
            #pragma message("Using USE_BOX2D embeeded, usually this is for ANDROID")
            if(strcmp(name,"box2d") == 0)
                return luaopen_box2d(lua);
        #endif

        #if defined USE_BOX2D_LIQUID_FUN
            #pragma message("Using USE_BOX2D_LIQUID_FUN embeeded, usually this is for ANDROID")
            if(strcmp(name,"box2dLiquidFun") == 0)
                return luaopen_box2dLiquidFun(lua);
        #endif

        #if defined USE_IMGUI
            #pragma message("Using USE_IMGUI embeeded, usually this is for ANDROID")
            if(strcmp(name,"ImGui") == 0)
                return luaopen_ImGui(lua);
        #endif
        lua_pushnil(lua);
        return 1;
    }
#endif
