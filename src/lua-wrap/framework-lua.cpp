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

#include <lua-wrap/framework-lua.h>
#include <lua-wrap/camera-lua.h>
#include <lua-wrap/vec2-lua.h>
#include <lua-wrap/vec3-lua.h>
#include <lua-wrap/render-table/texture-view-lua.h>
#include <lua-wrap/render-table/gif-view-lua.h>
#include <lua-wrap/render-table/shape-lua.h>
#include <lua-wrap/render-table/background-lua.h>
#include <lua-wrap/render-table/line-mesh-lua.h>
#include <lua-wrap/render-table/particle-lua.h>
#include <lua-wrap/render-table/render-2-texture-lua.h>
#include <lua-wrap/manager-lua.h>
#include <lua-wrap/timer-lua.h>
#include <lua-wrap/audio-lua.h>
#include <core_mbm/log-util.h>
#include <core_mbm/device.h>
#include <core_mbm/util-interface.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/dynamic-var.h>
#include <core_mbm/texture-manager.h>
#include <core_mbm/shader.h>
#include <core_mbm/shader-var-cfg.h>
#include <core_mbm/core-manager.h>
#include <core_mbm/vigenere.h>
#include <core_mbm/plugin-callback.h>
#include <core_mbm/audio.h>
#if defined _WIN32
    #include <dirent-1-13/dirent.h>
    #define __separator_dir '\\'
#else
    #include <dirent.h>
    #define __separator_dir '/'
#endif
#include <version/version.h>
#include <miniz-wrap/miniz-wrap.h>
#include <lodepng/lodepng.h>
#include <plugin-helper/plugin-helper.h>
#include <plugin-helper/user-data-lua.h>
#include <lua-wrap/render-table/tile-lua.h>
#include <lua-wrap/render-table/sprite-lua.h>
#include <lua-wrap/render-table/mesh-lua.h>
#include <lua-wrap/render-table/font-lua.h>
#include <lua-wrap/texture-info-lua.h>
#include <lua-wrap/render-table/mesh-debug-lua.h>

#include <algorithm>
#include <map>
#include <vector>
#include <audio-interface.h>
#if defined ANDROID
    // no includes here
#elif defined(__APPLE__) && !defined(ANDROID)
    #include <unistd.h>                 // getcwd — no X11 on macOS
#elif defined(__linux__)
    #include <unistd.h>
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
#endif

#ifdef USE_AESCRYPT
    #error ("Recommend not to use AESCrypt, you can remove this error if you insist on using it")
#ifdef _WIN32
    #include <AESCrypt/win32/aes.crypt.h>
#else
    #include <AESCrypt/linux/aes.crypt.h>
#endif

#elif defined USE_PLUSAES
    #include "plusaes/plusaes.hpp"
#endif


extern "C" 
{
    #include <lauxlib.h>
    #include <lualib.h>
}

#ifdef USE_VR
    #include <lua-wrap/render-table/vr-lua.h>
#endif



namespace mbm 
{
    inline const char* __std_p()
    {
        static_assert(sizeof(MBM_VERSION) == 4, "MBM_VERSION must be in format X.Y");
        static char _p[17] = {
            'M', 'i', 'N', 'i', 'M', 'b', 'M', '-',
            MBM_VERSION[0], MBM_VERSION[1], MBM_VERSION[2],
            '#', ' ', 'W', 'M', 'W', 
            '\0'
        };
        return _p;
    }

    inline const unsigned char* __iv_p()
    {
        static const unsigned char iv[16] = {
        'm', 'I', 'n', 'I', '-', 'M', 'b', 'M',
        0x01, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        };
        return iv;
    }

    #if !defined ANDROID
    int onExecuteInOtherThread(lua_State *lua)
    {
        std::string command              = luaL_checkstring(lua,1);
        mbm::DEVICE* device              = mbm::DEVICE::getInstance();
        device->ptrManager->execute_system_cmd_thread(command.c_str());
        return 0;
    }

    #endif

    int onGenerateImageResourceHeaderFromPng(lua_State *lua)
    {
        const char *pngPath = luaL_checkstring(lua,1);
        const char *outputHeaderPath = luaL_checkstring(lua,2);
        const char *fileName = strrchr(outputHeaderPath, '/');
        if (fileName)
            fileName++;
        else
            fileName = outputHeaderPath;
        // Extract resource name: "resource-particle.h" -> "particle", "particle.h" -> "particle"
        static char resourceNameBuf[256];
        const char *dot = strrchr(fileName, '.');
        size_t nameLen;
        const char *nameStart;
        if (strncmp(fileName, "resource-", 9) == 0)
        {
            nameStart = fileName + 9;
            nameLen = dot ? (size_t)(dot - nameStart) : strlen(nameStart);
        }
        else
        {
            nameStart = fileName;
            nameLen = dot ? (size_t)(dot - fileName) : strlen(fileName);
        }
        size_t copyLen = nameLen < sizeof(resourceNameBuf) - 1 ? nameLen : sizeof(resourceNameBuf) - 1;
        memcpy(resourceNameBuf, nameStart, copyLen);
        resourceNameBuf[copyLen] = '\0';
        const char *resourceName = resourceNameBuf;
        char strMessageError[1024] = {0};
        bool ret = TEXTURE_MANAGER::generateImageResourceHeaderFromPng(pngPath, outputHeaderPath, resourceName, strMessageError, sizeof(strMessageError));
        if(ret == false)
            ERROR_LOG("%s",strMessageError);
        lua_pushboolean(lua, ret);
        return 1;
    }
    

    void lua_userdata_register(lua_State *lua,const int value)
    {
        const char* __userdata_ = getUserTypeAsString(value);
        assert(strcmp("_usertype_unknown",__userdata_) != 0);
        luaL_newmetatable(lua, __userdata_);
        lua_pushinteger(lua,value);
        lua_rawseti(lua,-2,1);
        lua_settop(lua,0);
    }

    void registerClassUsersData(lua_State *lua)
    {
        lua_settop(lua,0);
        for(int i= L_USER_TYPE_BEGIN + 1; i < L_USER_TYPE_END; ++i)
        {
            lua_userdata_register(lua,i);
        }
    }

    int enableTextureFilterLua(lua_State *lua)
    {
        bool value = lua_toboolean(lua,1);
        TEXTURE::EnablePixelPerfectTexture(!value);
        return 0;
    }

    int onSetMinMaxWindowSizeLua(lua_State *lua)
    {
        const int   top   = lua_gettop(lua);
        const int  min_x  = top >= 1 ? luaL_checkinteger(lua, 1) : 0;
        const int  min_y  = top >= 2 ? luaL_checkinteger(lua, 2) : 0;
        const int  max_x  = top >= 3 ? luaL_checkinteger(lua, 3) : 0;
        const int  max_y  = top >= 4 ? luaL_checkinteger(lua, 4) : 0;
        DEVICE *device = DEVICE::getInstance();
        device->ptrManager->setMinMaxSizeWindow(min_x,min_y,max_x,max_y);
        return 0;
    }
    
    int onPauseAudioOnPauseGame(lua_State *lua)
    {
        bool bPauseOnPauseAll = lua_toboolean(lua,1);
        auto manager = AUDIO_MANAGER::getInstance();
        manager->pauseAudioOnPauseGame = bPauseOnPauseAll;
        return 0;
    }

    int onGetRealSizeBackBuffer(lua_State *lua)
    {
        DEVICE *device = DEVICE::getInstance();
        lua_pushnumber(lua, device->getBackBufferWidth());
        lua_pushnumber(lua, device->getBackBufferHeight());
        return 2;
    }

    int onGetSizeBackBuffer(lua_State *lua)
    {
        DEVICE *device = DEVICE::getInstance();
        lua_pushnumber(lua, device->getScaleBackBufferWidth());
        lua_pushnumber(lua, device->getScaleBackBufferHeight());
        return 2;
    }

    int onGetFps(lua_State *lua)
    {
        const int   top      = lua_gettop(lua);
        DEVICE *device = DEVICE::getInstance();
        if(top == 1)
        {
            if(lua_toboolean(lua,1) != 0)
                lua_pushnumber(lua, device->real_fps);
            else
                lua_pushnumber(lua, device->fps);
        }
        else
        {
            lua_pushnumber(lua, device->fps);
        }
        return 1;
    }

    int onSetColorBackground(lua_State *lua)
    {
        const float  r      = luaL_checknumber(lua, 1);
        const float  g      = luaL_checknumber(lua, 2);
        const float  b      = luaL_checknumber(lua, 3);
        DEVICE *device		= DEVICE::getInstance();
        COLOR color(r, g, b, 1.0f);
        device->colorClearBackGround = (unsigned int)color;
        return 0;
    }

    int onAddPathSourceMbm(lua_State *lua)
    {
        const char *  path    = luaL_checkstring(lua, 1);
        util::addPath(path);
        return 0;
    }

    const char *getPathAtLevel(const int level, const char *path, const char *filename)
    {
        static std::string strRet;
        strRet.clear();
        if (path == nullptr)
            return "NULL";
        std::vector<std::string> result;
        util::split(result, path, util::getCharDirSeparator());
        for (unsigned int i = 0; i < result.size() + level && i < result.size(); ++i)
        {
            strRet += result[i];
            if ((i + 1) < result.size() + level && (i + 1) < result.size())
                strRet += util::getCharDirSeparator();
        }
        if (filename)
        {
            strRet += util::getCharDirSeparator();
            strRet += filename;
        }
        return strRet.c_str();
    }

    int onGetFullPath(lua_State *lua)
    {
        bool exitFile               = false;
        const char *       filename = luaL_checkstring(lua, 1);
        const char *       ret      = util::getFullPath(filename,&exitFile);
        if (exitFile)
            lua_pushstring(lua, ret);
        else if (filename)
            lua_pushstring(lua, filename);
        else
            lua_pushnil(lua);
        return 1;
    }

    int onGetAllPath(lua_State *lua)
    {
        std::vector<std::string> lsPaths;
        util::getAllPaths(lsPaths);
        lua_newtable(lua);
        for (unsigned int i = 0; i < lsPaths.size(); ++i)
        {
            log_util::replaceString(lsPaths[i], "\\", "/");
            lua_pushstring(lua, lsPaths[i].c_str());
            lua_rawseti(lua, -2, i+1);
        }
        return 1;
    }

    int ontransform2dS2dWMbm(lua_State *lua)
    {
        const float  x      = luaL_checknumber(lua, 1);
        const float  y      = luaL_checknumber(lua, 2);
        DEVICE *device = DEVICE::getInstance();
        VEC2         out(0, 0);
        device->transformeScreen2dToWorld2d_scaled(x, y, out);
        lua_pushnumber(lua, out.x);
        lua_pushnumber(lua, out.y);
        return 2;
    }

    int ontransform2dW2dSMbm(lua_State *lua)
    {
        const float  x      = luaL_checknumber(lua, 1);
        const float  y      = luaL_checknumber(lua, 2);
        DEVICE *device = DEVICE::getInstance();
        VEC2         out(x, y);
        device->transformeWorld2dToScreen2d_scaled(x, y, out);
        lua_pushnumber(lua, out.x);
        lua_pushnumber(lua, out.y);
        return 2;
    }

    int ontransform2dsto3dmbm(lua_State *lua)
    {
        const float  x      = luaL_checknumber(lua, 1);
        const float  y      = luaL_checknumber(lua, 2);
        const float  z      = luaL_checknumber(lua, 3);
        DEVICE *device = DEVICE::getInstance();
        VEC3         out(x, y, z);
        device->transformeScreen2dToWorld3d_scaled(x, y, &out, z);
        lua_pushnumber(lua, out.x);
        lua_pushnumber(lua, out.y);
        lua_pushnumber(lua, out.z);
        return 3;
    }

