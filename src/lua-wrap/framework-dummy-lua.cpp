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

#if defined ANDROID
#if defined (USE_DUMMY_BACK_END_ENGINE)
#include <lua-wrap/framework-lua.h>
#include <core_mbm/log-util.h>
#include <core_mbm/device.h>
#include <core_mbm/util-interface.h>
#include <core_mbm/scene.h>
#include <plugin-helper/plugin-helper.h>
#include <plugin-helper/user-data-lua.h>

#include <core_mbm/specific-dummy.h> // for specific context of dummy engine

extern "C" 
{
    #include <lauxlib.h>
    #include <lualib.h>
}

namespace mbm 
{
    int onDoCommands(lua_State *lua)
    {
        return 1;
    }

    void showConsoleWindowLua()
    {
        PRINT_IF_DEBUG("showConsoleWindow without effect [Android]");
    }

    void hideConsoleWindowLua()
    {
        PRINT_IF_DEBUG("hideConsoleWindow without effect [Android]");
    }

    int onGetDisplayMetrics(lua_State *lua)
    {
        return 2;
    }

    int onQuitEngine(lua_State * lua)
    {
        return 0;
    }


    int onShowConsoleMbm(lua_State *lua)
    {
        return 0;
    }

    int onGetPathSourceMbm(lua_State *lua)
    {
        return 1;
    }

    int onIncludeFile(lua_State *lua)
    {
        
        return 1;
        
    }

    int getKeyCode(const char *key)
    {
        return 0;
    }

    const char *getKeyName(const int key)
    {
        return NULL;
    }

    int onGetIdiom(lua_State *lua)
    {
        return 1;
    }

    int onGetUserName(lua_State *lua)
    {
        return 1;
    }

    int onSaveFile(lua_State *lua)
    {
        return 1;
    }

    int openMultiSingleFile(lua_State *lua, int allowMultipleSelects)
    {
        return 1;
    }


    bool onShowMessageBoxAndroid(const char *const title, const char *const message, const char *dialogType)
    {
        return false;
    }

    int onShowMessageBox(lua_State *lua)
    {
        return 1;
    }

    int onOpenFolder(lua_State *lua)
    {
        return 1;
    }

    int onInputDialogBox(lua_State *lua)
    {
        return 1;
    }

    int onInputPasswordBox(lua_State *lua)
    {
        return 1;
    }

    int onColorFromDialogBox(lua_State *lua)
    {
        return 1;
    }

    int onPanic(lua_State *lua)
    {
        return 0;
    }
};
#endif
#endif