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
#ifndef USE_DUMMY_BACK_END_ENGINE
#if defined ANDROID
#include <lua-wrap/framework-lua.h>
#include <lua-wrap/manager-lua.h>
#include <core_mbm/log-util.h>
#include <core_mbm/device.h>
#include <core_mbm/util-interface.h>
#include <core_mbm/scene.h>
#include <plugin-helper/plugin-helper.h>
#include <plugin-helper/user-data-lua.h>

#if defined USE_OPENGL_ES
    #include <core_mbm/android-bridge.h>
#else
    #error "This file is only for OpenGL ES"
#endif

extern "C"
{
    #include <lauxlib.h>
    #include <lualib.h>
}

namespace mbm 
{
    int onDoCommands(lua_State *lua)
    {
        const int   top       = lua_gettop(lua);
        const char *what      = luaL_checkstring(lua, 1);
        const char *parameter = top > 1 ? luaL_checkstring(lua, 2) : "";
        auto *luaManager      = static_cast<LUA_MANAGER *>(LUA_MANAGER::pLuaManager);
        char result[1024]     = "";
        if (luaManager->onDoNativeCommand)
            luaManager->onDoNativeCommand(what, parameter, result, sizeof(result));
        lua_pushstring(lua, result);
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
        DEVICE *device = DEVICE::getInstance();
        const int width  = static_cast<int>(device->getBackBufferWidth());
        const int height = static_cast<int>(device->getBackBufferHeight());
        lua_pushnumber(lua, width);
        lua_pushnumber(lua, height);
        return 2;
    }

    int onQuitEngine(lua_State * lua)
    {
        DEVICE *device		= DEVICE::getInstance();
		const int   top		= lua_gettop(lua);
        device->setRun(false);
        device->setAppReturnCode(top == 1 && lua_type(lua, 1) == LUA_TNUMBER ? lua_tointeger(lua, 1) : 0);
        device->getScene()->onFinalizeScene();
        androidRequestQuit();
        return 0;
    }


    int onShowConsoleMbm(lua_State *lua)
    {
        #if _DEBUG
            PRINT_IF_DEBUG("showConsoleWindow without effect [Android]");
        #endif
        return 0;
    }

    int onGetPathSourceMbm(lua_State *lua)
    {
        const int   top      = lua_gettop(lua);
        const char *filename = top >= 1 && lua_type(lua, 1) == LUA_TSTRING ? lua_tostring(lua, 1) : nullptr;
        const int   level    = filename != nullptr && top >= 2 && lua_type(lua, 2) == LUA_TNUMBER
                              ? lua_tointeger(lua, 2)
                              : (top >= 1 && lua_type(lua, 1) == LUA_TNUMBER ? lua_tointeger(lua, 1) : 0);
        char             dir[255]   = "";
        dir[0]                      = 0;
        const char *     currentPath = androidGetAbsPath();
        if (currentPath)
            strncpy(dir, currentPath,sizeof(dir)-1);
        if (dir[0])
            lua_pushstring(lua, getPathAtLevel(level, dir, filename));
        else if (filename)
            lua_pushstring(lua, filename);
        else
            lua_pushnil(lua);
        return 1;
    }

    int onIncludeFile(lua_State *lua)
    {
        const char *fileName = luaL_checkstring(lua, 1);
        if (fileName)
        {
            bool             sucess = false;
    #if defined              ANDROID
            const char *     newPath = util::getFullPath(androidCopyFileFromAsset(fileName, "rt"),nullptr);
    #else
            const char *  newPath = util::getFullPath(fileName, nullptr);
    #endif
            if (newPath)
            {
                const int ret = luaL_dofile(lua, newPath);
                if (!ret)
                    sucess = true;
                else
                {
                    lua_print_line(lua,TYPE_LOG_ERROR, "mbm.include - Error occurs when calling luaL_dofile(%s) Hint Machine 0x%p\n",newPath, ret);
                    lua_print_line(lua,TYPE_LOG_ERROR, "mbm.include - Error: %s", lua_tostring(lua, -1));
                }
            }
            else
            {
                lua_print_line(lua,TYPE_LOG_ERROR, "mbm.include - error on open file [%s]!", fileName);
            }
            lua_pushboolean(lua, sucess ? 1 : 0);
            return 1;
        }
        else
        {
            lua_pushboolean(lua, 0);
            return 1;
        }
    }