    int onGetTotalObjectsRender(lua_State *lua)
    {
        DEVICE *device = DEVICE::getInstance();
        const int top = lua_gettop(lua);
        const char * type_obj = top >= 1 ? luaL_checkstring(lua,1) : "all";
        if (type_obj == nullptr || strcasecmp(type_obj,"all") == 0 )
        {
            lua_pushnumber(lua,static_cast<lua_Number>( device->totalObjectsIsRendering3D + device->totalObjectsIsRendering2D));
        }
        else if (strcasecmp(type_obj,"2d") == 0)
        {
            lua_pushnumber(lua,static_cast<lua_Number>( device->totalObjectsIsRendering2D));
        }
        else if (strcasecmp(type_obj,"3d") == 0)
        {
            lua_pushnumber(lua,static_cast<lua_Number>( device->totalObjectsIsRendering3D));
        }
        else
        {
            lua_pushnumber(lua,0);
        }
        return 1;
    }

    int addOnTouchMeshLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top == 2)
        {
            RENDERIZABLE * ptr  = getRenderizableFromRawTable(lua, 1, 1);
            auto *    userData  = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
            DEVICE *  device    = DEVICE::getInstance();
            auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
            bool inTheList = false;
            for (auto ptr2 : userScene->lsLuaCallBackOnTouchAsynchronous)
            {
                if (ptr2 == ptr)
                {
                    inTheList = true;
                    break;
                }
            }
            userData->refFunctionLua(lua, 2, &userData->ref_CallBackTouchDown);
            userData->refTableLua(lua, 1, &userData->ref_MeAsTable);
            if (inTheList == false)
                userScene->lsLuaCallBackOnTouchAsynchronous.push_back(ptr);
        }
        return 0;
    }

    int onSetGlobal(lua_State *lua)
    {
        #if defined DEBUG_SET_GET_GLOBAL
            luaL_dostring(lua,"if type(trace) == 'function' then trace(4,4) end");
        #endif
        const int top = lua_gettop(lua);
        if (top == 2)
        {
            const char *      what   = luaL_checkstring(lua, 1);
            const int         type   = lua_type(lua, 2);
            DEVICE *     device = DEVICE::getInstance();
            DYNAMIC_VAR *dyVar  = device->lsDynamicVarGlobal[what];
            switch (type)
            {
                case LUA_TNIL:
                {
                    device->lsDynamicVarGlobal[what] = nullptr;
                    if (dyVar)
                        delete dyVar;
                    #if defined DEBUG_SET_GET_GLOBAL
                        INFO_LOG(" setGlobal('%s',nil)",what);
                    #endif
                }
                break;
                case LUA_TNUMBER:
                {
                    float var = lua_tonumber(lua, top);
                    if (dyVar == nullptr)
                    {
                        dyVar                            = new DYNAMIC_VAR(DYNAMIC_FLOAT, &var);
                        device->lsDynamicVarGlobal[what] = dyVar;
                    }
                    else
                    {
                        switch(dyVar->type)
                        {
                            case DYNAMIC_FLOAT:
                            {
                                dyVar->setFloat(var);
                            }
                            break;
                            case DYNAMIC_INT:
                            {
                                dyVar->setInt(static_cast<int>(var));
                            }
                            break;
                            case DYNAMIC_SHORT:
                            {
                                dyVar->setShort(static_cast<short int>(var));
                            }
                            break;
                            default:
                            {
                                delete dyVar;
                                dyVar                            = new DYNAMIC_VAR(DYNAMIC_FLOAT, &var);
                                device->lsDynamicVarGlobal[what] = dyVar;
                            }
                        }
                    }
                    #if defined DEBUG_SET_GET_GLOBAL
                        INFO_LOG(" setGlobal('%s',%f)",what,var);
                    #endif
                }
                break;
                case LUA_TBOOLEAN:
                {
                    bool var = lua_toboolean(lua, top) ? true : false;
                    if (dyVar == nullptr)
                    {
                        dyVar                            = new DYNAMIC_VAR(DYNAMIC_BOOL, &var);
                        device->lsDynamicVarGlobal[what] = dyVar;
                    }
                    else if (dyVar->type == DYNAMIC_BOOL)
                    {
                        dyVar->setBool(var);
                    }
                    else
                    {
                        delete dyVar;
                        dyVar                            = new DYNAMIC_VAR(DYNAMIC_BOOL, &var);
                        device->lsDynamicVarGlobal[what] = dyVar;
                    }
                    #if defined DEBUG_SET_GET_GLOBAL
                        INFO_LOG(" setGlobal('%s',%s)",what,var ? "true" : "false");
                    #endif
                }
                break;
                case LUA_TSTRING:
                {
                    const char *var = lua_tostring(lua, top);
                    if (dyVar == nullptr)
                    {
                        dyVar                            = new DYNAMIC_VAR(DYNAMIC_CSTRING, static_cast<const void*>(var));
                        device->lsDynamicVarGlobal[what] = dyVar;
                    }
                    else if (dyVar->type == DYNAMIC_CSTRING)
                    {
                        dyVar->setString(var);
                    }
                    else
                    {
                        delete dyVar;
                        dyVar                            = new DYNAMIC_VAR(DYNAMIC_CSTRING, static_cast<const void*>(var));
                        device->lsDynamicVarGlobal[what] = dyVar;
                    }
                    #if defined DEBUG_SET_GET_GLOBAL
                        INFO_LOG("  setGlobal('%s',%s)",what,var);
                    #endif
                }
                break;
                case LUA_TTABLE: { return lua_error_debug(lua, "global variable [%s] not possible", what);}
                case LUA_TFUNCTION: { return lua_error_debug(lua, "global variable [%s] function not possible", what);}
                case LUA_TUSERDATA: { return lua_error_debug(lua, "global variable [%s] userdata not possible", what);}
                case LUA_TTHREAD: { return lua_error_debug(lua, "global variable [%s] thread not possible", what);}
                case LUA_TLIGHTUSERDATA: { return lua_error_debug(lua, "global variable [%s] light userdata not possible", what);}
                default: { return lua_error_debug(lua, "global variable [%s] unknown", what);}
            }
        }
        else
        {
            return lua_error_debug(lua, "expected [name_var] [value]!");
        }
        return 0;
    }

    int onGetGlobal(lua_State *lua)
    {
        #if defined DEBUG_SET_GET_GLOBAL
        luaL_dostring(lua,"if type(trace) == 'function' then trace(4,4) end");
        #endif
        const char *      what      = luaL_checkstring(lua, 1);
        const char *      strinChar = nullptr;
        DEVICE *     device    = DEVICE::getInstance();
        DYNAMIC_VAR *dyVar     = device->lsDynamicVarGlobal[what];
        if (dyVar == nullptr)
        {
            lua_pushnil(lua);
        }
        else
        {
            switch (dyVar->type)
            {
                case DYNAMIC_BOOL:
                {
                    const bool value = dyVar->getBool();
                    lua_pushboolean(lua, value ? 1 : 0);
                    #if defined DEBUG_SET_GET_GLOBAL
                        INFO_LOG(" getGlobal('%s')-> %s",what,value ? "true" : "false");
                    #endif
                }
                break;
                case DYNAMIC_CHAR:
                {
                    const char value = dyVar->getChar();
                    char       str[2];
                    str[0] = value;
                    str[1] = 0;
                    lua_pushstring(lua, str);
                    #if defined DEBUG_SET_GET_GLOBAL
                        INFO_LOG("  getGlobal('%s')-> '%s'",what,str);
                    #endif
                }
                break;
                case DYNAMIC_INT:
                {
                    const int value = dyVar->getInt();
                    lua_pushinteger(lua, value);
                    #if defined DEBUG_SET_GET_GLOBAL
                        INFO_LOG("  getGlobal('%s')-> %d",what,value);
                    #endif
                }
                break;
                case DYNAMIC_FLOAT:
                {
                    const float value = dyVar->getFloat();
                    lua_pushnumber(lua, value);
                    #if defined DEBUG_SET_GET_GLOBAL
                        INFO_LOG("  getGlobal('%s')-> %f",what,value);
                    #endif
                }
                break;
                case DYNAMIC_CSTRING:
                {
                    strinChar = dyVar->getString();
                    lua_pushstring(lua, strinChar);
                    #if defined DEBUG_SET_GET_GLOBAL
                        INFO_LOG("  getGlobal('%s')-> '%s'",what,strinChar);
                    #endif
                }
                break;
                case DYNAMIC_SHORT:
                {
                    const short value = dyVar->getShort();
                    lua_pushinteger(lua, (const int)value);
                    #if defined DEBUG_SET_GET_GLOBAL
                        INFO_LOG(" getGlobal('%s')-> %d",what,value);
                    #endif
                }
                break;
                case DYNAMIC_VOID:
                {
                    return lua_error_debug(lua, "global variable [%s] void!", what);
                }
                case DYNAMIC_FUNCTION:
                {
                    return lua_error_debug(lua, "global variable [%s] function!", what);
                }
                default:
                {
                    return lua_error_debug(lua, "global variable [%s] unknown!", what);
                }
            }
        }
        return 1;
    }

    int onGetAzimute(lua_State *lua)
    {
        DEVICE *device = DEVICE::getInstance();
        lua_pushnumber(lua, ((float)((const float)(device->camera.azimuthFromCamera))));
        return 1;
    }

    int onIs(lua_State *lua)
    {
        const char *what = luaL_checkstring(lua, 1);
        if (what)
        {
            if (strcasecmp(what, "windows") == 0)
            {
    #if defined _WIN32
                lua_pushboolean(lua, 1);
    #else
                lua_pushboolean(lua, 0);
    #endif
            }
            else if (strcasecmp(what, "android") == 0)
            {
    #if defined ANDROID
                lua_pushboolean(lua, 1);
    #else
                lua_pushboolean(lua, 0);
    #endif
            }
            else if (strcasecmp(what, "linux") == 0)
            {
    #if defined __linux__ || defined(__APPLE__) && !defined(ANDROID)
                lua_pushboolean(lua, 1);
    #else
                lua_pushboolean(lua, 0);
    #endif
            }
            else
            {
                lua_pushboolean(lua, 0);
            }
        }
        else
        {
            lua_pushboolean(lua, 0);
        }
        return 1;
    }

    int onGet(lua_State *lua)
    {
        const char *what = luaL_checkstring(lua, 1);
        if (what)
        {
            DEVICE *device = DEVICE::getInstance();
            if (strcasecmp(what, "windows") == 0 || strcasecmp(what, "android") == 0 || strcasecmp(what, "linux") == 0)
            {
    #if defined _WIN32
                lua_pushboolean(lua,1);
    #elif defined ANDROID
                lua_pushboolean(lua,1);
    #elif defined __linux__ || defined(__APPLE__)
                lua_pushboolean(lua,1);
    #else
                lua_pushboolean(lua,0);
    #endif
            }
            else if (strcasecmp(what, "version") == 0)
            {

                static std::string versions;
                versions.clear();
    #if defined _WIN32
                versions += "Plataform: Windows";
    #elif defined ANDROID
                versions += "Plataform: Android";
    #elif defined __linux__
                versions += "Plataform: Linux";
    #elif defined(__APPLE__)
                versions += "Plataform: MacOs";
    #else
                versions += "Plataform: Unknown";
    #endif
                versions += "\nMBM: ";
                versions += MBM_VERSION;
                versions += "\nLua: ";
                versions += LUA_RELEASE;
                const char *aud = AUDIO_ENGINE_version();
                if (aud)
                {
                    versions += "\nAudio engine: ";
                    versions += aud;
                }
                versions += device->getBackendEngineVersion();
                versions += "\nMini Z: ";
                versions += MZ_VERSION;

                versions += "\nLodePNG: ";
                versions += getLodePNGVersion();

                lua_pushstring(lua, versions.c_str());
            }
            else if (strcasecmp(what, "mbm") == 0 || strcasecmp(what, "mini-mbm") == 0)
            {
                lua_pushstring(lua, MBM_VERSION);
            }
            else if (strcasecmp(what, "lua") == 0)
            {
                lua_pushstring(lua, LUA_RELEASE);
            }
            else if (strcasecmp(what, "audio") == 0)
            {
                lua_pushstring(lua, AUDIO_ENGINE_version());
            }
            else if (strcasecmp(what, "opengl") == 0)
            {
                lua_pushstring(lua, device->getBackendEngineVersion());
            }
            else if (strcasecmp(what, "directx") == 0)
            {
                lua_pushstring(lua, device->getBackendEngineVersion());
            }
            else if (strcasecmp(what, "Mini Z") == 0 || strcasecmp(what, "Mini-Z") == 0 || strcasecmp(what, "MiniZ") == 0)
            {
                lua_pushstring(lua, MZ_VERSION);
            }
            else if (strcasecmp(what, "LodePNG") == 0 || strcasecmp(what, "Lode PNG") == 0)
            {
                lua_pushstring(lua, getLodePNGVersion());
            }
            else if (strcasecmp(what, "exe") == 0 || strcasecmp(what, "exe name") == 0 || strcasecmp(what, "exename") == 0)
            {
                DYNAMIC_VAR* dExeName = device->lsDynamicVarGlobal["_executable_name_"];
                if(dExeName)
                    lua_pushstring(lua, dExeName->getString());
                else
                    lua_pushnil(lua);
            }
            else if (strcasecmp(what, "USE_EDITOR_FEATURES") == 0)
            {
                lua_pushboolean(lua,1);
            }
            else if (strcasecmp(what, "MBM_ENABLE_MESH_LEGACY_V7") == 0 || strcasecmp(what, "USE_DEPRECATED_2_MINOR") == 0)
            {
                #ifdef MBM_ENABLE_MESH_LEGACY_V7
                    lua_pushboolean(lua,1);
                #else
                    lua_pushboolean(lua,0);
                #endif
            }
            else if (strcasecmp(what, "USE_VR") == 0)
            {
                #ifdef USE_VR
                    lua_pushboolean(lua,1);
                #else
                    lua_pushboolean(lua,0);
                #endif
            }
            else if (strcasecmp(what, "USE_OPENGL_ES") == 0)
            {
                const bool is_opengles = strcasecmp(device->getBackendEngineName(),"OpenGL ES") == 0;
                lua_pushboolean(lua,is_opengles ? 1 : 0);
            }
            else if (strcasecmp(what, "USE_DIRECTX9") == 0)
            {
                const bool is_Directx9 = strcasecmp(device->getBackendEngineName(), "Directx9") == 0;
                lua_pushboolean(lua, is_Directx9 ? 1 : 0);
            }
            else if (strcasecmp(what, "USE_METAL") == 0)
            {
                const bool is_metal = strcasecmp(device->getBackendEngineName(), "Metal") == 0;
                lua_pushboolean(lua, is_metal ? 1 : 0);
            }
            else if (strcasecmp(what, "backend_engine") == 0 || strcasecmp(what, "engine") == 0)
            {
                lua_pushstring(lua, device->getBackendEngineName());
            }
            else
            {
                lua_pushnil(lua);
            }
        }
        else
        {
            lua_pushnil(lua);
        }
        return 1;
    }

    int onGetTimeRun(lua_State *lua)
    {
        DEVICE *device = DEVICE::getInstance();
        lua_pushnumber(lua, device->getTotalRunTimer());
        return 1;
    }

    int onEnableClearBackGround(lua_State *lua)
    {
        const bool   clear      = lua_toboolean(lua, 1) ? true : false;
        DEVICE *device          = DEVICE::getInstance();
        device->clearBackGround = clear;
        return 0;
    }

    int onPauseGameLua(lua_State *)
    {
        DEVICE *device = DEVICE::getInstance();
        device->pauseGame();
        return 0;
    }

    int onResumeGameLua(lua_State *)
    {
        DEVICE *device = DEVICE::getInstance();
        device->resumeGame();
        return 0;
    }

    int onCreateTextureLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top < 4)
        {
            return lua_error_debug(lua,"expected: {[table_pixel: RR,GG,BB] or [table_pixel: RR,GG,BB,AA]} [width][height][channel][*nickName][*fileNamePng2Save]");
        }
        TEXTURE_MANAGER *man                 = TEXTURE_MANAGER::getInstance();
        const int              hasTablePixel = lua_type(lua, 1);
        const unsigned int     sTablePixels  = (hasTablePixel == LUA_TTABLE) ? lua_rawlen(lua, 1) : 0;
        const unsigned int     width         = luaL_checkinteger(lua, 2);
        const unsigned int     height        = luaL_checkinteger(lua, 3);
        const unsigned int     channel       = luaL_checkinteger(lua, 4);
        const char *           nickName      = top > 4 && lua_type(lua, 5) == LUA_TSTRING ? lua_tostring(lua, 5) : nullptr;
        const char *           saveAs        = top > 5 ? luaL_checkstring(lua, 6) : nullptr;
        const bool             ret           = man->existTexture(nickName);
        if (ret)
        {
            lua_pushstring(lua, nickName);
            return 1;
        }
        if (saveAs && man->existTexture(saveAs))
        {
            lua_pushstring(lua, saveAs);
            return 1;
        }
        if (nickName == nullptr && saveAs == nullptr)
            nickName = getRandomNameTexture();
        if (nickName == nullptr && saveAs)
            nickName = saveAs;

        if (sTablePixels == 0)
        {
            return lua_error_debug(lua, "expected: [table_pixel], got table with size ZERO");
        }
        const unsigned int totalPixel = width * height * channel;
        if (totalPixel != sTablePixels)
        {
            lua_pushnil(lua);
            if (width == 0)
            {
                return lua_error_debug(lua, "argument:[width], got zero!");
            }
            if (height == 0)
            {
                return lua_error_debug(lua, "argument:[height], got zero!");
            }
            if (channel == 0)
            {
                return lua_error_debug(lua, "argument:[channel] (3 or 4), got zero!");
            }
            return lua_error_debug(lua, "table:[table_pixel] expected [%d], got [%d]!", totalPixel, sTablePixels);
        }

        std::vector<unsigned char> pixel;
        pixel.resize(totalPixel + 1);
        pixel[totalPixel] = 0;
        getArrayFromTablePixels(lua, 1, pixel.data(), totalPixel);

        TEXTURE *texture = man->load(width, height, pixel.data(), nickName, 8, (unsigned short int)channel);
        if (saveAs)
        {
            std::string strSaveAs(saveAs);
            const int   len = strlen(saveAs);
            if (len > 4)
            {
                const char *f = &saveAs[len - 4];
                if (strcasecmp(f, ".png") != 0)
                    strSaveAs += ".png";
            }
            else
            {
                strSaveAs += ".png";
            }
            char strMessageError[1024] = "";
            bool _ret = man->saveDataAsPNG(strSaveAs.c_str(), pixel, channel, width, height, strMessageError, sizeof(strMessageError));
            if (_ret)
            {
                lua_pushstring(lua, strSaveAs.c_str());
                return 1;
            }
            else
            {
                lua_print_line(lua,TYPE_LOG_ERROR,"error on save image [%s]\nError [%s]!", strSaveAs.c_str(),strMessageError);
            }
        }
        if (texture != nullptr)
            lua_pushstring(lua, nickName);
        else
            lua_pushnil(lua);
        return 1;
    }

    int onExistTextureLua(lua_State *lua)
    {
        const char *fileNameTexture = luaL_checkstring(lua, 1);
        const bool  ret             = TEXTURE_MANAGER::getInstance()->existTexture(fileNameTexture);
        lua_pushboolean(lua, ret);
        return 1;
    }

    static bool print_tag(const char * arg, int & index)
    {
        if(arg)
        {
            if(strcasecmp(arg,"info") == 0)
            {
                log_util::print_colored(COLOR_TERMINAL_GREEN,"%s","INFO ");
                index += 1;
                return true;
            }
            else if(strcasecmp(arg,"warn") == 0)
            {
                log_util::print_colored(COLOR_TERMINAL_YELLOW,"%s","WARN ");
                index += 1;
                return true;
            }
            else if(strcasecmp(arg,"error") == 0)
            {
                log_util::print_colored(COLOR_TERMINAL_RED,"%s","ERROR ");
                index += 1;
                return true;
            }
        }
        return false;
    }

    static bool print_line(lua_State *lua,const char * arg, int & index)
    {
        if(arg && strcasecmp(arg,"line") == 0)//print file and line
        {
            lua_Debug ar;
            memset(&ar, 0, sizeof(lua_Debug));
            if (lua_getstack(lua, 1, &ar))
            {
                if (lua_getinfo(lua, "nSl", &ar))
                {
                    log_util::print_colored(COLOR_TERMINAL_CIAN,"File[%s] line[%d]\n", log_util::basename(ar.short_src), ar.currentline);
                    index += 1;
                    return true;
                }
                else
                {
                    ERROR_AT(__LINE__,__FILE__,"Could not get the line and file");
                }
            }
            else
            {
                ERROR_AT(__LINE__,__FILE__,"Could not get stack from LUA");
            }
        }
        return false;
    }

    static bool set_color_to_print(const char * arg, int & index, COLOR_TERMINAL & color_out)
    {
        if(arg)
        {
            static const std::map<std::string,COLOR_TERMINAL> map_color =
            {
                {"white", COLOR_TERMINAL_WHITE     },
                {"red", COLOR_TERMINAL_RED 	       },
                {"yellow", COLOR_TERMINAL_YELLOW   },
                {"green", COLOR_TERMINAL_GREEN 	   },
                {"blue", COLOR_TERMINAL_BLUE 	   },
                {"magenta", COLOR_TERMINAL_MAGENTA },
                {"cian", COLOR_TERMINAL_CIAN 	   },
                {"WHITE", COLOR_TERMINAL_WHITE     },
                {"RED", COLOR_TERMINAL_RED 	       },
                {"YELLOW", COLOR_TERMINAL_YELLOW   },
                {"GREEN", COLOR_TERMINAL_GREEN 	   },
                {"BLUE", COLOR_TERMINAL_BLUE 	   },
                {"MAGENTA", COLOR_TERMINAL_MAGENTA },
                {"CIAN", COLOR_TERMINAL_CIAN 	   },
            };
            
            const auto & term  = map_color.find(arg);
        
            if (term != map_color.end())
            {
                index += 1;
                color_out = term->second;
                return true;
            }
        }
        return false;
    }

    static int __luaB_print_color(lua_State *lua)
    {
        int index				= 1;
        const int n				= lua_gettop(lua); /* number of arguments */
        const char * arg		= lua_type(lua,index) == LUA_TSTRING ? lua_tostring(lua,index) : nullptr;
        COLOR_TERMINAL my_color = COLOR_TERMINAL_WHITE;
        if(arg)
        {
            if(print_tag(arg,index) == true)
            {
                arg		= lua_type(lua,index) == LUA_TSTRING ? lua_tostring(lua,index) : nullptr;
                if(print_line(lua,arg,index) == true)
                {
                    arg		= lua_type(lua,index) == LUA_TSTRING ? lua_tostring(lua,index) : nullptr;
                    set_color_to_print(arg,index,my_color);
                }
                else if(set_color_to_print(arg,index,my_color) == true)
                {
                    arg		= lua_type(lua,index) == LUA_TSTRING ? lua_tostring(lua,index) : nullptr;
                    print_line(lua,arg,index);
                }
            }
            else if(print_line(lua,arg,index) == true)
            {
                arg		= lua_type(lua,index) == LUA_TSTRING ? lua_tostring(lua,index) : nullptr;
                set_color_to_print(arg,index,my_color);
            }
            else if(set_color_to_print(arg,index,my_color) == true)
            {
                arg		= lua_type(lua,index) == LUA_TSTRING ? lua_tostring(lua,index) : nullptr;
                print_line(lua,arg,index);
            }
        }

        lua_getglobal(lua, "tostring");
        std::string all_msg;
        for (int i = index; i <= n; ++i)
        {
            size_t l;
            lua_pushvalue(lua, -1); /* function to be called */
            lua_pushvalue(lua, i);  /* value to print */
            lua_call(lua, 1, 1);
            const char *s = lua_tolstring(lua, -1, &l); /* get result */
            if (s == nullptr)
                return lua_error_debug(lua, "'tostring' must return a string to 'print'");
            if(i > index)
                all_msg += "\t";
            all_msg += s;
            lua_pop(lua, 1); /* pop result */
        }
        log_util::print_colored(my_color,"%s\n", all_msg.c_str());
        return 0;
    }

    int onCompressFile(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top < 1)
        {
            return lua_error_debug(lua, "expected: mbm.compress(string fileNameIn,*string fileNameOut,*number level)");
        }
        MINIZ              miniz;
        char               strErr[1024] = "";
        const char *       fileNameIn   = luaL_checkstring(lua, 1);
        const char *       fileNameOut = (top > 1 && lua_type(lua, 2) == LUA_TSTRING) ? lua_tostring(lua, 2) : fileNameIn;
        const unsigned int level       = (top > 1 && lua_type(lua, 2) == LUA_TNUMBER)
                                       ? lua_tointeger(lua, 2)
                                       : (top > 2 ? luaL_checkinteger(lua, 3) : MZ_UBER_COMPRESSION);

        std::string strOut(fileNameOut);
        if (strcasecmp(fileNameOut, fileNameIn) == 0)
            strOut += ".out.tmp";
        if (level > MZ_UBER_COMPRESSION)
            lua_print_line(lua,TYPE_LOG_WARN,"compress value for %d .", MZ_UBER_COMPRESSION);
        if (miniz.compressFile(fileNameIn, strOut.c_str(), level, strErr))
        {
            if (strcasecmp(fileNameOut, fileNameIn) == 0)
            {
                if (remove(fileNameIn))
                    lua_print_line(lua,TYPE_LOG_WARN,"failed on rename file [%s].", fileNameIn);
                if (rename(strOut.c_str(), fileNameIn))
                {
                    lua_pushboolean(lua, 0);
                    lua_print_line(lua,TYPE_LOG_ERROR,"failed on rename file [%s].", fileNameIn);
                }
                else
                {
                    lua_pushboolean(lua, 1);
                }
            }
            else
            {
                lua_pushboolean(lua, 1);
            }
        }
        else
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"failed to compress file [%s] -> [%s].\n[%s]", fileNameIn, fileNameOut, strErr);
            lua_pushboolean(lua, 0);
        }
        return 1;
    }

    int onDecompressFile(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top < 1)
        {
            return lua_error_debug(lua, "expected: mbm.decompress(fileNameIn,fileNameOut)");
        }
        MINIZ       miniz;
        char        strErr[1024] = "";
        const char *fileNameIn   = luaL_checkstring(lua, 1);
        const char *fileNameOut  = top > 1 ? luaL_checkstring(lua, 2) : fileNameIn;

        std::string strOut(fileNameOut);
        if (strcasecmp(fileNameOut, fileNameIn) == 0)
            strOut += ".out.tmp";
        if (miniz.decompressFile(fileNameIn, strOut.c_str(), strErr))
        {
            if (strcasecmp(fileNameOut, fileNameIn) == 0)
            {
                if (remove(fileNameIn))
                    lua_print_line(lua,TYPE_LOG_WARN,"failed on rename file [%s].", fileNameIn);
                if (rename(strOut.c_str(), fileNameIn))
                {
                    lua_pushboolean(lua, 0);
                    lua_print_line(lua,TYPE_LOG_ERROR,"failed on rename file [%s].", fileNameIn);
                }
                else
                {
                    lua_pushboolean(lua, 1);
                }
            }
            else
            {
                lua_pushboolean(lua, 1);
            }
        }
        else
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"failed to uncompress file [%s] -> [%s]. \n [%s]", fileNameIn, fileNameOut,strErr);
            lua_pushboolean(lua, 0);
        }
        return 1;
    }

    int onExistFile(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top < 1)
        {
            lua_pushboolean(lua, 0);
            lua_pushnil(lua);
        }
        else
        {
            const char *fileName = luaL_checkstring(lua, 1);
            const char *ret      = util::existFile(fileName);
            lua_pushboolean(lua, ret ? 1 : 0);
            if (ret)
                lua_pushstring(lua, ret);
            else
                lua_pushnil(lua);
        }
        return 2;
    }

    int onStopFlag(lua_State *lua)
    {
        DEVICE *        device      = DEVICE::getInstance();
        device->bOnErrorStopScript  = lua_toboolean(lua, 1) ? true : false;
        return 0;
    }



    int onGetKeyCode(lua_State *lua)
    {
        const char *whatKey = luaL_checkstring(lua, 1);
        const int   key     = getKeyCode(whatKey);
        lua_pushinteger(lua, key);
        return 1;
    }

    int onGetKeyName(lua_State *lua)
    {
        const int   whatKey = luaL_checkinteger(lua, 1);
        const char *desc    = getKeyName(whatKey);
        lua_pushstring(lua, desc);
        return 1;
    }

    int onIsCapitalKeyOn(lua_State *lua)
    {
        DEVICE *device = DEVICE::getInstance();
        lua_pushboolean(lua, device->ptrManager->keyCapsLockState);
        return 1;
    }

    int onClearGlobals(lua_State *)
    {
        DEVICE *device = DEVICE::getInstance();
        std::map<std::string,DYNAMIC_VAR*> map_globals;
        const auto & globals_lua = get_globals_lua();
        for(const auto & global_name :  globals_lua)
        {
            DYNAMIC_VAR* dynamic_var = device->lsDynamicVarGlobal[global_name];
            if(dynamic_var)
            {
                device->lsDynamicVarGlobal[global_name] = nullptr;
                map_globals[global_name] = dynamic_var;
            }
        }
        for (const auto & dynamic_var : device->lsDynamicVarGlobal)
        {
            DYNAMIC_VAR *dVar = dynamic_var.second;
            if(dVar)
                delete dVar;
        }
        device->lsDynamicVarGlobal.clear();
        for(const auto & global_name :  globals_lua)
        {
            DYNAMIC_VAR* dynamic_var = map_globals[global_name];
            if(dynamic_var)
            {
                device->lsDynamicVarGlobal[global_name] = dynamic_var;
            }
        }
        return 0;
    }
