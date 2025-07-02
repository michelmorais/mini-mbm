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
#include <lua-wrap/current-scene-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <lua-wrap/render-table/tile-lua.h>

#include <algorithm>
#include <map>
#include <vector>
#include <audio-interface.h>
#if defined ANDROID
    #include <platform/common-jni.h>
#elif defined __linux__
    #include <unistd.h>
#endif

#ifdef _WIN32
    #include <AESCrypt/win32/aes.crypt.h>
#else
    #include <AESCrypt/linux/aes.crypt.h>
#endif

extern "C" 
{
    #include <lauxlib.h>
    #include <lualib.h>
}

#ifdef USE_VR
    #include <lua-wrap/render-table/vr-lua.h>
#endif

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

namespace mbm 
{
    extern int onGetCamera(lua_State *lua);
    extern void registerClassVec2(lua_State *lua);
    extern void registerClassVec2NoGc(lua_State *lua);
    extern void registerClassVec3(lua_State *lua);
    extern void registerClassVec3NoGc(lua_State *lua);
    extern void registerClassTextureView(lua_State *lua);
    extern void registerClassGifView(lua_State *lua);
    extern void registerClassShapeMesh(lua_State *lua);
    extern void registerClassBackGround(lua_State *lua);
    extern void registerClassCamera(lua_State *lua);
    extern void registerClassAudio(lua_State *lua);
    extern void registerClassCallBackTimer(lua_State *lua);
    extern void registerClassLineMesh(lua_State *lua);
    extern void registerClassParticle(lua_State *lua);
    extern void registerClassRender2TextureTarget(lua_State *lua);
    #if defined USE_EDITOR_FEATURES
    extern void registerClassMeshDebug(lua_State *lua);
    #endif
    extern void registerClassSprite(lua_State *lua);
	extern void registerClassTile(lua_State *lua);
    extern void registerClassMesh(lua_State *lua);
    extern void registerClassFont(lua_State *lua);
	
    const char *__std_p()
    {
        return "passwd";
    }

    #if defined USE_EDITOR_FEATURES && !defined ANDROID
    int onExecuteInOtherThread(lua_State *lua)
    {
        std::string command              = luaL_checkstring(lua,1);
        mbm::DEVICE* device              = mbm::DEVICE::getInstance();
        device->ptrManager->execute_system_cmd_thread(command.c_str());
        return 0;
    }
    #endif

