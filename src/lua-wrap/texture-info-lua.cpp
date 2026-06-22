/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#include <lua-wrap/texture-info-lua.h>
#include <core_mbm/texture-manager.h>
#include <core_mbm/class-identifier.h>
#include <plugin-helper/plugin-helper.h>
#include <cstring>

namespace mbm
{
    TEXTURE_INFO_DATA *getTextureInfoDataFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<TEXTURE_INFO_DATA **>(lua_check_userType(lua, rawi, indexTable, L_USER_TYPE_TEXTURE_INFO));
        return *ud;
    }

    int onDestroyTextureInfoLua(lua_State *lua)
    {
        TEXTURE_INFO_DATA *data = getTextureInfoDataFromRawTable(lua, 1, 1);
        if (data)
            delete data;
        return 0;
    }

    int onGetWidthTextureInfo(lua_State *lua)
    {
        TEXTURE_INFO_DATA *data = getTextureInfoDataFromRawTable(lua, 1, 1);
        TEXTURE *texture = data ? data->getTexture() : nullptr;
        if (texture)
            lua_pushinteger(lua, texture->getWidth());
        else
            lua_pushinteger(lua, 0);
        return 1;
    }

    int onGetHeightTextureInfo(lua_State *lua)
    {
        TEXTURE_INFO_DATA *data = getTextureInfoDataFromRawTable(lua, 1, 1);
        TEXTURE *texture = data ? data->getTexture() : nullptr;
        if (texture)
            lua_pushinteger(lua, texture->getHeight());
        else
            lua_pushinteger(lua, 0);
        return 1;
    }

    int onGetSizeTextureInfo(lua_State *lua)
    {
        TEXTURE_INFO_DATA *data = getTextureInfoDataFromRawTable(lua, 1, 1);
        TEXTURE *texture = data ? data->getTexture() : nullptr;
        if (texture)
        {
            lua_pushinteger(lua, texture->getWidth());
            lua_pushinteger(lua, texture->getHeight());
        }
        else
        {
            lua_pushinteger(lua, 0);
            lua_pushinteger(lua, 0);
        }
        return 2;
    }

    int onHasAlphaTextureInfo(lua_State *lua)
    {
        TEXTURE_INFO_DATA *data = getTextureInfoDataFromRawTable(lua, 1, 1);
        TEXTURE *texture = data ? data->getTexture() : nullptr;
        if (texture)
            lua_pushboolean(lua, texture->hasAlphaChannel());
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onGetIdTextureInfo(lua_State *lua)
    {
        TEXTURE_INFO_DATA *data = getTextureInfoDataFromRawTable(lua, 1, 1);
        TEXTURE *texture = data ? data->getTexture() : nullptr;
        if (texture)
            lua_pushlightuserdata(lua, texture->getBackendTexturePointer());
        else
            lua_pushnil(lua);
        return 1;
    }

    int onGetFileNameTextureInfo(lua_State *lua)
    {
        TEXTURE_INFO_DATA *data = getTextureInfoDataFromRawTable(lua, 1, 1);
        if (data && !data->fileName.empty())
            lua_pushstring(lua, data->fileName.c_str());
        else
            lua_pushnil(lua);
        return 1;
    }

    int onIsLoadedTextureInfo(lua_State *lua)
    {
        TEXTURE_INFO_DATA *data = getTextureInfoDataFromRawTable(lua, 1, 1);
        TEXTURE *texture = data ? data->getTexture() : nullptr;
        if (texture)
            lua_pushboolean(lua, texture->isLoaded());
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onIsValidTextureInfo(lua_State *lua)
    {
        TEXTURE_INFO_DATA *data = getTextureInfoDataFromRawTable(lua, 1, 1);
        TEXTURE *texture = data ? data->getTexture() : nullptr;
        lua_pushboolean(lua, texture != nullptr);
        return 1;
    }

    int onIndexTextureInfo(lua_State *lua)
    {
        const char *what = luaL_checkstring(lua, 2);
        if (what)
        {
            if (strcmp(what, "width") == 0)
                return onGetWidthTextureInfo(lua);
            else if (strcmp(what, "height") == 0)
                return onGetHeightTextureInfo(lua);
            else if (strcmp(what, "hasAlpha") == 0)
                return onHasAlphaTextureInfo(lua);
            else if (strcmp(what, "fileName") == 0)
                return onGetFileNameTextureInfo(lua);
            else if (strcmp(what, "isLoaded") == 0)
                return onIsLoadedTextureInfo(lua);
            else if (strcmp(what, "isValid") == 0)
                return onIsValidTextureInfo(lua);
        }
        lua_pushnil(lua);
        return 1;
    }

    int onToStringTextureInfo(lua_State *lua)
    {
        TEXTURE_INFO_DATA *data = getTextureInfoDataFromRawTable(lua, 1, 1);
        TEXTURE *texture = data ? data->getTexture() : nullptr;
        if (texture)
        {
            lua_pushfstring(lua, "TextureInfo: %s (%dx%d, alpha:%s)", 
                data->fileName.c_str(),
                texture->getWidth(), 
                texture->getHeight(),
                texture->hasAlphaChannel() ? "true" : "false");
        }
        else if (data && !data->fileName.empty())
            lua_pushfstring(lua, "TextureInfo: %s (invalid/released)", data->fileName.c_str());
        else
            lua_pushstring(lua, "TextureInfo: nil");
        return 1;
    }

    int onNewTextureInfoLua(lua_State *lua, TEXTURE *texture)
    {
        if (texture == nullptr)
        {
            lua_pushnil(lua);
            return 1;
        }

        const char *fileName = texture->getFileNameTexture();
        if (fileName == nullptr)
        {
            lua_pushnil(lua);
            return 1;
        }

        luaL_Reg regTextureInfoMethods[] = {
            {"getWidth", onGetWidthTextureInfo},
            {"getHeight", onGetHeightTextureInfo},
            {"getSize", onGetSizeTextureInfo},
            {"hasAlpha", onHasAlphaTextureInfo},
            {"getId", onGetIdTextureInfo},
            {"getFileName", onGetFileNameTextureInfo},
            {"isLoaded", onIsLoadedTextureInfo},
            {"isValid", onIsValidTextureInfo},
            {nullptr, nullptr}
        };

        luaL_newlib(lua, regTextureInfoMethods);
        
        luaL_getmetatable(lua, "_mbmTextureInfo");
        lua_setmetatable(lua, -2);

        auto **udata = static_cast<TEXTURE_INFO_DATA **>(lua_newuserdata(lua, sizeof(TEXTURE_INFO_DATA *)));
        *udata = new TEXTURE_INFO_DATA(fileName, texture->hasAlphaChannel());

        /* trick to ensure that we will receive the expected metatable type. */
        const char *__userdata_name = getUserTypeAsString(L_USER_TYPE_TEXTURE_INFO);
        luaL_getmetatable(lua, __userdata_name);
        lua_setmetatable(lua, -2);
        /* end trick */

        lua_rawseti(lua, -2, 1);

        return 1;
    }

    void registerClassTextureInfo(lua_State *lua)
    {
        luaL_Reg regTextureInfoMMethods[] = {
            {"__index", onIndexTextureInfo},
            {"__tostring", onToStringTextureInfo},
            {"__gc", onDestroyTextureInfoLua},
            {nullptr, nullptr}
        };
        luaL_newmetatable(lua, "_mbmTextureInfo");
        luaL_setfuncs(lua, regTextureInfoMMethods, 0);
        lua_settop(lua, 0);
    }
}