#if defined USE_PLUSAES
    // Local file-size helper to avoid crossing CRT boundaries.
    // util::getSizeFile lives in core_mbm.dll and would call fseek/ftell
    // from a different CRT than the one that owns the FILE* opened here.
    static bool getSizeFileLocal(FILE *fp, size_t *sizeOut)
    {
        if (fp == nullptr)
            return false;
        if (fseek(fp, 0, SEEK_SET))
            return false;
        if (fseek(fp, 0, SEEK_END))
            return false;
        const long t = ftell(fp);
        if (t <= 0)
            return false;
        *sizeOut = static_cast<size_t>(t);
        if (fseek(fp, 0, SEEK_SET))
            return false;
        return true;
    }

    const bool encrypt_stream_plusaes(FILE* infp, FILE* outfp, const char (* passwd)[17], const int passlen, const unsigned char (*iv)[16], char* errorOut)
    {
        // encrypt
        size_t read_bytes = 0;
        if (getSizeFileLocal(infp, &read_bytes))
        {
            std::vector<unsigned char> raw_data(read_bytes);

            if (fread(raw_data.data(), read_bytes, 1, infp) == 1)
            {
                const unsigned long encrypted_size = plusaes::get_padded_encrypted_size(raw_data.size());
                std::vector<unsigned char> encrypted(encrypted_size);

                const std::vector<unsigned char> key = plusaes::key_from_string(passwd); // 16-char = 128-bit

                plusaes::Error error = plusaes::encrypt_cbc(raw_data.data(), raw_data.size(), key.data(), key.size(), iv, encrypted.data(), encrypted.size(), true);
                switch (error)
                {
                    case plusaes::Error::kErrorOk:
                    {
                        // we do not write padding
                        if (fwrite(encrypted.data(), 1, encrypted_size, outfp) == encrypted_size)
                        {
                            return true;
                        }
                        else
                        {
                            if (errorOut)
                            {
                                snprintf(errorOut, 511, "plusaes::Error: failed to write file");
                            }
                            return false;
                        }
                    }
                    case plusaes::Error::kErrorInvalidDataSize:
                        if (errorOut)
                        {
                            snprintf(errorOut, 511, "plusaes::Error: invalid data size");
                        }
                        return false;
                    case plusaes::Error::kErrorInvalidKeySize:
                        if (errorOut)
                        {
                            snprintf(errorOut, 511, "plusaes::Error: invalid key size, key_size != 16 && key_size != 24 && key_size != 32");
                        }
                        return false;
                    case plusaes::Error::kErrorInvalidBufferSize:
                        if (errorOut)
                        {
                            snprintf(errorOut, 511, "plusaes::Error: Invalid buffer size");
                        }
                        return false;
                    case plusaes::Error::kErrorInvalidKey:
                        if (errorOut)
                        {
                            snprintf(errorOut, 511, "plusaes::Error: Invalid key");
                        }
                        return false;
                    case plusaes::Error::kErrorDeprecated:
                        if (errorOut)
                        {
                            snprintf(errorOut, 511, "plusaes::Error: Error deprecated");
                        }
                        return false;
                    case plusaes::Error::kErrorInvalidIvSize:
                        if (errorOut)
                        {
                            snprintf(errorOut, 511, "plusaes::Error: Invalid Iv Size");
                        }
                        return false;
                    case plusaes::Error::kErrorInvalidTagSize:
                        if (errorOut)
                        {
                            snprintf(errorOut, 511, "plusaes::Error: Invalid tag size");
                        }
                        return false;
                    case plusaes::Error::kErrorInvalidTag:
                        if (errorOut)
                        {
                            snprintf(errorOut, 511, "plusaes::Error: Invalid tag");
                        }
                        return false;
                    default:
                        if (errorOut)
                        {
                            snprintf(errorOut, 511, "unknown error");
                        }
                        return false;
                }
            }
            else
            {
                if (errorOut)
                {
                    snprintf(errorOut, 511, "failed to read file");
                }
                return false;
            }
        }
        else
        {
            if (errorOut)
            {
                snprintf(errorOut, 511, "failed to get size file");
            }
            return false;
        }

    }
    const bool decrypt_stream_plusaes(FILE* infp, FILE* outfp, const char (*passwd)[17], const int passlen, const unsigned char (*iv)[16], char* errorOut)
    {
        size_t read_bytes = 0;
        if (getSizeFileLocal(infp, &read_bytes))
        {
            std::vector<unsigned char> encrypted(read_bytes);

            if (fread(encrypted.data(), read_bytes, 1, infp) == 1)
            {
                // decrypt
                unsigned long padded_size = 0;

                std::vector<unsigned char> decrypted(read_bytes);

                const std::vector<unsigned char> key = plusaes::key_from_string(passwd); // 16-char = 128-bit

                plusaes::Error error = plusaes::decrypt_cbc(encrypted.data(), encrypted.size(), key.data(), key.size(), iv, decrypted.data(), decrypted.size(), &padded_size);
                switch (error)
                {
                case plusaes::Error::kErrorOk:
                {
                    // we do not write padding
                    const size_t new_size = read_bytes - padded_size;
                    if (fwrite(decrypted.data(), 1, new_size, outfp) == new_size)
                    {
                        return true;
                    }
                    else
                    {
                        if (errorOut)
                        {
                            snprintf(errorOut, 511, "plusaes::Error: failed to write file");
                        }
                        return false;
                    }
                    return true;
                }
                case plusaes::Error::kErrorInvalidDataSize:
                    if (errorOut)
                    {
                        snprintf(errorOut, 511, "plusaes::Error: invalid data size");
                    }
                    return false;
                case plusaes::Error::kErrorInvalidKeySize:
                    if (errorOut)
                    {
                        snprintf(errorOut, 511, "plusaes::Error: invalid key size, key_size != 16 && key_size != 24 && key_size != 32");
                    }
                    return false;
                case plusaes::Error::kErrorInvalidBufferSize:
                    if (errorOut)
                    {
                        snprintf(errorOut, 511, "plusaes::Error: Invalid buffer size");
                    }
                    return false;
                case plusaes::Error::kErrorInvalidKey:
                    if (errorOut)
                    {
                        snprintf(errorOut, 511, "plusaes::Error: Invalid key");
                    }
                    return false;
                case plusaes::Error::kErrorDeprecated:
                    if (errorOut)
                    {
                        snprintf(errorOut, 511, "plusaes::Error: Error deprecated");
                    }
                    return false;
                case plusaes::Error::kErrorInvalidIvSize:
                    if (errorOut)
                    {
                        snprintf(errorOut, 511, "plusaes::Error: Invalid Iv Size");
                    }
                    return false;
                case plusaes::Error::kErrorInvalidTagSize:
                    if (errorOut)
                    {
                        snprintf(errorOut, 511, "plusaes::Error: Invalid tag size");
                    }
                    return false;
                case plusaes::Error::kErrorInvalidTag:
                    if (errorOut)
                    {
                        snprintf(errorOut, 511, "plusaes::Error: Invalid tag");
                    }
                    return false;
                default:
                    if (errorOut)
                    {
                        snprintf(errorOut, 511, "unknown error");
                    }
                    return false;
                }
            }
            else
            {
                if (errorOut)
                {
                    snprintf(errorOut, 511, "failed to read file");
                }
                return false;
            }
        }
        else
        {
            if (errorOut)
            {
                snprintf(errorOut, 511, "failed to get size file");
            }
            return false;
        }
    }