	int lua_error_debug(lua_State *lua,  const char *format, ...)
	{
		va_list va_args;
		va_start(va_args, format);
		const auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, va_args));
		va_end(va_args);
		va_start(va_args, format);
		char * buffer = log_util::formatNewMessage(length, format, va_args);
		va_end(va_args);
		lua_Debug ar;
		memset(&ar, 0, sizeof(lua_Debug));
		if (lua_getstack(lua, 1, &ar))
		{
			if (lua_getinfo(lua, "nSl", &ar))
			{
				std::string buffer_2(buffer);
				delete [] buffer;
				return luaL_error(lua,"File[%s] line[%d]\n%s", log_util::basename(ar.short_src), ar.currentline,buffer_2.c_str());
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
		std::string other_buffer(buffer);
		ERROR_LOG("%s", buffer);
		delete [] buffer;
		return luaL_error(lua,"%s",other_buffer.c_str());
	}

    int enableTextureFilterLua(lua_State *lua)
    {
        bool value = lua_toboolean(lua,1);
        TEXTURE::enableFilter(value);
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
        #if defined _WIN32 || defined(ANDROID)
        device->setMinMaxSizeWindow(min_x,min_y,max_x,max_y);
        #elif defined(__linux__) && !defined(ANDROID)
        device->setMinMaxSizeWindow(device->ptrManager->win,device->ptrManager->display, min_x,min_y,max_x,max_y);
        #endif
        return 0;
    }

    
    int onPauseAudioOnPauseGame(lua_State *lua)
	{
		bool bPauseOnPauseAll = lua_toboolean(lua,1);
		auto manager = AUDIO_MANAGER::getInstance();
		manager->pauseAudioOnPauseGame = bPauseOnPauseAll;
		return 0;
	}

	void lua_print_line(lua_State *lua, TYPE_LOG type_log, const char *format, ...)
	{
		va_list va_args;
		va_start(va_args, format);
		const auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, va_args));
		va_end(va_args);
		va_start(va_args, format);
		char * buffer = log_util::formatNewMessage(length, format, va_args);
		va_end(va_args);
		lua_Debug ar;
		memset(&ar, 0, sizeof(lua_Debug));
		if (lua_getstack(lua, 1, &ar))
		{
			if (lua_getinfo(lua, "nSl", &ar))
			{
				switch(type_log)
				{
					case TYPE_LOG_ERROR:
					{
						ERROR_LOG("File[%s] line[%d]\n%s", log_util::basename(ar.short_src), ar.currentline,buffer);
					};
					break;
					case TYPE_LOG_INFO:
					{
						INFO_LOG("File[%s] line[%d]\n%s", log_util::basename(ar.short_src), ar.currentline,buffer);
					};
					break;
					case TYPE_LOG_WARN:
					{
						WARN_LOG("File[%s] line[%d]\n%s", log_util::basename(ar.short_src), ar.currentline,buffer);
					};
					break;

				}
			}
			else
			{
				ERROR_AT(__LINE__,__FILE__,"Could not get the line and file\n%s",buffer);
			}
		}
		else
		{
			ERROR_AT(__LINE__,__FILE__,"Could not get stack from LUA\n%s",buffer);
		}
		delete [] buffer;
	}

    #if defined ANDROID
    int onDoCommands(lua_State *lua)
    {
        const char *what = luaL_checkstring(lua, 1);
        if (strcasecmp(what, "API-level") == 0)
        {
            util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
            JNIEnv *         jenv = cJni->jenv;
            jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassDoCommandsJniEngine, "getAPILevel", "()I");
            if (mid == NULL)
            {
                lua_print_line(lua,TYPE_LOG_ERROR,"method getAPILevel not found");
                return 0;
            }
            jint ret = jenv->CallStaticIntMethod(cJni->jclassDoCommandsJniEngine, mid);
            lua_pushboolean(lua, 1);
            lua_pushinteger(lua, ret);
            return 2;
        }
        else if (strcasecmp(what, "vibrate") == 0)
        {
            const int        milliseconds = (int)luaL_checkinteger(lua, 2);
            util::COMMON_JNI *cJni         = util::COMMON_JNI::getInstance();
            JNIEnv *         jenv         = cJni->jenv;
            jmethodID        mid          = jenv->GetStaticMethodID(cJni->jclassDoCommandsJniEngine, "vibrate", "(I)V");
            if (mid == NULL)
            {
                lua_print_line(lua,TYPE_LOG_ERROR,"method vibrate not found");
                lua_pushboolean(lua, 0);
                return 1;
            }
            jenv->CallStaticVoidMethod(cJni->jclassDoCommandsJniEngine, mid, milliseconds);
            lua_pushboolean(lua, 1);
            return 1;
        }
        else
        {
            util::COMMON_JNI *cJni  = util::COMMON_JNI::getInstance();
            JNIEnv *         jenv  = cJni->jenv;
            const int        top   = lua_gettop(lua);
            const char *     parm1 = what;
            const char *     parm2 = top > 1 ? luaL_checkstring(lua, 2) : "NULL";
            jmethodID        mid   = jenv->GetStaticMethodID(cJni->jclassDoCommandsJniEngine, "OnDoCommands",
                                                    "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
            if (mid == NULL)
            {
                lua_print_line(lua,TYPE_LOG_ERROR,"method OnDoCommands not found");
                lua_pushboolean(lua, 0);
                return 1;
            }
            jstring jParm1 = jenv->NewStringUTF(cJni->get_safe_string_utf(parm1));//fixed issue using local std::string
            if (jParm1 == NULL)
            {
                lua_print_line(lua,TYPE_LOG_ERROR,"error on call NewStringUTF!");
                lua_pushboolean(lua, 0);
                return 1;
            }
            jstring jParm2 = jenv->NewStringUTF(cJni->get_safe_string_utf(parm2));//fixed issue using local std::string
            if (jParm2 == NULL)
            {
                jenv->DeleteLocalRef(jParm1);
                lua_print_line(lua,TYPE_LOG_ERROR,"error on call NewStringUTF!");
                lua_pushboolean(lua, 0);
                return 1;
            }
            jstring ret = (jstring)jenv->CallStaticObjectMethod(cJni->jclassDoCommandsJniEngine, mid, jParm1, jParm2);
            jenv->DeleteLocalRef(jParm1);
            jenv->DeleteLocalRef(jParm2);
            if (ret)
            {
                const char *newRet = jenv->GetStringUTFChars(ret, 0);
                const char *r      = cJni->getStrToDelete(newRet);
                jenv->ReleaseStringUTFChars(ret, newRet);
                lua_pushstring(lua, r);
                jenv->DeleteLocalRef(ret);
                return 1;
            }
            jenv->DeleteLocalRef(ret);
            lua_pushboolean(lua, 0);
            return 1;
        }
    }

    #else
    // defined on manager-lua.cpp
    
    #endif



    void showConsoleWindowLua()
    {
    #if defined ANDROID
        PRINT_IF_DEBUG("showConsoleWindow without effect [Android]");
    #elif defined _WIN32
        HWND hConsole = GetConsoleWindow();
        if (hConsole)
            ShowWindow(hConsole,  SW_SHOWNOACTIVATE);
    #else
        PRINT_IF_DEBUG("showConsoleWindow without effect");
    #endif
    }

    void hideConsoleWindowLua()
    {
    #if defined ANDROID
        PRINT_IF_DEBUG("hideConsoleWindow without effect [Android]");
    #elif defined _WIN32
        HWND hConsole = GetConsoleWindow();
        if (hConsole)
            ShowWindow(hConsole, SW_HIDE);
    #else
        PRINT_IF_DEBUG("showConsoleWindow without effect");
    #endif
    }

    int onGetRealSizeBackBuffer(lua_State *lua)
    {
        DEVICE *device = DEVICE::getInstance();
        lua_pushnumber(lua, device->getBackBufferWidth());
        lua_pushnumber(lua, device->getBackBufferHeight());
        return 2;
    }

    int onGetDisplayMetrics(lua_State *lua)
    {
    #if defined          ANDROID
        const char *     methodName = "displayMetrics";
        const char *     signature  = "()[B"; //() byte array
        util::COMMON_JNI *cJni       = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv       = cJni->jenv;
        jmethodID        mid        = jenv->GetStaticMethodID(cJni->jclassDoCommandsJniEngine, methodName, signature);
        if (mid == NULL)
        {
            return lua_error_debug(lua, "method not found:%s", methodName);
        }
        jbyteArray ret = (jbyteArray)jenv->CallStaticObjectMethod(cJni->jclassDoCommandsJniEngine, mid);
        if (ret)
        {
            jbyte *buffer = new jbyte[2];
            buffer[0]     = 0;
            buffer[1]     = 0;
            jenv->GetByteArrayRegion(ret, 0, 2, buffer);
            lua_pushnumber(lua, buffer[0]);
            lua_pushnumber(lua, buffer[1]);
            delete[] buffer;
            jenv->DeleteLocalRef(ret);
            return 2;
        }
    #elif defined _WIN32
        int width  = 0;
        int height = 0;
        util::getDisplayMetrics(&width,&height);
        lua_pushnumber(lua,static_cast<lua_Number>(width));
        lua_pushnumber(lua,static_cast<lua_Number>(height));
        return 2;

    #elif defined __linux__
        int width  = 0;
        int height = 0;
        DEVICE *device = DEVICE::getInstance();
        device->ptrManager->getScreenSize(&width,&height);
        if(width > 0 && height > 0)
        {
            lua_pushnumber(lua, width);
            lua_pushnumber(lua, height);
            return 2;
        }

    #endif
        lua_pushnumber(lua, 0);
        lua_pushnumber(lua, 0);
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

    int onQuitEngine(lua_State * lua)
    {
        DEVICE *device		= DEVICE::getInstance();
		const int   top		= lua_gettop(lua);
        device->run         = false;
		DEVICE::returnCodeApp = top == 1 && lua_type(lua,1) == LUA_TNUMBER ? lua_tointeger(lua,1) : 0;
        device->scene->onFinalizeScene();
    #ifdef ANDROID
        device->callQuitInJava();
    #endif
        return 0;
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

    int onShowConsoleMbm(lua_State *lua)
    {
    #if defined ANDROID
        #if _DEBUG
            PRINT_IF_DEBUG("showConsoleWindow without effect [Android]");
        #endif
    #elif defined _WIN32
        const bool value = lua_toboolean(lua, 1) ? true : false;
        if (value)
            mbm::showConsoleWindow();
        else
            mbm::hideConsoleWindow();
    #endif
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

    int onGetPathSourceMbm(lua_State *lua)
    {
        const int   top      = lua_gettop(lua);
        const char *filename = top >= 1 && lua_type(lua, 1) == LUA_TSTRING ? lua_tostring(lua, 1) : nullptr;
        const int   level    = filename != nullptr && top >= 2 && lua_type(lua, 2) == LUA_TNUMBER
                              ? lua_tointeger(lua, 2)
                              : (top >= 1 && lua_type(lua, 1) == LUA_TNUMBER ? lua_tointeger(lua, 1) : 0);
        char             dir[255]   = "";
        dir[0]                      = 0;
    #if defined          ANDROID
        util::COMMON_JNI *cJni        = util::COMMON_JNI::getInstance();
        const char *     currentPath = cJni->absPath.c_str();
        if (currentPath)
            strncpy(dir, currentPath,sizeof(dir)-1);
    #elif defined _WIN32
        GetCurrentDirectoryA(sizeof(dir), dir);
    #elif defined __linux__
        getcwd(dir,sizeof(dir));
    #else
    #error "platform not suported"
    #endif
        if (dir[0])
            lua_pushstring(lua, getPathAtLevel(level, dir, filename));
        else if (filename)
            lua_pushstring(lua, filename);
        else
            lua_pushnil(lua);
        return 1;
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
                case LUA_TTABLE: { return lua_error_debug(lua, "global variable [%s] not allowed", what);}
                case LUA_TFUNCTION: { return lua_error_debug(lua, "global variable [%s] function not allowed", what);}
                case LUA_TUSERDATA: { return lua_error_debug(lua, "global variable [%s] userdata not allowed", what);}
                case LUA_TTHREAD: { return lua_error_debug(lua, "global variable [%s] thread not allowed", what);}
                case LUA_TLIGHTUSERDATA: { return lua_error_debug(lua, "global variable [%s] light userdata not allowed", what);}
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
    #if defined __linux__ && !defined(ANDROID)
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
            if (strcasecmp(what, "windows") == 0 || strcasecmp(what, "android") == 0 || strcasecmp(what, "linux") == 0)
            {
    #if defined _WIN32
                lua_pushboolean(lua,1);
    #elif defined ANDROID
				lua_pushboolean(lua,1);
    #elif defined __linux__
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
    #ifdef USE_OPENGL_ES
                const char *v = (const char *)glGetString(GL_VERSION);
                versions += "\nOpengL: ";
                versions += v;
    #else
                versions += "\nDirectx: ";
                sprintf(tempVersion, "%x", DIRECT3D_VERSION);
                versions += tempVersion;
    #endif
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
    #ifdef USE_OPENGL_ES
            else if (strcasecmp(what, "opengl") == 0)
            {
                lua_pushstring(lua, (const char *)glGetString(GL_VERSION));
            }
    #else
            else if (strcasecmp(what, "directx") == 0)
            {
                lua_pushfstring(lua, "%p", DIRECT3D_VERSION);
            }
    #endif
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
                DEVICE* tDevice = DEVICE::getInstance();
                DYNAMIC_VAR* dExeName = tDevice->lsDynamicVarGlobal["_executable_name_"];
                if(dExeName)
                    lua_pushstring(lua, dExeName->getString());
                else
                    lua_pushnil(lua);
            }
            else if (strcasecmp(what, "USE_EDITOR_FEATURES") == 0)
            {
                #ifdef USE_EDITOR_FEATURES
                    lua_pushboolean(lua,1);
                #else
                    lua_pushboolean(lua,0);
                #endif
            }
            else if (strcasecmp(what, "USE_DEPRECATED_2_MINOR") == 0)
            {
                #ifdef USE_DEPRECATED_2_MINOR
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
                #ifdef USE_OPENGL_ES
                    lua_pushboolean(lua,1);
                #else
                    lua_pushboolean(lua,0);
                #endif
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

    int onIncludeFile(lua_State *lua)
    {
        const char *fileName = luaL_checkstring(lua, 1);
        if (fileName)
        {
            bool             sucess = false;
    #if defined              ANDROID
            util::COMMON_JNI *cJni   = util::COMMON_JNI::getInstance();
            const char *     newPath = util::getFullPath(cJni->copyFileFromAsset(fileName, "rt"),nullptr);
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
            bool _ret = man->saveDataAsPNG(strSaveAs.c_str(), pixel, channel, width, height, strMessageError);
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

    int getKeyCode(const char *key)
    {
    #ifdef _WIN32
        const int len = strlen(key);
        if (len == 1)
        {
            switch (key[0])
            {
                case '/': return VK_DIVIDE;
                case '*': return VK_MULTIPLY;
                case '-': return VK_SUBTRACT;
                case '+': return VK_ADD;
                case '.': return VK_DECIMAL;
                case '0': return 0x30;
                case '1': return 0x31;
                case '2': return 0x32;
                case '3': return 0x33;
                case '4': return 0x34;
                case '5': return 0x35;
                case '6': return 0x36;
                case '7': return 0x37;
                case '8': return 0x38;
                case '9': return 0x39;
                case '\\': return VK_OEM_102;
                case 'A':case 'a': return 0x41;
                case 'B':case 'b': return 0x42;
                case 'C':case 'c': return 0x43;
                case 'D':case 'd': return 0x44;
                case 'E':case 'e': return 0x45;
                case 'F':case 'f': return 0x46;
                case 'G':case 'g': return 0x47;
                case 'H':case 'h': return 0x48;
                case 'I':case 'i': return 0x49;
                case 'J':case 'j': return 0x4A;
                case 'K':case 'k': return 0x4B;
                case 'L':case 'l': return 0x4C;
                case 'M':case 'm': return 0x4D;
                case 'N':case 'n': return 0x4E;
                case 'O':case 'o': return 0x4F;
                case 'P':case 'p': return 0x50;
                case 'Q':case 'q': return 0x51;
                case 'R':case 'r': return 0x52;
                case 'S':case 's': return 0x53;
                case 'T':case 't': return 0x54;
                case 'U':case 'u': return 0x55;
                case 'V':case 'v': return 0x56;
                case 'W':case 'w': return 0x57;
                case 'X':case 'x': return 0x58;
                case 'Y':case 'y': return 0x59;
                case 'Z':case 'z': return 0x60;
                case '=': return VK_OEM_PLUS;
                case ',': return VK_OEM_COMMA;
                case ';': return VK_OEM_1;
                case '`': return VK_OEM_3;
                case '[': return VK_OEM_4;
                case ']': return VK_OEM_6;
                default: return key[0];
            }
        }
        else if (len == 2 && (key[0] == 'f' || key[0] == 'F'))
        {
            switch(key[1])
            {
                case '1': return VK_F1;
                case '2': return VK_F2;
                case '3': return VK_F3;
                case '4': return VK_F4;
                case '5': return VK_F5;
                case '6': return VK_F6;
                case '7': return VK_F7;
                case '8': return VK_F8;
                case '9': return VK_F9;
            }
        }
        else if (len == 3 && (key[0] == 'f' || key[0] == 'F'))
        {
            switch(key[1])
            {
                case '0': return VK_F10;
                case '1': return VK_F11;
                case '2': return VK_F12;
            }
        }
        else
        {
            if (strcasecmp(key, "left") == 0)
                return VK_LEFT;
            if (strcasecmp(key, "right") == 0)
                return VK_RIGHT;
            if (strcasecmp(key, "up") == 0)
                return VK_UP;
            if (strcasecmp(key, "down") == 0)
                return VK_DOWN;
            if (strcasecmp(key, "esc") == 0 || strcasecmp(key, "escape") == 0)
                return VK_ESCAPE;
            if (strcasecmp(key, "space") == 0)
                return VK_SPACE;
            if (strcasecmp(key, "insert") == 0)
                return VK_INSERT;
            if (strcasecmp(key, "pageup") == 0 || strcasecmp(key, "page up") == 0)
                return VK_PRIOR;
            if (strcasecmp(key, "pagedown") == 0 || strcasecmp(key, "page down") == 0)
                return VK_NEXT;
            if (strcasecmp(key, "end") == 0)
                return VK_END;
            if (strcasecmp(key, "home") == 0)
                return VK_HOME;
            if (strcasecmp(key, "delete") == 0)
                return VK_DELETE;
            if (strcasecmp(key, "printscreen") == 0 || strcasecmp(key, "print screen") == 0)
                return VK_SNAPSHOT;
            if (strcasecmp(key, "enter") == 0)
                return VK_RETURN;
            if (strcasecmp(key, "shift") == 0)
                return VK_SHIFT;
            if (strcasecmp(key, "control") == 0)
                return VK_CONTROL;
            if (strcasecmp(key, "backspace") == 0 || strcasecmp(key, "back space") == 0)
                return VK_BACK;
            if (strcasecmp(key, "pause") == 0)
                return VK_PAUSE;
            if (strcasecmp(key, "tab") == 0)
                return VK_TAB;
            if (strcasecmp(key, "capslook") == 0 || strcasecmp(key, "caps look") == 0)
                return VK_CAPITAL;
            if (strcasecmp(key, "super") == 0)
                return VK_LWIN;
            if (strcasecmp(key, "alt") == 0)
                return VK_MENU;
            if (strcasecmp(key, "scroll") == 0 || strcasecmp(key, "scroll lock") == 0)
                return VK_SCROLL;
            return 0;
        }
    #elif defined ANDROID
        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassKeyCodeJniEngine, "getKeyCode", "(Ljava/lang/String;)I");
        if (mid == NULL)
        {
            ERROR_AT(__LINE__,__FILE__,"%s","method getKeyCode not found");
            return 0;
        }
        jstring jstr = jenv->NewStringUTF(cJni->get_safe_string_utf(key));//fixed issue using local std::string
        if (jstr == NULL)
        {
            ERROR_AT(__LINE__,__FILE__,"%s","error on call NewStringUTF!");
            return 0;
        }
        jint ret = jenv->CallStaticIntMethod(cJni->jclassKeyCodeJniEngine, mid, jstr);
        jenv->DeleteLocalRef(jstr);
        return (int)ret;
    #elif defined __linux__
        const int len = strlen(key);
        if (len == 1)
        {
            switch (key[0])
            {
                case '*': return XK_KP_Multiply;
                case '+': return XK_KP_Add;
                case '-': return XK_KP_Subtract;
                case '/': return XK_KP_Divide;
                case '.': return XK_KP_Decimal;
                default:
                {
                    KeySym keyCode = XStringToKeysym(key);
                    return keyCode;
                }
            }
        }
        else if (len == 2 && (key[0] == 'f' || key[0] == 'F'))
        {
            switch(key[1])
            {
                case '1': return XK_F1;
                case '2': return XK_F2;
                case '3': return XK_F3;
                case '4': return XK_F4;
                case '5': return XK_F5;
                case '6': return XK_F6;
                case '7': return XK_F7;
                case '8': return XK_F8;
                case '9': return XK_F9;
            }
        }
        else if (len == 3 && (key[0] == 'f' || key[0] == 'F'))
        {
            switch(key[1])
            {
                case '0': return XK_F10;
                case '1': return XK_F11;
                case '2': return XK_F12;
            }
        }
        else
        {
            if (strcasecmp(key, "left") == 0)
                return XK_Left;
            if (strcasecmp(key, "right") == 0)
                return XK_Right;
            if (strcasecmp(key, "up") == 0)
                return XK_Up;
            if (strcasecmp(key, "down") == 0)
                return XK_Down;
            if (strcasecmp(key, "esc") == 0 || strcasecmp(key, "escape") == 0)
                return XK_Escape;
            if (strcasecmp(key, "space") == 0)
                return XK_space;
            if (strcasecmp(key, "insert") == 0)
                return XK_Insert;
            if (strcasecmp(key, "pageup") == 0 || strcasecmp(key, "page up") == 0)
                return XK_Page_Up;
            if (strcasecmp(key, "pagedown") == 0 || strcasecmp(key, "page down") == 0)
                return XK_Page_Down;
            if (strcasecmp(key, "end") == 0)
                return XK_End;
            if (strcasecmp(key, "delete") == 0)
                return XK_Delete;
            if (strcasecmp(key, "printscreen") == 0 || strcasecmp(key, "print screen") == 0)
                return XK_Print;
            if (strcasecmp(key, "keypad enter") == 0)
                return XK_KP_Enter;
            if (strcasecmp(key, "enter") == 0)
                return XK_Return;
            if (strcasecmp(key, "shift") == 0)
                return XK_Shift_L;
            if (strcasecmp(key, "control") == 0)
                return XK_Control_L;
            if (strcasecmp(key, "backspace") == 0 || strcasecmp(key, "back space") == 0)
                return XK_BackSpace;
            if (strcasecmp(key, "pause") == 0)
                return XK_Pause;
            if (strcasecmp(key, "tab") == 0)
                return XK_Tab;
            if (strcasecmp(key, "capslook") == 0 || strcasecmp(key, "caps look") == 0)
                return XK_Caps_Lock;
            if (strcasecmp(key, "numlock") == 0 || strcasecmp(key, "num lock") == 0)
                return XK_Num_Lock;
            if (strcasecmp(key, "alt") == 0)
                return XK_Alt_L;
            if (strcasecmp(key, "home") == 0)
                return XK_Home;
            if (strcasecmp(key, "scroll") == 0 || strcasecmp(key, "scroll lock") == 0)
                return XK_Scroll_Lock;
            if (strcasecmp(key, "super") == 0)
                return XK_Super_L;
            return key[0];
        }
    #else
    #error "EROR platform not FOUND";
    #endif
        return key[0];
    }

    const char *getKeyName(const int key)
    {
    #ifdef _WIN32
        switch (key)
        {
            case VK_NUMPAD0:
            case 0x30: return "0";
            case VK_NUMPAD1:
            case 0x31: return "1";
            case VK_NUMPAD2:
            case 0x32: return "2";
            case VK_NUMPAD3:
            case 0x33: return "3";
            case VK_NUMPAD4:
            case 0x34: return "4";
            case VK_NUMPAD5:
            case 0x35: return "5";
            case VK_NUMPAD6:
            case 0x36: return "6";
            case VK_NUMPAD7:
            case 0x37: return "7";
            case VK_NUMPAD8:
            case 0x38: return "8";
            case VK_NUMPAD9:
            case 0x39: return "9";
            case VK_MULTIPLY: return "*";
            case VK_ADD: return "+";
            case VK_SUBTRACT: return "-";
            case VK_DIVIDE: return "/";
            case VK_F1: return "F1";
            case VK_F2: return "F2";
            case VK_F3: return "F3";
            case VK_F4: return "F4";
            case VK_F5: return "F5";
            case VK_F6: return "F6";
            case VK_F7: return "F7";
            case VK_F8: return "F8";
            case VK_F9: return "F9";
            case VK_F10: return "F10";
            case VK_F11: return "F11";
            case VK_F12: return "F12";
            case VK_SLEEP: return "SLEEP";
            case VK_RETURN: return "ENTER";
            case VK_UP: return "UP";
            case VK_DOWN: return "DOWN";
            case VK_LEFT: return "LEFT";
            case VK_RIGHT: return "RIGHT";
            case VK_TAB: return "TAB";
            case VK_MENU: return "ALT";
            case VK_PAUSE: return "PAUSE";
            case VK_SPACE: return "SPACE";
            case VK_ESCAPE: return "ESCAPE";
            case VK_PRIOR: return "PAGE UP";
            case VK_NEXT: return "PAGE DOWN";
            case VK_HOME: return "HOME";
            case VK_DELETE: return "DELETE";
            case VK_SCROLL: return "SCROLL";
            case VK_LCONTROL: return "CONTROL";
            case VK_RCONTROL: return "CONTROL";
            case 0xC2: return ".";
            case VK_DECIMAL: return ".";
            case VK_SHIFT: return "SHIFT";
            case VK_BACK: return "BACKSPACE";
            case VK_INSERT: return "INSERT";
            case VK_END: return "END";
            case VK_SNAPSHOT: return "PRINT SCREEN";
            case VK_LWIN: return "SUPER";
            case VK_RWIN: return "SUPER";
            case VK_RMENU: return "ALT";
            case VK_LMENU: return "ALT";
            case VK_CONTROL: return "CONTROL";
            case VK_CAPITAL: return "CAPS LOOK";
            case 'A': return "A";
            case 'B': return "B";
            case 'C': return "C";
            case 'D': return "D";
            case 'E': return "E";
            case 'F': return "F";
            case 'G': return "G";
            case 'H': return "H";
            case 'I': return "I";
            case 'J': return "J";
            case 'K': return "K";
            case 'L': return "L";
            case 'M': return "M";
            case 'N': return "N";
            case 'O': return "O";
            case 'P': return "P";
            case 'Q': return "Q";
            case 'R': return "R";
            case 'S': return "S";
            case 'T': return "T";
            case 'U': return "U";
            case 'V': return "V";
            case 'W': return "W";
            case 'X': return "X";
            case 'Y': return "Y";
            case 'Z': return "Z";
            case VK_OEM_102: return "\\";
            case VK_OEM_PLUS: return "=";
            case VK_OEM_COMMA: return ",";
            case VK_OEM_MINUS: return "-";
            case VK_OEM_PERIOD: return ".";
            case VK_OEM_1: return ";"; 
            case VK_OEM_2: return "/"; 
            case VK_OEM_3: return "`"; 
            case VK_OEM_4: return "["; 
            case VK_OEM_5: return "\\"; 
            case VK_OEM_6: return "]";
            default:
            {
                static char str[20] = "";
                snprintf( str,sizeof(str)-1,"0X%x",key);
                return str;
            };
        }
    #elif defined ANDROID
        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassKeyCodeJniEngine, "getKeyName", "(I)Ljava/lang/String;");
        if (mid == NULL)
        {
            ERROR_AT(__LINE__,__FILE__,"%s","method getKeyName not found");
            return 0;
        }
        jstring ret = (jstring)jenv->CallStaticObjectMethod(cJni->jclassKeyCodeJniEngine, mid, key);
        if (ret)
        {
            const char *newRet = jenv->GetStringUTFChars(ret, 0);
            const char *r      = cJni->getStrToDelete(newRet);
            jenv->ReleaseStringUTFChars(ret, newRet);
            jenv->DeleteLocalRef(ret);
            return r;
        }
        return NULL;
    #elif defined __linux__
        switch (key)
        {
            case XK_KP_Insert: return "0";
            case XK_KP_Delete: return "DELETE";
            case XK_KP_Enter: return "ENTER";
            case XK_KP_End: return "1";
            case XK_KP_Down: return "2";
            case XK_KP_Page_Down: return "3";
            case XK_KP_Left: return "4";
            case XK_KP_Begin: return "5";
            case XK_KP_Right: return "6";
            case XK_KP_Home: return "7";
            case XK_KP_Up: return "8";
            case XK_KP_Page_Up: return "9";
            case XK_Num_Lock: return "NUM LOCK";
            case XK_Super_L: return "windows";
            case XK_Super_R: return "windows";
            case XK_KP_0:
            case '0': return "0";
            case XK_KP_1:
            case '1': return "1";
            case XK_KP_2:
            case '2': return "2";
            case XK_KP_3:
            case '3': return "3";
            case XK_KP_4:
            case '4': return "4";
            case XK_KP_5:
            case '5': return "5";
            case XK_KP_6:
            case '6': return "6";
            case XK_KP_7:
            case '7': return "7";
            case XK_KP_8:
            case '8': return "8";
            case XK_KP_9:
            case '9': return "9";
            case XK_KP_Multiply: return "*";
            case XK_KP_Add: return "+";
            case XK_KP_Subtract: return "-";
            case XK_KP_Divide: return "/";
            case XK_KP_Decimal: return ".";
            case XK_F1: return "F1";
            case XK_F2: return "F2";
            case XK_F3: return "F3";
            case XK_F4: return "F4";
            case XK_F5: return "F5";
            case XK_F6: return "F6";
            case XK_F7: return "F7";
            case XK_F8: return "F8";
            case XK_F9: return "F9";
            case XK_F10: return "F10";
            case XK_F11: return "F11";
            case XK_F12: return "F12";
            case XK_Return: return "ENTER";
            case XK_Up: return "UP";
            case XK_Down: return "DOWN";
            case XK_Left: return "LEFT";
            case XK_Right: return "RIGHT";
            case XK_Tab: return "TAB";
            case XK_Menu: return "ALT";
            case 0xfe03: return "ALT";
            case XK_Pause: return "PAUSE";
            case XK_space: return "SPACE";
            case XK_Escape: return "ESCAPE";
            case XK_Page_Up: return "PAGE UP";
            case XK_Page_Down: return "PAGE DOWN";
            case XK_Home: return "HOME";
            case XK_Delete: return "DELETE";
            case XK_Scroll_Lock: return "SCROLL";
            case XK_Control_L: return "CONTROL";
            case XK_Control_R: return "CONTROL";
            case XK_Shift_L: return "SHIFT";
            case XK_BackSpace: return "BACKSPACE";
            case XK_Insert: return "INSERT";
            case XK_End: return "END";
            case XK_Print: return "PRINT SCREEN";
            case XK_Alt_R: return "ALT";
            case XK_Alt_L: return "ALT";
            case XK_Caps_Lock: return "CAPS LOOK";
            case 'A': return "A";
            case 'B': return "B";
            case 'C': return "C";
            case 'D': return "D";
            case 'E': return "E";
            case 'F': return "F";
            case 'G': return "G";
            case 'H': return "H";
            case 'I': return "I";
            case 'J': return "J";
            case 'K': return "K";
            case 'L': return "L";
            case 'M': return "M";
            case 'N': return "N";
            case 'O': return "O";
            case 'P': return "P";
            case 'Q': return "Q";
            case 'R': return "R";
            case 'S': return "S";
            case 'T': return "T";
            case 'U': return "U";
            case 'V': return "V";
            case 'W': return "W";
            case 'X': return "X";
            case 'Y': return "Y";
            case 'Z': return "Z";
            // case VK_OEM_102: return "\\";
            // case VK_OEM_PLUS:    return "=";
            // case VK_OEM_COMMA:   return ",";
            // case VK_OEM_MINUS:   return "-";
            // case VK_OEM_PERIOD:  return ".";
            default:
            {
                static char str[20] = "";
                snprintf( str,sizeof(str)-1,"0X%x",key);
                return str;
            };
        }
    #else
    #error "EROR platform not FOUND"
    #endif
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

    int onGetIdiom(lua_State *lua)
    {
    #if defined   _WIN32
        WCHAR     localeName[LOCALE_NAME_MAX_LENGTH] = {0};
        const int len                                = sizeof(localeName) / sizeof(*(localeName));
        int       ret                                = GetUserDefaultLocaleName(localeName, len);
        if (ret == 0)
            lua_pushstring(lua, "Unknown");
        else
        {
            char stextOut[1024] = "";
            lua_pushstring(lua, util::toChar(localeName, stextOut));
        }
        return 1;
    #elif defined ANDROID
        const char *     methodName = "getIdiom";
        const char *     signature  = "()Ljava/lang/String;"; //(string) void
        util::COMMON_JNI *cJni       = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv       = cJni->jenv;
        jmethodID        mid        = jenv->GetStaticMethodID(cJni->jclassDoCommandsJniEngine, methodName, signature);
        if (mid == NULL)
        {
            return lua_error_debug(lua, "method not found:%s", methodName);
        }
        jstring ret = (jstring)jenv->CallStaticObjectMethod(cJni->jclassDoCommandsJniEngine, mid);
        if (ret)
        {
            const char *newRet = jenv->GetStringUTFChars(ret, 0);
            const char *r      = cJni->getStrToDelete(newRet);
            jenv->ReleaseStringUTFChars(ret, newRet);
            lua_pushstring(lua, r);
            jenv->DeleteLocalRef(ret);
        }
        else
        {
            lua_pushstring(lua, "Unknown");
        }
        return 1;
    #elif defined __linux__
        const char *lang = getenv("LANG");
        if (lang == nullptr)
            lua_pushstring(lua, "unknown");
        else
        {
            setlocale(LC_ALL, lang);
            lua_pushstring(lua, nl_langinfo(_NL_IDENTIFICATION_LANGUAGE));
        }
        return 1;
    #else
    #error "platform not defined"
    #endif
    }

    int onGetUserName(lua_State *lua)
    {
    #if defined _WIN32
        char    user[255] = "";
        DWORD   dUser     = sizeof(user);
        if (!GetUserNameA(user, &dUser))
        {
            printf("\nfailed to get user name!");
            lua_pushnil(lua);
            return 1;
        }
        lua_pushstring(lua, user);
        return 1;
    #elif defined ANDROID
        const char *     methodName = "getUserName";
        const char *     signature  = "()Ljava/lang/String;"; //(string) void
        util::COMMON_JNI *cJni       = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv       = cJni->jenv;
        jmethodID        mid        = jenv->GetStaticMethodID(cJni->jclassDoCommandsJniEngine, methodName, signature);
        if (mid == NULL)
        {
            return lua_error_debug(lua, "method not found:%s", methodName);
        }
        jstring ret = (jstring)jenv->CallStaticObjectMethod(cJni->jclassDoCommandsJniEngine, mid);
        if (ret)
        {
            const char *newRet = jenv->GetStringUTFChars(ret, 0);
            const char *r      = cJni->getStrToDelete(newRet);
            jenv->ReleaseStringUTFChars(ret, newRet);
            lua_pushstring(lua, r);
            jenv->DeleteLocalRef(ret);
        }
        else
        {
            ERROR_LOG("To get username from Android you need to add the following permission on XML manifest:\n%s","<uses-permission android:name=\"android.permission.GET_ACCOUNTS\" />");
            lua_pushnil(lua);
        }
        return 1;
    #elif defined __linux__
        const uid_t          uid = geteuid();
        const struct passwd *pw  = getpwuid(uid);
        if (pw)
            lua_pushstring(lua, pw->pw_name);
        else
            lua_pushstring(lua, "null");
        return 1;
    #else
    #error "platform not implemented"
    #endif
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

        std::string strOut(fileNameOut);
        if (strcasecmp(fileNameOut, fileNameIn) == 0)
            strOut += ".out.tmp";
        const int passlen = strlen(password);
        if (passlen <= 4)
        {
            lua_print_line(lua,TYPE_LOG_WARN,"weak password [%s]", password);
            lua_pushboolean(lua, 0);
            return 1;
        }
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
        if (encrypt_stream(fp1, fp2, password, passlen, strErr))
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

        std::string strOut(fileNameOut);
        if (strcasecmp(fileNameOut, fileNameIn) == 0)
            strOut += ".out.tmp";
        const int passlen = strlen(password);
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
        if (decrypt_stream(fp1, fp2, password, passlen, strErr))
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
        return 1;
    }

    int onGetSceneName(lua_State *lua)
    {
        DEVICE *device = DEVICE::getInstance();
        lua_pushstring(lua, device->scene->getSceneName());
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

    #if defined ANDROID

        const char *     methodName = "saveFile";
        const char *     signature  = "(Ljava/lang/String;)Ljava/lang/String;"; // String (string)
        util::COMMON_JNI *cJni       = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv       = cJni->jenv;
        jmethodID        mid        = jenv->GetStaticMethodID(cJni->jclassDoCommandsJniEngine, methodName, signature);
        if (mid == NULL)
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"method not found: %s", methodName);
            lua_pushnil(lua);
            delete[] filtersArray;
            return 1;
        }
        if (defaultName == NULL)
            defaultName = "callBackSaveImageLua";
        jstring jstr    = jenv->NewStringUTF(cJni->get_safe_string_utf(defaultName));//fixed issue using local std::string
        if (jstr == NULL)
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"error on call NewStringUTF");
            lua_pushnil(lua);
            delete[] filtersArray;
            return 1;
        }
        jstring ret = (jstring)jenv->CallStaticObjectMethod(cJni->jclassDoCommandsJniEngine, mid, jstr);
        jenv->DeleteLocalRef(jstr);
        if (ret == NULL)
        {
            delete[] filtersArray;
            lua_pushnil(lua);
            return 1;
        }
        const char *newRet   = jenv->GetStringUTFChars(ret, 0);
        const char *fileName = cJni->getStrToDelete(newRet);
        jenv->ReleaseStringUTFChars(ret, newRet);
        jenv->DeleteLocalRef(ret);
    #else

        const char *fileName = tinyfd_saveFileDialog("Save As", defaultName, filters.size(), filtersArray, nullptr);
    #endif
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

    #if defined ANDROID

        const char *     methodName = allowMultipleSelects ? "openMultFile" : "getImage";
        const char *     signature  = "(Ljava/lang/String;)V"; // void (string)
        util::COMMON_JNI *cJni       = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv       = cJni->jenv;
        jmethodID        mid        = jenv->GetStaticMethodID(cJni->jclassDoCommandsJniEngine, methodName, signature);
        if (mid == NULL)
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"method not found: %s", methodName);
            lua_pushnil(lua);
            delete[] filtersArray;
            return 1;
        }
        const char *callBack = "callBackLoadImageLua";
        if (defaultName)
            callBack = defaultName;
        jstring jstr = jenv->NewStringUTF(cJni->get_safe_string_utf(callBack));//fixed issue using local std::string
        if (jstr == NULL)
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"error on call NewStringUTF");
            lua_pushnil(lua);
            delete[] filtersArray;
            return 1;
        }
        jenv->CallStaticVoidMethod(cJni->jclassDoCommandsJniEngine, mid, jstr);
        jenv->DeleteLocalRef(jstr);
        const char *filename = "NULL";
    #else

        const char *filename =
            tinyfd_openFileDialog("Open file", defaultName, filters.size(), filtersArray, nullptr, allowMultipleSelects);
    #endif
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

    #if defined ANDROID

    bool onShowMessageBoxAndroid(const char *const title, const char *const message, const char *dialogType)
    {
        const char *methodName = "messageBox";
        const char *signature = "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z"; // boolean (string,string,string)
        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassFileJniEngine, methodName, signature);
        if (mid == NULL)
        {
            ERROR_AT(__LINE__,__FILE__,"method not found: %s", methodName);
            return false;
        }
        jstring jstrTitle = jenv->NewStringUTF(cJni->get_safe_string_utf(title));//fixed issue using local std::string
        if (jstrTitle == NULL)
        {
            ERROR_AT(__LINE__,__FILE__,"%s","error on call NewStringUTF");
            return false;
        }
        jstring jstrMessage = jenv->NewStringUTF(cJni->get_safe_string_utf(message));//fixed issue using local std::string
        if (jstrMessage == NULL)
        {
            ERROR_AT(__LINE__,__FILE__,"%s","error on call NewStringUTF");
            return false;
        }
        jstring jstrDialogType = jenv->NewStringUTF(cJni->get_safe_string_utf(dialogType));//fixed issue using local std::string
        if (jstrDialogType == NULL)
        {
            ERROR_AT(__LINE__,__FILE__,"%s", "error on call NewStringUTF");
            return false;
        }
        jboolean ret = jenv->CallStaticBooleanMethod(cJni->jclassFileJniEngine, mid, jstrTitle, jstrMessage, jstrDialogType);
        jenv->DeleteLocalRef(jstrTitle);
        jenv->DeleteLocalRef(jstrMessage);
        jenv->DeleteLocalRef(jstrDialogType);
        return ret;
    }
    #endif

    
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

    #if defined ANDROID
        if (onShowMessageBoxAndroid(title, message, dialogType))
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
    #else
        const int ret = tinyfd_messageBox(title, message, dialogType, iconType, defaultButton);
        lua_pushboolean(lua, ret);
    #endif
        return 1;
    }

    int onOpenFolder(lua_State *lua)
    {
        const int         top         = lua_gettop(lua);
        const char *const title       = top > 0 && lua_type(lua, 1) == LUA_TSTRING ? lua_tostring(lua, 1) : "Choose a folder";
        const char *const defaultPath = top > 1 && lua_type(lua, 2) == LUA_TSTRING ? lua_tostring(lua, 2) : "";
    #if defined           ANDROID
        const char *      methodName = "openFolder";
        const char *      signature  = "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;"; // String (string)
        util::COMMON_JNI * cJni      = util::COMMON_JNI::getInstance();
        JNIEnv *          jenv       = cJni->jenv;
        jmethodID         mid        = jenv->GetStaticMethodID(cJni->jclassFileJniEngine, methodName, signature);
        if (mid == NULL)
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"method not found: %s", methodName);
            lua_pushnil(lua);
            return 1;
        }
        jstring jstrTitle = jenv->NewStringUTF(cJni->get_safe_string_utf(title));//fixed issue using local std::string
        if (jstrTitle == NULL)
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"error on call NewStringUTF");
            lua_pushnil(lua);
            return 1;
        }
        jstring jstrDefaultPath = jenv->NewStringUTF(cJni->get_safe_string_utf(defaultPath));//fixed issue using local std::string
        if (jstrDefaultPath == NULL)
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"error on call NewStringUTF");
            lua_pushnil(lua);
            return 1;
        }
        jstring ret = (jstring)jenv->CallStaticObjectMethod(cJni->jclassFileJniEngine, mid, jstrTitle, jstrDefaultPath);
        jenv->DeleteLocalRef(jstrTitle);
        jenv->DeleteLocalRef(jstrDefaultPath);
        const char *path = NULL;
        if (ret)
        {
            const char *newRet = jenv->GetStringUTFChars(ret, 0);
            path               = cJni->getStrToDelete(newRet);
            jenv->ReleaseStringUTFChars(ret, newRet);
            jenv->DeleteLocalRef(ret);
        }
    #elif defined _WIN32
        char dir[255]   = "";
        if(defaultPath && strlen(defaultPath) > 0)
            strncpy(dir,defaultPath,sizeof(dir) - 1);
        const char *      path         = mbm::selectFolderDialog(dir);
    #else
        const char *      path         = tinyfd_selectFolderDialog(title, defaultPath);
    #endif
        if (path)
            lua_pushstring(lua, path);
        else
            lua_pushnil(lua);
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

    int onInputDialogBox(lua_State *lua)
    {
    #if defined ANDROID
        lua_pushnil(lua);
    #else
        const int         top          = lua_gettop(lua);
        const char *const title        = top > 0 && lua_type(lua, 1) != LUA_TNIL ? lua_tostring(lua, 1) : "title";
        const char *const message      = top > 1 && lua_type(lua, 2) != LUA_TNIL ? lua_tostring(lua, 2) : "input message";
        const char *const defaultInput = top > 2 && lua_type(lua, 3) != LUA_TNIL ? lua_tostring(lua, 3) : "";
    #ifdef _WIN32
        std::string       msg(message);
        log_util::replaceString(msg, "\n", "\"+chr(13)+chr(10)+\""); // vb script
        const char *result = tinyfd_inputBox(title, msg.c_str(), defaultInput ? defaultInput : "");
    #else
        const char *result = tinyfd_inputBox(title, message, defaultInput ? defaultInput : "");
    #endif
        if (result)
            lua_pushstring(lua, result);
        else
            lua_pushnil(lua);
    #endif
        return 1;
    }

    int onInputPasswordBox(lua_State *lua)
    {
    #if defined ANDROID
        lua_pushnil(lua);
    #else
        const int         top     = lua_gettop(lua);
        const char *const title   = top > 0 && lua_type(lua, 1) != LUA_TNIL ? lua_tostring(lua, 1) : "title";
        const char *const message = top > 1 && lua_type(lua, 2) != LUA_TNIL ? lua_tostring(lua, 2) : "input message";
    #ifdef _WIN32
        std::string msg(message);
        log_util::replaceString(msg, "\n", "\"+chr(13)+chr(10)+\""); // vb script
        const char *result = tinyfd_inputBox(title, msg.c_str(), NULL);
    #else
        const char *result = tinyfd_inputBox(title, message, nullptr);
    #endif
        if (result)
            lua_pushstring(lua, result);
        else
            lua_pushnil(lua);
    #endif
        return 1;
    }

    int onColorFromDialogBox(lua_State *lua)
    {
    #if defined ANDROID
        lua_pushnil(lua);
    #else
        const int         top   = lua_gettop(lua);
        char const * const aTitle = "Select color";
        unsigned char aoResultRGB[3] = {0,0,0};
        unsigned char const aDefaultRGB[3] = {  top > 0 ? static_cast<const unsigned char>(luaL_checknumber(lua,1) * 255.0f) : static_cast<const unsigned char>(0),
                                                top > 1 ? static_cast<const unsigned char>(luaL_checknumber(lua,2) * 255.0f) : static_cast<const unsigned char>(255),
                                                top > 2 ? static_cast<const unsigned char>(luaL_checknumber(lua,3) * 255.0f) : static_cast<const unsigned char>(255)};
        const char *result = tinyfd_colorChooser(aTitle,nullptr,aDefaultRGB,aoResultRGB);
        if (result)
        {
            constexpr float p = 1.0f / 255.0f;
            const float r = (static_cast<const float>(static_cast<const int>(aoResultRGB[0]))) * p;
            const float g = (static_cast<const float>(static_cast<const int>(aoResultRGB[1]))) * p;
            const float b = (static_cast<const float>(static_cast<const int>(aoResultRGB[2]))) * p;
            lua_pushnumber(lua, r);
            lua_pushnumber(lua, g);
            lua_pushnumber(lua, b);
            return 3;
        }
        lua_pushnil(lua);
    #endif
        return 1;
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
                sprintf(tmp, "%s %f ", minMaxDefault, lsVec->at(0));
            }
            else if (s == 2)
            {
                sprintf(tmp, "%s %f %f ", minMaxDefault, lsVec->at(0), lsVec->at(1));
            }
            else if (s == 3)
            {
                sprintf(tmp, "%s %f %f %f ", minMaxDefault, lsVec->at(0), lsVec->at(1), lsVec->at(2));
            }
            else if (s == 4)
            {
                sprintf(tmp, "%s %f %f %f %f ", minMaxDefault, lsVec->at(0), lsVec->at(1), lsVec->at(2), lsVec->at(3));
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

    int onPanic(lua_State *lua)
    {
        DEVICE *        device    = DEVICE::getInstance();
        auto *userScene           = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        const char *    error     = lua_tostring(lua, -1);
        std::string               strErr(error ? error : "undefined");
        ERROR_LOG("%s",strErr.c_str());
    #if defined ANDROID
        onShowMessageBoxAndroid("PANIC: unprotected error in call to Lua API", strErr.c_str(), "ok");
    #else
        tinyfd_messageBox("PANIC: unprotected error in call to Lua API", strErr.c_str(), "ok", "error", 0);
    #endif
        if (userScene && userScene->oldPanicFunction)
            userScene->oldPanicFunction(lua);
        else
            exit(255);
        return 0;
    }

    int onRefresh(lua_State * )
    {
        DEVICE *device = DEVICE::getInstance();
        device->ptrManager->forceRestore();
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
        uint32 width                       = 0;
        uint32 height                      = 0;
        TEXTURE_MANAGER * texture_manager  = TEXTURE_MANAGER::getInstance();
        TEXTURE * tex                      = texture_manager->load(file_name_texture,alpha);
        if(tex)
        {
            width   = tex->getWidth();
            height  = tex->getHeight();
        }
        lua_pushinteger(lua,width);
        lua_pushinteger(lua,height);
        if(tex)
        {
            lua_pushinteger(lua,tex->idTexture);
            lua_pushboolean(lua,tex->useAlphaChannel);
        }
        else
        {
            lua_pushinteger(lua,0);
            lua_pushboolean(lua,0);
        }
        return 4;
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
#if defined REQUIRE_EMBEDDED
    //# plugins for android (https://developer.android.com/about/versions/nougat/android-7.0-changes.html#ndk) will be linked (workaround)
    int __luaB_require_embedded(lua_State *lua)
    {
        const char* name     = luaL_checkstring(lua,1);
        (void)name;
        #if defined USE_LSQLITE3
            if(strcmp(name,"lsqlite3") == 0)
                return luaopen_lsqlite3(lua);
        #endif

        #if defined USE_BOX2D
            if(strcmp(name,"box2d") == 0)
                return luaopen_box2d(lua);
        #endif
        #if defined USE_IMGUI
            if(strcmp(name,"ImGui") == 0)
                return luaopen_ImGui(lua);
        #endif
        lua_pushnil(lua);
        return 1;
    }
#endif

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
            
    #if defined USE_EDITOR_FEATURES && !defined ANDROID
            {"executeInThread", onExecuteInOtherThread},
    #endif
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
    #if defined USE_EDITOR_FEATURES
        registerClassMeshDebug(lua);
    #endif
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
