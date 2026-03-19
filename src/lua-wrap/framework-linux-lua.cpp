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

#if !defined(ANDROID) && defined(__linux__) && !defined(__APPLE__)

#include <lua-wrap/framework-lua.h>
#include <core_mbm/device.h>
#include <lua-wrap/manager-lua.h>
#include <core_mbm/util-interface.h>

#if defined USE_OPENGL_ES
    #include <core_mbm/specific-opengl_es.h>
#elif defined USE_DUMMY_BACK_END_ENGINE
    #include <core_mbm/specific-dummy.h> // replace with your specific backend engine header
    #if defined __linux__  || defined(__APPLE__)
        #include <X11/Xlib.h>
        #include <X11/Xutil.h>
        #include <X11/XKBlib.h>
    #endif
#else
    #error "This file is only for OpenGL ES"
#endif


#include <lua-wrap/render-table/mesh-debug-lua.h>

#include <algorithm>
#include <vector>
#include <audio-interface.h>

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
        PRINT_WARN_IF_DEBUG("showConsoleWindow without effect");
    }

    void hideConsoleWindowLua()
    {
        PRINT_WARN_IF_DEBUG("showConsoleWindow without effect");
    }

    int onGetDisplayMetrics(lua_State *lua)
    {
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
        return 0;
    }


    int onShowConsoleMbm(lua_State *lua)
    {
        PRINT_WARN_IF_DEBUG("showConsoleWindow without effect [linux]");
        return 0;
    }

    int onGetPathSourceMbm(lua_State *lua)
    {
        const int   top      = lua_gettop(lua);
        const char *filename = top >= 1 && lua_type(lua, 1) == LUA_TSTRING ? lua_tostring(lua, 1) : nullptr;
        const int   level    = filename != nullptr && top >= 2 && lua_type(lua, 2) == LUA_TNUMBER ? lua_tointeger(lua, 2)
                              : (top >= 1 && lua_type(lua, 1) == LUA_TNUMBER ? lua_tointeger(lua, 1) : 0);
        char             dir[255]   = "";
        dir[0]                      = 0;

        getcwd(dir,sizeof(dir));

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
            const char *  newPath = util::getFullPath(fileName, nullptr);
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
        return key[0];
    }

    const char *getKeyName(const int key)
    {
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
    }

    int onGetIdiom(lua_State *lua)
    {
        const char *lang = getenv("LANG");
        if (lang == nullptr)
            lua_pushstring(lua, "unknown");
        else
        {
            setlocale(LC_ALL, lang);
            lua_pushstring(lua, nl_langinfo(_NL_IDENTIFICATION_LANGUAGE));
        }
        return 1;
    }

    int onGetUserName(lua_State *lua)
    {
        const uid_t          uid = geteuid();
        const struct passwd *pw  = getpwuid(uid);
        if (pw)
            lua_pushstring(lua, pw->pw_name);
        else
            lua_pushstring(lua, "null");
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

        const char *fileName = dialog_util::saveFileDialog("Save As", defaultName, filtersArray, (int)filters.size());

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

        const char *filename = dialog_util::openFileDialog("Open file", defaultName, filtersArray, (int)filters.size(), allowMultipleSelects);

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
        const int ret = tinyfd_messageBox(title, message, dialogType, iconType, defaultButton);
        lua_pushboolean(lua, ret);
        return 1;
    }

    int onOpenFolder(lua_State *lua)
    {
        const int         top         = lua_gettop(lua);
        const char *const title       = top > 0 && lua_type(lua, 1) == LUA_TSTRING ? lua_tostring(lua, 1) : "Choose a folder";
        const char *const defaultPath = top > 1 && lua_type(lua, 2) == LUA_TSTRING ? lua_tostring(lua, 2) : "";
        const char *      path         = tinyfd_selectFolderDialog(title, defaultPath);
        if (path)
            lua_pushstring(lua, path);
        else
            lua_pushnil(lua);
        return 1;
    }

    int onInputDialogBox(lua_State *lua)
    {
        const int         top          = lua_gettop(lua);
        const char *const title        = top > 0 && lua_type(lua, 1) != LUA_TNIL ? lua_tostring(lua, 1) : "title";
        const char *const message      = top > 1 && lua_type(lua, 2) != LUA_TNIL ? lua_tostring(lua, 2) : "input message";
        const char *const defaultInput = top > 2 && lua_type(lua, 3) != LUA_TNIL ? lua_tostring(lua, 3) : "";
        const char *result = tinyfd_inputBox(title, message, defaultInput ? defaultInput : "");
        if (result)
            lua_pushstring(lua, result);
        else
            lua_pushnil(lua);
        return 1;
    }

    int onInputPasswordBox(lua_State *lua)
    {
        const int         top     = lua_gettop(lua);
        const char *const title   = top > 0 && lua_type(lua, 1) != LUA_TNIL ? lua_tostring(lua, 1) : "title";
        const char *const message = top > 1 && lua_type(lua, 2) != LUA_TNIL ? lua_tostring(lua, 2) : "input message";
        const char *result = tinyfd_inputBox(title, message, nullptr);
        if (result)
            lua_pushstring(lua, result);
        else
            lua_pushnil(lua);
        return 1;
    }

    int onColorFromDialogBox(lua_State *lua)
    {
        const int         top     = lua_gettop(lua);
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
        return 1;
    }

    int onPanic(lua_State *lua)
    {
        DEVICE *        device    = DEVICE::getInstance();
        auto *userScene           = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        const char *    error     = lua_tostring(lua, -1);
        std::string               strErr(error ? error : "undefined");
        ERROR_LOG("%s",strErr.c_str());
        tinyfd_messageBox("PANIC: unprotected error in call to Lua API", strErr.c_str(), "ok", "error", 0);
        if (userScene && userScene->oldPanicFunction)
            userScene->oldPanicFunction(lua);
        else
            exit(255);
        return 0;
    }
};

#endif