#endif

    int onEncryptFile(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top < 1)
        {
            return lua_error_debug(lua, "expected: mbm.encrypt(string fileNameIn,*string fileNameOut,*string password)");
        }
        char        strErr[512] = "";
        const char *fileNameIn  = luaL_checkstring(lua, 1);
        const char *fileNameOut = (top > 1) ? luaL_checkstring(lua, 2) : fileNameIn;
        const char *password    = (top > 2) ? luaL_checkstring(lua, 3) : __std_p();

#if defined USE_PLUSAES
        const unsigned char* iv = __iv_p();
        if (top > 3)
        {
            size_t len = 0;
            const char* strIv = luaL_checklstring(lua, 4, &len);
            if (len != 16)
            {
                return lua_error_debug(lua, "when using custom iv it must be 16 bytes length");
            }
            iv = reinterpret_cast<const unsigned char*>(strIv);
        }
#endif
        std::string strOut(fileNameOut);
        if (strcasecmp(fileNameOut, fileNameIn) == 0)
            strOut += ".out.tmp";
        const int passlen = strlen(password);
        char good_password[17] = {0};
        memcpy(good_password, __std_p(), sizeof(good_password) - 1);
        memcpy(good_password, password, std::min<int>(sizeof(good_password) - 1, passlen));
        FILE *fp1 = util::openFile(fileNameIn, "rb");
        if (fp1 == nullptr)
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"failed to open file [%s]", fileNameIn);
            lua_pushboolean(lua, 0);
            return 1;
        }
        FILE *fp2 = util::openFile(strOut.c_str(), "wb");
        if (fp2 == nullptr)
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"failed to open file [%s]", strOut.c_str());
            lua_pushboolean(lua, 0);
            return 1;
        }