    int getKeyCode(const char *key)
    {
        return androidGetKeyCode(key);
    }

    const char *getKeyName(const int key)
    {
        return androidGetKeyName(key);
    }

    int onGetIdiom(lua_State *lua)
    {
        lua_pushstring(lua, androidGetIdiom());
        return 1;
    }

    int onGetUserName(lua_State *lua)
    {
        const char *userName = androidGetUserName();
        if (userName)
            lua_pushstring(lua, userName);
        else
            lua_pushnil(lua);
        return 1;
    }

    int onSaveFile(lua_State *lua)
    {
        const int                top         = lua_gettop(lua);
        const char *             defaultName = (top > 0 && lua_type(lua, 1) == LUA_TSTRING) ? lua_tostring(lua, 1) : nullptr;
        std::vector<std::string> filters;
        for (int i = 2; i <= top; ++i)
        {
            const char *filter = luaL_checkstring(lua, i);
            filters.emplace_back(filter);
        }
        if (filters.size() == 0)
            filters.emplace_back("*.*");
        for (auto & i : filters)
        {
            const std::string filter(i);
            if (filter.size() >= 2 && strncmp(filter.c_str(), "*.", 2) != 0 && strncmp(filter.c_str(), ".", 1) != 0)
            {
                i.insert(0, "*.");
            }
            else if (filter.size() >= 1 && strncmp(filter.c_str(), ".", 1) == 0)
            {
                i.insert(0, "*");
            }
        }
        const int    total        = filters.size();
        const auto filtersArray = new const char *[total];
        for (unsigned int i = 0; i < filters.size(); i++)
        {
            filtersArray[i] = filters[i].c_str();
        }

        if (defaultName == NULL)
            defaultName = "callBackSaveImageLua";
        const char *fileName = androidSaveFile(defaultName);

        delete[] filtersArray;
        if (fileName)
        {
            bool        extension = false;
            std::string ret(fileName);
            const int   t = ret.size();
            for (auto & i : filters)
            {
                const int s      = i.size();
                int       offset = t - ((int)s - 1);
                if (offset > 0)
                {
                    const char *filter = i.c_str();
                    if (filter[0] == '*')
                        filter++; //*
                    const char *p = &fileName[offset];
                    if (strncmp(p, filter, s - 1) == 0)
                    {
                        extension = true;
                        break;
                    }
                }
            }
            if (extension == false)
            {
                for (auto & i : filters)
                {
                    if (strcmp(i.c_str(), "*.*") != 0)
                    {
                        const char *filter = i.c_str();
                        if (filter[0] == '*')
                            filter++; //*
                        ret += filter;
                        break;
                    }
                }
            }
            lua_pushstring(lua, ret.c_str());
            return 1;
        }
        lua_pushnil(lua);
        return 1;
    }

    int openMultiSingleFile(lua_State *lua, int allowMultipleSelects)
    {
        const int                top         = lua_gettop(lua);
        const char *             defaultName = (top > 0 && lua_type(lua, 1) == LUA_TSTRING) ? lua_tostring(lua, 1) : nullptr;
        std::vector<std::string> filters;
        for (int i = 2; i <= top; ++i)
        {
            const char *filter = luaL_checkstring(lua, i);
            filters.emplace_back(filter);
        }
        if (filters.size() == 0)
            filters.emplace_back("*.*");
        for (auto & i : filters)
        {
            const std::string filter(i);
            if (filter.size() >= 2 && strncmp(filter.c_str(), "*.", 2) != 0 && strncmp(filter.c_str(), ".", 1) != 0)
            {
                i.insert(0, "*.");
            }
            else if (filter.size() >= 1 && strncmp(filter.c_str(), ".", 1) == 0)
            {
                i.insert(0, "*");
            }
        }
        const int    total        = filters.size();
        const auto filtersArray = new const char *[total];
        for (unsigned int i = 0; i < filters.size(); i++)
        {
            filtersArray[i] = filters[i].c_str();
        }

        const char *callBack = "callBackLoadImageLua";
        if (defaultName)
            callBack = defaultName;
        if (!androidRequestOpenFile(callBack, allowMultipleSelects != 0))
        {
            lua_pushnil(lua);
            delete[] filtersArray;
            return 1;
        }
        const char *filename = "NULL";
        delete[] filtersArray;
        if (filename)
        {
            if(allowMultipleSelects)
            {
                std::vector<std::string> res;
                util::split(res,filename,'|');
                if(res.size())
                {
                    lua_newtable(lua);
                    for (unsigned int i = 0; i < res.size(); ++i)
                    {
                        log_util::replaceString(res[i], "\\", "/");
                        lua_pushstring(lua, res[i].c_str());
                        lua_rawseti(lua, -2, i+1);
                    }
                }
                else
                    lua_pushnil(lua);
            }
            else
            {
                lua_pushstring(lua, filename);
            }
        }
        else
            lua_pushnil(lua);
        return 1;
    }


