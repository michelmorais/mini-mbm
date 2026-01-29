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

#if !defined ANDROID && (defined(__linux__) || defined(__APPLE__))

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
#include <core_mbm/specific-opengl_es.h>
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
#if defined USE_EDITOR_FEATURES
    #include <lua-wrap/render-table/mesh-debug-lua.h>
#endif

#include <algorithm>
#include <map>
#include <vector>
#include <audio-interface.h>
#if defined ANDROID
    // no includes here
#elif defined __linux__ || defined(__APPLE__) && !defined ANDROID
    #include <unistd.h>
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
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
    int onDoCommands(lua_State *lua)
    {
        const char *what = luaL_checkstring(lua, 1);
        if (strcasecmp(what, "API-level") == 0)
        {
            SPECIFIC_AUX_CONTEXT_DEVICE * cJni = mbm::DEVICE::getInstance()->specificContextDevice;
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
            SPECIFIC_AUX_CONTEXT_DEVICE * cJni = mbm::DEVICE::getInstance()->specificContextDevice;
            const int        milliseconds = (int)luaL_checkinteger(lua, 2);
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
            SPECIFIC_AUX_CONTEXT_DEVICE * cJni = mbm::DEVICE::getInstance()->specificContextDevice;
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
        const char *     methodName = "displayMetrics";
        const char *     signature  = "()[B"; //() byte array
        SPECIFIC_AUX_CONTEXT_DEVICE * cJni = mbm::DEVICE::getInstance()->specificContextDevice;
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
        lua_pushnumber(lua, 0);
        lua_pushnumber(lua, 0);
        return 2;
    }

    int onQuitEngine(lua_State * lua)
    {
        DEVICE *device		= DEVICE::getInstance();
		const int   top		= lua_gettop(lua);
        device->run         = false;
        device->setAppReturnCode(top == 1 && lua_type(lua, 1) == LUA_TNUMBER ? lua_tointeger(lua, 1) : 0);
        device->scene->onFinalizeScene();
        device->callQuitInJava();
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
        SPECIFIC_AUX_CONTEXT_DEVICE * cJni = mbm::DEVICE::getInstance()->specificContextDevice;
        const char *     currentPath = cJni->absPath.c_str();
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
            SPECIFIC_AUX_CONTEXT_DEVICE * cJni = mbm::DEVICE::getInstance()->specificContextDevice;
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

    int getKeyCode(const char *key)
    {
    
        SPECIFIC_AUX_CONTEXT_DEVICE * cJni = mbm::DEVICE::getInstance()->specificContextDevice;
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
    }

    const char *getKeyName(const int key)
    {
    
        SPECIFIC_AUX_CONTEXT_DEVICE * cJni = mbm::DEVICE::getInstance()->specificContextDevice;
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
    }

    int onGetIdiom(lua_State *lua)
    {
        const char *     methodName = "getIdiom";
        const char *     signature  = "()Ljava/lang/String;"; //(string) void
        SPECIFIC_AUX_CONTEXT_DEVICE * cJni = mbm::DEVICE::getInstance()->specificContextDevice;
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
    }

    int onGetUserName(lua_State *lua)
    {
        const char *     methodName = "getUserName";
        const char *     signature  = "()Ljava/lang/String;"; //(string) void
        SPECIFIC_AUX_CONTEXT_DEVICE * cJni = mbm::DEVICE::getInstance()->specificContextDevice;
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

        const char *     methodName = "saveFile";
        const char *     signature  = "(Ljava/lang/String;)Ljava/lang/String;"; // String (string)
        SPECIFIC_AUX_CONTEXT_DEVICE * cJni = mbm::DEVICE::getInstance()->specificContextDevice;
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

        const char *     methodName = allowMultipleSelects ? "openMultFile" : "getImage";
        const char *     signature  = "(Ljava/lang/String;)V"; // void (string)
        SPECIFIC_AUX_CONTEXT_DEVICE * cJni = mbm::DEVICE::getInstance()->specificContextDevice;
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
        const char *methodName = "messageBox";
        const char *signature = "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z"; // boolean (string,string,string)
        SPECIFIC_AUX_CONTEXT_DEVICE * cJni = mbm::DEVICE::getInstance()->specificContextDevice;
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
        const char *      methodName = "openFolder";
        const char *      signature  = "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;"; // String (string)
        SPECIFIC_AUX_CONTEXT_DEVICE * cJni = mbm::DEVICE::getInstance()->specificContextDevice;
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
        auto *userScene           = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
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