#ifdef USE_AESCRYPT
        if (encrypt_stream(fp1, fp2, good_password, sizeof(good_password) - 1, strErr))
        {
            fclose(fp1);
            fclose(fp2);
            if (strcasecmp(fileNameOut, fileNameIn) == 0)
            {
                if (remove(fileNameIn))
                    lua_print_line(lua,TYPE_LOG_WARN,"failed on rename file [%s].", fileNameIn);
                if (rename(strOut.c_str(), fileNameIn))
                {
                    lua_pushboolean(lua, 0);
                    lua_print_line(lua,TYPE_LOG_ERROR,"failed on rename file [%s].", fileNameIn);
                }
                else
                {
                    lua_pushboolean(lua, 1);
                }
            }
            else
            {
                lua_pushboolean(lua, 1);
            }
        }
        else
        {
            fclose(fp1);
            fclose(fp2);
            lua_print_line(lua,TYPE_LOG_ERROR,"failed on cript file [%s] -> [%s].\n[%s]", fileNameIn, fileNameOut, strErr);
            lua_pushboolean(lua, 0);
        }
#elif defined USE_PLUSAES
        if (encrypt_stream_plusaes(fp1, fp2, reinterpret_cast<const char(*)[17]>(good_password), sizeof(good_password) - 1, reinterpret_cast<const unsigned char (*)[16]>(iv), strErr))
        {
            fclose(fp1);
            fclose(fp2);
            if (strcasecmp(fileNameOut, fileNameIn) == 0)
            {
                if (remove(fileNameIn))
                    lua_print_line(lua, TYPE_LOG_WARN, "failed on rename file [%s].", fileNameIn);
                if (rename(strOut.c_str(), fileNameIn))
                {
                    lua_pushboolean(lua, 0);
                    lua_print_line(lua, TYPE_LOG_ERROR, "failed on rename file [%s].", fileNameIn);
                }
                else
                {
                    lua_pushboolean(lua, 1);
                }
            }
            else
            {
                lua_pushboolean(lua, 1);
            }
        }
        else
        {
            fclose(fp1);
            fclose(fp2);
            lua_print_line(lua, TYPE_LOG_ERROR, "failed on cript file [%s] -> [%s].\n[%s]", fileNameIn, fileNameOut, strErr);
            lua_pushboolean(lua, 0);
        }
#else
    #error ("You need to define USE_AESCRYPT or USE_PLUSAES on project")
#endif
        return 1;
    }

    int onDecryptFile(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top < 1)
        {
            return lua_error_debug(lua, "expected: mbm.decrypt(string fileNameIn,*string fileNameOut,*string password)");
        }
        char        strErr[512] = "";
        const char *fileNameIn  = luaL_checkstring(lua, 1);
        const char *fileNameOut = (top > 1) ? luaL_checkstring(lua, 2) : fileNameIn;
        const char *password    = (top > 2) ? luaL_checkstring(lua, 3) : __std_p();
#if defined USE_PLUSAES
        const unsigned char* iv = __iv_p();
        if (top > 3)
        {
            size_t len = 0;
            const char* strIv = luaL_checklstring(lua, 4, &len);
            if (len != 16)
            {
                return lua_error_debug(lua, "when using custom iv it must be 16 bytes length");
            }
            iv = reinterpret_cast<const unsigned char*>(strIv);
        }
#endif

        std::string strOut(fileNameOut);
        if (strcasecmp(fileNameOut, fileNameIn) == 0)
            strOut += ".out.tmp";
        const int passlen = strlen(password);
        char good_password[17] = { 0 };
        memcpy(good_password, __std_p(), sizeof(good_password) - 1);
        memcpy(good_password, password, std::min<int>(sizeof(good_password) - 1, passlen));
        FILE *    fp1     = util::openFile(fileNameIn, "rb");
        if (fp1 == nullptr)
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"failed to open file [%s]", fileNameIn);
            lua_pushboolean(lua, 0);
            return 1;
        }
        FILE *fp2 = util::openFile(strOut.c_str(), "wb");
        if (fp2 == nullptr)
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"failed to open file [%s]", strOut.c_str());
            lua_pushboolean(lua, 0);
            return 1;
        }
#ifdef USE_AESCRYPT
        if (decrypt_stream(fp1, fp2, good_password, sizeof(good_password) - 1, strErr))
        {
            fclose(fp1);
            fclose(fp2);
            if (strcasecmp(fileNameOut, fileNameIn) == 0)
            {
                if (remove(fileNameIn))
                    lua_print_line(lua,TYPE_LOG_WARN,"failed on rename file [%s].", fileNameIn);
                if (rename(strOut.c_str(), fileNameIn))
                {
                    lua_pushboolean(lua, 0);
                    lua_print_line(lua,TYPE_LOG_ERROR,"failed on rename file [%s].", fileNameIn);
                }
                else
                {
                    lua_pushboolean(lua, 1);
                }
            }
            else
            {
                lua_pushboolean(lua, 1);
            }
        }
        else
        {
            fclose(fp1);
            fclose(fp2);
            lua_print_line(lua,TYPE_LOG_ERROR,"failed on uncript file [%s] -> [%s].\n[%s]", fileNameIn, fileNameOut, strErr);
            lua_pushboolean(lua, 0);
        }
#elif defined USE_PLUSAES
        if (decrypt_stream_plusaes(fp1, fp2, reinterpret_cast<const char(*)[17]>(good_password), sizeof(good_password) - 1, reinterpret_cast<const unsigned char (*)[16]>(iv), strErr))
        {
            fclose(fp1);
            fclose(fp2);
            if (strcasecmp(fileNameOut, fileNameIn) == 0)
            {
                if (remove(fileNameIn))
                    lua_print_line(lua, TYPE_LOG_WARN, "failed on rename file [%s].", fileNameIn);
                if (rename(strOut.c_str(), fileNameIn))
                {
                    lua_pushboolean(lua, 0);
                    lua_print_line(lua, TYPE_LOG_ERROR, "failed on rename file [%s].", fileNameIn);
                }
                else
                {
                    lua_pushboolean(lua, 1);
                }
            }
            else
            {
                lua_pushboolean(lua, 1);
            }
        }
        else
        {
            fclose(fp1);
            fclose(fp2);
            lua_print_line(lua, TYPE_LOG_ERROR, "failed on uncript file [%s] -> [%s].\n[%s]", fileNameIn, fileNameOut, strErr);
            lua_pushboolean(lua, 0);
        }