    bool onShowMessageBoxAndroid(const char *const title, const char *const message, const char *dialogType)
    {
        return androidShowMessageBox(title, message, dialogType);
    }

    int onShowMessageBox(lua_State *lua)
    {
        const int         top     = lua_gettop(lua);
        const char *const title   = top > 0 && lua_type(lua, 1) == LUA_TSTRING ? lua_tostring(lua, 1) : "title";
        const char *const message = top > 1 && lua_type(lua, 2) == LUA_TSTRING ? lua_tostring(lua, 2) : "your message";
        const char *   dialogType = top > 2 && lua_type(lua, 3) == LUA_TSTRING ? lua_tostring(lua, 3) : "ok"; /* "ok" "okcancel" "yesno" */
        const char *iconType      = top > 3 && lua_type(lua, 4) == LUA_TSTRING ? lua_tostring(lua, 4) : "info"; /* "info" "warning" "error" "question" */
        int defaultButton         = top > 4 && lua_type(lua, 5) == LUA_TNUMBER ? lua_tointeger(lua,5) : 0; /* 0 for cancel/no , 1 for ok/yes */
        if (defaultButton != 0 && defaultButton != 1)
            defaultButton = 0;
        if (strcmp(dialogType, "ok") != 0 && strcmp(dialogType, "okcancel")!= 0 && strcmp(dialogType, "yesno")!= 0)
            dialogType = "ok";
        if (strcmp(iconType, "info")!= 0 && strcmp(iconType, "warning")!= 0 && strcmp(iconType, "error")!= 0 &&
            strcmp(iconType, "question")!= 0)
            iconType = "info";

        if (onShowMessageBoxAndroid(title, message, dialogType))
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onOpenFolder(lua_State *lua)
    {
        const int         top         = lua_gettop(lua);
        const char *const title       = top > 0 && lua_type(lua, 1) == LUA_TSTRING ? lua_tostring(lua, 1) : "Choose a folder";
        const char *const defaultPath = top > 1 && lua_type(lua, 2) == LUA_TSTRING ? lua_tostring(lua, 2) : "";
        const char *path = androidOpenFolder(title, defaultPath);

        if (path)
            lua_pushstring(lua, path);
        else
            lua_pushnil(lua);
        return 1;
    }

    int onInputDialogBox(lua_State *lua)
    {
        lua_pushnil(lua);
        return 1;
    }

    int onInputPasswordBox(lua_State *lua)
    {
        lua_pushnil(lua);
        return 1;
    }

    int onColorFromDialogBox(lua_State *lua)
    {
        lua_pushnil(lua);
        return 1;
    }

    int onPanic(lua_State *lua)
    {
        DEVICE *        device    = DEVICE::getInstance();
        auto *userScene           = static_cast<USER_DATA_SCENE_LUA *>(device->getScene()->userData);
        const char *    error     = lua_tostring(lua, -1);
        std::string               strErr(error ? error : "undefined");
        ERROR_LOG("%s",strErr.c_str());
        onShowMessageBoxAndroid("PANIC: unprotected error in call to Lua API", strErr.c_str(), "ok");
        if (userScene && userScene->oldPanicFunction)
            userScene->oldPanicFunction(lua);
        else
            exit(255);
        return 0;
    }
};
#endif
#endif
