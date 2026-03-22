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

#if defined USE_DIRECTX9
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
        const int   top  = lua_gettop(lua);
        const char *what = luaL_checkstring(lua, 1);
        const char *parameter = top > 1 ? luaL_checkstring(lua, 2) : "";
        auto *luaManager = static_cast<LUA_MANAGER *>(LUA_MANAGER::pLuaManager);
        char result[1024] = "";
        if(luaManager->onDoNativeCommand)
            luaManager->onDoNativeCommand(what,parameter,result,sizeof(result));
        lua_pushstring(lua,result);
        return 1;
    }

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

    int onGetDisplayMetrics(lua_State *lua)
    {
    #if defined          ANDROID
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
    #elif defined _WIN32
        int width  = 0;
        int height = 0;
        util::getDisplayMetrics(&width,&height);
        lua_pushnumber(lua,static_cast<lua_Number>(width));
        lua_pushnumber(lua,static_cast<lua_Number>(height));
        return 2;

    #elif defined __linux__ || defined(__APPLE__)
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

    int onQuitEngine(lua_State * lua)
    {
        DEVICE *device		= DEVICE::getInstance();
		const int   top		= lua_gettop(lua);
        device->run         = false;
        device->setAppReturnCode(top == 1 && lua_type(lua, 1) == LUA_TNUMBER ? lua_tointeger(lua, 1) : 0);
        device->scene->onFinalizeScene();
    #ifdef ANDROID
        device->callQuitInJava();
    #endif
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
        SPECIFIC_AUX_CONTEXT_DEVICE * cJni = mbm::DEVICE::getInstance()->specificContextDevice;
        const char *     currentPath = cJni->absPath.c_str();
        if (currentPath)
            strncpy(dir, currentPath,sizeof(dir)-1);
    #elif defined _WIN32
        GetCurrentDirectoryA(sizeof(dir), dir);
    #elif defined __linux__ || defined(__APPLE__)
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
    #elif defined __linux__ || defined(__APPLE__)
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
    #elif defined(__APPLE__)
    #pragma message("getKeyName not implemented on __APPLE__")
        return "";
    #else
    #error "EROR platform not FOUND"
    #endif
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
    #elif defined __linux__ || defined(__APPLE__)
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
    #elif defined __linux__ || defined(__APPLE__)
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
    #else

        const char *fileName = dialog_util::saveFileDialog("Save As", defaultName, filtersArray, (int)filters.size());
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
    #else

        const char *filename =
            dialog_util::openFileDialog("Open file", defaultName, filtersArray, (int)filters.size(), allowMultipleSelects);
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
};
#endif