#else
#error ("You need to define USE_AESCRYPT or USE_PLUSAES on project")
#endif
        return 1;
    }

    int onGetSceneName(lua_State *lua)
    {
        DEVICE *device = DEVICE::getInstance();
        lua_pushstring(lua, device->scene->getSceneName());
        return 1;
    }
   
    int onOpenFile(lua_State *lua)
    {
        return openMultiSingleFile(lua, 0);
    }

    int onOpenMultiFile(lua_State *lua)
    {
        return openMultiSingleFile(lua, 1);
    }

    void fillVarTableShaderList(lua_State *lua, const std::vector<VAR_CFG *> &lsVar, const TYPE_VAR_PRINT typeVarPrint)
    {
        lua_newtable(lua); // var
        for (auto var : lsVar)
        {
            lua_newtable(lua); // each var
            const float *   out = nullptr;
            switch (typeVarPrint)
            {
                case VAR_PRINT_DEFAULT: { out = var->Default;}break;
                case VAR_PRINT_MIN: { out = var->Min;}break;
                case VAR_PRINT_MAX: { out = var->Max;}break;
            }
            switch (var->type)
            {
                case VAR_FLOAT:
                {
                    lua_pushnumber(lua,out[0]);
                    lua_rawseti(lua, -2, 1);
                }
                break;
                case VAR_VECTOR2:
                {
                    lua_pushnumber(lua, out[0]);
                    lua_rawseti(lua, -2, 1);

                    lua_pushnumber(lua, out[1]);
                    lua_rawseti(lua, -2, 2);
                }
                break;
                case VAR_COLOR_RGB:
                case VAR_VECTOR:
                {
                    lua_pushnumber(lua, out[0]);
                    lua_rawseti(lua, -2, 1);

                    lua_pushnumber(lua, out[1]);
                    lua_rawseti(lua, -2, 2);

                    lua_pushnumber(lua, out[2]);
                    lua_rawseti(lua, -2, 3);
                }
                break;
                case VAR_COLOR_RGBA:
                {
                    lua_pushnumber(lua, out[0]);
                    lua_rawseti(lua, -2, 1);

                    lua_pushnumber(lua, out[1]);
                    lua_rawseti(lua, -2, 2);

                    lua_pushnumber(lua, out[2]);
                    lua_rawseti(lua, -2, 3);

                    lua_pushnumber(lua, out[3]);
                    lua_rawseti(lua, -2, 4);
                }
                break;
            }
            lua_setfield(lua, -2, var->name.c_str());
        }
        switch (typeVarPrint)
        {
            case VAR_PRINT_DEFAULT:
            {
                lua_setfield(lua, -2, "var"); // var
            }
            break;
            case VAR_PRINT_MIN:
            {
                lua_setfield(lua, -2, "min"); // var
            }
            break;
            case VAR_PRINT_MAX:
            {
                lua_setfield(lua, -2, "max"); // var
            }
            break;
        }
    }

    void fillTableShaderList(lua_State *lua, const std::vector<SHADER_CFG *> &lsShader, const bool bMin, const bool bMax,const bool bCode)
    {

        for (unsigned int i = 0, j = 1; i < lsShader.size(); ++i, ++j)
        {
            lua_newtable(lua); // shader
            const SHADER_CFG *shader     = lsShader.at(i);
            const char *           shaderName = shader->fileName.c_str();
            lua_pushstring(lua, shaderName);
            lua_setfield(lua, -2, "name");

            if(bCode)
            {
                const char *code = shader->codeShader.c_str();
                lua_pushstring(lua, code);
                lua_setfield(lua, -2, "code");
            }

            fillVarTableShaderList(lua, shader->lsVar, VAR_PRINT_DEFAULT);
            if (bMin)
                fillVarTableShaderList(lua, shader->lsVar, VAR_PRINT_MIN);
            if (bMax)
                fillVarTableShaderList(lua, shader->lsVar, VAR_PRINT_MAX);
            lua_rawseti(lua, -2, j); // shader
        }
    }

    int onGetShaderList(lua_State *lua)
    {
        DEVICE *device         = DEVICE::getInstance();
        const int    top       = lua_gettop(lua);
        const bool   bDetail   = top > 0 && lua_toboolean(lua, 1) ? true : false;
        const char * strFilter = top > 1 && lua_type(lua, 2) == LUA_TSTRING ? lua_tostring(lua, 2) : nullptr;
        const bool   bMin      = top > 2 && lua_toboolean(lua, 3) ? true : false;
        const bool   bMax      = top > 3 && lua_toboolean(lua, 4) ? true : false;
        const bool   bCode     = top > 4 && lua_toboolean(lua, 5) ? true : false;
        lua_newtable(lua);
        if (bDetail)
        {
            bool isPs = false;
            bool isVs = false;
            if (strFilter == nullptr || strcasecmp(strFilter, "ps") == 0 || strcasecmp(strFilter, "fs") == 0)
            {
                fillTableShaderList(lua, device->cfg.lsPs, bMin, bMax,bCode);
                isPs = true;
            }
            if (strFilter == nullptr || strcasecmp(strFilter, "vs") == 0)
            {
                fillTableShaderList(lua, device->cfg.lsVs, bMin, bMax,bCode);
                isVs = true;
            }
            if (isPs == false && isVs == false && strFilter)
            {
                std::vector<SHADER_CFG *> lsShader;
                for (auto shader : device->cfg.lsPs)
                {
                    if (shader->fileName.compare(strFilter) == 0)
                    {
                        lsShader.push_back(shader);
                        break;
                    }
                }
                if (lsShader.size() == 0)
                {
                    for (auto shader : device->cfg.lsVs)
                    {
                        if (shader->fileName.compare(strFilter) == 0)
                        {
                            lsShader.push_back(shader);
                            break;
                        }
                    }
                }
                if (lsShader.size())
                    fillTableShaderList(lua, lsShader, bMin, bMax,bCode);
                else
                {
                    lua_print_line(lua,TYPE_LOG_ERROR, "filter shader not found [%s]", strFilter);
                }
            }
        }
        else
        {
            if (strFilter == nullptr || strcasecmp(strFilter, "ps") == 0)
            {
                for (unsigned int i = 0, j = 1; i < device->cfg.lsPs.size(); ++i, ++j)
                {
                    SHADER_CFG *shader     = device->cfg.lsPs[i];
                    const char *     shaderName = shader->fileName.c_str();
                    lua_pushstring(lua, shaderName);
                    lua_rawseti(lua, -2, j);
                }
            }
            if (strFilter == nullptr || strcasecmp(strFilter, "vs") == 0)
            {
                for (unsigned int i = 0, j = 1; i < device->cfg.lsVs.size(); ++i, ++j)
                {
                    SHADER_CFG *shader     = device->cfg.lsVs[i];
                    const char *     shaderName = shader->fileName.c_str();
                    lua_pushstring(lua, shaderName);
                    lua_rawseti(lua, -2, j);
                }
            }
        }
        return 1;
    }

    int onExistShader(lua_State *lua)
    {
        DEVICE *     device     = DEVICE::getInstance();
        const char *const shaderName = luaL_checkstring(lua, 1);
        for (auto shader : device->cfg.lsPs)
        {
            if (shader->fileName.compare(shaderName) == 0)
            {
                lua_pushboolean(lua, 1);
                return 1;
            }
        }
        for (auto shader : device->cfg.lsVs)
        {
            if (shader->fileName.compare(shaderName) == 0)
            {
                lua_pushboolean(lua, 1);
                return 1;
            }
        }
        lua_pushboolean(lua, 0);
        return 1;
    }

    bool fillVarShadersFromMap(std::map<std::string, std::vector<float> *> &lsMapVars, const char *minMaxDefault,std::vector<std::string> &out)
    {
        unsigned int i = 0;
        for (const auto & lsMapVar : lsMapVars)
        {
            char                tmp[1024] = "";
            std::vector<float> *lsVec     = lsMapVar.second;
            const unsigned int  s         = lsVec->size();
            if (s == 1)
            {
                snprintf(tmp, sizeof(tmp), "%s %f ", minMaxDefault, lsVec->at(0));
            }
            else if (s == 2)
            {
                snprintf(tmp, sizeof(tmp), "%s %f %f ", minMaxDefault, lsVec->at(0), lsVec->at(1));
            }
            else if (s == 3)
            {
                snprintf(tmp, sizeof(tmp), "%s %f %f %f ", minMaxDefault, lsVec->at(0), lsVec->at(1), lsVec->at(2));
            }
            else if (s == 4)
            {
                snprintf(tmp, sizeof(tmp), "%s %f %f %f %f ", minMaxDefault, lsVec->at(0), lsVec->at(1), lsVec->at(2), lsVec->at(3));
            }
            else
            {
                return false;
            }
            if (i < out.size())
                out[i] += tmp;
            else
                out.emplace_back(tmp);
            ++i;
        }
        return true;
    }

    int onSortShader(lua_State *)
    {
        DEVICE *    device = DEVICE::getInstance();
        device->cfg.sortShader();
        return 0;
    }

    int onAddShader(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top == 1 && lua_type(lua, 1) == LUA_TTABLE)
        {
            std::string name;
            getFieldPrimaryFromTable(lua, 1, "name", LUA_TSTRING, &name);
            if (name.size())
            {
                const char *pName = name.c_str();
                const int   len   = name.size();
                if (len > 3)
                {
                    bool        validName = false;
                    bool        isPS      = false;
                    const char *extension = &pName[len - 3];
                    if (strcmp(extension, ".ps") == 0)
                    {
                        isPS      = true;
                        validName = true;
                    }
                    else if (strcmp(extension, ".vs") == 0)
                    {
                        validName = true;
                    }
                    if (validName)
                    {
                        DEVICE *    device = DEVICE::getInstance();
                        SHADER_CFG *shader = device->cfg.getShader(pName);
                        if (shader)
                        {
                            lua_print_line(lua,TYPE_LOG_ERROR,"shader [%s] already exist!", pName);
                            lua_pushboolean(lua, 0);
                            return 1;
                        }
                        std::string code;
                        getFieldPrimaryFromTable(lua, 1, "code", LUA_TSTRING, &code);
                        if (code.size())
                        {
                            struct REFLECT_VARS_FROM_TABLE
                            {
                                std::map<std::string, std::vector<float> *> Vars;
                                std::map<std::string, std::vector<float> *> Min;
                                std::map<std::string, std::vector<float> *> Max;
                                REFLECT_VARS_FROM_TABLE()
                                = default;
                                ~REFLECT_VARS_FROM_TABLE()
                                {
                                    destroyVarMapShaders(Vars);
                                    destroyVarMapShaders(Min);
                                    destroyVarMapShaders(Max);
                                }
                                void destroyVarMapShaders(std::map<std::string, std::vector<float> *> &lsMapVars)
                                {
                                    for (const auto & lsMapVar : lsMapVars)
                                    {
                                        std::vector<float> *lsVec = lsMapVar.second;
                                        delete lsVec;
                                    }
                                }
                                const bool inPairFromTableShader(lua_State *lua, const int                    index,
                                                                 std::map<std::string, std::vector<float> *> *_lsVars,
                                                                 const char *key = "no_key")
                                {
                                    lua_pushnil(lua); /* first key */
                                    while (lua_next(lua, index) != 0)
                                    {
                                        float value = 0.0f;
                                        if (lua_type(lua, -2) == LUA_TSTRING) // key
                                        {
                                            key = lua_tostring(lua, -2);
                                        }
                                        const int tType = lua_type(lua, -1);
                                        if (tType == LUA_TTABLE) // value
                                        {
                                            if (!inPairFromTableShader(lua, index + 2, _lsVars, key))
                                                return false;
                                        }
                                        else if (tType == LUA_TNUMBER)
                                        {
                                            value                            = lua_tonumber(lua, -1);
                                            std::vector<float> *lsCurrentVar = (*_lsVars)[key];
                                            if (lsCurrentVar == nullptr)
                                            {
                                                lsCurrentVar    = new std::vector<float>();
                                                (*_lsVars)[key] = lsCurrentVar;
                                            }
                                            lsCurrentVar->push_back(value);
                                        }
                                        else
                                        {
                                            return false;
                                        }
                                        lua_pop(lua, 1);
                                    }
                                    return true;
                                }

                            } list;

                            lua_getfield(lua, 1, "var");
                            if (lua_type(lua, 2) == LUA_TTABLE)
                            {
                                if (!list.inPairFromTableShader(lua, 2, &list.Vars))
                                    return lua_error_debug(lua, "error getting 'var' from table shader [%s]", pName);
                            }
                            lua_pop(lua, 1);

                            lua_getfield(lua, 1, "min");
                            if (lua_type(lua, 2) == LUA_TTABLE)
                            {
                                if (!list.inPairFromTableShader(lua, 2, &list.Min))
                                    return lua_error_debug(lua, "error getting 'min' from table shader [%s]", pName);
                            }
                            lua_pop(lua, 1);

                            lua_getfield(lua, 1, "max");
                            if (lua_type(lua, 2) == LUA_TTABLE)
                            {
                                if (!list.inPairFromTableShader(lua, 2, &list.Max))
                                    return lua_error_debug(lua, "error getting 'max' from table shader [%s]", pName);
                            }
                            lua_pop(lua, 1);

                            std::vector<std::string> lsDescriptionAsString;
                            shader             = new SHADER_CFG(pName);
                            shader->codeShader = std::move(code);
                            if (list.Vars.size() &&
                                fillVarShadersFromMap(list.Vars, "default", lsDescriptionAsString) == false)
                                return lua_error_debug(lua, "error filling 'var' from table shader [%s]", pName);
                            if (list.Min.size() && fillVarShadersFromMap(list.Min, "min", lsDescriptionAsString) == false)
                                return lua_error_debug(lua, "error filling 'min' from table shader [%s]", pName);
                            if (list.Max.size() && fillVarShadersFromMap(list.Max, "max", lsDescriptionAsString) == false)
                                return lua_error_debug(lua, "error filling 'max' from table shader [%s]", pName);
                            if (list.Min.size() == 0 && list.Vars.size())
                            {
                                if (fillVarShadersFromMap(list.Vars, "min", lsDescriptionAsString) == false)
                                    return lua_error_debug(lua, "error filling 'min' from table shader [%s]", pName);
                            }
                            if (list.Max.size() == 0 && list.Vars.size())
                            {
                                if (fillVarShadersFromMap(list.Vars, "max", lsDescriptionAsString) == false)
                                    return lua_error_debug(lua, "error filling 'max' from table shader [%s]", pName);
                            }

                            unsigned int indexVar = 0;
                            for (auto it = list.Vars.cbegin();
                                 it != list.Vars.cend(); ++it, ++indexVar)
                            {
                                const char *       varName = it->first.c_str();
                                const char *       typeVar = nullptr;
                                const unsigned int s       = it->second->size();
                                if (s == 1)
                                    typeVar = "float";
                                else if (s == 2)
                                    typeVar = "vector2";
                                else if (s == 3)
                                    typeVar = "vector";
                                else if (s == 4)
                                    typeVar = "rgba";
                                else
                                {
                                    delete shader;
                                    return lua_error_debug(lua, "error getting 'var' from table shader [%s]", pName);
                                }
                                if (indexVar < lsDescriptionAsString.size())
                                    shader->addVar(typeVar, varName, lsDescriptionAsString[indexVar].c_str());
                            }
                            if (shader->lsVar.size() != lsDescriptionAsString.size())
                            {
                                delete shader;
                                return lua_error_debug(lua, "error adding 'var' from table shader [%s]", pName);
                            }
                            if (isPS)
                                device->cfg.lsPs.push_back(shader);
                            else
                                device->cfg.lsVs.push_back(shader);

                            lua_pushboolean(lua, 1);
                            return 1;
                        }
                    }
                }
            }
        }
        lua_print_line(lua,TYPE_LOG_ERROR,"expected [tableShader = \n{name = 'name.[ps][vs]',\ncode = 'void main() ...',\nvar = {someVar = {0,0}, otherVar = {5,9,0} }\n} ]");
        lua_pushboolean(lua, 0);
        return 1;
    }

    int onRefresh(lua_State * )
    {
        //force refresh window by sending resize event
        DEVICE *device = DEVICE::getInstance();
        device->refreshDevice();
        return 0;
    }

    int setFakeFps(lua_State * lua)
    {
        unsigned int cicles = luaL_checkinteger(lua,1);
        unsigned int fps    = luaL_checkinteger(lua,2);
        DEVICE *device      = DEVICE::getInstance();
        device->setFakeFps(cicles,fps);
        return 0;
    }

    
    int onDoShuffle(lua_State * lua)
    {
        const int top   = lua_gettop(lua);
        if(top < 2)
            return lua_error_debug(lua, "expected: mbm.shuffle(string msg,string key)");
        const char* msg = luaL_checkstring(lua,1);
        const char* key = luaL_checkstring(lua,2);
        mbm::VIGENERE vigenere(key);
        std::string msg_out;
        vigenere.encrypt(msg,msg_out);
        lua_pushstring(lua,msg_out.c_str());
        return 1;
    }

    int onUndoShuffle(lua_State * lua)
    {
        const int top   = lua_gettop(lua);
        if(top < 2)
            return lua_error_debug(lua, "expected: mbm.undoShuffle(string msg_shuffled,string key)");
        const char* msg = luaL_checkstring(lua,1);
        const char* key = luaL_checkstring(lua,2);
        mbm::VIGENERE vigenere(key);
        std::string msg_out;
        vigenere.decrypt(msg,msg_out);
        lua_pushstring(lua,msg_out.c_str());
        return 1;
    }

    static int onIndexConstants(lua_State *lua) // reading
    {
        const char *         what      = luaL_checkstring(lua, 2);
        if(what)
        {
            if(strcmp(what,"PAUSED") == 0)
                lua_pushinteger(lua,TYPE_ANIMATION_PAUSED);
            else if(strcmp(what,"GROWING") == 0)
                lua_pushinteger(lua,TYPE_ANIMATION_GROWING);
            else if(strcmp(what,"GROWING_LOOP") == 0)
                lua_pushinteger(lua,TYPE_ANIMATION_GROWING_LOOP);
            else if(strcmp(what,"DECREASING") == 0)
                lua_pushinteger(lua,TYPE_ANIMATION_DECREASING);
            else if(strcmp(what,"DECREASING_LOOP") == 0)
                lua_pushinteger(lua,TYPE_ANIMATION_DECREASING_LOOP);
            else if(strcmp(what,"RECURSIVE") == 0)
                lua_pushinteger(lua,TYPE_ANIMATION_RECURSIVE);
            else if(strcmp(what,"RECURSIVE_LOOP") == 0)
                lua_pushinteger(lua,TYPE_ANIMATION_RECURSIVE_LOOP);
            else if(strcasecmp(what,"ADD") == 0)
                lua_pushinteger(lua,1);
            else if(strcasecmp(what,"SUBTRACT") == 0)
                lua_pushinteger(lua,2);
            else if(strcasecmp(what,"REVERSE_SUBTRACT") == 0)
                lua_pushinteger(lua,3);
            else if(strcasecmp(what,"MIN") == 0)
                lua_pushinteger(lua,4);
            else if(strcasecmp(what,"MAX") == 0)
                lua_pushinteger(lua,5);
            else if(strcmp(what,"STAGE_1") == 0)
                lua_pushinteger(lua,1);
            else if(strcmp(what,"STAGE_2") == 0)
                lua_pushinteger(lua,2);
            else if(strcmp(what,"DISABLE") == 0)
                lua_pushinteger(lua,BLEND_DISABLE);
            else if(strcmp(what,"ZERO") == 0)
                lua_pushinteger(lua,BLEND_ZERO);
            else if(strcmp(what,"ONE") == 0)
                lua_pushinteger(lua,BLEND_ONE);
            else if(strcmp(what,"SRC_COLOR") == 0)
                lua_pushinteger(lua,BLEND_SRCCOLOR);
            else if(strcmp(what,"INV_SRC_COLOR") == 0)
                lua_pushinteger(lua,BLEND_INVSRCCOLOR);
            else if(strcmp(what,"SRC_ALPHA") == 0)
                lua_pushinteger(lua,BLEND_SRCALPHA);
            else if(strcmp(what,"INV_SRC_ALPHA") == 0)
                lua_pushinteger(lua,BLEND_INVSRCALPHA);
            else if(strcmp(what,"DEST_ALPHA") == 0)
                lua_pushinteger(lua,BLEND_DESTALPHA);
            else if(strcmp(what,"INV_DEST_ALPHA") == 0)
                lua_pushinteger(lua,BLEND_INVDESTALPHA);
            else if(strcmp(what,"DEST_COLOR") == 0)
                lua_pushinteger(lua,BLEND_DESTCOLOR);
            else if(strcmp(what,"INV_DEST_COLOR") == 0)
                lua_pushinteger(lua,BLEND_INVDESTCOLOR);
            else
                return 0;
            return 1;
        }
        return 0;
    }

    int onSubscribePlugin(lua_State *lua)
    {
        PLUGIN **ud              = static_cast<PLUGIN **>(lua_check_userType(lua,1,1,L_USER_TYPE_PLUGIN));
        PLUGIN * pPlugin         = *ud;
        DEVICE *device           = DEVICE::getInstance();
        const unsigned int index = device->ptrManager->addPlugin(pPlugin);
        if (index == 0xffffffff)
        {
            ERROR_LOG("Could not register plugin...");
            lua_pushinteger(lua,0);
        }
        else
            lua_pushinteger(lua,index + 1);
        return 1;
    }

    int onLoadDetailedTexture(lua_State *lua)
    {
        const int top   = lua_gettop(lua);
        if(top < 1)
            return lua_error_debug(lua, "expected: mbm.loadTexture(string file_name_texture,boolean * alpha = true)");
        const char* file_name_texture      = luaL_checkstring(lua,1);
        const bool  alpha                  = top > 1 ? lua_toboolean(lua,2) : true;
        TEXTURE_MANAGER * texture_manager  = TEXTURE_MANAGER::getInstance();
        TEXTURE * tex                      = texture_manager->load(file_name_texture,alpha);
        return onNewTextureInfoLua(lua, tex);
    }

    inline void addFolderToMap(const char * path, std::map<std::string,std::vector<std::string>> & folderAndFiles)
    {
        std::string str(path);
        const auto it = folderAndFiles.find(path);
        if(it == folderAndFiles.cend())
        {
            std::vector<std::string> files;
            folderAndFiles[path] = std::move(files);
        }
    }

    static void listFilesRecursive(const char * path, std::map<std::string,std::vector<std::string>> & folderAndFiles, const bool recursive)
    {
        DIR * dirp         = opendir(path);
        struct dirent * dp = nullptr;
        if (dirp) 
        {
            do {
                errno = 0;
                dp    = readdir(dirp);
                if (dp != nullptr)
                {
                    if(strcmp(dp->d_name, ".") == 0 && strcmp(path, ".") != 0)
                    {
                        addFolderToMap(path, folderAndFiles);
                    }
                    else if(strcmp(dp->d_name, "..") != 0)
                    {
                        if (dp->d_type == DT_DIR)
                        {
                            if(recursive)
                            {
                                std::string str(path);
                                str += __separator_dir;
                                str += dp->d_name;
                                listFilesRecursive(str.c_str(),folderAndFiles, recursive);
                            }
                            else
                            {
                                addFolderToMap(dp->d_name, folderAndFiles);
                            }
                        }
                        else if(dp->d_type == DT_REG )
                        {
                            std::string str(dp->d_name);
                            addFolderToMap(path, folderAndFiles);
                            std::vector<std::string> & files = folderAndFiles[path];
                            files.emplace_back(str);
                        }
                    }
                }
            }while (dp != nullptr);
            closedir(dirp);
        }
        else
        {
            const char *  pSError = strerror(errno);
            ERROR_LOG("Could not open path [%s]\n[%s]",path, pSError ? pSError : "");
        }
    }

    int onlistFiles(lua_State *lua)
    {
        std::string new_path;
        const int top        = lua_gettop(lua);
        const char* path     = luaL_checkstring(lua,1);
        const bool recursive = top > 1 ? lua_toboolean(lua,2) : false;
        std::map<std::string,std::vector<std::string>> folderAndFiles;
        if(path)
        {
            const int len = strlen(path);
            if(path[len-1] == '\\' || path[len-1] == '/')
            {
                new_path = path;
                new_path.pop_back();
                path = new_path.c_str();
            }
        }
        else
        {
            path = new_path.c_str();
        }
        listFilesRecursive(path, folderAndFiles, recursive);
        lua_newtable(lua);
        const char sep [2] = {__separator_dir, 0};
        lua_pushstring(lua,sep);
        lua_setfield(lua,-2,"separator");
        int index = 1;
        for(auto it = folderAndFiles.cbegin(); it != folderAndFiles.cend(); ++it)
        {
            lua_newtable(lua);
            lua_pushstring(lua,it->first.c_str());
            lua_setfield(lua,-2,"path");
            const std::vector<std::string> & files = it->second;
            for(unsigned int i=0; i < files.size(); ++i)
            {
                lua_pushstring(lua,files[i].c_str());
                lua_rawseti(lua, -2, i+1);
            }
            lua_rawseti(lua, -2, index++);
        }
        return 1;
    }

    static int _checkload (lua_State *L, int stat, const char *filename) 
    {
        if (stat) 
        {  /* module loaded successfully? */
            lua_pushstring(L, filename);  /* will be 2nd argument to module */
            return 2;  /* return open function and file name */
        }
        else
            return luaL_error(L, "error loading module '%s' from file '%s':\n\t%s",
                                lua_tostring(L, 1), filename, lua_tostring(L, -1));
    }

    int __luaB_searchLuaModule(lua_State *lua)
    {
        const char *name = luaL_checkstring(lua, 1);
        std::string full_name_lua(name);
        full_name_lua += ".lua";
        bool exitFile  = false;
        const char * fullPath = util::getFullPath(full_name_lua.c_str(),&exitFile);
        if (fullPath == nullptr || exitFile == false)
        {
            std::string full_name_C(name);
            #if defined _WIN32
            full_name_C += ".dll";
            #elif defined __APPLE__
            full_name_C += ".dylib";
            #else
            full_name_C += ".so";
            #endif
            fullPath = util::getFullPath(full_name_C.c_str(),&exitFile);
            if (fullPath == nullptr || exitFile == false)
                return 1;  /* module not found in this path */
        }
        return _checkload(lua, (luaL_loadfile(lua, fullPath) == LUA_OK), fullPath);
    }

    void registerNamespaceMBM(lua_State *lua, SCENE *scene, lua_CFunction OnNewScene, lua_CFunction OnGetSplash)
    {
        luaL_Reg regMbmConstantsMethods[] = {{"__index", onIndexConstants},{nullptr, nullptr}};
        luaL_newmetatable(lua,"mbmCONSTANTS");
        luaL_setfuncs(lua, regMbmConstantsMethods, 0);
        lua_settop(lua,0);

        luaL_Reg regMbmFrameworkMethods[] = {
    #if defined USE_DX
            {"addText", onNewFontDxLua},
    #endif
            {"loadScene", OnNewScene},
            {"getRealSizeScreen", onGetRealSizeBackBuffer},
            {"getDisplayMetrics", onGetDisplayMetrics},
            {"getSizeScreen", onGetSizeBackBuffer},
            {"getCamera", onGetCamera},
            {"getFps", onGetFps},
            {"quit", onQuitEngine},
            {"setColor", onSetColorBackground},
            {"showConsole", onShowConsoleMbm},
            {"addPath", onAddPathSourceMbm},
            {"getPathEngine", onGetPathSourceMbm},
            {"getFullPath", onGetFullPath},
            {"getAllPaths", onGetAllPath },
            {"to2dw", ontransform2dS2dWMbm},
            {"to2ds", ontransform2dW2dSMbm},
            {"to3d", ontransform2dsto3dmbm},
            {"getObjectsRendered", onGetTotalObjectsRender},
            {"addOnTouch", addOnTouchMeshLua},
            {"setGlobal", onSetGlobal},
            {"getGlobal", onGetGlobal},
            {"getAzimute", onGetAzimute},
            {"is", onIs},
            {"get", onGet},
            {"doCommands", onDoCommands},
            {"getTimeRun", onGetTimeRun},
            {"enableClearScreen", onEnableClearBackGround},
            {"include", onIncludeFile},
            {"pause", onPauseGameLua},
            {"resume", onResumeGameLua},
            {"createTexture", onCreateTextureLua},
            {"existTexture", onExistTextureLua},
            {"compress", onCompressFile},
            {"decompress", onDecompressFile},
            {"existFile", onExistFile},
            {"onErrorStop", onStopFlag},
            {"getKeyCode", onGetKeyCode},
            {"isCapitalKeyOn", onIsCapitalKeyOn},
            {"getKeyName", onGetKeyName},
            {"getIdiom", onGetIdiom},
            {"getUserName", onGetUserName},
            {"clearGlobals", onClearGlobals},
            {"encrypt", onEncryptFile},
            {"decrypt", onDecryptFile},
            {"getSceneName", onGetSceneName},
            {"openFile", onOpenFile},
            {"saveFile", onSaveFile},
            {"openMultiFile", onOpenMultiFile},
            {"openFolder", onOpenFolder},
            {"messageBox", onShowMessageBox},
            {"getShaderList", onGetShaderList},
            {"existShader", onExistShader},
            {"addShader", onAddShader},
            {"sortShader", onSortShader },
            {"inputBox", onInputDialogBox},
            {"inputPassword", onInputPasswordBox},
            {"colorDialog", onColorFromDialogBox},
            {"refresh", onRefresh},
            {"setFakeFps", setFakeFps},
            {"shuffle", onDoShuffle},
            {"undoShuffle", onUndoShuffle},
            {"getSplash", OnGetSplash },
            {"subscribe", onSubscribePlugin},
            {"loadTexture", onLoadDetailedTexture},
            {"listFiles", onlistFiles},
            {"enableTextureFilter", enableTextureFilterLua},
            {"setMinMaxWindowSize", onSetMinMaxWindowSizeLua},
            {"pauseAudioOnPauseGame", onPauseAudioOnPauseGame },
            
    #if !defined ANDROID
            {"executeInThread", onExecuteInOtherThread},
            #endif
            {"generateImageResourceHeaderFromPng", onGenerateImageResourceHeaderFromPng},
            {nullptr, nullptr}};
        DEVICE *device = DEVICE::getInstance();
        device->scene       = scene;

        lua_newtable(lua);
        luaL_setfuncs(lua, regMbmFrameworkMethods, 0);
        luaL_getmetatable(lua, "mbmCONSTANTS");
        lua_setmetatable(lua, -2);
        lua_setglobal(lua, "mbm");
        lua_settop(lua,0);
        
        registerClassUsersData(lua);

        registerClassVec2(lua);
        registerClassVec2NoGc(lua);
        registerClassVec3(lua);
        registerClassVec3NoGc(lua);
        registerClassSprite(lua);
        registerClassMesh(lua);
        registerClassFont(lua);
        registerClassTextureView(lua);
        registerClassGifView(lua);
        registerClassShapeMesh(lua);
        registerClassBackGround(lua);
        registerClassCamera(lua);
        registerClassAudio(lua);
        registerClassCallBackTimer(lua);
        registerClassLineMesh(lua);
        registerClassParticle(lua);
        registerClassRender2TextureTarget(lua);
        registerClassTextureInfo(lua);
        registerClassMeshDebug(lua);
    #if defined USE_VR
        registerClassVR(lua);
    #endif

        registerClassTile(lua);
        
        lua_pushcfunction(lua, __luaB_print_color); // override print
        lua_setglobal(lua, "print");

        const char *splitStringLua = "\n"
                                     "function string:split( inSplitPattern, outResults ) \n"
                                     "  if not outResults then \n"
                                     "    outResults = { } \n"
                                     "  end \n"
                                     "  local theStart = 1 \n"
                                     "  local theSplitStart, theSplitEnd = string.find( self, inSplitPattern, theStart ) \n"
                                     "  while theSplitStart do \n"
                                     "    table.insert( outResults, string.sub( self, theStart, theSplitStart-1 ) ) \n"
                                     "    theStart = theSplitEnd + 1 \n"
                                     "    theSplitStart, theSplitEnd = string.find( self, inSplitPattern, theStart ) \n"
                                     "  end \n"
                                     "  table.insert( outResults, string.sub( self, theStart ) ) \n"
                                     "  return outResults \n"
                                     "end \n";
        luaL_dostring(lua, splitStringLua);
        const char* traceLua = "\n"
            "function trace(num,untilWhat)\n"
            "   local dDebug = debug.getinfo(num or 2)\n"
            "   local i = num or 2\n"
            "   while dDebug do\n"
            "       if dDebug then\n"
            "           print('Function name:',dDebug.name) \n"
            "           print('Source:',dDebug.source:sub(2,240)) \n"
            "           print('Function line:',dDebug.currentline)\n"
            "       end\n"
            "       i = i + 1\n"
            "       dDebug=debug.getinfo(i)\n"
            "       if untilWhat and num and num >= untilWhat then break end"
            "   end\n"
            "end\n";
        luaL_dostring(lua, traceLua);

        //Basically Plugins Android, workaround however could be in other platforms as well
        #if defined REQUIRE_EMBEDDED
        lua_pushcfunction(lua, __luaB_require_embedded); // require embedded
        lua_setglobal(lua, "require_embedded");

        const char* require_embeddedLua = "\n"
        "_old_require = require\n"
        ""
        "function require (name)\n"
        "    local t = require_embedded(name)\n"
        "    if(t) then return t end;\n"
        "    return _old_require(name)\n"
        "end";
        luaL_dostring(lua, require_embeddedLua);
        #endif
        lua_getglobal(lua,"package");
        lua_getfield(lua, -1, "searchers");
        const int len_searchers = luaL_len(lua,-1);
        lua_pushcfunction(lua, __luaB_searchLuaModule);
        lua_rawseti(lua, -2,len_searchers + 1);
        auto *userScene  = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->oldPanicFunction     = lua_atpanic(lua, onPanic);
        lua_settop(lua,0);
    }
};
