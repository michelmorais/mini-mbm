/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT);                                                                                                     |
| Copyright (C); 2020      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                      |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software");, to deal in the Software without restriction, including without limitation       |
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

#include "imgui.h"
#include "imgui_internal.h"

#if defined USE_OPENGL_ES

    #ifndef IMGUI_IMPL_OPENGL_ES2
        #define IMGUI_IMPL_OPENGL_ES2
    #endif //!IMGUI_IMPL_OPENGL_ES2
    #ifndef IMGUI_IMPL_OPENGL_LOADER_CUSTOM
        #define IMGUI_IMPL_OPENGL_LOADER_CUSTOM
    #endif // !IMGUI_IMPL_OPENGL_LOADER_CUSTOM
    #include "imgui_impl_opengl3.h"
    #include "imgui_stdlib.h"
#elif defined USE_DIRECTX9
    #include "imgui_impl_dx9.h"
#elif defined USE_DUMMY_BACK_END_ENGINE
    #include <core_mbm/dummy-engine.h> // for compiler_message, you can remove it after implement the functions
#else
    #error "you need to define a rendering backend for imgui"
#endif



#if defined _WIN32
    #include "imgui_impl_win32.h"
#elif (defined(__linux__) || defined(__APPLE__)) && !defined (ANDROID)
    #ifndef XK_MISCELLANY
        #define XK_MISCELLANY
    #endif
    #ifndef XK_LATIN1
        #define XK_LATIN1
    #endif
    #include <X11/XKBlib.h>
    #include <X11/keysymdef.h>
    #include <X11/cursorfont.h>
#elif defined(ANDROID)
    #include <jni.h>
#endif

#include "imgui-lua.h"
#include <core_mbm/device.h>
#include <core_mbm/texture-manager.h>
#include <plugin-helper/plugin-helper.h>
#include <lua-wrap/texture-info-lua.h>

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#include <string>
#include <vector>
#include <map>
#include <set>
#include <stdio.h>

#if defined _WIN32
    #include <Windows.h>
#else
    #include <core_mbm/core-manager.h>
#endif

#include <core_mbm/util-interface.h>
#include <cstring>

class IMGUI_LUA;

//-----------------------------------------------------------------------------
// OS-dependent clipboard for ImGui
// Windows: Uses built-in Win32 clipboard (imgui.cpp default)
// Linux/macOS: Override with xclip/xsel for OS clipboard integration
//-----------------------------------------------------------------------------
#if (defined(__linux__) || defined(__APPLE__)) && !defined(ANDROID)
// Requirements on Linux/macOS
// Install one of:
// xclip: apt install xclip (Debian/Ubuntu) or brew install xclip (macOS)
// xsel: apt install xsel (Debian/Ubuntu) or brew install xsel (macOS)
static void Platform_SetClipboardTextFn_Linux(ImGuiContext* ctx, const char* text)
{
    if (!text) return;
    FILE* f = popen("xclip -selection clipboard 2>/dev/null", "w");
    if (f)
    {
        size_t len = strlen(text);
        if (fwrite(text, 1, len, f) == len)
        {
            pclose(f);
            return;
        }
        pclose(f);
    }
    f = popen("xsel --clipboard --input 2>/dev/null", "w");
    if (f)
    {
        size_t len = strlen(text);
        if (fwrite(text, 1, len, f) == len)
            pclose(f);
        else
            pclose(f);
    }
}
static const char* Platform_GetClipboardTextFn_Linux(ImGuiContext* ctx)
{
    ImGuiContext& g = *ctx;
    g.ClipboardHandlerData.clear();
    FILE* f = popen("xclip -selection clipboard -o 2>/dev/null", "r");
    if (f)
    {
        char buf[256];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        {
            size_t old_sz = g.ClipboardHandlerData.Size;
            g.ClipboardHandlerData.resize(old_sz + (int)n);
            memcpy(g.ClipboardHandlerData.Data + old_sz, buf, n);
        }
        pclose(f);
        if (g.ClipboardHandlerData.Size > 0)
        {
            g.ClipboardHandlerData.push_back(0);
            return g.ClipboardHandlerData.Data;
        }
    }
    g.ClipboardHandlerData.clear();
    f = popen("xsel --clipboard --output 2>/dev/null", "r");
    if (f)
    {
        char buf[256];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        {
            size_t old_sz = g.ClipboardHandlerData.Size;
            g.ClipboardHandlerData.resize(old_sz + (int)n);
            memcpy(g.ClipboardHandlerData.Data + old_sz, buf, n);
        }
        pclose(f);
        if (g.ClipboardHandlerData.Size > 0)
        {
            g.ClipboardHandlerData.push_back(0);
            return g.ClipboardHandlerData.Data;
        }
    }
    return NULL;
}
#endif

/*
    This class is intended to be used as interface to mbm engine.
    If there is no intent to use this module in the engine there is no problem. It can be used as normal module in lua.
*/

#include <core_mbm/plugin-callback.h>

// Helper function to map native keys to ImGuiKey
static ImGuiKey MapNativeKeyToImGuiKey(int native_key)
{
    #if defined(_WIN32)
    switch(native_key)
    {
        case VK_TAB: return ImGuiKey_Tab;
        case VK_LEFT: return ImGuiKey_LeftArrow;
        case VK_RIGHT: return ImGuiKey_RightArrow;
        case VK_UP: return ImGuiKey_UpArrow;
        case VK_DOWN: return ImGuiKey_DownArrow;
        case VK_PRIOR: return ImGuiKey_PageUp;
        case VK_NEXT: return ImGuiKey_PageDown;
        case VK_HOME: return ImGuiKey_Home;
        case VK_END: return ImGuiKey_End;
        case VK_INSERT: return ImGuiKey_Insert;
        case VK_DELETE: return ImGuiKey_Delete;
        case VK_BACK: return ImGuiKey_Backspace;
        case VK_SPACE: return ImGuiKey_Space;
        case VK_RETURN: return ImGuiKey_Enter;
        case VK_ESCAPE: return ImGuiKey_Escape;
        case VK_OEM_7: return ImGuiKey_Apostrophe;
        case VK_OEM_COMMA: return ImGuiKey_Comma;
        case VK_OEM_MINUS: return ImGuiKey_Minus;
        case VK_OEM_PERIOD: return ImGuiKey_Period;
        case VK_OEM_2: return ImGuiKey_Slash;
        case VK_OEM_1: return ImGuiKey_Semicolon;
        case VK_OEM_PLUS: return ImGuiKey_Equal;
        case VK_OEM_4: return ImGuiKey_LeftBracket;
        case VK_OEM_5: return ImGuiKey_Backslash;
        case VK_OEM_6: return ImGuiKey_RightBracket;
        case VK_OEM_3: return ImGuiKey_GraveAccent;
        case VK_CAPITAL: return ImGuiKey_CapsLock;
        case VK_SCROLL: return ImGuiKey_ScrollLock;
        case VK_NUMLOCK: return ImGuiKey_NumLock;
        case VK_SNAPSHOT: return ImGuiKey_PrintScreen;
        case VK_PAUSE: return ImGuiKey_Pause;
        case VK_NUMPAD0: return ImGuiKey_Keypad0;
        case VK_NUMPAD1: return ImGuiKey_Keypad1;
        case VK_NUMPAD2: return ImGuiKey_Keypad2;
        case VK_NUMPAD3: return ImGuiKey_Keypad3;
        case VK_NUMPAD4: return ImGuiKey_Keypad4;
        case VK_NUMPAD5: return ImGuiKey_Keypad5;
        case VK_NUMPAD6: return ImGuiKey_Keypad6;
        case VK_NUMPAD7: return ImGuiKey_Keypad7;
        case VK_NUMPAD8: return ImGuiKey_Keypad8;
        case VK_NUMPAD9: return ImGuiKey_Keypad9;
        case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
        case VK_DIVIDE: return ImGuiKey_KeypadDivide;
        case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
        case VK_ADD: return ImGuiKey_KeypadAdd;
        case VK_LSHIFT: return ImGuiKey_LeftShift;
        case VK_LCONTROL: return ImGuiKey_LeftCtrl;
        case VK_LMENU: return ImGuiKey_LeftAlt;
        case VK_LWIN: return ImGuiKey_LeftSuper;
        case VK_RSHIFT: return ImGuiKey_RightShift;
        case VK_RCONTROL: return ImGuiKey_RightCtrl;
        case VK_RMENU: return ImGuiKey_RightAlt;
        case VK_RWIN: return ImGuiKey_RightSuper;
        case VK_APPS: return ImGuiKey_Menu;
        case '0': return ImGuiKey_0;
        case '1': return ImGuiKey_1;
        case '2': return ImGuiKey_2;
        case '3': return ImGuiKey_3;
        case '4': return ImGuiKey_4;
        case '5': return ImGuiKey_5;
        case '6': return ImGuiKey_6;
        case '7': return ImGuiKey_7;
        case '8': return ImGuiKey_8;
        case '9': return ImGuiKey_9;
        case 'A': return ImGuiKey_A;
        case 'B': return ImGuiKey_B;
        case 'C': return ImGuiKey_C;
        case 'D': return ImGuiKey_D;
        case 'E': return ImGuiKey_E;
        case 'F': return ImGuiKey_F;
        case 'G': return ImGuiKey_G;
        case 'H': return ImGuiKey_H;
        case 'I': return ImGuiKey_I;
        case 'J': return ImGuiKey_J;
        case 'K': return ImGuiKey_K;
        case 'L': return ImGuiKey_L;
        case 'M': return ImGuiKey_M;
        case 'N': return ImGuiKey_N;
        case 'O': return ImGuiKey_O;
        case 'P': return ImGuiKey_P;
        case 'Q': return ImGuiKey_Q;
        case 'R': return ImGuiKey_R;
        case 'S': return ImGuiKey_S;
        case 'T': return ImGuiKey_T;
        case 'U': return ImGuiKey_U;
        case 'V': return ImGuiKey_V;
        case 'W': return ImGuiKey_W;
        case 'X': return ImGuiKey_X;
        case 'Y': return ImGuiKey_Y;
        case 'Z': return ImGuiKey_Z;
        case VK_F1: return ImGuiKey_F1;
        case VK_F2: return ImGuiKey_F2;
        case VK_F3: return ImGuiKey_F3;
        case VK_F4: return ImGuiKey_F4;
        case VK_F5: return ImGuiKey_F5;
        case VK_F6: return ImGuiKey_F6;
        case VK_F7: return ImGuiKey_F7;
        case VK_F8: return ImGuiKey_F8;
        case VK_F9: return ImGuiKey_F9;
        case VK_F10: return ImGuiKey_F10;
        case VK_F11: return ImGuiKey_F11;
        case VK_F12: return ImGuiKey_F12;
        default: return ImGuiKey_None;
    }
    #elif (defined(__linux__) || defined(__APPLE__)) && !defined (ANDROID)
    // Linux/Mac key mapping (keys are converted to uppercase by the engine)
    switch(native_key)
    {
        case XK_Tab: return ImGuiKey_Tab;
        case XK_Left: return ImGuiKey_LeftArrow;
        case XK_Right: return ImGuiKey_RightArrow;
        case XK_Up: return ImGuiKey_UpArrow;
        case XK_Down: return ImGuiKey_DownArrow;
        case XK_Page_Up: return ImGuiKey_PageUp;
        case XK_Page_Down: return ImGuiKey_PageDown;
        case XK_Home: return ImGuiKey_Home;
        case XK_End: return ImGuiKey_End;
        case XK_Insert: return ImGuiKey_Insert;
        case XK_Delete: return ImGuiKey_Delete;
        case XK_BackSpace: return ImGuiKey_Backspace;
        case XK_space: return ImGuiKey_Space;
        case XK_KP_Space: return ImGuiKey_Space;
        case XK_Return: return ImGuiKey_Enter;
        case XK_KP_Enter: return ImGuiKey_KeypadEnter;
        case XK_Escape: return ImGuiKey_Escape;
        case XK_apostrophe: return ImGuiKey_Apostrophe;
        case XK_comma: return ImGuiKey_Comma;
        case XK_minus: return ImGuiKey_Minus;
        case XK_period: return ImGuiKey_Period;
        case XK_slash: return ImGuiKey_Slash;
        case XK_semicolon: return ImGuiKey_Semicolon;
        case XK_equal: return ImGuiKey_Equal;
        case XK_bracketleft: return ImGuiKey_LeftBracket;
        case XK_backslash: return ImGuiKey_Backslash;
        case XK_bracketright: return ImGuiKey_RightBracket;
        case XK_grave: return ImGuiKey_GraveAccent;
        case XK_Caps_Lock: return ImGuiKey_CapsLock;
        case XK_Scroll_Lock: return ImGuiKey_ScrollLock;
        case XK_Num_Lock: return ImGuiKey_NumLock;
        case XK_Print: return ImGuiKey_PrintScreen;
        case XK_Pause: return ImGuiKey_Pause;
        // Alphanumeric keys (engine converts to uppercase)
        case '0': return ImGuiKey_0;
        case '1': return ImGuiKey_1;
        case '2': return ImGuiKey_2;
        case '3': return ImGuiKey_3;
        case '4': return ImGuiKey_4;
        case '5': return ImGuiKey_5;
        case '6': return ImGuiKey_6;
        case '7': return ImGuiKey_7;
        case '8': return ImGuiKey_8;
        case '9': return ImGuiKey_9;
        case 'A': return ImGuiKey_A;
        case 'B': return ImGuiKey_B;
        case 'C': return ImGuiKey_C;
        case 'D': return ImGuiKey_D;
        case 'E': return ImGuiKey_E;
        case 'F': return ImGuiKey_F;
        case 'G': return ImGuiKey_G;
        case 'H': return ImGuiKey_H;
        case 'I': return ImGuiKey_I;
        case 'J': return ImGuiKey_J;
        case 'K': return ImGuiKey_K;
        case 'L': return ImGuiKey_L;
        case 'M': return ImGuiKey_M;
        case 'N': return ImGuiKey_N;
        case 'O': return ImGuiKey_O;
        case 'P': return ImGuiKey_P;
        case 'Q': return ImGuiKey_Q;
        case 'R': return ImGuiKey_R;
        case 'S': return ImGuiKey_S;
        case 'T': return ImGuiKey_T;
        case 'U': return ImGuiKey_U;
        case 'V': return ImGuiKey_V;
        case 'W': return ImGuiKey_W;
        case 'X': return ImGuiKey_X;
        case 'Y': return ImGuiKey_Y;
        case 'Z': return ImGuiKey_Z;
        // F-keys
        case XK_F1: return ImGuiKey_F1;
        case XK_F2: return ImGuiKey_F2;
        case XK_F3: return ImGuiKey_F3;
        case XK_F4: return ImGuiKey_F4;
        case XK_F5: return ImGuiKey_F5;
        case XK_F6: return ImGuiKey_F6;
        case XK_F7: return ImGuiKey_F7;
        case XK_F8: return ImGuiKey_F8;
        case XK_F9: return ImGuiKey_F9;
        case XK_F10: return ImGuiKey_F10;
        case XK_F11: return ImGuiKey_F11;
        case XK_F12: return ImGuiKey_F12;
        // Keypad numbers (NumLock on)
        case XK_KP_0: return ImGuiKey_Keypad0;
        case XK_KP_1: return ImGuiKey_Keypad1;
        case XK_KP_2: return ImGuiKey_Keypad2;
        case XK_KP_3: return ImGuiKey_Keypad3;
        case XK_KP_4: return ImGuiKey_Keypad4;
        case XK_KP_5: return ImGuiKey_Keypad5;
        case XK_KP_6: return ImGuiKey_Keypad6;
        case XK_KP_7: return ImGuiKey_Keypad7;
        case XK_KP_8: return ImGuiKey_Keypad8;
        case XK_KP_9: return ImGuiKey_Keypad9;
        case XK_KP_Decimal: return ImGuiKey_KeypadDecimal;
        case XK_KP_Divide: return ImGuiKey_KeypadDivide;
        case XK_KP_Multiply: return ImGuiKey_KeypadMultiply;
        case XK_KP_Subtract: return ImGuiKey_KeypadSubtract;
        case XK_KP_Add: return ImGuiKey_KeypadAdd;
        // Keypad navigation (NumLock off - arrows combined with numpad)
        case XK_KP_Home: return ImGuiKey_Home;
        case XK_KP_Up: return ImGuiKey_UpArrow;
        case XK_KP_Page_Up: return ImGuiKey_PageUp;
        case XK_KP_Left: return ImGuiKey_LeftArrow;
        case XK_KP_Begin: return ImGuiKey_Keypad5;  // Numpad 5 without NumLock
        case XK_KP_Right: return ImGuiKey_RightArrow;
        case XK_KP_End: return ImGuiKey_End;
        case XK_KP_Down: return ImGuiKey_DownArrow;
        case XK_KP_Page_Down: return ImGuiKey_PageDown;
        case XK_KP_Insert: return ImGuiKey_Insert;
        case XK_KP_Delete: return ImGuiKey_Delete;
        // Modifier keys
        case XK_Shift_L: return ImGuiKey_LeftShift;
        case XK_Shift_R: return ImGuiKey_RightShift;
        case XK_Control_L: return ImGuiKey_LeftCtrl;
        case XK_Control_R: return ImGuiKey_RightCtrl;
        case XK_Alt_L: return ImGuiKey_LeftAlt;
        case XK_Alt_R: return ImGuiKey_RightAlt;
        case XK_Super_L: return ImGuiKey_LeftSuper;
        case XK_Super_R: return ImGuiKey_RightSuper;
        case XK_Menu: return ImGuiKey_Menu;
        default: return ImGuiKey_None;
    }
    #else
    return ImGuiKey_None;
    #endif
}


static int PLUGIN_IDENTIFIER = 1; //this value is auto set by this module. It is set in the metatable to make sure that we can convert the userdata to ** IMGUI_LUA
static bool bDrawListToBackground = false;
static bool bDrawListToForeground = false;

ImDrawList* GetImDrawListLua()
{
    if(bDrawListToForeground)
        return ImGui::GetForegroundDrawList();
    if(bDrawListToForeground)
        return ImGui::GetBackgroundDrawList();
    return ImGui::GetWindowDrawList();
}

IMGUI_LUA *getImGuiFromRawTable(lua_State *lua, const int rawi, const int indexTable);
void lua_log_error(lua_State *lua,const char * message);

const int get_texture_id(lua_State *lua,const char* texture_name,unsigned int & width_out, unsigned int & height_out)
{
    mbm::TEXTURE_MANAGER* texMan = mbm::TEXTURE_MANAGER::getInstance();
    mbm::TEXTURE* texture        = texMan->load(texture_name,true);
    if(texture)
    {
        width_out  = texture->getWidth();
        height_out = texture->getHeight();
        return texture->idTexture;
    }
    std::string msg("Texture [");
    msg += texture_name ? texture_name : "nullptr";
    msg += "] not found!";
    lua_log_error(lua,msg.c_str());
    return 0;
}

const int get_texture_id(lua_State *lua,const char* texture_name)
{
    mbm::TEXTURE_MANAGER* texMan = mbm::TEXTURE_MANAGER::getInstance();
    mbm::TEXTURE* texture        = texMan->load(texture_name,true);
    if(texture)
    {
        return texture->idTexture;
    }
    std::string msg("Texture [");
    msg += texture_name ? texture_name : "nullptr";
    msg += "] not found!";
    lua_log_error(lua,msg.c_str());
    return 0;
}

// Helper to get texture from: TextureInfo table, number (texture id), or string (filename)
// Returns texture pointer and populates width/height if texture is valid
mbm::TEXTURE* get_texture_from_lua(lua_State *lua, int index, unsigned int &width_out, unsigned int &height_out)
{
    width_out = 0;
    height_out = 0;
    
    const int type = lua_type(lua, index);
    
    if (type == LUA_TTABLE)
    {
        // Check if it's a TextureInfo table
        auto **ud = static_cast<mbm::TEXTURE_INFO_DATA **>(
            mbm::lua_get_userType_no_throw(lua, 1, index, mbm::L_USER_TYPE_TEXTURE_INFO));
        if (ud && *ud)
        {
            mbm::TEXTURE *texture = (*ud)->getTexture();
            if (texture)
            {
                width_out = texture->getWidth();
                height_out = texture->getHeight();
                return texture;
            }
        }
        lua_log_error(lua, "Invalid or released TextureInfo");
        return nullptr;
    }
    else if (type == LUA_TNUMBER)
    {
        // Direct texture ID - need to find by ID (not recommended but kept for compatibility)
        // This path doesn't provide width/height
        return nullptr;
    }
    else if (type == LUA_TSTRING)
    {
        const char* texture_name = lua_tostring(lua, index);
        mbm::TEXTURE_MANAGER* texMan = mbm::TEXTURE_MANAGER::getInstance();
        mbm::TEXTURE* texture = texMan->load(texture_name, true);
        if (texture)
        {
            width_out = texture->getWidth();
            height_out = texture->getHeight();
            return texture;
        }
        std::string msg("Texture [");
        msg += texture_name ? texture_name : "nullptr";
        msg += "] not found!";
        lua_log_error(lua, msg.c_str());
        return nullptr;
    }
    
    lua_log_error(lua, "Expected TextureInfo, texture filename (string), or texture id (number)");
    return nullptr;
}

// Get ImTextureID from: TextureInfo table, number (texture id), or string (filename)
ImTextureID get_imgui_texture_id(lua_State *lua, int &index, unsigned int &width_out, unsigned int &height_out)
{
    width_out = 0;
    height_out = 0;
    
    const int type = lua_type(lua, index);
    
    if (type == LUA_TNUMBER)
    {
        // Direct texture ID
        return (ImTextureID)(intptr_t)(lua_tointeger(lua, index++));
    }
    else if (type == LUA_TSTRING)
    {
        const char* texture_name = lua_tostring(lua, index++);
        return (ImTextureID)(intptr_t)(get_texture_id(lua, texture_name, width_out, height_out));
    }
    else if (type == LUA_TTABLE)
    {
        // Check if it's a TextureInfo table
        auto **ud = static_cast<mbm::TEXTURE_INFO_DATA **>(
            mbm::lua_get_userType_no_throw(lua, 1, index, mbm::L_USER_TYPE_TEXTURE_INFO));
        if (ud && *ud)
        {
            mbm::TEXTURE *texture = (*ud)->getTexture();
            if (texture)
            {
                width_out = texture->getWidth();
                height_out = texture->getHeight();
                index++;
                return (ImTextureID)(texture->ptrTexture);
            }
        }
        lua_log_error(lua, "Invalid or released TextureInfo");
        index++;
        return (ImTextureID)(0);
    }
    
    lua_log_error(lua, "Expected TextureInfo, texture filename (string), or texture id (number)");
    index++;
    return (ImTextureID)(0);
}

void assert_imgui_lua(bool value,const char* file_name,const int line)
{
    if(value == false)
    {
        lua_State * lua = static_cast<lua_State *>(mbm::DEVICE::getInstance()->get_lua_state());
        if(lua)
        {
            char log_error[1024] = "";
            snprintf(log_error,sizeof(log_error)-1,"assert failed at\n%s\nline: %d\n",file_name,line);
            lua_log_error(lua,log_error);
        }
        else
        {
            ERROR_AT(line,file_name,"assert failed");
        }
    }
}

// Helper function to merge multiple maps into one
static std::map<std::string, int> mergeFlagMaps(std::initializer_list<std::reference_wrapper<const std::map<std::string, int>>> maps) {
    std::map<std::string, int> result;
    for (const auto& mapRef : maps) {
        result.insert(mapRef.get().begin(), mapRef.get().end());
    }
    return result;
}

static const std::map<std::string,int> enumMouseCursorMap = {
        {"ImGuiMouseCursor_None",                             ImGuiMouseCursor_None},
        {"ImGuiMouseCursor_Arrow",                            ImGuiMouseCursor_Arrow},
        {"ImGuiMouseCursor_TextInput",                        ImGuiMouseCursor_TextInput},
        {"ImGuiMouseCursor_ResizeAll",                        ImGuiMouseCursor_ResizeAll},
        {"ImGuiMouseCursor_ResizeNS",                         ImGuiMouseCursor_ResizeNS},
        {"ImGuiMouseCursor_ResizeEW",                         ImGuiMouseCursor_ResizeEW},
        {"ImGuiMouseCursor_ResizeNESW",                       ImGuiMouseCursor_ResizeNESW},
        {"ImGuiMouseCursor_ResizeNWSE",                       ImGuiMouseCursor_ResizeNWSE},
        {"ImGuiMouseCursor_Hand",                             ImGuiMouseCursor_Hand},
        {"ImGuiMouseCursor_NotAllowed",                       ImGuiMouseCursor_NotAllowed}};

static const std::map<std::string,int> enumDirMap = {
        {"ImGuiDir_None",                                     ImGuiDir_None},
        {"ImGuiDir_Left",                                     ImGuiDir_Left},
        {"ImGuiDir_Right",                                    ImGuiDir_Right},
        {"ImGuiDir_Up",                                       ImGuiDir_Up},
        {"ImGuiDir_Down",                                     ImGuiDir_Down}};

static const std::map<std::string,int> enumKeyMap = {
        {"ImGuiKey_Tab",                                      ImGuiKey_Tab},
        {"ImGuiKey_LeftArrow",                                ImGuiKey_LeftArrow},
        {"ImGuiKey_RightArrow",                               ImGuiKey_RightArrow},
        {"ImGuiKey_UpArrow",                                  ImGuiKey_UpArrow},
        {"ImGuiKey_DownArrow",                                ImGuiKey_DownArrow},
        {"ImGuiKey_PageUp",                                   ImGuiKey_PageUp},
        {"ImGuiKey_PageDown",                                 ImGuiKey_PageDown},
        {"ImGuiKey_Home",                                     ImGuiKey_Home},
        {"ImGuiKey_End",                                      ImGuiKey_End},
        {"ImGuiKey_Insert",                                   ImGuiKey_Insert},
        {"ImGuiKey_Delete",                                   ImGuiKey_Delete},
        {"ImGuiKey_Backspace",                                ImGuiKey_Backspace},
        {"ImGuiKey_Space",                                    ImGuiKey_Space},
        {"ImGuiKey_Enter",                                    ImGuiKey_Enter},
        {"ImGuiKey_Escape",                                   ImGuiKey_Escape},
        {"ImGuiKey_KeypadEnter",                              ImGuiKey_KeypadEnter},
        {"ImGuiKey_A",                                        ImGuiKey_A},
        {"ImGuiKey_C",                                        ImGuiKey_C},
        {"ImGuiKey_V",                                        ImGuiKey_V},
        {"ImGuiKey_X",                                        ImGuiKey_X},
        {"ImGuiKey_Y",                                        ImGuiKey_Y},
        {"ImGuiKey_Z",                                        ImGuiKey_Z},
};

static const std::map<std::string,int> enumColMap = {
        {"ImGuiCol_Text",                                     ImGuiCol_Text},
        {"ImGuiCol_TextDisabled",                             ImGuiCol_TextDisabled},
        {"ImGuiCol_WindowBg",                                 ImGuiCol_WindowBg},
        {"ImGuiCol_ChildBg",                                  ImGuiCol_ChildBg},
        {"ImGuiCol_PopupBg",                                  ImGuiCol_PopupBg},
        {"ImGuiCol_Border",                                   ImGuiCol_Border},
        {"ImGuiCol_BorderShadow",                             ImGuiCol_BorderShadow},
        {"ImGuiCol_FrameBg",                                  ImGuiCol_FrameBg},
        {"ImGuiCol_FrameBgHovered",                           ImGuiCol_FrameBgHovered},
        {"ImGuiCol_FrameBgActive",                            ImGuiCol_FrameBgActive},
        {"ImGuiCol_TitleBg",                                  ImGuiCol_TitleBg},
        {"ImGuiCol_TitleBgActive",                            ImGuiCol_TitleBgActive},
        {"ImGuiCol_TitleBgCollapsed",                         ImGuiCol_TitleBgCollapsed},
        {"ImGuiCol_MenuBarBg",                                ImGuiCol_MenuBarBg},
        {"ImGuiCol_ScrollbarBg",                              ImGuiCol_ScrollbarBg},
        {"ImGuiCol_ScrollbarGrab",                            ImGuiCol_ScrollbarGrab},
        {"ImGuiCol_ScrollbarGrabHovered",                     ImGuiCol_ScrollbarGrabHovered},
        {"ImGuiCol_ScrollbarGrabActive",                      ImGuiCol_ScrollbarGrabActive},
        {"ImGuiCol_CheckMark",                                ImGuiCol_CheckMark},
        {"ImGuiCol_SliderGrab",                               ImGuiCol_SliderGrab},
        {"ImGuiCol_SliderGrabActive",                         ImGuiCol_SliderGrabActive},
        {"ImGuiCol_Button",                                   ImGuiCol_Button},
        {"ImGuiCol_ButtonHovered",                            ImGuiCol_ButtonHovered},
        {"ImGuiCol_ButtonActive",                             ImGuiCol_ButtonActive},
        {"ImGuiCol_Header",                                   ImGuiCol_Header},
        {"ImGuiCol_HeaderHovered",                            ImGuiCol_HeaderHovered},
        {"ImGuiCol_HeaderActive",                             ImGuiCol_HeaderActive},
        {"ImGuiCol_Separator",                                ImGuiCol_Separator},
        {"ImGuiCol_SeparatorHovered",                         ImGuiCol_SeparatorHovered},
        {"ImGuiCol_SeparatorActive",                          ImGuiCol_SeparatorActive},
        {"ImGuiCol_ResizeGrip",                               ImGuiCol_ResizeGrip},
        {"ImGuiCol_ResizeGripHovered",                        ImGuiCol_ResizeGripHovered},
        {"ImGuiCol_ResizeGripActive",                         ImGuiCol_ResizeGripActive},
        {"ImGuiCol_Tab",                                      ImGuiCol_Tab},
        {"ImGuiCol_TabHovered",                               ImGuiCol_TabHovered},
        {"ImGuiCol_TabSelected",                              ImGuiCol_TabSelected},
        {"ImGuiCol_TabDimmed",                                ImGuiCol_TabDimmed},
        {"ImGuiCol_TabDimmedSelected",                        ImGuiCol_TabDimmedSelected},
        {"ImGuiCol_PlotLines",                                ImGuiCol_PlotLines},
        {"ImGuiCol_PlotLinesHovered",                         ImGuiCol_PlotLinesHovered},
        {"ImGuiCol_PlotHistogram",                            ImGuiCol_PlotHistogram},
        {"ImGuiCol_PlotHistogramHovered",                     ImGuiCol_PlotHistogramHovered},
        {"ImGuiCol_TextSelectedBg",                           ImGuiCol_TextSelectedBg},
        {"ImGuiCol_DragDropTarget",                           ImGuiCol_DragDropTarget},
        {"ImGuiCol_NavCursor",                                ImGuiCol_NavCursor},
        {"ImGuiCol_NavWindowingHighlight",                    ImGuiCol_NavWindowingHighlight},
        {"ImGuiCol_NavWindowingDimBg",                        ImGuiCol_NavWindowingDimBg},
        {"ImGuiCol_ModalWindowDimBg",                         ImGuiCol_ModalWindowDimBg}
};

// Typed flag maps for validation
static const std::map<std::string,int> windowFlagsMap = {
        {"ImGuiWindowFlags_None",                             ImGuiWindowFlags_None},
        {"ImGuiWindowFlags_NoTitleBar",                       ImGuiWindowFlags_NoTitleBar},
        {"ImGuiWindowFlags_NoResize",                         ImGuiWindowFlags_NoResize},
        {"ImGuiWindowFlags_NoMove",                           ImGuiWindowFlags_NoMove},
        {"ImGuiWindowFlags_NoScrollbar",                      ImGuiWindowFlags_NoScrollbar},
        {"ImGuiWindowFlags_NoScrollWithMouse",                ImGuiWindowFlags_NoScrollWithMouse},
        {"ImGuiWindowFlags_NoCollapse",                       ImGuiWindowFlags_NoCollapse},
        {"ImGuiWindowFlags_AlwaysAutoResize",                 ImGuiWindowFlags_AlwaysAutoResize},
        {"ImGuiWindowFlags_NoBackground",                     ImGuiWindowFlags_NoBackground},
        {"ImGuiWindowFlags_NoSavedSettings",                  ImGuiWindowFlags_NoSavedSettings},
        {"ImGuiWindowFlags_NoMouseInputs",                    ImGuiWindowFlags_NoMouseInputs},
        {"ImGuiWindowFlags_MenuBar",                          ImGuiWindowFlags_MenuBar},
        {"ImGuiWindowFlags_HorizontalScrollbar",              ImGuiWindowFlags_HorizontalScrollbar},
        {"ImGuiWindowFlags_NoFocusOnAppearing",               ImGuiWindowFlags_NoFocusOnAppearing},
        {"ImGuiWindowFlags_NoBringToFrontOnFocus",            ImGuiWindowFlags_NoBringToFrontOnFocus},
        {"ImGuiWindowFlags_AlwaysVerticalScrollbar",          ImGuiWindowFlags_AlwaysVerticalScrollbar},
        {"ImGuiWindowFlags_AlwaysHorizontalScrollbar",        ImGuiWindowFlags_AlwaysHorizontalScrollbar},
        {"ImGuiWindowFlags_NoNavInputs",                      ImGuiWindowFlags_NoNavInputs},
        {"ImGuiWindowFlags_NoNavFocus",                       ImGuiWindowFlags_NoNavFocus},
        {"ImGuiWindowFlags_UnsavedDocument",                  ImGuiWindowFlags_UnsavedDocument},
        {"ImGuiWindowFlags_NoNav",                            ImGuiWindowFlags_NoNav},
        {"ImGuiWindowFlags_NoDecoration",                     ImGuiWindowFlags_NoDecoration},
        {"ImGuiWindowFlags_NoInputs",                         ImGuiWindowFlags_NoInputs}
};

static const std::map<std::string,int> inputTextFlagsMap = {
        {"ImGuiInputTextFlags_None",                          ImGuiInputTextFlags_None},
        {"ImGuiInputTextFlags_CharsDecimal",                  ImGuiInputTextFlags_CharsDecimal},
        {"ImGuiInputTextFlags_CharsHexadecimal",              ImGuiInputTextFlags_CharsHexadecimal},
        {"ImGuiInputTextFlags_CharsUppercase",                ImGuiInputTextFlags_CharsUppercase},
        {"ImGuiInputTextFlags_CharsNoBlank",                  ImGuiInputTextFlags_CharsNoBlank},
        {"ImGuiInputTextFlags_AutoSelectAll",                 ImGuiInputTextFlags_AutoSelectAll},
        {"ImGuiInputTextFlags_EnterReturnsTrue",              ImGuiInputTextFlags_EnterReturnsTrue},
        {"ImGuiInputTextFlags_CallbackCompletion",            ImGuiInputTextFlags_CallbackCompletion},
        {"ImGuiInputTextFlags_CallbackHistory",               ImGuiInputTextFlags_CallbackHistory},
        {"ImGuiInputTextFlags_CallbackAlways",                ImGuiInputTextFlags_CallbackAlways},
        {"ImGuiInputTextFlags_CallbackCharFilter",            ImGuiInputTextFlags_CallbackCharFilter},
        {"ImGuiInputTextFlags_AllowTabInput",                 ImGuiInputTextFlags_AllowTabInput},
        {"ImGuiInputTextFlags_CtrlEnterForNewLine",           ImGuiInputTextFlags_CtrlEnterForNewLine},
        {"ImGuiInputTextFlags_NoHorizontalScroll",            ImGuiInputTextFlags_NoHorizontalScroll},
        {"ImGuiInputTextFlags_AlwaysOverwrite",               ImGuiInputTextFlags_AlwaysOverwrite},
        {"ImGuiInputTextFlags_ReadOnly",                      ImGuiInputTextFlags_ReadOnly},
        {"ImGuiInputTextFlags_Password",                      ImGuiInputTextFlags_Password},
        {"ImGuiInputTextFlags_NoUndoRedo",                    ImGuiInputTextFlags_NoUndoRedo},
        {"ImGuiInputTextFlags_CharsScientific",               ImGuiInputTextFlags_CharsScientific},
        {"ImGuiInputTextFlags_CallbackResize",                ImGuiInputTextFlags_CallbackResize}
};

static const std::map<std::string,int> sliderFlagsMap = {
        {"ImGuiSliderFlags_None",                             ImGuiSliderFlags_None},
        {"ImGuiSliderFlags_Logarithmic",                      ImGuiSliderFlags_Logarithmic},
        {"ImGuiSliderFlags_NoRoundToFormat",                  ImGuiSliderFlags_NoRoundToFormat},
        {"ImGuiSliderFlags_NoInput",                          ImGuiSliderFlags_NoInput},
        {"ImGuiSliderFlags_WrapAround",                       ImGuiSliderFlags_WrapAround},
        {"ImGuiSliderFlags_ClampOnInput",                     ImGuiSliderFlags_ClampOnInput},
        {"ImGuiSliderFlags_ClampZeroRange",                   ImGuiSliderFlags_ClampZeroRange},
        {"ImGuiSliderFlags_NoSpeedTweaks",                    ImGuiSliderFlags_NoSpeedTweaks},
        {"ImGuiSliderFlags_AlwaysClamp",                      ImGuiSliderFlags_AlwaysClamp}
};

static const std::map<std::string,int> treeNodeFlagsMap = {
        {"ImGuiTreeNodeFlags_None",                           ImGuiTreeNodeFlags_None},
        {"ImGuiTreeNodeFlags_Selected",                       ImGuiTreeNodeFlags_Selected},
        {"ImGuiTreeNodeFlags_Framed",                         ImGuiTreeNodeFlags_Framed},
        {"ImGuiTreeNodeFlags_AllowOverlap",                   ImGuiTreeNodeFlags_AllowOverlap},
        {"ImGuiTreeNodeFlags_NoTreePushOnOpen",               ImGuiTreeNodeFlags_NoTreePushOnOpen},
        {"ImGuiTreeNodeFlags_NoAutoOpenOnLog",                ImGuiTreeNodeFlags_NoAutoOpenOnLog},
        {"ImGuiTreeNodeFlags_DefaultOpen",                    ImGuiTreeNodeFlags_DefaultOpen},
        {"ImGuiTreeNodeFlags_OpenOnDoubleClick",              ImGuiTreeNodeFlags_OpenOnDoubleClick},
        {"ImGuiTreeNodeFlags_OpenOnArrow",                    ImGuiTreeNodeFlags_OpenOnArrow},
        {"ImGuiTreeNodeFlags_Leaf",                           ImGuiTreeNodeFlags_Leaf},
        {"ImGuiTreeNodeFlags_Bullet",                         ImGuiTreeNodeFlags_Bullet},
        {"ImGuiTreeNodeFlags_FramePadding",                   ImGuiTreeNodeFlags_FramePadding},
        {"ImGuiTreeNodeFlags_SpanAvailWidth",                 ImGuiTreeNodeFlags_SpanAvailWidth},
        {"ImGuiTreeNodeFlags_SpanFullWidth",                  ImGuiTreeNodeFlags_SpanFullWidth}
};

static const std::map<std::string,int> selectableFlagsMap = {
        {"ImGuiSelectableFlags_None",                         ImGuiSelectableFlags_None},
        {"ImGuiSelectableFlags_SpanAllColumns",               ImGuiSelectableFlags_SpanAllColumns},
        {"ImGuiSelectableFlags_AllowDoubleClick",             ImGuiSelectableFlags_AllowDoubleClick},
        {"ImGuiSelectableFlags_Disabled",                     ImGuiSelectableFlags_Disabled},
        {"ImGuiSelectableFlags_AllowOverlap",                 ImGuiSelectableFlags_AllowOverlap}
};

static const std::map<std::string,int> comboFlagsMap = {
        {"ImGuiComboFlags_None",                              ImGuiComboFlags_None},
        {"ImGuiComboFlags_PopupAlignLeft",                    ImGuiComboFlags_PopupAlignLeft},
        {"ImGuiComboFlags_HeightSmall",                       ImGuiComboFlags_HeightSmall},
        {"ImGuiComboFlags_HeightRegular",                     ImGuiComboFlags_HeightRegular},
        {"ImGuiComboFlags_HeightLarge",                       ImGuiComboFlags_HeightLarge},
        {"ImGuiComboFlags_HeightLargest",                     ImGuiComboFlags_HeightLargest},
        {"ImGuiComboFlags_NoArrowButton",                     ImGuiComboFlags_NoArrowButton},
        {"ImGuiComboFlags_NoPreview",                         ImGuiComboFlags_NoPreview},
        {"ImGuiComboFlags_HeightMask_",                       ImGuiComboFlags_HeightMask_}
};

static const std::map<std::string,int> tabBarFlagsMap = {
        {"ImGuiTabBarFlags_None",                             ImGuiTabBarFlags_None},
        {"ImGuiTabBarFlags_Reorderable",                      ImGuiTabBarFlags_Reorderable},
        {"ImGuiTabBarFlags_AutoSelectNewTabs",                ImGuiTabBarFlags_AutoSelectNewTabs},
        {"ImGuiTabBarFlags_TabListPopupButton",               ImGuiTabBarFlags_TabListPopupButton},
        {"ImGuiTabBarFlags_NoCloseWithMiddleMouseButton",     ImGuiTabBarFlags_NoCloseWithMiddleMouseButton},
        {"ImGuiTabBarFlags_NoTabListScrollingButtons",        ImGuiTabBarFlags_NoTabListScrollingButtons},
        {"ImGuiTabBarFlags_NoTooltip",                        ImGuiTabBarFlags_NoTooltip},
        {"ImGuiTabBarFlags_FittingPolicyScroll",              ImGuiTabBarFlags_FittingPolicyScroll},
        {"ImGuiTabBarFlags_FittingPolicyMask_",               ImGuiTabBarFlags_FittingPolicyMask_},
        {"ImGuiTabBarFlags_FittingPolicyDefault_",            ImGuiTabBarFlags_FittingPolicyDefault_}
};

static const std::map<std::string,int> tabItemFlagsMap = {
        {"ImGuiTabItemFlags_None",                            ImGuiTabItemFlags_None},
        {"ImGuiTabItemFlags_UnsavedDocument",                 ImGuiTabItemFlags_UnsavedDocument},
        {"ImGuiTabItemFlags_SetSelected",                     ImGuiTabItemFlags_SetSelected},
        {"ImGuiTabItemFlags_NoCloseWithMiddleMouseButton",    ImGuiTabItemFlags_NoCloseWithMiddleMouseButton},
        {"ImGuiTabItemFlags_NoPushId",                        ImGuiTabItemFlags_NoPushId}
};

static const std::map<std::string,int> focusedFlagsMap = {
        {"ImGuiFocusedFlags_None",                            ImGuiFocusedFlags_None},
        {"ImGuiFocusedFlags_ChildWindows",                    ImGuiFocusedFlags_ChildWindows},
        {"ImGuiFocusedFlags_RootWindow",                      ImGuiFocusedFlags_RootWindow},
        {"ImGuiFocusedFlags_AnyWindow",                       ImGuiFocusedFlags_AnyWindow},
        {"ImGuiFocusedFlags_RootAndChildWindows",             ImGuiFocusedFlags_RootAndChildWindows}
};

static const std::map<std::string,int> hoveredFlagsMap = {
        {"ImGuiHoveredFlags_None",                            ImGuiHoveredFlags_None},
        {"ImGuiHoveredFlags_ChildWindows",                    ImGuiHoveredFlags_ChildWindows},
        {"ImGuiHoveredFlags_RootWindow",                      ImGuiHoveredFlags_RootWindow},
        {"ImGuiHoveredFlags_AnyWindow",                       ImGuiHoveredFlags_AnyWindow},
        {"ImGuiHoveredFlags_AllowWhenBlockedByPopup",         ImGuiHoveredFlags_AllowWhenBlockedByPopup},
        {"ImGuiHoveredFlags_AllowWhenBlockedByActiveItem",    ImGuiHoveredFlags_AllowWhenBlockedByActiveItem},
        {"ImGuiHoveredFlags_AllowWhenOverlapped",             ImGuiHoveredFlags_AllowWhenOverlapped},
        {"ImGuiHoveredFlags_AllowWhenDisabled",               ImGuiHoveredFlags_AllowWhenDisabled},
        {"ImGuiHoveredFlags_RectOnly",                        ImGuiHoveredFlags_RectOnly},
        {"ImGuiHoveredFlags_RootAndChildWindows",             ImGuiHoveredFlags_RootAndChildWindows}
};

static const std::map<std::string,int> popupFlagsMap = {
        {"ImGuiPopupFlags_None",                              ImGuiPopupFlags_None},
        {"ImGuiPopupFlags_MouseButtonLeft",                   ImGuiPopupFlags_MouseButtonLeft},
        {"ImGuiPopupFlags_MouseButtonRight",                  ImGuiPopupFlags_MouseButtonRight},
        {"ImGuiPopupFlags_MouseButtonMiddle",                 ImGuiPopupFlags_MouseButtonMiddle},
        {"ImGuiPopupFlags_NoOpenOverExistingPopup",           ImGuiPopupFlags_NoOpenOverExistingPopup},
        {"ImGuiPopupFlags_NoOpenOverItems",                   ImGuiPopupFlags_NoOpenOverItems},
        {"ImGuiPopupFlags_AnyPopupId",                        ImGuiPopupFlags_AnyPopupId},
        {"ImGuiPopupFlags_AnyPopupLevel",                     ImGuiPopupFlags_AnyPopupLevel},
        {"ImGuiPopupFlags_AnyPopup",                          ImGuiPopupFlags_AnyPopup}
};

static const std::map<std::string,int> colorEditFlagsMap = {
        {"ImGuiColorEditFlags_None",                          ImGuiColorEditFlags_None},
        {"ImGuiColorEditFlags_NoAlpha",                       ImGuiColorEditFlags_NoAlpha},
        {"ImGuiColorEditFlags_NoPicker",                      ImGuiColorEditFlags_NoPicker},
        {"ImGuiColorEditFlags_NoOptions",                     ImGuiColorEditFlags_NoOptions},
        {"ImGuiColorEditFlags_NoSmallPreview",                ImGuiColorEditFlags_NoSmallPreview},
        {"ImGuiColorEditFlags_NoInputs",                      ImGuiColorEditFlags_NoInputs},
        {"ImGuiColorEditFlags_NoTooltip",                     ImGuiColorEditFlags_NoTooltip},
        {"ImGuiColorEditFlags_NoLabel",                       ImGuiColorEditFlags_NoLabel},
        {"ImGuiColorEditFlags_NoSidePreview",                 ImGuiColorEditFlags_NoSidePreview},
        {"ImGuiColorEditFlags_NoDragDrop",                    ImGuiColorEditFlags_NoDragDrop},
        {"ImGuiColorEditFlags_AlphaBar",                      ImGuiColorEditFlags_AlphaBar},
        {"ImGuiColorEditFlags_AlphaPreviewHalf",              ImGuiColorEditFlags_AlphaPreviewHalf},
        {"ImGuiColorEditFlags_HDR",                           ImGuiColorEditFlags_HDR},
        {"ImGuiColorEditFlags_DisplayRGB",                    ImGuiColorEditFlags_DisplayRGB},
        {"ImGuiColorEditFlags_DisplayHSV",                    ImGuiColorEditFlags_DisplayHSV},
        {"ImGuiColorEditFlags_DisplayHex",                    ImGuiColorEditFlags_DisplayHex},
        {"ImGuiColorEditFlags_Uint8",                         ImGuiColorEditFlags_Uint8},
        {"ImGuiColorEditFlags_Float",                         ImGuiColorEditFlags_Float},
        {"ImGuiColorEditFlags_PickerHueBar",                  ImGuiColorEditFlags_PickerHueBar},
        {"ImGuiColorEditFlags_PickerHueWheel",                ImGuiColorEditFlags_PickerHueWheel},
        {"ImGuiColorEditFlags_InputRGB",                      ImGuiColorEditFlags_InputRGB},
        {"ImGuiColorEditFlags_InputHSV",                      ImGuiColorEditFlags_InputHSV}
};

static const std::map<std::string,int> itemFlagsMap = {
        {"ImGuiItemFlags_None",                               ImGuiItemFlags_None},
        {"ImGuiItemFlags_NoTabStop",                          ImGuiItemFlags_NoTabStop},
        {"ImGuiItemFlags_NoNav",                              ImGuiItemFlags_NoNav},
        {"ImGuiItemFlags_NoNavDefaultFocus",                  ImGuiItemFlags_NoNavDefaultFocus},
        {"ImGuiItemFlags_ButtonRepeat",                       ImGuiItemFlags_ButtonRepeat},
        {"ImGuiItemFlags_AutoClosePopups",                    ImGuiItemFlags_AutoClosePopups}
};

static const std::map<std::string,int> condFlagsMap = {
        {"ImGuiCond_None",                                    ImGuiCond_None},
        {"ImGuiCond_Always",                                  ImGuiCond_Always},
        {"ImGuiCond_Once",                                    ImGuiCond_Once},
        {"ImGuiCond_FirstUseEver",                            ImGuiCond_FirstUseEver},
        {"ImGuiCond_Appearing",                               ImGuiCond_Appearing}
};

static const std::map<std::string,int> drawFlagsMap = {
        {"ImDrawFlags_None",                                  ImDrawFlags_None},
        {"ImDrawFlags_Closed",                                ImDrawFlags_Closed},
        {"ImDrawFlags_RoundCornersTopLeft",                   ImDrawFlags_RoundCornersTopLeft},
        {"ImDrawFlags_RoundCornersTopRight",                  ImDrawFlags_RoundCornersTopRight},
        {"ImDrawFlags_RoundCornersBottomLeft",                ImDrawFlags_RoundCornersBottomLeft},
        {"ImDrawFlags_RoundCornersBottomRight",               ImDrawFlags_RoundCornersBottomRight},
        {"ImDrawFlags_RoundCornersTop",                       ImDrawFlags_RoundCornersTop},
        {"ImDrawFlags_RoundCornersBottom",                    ImDrawFlags_RoundCornersBottom},
        {"ImDrawFlags_RoundCornersLeft",                      ImDrawFlags_RoundCornersLeft},
        {"ImDrawFlags_RoundCornersRight",                     ImDrawFlags_RoundCornersRight},
        {"ImDrawFlags_RoundCornersAll",                       ImDrawFlags_RoundCornersAll},
        {"ImDrawFlags_RoundCornersNone",                      ImDrawFlags_RoundCornersNone}
};

// Other flags not in specialized typed maps
static const std::map<std::string,int> othersFlag = {
        {"ImGuiDragDropFlags_None",                           ImGuiDragDropFlags_None},
        {"ImGuiDragDropFlags_SourceNoPreviewTooltip",         ImGuiDragDropFlags_SourceNoPreviewTooltip},
        {"ImGuiDragDropFlags_SourceNoDisableHover",           ImGuiDragDropFlags_SourceNoDisableHover},
        {"ImGuiDragDropFlags_SourceNoHoldToOpenOthers",       ImGuiDragDropFlags_SourceNoHoldToOpenOthers},
        {"ImGuiDragDropFlags_SourceAllowNullID",              ImGuiDragDropFlags_SourceAllowNullID},
        {"ImGuiDragDropFlags_SourceExtern",                   ImGuiDragDropFlags_SourceExtern},
        {"ImGuiDragDropFlags_AcceptBeforeDelivery",           ImGuiDragDropFlags_AcceptBeforeDelivery},
        {"ImGuiDragDropFlags_AcceptNoDrawDefaultRect",        ImGuiDragDropFlags_AcceptNoDrawDefaultRect},
        {"ImGuiDragDropFlags_AcceptNoPreviewTooltip",         ImGuiDragDropFlags_AcceptNoPreviewTooltip},
        {"ImGuiDragDropFlags_AcceptPeekOnly",                 ImGuiDragDropFlags_AcceptPeekOnly},
        {"ImGuiConfigFlags_None",                             ImGuiConfigFlags_None},
        {"ImGuiConfigFlags_NavEnableKeyboard",                ImGuiConfigFlags_NavEnableKeyboard},
        {"ImGuiConfigFlags_NavEnableGamepad",                 ImGuiConfigFlags_NavEnableGamepad},
        {"ImGuiConfigFlags_NoMouse",                          ImGuiConfigFlags_NoMouse},
        {"ImGuiConfigFlags_NoMouseCursorChange",              ImGuiConfigFlags_NoMouseCursorChange},
        {"ImGuiConfigFlags_IsSRGB",                           ImGuiConfigFlags_IsSRGB},
        {"ImGuiConfigFlags_IsTouchScreen",                    ImGuiConfigFlags_IsTouchScreen},
        {"ImGuiBackendFlags_None",                            ImGuiBackendFlags_None},
        {"ImGuiBackendFlags_HasGamepad",                      ImGuiBackendFlags_HasGamepad},
        {"ImGuiBackendFlags_HasMouseCursors",                 ImGuiBackendFlags_HasMouseCursors},
        {"ImGuiBackendFlags_HasSetMousePos",                  ImGuiBackendFlags_HasSetMousePos},
        {"ImGuiBackendFlags_RendererHasVtxOffset",            ImGuiBackendFlags_RendererHasVtxOffset},
        {"ImGuiStyleVar_Alpha",                               ImGuiStyleVar_Alpha},
        {"ImGuiStyleVar_DisabledAlpha",                       ImGuiStyleVar_DisabledAlpha},
        {"ImGuiStyleVar_WindowPadding",                       ImGuiStyleVar_WindowPadding},
        {"ImGuiStyleVar_WindowRounding",                      ImGuiStyleVar_WindowRounding},
        {"ImGuiStyleVar_WindowBorderSize",                    ImGuiStyleVar_WindowBorderSize},
        {"ImGuiStyleVar_WindowMinSize",                       ImGuiStyleVar_WindowMinSize},
        {"ImGuiStyleVar_WindowTitleAlign",                    ImGuiStyleVar_WindowTitleAlign},
        {"ImGuiStyleVar_ChildRounding",                       ImGuiStyleVar_ChildRounding},
        {"ImGuiStyleVar_ChildBorderSize",                     ImGuiStyleVar_ChildBorderSize},
        {"ImGuiStyleVar_PopupRounding",                       ImGuiStyleVar_PopupRounding},
        {"ImGuiStyleVar_PopupBorderSize",                     ImGuiStyleVar_PopupBorderSize},
        {"ImGuiStyleVar_FramePadding",                        ImGuiStyleVar_FramePadding},
        {"ImGuiStyleVar_FrameRounding",                       ImGuiStyleVar_FrameRounding},
        {"ImGuiStyleVar_FrameBorderSize",                     ImGuiStyleVar_FrameBorderSize},
        {"ImGuiStyleVar_ItemSpacing",                         ImGuiStyleVar_ItemSpacing},
        {"ImGuiStyleVar_ItemInnerSpacing",                    ImGuiStyleVar_ItemInnerSpacing},
        {"ImGuiStyleVar_IndentSpacing",                       ImGuiStyleVar_IndentSpacing},
        {"ImGuiStyleVar_CellPadding",                         ImGuiStyleVar_CellPadding},
        {"ImGuiStyleVar_ScrollbarSize",                       ImGuiStyleVar_ScrollbarSize},
        {"ImGuiStyleVar_ScrollbarRounding",                   ImGuiStyleVar_ScrollbarRounding},
        {"ImGuiStyleVar_ScrollbarPadding",                    ImGuiStyleVar_ScrollbarPadding},
        {"ImGuiStyleVar_GrabMinSize",                         ImGuiStyleVar_GrabMinSize},
        {"ImGuiStyleVar_GrabRounding",                        ImGuiStyleVar_GrabRounding},
        {"ImGuiStyleVar_ImageRounding",                       ImGuiStyleVar_ImageRounding},
        {"ImGuiStyleVar_ImageBorderSize",                     ImGuiStyleVar_ImageBorderSize},
        {"ImGuiStyleVar_TabRounding",                         ImGuiStyleVar_TabRounding},
        {"ImGuiStyleVar_TabBorderSize",                       ImGuiStyleVar_TabBorderSize},
        {"ImGuiStyleVar_TabMinWidthBase",                     ImGuiStyleVar_TabMinWidthBase},
        {"ImGuiStyleVar_TabMinWidthShrink",                   ImGuiStyleVar_TabMinWidthShrink},
        {"ImGuiStyleVar_TabBarBorderSize",                    ImGuiStyleVar_TabBarBorderSize},
        {"ImGuiStyleVar_TabBarOverlineSize",                  ImGuiStyleVar_TabBarOverlineSize},
        {"ImGuiStyleVar_TableAngledHeadersAngle",             ImGuiStyleVar_TableAngledHeadersAngle},
        {"ImGuiStyleVar_TableAngledHeadersTextAlign",         ImGuiStyleVar_TableAngledHeadersTextAlign},
        {"ImGuiStyleVar_TreeLinesSize",                       ImGuiStyleVar_TreeLinesSize},
        {"ImGuiStyleVar_TreeLinesRounding",                   ImGuiStyleVar_TreeLinesRounding},
        {"ImGuiStyleVar_ButtonTextAlign",                     ImGuiStyleVar_ButtonTextAlign},
        {"ImGuiStyleVar_SelectableTextAlign",                 ImGuiStyleVar_SelectableTextAlign},
        {"ImGuiStyleVar_SeparatorTextBorderSize",             ImGuiStyleVar_SeparatorTextBorderSize},
        {"ImGuiStyleVar_SeparatorTextAlign",                  ImGuiStyleVar_SeparatorTextAlign},
        {"ImGuiStyleVar_SeparatorTextPadding",                 ImGuiStyleVar_SeparatorTextPadding},
        {"ImGuiMouseButton_Left",                             ImGuiMouseButton_Left},
        {"ImGuiMouseButton_Right",                            ImGuiMouseButton_Right},
        {"ImGuiMouseButton_Middle",                           ImGuiMouseButton_Middle},
        {"ImGuiMouseButton_COUNT",                            ImGuiMouseButton_COUNT},
        {"ImDrawListFlags_None",                              ImDrawListFlags_None},
        {"ImDrawListFlags_AntiAliasedLines",                  ImDrawListFlags_AntiAliasedLines},
        {"ImDrawListFlags_AntiAliasedFill",                   ImDrawListFlags_AntiAliasedFill},
        {"ImDrawListFlags_AllowVtxOffset",                    ImDrawListFlags_AllowVtxOffset},
        // ImGuiTableFlags (BeginTable)
        {"ImGuiTableFlags_None",                              ImGuiTableFlags_None},
        {"ImGuiTableFlags_Resizable",                         ImGuiTableFlags_Resizable},
        {"ImGuiTableFlags_Reorderable",                      ImGuiTableFlags_Reorderable},
        {"ImGuiTableFlags_Hideable",                          ImGuiTableFlags_Hideable},
        {"ImGuiTableFlags_Sortable",                          ImGuiTableFlags_Sortable},
        {"ImGuiTableFlags_NoSavedSettings",                   ImGuiTableFlags_NoSavedSettings},
        {"ImGuiTableFlags_ContextMenuInBody",                 ImGuiTableFlags_ContextMenuInBody},
        {"ImGuiTableFlags_RowBg",                             ImGuiTableFlags_RowBg},
        {"ImGuiTableFlags_BordersInnerH",                     ImGuiTableFlags_BordersInnerH},
        {"ImGuiTableFlags_BordersOuterH",                     ImGuiTableFlags_BordersOuterH},
        {"ImGuiTableFlags_BordersInnerV",                     ImGuiTableFlags_BordersInnerV},
        {"ImGuiTableFlags_BordersOuterV",                     ImGuiTableFlags_BordersOuterV},
        {"ImGuiTableFlags_BordersH",                          ImGuiTableFlags_BordersH},
        {"ImGuiTableFlags_BordersV",                          ImGuiTableFlags_BordersV},
        {"ImGuiTableFlags_BordersInner",                      ImGuiTableFlags_BordersInner},
        {"ImGuiTableFlags_BordersOuter",                      ImGuiTableFlags_BordersOuter},
        {"ImGuiTableFlags_Borders",                           ImGuiTableFlags_Borders},
        {"ImGuiTableFlags_NoBordersInBody",                   ImGuiTableFlags_NoBordersInBody},
        {"ImGuiTableFlags_NoBordersInBodyUntilResize",         ImGuiTableFlags_NoBordersInBodyUntilResize},
        {"ImGuiTableFlags_SizingFixedFit",                    ImGuiTableFlags_SizingFixedFit},
        {"ImGuiTableFlags_SizingFixedSame",                   ImGuiTableFlags_SizingFixedSame},
        {"ImGuiTableFlags_SizingStretchProp",                 ImGuiTableFlags_SizingStretchProp},
        {"ImGuiTableFlags_SizingStretchSame",                 ImGuiTableFlags_SizingStretchSame},
        {"ImGuiTableFlags_NoHostExtendX",                     ImGuiTableFlags_NoHostExtendX},
        {"ImGuiTableFlags_NoHostExtendY",                     ImGuiTableFlags_NoHostExtendY},
        {"ImGuiTableFlags_NoKeepColumnsVisible",              ImGuiTableFlags_NoKeepColumnsVisible},
        {"ImGuiTableFlags_PreciseWidths",                     ImGuiTableFlags_PreciseWidths},
        {"ImGuiTableFlags_NoClip",                            ImGuiTableFlags_NoClip},
        {"ImGuiTableFlags_PadOuterX",                         ImGuiTableFlags_PadOuterX},
        {"ImGuiTableFlags_NoPadOuterX",                       ImGuiTableFlags_NoPadOuterX},
        {"ImGuiTableFlags_NoPadInnerX",                       ImGuiTableFlags_NoPadInnerX},
        {"ImGuiTableFlags_ScrollX",                           ImGuiTableFlags_ScrollX},
        {"ImGuiTableFlags_ScrollY",                           ImGuiTableFlags_ScrollY},
        {"ImGuiTableFlags_SortMulti",                         ImGuiTableFlags_SortMulti},
        {"ImGuiTableFlags_SortTristate",                      ImGuiTableFlags_SortTristate},
        {"ImGuiTableFlags_HighlightHoveredColumn",            ImGuiTableFlags_HighlightHoveredColumn},
        // ImGuiTableColumnFlags (TableSetupColumn)
        {"ImGuiTableColumnFlags_None",                        ImGuiTableColumnFlags_None},
        {"ImGuiTableColumnFlags_Disabled",                    ImGuiTableColumnFlags_Disabled},
        {"ImGuiTableColumnFlags_DefaultHide",                 ImGuiTableColumnFlags_DefaultHide},
        {"ImGuiTableColumnFlags_DefaultSort",                 ImGuiTableColumnFlags_DefaultSort},
        {"ImGuiTableColumnFlags_WidthStretch",                ImGuiTableColumnFlags_WidthStretch},
        {"ImGuiTableColumnFlags_WidthFixed",                  ImGuiTableColumnFlags_WidthFixed},
        {"ImGuiTableColumnFlags_NoResize",                    ImGuiTableColumnFlags_NoResize},
        {"ImGuiTableColumnFlags_NoReorder",                   ImGuiTableColumnFlags_NoReorder},
        {"ImGuiTableColumnFlags_NoHide",                       ImGuiTableColumnFlags_NoHide},
        {"ImGuiTableColumnFlags_NoClip",                      ImGuiTableColumnFlags_NoClip},
        {"ImGuiTableColumnFlags_NoSort",                       ImGuiTableColumnFlags_NoSort},
        {"ImGuiTableColumnFlags_NoSortAscending",              ImGuiTableColumnFlags_NoSortAscending},
        {"ImGuiTableColumnFlags_NoSortDescending",            ImGuiTableColumnFlags_NoSortDescending},
        {"ImGuiTableColumnFlags_NoHeaderLabel",               ImGuiTableColumnFlags_NoHeaderLabel},
        {"ImGuiTableColumnFlags_NoHeaderWidth",               ImGuiTableColumnFlags_NoHeaderWidth},
        {"ImGuiTableColumnFlags_PreferSortAscending",         ImGuiTableColumnFlags_PreferSortAscending},
        {"ImGuiTableColumnFlags_PreferSortDescending",        ImGuiTableColumnFlags_PreferSortDescending},
        {"ImGuiTableColumnFlags_IndentEnable",                ImGuiTableColumnFlags_IndentEnable},
        {"ImGuiTableColumnFlags_IndentDisable",               ImGuiTableColumnFlags_IndentDisable},
        {"ImGuiTableColumnFlags_AngledHeader",                ImGuiTableColumnFlags_AngledHeader},
        {"ImGuiTableColumnFlags_IsEnabled",                   ImGuiTableColumnFlags_IsEnabled},
        {"ImGuiTableColumnFlags_IsVisible",                   ImGuiTableColumnFlags_IsVisible},
        {"ImGuiTableColumnFlags_IsSorted",                    ImGuiTableColumnFlags_IsSorted},
        {"ImGuiTableColumnFlags_IsHovered",                   ImGuiTableColumnFlags_IsHovered},
        // ImGuiTableRowFlags (TableNextRow)
        {"ImGuiTableRowFlags_None",                           ImGuiTableRowFlags_None},
        {"ImGuiTableRowFlags_Headers",                        ImGuiTableRowFlags_Headers},
        // ImGuiTableBgTarget (TableSetBgColor)
        {"ImGuiTableBgTarget_None",                           ImGuiTableBgTarget_None},
        {"ImGuiTableBgTarget_RowBg0",                         ImGuiTableBgTarget_RowBg0},
        {"ImGuiTableBgTarget_RowBg1",                         ImGuiTableBgTarget_RowBg1},
        {"ImGuiTableBgTarget_CellBg",                         ImGuiTableBgTarget_CellBg}
};

// Combine all flag maps for lookup
static const std::map<std::string,int> allFlags = mergeFlagMaps({
    std::cref(enumMouseCursorMap),
    std::cref(enumDirMap),
    std::cref(enumKeyMap),
    std::cref(enumColMap),
    std::cref(windowFlagsMap),
    std::cref(inputTextFlagsMap),
    std::cref(sliderFlagsMap),
    std::cref(treeNodeFlagsMap),
    std::cref(selectableFlagsMap),
    std::cref(comboFlagsMap),
    std::cref(tabBarFlagsMap),
    std::cref(tabItemFlagsMap),
    std::cref(focusedFlagsMap),
    std::cref(hoveredFlagsMap),
    std::cref(popupFlagsMap),
    std::cref(colorEditFlagsMap),
    std::cref(itemFlagsMap),
    std::cref(condFlagsMap),
    std::cref(drawFlagsMap),
    std::cref(othersFlag)
});



void lua_log_error(lua_State *lua,const char * message)
{
    lua_Debug ar;
    memset(&ar, 0, sizeof(lua_Debug));
    if (lua_getstack(lua, 1, &ar))
    {
        if (lua_getinfo(lua, "nSl", &ar))
        {
            static bool show_stack = false;
            if(show_stack == false)
                mbm::printStack(lua,ar.short_src,ar.currentline);
            show_stack = true;
            luaL_error(lua,"File[%s] line [%d] \n    %s",ar.short_src,ar.currentline,message);
        }
        else
        {
            luaL_error(lua,"File[unknown] line [?] \n    %s",message);
        }
    }
    else
    {
        luaL_error(lua,"File[unknown] line [?] \n    %s",message);
    }
}

void lua_check_is_table(lua_State *lua, const int index,const char * table_name)
{
    if (lua_type(lua,index) != LUA_TTABLE)
    {
        std::string message("Expected table [");
        message.append(table_name ? table_name : "No_name");
        message.append("]");
        lua_log_error(lua,message.c_str());
    }
}

void get_ImVec2_arrayFromTable(lua_State *lua, const int index, std::vector<ImVec2> & lsArrayOut,const char* table_name);
void get_int_arrayFromTable(lua_State *lua, const int index, int *lsArrayOut, const unsigned int sizeBuffer,const char* table_name);
void get_float_arrayFromTable(lua_State *lua, const int index, float *lsArrayOut, const unsigned int sizeBuffer,const char* table_name);
std::vector<std::string> get_string_arrayFromTable(lua_State *lua, const int index,const char* table_name);
void push_float_arrayFromTable(lua_State *lua, const float *lsArrayIn, const unsigned int sizeBuffer);
void push_RGBA_arrayFromTable(lua_State *lua, const ImVec4 * lsArrayIn, const unsigned int sizeBuffer);
void push_int_arrayFromTable(lua_State *lua, const int *lsArrayIn, const unsigned int sizeBuffer);

const char * get_string_or_null(lua_State *lua,const int index_input)
{
    if(lua_type(lua,index_input) == LUA_TSTRING)
    {
        const char * my_string = lua_tostring(lua,index_input);
        return my_string;
    }
    else
    {
        return nullptr;
    }
}


lua_Number get_number_from_field(lua_State* lua,const int index,lua_Number in_out,const char* field_name)
{
    lua_getfield(lua, index, field_name);
    if(lua_type(lua,-1) == LUA_TNUMBER)
        in_out = lua_tonumber(lua,-1);
    lua_pop(lua, 1);
    return in_out;
}

const char * get_string_from_field(lua_State* lua,const int index,const char* field_name)
{
    static std::string out_string;
    out_string.clear();
    lua_getfield(lua, index, field_name);
    if(lua_type(lua,-1) == LUA_TSTRING)
        out_string = lua_tostring(lua,-1);
    lua_pop(lua, 1);
    return out_string.c_str();
}

// Helper to build error message with valid flags
static std::string buildValidFlagsMessage(const std::map<std::string, int>& validFlags)
{
    std::string msg = "Valid flags are: ";
    bool first = true;
    for (const auto& pair : validFlags)
    {
        if (!first) msg += ", ";
        msg += pair.first;
        first = false;
    }
    return msg;
}

// Helper to check if integer value is valid for the given flag map
static bool isValidFlagValue(int value, const std::map<std::string, int>& validFlags)
{
    if (value == 0) return true; // 0 (None) is always valid
    
    // Check if value matches any single flag or combination of flags
    int remaining = value;
    for (const auto& pair : validFlags)
    {
        if (pair.second != 0 && (remaining & pair.second) == pair.second)
        {
            remaining &= ~pair.second;
        }
    }
    return remaining == 0;
}

/*
    Helper function to read ImGui flags from Lua stack with validation.
    Accepts:
    - String: single flag name (e.g., "ImGuiWindowFlags_NoTitleBar")
    - Table: array of flag names to combine (e.g., {"ImGuiWindowFlags_NoTitleBar", "ImGuiWindowFlags_NoResize"})
    - Integer: validated against validFlags map
    
    Parameters:
    - lua: Lua state
    - index: stack index
    - default_value: value to return if nil/none
    - validFlags: map of valid flags for this parameter (for validation)
*/
int lua_get_flags(lua_State *lua, int index, int default_value, const std::map<std::string, int>& validFlags)
{
    const int lua_type_at_index = lua_type(lua, index);
    
    if (lua_type_at_index == LUA_TNIL || lua_type_at_index == LUA_TNONE)
    {
        return default_value;
    }
    
    if (lua_type_at_index == LUA_TSTRING)
    {
        // Single string flag - validate against specific map first, then allFlags
        const char* flag_name = lua_tostring(lua, index);
        auto itFlag = validFlags.find(flag_name);
        if (itFlag != validFlags.cend())
        {
            return itFlag->second;
        }
        // Also check allFlags for backward compatibility
        itFlag = allFlags.find(flag_name);
        if (itFlag != allFlags.cend())
        {
            // Warn in debug mode if flag is not in expected set
            WARN_LOG("Flag [%s] is not in the expected flag set for this function.\n%s\n", 
                   flag_name, buildValidFlagsMessage(validFlags).c_str());
            return itFlag->second;
        }
        WARN_LOG("Flag [%s] not found!\n%s\n", flag_name, buildValidFlagsMessage(validFlags).c_str());
        return default_value;
    }
    
    if (lua_type_at_index == LUA_TTABLE)
    {
        // Table of flag strings - combine them
        int combined_flags = 0;
        std::vector<std::string> flags = get_string_arrayFromTable(lua, index, "flags");
        for (std::size_t i = 0; i < flags.size(); ++i)
        {
            auto itFlag = validFlags.find(flags[i]);
            if (itFlag != validFlags.cend())
            {
                combined_flags |= itFlag->second;
            }
            else
            {
                // Check allFlags for backward compatibility
                itFlag = allFlags.find(flags[i]);
                if (itFlag != allFlags.cend())
                {
                    WARN_LOG("Flag [%s] is not in the expected flag set for this function.\n", flags[i].c_str());
                    combined_flags |= itFlag->second;
                }
                else
                {
                    WARN_LOG("Flag [%s] not found!\n%s\n", flags[i].c_str(), buildValidFlagsMessage(validFlags).c_str());
                }
            }
        }
        return combined_flags;
    }
    
    if (lua_type_at_index == LUA_TNUMBER)
    {
        const int flag_value = static_cast<int>(lua_tointeger(lua, index));
        // validate the integer value
        if (!isValidFlagValue(flag_value, validFlags))
        {
            std::string error_msg = "Invalid flag value passed as integer. ";
            error_msg += buildValidFlagsMessage(validFlags);
            lua_log_error(lua, error_msg.c_str());
            return default_value;
        }
        return flag_value;
    }
    
    // Unknown type
    lua_log_error(lua, "Invalid type for flags parameter. Expected string, table, integer, or nil.");
    return default_value;
}

// Overload without validation map (uses allFlags, no specific validation)
int lua_get_flags(lua_State *lua, int index, int default_value)
{
    return lua_get_flags(lua, index, default_value, allFlags);
}

// Convenience function for required flags with validation map
int lua_check_flags(lua_State *lua, int &index_input, const std::map<std::string, int>& validFlags)
{
    return lua_get_flags(lua, index_input++, 0, validFlags);
}

// Convenience function for required flags without validation (backward compat)
int lua_check_flags(lua_State *lua, int &index_input)
{
    return lua_get_flags(lua, index_input++, 0, allFlags);
}

// Convenience function for optional flags with validation map
int lua_opt_flags(lua_State *lua, int top, int &index_input, int default_value, const std::map<std::string, int>& validFlags)
{
    if (top >= index_input)
    {
        return lua_get_flags(lua, index_input++, default_value, validFlags);
    }
    return default_value;
}

// Convenience function for optional flags without validation (backward compat)
int lua_opt_flags(lua_State *lua, int top, int &index_input, int default_value)
{
    if (top >= index_input)
    {
        return lua_get_flags(lua, index_input++, default_value, allFlags);
    }
    return default_value;
}

void lua_get_rgba_FromTable(lua_State * lua, int index, float p_col[4]);
ImVec4 lua_get_rgba_to_ImVec4_fromTable(lua_State * lua,const int index);
void lua_push_rgba(lua_State * lua, const float p_col[4]);
void lua_push_rgba(lua_State * lua, const ImVec4 & color);
ImGuiStyle * lua_pop_ImGuiStyle_pointer(lua_State *lua, const int index,ImGuiStyle * p_ImGuiStyle);
ImVec2 * lua_pop_ImVec2_pointer(lua_State *lua, const int index,ImVec2 * p_ImVec2);
ImVec2 lua_pop_ImVec2(lua_State *lua, const int index);
ImVec4 * lua_pop_ImVec4_pointer(lua_State *lua, const int index,ImVec4 * p_ImVec4);
ImVec4 lua_pop_ImVec4(lua_State *lua, const int index);
void lua_push_ImGuiStyle(lua_State *lua, const ImGuiStyle & in);
void lua_push_ImVec2(lua_State *lua, const ImVec2 & in);
void lua_push_ImVec2_pointer(lua_State *lua,const ImVec2 * p_ImVec2);
void lua_push_ImVec4(lua_State *lua, const ImVec4 & in);
void lua_push_ImVec4_pointer(lua_State *lua,const ImVec4 * p_ImVec4);


class IMGUI_LUA : public PLUGIN // structure that represent the wrapper
{
public:
    IMGUI_LUA():KEY_SPACE(' '),KEY_0('0'),KEY_1('1'),KEY_9('9'),KEY_A('A'),KEY_Z('Z')
    {
        imGuiContext            = nullptr;
        delta                   = 0;//updated each loop
        sx                      = 1.0f;
        sy                      = 1.0f;
        MousePos.x     = 0;
        MousePos.y     = 0;
        MousePosPrev.x = 0;
        MousePosPrev.y = 0;
        beginRenderWasCalled = false;

    }
    const int       KEY_SPACE,KEY_0,KEY_1,KEY_9,KEY_A,KEY_Z;
    float           delta,sx,sy;
    bool            beginRenderWasCalled;
    
    ImVec2 MousePos,MousePosPrev;
    std::set<int> keysDown;  // Track keys that are currently down to filter OS key repeats

    ImGuiContext*   imGuiContext;
    #if (defined(__linux__) || defined(__APPLE__)) && !defined (ANDROID)
        Display*    context;
    #elif defined(_WIN32)
        HWND        context;
    #elif defined(ANDROID)
        JNIEnv*     context;
    #endif

    void onSubscribe(int width,int height, void * _context)
    {
        #if defined DEBUG || defined _DEBUG
            // Debug: Print struct sizes to identify mismatch
            printf("Subscribing to ImGui with context: %p\n", _context);
            printf("=== ImGui Struct Size Debug ===\n");
            printf("sizeof(ImGuiIO):    %zu\n", sizeof(ImGuiIO));
            printf("sizeof(ImGuiStyle): %zu\n", sizeof(ImGuiStyle));
            printf("sizeof(ImVec2):     %zu\n", sizeof(ImVec2));
            printf("sizeof(ImVec4):     %zu\n", sizeof(ImVec4));
            printf("sizeof(ImDrawVert): %zu\n", sizeof(ImDrawVert));
            printf("sizeof(ImDrawIdx):  %zu\n", sizeof(ImDrawIdx));
            printf("IMGUI_VERSION:      %s\n", IMGUI_VERSION);
            printf("==============================\n");
        #endif  
        
        IMGUI_CHECKVERSION();
        imGuiContext = ImGui::CreateContext();
        if(imGuiContext != nullptr)
        {
            // IMPORTANT: When using ImGui as a DLL, you MUST set the context explicitly
            // because each DLL has its own static storage
            ImGui::SetCurrentContext(imGuiContext);
            
            // Setup Dear ImGui style
            ImGui::StyleColorsDark();
            ImGuiIO& imGuIo = ImGui::GetIO();
            
            // Load default font with extended Latin characters (includes Portuguese accents)
            ImFontConfig font_cfg;
            font_cfg.OversampleH = 2;
            font_cfg.OversampleV = 2;
            font_cfg.PixelSnapH = true;
            
            // GetGlyphRangesDefault() includes basic Latin (0x0020-0x00FF) which covers Portuguese
            // Font atlas will be built automatically when rendering starts
            imGuIo.Fonts->AddFontDefault(&font_cfg);

            // High values to prevent rapid navigation repeat when holding arrow keys
            // This affects tree navigation, list selection, etc.
            // Text field character repeat still works via AddInputCharacter
            imGuIo.KeyRepeatDelay = 0.400f;  // Time before first repeat (default 0.25f)
            imGuIo.KeyRepeatRate = 0.080f;   // Time between repeats (default 0.05f)
            
            //imgui_impl_opengl3.cpp
            //ImGui_ImplOpenGL3_Init("#version 110");
            //#if defined _WIN32
            //    glShaderSource(g_VertHandle, 1, &vertex_shader, nullptr);
            //#else
            //    glShaderSource(g_VertHandle, 2, vertex_shader_with_version, nullptr);
            //#endif
#if defined(USE_OPENGL_ES)
    #if defined (IMGUI_IMPL_OPENGL_ES2) || defined (IMGUI_IMPL_OPENGL_ES3)
            ImGui_ImplOpenGL3_Init("#version 100");
    #else
        #error "Not implemented for opengl if not using ImGui_ImplOpenGL_ES2 or ImGui_ImplOpenGL_ES3"
    #endif
#elif defined USE_DIRECTX9
            ImGui_ImplDX9_Init(_context);
#elif defined USE_DUMMY_BACK_END_ENGINE
            REMINDER_TODO
#else
            #error "Not implemented for ImGui Init"
#endif

            #if defined _WIN32
                context = static_cast<HWND>(_context);
                ImGui_ImplWin32_Init(context);
                // Windows: clipboard uses built-in Win32 handlers from imgui.cpp
            #elif (defined(__linux__) || defined(__APPLE__)) && !defined (ANDROID)
                context = static_cast<Display*>(_context);
                // Linux/macOS: install xclip/xsel-based clipboard for OS integration
                ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
                platform_io.Platform_GetClipboardTextFn = Platform_GetClipboardTextFn_Linux;
                platform_io.Platform_SetClipboardTextFn = Platform_SetClipboardTextFn_Linux;
            #endif

            imGuIo.DeltaTime            = 1.0f/60.0f;
            imGuIo.DisplaySize.x        = static_cast<float>(width);
            imGuIo.DisplaySize.y        = static_cast<float>(height);

            imGuIo.MousePos.x           = 0;
            imGuIo.MousePos.y           = 0;
            imGuIo.MouseClickedPos[0].x = 0;
            imGuIo.MouseClickedPos[0].y = 0;
            imGuIo.MouseClickedPos[1].x = 0;
            imGuIo.MouseClickedPos[1].y = 0;
            imGuIo.MouseClickedPos[2].x = 0;
            imGuIo.MouseClickedPos[2].y = 0;
            imGuIo.MouseClickedPos[3].x = 0;
            imGuIo.MouseClickedPos[3].y = 0;
            imGuIo.MouseClickedPos[4].x = 0;
            imGuIo.MouseClickedPos[4].y = 0;

            // Backend flags
            imGuIo.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
            imGuIo.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;
        }
        else
        {
            printf("Failed to CreateContext ImGui");
        }
    
    }

    void onResizeWindow(int width,int height)
    {
        if(imGuiContext)
        {
            ImGuiIO& imGuIo = ImGui::GetIO();
            imGuIo.DisplaySize.x        = static_cast<float>(width);
            imGuIo.DisplaySize.y        = static_cast<float>(height);
        }
    }

    void onTouchDown(int key, float x, float y)
    {
        // On Windows, ImGui_ImplWin32 handles mouse input automatically via WndProc however we implement because can't modify the engine's WndProc
        // On Linux/Android, we must feed input manually for both mouse and touch
        if(imGuiContext)
        {
            ImGuiIO& io = ImGui::GetIO();
            x *= sx;
            y *= sy;
            MousePos.x = x;
            MousePos.y = y;
            io.AddMousePosEvent(x, y);
            if(key >= 0 && key < ImGuiMouseButton_COUNT)
            {
                io.AddMouseButtonEvent(key, true);
            }
        }
    }

    void onTouchUp(int key, float x, float y)
    {
        // On Windows, ImGui_ImplWin32 handles mouse input automatically via WndProc however we implement because can't modify the engine's WndProc
        // On Linux/Android, we must feed input manually for both mouse and touch
        if(imGuiContext)
        {
            ImGuiIO& io = ImGui::GetIO();
            x *= sx;
            y *= sy;
            MousePos.x = x;
            MousePos.y = y;
            io.AddMousePosEvent(x, y);
            if(key >= 0 && key < ImGuiMouseButton_COUNT)
            {
                io.AddMouseButtonEvent(key, false);
            }
        }
    }

    void onTouchMove(int key, float x, float y)
    {
        // On Windows, ImGui_ImplWin32 handles mouse input automatically via WndProc however we implement because can't modify the engine's WndProc
        // On Linux/Android, we must feed input manually for both mouse and touch
        if(imGuiContext)
        {
            ImGuiIO& io = ImGui::GetIO();
            x *= sx;
            y *= sy;
            MousePosPrev.x  = MousePos.x;
            MousePosPrev.y  = MousePos.y;
            MousePos.x      = x;
            MousePos.y      = y;
            io.AddMousePosEvent(x, y);
        }
    }

    void onTouchZoom(float zoom)
    {
        // On Windows, ImGui_ImplWin32 handles mouse input automatically via WndProc however we implement because can't modify the engine's WndProc
        // On Linux/Android, we must feed input manually for both mouse and touch
        if(imGuiContext)
        {
            ImGuiIO& io = ImGui::GetIO();
            io.AddMouseWheelEvent(0.0f, zoom);
        }
    }
    
    void onKeyDown(int key)
    {
        // NOTE: ImGui_ImplWin32_NewFrame() handles time updates, gamepad polling, and keyboard workarounds
        // but does NOT handle mouse/keyboard input (requires WndProc hook which we can't modify).
        // We feed all input manually via these callbacks on all platforms.

        if(imGuiContext)
        {
            ImGuiIO& io = ImGui::GetIO();
            
            // Check if this is an OS key repeat (key already down)
            // If so, skip AddKeyEvent but still process character input for text fields
            bool isRepeat = (keysDown.find(key) != keysDown.end());
            
            // Map native key to ImGuiKey
            ImGuiKey imgui_key = MapNativeKeyToImGuiKey(key);
            if (imgui_key != ImGuiKey_None && !isRepeat)
            {
                // For navigation keys (arrows), send as instant tap to prevent
                // ImGui's internal repeat from processing multiple frames
                bool isNavKey = (imgui_key == ImGuiKey_UpArrow || 
                                 imgui_key == ImGuiKey_DownArrow ||
                                 imgui_key == ImGuiKey_LeftArrow || 
                                 imgui_key == ImGuiKey_RightArrow ||
                                 imgui_key == ImGuiKey_Home ||
                                 imgui_key == ImGuiKey_End ||
                                 imgui_key == ImGuiKey_PageUp ||
                                 imgui_key == ImGuiKey_PageDown);
                
                io.AddKeyEvent(imgui_key, true);
                if (isNavKey)
                {
                    // Immediately release nav keys so they don't repeat
                    io.AddKeyEvent(imgui_key, false);
                    // Don't add to keysDown so next OS repeat can trigger another single nav
                }
                else
                {
                    keysDown.insert(key);
                }
            }
            
            // Handle modifier keys
            #if defined(_WIN32)
            if (key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL)
                io.AddKeyEvent(ImGuiMod_Ctrl, true);
            if (key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT)
                io.AddKeyEvent(ImGuiMod_Shift, true);
            if (key == VK_MENU || key == VK_LMENU || key == VK_RMENU)
                io.AddKeyEvent(ImGuiMod_Alt, true);
            if (key == VK_LWIN || key == VK_RWIN)
                io.AddKeyEvent(ImGuiMod_Super, true);
            
            // Add character input for text fields (only for printable characters)
            if (key >= 'A' && key <= 'Z')
            {
                // Check if shift is held - if so, use uppercase, otherwise lowercase
                bool shift_held = io.KeyShift;
                bool caps_on = isCapsLockOn();
                bool uppercase = (shift_held && !caps_on) || (!shift_held && caps_on);
                io.AddInputCharacter(uppercase ? key : (key - 'A' + 'a'));
            }
            else if (key >= '0' && key <= '9')
            {
                // Handle shift+number for symbols
                if (io.KeyShift)
                {
                    static const char shifted[] = ")!@#$%^&*(";
                    io.AddInputCharacter(shifted[key - '0']);
                }
                else
                {
                    io.AddInputCharacter(key);
                }
            }
            // Numpad numbers (NumLock on)
            else if (key >= VK_NUMPAD0 && key <= VK_NUMPAD9)
            {
                io.AddInputCharacter('0' + (key - VK_NUMPAD0));
            }
            else if (key == VK_DECIMAL)
            {
                io.AddInputCharacter('.');
            }
            else if (key == VK_DIVIDE)
            {
                io.AddInputCharacter('/');
            }
            else if (key == VK_MULTIPLY)
            {
                io.AddInputCharacter('*');
            }
            else if (key == VK_SUBTRACT)
            {
                io.AddInputCharacter('-');
            }
            else if (key == VK_ADD)
            {
                io.AddInputCharacter('+');
            }
            else if (key == VK_SPACE)
            {
                io.AddInputCharacter(' ');
            }
            else if (key == VK_RETURN)
            {
                io.AddInputCharacter('\n');
            }
            // Handle special punctuation keys on Windows
            else if (key == VK_OEM_1) // ';:'
            {
                io.AddInputCharacter(io.KeyShift ? ':' : ';');
            }
            else if (key == VK_OEM_PLUS) // '=+'
            {
                io.AddInputCharacter(io.KeyShift ? '+' : '=');
            }
            else if (key == VK_OEM_COMMA) // ',<'
            {
                io.AddInputCharacter(io.KeyShift ? '<' : ',');
            }
            else if (key == VK_OEM_MINUS) // '-_'
            {
                io.AddInputCharacter(io.KeyShift ? '_' : '-');
            }
            else if (key == VK_OEM_PERIOD) // '.>'
            {
                io.AddInputCharacter(io.KeyShift ? '>' : '.');
            }
            else if (key == VK_OEM_2) // '/?'
            {
                io.AddInputCharacter(io.KeyShift ? '?' : '/');
            }
            else if (key == VK_OEM_3) // '`~'
            {
                io.AddInputCharacter(io.KeyShift ? '~' : '`');
            }
            else if (key == VK_OEM_4) // '[{'
            {
                io.AddInputCharacter(io.KeyShift ? '{' : '[');
            }
            else if (key == VK_OEM_5) // '\|'
            {
                io.AddInputCharacter(io.KeyShift ? '|' : '\\');
            }
            else if (key == VK_OEM_6) // ']}'
            {
                io.AddInputCharacter(io.KeyShift ? '}' : ']');
            }
            else if (key == VK_OEM_7) // '\'"'
            {
                io.AddInputCharacter(io.KeyShift ? '"' : '\'');
            }
            #elif (defined(__linux__) || defined(__APPLE__)) && !defined (ANDROID)
            if (key == XK_Control_L || key == XK_Control_R)
                io.AddKeyEvent(ImGuiMod_Ctrl, true);
            if (key == XK_Shift_L || key == XK_Shift_R)
                io.AddKeyEvent(ImGuiMod_Shift, true);
            if (key == XK_Alt_L || key == XK_Alt_R)
                io.AddKeyEvent(ImGuiMod_Alt, true);
            if (key == XK_Super_L || key == XK_Super_R)
                io.AddKeyEvent(ImGuiMod_Super, true);
            
            // Add character input for text fields (only for printable characters)
            // The engine converts lowercase to uppercase, so we need to handle case
            if (key >= 'A' && key <= 'Z')
            {
                // Check if shift is held - if so, use uppercase, otherwise lowercase
                bool shift_held = io.KeyShift;
                bool caps_on = isCapsLockOn();
                bool uppercase = (shift_held && !caps_on) || (!shift_held && caps_on);
                io.AddInputCharacter(uppercase ? key : (key - 'A' + 'a'));
            }
            else if (key >= '0' && key <= '9')
            {
                // Handle shift+number for symbols
                if (io.KeyShift)
                {
                    static const char shifted[] = ")!@#$%^&*(";
                    io.AddInputCharacter(shifted[key - '0']);
                }
                else
                {
                    io.AddInputCharacter(key);
                }
            }
            // Numpad numbers (NumLock on)
            else if (key >= XK_KP_0 && key <= XK_KP_9)
            {
                io.AddInputCharacter('0' + (key - XK_KP_0));
            }
            else if (key == XK_KP_Decimal)
            {
                io.AddInputCharacter('.');
            }
            else if (key == XK_KP_Divide)
            {
                io.AddInputCharacter('/');
            }
            else if (key == XK_KP_Multiply)
            {
                io.AddInputCharacter('*');
            }
            else if (key == XK_KP_Subtract)
            {
                io.AddInputCharacter('-');
            }
            else if (key == XK_KP_Add)
            {
                io.AddInputCharacter('+');
            }
            else if (key == XK_space || key == XK_KP_Space)
            {
                io.AddInputCharacter(' ');
            }
            else if (key == XK_Return || key == XK_KP_Enter)
            {
                io.AddInputCharacter('\n');
            }
            // Handle special punctuation keys on Linux/macOS
            else if (key == XK_semicolon)
            {
                io.AddInputCharacter(io.KeyShift ? ':' : ';');
            }
            else if (key == XK_equal)
            {
                io.AddInputCharacter(io.KeyShift ? '+' : '=');
            }
            else if (key == XK_comma)
            {
                io.AddInputCharacter(io.KeyShift ? '<' : ',');
            }
            else if (key == XK_minus)
            {
                io.AddInputCharacter(io.KeyShift ? '_' : '-');
            }
            else if (key == XK_period)
            {
                io.AddInputCharacter(io.KeyShift ? '>' : '.');
            }
            else if (key == XK_slash)
            {
                io.AddInputCharacter(io.KeyShift ? '?' : '/');
            }
            else if (key == XK_grave)
            {
                io.AddInputCharacter(io.KeyShift ? '~' : '`');
            }
            else if (key == XK_bracketleft)
            {
                io.AddInputCharacter(io.KeyShift ? '{' : '[');
            }
            else if (key == XK_backslash)
            {
                io.AddInputCharacter(io.KeyShift ? '|' : '\\');
            }
            else if (key == XK_bracketright)
            {
                io.AddInputCharacter(io.KeyShift ? '}' : ']');
            }
            else if (key == XK_apostrophe)
            {
                io.AddInputCharacter(io.KeyShift ? '"' : '\'');
            }
            #endif
        }
    }

    void onKeyUp(int key)
    {
        if(imGuiContext)
        {
            ImGuiIO& io = ImGui::GetIO();
            
            // Remove from tracked keys
            keysDown.erase(key);
            
            // Map native key to ImGuiKey
            ImGuiKey imgui_key = MapNativeKeyToImGuiKey(key);
            if (imgui_key != ImGuiKey_None)
            {
                // Navigation keys already had their key-up sent immediately in onKeyDown
                // Skip sending another key-up to avoid potential issues
                bool isNavKey = (imgui_key == ImGuiKey_UpArrow || 
                                 imgui_key == ImGuiKey_DownArrow ||
                                 imgui_key == ImGuiKey_LeftArrow || 
                                 imgui_key == ImGuiKey_RightArrow ||
                                 imgui_key == ImGuiKey_Home ||
                                 imgui_key == ImGuiKey_End ||
                                 imgui_key == ImGuiKey_PageUp ||
                                 imgui_key == ImGuiKey_PageDown);
                if (!isNavKey)
                {
                    io.AddKeyEvent(imgui_key, false);
                }
            }
            
            // Handle modifier keys
            #if defined(_WIN32)
            if (key == VK_CONTROL || key == VK_LCONTROL || key == VK_RCONTROL)
                io.AddKeyEvent(ImGuiMod_Ctrl, false);
            if (key == VK_SHIFT || key == VK_LSHIFT || key == VK_RSHIFT)
                io.AddKeyEvent(ImGuiMod_Shift, false);
            if (key == VK_MENU || key == VK_LMENU || key == VK_RMENU)
                io.AddKeyEvent(ImGuiMod_Alt, false);
            if (key == VK_LWIN || key == VK_RWIN)
                io.AddKeyEvent(ImGuiMod_Super, false);
            #elif (defined(__linux__) || defined(__APPLE__)) && !defined (ANDROID)
            if (key == XK_Control_L || key == XK_Control_R)
                io.AddKeyEvent(ImGuiMod_Ctrl, false);
            if (key == XK_Shift_L || key == XK_Shift_R)
                io.AddKeyEvent(ImGuiMod_Shift, false);
            if (key == XK_Alt_L || key == XK_Alt_R)
                io.AddKeyEvent(ImGuiMod_Alt, false);
            if (key == XK_Super_L || key == XK_Super_R)
                io.AddKeyEvent(ImGuiMod_Super, false);
            #endif
        }
    }

    const bool isCapsLockOn()
    {
        #if defined(__linux__) || defined(__APPLE__) || defined (ANDROID)
            mbm::DEVICE* device = mbm::DEVICE::getInstance();
            return device->ptrManager->keyCapsLockState;
        #elif defined (_WIN32)
            if ((GetKeyState(VK_CAPITAL) & 0x0001)!=0)
                return true;
            return false;
        #else
            return false;
        #endif
    }

    void onDoubleClick(float x, float y, int key)
    {
        // NOTE: Backend handles double-click detection automatically via io.MouseDoubleClickTime
    }

    void onKeyDownJoystick(int, int)
    {
    }

    void onKeyUpJoystick(int, int)
    {
    }

    void onMoveJoystick(int, float, float, float,float)
    {
    }

    void onInfoDeviceJoystick(int, int, const char *,const char *)
    {
    }
    void onPrepare()
    {
        if(imGuiContext && beginRenderWasCalled == false)
        {
            ImGuiIO& imGuIo = ImGui::GetIO();
            
            // Update delta time
            imGuIo.DeltaTime = delta <= 0.0f ? 1.0f/60.0f : delta;
            
        // Backend NewFrame calls
        // NOTE: ImGui_ImplWin32_NewFrame() handles time tracking, gamepad polling, and keyboard workarounds
        // It does NOT handle input events - we do that manually via onKeyDown/onTouch* callbacks
#if defined(_WIN32)
            ImGui_ImplWin32_NewFrame();
#endif

#if defined(USE_OPENGL_ES)
    #if defined (IMGUI_IMPL_OPENGL_ES2) || defined (IMGUI_IMPL_OPENGL_ES3)
            ImGui_ImplOpenGL3_NewFrame();
    #else
        #error "Not implemented for opengl if not using ImGui_ImplOpenGL_ES2 or ImGui_ImplOpenGL_ES3"
    #endif
#elif defined USE_DIRECTX9
            ImGui_ImplDX9_NewFrame();
#elif defined USE_DUMMY_BACK_END_ENGINE
            REMINDER_TODO
#else
            #error "Not implemented for ImGui NewFrame"
#endif

            // Start new ImGui frame
            ImGui::NewFrame();
            
            // Update scaling
            mbm::DEVICE* device = mbm::DEVICE::getInstance();
            sx = device->camera.scale2d.x;
            sy = device->camera.scale2d.y;
            
            beginRenderWasCalled = true;
        }
    }

    void onLoop(float d)
    {
        delta = d;
    }

    void onRender()
    {
        if(imGuiContext && beginRenderWasCalled)
        {
            beginRenderWasCalled = false;
            updateCursorMouse();
            ImGui::EndFrame();
            ImGui::Render();
            ImDrawData* draw_data = ImGui::GetDrawData();
#if defined(USE_OPENGL_ES)
    #if defined (IMGUI_IMPL_OPENGL_ES2) || defined (IMGUI_IMPL_OPENGL_ES3)
            ImGui_ImplOpenGL3_RenderDrawData(draw_data);
    #else
        #error "Not implemented for opengl if not using ImGui_ImplOpenGL_ES2 or ImGui_ImplOpenGL_ES3"
    #endif
#elif defined USE_DIRECTX9
            ImGui_ImplDX9_RenderDrawData(draw_data);
#elif defined USE_DUMMY_BACK_END_ENGINE
            REMINDER_TODO
#else
            #error "Not implemented for ImGui RenderDrawData"
#endif
        }
    }
    void onDestroy()
    {

#if defined(USE_OPENGL_ES)
    #if defined (IMGUI_IMPL_OPENGL_ES2) || defined (IMGUI_IMPL_OPENGL_ES3)
            ImGui_ImplOpenGL3_Shutdown();
    #else
        #error "Not implemented for opengl if not using ImGui_ImplOpenGL_ES2 or ImGui_ImplOpenGL_ES3"
    #endif
#elif defined USE_DIRECTX9
        ImGui_ImplDX9_Shutdown();
#elif defined USE_DUMMY_BACK_END_ENGINE
            REMINDER_TODO
#else
        #error "Not implemented for ImGui Shutdown"
#endif
        
        // Shutdown platform backend
        #if defined _WIN32
            ImGui_ImplWin32_Shutdown();
        #endif
        
        ImGui::DestroyContext();
        imGuiContext = nullptr;
    }

    void updateCursorMouse()
    {
        #if defined (_WIN32)
        if(GetFocus() == this->context)
        {
            const ImGuiIO& imGuIo                 = ImGui::GetIO();
            if(imGuIo.MouseDrawCursor)
            {
                ShowCursor(0);
            }
            else
            {
                const ImGuiMouseCursor imgui_cursor   = ImGui::GetMouseCursor();
                switch(imgui_cursor)
                {
                    case ImGuiMouseCursor_None:         ShowCursor(0);                                break;
                    case ImGuiMouseCursor_Arrow:        SetCursor(LoadCursor(nullptr, IDC_ARROW));    break;
                    case ImGuiMouseCursor_TextInput:    SetCursor(LoadCursor(nullptr, IDC_IBEAM));    break;     // When hovering over InputText, etc.
                    case ImGuiMouseCursor_ResizeAll:    SetCursor(LoadCursor(nullptr, IDC_SIZEALL));  break;     // (Unused by Dear ImGui functions)
                    case ImGuiMouseCursor_ResizeNS:     SetCursor(LoadCursor(nullptr, IDC_SIZENS));   break;     // When hovering over an horizontal border
                    case ImGuiMouseCursor_ResizeEW:     SetCursor(LoadCursor(nullptr, IDC_SIZEWE));   break;     // When hovering over a vertical border or a column
                    case ImGuiMouseCursor_ResizeNESW:   SetCursor(LoadCursor(nullptr, IDC_SIZENESW)); break;     // When hovering over the bottom-left corner of a window
                    case ImGuiMouseCursor_ResizeNWSE:   SetCursor(LoadCursor(nullptr, IDC_SIZENWSE)); break;     // When hovering over the bottom-right corner of a window
                    case ImGuiMouseCursor_Hand:         SetCursor(LoadCursor(nullptr, IDC_HAND));     break;     // (Unused by Dear ImGui functions. Use for e.g. hyperlinks)
                    case ImGuiMouseCursor_NotAllowed:   SetCursor(LoadCursor(nullptr, IDC_NO));       break;     // When hovering something with disallowed interaction. Usually a crossed circle.
                }
            }
        }
        #elif (defined(__linux__) || defined(__APPLE__)) && !defined (ANDROID)
        Window w = 0;
        int current_focus_state = 0;
        if(XGetInputFocus(context,&w,&current_focus_state) != RevertToNone && w != 0)
        {
            const ImGuiIO& imGuIo                 = ImGui::GetIO();
            if(imGuIo.MouseDrawCursor == false)
            {
                const ImGuiMouseCursor imgui_cursor   = ImGui::GetMouseCursor();
                switch(imgui_cursor)
                {
                    //https://tronche.com/gui/x/xlib/appendix/b/
                    //case ImGuiMouseCursor_None:         ShowCursor(0);                                break;
                    case ImGuiMouseCursor_Arrow:        XDefineCursor(context, w, XCreateFontCursor(context, XC_left_ptr));            break;
                    case ImGuiMouseCursor_TextInput:    XDefineCursor(context, w, XCreateFontCursor(context, XC_xterm));               break;     // When hovering over InputText, etc.
                    case ImGuiMouseCursor_ResizeAll:    XDefineCursor(context, w, XCreateFontCursor(context, XC_sizing));              break;     // (Unused by Dear ImGui functions)
                    case ImGuiMouseCursor_ResizeNS:     XDefineCursor(context, w, XCreateFontCursor(context, XC_sb_h_double_arrow));   break;     // When hovering over an horizontal border
                    case ImGuiMouseCursor_ResizeEW:     XDefineCursor(context, w, XCreateFontCursor(context, XC_sb_v_double_arrow));   break;     // When hovering over a vertical border or a column
                    case ImGuiMouseCursor_ResizeNESW:   XDefineCursor(context, w, XCreateFontCursor(context, XC_bottom_left_corner));  break;     // When hovering over the bottom-left corner of a window
                    case ImGuiMouseCursor_ResizeNWSE:   XDefineCursor(context, w, XCreateFontCursor(context, XC_bottom_right_corner)); break;     // When hovering over the bottom-right corner of a window
                    case ImGuiMouseCursor_Hand:         XDefineCursor(context, w, XCreateFontCursor(context, XC_hand1));               break;     // (Unused by Dear ImGui functions. Use for e.g. hyperlinks)
                    case ImGuiMouseCursor_NotAllowed:   XDefineCursor(context, w, XCreateFontCursor(context, XC_X_cursor));            break;     // When hovering something with disallowed interaction. Usually a crossed circle.
                }
            }
        }
        #endif
    }
};

IMGUI_LUA *getImGuiFromRawTable(lua_State *lua, const int rawi, const int indexTable)
{
    const int typeObj = lua_type(lua, indexTable);
    if (typeObj != LUA_TTABLE)
    {
        if(typeObj == LUA_TNONE)
            lua_log_error(lua, "expected: [plugin]. got [nil]");
        else
        {
            char message[255] = "";
            snprintf(message,sizeof(message),"expected: [plugin]. got [%s]",lua_typename(lua, typeObj));
            lua_log_error(lua, message);
        }
        return nullptr;
    }
    lua_rawgeti(lua, indexTable, rawi);
    void *p = lua_touserdata(lua, -1);
    if (p != nullptr) 
    {  /* value is a userdata? */
        if (lua_getmetatable(lua, -1))
        {  /* does it have a metatable? */
            lua_rawgeti(lua,-1, 1);
            const int L_USER_TYPE_PLUGIN  = lua_tointeger(lua,-1);
            lua_pop(lua, 3);
            if(L_USER_TYPE_PLUGIN == PLUGIN_IDENTIFIER)//Is it really a plugin defined by the engine ?
            {
                IMGUI_LUA **ud = static_cast<IMGUI_LUA **>(p);
                if(ud && *ud)
                    return *ud;
            }
        }
        else
        {
            lua_pop(lua, 2);
        }
    }
    else
    {
        lua_pop(lua, 1);
    }
    return nullptr;
}

void lua_push_rgba(lua_State * lua, const float p_col[4])
{
    lua_newtable(lua);
    lua_pushnumber(lua,p_col[0]);
    lua_setfield(lua, -2, "r");
    lua_pushnumber(lua,p_col[1]);
    lua_setfield(lua, -2, "g");
    lua_pushnumber(lua,p_col[2]);
    lua_setfield(lua, -2, "b");
    lua_pushnumber(lua,p_col[3]);
    lua_setfield(lua, -2, "a");
}

void lua_push_rgba(lua_State * lua, const ImVec4 & color)
{
    lua_newtable(lua);
    lua_pushnumber(lua,color.x);
    lua_setfield(lua, -2, "r");
    lua_pushnumber(lua,color.y);
    lua_setfield(lua, -2, "g");
    lua_pushnumber(lua,color.z);
    lua_setfield(lua, -2, "b");
    lua_pushnumber(lua,color.w);
    lua_setfield(lua, -2, "a");
}

void lua_push_ImVec4(lua_State *lua, const ImVec4 & in)
{
    lua_newtable(lua);
    lua_pushnumber(lua,in.x);
    lua_setfield(lua, -2, "x");
    lua_pushnumber(lua,in.y);
    lua_setfield(lua, -2, "y");
    lua_pushnumber(lua,in.z);
    lua_setfield(lua, -2, "z");
    lua_pushnumber(lua,in.w);
    lua_setfield(lua, -2, "w");

}


void lua_push_ImGuiStyle(lua_State *lua, const ImGuiStyle & in)
{
    lua_newtable(lua);
    lua_pushnumber(lua,in.FontSizeBase);
    lua_setfield(lua, -2, "FontSizeBase");
    lua_pushnumber(lua,in.FontScaleMain);
    lua_setfield(lua, -2, "FontScaleMain");
    lua_pushnumber(lua,in.FontScaleDpi);
    lua_setfield(lua, -2, "FontScaleDpi");
    lua_pushnumber(lua,in.Alpha);
    lua_setfield(lua, -2, "Alpha");
    lua_pushnumber(lua,in.DisabledAlpha);
    lua_setfield(lua, -2, "DisabledAlpha");
    lua_push_ImVec2(lua,in.WindowPadding);
    lua_setfield(lua, -2, "WindowPadding");
    lua_pushnumber(lua,in.WindowRounding);
    lua_setfield(lua, -2, "WindowRounding");
    lua_pushnumber(lua,in.WindowBorderSize);
    lua_setfield(lua, -2, "WindowBorderSize");
    lua_pushnumber(lua,in.WindowBorderHoverPadding);
    lua_setfield(lua, -2, "WindowBorderHoverPadding");
    lua_push_ImVec2(lua,in.WindowMinSize);
    lua_setfield(lua, -2, "WindowMinSize");
    lua_push_ImVec2(lua,in.WindowTitleAlign);
    lua_setfield(lua, -2, "WindowTitleAlign");
    lua_pushinteger(lua,in.WindowMenuButtonPosition);
    lua_setfield(lua, -2, "WindowMenuButtonPosition");
    lua_pushnumber(lua,in.ChildRounding);
    lua_setfield(lua, -2, "ChildRounding");
    lua_pushnumber(lua,in.ChildBorderSize);
    lua_setfield(lua, -2, "ChildBorderSize");
    lua_pushnumber(lua,in.PopupRounding);
    lua_setfield(lua, -2, "PopupRounding");
    lua_pushnumber(lua,in.PopupBorderSize);
    lua_setfield(lua, -2, "PopupBorderSize");
    lua_push_ImVec2(lua,in.FramePadding);
    lua_setfield(lua, -2, "FramePadding");
    lua_pushnumber(lua,in.FrameRounding);
    lua_setfield(lua, -2, "FrameRounding");
    lua_pushnumber(lua,in.FrameBorderSize);
    lua_setfield(lua, -2, "FrameBorderSize");
    lua_push_ImVec2(lua,in.ItemSpacing);
    lua_setfield(lua, -2, "ItemSpacing");
    lua_push_ImVec2(lua,in.ItemInnerSpacing);
    lua_setfield(lua, -2, "ItemInnerSpacing");
    lua_push_ImVec2(lua,in.CellPadding);
    lua_setfield(lua, -2, "CellPadding");
    lua_push_ImVec2(lua,in.TouchExtraPadding);
    lua_setfield(lua, -2, "TouchExtraPadding");
    lua_pushnumber(lua,in.IndentSpacing);
    lua_setfield(lua, -2, "IndentSpacing");
    lua_pushnumber(lua,in.ColumnsMinSpacing);
    lua_setfield(lua, -2, "ColumnsMinSpacing");
    lua_pushnumber(lua,in.ScrollbarSize);
    lua_setfield(lua, -2, "ScrollbarSize");
    lua_pushnumber(lua,in.ScrollbarRounding);
    lua_setfield(lua, -2, "ScrollbarRounding");
    lua_pushnumber(lua,in.ScrollbarPadding);
    lua_setfield(lua, -2, "ScrollbarPadding");
    lua_pushnumber(lua,in.GrabMinSize);
    lua_setfield(lua, -2, "GrabMinSize");
    lua_pushnumber(lua,in.GrabRounding);
    lua_setfield(lua, -2, "GrabRounding");
    lua_pushnumber(lua,in.LogSliderDeadzone);
    lua_setfield(lua, -2, "LogSliderDeadzone");
    lua_pushnumber(lua,in.ImageRounding);
    lua_setfield(lua, -2, "ImageRounding");
    lua_pushnumber(lua,in.ImageBorderSize);
    lua_setfield(lua, -2, "ImageBorderSize");
    lua_pushnumber(lua,in.TabRounding);
    lua_setfield(lua, -2, "TabRounding");
    lua_pushnumber(lua,in.TabBorderSize);
    lua_setfield(lua, -2, "TabBorderSize");
    lua_pushnumber(lua,in.TabMinWidthBase);
    lua_setfield(lua, -2, "TabMinWidthBase");
    lua_pushnumber(lua,in.TabMinWidthShrink);
    lua_setfield(lua, -2, "TabMinWidthShrink");
    lua_pushnumber(lua,in.TabCloseButtonMinWidthSelected);
    lua_setfield(lua, -2, "TabCloseButtonMinWidthSelected");
    lua_pushnumber(lua,in.TabCloseButtonMinWidthUnselected);
    lua_setfield(lua, -2, "TabCloseButtonMinWidthUnselected");
    lua_pushnumber(lua,in.TabBarBorderSize);
    lua_setfield(lua, -2, "TabBarBorderSize");
    lua_pushnumber(lua,in.TabBarOverlineSize);
    lua_setfield(lua, -2, "TabBarOverlineSize");
    lua_pushnumber(lua,in.TableAngledHeadersAngle);
    lua_setfield(lua, -2, "TableAngledHeadersAngle");
    lua_push_ImVec2(lua,in.TableAngledHeadersTextAlign);
    lua_setfield(lua, -2, "TableAngledHeadersTextAlign");
    lua_pushinteger(lua,in.TreeLinesFlags);
    lua_setfield(lua, -2, "TreeLinesFlags");
    lua_pushnumber(lua,in.TreeLinesSize);
    lua_setfield(lua, -2, "TreeLinesSize");
    lua_pushnumber(lua,in.TreeLinesRounding);
    lua_setfield(lua, -2, "TreeLinesRounding");
    lua_pushnumber(lua,in.DragDropTargetRounding);
    lua_setfield(lua, -2, "DragDropTargetRounding");
    lua_pushnumber(lua,in.DragDropTargetBorderSize);
    lua_setfield(lua, -2, "DragDropTargetBorderSize");
    lua_pushnumber(lua,in.DragDropTargetPadding);
    lua_setfield(lua, -2, "DragDropTargetPadding");
    lua_pushnumber(lua,in.ColorMarkerSize);
    lua_setfield(lua, -2, "ColorMarkerSize");
    lua_pushinteger(lua,in.ColorButtonPosition);
    lua_setfield(lua, -2, "ColorButtonPosition");
    lua_push_ImVec2(lua,in.ButtonTextAlign);
    lua_setfield(lua, -2, "ButtonTextAlign");
    lua_push_ImVec2(lua,in.SelectableTextAlign);
    lua_setfield(lua, -2, "SelectableTextAlign");
    lua_pushnumber(lua,in.SeparatorTextBorderSize);
    lua_setfield(lua, -2, "SeparatorTextBorderSize");
    lua_push_ImVec2(lua,in.SeparatorTextAlign);
    lua_setfield(lua, -2, "SeparatorTextAlign");
    lua_push_ImVec2(lua,in.SeparatorTextPadding);
    lua_setfield(lua, -2, "SeparatorTextPadding");
    lua_push_ImVec2(lua,in.DisplayWindowPadding);
    lua_setfield(lua, -2, "DisplayWindowPadding");
    lua_push_ImVec2(lua,in.DisplaySafeAreaPadding);
    lua_setfield(lua, -2, "DisplaySafeAreaPadding");
    lua_pushnumber(lua,in.MouseCursorScale);
    lua_setfield(lua, -2, "MouseCursorScale");
    lua_pushboolean(lua,in.AntiAliasedLines);
    lua_setfield(lua, -2, "AntiAliasedLines");
    lua_pushboolean(lua,in.AntiAliasedLinesUseTex);
    lua_setfield(lua, -2, "AntiAliasedLinesUseTex");
    lua_pushboolean(lua,in.AntiAliasedFill);
    lua_setfield(lua, -2, "AntiAliasedFill");
    lua_pushnumber(lua,in.CurveTessellationTol);
    lua_setfield(lua, -2, "CurveTessellationTol");
    lua_pushnumber(lua,in.CircleTessellationMaxError);
    lua_setfield(lua, -2, "CircleTessellationMaxError");
    lua_pushnumber(lua,in.HoverStationaryDelay);
    lua_setfield(lua, -2, "HoverStationaryDelay");
    lua_pushnumber(lua,in.HoverDelayShort);
    lua_setfield(lua, -2, "HoverDelayShort");
    lua_pushnumber(lua,in.HoverDelayNormal);
    lua_setfield(lua, -2, "HoverDelayNormal");
    lua_pushinteger(lua,in.HoverFlagsForTooltipMouse);
    lua_setfield(lua, -2, "HoverFlagsForTooltipMouse");
    lua_pushinteger(lua,in.HoverFlagsForTooltipNav);
    lua_setfield(lua, -2, "HoverFlagsForTooltipNav");
    push_RGBA_arrayFromTable(lua,in.Colors,sizeof(in.Colors) / sizeof(in.Colors[0]));
    lua_setfield(lua, -2, "Colors");

}

void push_RGBA_arrayFromTable(lua_State *lua, const ImVec4 * lsArrayIn, const unsigned int sizeBuffer)
{
    lua_newtable(lua);
    for(unsigned int i=0; i < sizeBuffer; ++i )
    {
        lua_push_rgba(lua,lsArrayIn[i]);
        lua_rawseti(lua, -2, i+1);
    }
}

void lua_push_ImVec2(lua_State *lua, const ImVec2 & in)
{
    lua_newtable(lua);
    lua_pushnumber(lua,in.x);
    lua_setfield(lua, -2, "x");
    lua_pushnumber(lua,in.y);
    lua_setfield(lua, -2, "y");

}


ImVec4 lua_pop_ImVec4(lua_State *lua,const int index)
{
    ImVec4 ImVec4_out;
    lua_check_is_table(lua, index, "ImVec4");
    lua_getfield(lua, index, "x");
    ImVec4_out.x  = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "y");
    ImVec4_out.y  = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "z");
    ImVec4_out.z  = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "w");
    ImVec4_out.w  = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);

    return ImVec4_out;
}

void lua_get_rgba_FromTable(lua_State * lua, int index, float p_col[4])
{
    lua_check_is_table(lua, index, "tRgb");
    lua_getfield(lua, index, "r");
    p_col[0] = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "g");
    p_col[1] = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "b");
    p_col[2] = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "a");
    if (lua_type(lua,-1) == LUA_TNUMBER)
        p_col[3] = lua_tonumber(lua,-1);
    lua_pop(lua, 1);
}

ImVec4 lua_get_rgba_to_ImVec4_fromTable(lua_State * lua,const int index)
{
    ImVec4 color(0,0,0,1);
    lua_check_is_table(lua, index, "tRgb");
    lua_getfield(lua, index, "r");
    if (lua_type(lua,-1) == LUA_TNUMBER)
        color.x = lua_tonumber(lua,-1);
    else
    {
        lua_pop(lua, 1);
        lua_getfield(lua, index, "x");
        color.x = luaL_checknumber(lua,-1);
        lua_pop(lua, 1);
    }

    lua_getfield(lua, index, "g");
    if (lua_type(lua,-1) == LUA_TNUMBER)
        color.y = lua_tonumber(lua,-1);
    else
    {
        lua_pop(lua, 1);
        lua_getfield(lua, index, "y");
        color.y = luaL_checknumber(lua,-1);
        lua_pop(lua, 1);
    }

    lua_getfield(lua, index, "b");
    if (lua_type(lua,-1) == LUA_TNUMBER)
        color.z = lua_tonumber(lua,-1);
    else
    {
        lua_pop(lua, 1);
        lua_getfield(lua, index, "z");
        color.z = luaL_checknumber(lua,-1);
        lua_pop(lua, 1);
    }

    lua_getfield(lua, index, "a");
    if (lua_type(lua,-1) == LUA_TNUMBER)
        color.w = lua_tonumber(lua,-1);
    else
    {
        lua_pop(lua, 1);
        lua_getfield(lua, index, "w");
        if (lua_type(lua,-1) == LUA_TNUMBER)
            color.w = lua_tonumber(lua,-1);
        lua_pop(lua, 1);
    }
    return color;
}

ImVec2 lua_pop_ImVec2(lua_State *lua,const int index)
{
    ImVec2 ImVec2_out;
    lua_check_is_table(lua, index, "ImVec2");
    lua_getfield(lua, index, "x");
    ImVec2_out.x  = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "y");
    ImVec2_out.y  = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);

    return ImVec2_out;
}


ImVec4 * lua_pop_ImVec4_pointer(lua_State *lua, const int index, ImVec4 * in_out_ImVec4)
{
    if (in_out_ImVec4 == nullptr)
    {
        lua_log_error(lua,"ImVec4 can not be null");
        return nullptr;
    }
    lua_check_is_table(lua, index, "ImVec4");
    in_out_ImVec4->x  = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImVec4->x),"x"));
    in_out_ImVec4->y  = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImVec4->y),"y"));
    in_out_ImVec4->z  = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImVec4->z),"z"));
    in_out_ImVec4->w  = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImVec4->w),"w"));

    return in_out_ImVec4;
}

ImVec2 * lua_pop_ImVec2_pointer(lua_State *lua, const int index, ImVec2 * in_out_ImVec2)
{
    if (in_out_ImVec2 == nullptr)
    {
        lua_log_error(lua,"ImVec2 can not be null");
        return nullptr;
    }
    lua_check_is_table(lua, index, "ImVec2");
    in_out_ImVec2->x  = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImVec2->x),"x"));
    in_out_ImVec2->y  = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImVec2->y),"y"));

    return in_out_ImVec2;
}


ImGuiStyle * lua_pop_ImGuiStyle_pointer(lua_State *lua, const int index, ImGuiStyle * in_out_ImGuiStyle)
{
    if (in_out_ImGuiStyle == nullptr)
    {
        lua_log_error(lua,"ImGuiStyle can not be null");
        return nullptr;
    }
    lua_check_is_table(lua, index, "ImGuiStyle");
    in_out_ImGuiStyle->FontSizeBase              = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->FontSizeBase),"FontSizeBase"));
    in_out_ImGuiStyle->FontScaleMain             = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->FontScaleMain),"FontScaleMain"));
    in_out_ImGuiStyle->FontScaleDpi              = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->FontScaleDpi),"FontScaleDpi"));
    in_out_ImGuiStyle->Alpha                     = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->Alpha),"Alpha"));
    in_out_ImGuiStyle->DisabledAlpha             = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->DisabledAlpha),"DisabledAlpha"));
    lua_getfield(lua, index, "WindowPadding");
    in_out_ImGuiStyle->WindowPadding             = lua_pop_ImVec2(lua,index);
    in_out_ImGuiStyle->WindowRounding            = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->WindowRounding),"WindowRounding"));
    in_out_ImGuiStyle->WindowBorderSize          = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->WindowBorderSize),"WindowBorderSize"));
    in_out_ImGuiStyle->WindowBorderHoverPadding  = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->WindowBorderHoverPadding),"WindowBorderHoverPadding"));
    lua_getfield(lua, index, "WindowMinSize");
    in_out_ImGuiStyle->WindowMinSize             = lua_pop_ImVec2(lua,index);
    lua_getfield(lua, index, "WindowTitleAlign");
    in_out_ImGuiStyle->WindowTitleAlign          = lua_pop_ImVec2(lua,index);
    lua_getfield(lua, index, "WindowMenuButtonPosition");
    in_out_ImGuiStyle->WindowMenuButtonPosition  = (ImGuiDir)luaL_checkinteger(lua,index);
    in_out_ImGuiStyle->ChildRounding             = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->ChildRounding),"ChildRounding"));
    in_out_ImGuiStyle->ChildBorderSize           = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->ChildBorderSize),"ChildBorderSize"));
    in_out_ImGuiStyle->PopupRounding             = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->PopupRounding),"PopupRounding"));
    in_out_ImGuiStyle->PopupBorderSize           = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->PopupBorderSize),"PopupBorderSize"));
    lua_getfield(lua, index, "FramePadding");
    in_out_ImGuiStyle->FramePadding              = lua_pop_ImVec2(lua,index);
    in_out_ImGuiStyle->FrameRounding             = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->FrameRounding),"FrameRounding"));
    in_out_ImGuiStyle->FrameBorderSize           = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->FrameBorderSize),"FrameBorderSize"));
    lua_getfield(lua, index, "ItemSpacing");
    in_out_ImGuiStyle->ItemSpacing               = lua_pop_ImVec2(lua,index);
    lua_getfield(lua, index, "ItemInnerSpacing");
    in_out_ImGuiStyle->ItemInnerSpacing          = lua_pop_ImVec2(lua,index);
    lua_getfield(lua, index, "CellPadding");
    if (lua_istable(lua, -1))
        lua_pop_ImVec2_pointer(lua, lua_gettop(lua), &in_out_ImGuiStyle->CellPadding);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "TouchExtraPadding");
    in_out_ImGuiStyle->TouchExtraPadding         = lua_pop_ImVec2(lua,index);
    in_out_ImGuiStyle->IndentSpacing             = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->IndentSpacing),"IndentSpacing"));
    in_out_ImGuiStyle->ColumnsMinSpacing         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->ColumnsMinSpacing),"ColumnsMinSpacing"));
    in_out_ImGuiStyle->ScrollbarSize             = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->ScrollbarSize),"ScrollbarSize"));
    in_out_ImGuiStyle->ScrollbarRounding         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->ScrollbarRounding),"ScrollbarRounding"));
    in_out_ImGuiStyle->ScrollbarPadding          = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->ScrollbarPadding),"ScrollbarPadding"));
    in_out_ImGuiStyle->GrabMinSize               = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->GrabMinSize),"GrabMinSize"));
    in_out_ImGuiStyle->GrabRounding              = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->GrabRounding),"GrabRounding"));
    in_out_ImGuiStyle->LogSliderDeadzone         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->LogSliderDeadzone),"LogSliderDeadzone"));
    in_out_ImGuiStyle->ImageRounding             = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->ImageRounding),"ImageRounding"));
    in_out_ImGuiStyle->ImageBorderSize           = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->ImageBorderSize),"ImageBorderSize"));
    in_out_ImGuiStyle->TabRounding               = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->TabRounding),"TabRounding"));
    in_out_ImGuiStyle->TabBorderSize             = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->TabBorderSize),"TabBorderSize"));
    in_out_ImGuiStyle->TabMinWidthBase           = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->TabMinWidthBase),"TabMinWidthBase"));
    in_out_ImGuiStyle->TabMinWidthShrink         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->TabMinWidthShrink),"TabMinWidthShrink"));
    in_out_ImGuiStyle->TabCloseButtonMinWidthSelected   = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->TabCloseButtonMinWidthSelected),"TabCloseButtonMinWidthSelected"));
    in_out_ImGuiStyle->TabCloseButtonMinWidthUnselected  = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->TabCloseButtonMinWidthUnselected),"TabCloseButtonMinWidthUnselected"));
    in_out_ImGuiStyle->TabBarBorderSize          = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->TabBarBorderSize),"TabBarBorderSize"));
    in_out_ImGuiStyle->TabBarOverlineSize        = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->TabBarOverlineSize),"TabBarOverlineSize"));
    in_out_ImGuiStyle->TableAngledHeadersAngle   = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->TableAngledHeadersAngle),"TableAngledHeadersAngle"));
    lua_getfield(lua, index, "TableAngledHeadersTextAlign");
    if (lua_istable(lua, -1))
        lua_pop_ImVec2_pointer(lua, lua_gettop(lua), &in_out_ImGuiStyle->TableAngledHeadersTextAlign);
    lua_pop(lua, 1);
    in_out_ImGuiStyle->TreeLinesFlags            = (ImGuiTreeNodeFlags)static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->TreeLinesFlags),"TreeLinesFlags"));
    in_out_ImGuiStyle->TreeLinesSize             = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->TreeLinesSize),"TreeLinesSize"));
    in_out_ImGuiStyle->TreeLinesRounding        = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->TreeLinesRounding),"TreeLinesRounding"));
    in_out_ImGuiStyle->DragDropTargetRounding    = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->DragDropTargetRounding),"DragDropTargetRounding"));
    in_out_ImGuiStyle->DragDropTargetBorderSize  = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->DragDropTargetBorderSize),"DragDropTargetBorderSize"));
    in_out_ImGuiStyle->DragDropTargetPadding     = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->DragDropTargetPadding),"DragDropTargetPadding"));
    in_out_ImGuiStyle->ColorMarkerSize           = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->ColorMarkerSize),"ColorMarkerSize"));
    lua_getfield(lua, index, "ColorButtonPosition");
    in_out_ImGuiStyle->ColorButtonPosition       = (ImGuiDir)luaL_checkinteger(lua,index);
    lua_getfield(lua, index, "ButtonTextAlign");
    in_out_ImGuiStyle->ButtonTextAlign           = lua_pop_ImVec2(lua,index);
    lua_getfield(lua, index, "SelectableTextAlign");
    in_out_ImGuiStyle->SelectableTextAlign       = lua_pop_ImVec2(lua,index);
    in_out_ImGuiStyle->SeparatorTextBorderSize   = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->SeparatorTextBorderSize),"SeparatorTextBorderSize"));
    lua_getfield(lua, index, "SeparatorTextAlign");
    if (lua_istable(lua, -1))
        lua_pop_ImVec2_pointer(lua, lua_gettop(lua), &in_out_ImGuiStyle->SeparatorTextAlign);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "SeparatorTextPadding");
    if (lua_istable(lua, -1))
        lua_pop_ImVec2_pointer(lua, lua_gettop(lua), &in_out_ImGuiStyle->SeparatorTextPadding);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "DisplayWindowPadding");
    in_out_ImGuiStyle->DisplayWindowPadding      = lua_pop_ImVec2(lua,index);
    lua_getfield(lua, index, "DisplaySafeAreaPadding");
    in_out_ImGuiStyle->DisplaySafeAreaPadding    = lua_pop_ImVec2(lua,index);
    in_out_ImGuiStyle->MouseCursorScale          = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->MouseCursorScale),"MouseCursorScale"));
    in_out_ImGuiStyle->AntiAliasedLines          = static_cast<bool>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->AntiAliasedLines),"AntiAliasedLines"));
    in_out_ImGuiStyle->AntiAliasedLinesUseTex    = static_cast<bool>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->AntiAliasedLinesUseTex),"AntiAliasedLinesUseTex"));
    in_out_ImGuiStyle->AntiAliasedFill           = static_cast<bool>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->AntiAliasedFill),"AntiAliasedFill"));
    in_out_ImGuiStyle->CurveTessellationTol     = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->CurveTessellationTol),"CurveTessellationTol"));
    in_out_ImGuiStyle->CircleTessellationMaxError= static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->CircleTessellationMaxError),"CircleTessellationMaxError"));
    in_out_ImGuiStyle->HoverStationaryDelay      = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->HoverStationaryDelay),"HoverStationaryDelay"));
    in_out_ImGuiStyle->HoverDelayShort          = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->HoverDelayShort),"HoverDelayShort"));
    in_out_ImGuiStyle->HoverDelayNormal         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->HoverDelayNormal),"HoverDelayNormal"));
    in_out_ImGuiStyle->HoverFlagsForTooltipMouse = (ImGuiHoveredFlags)static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->HoverFlagsForTooltipMouse),"HoverFlagsForTooltipMouse"));
    in_out_ImGuiStyle->HoverFlagsForTooltipNav   = (ImGuiHoveredFlags)static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->HoverFlagsForTooltipNav),"HoverFlagsForTooltipNav"));
    lua_getfield(lua, index, "Colors");
    for(int i=0; i < ImGuiCol_COUNT; i++)
    {
        lua_rawgeti(lua,index+1,i+1);
        in_out_ImGuiStyle->Colors[i]    = lua_pop_ImVec4(lua,index+2);
    }
    return in_out_ImGuiStyle;
}


void lua_push_ImVec4_pointer(lua_State *lua, const ImVec4 * p_in_ImVec4)
{
    if (p_in_ImVec4 == nullptr)
    {
        lua_log_error(lua,"ImVec4 can not be null");
    }
    else
    {
        lua_newtable(lua);
        lua_pushnumber(lua,p_in_ImVec4->x);
        lua_setfield(lua, -2, "x");
        lua_pushnumber(lua,p_in_ImVec4->y);
        lua_setfield(lua, -2, "y");
        lua_pushnumber(lua,p_in_ImVec4->z);
        lua_setfield(lua, -2, "z");
        lua_pushnumber(lua,p_in_ImVec4->w);
        lua_setfield(lua, -2, "w");

    }
}

void lua_push_ImVec2_pointer(lua_State *lua, const ImVec2 * p_in_ImVec2)
{
    if (p_in_ImVec2 == nullptr)
    {
        lua_log_error(lua,"ImVec2 can not be null");
    }
    else
    {
        lua_newtable(lua);
        lua_pushnumber(lua,p_in_ImVec2->x);
        lua_setfield(lua, -2, "x");
        lua_pushnumber(lua,p_in_ImVec2->y);
        lua_setfield(lua, -2, "y");

    }
}


ImDrawIdx * lua_pushinteger_pointer(lua_State *lua, const int index, ImDrawIdx * in_out_ImDrawIdx)
{
    if (in_out_ImDrawIdx == nullptr)
    {
        lua_log_error(lua,"ImDrawIdx can not be null");
        return nullptr;
    }
    lua_check_is_table(lua, index, "ImDrawIdx");

    return in_out_ImDrawIdx;
}


void get_int_arrayFromTable(lua_State *lua, const int index, int *lsArrayOut, const unsigned int sizeBuffer,const char* table_name)
{
    const size_t rawlen = lua_rawlen(lua,index);
    if(sizeBuffer == rawlen)
    {
        for (size_t i = 1; i <= sizeBuffer; ++i)
        {
            lua_rawgeti(lua,index, i);
            lsArrayOut[i-1]  = lua_tointeger(lua,-1);
            lua_pop(lua,1);
        }
    }
    else
    {
        char message[255] = "";
        snprintf(message,sizeof(message) - 1, "Error table has different size as expected:\nexpected[%d] table[%u]\n[%s:%d]\n ",sizeBuffer,(unsigned int)rawlen,__FILE__,__LINE__);
        lua_log_error(lua,message);
    }
}

void get_float_arrayFromTable(lua_State *lua, const int index, float *lsArrayOut, const unsigned int sizeBuffer,const char* table_name)
{
    lua_check_is_table(lua,index,table_name);
    const size_t rawlen = lua_rawlen(lua,index);
    if(sizeBuffer == rawlen)
    {
        for (size_t i = 1; i <= sizeBuffer; ++i)
        {
            lua_rawgeti(lua,index, i);
            lsArrayOut[i-1]  = lua_tonumber(lua,-1);
            lua_pop(lua,1);
        }
    }
    else
    {
        char message[255] = "";
        snprintf(message,sizeof(message) - 1, "Error table has different size as expected:\nexpected[%d] table[%u]\n[%s:%d]\n ",sizeBuffer,(unsigned int)rawlen,__FILE__,__LINE__);
        lua_log_error(lua,message);
    }
}

std::vector<std::string> get_string_arrayFromTable(lua_State *lua, const int index, const char* table_name)
{
    std::vector<std::string> lsArrayOut;
    lua_check_is_table(lua,index,table_name);
    const size_t sizeBuffer = lua_rawlen(lua,index);
    if(sizeBuffer > 0)
    {
        lsArrayOut.resize(sizeBuffer);
        lua_getglobal(lua, "tostring");
        for (size_t i = 1; i <= sizeBuffer; ++i)
        {
            size_t l;
            lua_pushvalue(lua, -1);
            lua_rawgeti(lua,index,i);
            lua_call(lua, 1, 1);
            const char * s = lua_tolstring(lua, -1, &l);
            if (s == nullptr)
                lua_log_error(lua, "'tostring' must return a string to 'print'");
            else
            {
                lua_pop(lua, 1);
                lsArrayOut[i-1] = s;
            }
        }
        lua_pop(lua, 1);
    }
    return lsArrayOut;
}

void get_ImVec2_arrayFromTable(lua_State *lua, const int index, std::vector<ImVec2> & lsArrayOut,const char* table_name)
{
    const int top = lua_gettop(lua);
    lua_check_is_table(lua,index,table_name);
    const size_t sizeBuffer = lua_rawlen(lua,index);
    if(sizeBuffer > 0)
    {
        lsArrayOut.resize(sizeBuffer);
        for (size_t i = 1; i <= sizeBuffer; ++i)
        {
            lua_rawgeti(lua,index,i);
            lsArrayOut[i -1].x  = static_cast<float>(get_number_from_field(lua,top + 1,static_cast<lua_Number>(0),"x"));
            lsArrayOut[i -1].y  = static_cast<float>(get_number_from_field(lua,top + 1,static_cast<lua_Number>(0),"y"));
            lua_pop(lua, 1);
        }
    }
}

void push_float_arrayFromTable(lua_State *lua, const float * lsArrayIn, const unsigned int sizeBuffer)
{
    lua_newtable(lua);
    for(unsigned int i=0; i < sizeBuffer; ++i )
    {
        lua_pushnumber(lua,lsArrayIn[i]);
        lua_rawseti(lua, -2, i+1);
    }
}

void push_int_arrayFromTable(lua_State *lua, const int * lsArrayIn, const unsigned int sizeBuffer)
{
    lua_newtable(lua);
    for(unsigned int i=0; i < sizeBuffer; ++i )
    {
        lua_pushinteger(lua,lsArrayIn[i]);
        lua_rawseti(lua, -2, i+1);
    }
}

int onDestroyimguiLua(lua_State *lua)
{
    IMGUI_LUA *       that      = getImGuiFromRawTable(lua,1,1);
#if _DEBUG
    static int v                = 1;
    printf("destroying plugin IMGUI_LUA %d \n", v++);
#endif
    delete that;
    return 0;
}

int onGetStyleImGuiLua(lua_State *lua)
{
    //  Access the Style structure (colors, sizes). Always use PushStyleColor(), PushStyleVar() to modify style mid-frame.
    const int top                    = lua_gettop(lua);
    const ImGuiStyle ret_ImGuiStyle  = ImGui::GetStyle();
    if(top >= 1)
    {
        for(int i=1; i <= top; ++i)
        {
            const char * sNext = luaL_checkstring(lua,i);
            if(strcmp(sNext,"FontSizeBase")                  == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.FontSizeBase);
            }
            else if(strcmp(sNext,"FontScaleMain")            == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.FontScaleMain);
            }
            else if(strcmp(sNext,"FontScaleDpi")             == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.FontScaleDpi);
            }
            else if(strcmp(sNext,"Alpha")                        == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.Alpha);
            }
            else if(strcmp(sNext,"DisabledAlpha")            == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.DisabledAlpha);
            }
            else if(strcmp(sNext,"WindowPadding")            == 0)
            {
                lua_push_ImVec2(lua,ret_ImGuiStyle.WindowPadding);
            }
            else if(strcmp(sNext,"WindowRounding")           == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.WindowRounding);
            }
            else if(strcmp(sNext,"WindowBorderSize")         == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.WindowBorderSize);
            }
            else if(strcmp(sNext,"WindowBorderHoverPadding")  == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.WindowBorderHoverPadding);
            }
            else if(strcmp(sNext,"WindowMinSize")            == 0)
            {
                lua_push_ImVec2(lua,ret_ImGuiStyle.WindowMinSize);
            }
            else if(strcmp(sNext,"WindowTitleAlign")         == 0)
            {
                lua_push_ImVec2(lua,ret_ImGuiStyle.WindowTitleAlign);
            }
            else if(strcmp(sNext,"WindowMenuButtonPosition") == 0)
            {
                lua_pushinteger(lua,ret_ImGuiStyle.WindowMenuButtonPosition);
            }
            else if(strcmp(sNext,"ChildRounding")            == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.ChildRounding);
            }
            else if(strcmp(sNext,"ChildBorderSize")          == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.ChildBorderSize);
            }
            else if(strcmp(sNext,"PopupRounding")            == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.PopupRounding);
            }
            else if(strcmp(sNext,"PopupBorderSize")          == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.PopupBorderSize);
            }
            else if(strcmp(sNext,"FramePadding")             == 0)
            {
                lua_push_ImVec2(lua,ret_ImGuiStyle.FramePadding);
            }
            else if(strcmp(sNext,"FrameRounding")            == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.FrameRounding);
            }
            else if(strcmp(sNext,"FrameBorderSize")          == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.FrameBorderSize);
            }
            else if(strcmp(sNext,"ItemSpacing")              == 0)
            {
                lua_push_ImVec2(lua,ret_ImGuiStyle.ItemSpacing);
            }
            else if(strcmp(sNext,"ItemInnerSpacing")         == 0)
            {
                lua_push_ImVec2(lua,ret_ImGuiStyle.ItemInnerSpacing);
            }
            else if(strcmp(sNext,"CellPadding")               == 0)
            {
                lua_push_ImVec2(lua,ret_ImGuiStyle.CellPadding);
            }
            else if(strcmp(sNext,"TouchExtraPadding")        == 0)
            {
                lua_push_ImVec2(lua,ret_ImGuiStyle.TouchExtraPadding);
            }
            else if(strcmp(sNext,"IndentSpacing")            == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.IndentSpacing);
            }
            else if(strcmp(sNext,"ColumnsMinSpacing")        == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.ColumnsMinSpacing);
            }
            else if(strcmp(sNext,"ScrollbarSize")            == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.ScrollbarSize);
            }
            else if(strcmp(sNext,"ScrollbarRounding")        == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.ScrollbarRounding);
            }
            else if(strcmp(sNext,"ScrollbarPadding")         == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.ScrollbarPadding);
            }
            else if(strcmp(sNext,"GrabMinSize")              == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.GrabMinSize);
            }
            else if(strcmp(sNext,"GrabRounding")             == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.GrabRounding);
            }
            else if(strcmp(sNext,"LogSliderDeadzone")        == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.LogSliderDeadzone);
            }
            else if(strcmp(sNext,"ImageRounding")            == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.ImageRounding);
            }
            else if(strcmp(sNext,"ImageBorderSize")          == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.ImageBorderSize);
            }
            else if(strcmp(sNext,"TabRounding")              == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.TabRounding);
            }
            else if(strcmp(sNext,"TabBorderSize")            == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.TabBorderSize);
            }
            else if(strcmp(sNext,"TabMinWidthBase")          == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.TabMinWidthBase);
            }
            else if(strcmp(sNext,"TabMinWidthShrink")        == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.TabMinWidthShrink);
            }
            else if(strcmp(sNext,"TabCloseButtonMinWidthSelected") == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.TabCloseButtonMinWidthSelected);
            }
            else if(strcmp(sNext,"TabCloseButtonMinWidthUnselected") == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.TabCloseButtonMinWidthUnselected);
            }
            else if(strcmp(sNext,"TabBarBorderSize")         == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.TabBarBorderSize);
            }
            else if(strcmp(sNext,"TabBarOverlineSize")       == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.TabBarOverlineSize);
            }
            else if(strcmp(sNext,"TableAngledHeadersAngle")   == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.TableAngledHeadersAngle);
            }
            else if(strcmp(sNext,"TableAngledHeadersTextAlign") == 0)
            {
                lua_push_ImVec2(lua,ret_ImGuiStyle.TableAngledHeadersTextAlign);
            }
            else if(strcmp(sNext,"TreeLinesFlags")           == 0)
            {
                lua_pushinteger(lua,ret_ImGuiStyle.TreeLinesFlags);
            }
            else if(strcmp(sNext,"TreeLinesSize")            == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.TreeLinesSize);
            }
            else if(strcmp(sNext,"TreeLinesRounding")        == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.TreeLinesRounding);
            }
            else if(strcmp(sNext,"DragDropTargetRounding")   == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.DragDropTargetRounding);
            }
            else if(strcmp(sNext,"DragDropTargetBorderSize") == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.DragDropTargetBorderSize);
            }
            else if(strcmp(sNext,"DragDropTargetPadding")    == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.DragDropTargetPadding);
            }
            else if(strcmp(sNext,"ColorMarkerSize")          == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.ColorMarkerSize);
            }
            else if(strcmp(sNext,"ColorButtonPosition")      == 0)
            {
                lua_pushinteger(lua,ret_ImGuiStyle.ColorButtonPosition);
            }
            else if(strcmp(sNext,"ButtonTextAlign")          == 0)
            {
                lua_push_ImVec2(lua,ret_ImGuiStyle.ButtonTextAlign);
            }
            else if(strcmp(sNext,"SelectableTextAlign")      == 0)
            {
                lua_push_ImVec2(lua,ret_ImGuiStyle.SelectableTextAlign);
            }
            else if(strcmp(sNext,"SeparatorTextBorderSize")  == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.SeparatorTextBorderSize);
            }
            else if(strcmp(sNext,"SeparatorTextAlign")       == 0)
            {
                lua_push_ImVec2(lua,ret_ImGuiStyle.SeparatorTextAlign);
            }
            else if(strcmp(sNext,"SeparatorTextPadding")     == 0)
            {
                lua_push_ImVec2(lua,ret_ImGuiStyle.SeparatorTextPadding);
            }
            else if(strcmp(sNext,"DisplayWindowPadding")     == 0)
            {
                lua_push_ImVec2(lua,ret_ImGuiStyle.DisplayWindowPadding);
            }
            else if(strcmp(sNext,"DisplaySafeAreaPadding")   == 0)
            {
                lua_push_ImVec2(lua,ret_ImGuiStyle.DisplaySafeAreaPadding);
            }
            else if(strcmp(sNext,"MouseCursorScale")         == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.MouseCursorScale);
            }
            else if(strcmp(sNext,"AntiAliasedLines")         == 0)
            {
                lua_pushboolean(lua,ret_ImGuiStyle.AntiAliasedLines);
            }
            else if(strcmp(sNext,"AntiAliasedLinesUseTex")   == 0)
            {
                lua_pushboolean(lua,ret_ImGuiStyle.AntiAliasedLinesUseTex);
            }
            else if(strcmp(sNext,"AntiAliasedFill")          == 0)
            {
                lua_pushboolean(lua,ret_ImGuiStyle.AntiAliasedFill);
            }
            else if(strcmp(sNext,"CurveTessellationTol")     == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.CurveTessellationTol);
            }
            else if(strcmp(sNext,"CircleTessellationMaxError") == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.CircleTessellationMaxError);
            }
            else if(strcmp(sNext,"HoverStationaryDelay")     == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.HoverStationaryDelay);
            }
            else if(strcmp(sNext,"HoverDelayShort")           == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.HoverDelayShort);
            }
            else if(strcmp(sNext,"HoverDelayNormal")         == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.HoverDelayNormal);
            }
            else if(strcmp(sNext,"HoverFlagsForTooltipMouse") == 0)
            {
                lua_pushinteger(lua,ret_ImGuiStyle.HoverFlagsForTooltipMouse);
            }
            else if(strcmp(sNext,"HoverFlagsForTooltipNav")  == 0)
            {
                lua_pushinteger(lua,ret_ImGuiStyle.HoverFlagsForTooltipNav);
            }
            else if(strncmp(sNext,"ImGuiCol_",9)             == 0)
            {
                const auto it = enumColMap.find(sNext);
                if(it != enumColMap.end())
                {
                    lua_push_rgba(lua,ret_ImGuiStyle.Colors[it->second]);
                }
                else
                {
                    std::string msg("attribute not found [");
                    msg += sNext;
                    msg += ']';
                    lua_log_error(lua,msg.c_str());
                }
            }
            else if(strcmp(sNext,"Colors")                   == 0)
            {
                push_RGBA_arrayFromTable(lua,ret_ImGuiStyle.Colors,sizeof(ret_ImGuiStyle.Colors) / sizeof(ret_ImGuiStyle.Colors[0]));
            }
            else 
            {
                std::string msg("attribute not found [");
                msg += sNext;
                msg += ']';
                lua_log_error(lua,msg.c_str());
            }
        }
        return top;
    }
    else
    {
        lua_push_ImGuiStyle(lua,ret_ImGuiStyle);
        return 1;
    }
}
#if defined DEBUG || defined _DEBUG
#if !defined (ANDROID)
int onShowDemoWindowImGuiLua(lua_State *lua)
{
    //  Create Demo window (previously called ShowTestWindow). demonstrate most ImGui features. call this to learn about the library! try to make it always available in your application!
    ImGui::ShowDemoWindow(nullptr);
    return 0;
}
 
int onShowStyleSelectorImGuiLua(lua_State *lua)
{
    //  Add style selector block (not a window), essentially a combo listing the default styles.
    int index_input       = 1;
    const char * p_label  = luaL_checkstring(lua,index_input++);
    const bool ret_bool   = ImGui::ShowStyleSelector(p_label);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onShowFontSelectorImGuiLua(lua_State *lua)
{
    //  Add font selector block (not a window), essentially a combo listing the loaded fonts.
    int index_input       = 1;
    const char * p_label  = luaL_checkstring(lua,index_input++);
    ImGui::ShowFontSelector(p_label);
    return 0;
}

int onShowUserGuideImGuiLua(lua_State *lua)
{
    //  Add basic help/info block (not a window): how to manipulate ImGui as a end-user (mouse/keyboard controls).
    ImGui::ShowUserGuide();
    return 0;
}
#endif
#endif

int onGetVersionImGuiLua(lua_State *lua)
{
    //  Get the compiled version string e.g. "1.92.6 WIP" (essentially the compiled value for IMGUI_VERSION)
    const char* ret_char  = ImGui::GetVersion();
    lua_pushstring(lua,ret_char);
    return 1;
}

int onStyleColorsDarkImGuiLua(lua_State *lua)
{
    //  New, recommended style (default)
    int index_input     = 1;
    ImGuiStyle dst;
    ImGuiStyle * p_dst  =  lua_type(lua,index_input) == LUA_TNIL ? nullptr : lua_pop_ImGuiStyle_pointer(lua, index_input++, &dst);
    ImGui::StyleColorsDark(p_dst);
    return 0;
}

int onStyleColorsClassicImGuiLua(lua_State *lua)
{
    //  Classic imgui style
    int index_input     = 1;
    ImGuiStyle dst;
    ImGuiStyle * p_dst  =  lua_type(lua,index_input) == LUA_TNIL ? nullptr : lua_pop_ImGuiStyle_pointer(lua, index_input++, &dst);
    ImGui::StyleColorsClassic(p_dst);
    return 0;
}

int onStyleColorsLightImGuiLua(lua_State *lua)
{
    //  Best used with borders and a custom, thicker font
    int index_input     = 1;
    ImGuiStyle dst;
    ImGuiStyle * p_dst  =  lua_type(lua,index_input) == LUA_TNIL ? nullptr : lua_pop_ImGuiStyle_pointer(lua, index_input++, &dst);
    ImGui::StyleColorsLight(p_dst);
    return 0;
}

int onBeginImGuiLua(lua_State *lua)
{
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    const char * p_name         = luaL_checkstring(lua,index_input++);
    bool   can_be_closed        = false;
    ImGuiWindowFlags flags      = 0;
    bool closeable              = false;
    if(top >= index_input)
    {
        closeable               = lua_toboolean(lua,index_input++);
        can_be_closed           = closeable;
    }
    if(top >= index_input)
    {
        flags                   = lua_check_flags(lua,index_input);
    }
    const bool is_opened        = ImGui::Begin(p_name,can_be_closed ? &closeable : nullptr,flags);
    lua_pushboolean(lua,is_opened);
    lua_pushboolean(lua,can_be_closed ? closeable == false : false);
    return 2;
}

int onEndImGuiLua(lua_State *lua)
{
    ImGui::End();
    return 0;
}

int onBeginChildImGuiLua(lua_State *lua)
{
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    const char * p_str_id       = get_string_or_null(lua,index_input++);
    ImVec2 size                 = top >= index_input ? lua_pop_ImVec2(lua, index_input++) :  ImVec2(0,0);
    const bool border           = top >= index_input ? lua_toboolean(lua,index_input++) :  false;
    ImGuiWindowFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiWindowFlags_None, windowFlagsMap);
    const bool ret_bool         = ImGui::BeginChild(p_str_id,size,border,flags);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onEndChildImGuiLua(lua_State *)
{
    ImGui::EndChild();
    return 0;
}

// Tables API - see imgui_demo.cpp "Tables & Columns"
int onBeginTableImGuiLua(lua_State *lua)
{
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    const char * str_id         = luaL_checkstring(lua, index_input++);
    const int columns           = luaL_checkinteger(lua, index_input++);
    ImGuiTableFlags flags       = top >= index_input ? lua_opt_flags(lua, top, index_input, ImGuiTableFlags_None, allFlags) : ImGuiTableFlags_None;
    ImVec2 outer_size           = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(0.0f, 0.0f);
    const float inner_width     = top >= index_input ? (float)luaL_optnumber(lua, index_input++, 0.0) : 0.0f;
    const bool ret_bool        = ImGui::BeginTable(str_id, columns, flags, outer_size, inner_width);
    lua_pushboolean(lua, ret_bool);
    return 1;
}

int onEndTableImGuiLua(lua_State *)
{
    ImGui::EndTable();
    return 0;
}

int onTableNextRowImGuiLua(lua_State *lua)
{
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    ImGuiTableRowFlags row_flags = top >= index_input ? lua_opt_flags(lua, top, index_input, ImGuiTableRowFlags_None, allFlags) : ImGuiTableRowFlags_None;
    const float min_row_height  = top >= index_input ? (float)luaL_checknumber(lua, index_input++) : 0.0f;
    ImGui::TableNextRow(row_flags, min_row_height);
    return 0;
}

int onTableNextColumnImGuiLua(lua_State *lua)
{
    const bool ret_bool = ImGui::TableNextColumn();
    lua_pushboolean(lua, ret_bool);
    return 1;
}

int onTableSetColumnIndexImGuiLua(lua_State *lua)
{
    const int column_n  = luaL_checkinteger(lua, 1);
    const bool ret_bool = ImGui::TableSetColumnIndex(column_n);
    lua_pushboolean(lua, ret_bool);
    return 1;
}

int onTableSetupColumnImGuiLua(lua_State *lua)
{
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    const char * label          = luaL_checkstring(lua, index_input++);
    ImGuiTableColumnFlags flags = top >= index_input ? lua_opt_flags(lua, top, index_input, ImGuiTableColumnFlags_None, allFlags) : ImGuiTableColumnFlags_None;
    const float init_width      = top >= index_input ? (float)luaL_optnumber(lua, index_input++, 0.0) : 0.0f;
    ImGuiID user_id             = top >= index_input ? (ImGuiID)luaL_optinteger(lua, index_input++, 0) : 0;
    ImGui::TableSetupColumn(label, flags, init_width, user_id);
    return 0;
}

int onTableSetupScrollFreezeImGuiLua(lua_State *lua)
{
    const int cols = luaL_checkinteger(lua, 1);
    const int rows = luaL_checkinteger(lua, 2);
    ImGui::TableSetupScrollFreeze(cols, rows);
    return 0;
}

int onTableHeaderImGuiLua(lua_State *lua)
{
    const char * label = luaL_checkstring(lua, 1);
    ImGui::TableHeader(label);
    return 0;
}

int onTableHeadersRowImGuiLua(lua_State *)
{
    ImGui::TableHeadersRow();
    return 0;
}

int onTableAngledHeadersRowImGuiLua(lua_State *)
{
    ImGui::TableAngledHeadersRow();
    return 0;
}

int onTableGetColumnCountImGuiLua(lua_State *lua)
{
    const int ret = ImGui::TableGetColumnCount();
    lua_pushinteger(lua, ret);
    return 1;
}

int onTableGetColumnIndexImGuiLua(lua_State *lua)
{
    const int ret = ImGui::TableGetColumnIndex();
    lua_pushinteger(lua, ret);
    return 1;
}

int onTableGetRowIndexImGuiLua(lua_State *lua)
{
    const int ret = ImGui::TableGetRowIndex();
    lua_pushinteger(lua, ret);
    return 1;
}

int onTableGetColumnNameImGuiLua(lua_State *lua)
{
    const int column_n  = lua_gettop(lua) >= 1 ? luaL_optinteger(lua, 1, -1) : -1;
    const char * ret    = ImGui::TableGetColumnName(column_n);
    lua_pushstring(lua, ret ? ret : "");
    return 1;
}

int onTableGetColumnFlagsImGuiLua(lua_State *lua)
{
    const int column_n  = lua_gettop(lua) >= 1 ? luaL_optinteger(lua, 1, -1) : -1;
    const ImGuiTableColumnFlags ret = ImGui::TableGetColumnFlags(column_n);
    lua_pushinteger(lua, ret);
    return 1;
}

int onTableSetColumnEnabledImGuiLua(lua_State *lua)
{
    const int column_n  = luaL_checkinteger(lua, 1);
    const bool v        = lua_toboolean(lua, 2);
    ImGui::TableSetColumnEnabled(column_n, v);
    return 0;
}

int onTableGetHoveredColumnImGuiLua(lua_State *lua)
{
    const int ret = ImGui::TableGetHoveredColumn();
    lua_pushinteger(lua, ret);
    return 1;
}

int onTableSetBgColorImGuiLua(lua_State *lua)
{
    int index_input         = 1;
    const int top           = lua_gettop(lua);
    ImGuiTableBgTarget target = ImGuiTableBgTarget_None;
    if (lua_type(lua, index_input) == LUA_TSTRING)
    {
        const char * name = lua_tostring(lua, index_input++);
        const auto it = allFlags.find(name);
        if (it != allFlags.cend())
            target = (ImGuiTableBgTarget)it->second;
    }
    else
        target = (ImGuiTableBgTarget)luaL_checkinteger(lua, index_input++);
    ImU32 color;
    if (lua_type(lua, index_input) == LUA_TTABLE)
        color = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua, index_input++));
    else
        color = (ImU32)lua_tointeger(lua, index_input++);
    const int column_n = top >= index_input ? luaL_optinteger(lua, index_input++, -1) : -1;
    ImGui::TableSetBgColor(target, color, column_n);
    return 0;
}

int onIsWindowAppearingImGuiLua(lua_State *lua)
{
    const bool ret_bool  = ImGui::IsWindowAppearing();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsWindowCollapsedImGuiLua(lua_State *lua)
{
    const bool ret_bool  = ImGui::IsWindowCollapsed();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsWindowFocusedImGuiLua(lua_State *lua)
{
    //  Is current window focused? or its root/child, depending on flags. see flags for options.
    int index_input              = 1;
    const int top                = lua_gettop(lua);
    ImGuiFocusedFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiFocusedFlags_None, focusedFlagsMap);
    const bool ret_bool          = ImGui::IsWindowFocused(flags);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsWindowHoveredImGuiLua(lua_State *lua)
{
    //  Is current window hovered (and typically: not blocked by a popup/modal)? see flags for options. NB: If you are trying to check whether your mouse should be dispatched to imgui or to your app, you should use the 'io.WantCaptureMouse' boolean for that! Please read the FAQ!
    int index_input              = 1;
    const int top                = lua_gettop(lua);
    ImGuiHoveredFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiHoveredFlags_None, hoveredFlagsMap);
    const bool ret_bool          = ImGui::IsWindowHovered(flags);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onGetWindowPosImGuiLua(lua_State *lua)
{
    //  Get current window position in screen space (useful if you want to do your own drawing via the DrawList API)
    const ImVec2 ret_ImVec2  = ImGui::GetWindowPos();
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

int onGetWindowSizeImGuiLua(lua_State *lua)
{
    //  Get current window size
    const ImVec2 ret_ImVec2  = ImGui::GetWindowSize();
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

int onGetWindowWidthImGuiLua(lua_State *lua)
{
    //  Get current window width (shortcut for GetWindowSize().x)
    const float ret_float  = ImGui::GetWindowWidth();
    lua_pushnumber(lua,ret_float);
    return 1;
}

int onGetZoomImGuiLua(lua_State *lua)
{
    ImGuiIO& io = ImGui::GetIO();
    const float zoom = io.MouseWheel;
    lua_pushnumber(lua,zoom);
    return 1;
}


int onGetWindowHeightImGuiLua(lua_State *lua)
{
    //  Get current window height (shortcut for GetWindowSize().y)
    const float ret_float  = ImGui::GetWindowHeight();
    lua_pushnumber(lua,ret_float);
    return 1;
}

int onSetNextWindowPosImGuiLua(lua_State *lua)
{
    //  Set next window position. call before Begin(). use pivot=(0.5f,0.5f) to center on given point, etc.
    int index_input     = 1;
    const int top       = lua_gettop(lua);
    ImVec2 pos          = lua_pop_ImVec2(lua, index_input++);
    ImGuiCond cond      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    ImVec2 pivot        = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(0,0);
    ImGui::SetNextWindowPos(pos,cond,pivot);
    return 0;
}

int onSetNextWindowSizeImGuiLua(lua_State *lua)
{
    //  Set next window size. set axis to 0.0f to force an auto-fit on this axis. call before Begin()
    int index_input     = 1;
    const int top       = lua_gettop(lua);
    ImVec2 size         = lua_pop_ImVec2(lua, index_input++);
    ImGuiCond cond      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    ImGui::SetNextWindowSize(size,cond);
    return 0;
}


int onSetNextWindowSizeConstraintsImGuiLua(lua_State *lua)
{
    //  Set next window size limits. use -1,-1 on either X/Y axis to preserve the current size. Sizes will be rounded down. Use callback to apply non-trivial programmatic constraints.
    int index_input                             = 1;
    ImVec2 size_min                             = lua_pop_ImVec2(lua, index_input++);
    ImVec2 size_max                             = lua_pop_ImVec2(lua, index_input++);
    ImGui::SetNextWindowSizeConstraints(size_min,size_max,nullptr,nullptr);
    return 0;
}

int onSetNextWindowContentSizeImGuiLua(lua_State *lua)
{
    //  Set next window content size (~ scrollable client area, which enforce the range of scrollbars). Not including window decorations (title bar, menu bar, etc.) nor WindowPadding. set an axis to 0.0f to leave it automatic. call before Begin()
    int index_input  = 1;
    ImVec2 size      = lua_pop_ImVec2(lua, index_input++);
    ImGui::SetNextWindowContentSize(size);
    return 0;
}

int onSetNextWindowCollapsedImGuiLua(lua_State *lua)
{
    //  Set next window collapsed state. call before Begin()
    int index_input       = 1;
    const int top         = lua_gettop(lua);
    const bool collapsed  = lua_toboolean(lua,index_input++);
    ImGuiCond cond        = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    ImGui::SetNextWindowCollapsed(collapsed,cond);
    return 0;
}

int onSetNextWindowFocusImGuiLua(lua_State *lua)
{
    //  Set next window to be focused / top-most. call before Begin()
    ImGui::SetNextWindowFocus();
    return 0;
}

int onSetNextWindowBgAlphaImGuiLua(lua_State *lua)
{
    //  Set next window background color alpha. helper to easily modify ImGuiCol_WindowBg/ChildBg/PopupBg. you may also use ImGuiWindowFlags_NoBackground.
    int index_input    = 1;
    const float alpha  = luaL_checknumber(lua,index_input++);
    ImGui::SetNextWindowBgAlpha(alpha);
    return 0;
}

int onSetWindowPosImGuiLua(lua_State *lua)
{
    //  (not recommended) set current window position - call within Begin()/End(). prefer using SetNextWindowPos(), as this may incur tearing and side-effects.
    int index_input     = 1;
    const int top       = lua_gettop(lua);
    const char * name   = get_string_or_null(lua,index_input++);
    ImVec2 pos          = lua_pop_ImVec2(lua, index_input++);
    ImGuiCond cond      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    if(name)
        ImGui::SetWindowPos(name,pos,cond);
    else
        ImGui::SetWindowPos(pos,cond);
    return 0;
}

int onSetWindowSizeImGuiLua(lua_State *lua)
{
    //  (not recommended) set current window size - call within Begin()/End(). set to ImVec2(0,0) to force an auto-fit. prefer using SetNextWindowSize(), as this may incur tearing and minor side-effects.
    int index_input     = 1;
    const int top       = lua_gettop(lua);
    const char * name   = get_string_or_null(lua,index_input++);
    ImVec2 size         = lua_pop_ImVec2(lua, index_input++);
    ImGuiCond cond      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    if(name)
        ImGui::SetWindowSize(name,size,cond);
    else
        ImGui::SetWindowSize(size,cond);
    return 0;
}

int onSetWindowFocusImGuiLua(lua_State *lua)
{
    //  Set named window to be focused / top-most. use nullptr to remove focus.
    int index_input      = 1;
    const char * p_name  = get_string_or_null(lua,index_input);
    ImGui::SetWindowFocus(p_name);
    return 0;
}


// SetWindowFontScale removed - deprecated in ImGui 1.92, use GetIO().FontGlobalScale instead
    
int onSetWindowCollapsedImGuiLua(lua_State *lua)
{
    //  Set named window collapsed state
    int index_input       = 1;
    const int top         = lua_gettop(lua);
    const char * p_name   = get_string_or_null(lua,index_input++);
    const bool collapsed  = lua_toboolean(lua,index_input++);
    ImGuiCond cond        = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    if(p_name)
        ImGui::SetWindowCollapsed(p_name,collapsed,cond);
    else
        ImGui::SetWindowCollapsed(collapsed,cond);
    return 0;
}

// GetContentRegionMax removed - deprecated in ImGui 1.89+, use GetContentRegionAvail() + GetCursorPos() instead

int onGetContentRegionAvailImGuiLua(lua_State *lua)
{
    //  == GetContentRegionMax() - GetCursorPos()
    const ImVec2 ret_ImVec2  = ImGui::GetContentRegionAvail();
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

// GetWindowContentRegionMin/Max removed - deprecated in ImGui 1.89+

int onGetScrollXImGuiLua(lua_State *lua)
{
    //  Get scrolling amount [0..GetScrollMaxX()]
    const float ret_float  = ImGui::GetScrollX();
    lua_pushnumber(lua,ret_float);
    return 1;
}

int onGetScrollYImGuiLua(lua_State *lua)
{
    //  Get scrolling amount [0..GetScrollMaxY()]
    const float ret_float  = ImGui::GetScrollY();
    lua_pushnumber(lua,ret_float);
    return 1;
}

int onGetScrollMaxXImGuiLua(lua_State *lua)
{
    //  Get maximum scrolling amount ~~ ContentSize.X - WindowSize.X
    const float ret_float  = ImGui::GetScrollMaxX();
    lua_pushnumber(lua,ret_float);
    return 1;
}


int onIsScrollVisibleImGuiLua(lua_State *lua)
{
    // Check if scrollbars are visible by checking if there's scrollable content
    bool x = ImGui::GetScrollMaxX() > 0.0f;
    bool y = ImGui::GetScrollMaxY() > 0.0f;
    lua_pushboolean(lua,x);
    lua_pushboolean(lua,y);
    return 2;
}

int onGetScrollMaxYImGuiLua(lua_State *lua)
{
    //  Get maximum scrolling amount ~~ ContentSize.Y - WindowSize.Y
    const float ret_float  = ImGui::GetScrollMaxY();
    lua_pushnumber(lua,ret_float);
    return 1;
}

int onSetScrollXImGuiLua(lua_State *lua)
{
    //  Set scrolling amount [0..GetScrollMaxX()]
    int index_input       = 1;
    const float scroll_x  = luaL_checknumber(lua,index_input++);
    ImGui::SetScrollX(scroll_x);
    return 0;
}

int onSetScrollYImGuiLua(lua_State *lua)
{
    //  Set scrolling amount [0..GetScrollMaxY()]
    int index_input       = 1;
    const float scroll_y  = luaL_checknumber(lua,index_input++);
    ImGui::SetScrollY(scroll_y);
    return 0;
}

int onSetScrollHereXImGuiLua(lua_State *lua)
{
    //  Adjust scrolling amount to make current cursor position visible. center_x_ratio=0.0: left, 0.5: center, 1.0: right. When using to make a "default/current item" visible, consider using SetItemDefaultFocus() instead.
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    const float center_x_ratio  = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.5f;
    ImGui::SetScrollHereX(center_x_ratio);
    return 0;
}

int onSetScrollHereYImGuiLua(lua_State *lua)
{
    //  Adjust scrolling amount to make current cursor position visible. center_y_ratio=0.0: top, 0.5: center, 1.0: bottom. When using to make a "default/current item" visible, consider using SetItemDefaultFocus() instead.
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    const float center_y_ratio  = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.5f;
    ImGui::SetScrollHereY(center_y_ratio);
    return 0;
}

int onSetScrollFromPosXImGuiLua(lua_State *lua)
{
    //  Adjust scrolling amount to make given position visible. Generally GetCursorStartPos() + offset to compute a valid position.
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    const float local_x         = luaL_checknumber(lua,index_input++);
    const float center_x_ratio  = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.5f;
    ImGui::SetScrollFromPosX(local_x,center_x_ratio);
    return 0;
}

int onSetScrollFromPosYImGuiLua(lua_State *lua)
{
    //  Adjust scrolling amount to make given position visible. Generally GetCursorStartPos() + offset to compute a valid position.
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    const float local_y         = luaL_checknumber(lua,index_input++);
    const float center_y_ratio  = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.5f;
    ImGui::SetScrollFromPosY(local_y,center_y_ratio);
    return 0;
}

int onPopFontImGuiLua(lua_State *lua)
{
    ImGui::PopFont();
    return 0;
}

int onPushStyleColorImGuiLua(lua_State *lua)
{
    int index_input    = 1;
    constexpr int sCol = sizeof(ImGuiStyle::Colors) / sizeof(ImVec4);
    ImGuiCol idx       = 0;
    if(lua_type(lua,index_input) == LUA_TSTRING)
    {
        const char* styleCol = lua_tostring(lua,index_input++);
        const auto it = enumColMap.find(styleCol);
        if(it != enumColMap.end())
        {
            idx = it->second;
        }
        else
        {
            std::string msg("StyleColor not found [");
            msg += styleCol;
            msg += ']';
            lua_log_error(lua,msg.c_str());
        }
    }
    else
    {
        idx = luaL_checkinteger(lua, index_input++);
    }
    if( idx < 0 )
        lua_log_error(lua,"Index idx must be greater then Zero!");
    if( idx >= sCol)
    {
        char message[255] = "";
        snprintf(message,sizeof(message),"idx out of range! max[%d] ",sCol);
        lua_log_error(lua,message);
    }
    if(lua_type(lua,index_input) == LUA_TTABLE)
    {
        ImVec4 color = lua_get_rgba_to_ImVec4_fromTable(lua,index_input++);
        ImGui::PushStyleColor(idx,color);
    }
    else
    {
        ImU32     color     = lua_tointeger(lua, index_input++);
        ImGui::PushStyleColor(idx,color);
    }
    return 0;
}

int onPopStyleColorImGuiLua(lua_State *lua)
{
    int index_input  = 1;
    const int top    = lua_gettop(lua);
    const int count  = top >= index_input ? luaL_checkinteger(lua,index_input++) :  1;
    ImGui::PopStyleColor(count);
    return 0;
}

int onPushStyleVarImGuiLua(lua_State *lua)
{
    int index_input    = 1;
    ImGuiStyleVar idx  = 0;
    if (lua_type(lua,1) == LUA_TSTRING)
    {
        const char* styleVar  = lua_tostring(lua,index_input++);
        const auto itFlag     = allFlags.find(styleVar);
        if(itFlag != allFlags.cend())
        {
            idx = itFlag->second;
        }
        else
        {
            std::string msg("ImGuiStyleVar not found [");
            msg += styleVar;
            msg += ']';
            lua_log_error(lua,msg.c_str());
        }
    }
    else
    {
        idx  = luaL_checkinteger(lua, index_input++);
    }
    if( idx < 0 )
        lua_log_error(lua,"Index idx must be greater then Zero!");
    if( idx >= ImGuiStyleVar_COUNT)
    {
        char message[255] = "";
        snprintf(message,sizeof(message),"idx out of range! max[%d] ",ImGuiStyleVar_COUNT);
        lua_log_error(lua,message);
    }
    
    if(lua_type(lua,index_input) == LUA_TTABLE)
    {
        const ImVec2 val = lua_pop_ImVec2(lua,index_input++);
        ImGui::PushStyleVar(idx,val);
    }
    else
    {
        const float val    = luaL_checknumber(lua,index_input++);
        ImGui::PushStyleVar(idx,val);
    }
    return 0;
}


int onPopStyleVarImGuiLua(lua_State *lua)
{
    int index_input  = 1;
    const int top    = lua_gettop(lua);
    const int count  = top >= index_input ? luaL_checkinteger(lua,index_input++) :  1;
    ImGui::PopStyleVar(count);
    return 0;
}

int onGetStyleColorVec4ImGuiLua(lua_State *lua)
{
    //  Retrieve style color as stored in ImGuiStyle structure. use to feed back into PushStyleColor(), otherwise use GetColorU32() to get style color with style alpha baked in.
    int index_input          = 1;
    ImGuiCol  idx            = luaL_checkinteger(lua, index_input++);
    const ImVec4 ret_ImVec4  = ImGui::GetStyleColorVec4(idx);
    float color[4] = {ret_ImVec4.x,ret_ImVec4.y,ret_ImVec4.z,ret_ImVec4.w};
    lua_push_rgba(lua,color);
    return 1;
}

int onGetColorU32ImGuiLua(lua_State *lua)
{
    //  Retrieve given style color with style alpha applied and optional extra alpha multiplier
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    ImGuiCol idx           = luaL_checkinteger(lua, index_input++);
    const float alpha_mul  = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const ImU32 ret_ImU32  = ImGui::GetColorU32(idx,alpha_mul);
    lua_pushinteger(lua,ret_ImU32);
    return 1;
}

int onPushItemWidthImGuiLua(lua_State *lua)
{
    //  Set width of items for common large "item+label" widgets. >0.0f: width in pixels, <0.0f align xx pixels to the right of window (so -1.0f always align width to the right side). 0.0f = default to ~2/3 of windows width,
    int index_input         = 1;
    const float item_width  = luaL_checknumber(lua,index_input++);
    ImGui::PushItemWidth(item_width);
    return 0;
}

int onPopItemWidthImGuiLua(lua_State *lua)
{
    ImGui::PopItemWidth();
    return 0;
}

int onSetNextItemWidthImGuiLua(lua_State *lua)
{
    //  Set width of the _next_ common large "item+label" widget. >0.0f: width in pixels, <0.0f align xx pixels to the right of window (so -1.0f always align width to the right side)
    int index_input         = 1;
    const float item_width  = luaL_checknumber(lua,index_input++);
    ImGui::SetNextItemWidth(item_width);
    return 0;
}

int onCalcItemWidthImGuiLua(lua_State *lua)
{
    //  Width of item given pushed settings and current cursor position. NOT necessarily the width of last item unlike most 'Item' functions.
    const float ret_float  = ImGui::CalcItemWidth();
    lua_pushnumber(lua,ret_float);
    return 1;
}

int onPushTextWrapPosImGuiLua(lua_State *lua)
{
    //  Word-wrapping for Text*() commands. < 0.0f: no wrapping; 0.0f: wrap to end of window (or column); > 0.0f: wrap at 'wrap_pos_x' position in window local space
    int index_input               = 1;
    const int top                 = lua_gettop(lua);
    const float wrap_local_pos_x  = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    ImGui::PushTextWrapPos(wrap_local_pos_x);
    return 0;
}

int onPopTextWrapPosImGuiLua(lua_State *lua)
{
    ImGui::PopTextWrapPos();
    return 0;
}

// PushTabStop, PopTabStop, PushButtonRepeat, PopButtonRepeat removed - deprecated in ImGui 1.90+, use PushItemFlag/PopItemFlag instead

int onPushItemFlagImGuiLua(lua_State *lua)
{
    //  Generic item flag push - allows setting any ImGuiItemFlags
    int index_input           = 1;
    ImGuiItemFlags flags      = lua_check_flags(lua, index_input, itemFlagsMap);
    const bool value          = lua_toboolean(lua, index_input++);
    ImGui::PushItemFlag(flags, value);
    return 0;
}

int onPopItemFlagImGuiLua(lua_State *lua)
{
    //  Pop item flag
    ImGui::PopItemFlag();
    return 0;
}

int onSeparatorImGuiLua(lua_State *lua)
{
    //  Separator, generally horizontal. inside a menu bar or in horizontal layout mode, this becomes a vertical separator.
    ImGui::Separator();
    return 0;
}

int onSameLineImGuiLua(lua_State *lua)
{
    //  Call between widgets or groups to layout them horizontally. X position given in window coordinates.
    int index_input                  = 1;
    const int top                    = lua_gettop(lua);
    const float offset_from_start_x  = top >= index_input ? luaL_checknumber(lua,index_input++) : 0.0f;
    const float spacing              = top >= index_input ? luaL_checknumber(lua,index_input++) : -1.0f;
    ImGui::SameLine(offset_from_start_x,spacing);
    return 0;
}

int onNewLineImGuiLua(lua_State *lua)
{
    //  Undo a SameLine() or force a new line when in an horizontal-layout context.
    ImGui::NewLine();
    return 0;
}

int onSpacingImGuiLua(lua_State *lua)
{
    //  Add vertical spacing.
    ImGui::Spacing();
    return 0;
}

int onDummyImGuiLua(lua_State *lua)
{
    //  Add a dummy item of given size. unlike InvisibleButton(), Dummy() won't take the mouse click or be navigable into.
    int index_input  = 1;
    ImVec2 size      = lua_pop_ImVec2(lua, index_input++);
    ImGui::Dummy(size);
    return 0;
}

int onIndentImGuiLua(lua_State *lua)
{
    //  Move content position toward the right, by style.IndentSpacing or indent_w if != 0
    int index_input       = 1;
    const int top         = lua_gettop(lua);
    const float indent_w  = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    ImGui::Indent(indent_w);
    return 0;
}

int onUnindentImGuiLua(lua_State *lua)
{
    //  Move content position back to the left, by style.IndentSpacing or indent_w if != 0
    int index_input       = 1;
    const int top         = lua_gettop(lua);
    const float indent_w  = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    ImGui::Unindent(indent_w);
    return 0;
}

int onBeginGroupImGuiLua(lua_State *lua)
{
    //  Lock horizontal starting position
    ImGui::BeginGroup();
    return 0;
}

int onEndGroupImGuiLua(lua_State *lua)
{
    //  Unlock horizontal starting position + capture the whole group bounding box into one "item" (so you can use IsItemHovered() or layout primitives such as SameLine() on whole group, etc.)
    ImGui::EndGroup();
    return 0;
}

int onGetCursorPosImGuiLua(lua_State *lua)
{
    //  Cursor position in window coordinates (relative to window position)
    const ImVec2 ret_ImVec2  = ImGui::GetCursorPos();
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

int onGetCursorPosXImGuiLua(lua_State *lua)
{
    //   (some functions are using window-relative coordinates, such as: GetCursorPos, GetCursorStartPos, GetContentRegionMax, GetWindowContentRegion* etc.
    const float ret_float  = ImGui::GetCursorPosX();
    lua_pushnumber(lua,ret_float);
    return 1;
}

int onGetCursorPosYImGuiLua(lua_State *lua)
{
    //   other functions such as GetCursorScreenPos or everything in ImDrawList::
    const float ret_float  = ImGui::GetCursorPosY();
    lua_pushnumber(lua,ret_float);
    return 1;
}

int onSetCursorPosImGuiLua(lua_State *lua)
{
    //   are using the main, absolute coordinate system.
    int index_input   = 1;
    ImVec2 local_pos  = lua_pop_ImVec2(lua, index_input++);
    ImGui::SetCursorPos(local_pos);
    return 0;
}

int onSetCursorPosXImGuiLua(lua_State *lua)
{
    //   GetWindowPos() + GetCursorPos() == GetCursorScreenPos() etc.)
    int index_input      = 1;
    const float local_x  = luaL_checknumber(lua,index_input++);
    ImGui::SetCursorPosX(local_x);
    return 0;
}

int onSetCursorPosYImGuiLua(lua_State *lua)
{
    // 
    int index_input      = 1;
    const float local_y  = luaL_checknumber(lua,index_input++);
    ImGui::SetCursorPosY(local_y);
    return 0;
}

int onGetCursorStartPosImGuiLua(lua_State *lua)
{
    //  Initial cursor position in window coordinates
    const ImVec2 ret_ImVec2  = ImGui::GetCursorStartPos();
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

int onGetCursorScreenPosImGuiLua(lua_State *lua)
{
    //  Cursor position in absolute screen coordinates [0..io.DisplaySize] (useful to work with ImDrawList API)
    const ImVec2 ret_ImVec2  = ImGui::GetCursorScreenPos();
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

int onSetCursorScreenPosImGuiLua(lua_State *lua)
{
    //  Cursor position in absolute screen coordinates [0..io.DisplaySize]
    int index_input  = 1;
    ImVec2 pos       = lua_pop_ImVec2(lua, index_input++);
    ImGui::SetCursorScreenPos(pos);
    return 0;
}

int onAlignTextToFramePaddingImGuiLua(lua_State *lua)
{
    //  Vertically align upcoming text baseline to FramePadding.y so that it will align properly to regularly framed items (call if you have text on a line before a framed item)
    ImGui::AlignTextToFramePadding();
    return 0;
}

int onGetTextLineHeightImGuiLua(lua_State *lua)
{
    //  ~FontSize
    const float ret_float  = ImGui::GetTextLineHeight();
    lua_pushnumber(lua,ret_float);
    return 1;
}

int onGetTextLineHeightWithSpacingImGuiLua(lua_State *lua)
{
    //  ~FontSize + style.ItemSpacing.y (distance in pixels between 2 consecutive lines of text)
    const float ret_float  = ImGui::GetTextLineHeightWithSpacing();
    lua_pushnumber(lua,ret_float);
    return 1;
}

// GetFrameHeight, GetFrameHeightWithSpacing removed - not used

int onPushIDImGuiLua(lua_State *lua)
{
    //  Push string into the ID stack (will hash string).
    const int top          = lua_gettop(lua);
    int index_input        = 1;
    switch(lua_type(lua,index_input))
    {
        case LUA_TSTRING:
        {
            const char * p_str_id = lua_tostring(lua,index_input++);
            if(index_input <= top)
            {
                const char * str_id_end   = luaL_checkstring(lua,index_input++);
                const char * str_id_begin = p_str_id;
                ImGui::PushID(str_id_begin,str_id_end);
            }
            else
            {
                ImGui::PushID(p_str_id);
            }
        }
        break;
        case LUA_TNUMBER:
        {
            const int int_id = lua_tointeger(lua,index_input++);
            ImGui::PushID(int_id);
        }
        break;
        default:
        {
            lua_log_error(lua,"Expected: <number> or <string> or <string> <string>");
        }
        break;
    }
    return 0;
}

int onPopIDImGuiLua(lua_State *lua)
{
    //  Pop from the ID stack.
    ImGui::PopID();
    return 0;
}

// GetID removed - not used

int onTextImGuiLua(lua_State *lua)
{
    int index_input                                                            = 1;
    const char * text                                                          = luaL_checkstring(lua,index_input++);
    ImGui::Text("%s",text);
    return 0;
}

int onTextColoredImGuiLua(lua_State *lua)
{
    //  Shortcut for PushStyleColor(ImGuiCol_Text, col); Text(fmt, ...); PopStyleColor();
    int index_input              = 1;
    ImVec4 col                   = lua_get_rgba_to_ImVec4_fromTable(lua, index_input++);
    const char * text            = luaL_checkstring(lua,index_input++);
    ImGui::TextColored(col,"%s", text);
    return 0;
}

int onTextDisabledImGuiLua(lua_State *lua)
{
    //  Shortcut for PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_TextDisabled]); Text(fmt, ...); PopStyleColor();
    int index_input                                                           = 1;
    const char * text                                                         = luaL_checkstring(lua,index_input++);
    ImGui::TextDisabled("%s",text);
    return 0;
}

int onTextWrappedImGuiLua(lua_State *lua)
{
    //  Shortcut for PushTextWrapPos(0.0f); Text(fmt, ...); PopTextWrapPos();. Note that this won't work on an auto-resizing window if there's no other widgets to extend the window width, yoy may need to set a size using SetNextWindowSize().
    int index_input                                                           = 1;
    const char * text                                                         = luaL_checkstring(lua,index_input++);
    ImGui::TextWrapped("%s", text);
    return 0;
}

int onLabelTextImGuiLua(lua_State *lua)
{
    //  Display text+label aligned the same way as value+label widgets
    int index_input                                                            = 1;
    const char * p_label                                                       = luaL_checkstring(lua,index_input++);
    const char * p_fmt                                                         = luaL_checkstring(lua,index_input++);
    ImGui::LabelText(p_label,"%s",p_fmt);
    return 0;
}

int onBulletTextImGuiLua(lua_State *lua)
{
    //  Shortcut for Bullet()+Text()
    int index_input                                                            = 1;
    const char * str                                                           = luaL_checkstring(lua,index_input++);
    ImGui::BulletText("%s", str);
    return 0;
}

int onButtonImGuiLua(lua_State *lua)
{
    //  Button
    int index_input       = 1;
    const int top         = lua_gettop(lua);
    const char * p_label  = luaL_checkstring(lua,index_input++);
    ImVec2 size           = top >= index_input ? lua_pop_ImVec2(lua, index_input++) :  ImVec2 (0,0);
    const bool ret_bool   = ImGui::Button(p_label,size);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onSmallButtonImGuiLua(lua_State *lua)
{
    //  Button with FramePadding=(0,0) to easily embed within text
    int index_input       = 1;
    const char * p_label  = luaL_checkstring(lua,index_input++);
    const bool ret_bool   = ImGui::SmallButton(p_label);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onInvisibleButtonImGuiLua(lua_State *lua)
{
    //  Button behavior without the visuals, frequently useful to build custom behaviors using the public api (along with IsItemActive, IsItemHovered, etc.)
    int index_input        = 1;
    const char * p_str_id  = luaL_checkstring(lua,index_input++);
    ImVec2 size            = lua_pop_ImVec2(lua, index_input++);
    const bool ret_bool    = ImGui::InvisibleButton(p_str_id,size);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onArrowButtonImGuiLua(lua_State *lua)
{
    //  Square button with an arrow shape
    int index_input        = 1;
    const char * p_str_id  = luaL_checkstring(lua,index_input++);
    ImGuiDir dir           = ImGuiDir_None;
    
    if(lua_type(lua,index_input) == LUA_TSTRING)
    {
        const char * dirStr = luaL_checkstring(lua,index_input++);
        const auto it = enumDirMap.find(dirStr);
        if(it == enumDirMap.end())
        {
            std::string msg("ImGuiDir not found [");
            msg += dirStr;
            msg += "]. Valid values: ImGuiDir_None, ImGuiDir_Left, ImGuiDir_Right, ImGuiDir_Up, ImGuiDir_Down";
            lua_log_error(lua,msg.c_str());
        }
        else
        {
            dir = static_cast<ImGuiDir>(it->second);
        }
    }
    else
    {
        int dirInt = luaL_checkinteger(lua, index_input++);
        if(dirInt < ImGuiDir_None || dirInt >= ImGuiDir_COUNT)
        {
            char msg[255];
            snprintf(msg, sizeof(msg), "ImGuiDir value out of range [%d]. Valid range: %d to %d", 
                     dirInt, ImGuiDir_None, ImGuiDir_COUNT - 1);
            lua_log_error(lua, msg);
        }
        dir = static_cast<ImGuiDir>(dirInt);
    }
    
    const bool ret_bool    = ImGui::ArrowButton(p_str_id,dir);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onImageImGuiLua(lua_State *lua)
{
    int index_input                     = 1;
    const int top                       = lua_gettop(lua);
    unsigned int width                  = 0;
    unsigned int height                 = 0;
    ImTextureID user_texture_id         = get_imgui_texture_id(lua, index_input, width, height);
    ImVec2 size (static_cast<float>(width), static_cast<float>(height));
    if(top >= index_input && lua_type(lua, index_input) != LUA_TNIL)
        size                            = lua_pop_ImVec2(lua, index_input);
    
    index_input++;
    const ImVec2 uv0                    = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(0,0);
    const ImVec2 uv1                    = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(1,1);
    // Use ImageWithBg if tint/bg colors are provided, otherwise use basic Image
    if (top >= index_input)
    {
        const ImVec4 bg_col           = lua_get_rgba_to_ImVec4_fromTable(lua, index_input++);
        const ImVec4 tint_col         = top >= index_input ? lua_get_rgba_to_ImVec4_fromTable(lua, index_input++) : ImVec4(1,1,1,1);
        ImGui::ImageWithBg(user_texture_id, size, uv0, uv1, bg_col, tint_col);
    }
    else
    {
        ImGui::Image(user_texture_id, size, uv0, uv1);
    }
    return 0;
}

int onImageQuadImGuiLua(lua_State *lua)
{
    int index_input                     = 1;
    const int top                       = lua_gettop(lua);
    unsigned int width                  = 0;
    unsigned int height                 = 0;
    ImTextureID user_texture_id         = get_imgui_texture_id(lua, index_input, width, height);
    ImVec2 size (static_cast<float>(width), static_cast<float>(height));
    if(top >= index_input && lua_type(lua, index_input) != LUA_TNIL)
        size                            = lua_pop_ImVec2(lua, index_input);
    
    index_input++;
    const ImVec2 uv0                    = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(0,0);
    const ImVec2 uv1                    = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(1,0);
    const ImVec2 uv2                    = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(1,1);
    const ImVec2 uv3                    = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(0,1);
    const ImVec4 tint_col               = top >= index_input ? lua_get_rgba_to_ImVec4_fromTable(lua, index_input++) : ImVec4(1,1,1,1);
    // Note: ImageQuad is not a standard ImGui function - using draw list instead
    ImDrawList* draw_list               = ImGui::GetWindowDrawList();
    ImVec2 cursor_pos                   = ImGui::GetCursorScreenPos();
    ImVec2 p1(cursor_pos.x, cursor_pos.y);
    ImVec2 p2(cursor_pos.x + size.x, cursor_pos.y);
    ImVec2 p3(cursor_pos.x + size.x, cursor_pos.y + size.y);
    ImVec2 p4(cursor_pos.x, cursor_pos.y + size.y);
    draw_list->AddImageQuad(user_texture_id, p1, p2, p3, p4, uv0, uv1, uv2, uv3, ImGui::GetColorU32(tint_col));
    ImGui::Dummy(size); // Reserve space
    return 0;
}

int onImageButtonImGuiLua(lua_State *lua)
{
    int index_input                     = 1;
    const int top                       = lua_gettop(lua);
    unsigned int width                  = 0;
    unsigned int height                 = 0;
    const char* str_id                  = luaL_checkstring(lua, index_input++);
    ImTextureID user_texture_id         = get_imgui_texture_id(lua, index_input, width, height);
    ImVec2 size (static_cast<float>(width), static_cast<float>(height));
    if(top >= index_input && lua_type(lua, index_input) != LUA_TNIL)
        size                            = lua_pop_ImVec2(lua, index_input);
    
    index_input++;
    const ImVec2 uv0                    = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(0,0);
    const ImVec2 uv1                    = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(1,1);
    const ImVec4 bg_col                 = top >= index_input ? lua_get_rgba_to_ImVec4_fromTable(lua, index_input++) : ImVec4(0,0,0,0);
    const ImVec4 tint_col               = top >= index_input ? lua_get_rgba_to_ImVec4_fromTable(lua, index_input++) : ImVec4(1,1,1,1);
    const bool result                   = ImGui::ImageButton(str_id, user_texture_id, size, uv0, uv1, bg_col, tint_col);
    lua_pushboolean(lua, result);
    return 1;
}

int onCheckboxImGuiLua(lua_State *lua)
{
    int index_input          = 1;
    const char * p_label     = luaL_checkstring(lua,index_input++);
    bool active              = lua_toboolean(lua,index_input++);
    ImGui::Checkbox(p_label,&active);
    lua_pushboolean(lua,active);
    return 1;
}

int onCheckboxFlagsImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const char * p_label   = luaL_checkstring(lua,index_input++);
    unsigned int p_flags   = lua_check_flags(lua, index_input);
    const int flags_value  = lua_check_flags(lua, index_input);
    const bool ret_bool    = ImGui::CheckboxFlags(p_label,&p_flags,flags_value);
    lua_pushboolean(lua,ret_bool);
    lua_pushinteger(lua,p_flags);
    return 2;
}

int onRadioButtonImGuiLua(lua_State *lua)
{
    int index_input       = 1;
    const char * p_label  = luaL_checkstring(lua,index_input++);
    int index_activated   = luaL_checkinteger(lua,index_input++);
    const int my_index    = luaL_checkinteger(lua,index_input++);
    ImGui::RadioButton(p_label,&index_activated,my_index);
    lua_pushinteger(lua,index_activated);
    return 1;
}

int onProgressBarImGuiLua(lua_State *lua)
{
    int index_input         = 1;
    const int top           = lua_gettop(lua);
    const float fraction    = luaL_checknumber(lua,index_input++);
    ImVec2 size_arg         = top >= index_input ? lua_pop_ImVec2(lua, index_input++)  :  ImVec2(0,0);
    const char * p_overlay  = top >= index_input ? get_string_or_null(lua,index_input) :  nullptr;
    ImGui::ProgressBar(fraction,size_arg,p_overlay);
    return 0;
}

int onBulletImGuiLua(lua_State *lua)
{
    //  Draw a small circle and keep the cursor on the same line. advance cursor x position by GetTreeNodeToLabelSpacing(), same distance that TreeNode() uses
    ImGui::Bullet();
    return 0;
}

int onBeginComboImGuiLua(lua_State *lua)
{
    int index_input               = 1;
    const int top                 = lua_gettop(lua);
    const char * p_label          = luaL_checkstring(lua,index_input++);
    const char * p_preview_value  = luaL_checkstring(lua,index_input++);
    ImGuiComboFlags flags         = lua_opt_flags(lua, top, index_input, ImGuiComboFlags_None, comboFlagsMap);
    const bool ret_bool           = ImGui::BeginCombo(p_label,p_preview_value,flags);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onEndComboImGuiLua(lua_State *lua)
{
    //  Only call EndCombo() if BeginCombo() returns true!
    ImGui::EndCombo();
    return 0;
}


int onComboImGuiLua(lua_State *lua)
{
    int index_input                          = 1;
    const int top                            = lua_gettop(lua);
    const char * p_label                     = get_string_or_null(lua,index_input++);
    int p_current_item                       = luaL_checkinteger(lua,index_input++) - 1;
    std::vector<std::string> lsItems         = get_string_arrayFromTable(lua,index_input++,"Combo items");
    const int height_in_items                = top >= index_input ? luaL_checkinteger(lua,index_input++) :  -1;
    std::vector<const char*> items(lsItems.size());
    for(unsigned int i=0; i < lsItems.size(); ++i)
    {
        items[i] = lsItems[i].c_str();
    }
    if (p_label == nullptr || strlen(p_label) == 0)
    {
        static std::string label;
        label = "##COMBO_";
        label += std::to_string(lsItems.size());
        label += std::to_string(p_current_item);
        p_label = label.c_str();
    }
    const bool ret_bool                      = ImGui::Combo(p_label,&p_current_item,items.data(),items.size(),height_in_items);
    lua_pushboolean(lua,ret_bool);
    lua_pushinteger(lua,p_current_item + 1);
    if(p_current_item >=0 && p_current_item < static_cast<int>(items.size()))
        lua_pushstring(lua,items[p_current_item]);
    else
        lua_pushnil(lua);
    return 3;
}


int onDragFloatImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    float value                = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const float v_speed        = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const float v_min          = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const float v_max          = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const char * p_format      = top >= index_input ? luaL_checkstring(lua,index_input++) :  "%.3f";
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::DragFloat(p_label,&value,v_speed,v_min,v_max,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushnumber(lua,value);
    return 2;
}

int onDragFloat2ImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    float value[2]             = {0, 0};
    get_float_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"float table[2]");
    const float v_speed        = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const float v_min          = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const float v_max          = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const char * p_format      = top >= index_input ? luaL_checkstring(lua,index_input++) :  "%.3f";
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::DragFloat2(p_label,value,v_speed,v_min,v_max,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    push_float_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onDragFloat3ImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    float value[3]             = {0, 0, 0};
    get_float_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"float table[3]");
    const float v_speed        = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const float v_min          = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const float v_max          = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const char * p_format      = top >= index_input ? luaL_checkstring(lua,index_input++) :  "%.3f";
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::DragFloat3(p_label,value,v_speed,v_min,v_max,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    push_float_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onDragFloat4ImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    float value[4]             = {0, 0, 0, 0};
    get_float_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"float table[4]");
    const float v_speed        = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const float v_min          = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const float v_max          = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const char * p_format      = top >= index_input ? luaL_checkstring(lua,index_input++) :  "%.3f";
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::DragFloat4(p_label,value,v_speed,v_min,v_max,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    push_float_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onDragFloatRange2ImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    float v_current_min        = luaL_checknumber(lua,index_input++);
    float v_current_max        = luaL_checknumber(lua,index_input++);
    const float v_speed        = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const float v_min          = top >= index_input ? luaL_checknumber(lua,index_input++) :  0;
    const float v_max          = top >= index_input ? luaL_checknumber(lua,index_input++) :  0;
    const char * p_format      = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    const char * p_format_max  = top >= index_input ? lua_tostring(lua,index_input++) :  nullptr;
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::DragFloatRange2(p_label,&v_current_min,&v_current_max,v_speed,v_min,v_max,p_format,p_format_max,flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushnumber(lua,v_current_min);
    lua_pushnumber(lua,v_current_max);
    return 3;
}

int onDragIntImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    int value              = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
    const float v_speed    = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const int v_min        = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
    const int v_max        = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
    const char * p_format  = top >= index_input ? lua_tostring(lua,index_input++) :  "%d";
    const bool ret_bool    = ImGui::DragInt(p_label,&value,v_speed,v_min,v_max,p_format);
    lua_pushboolean(lua,ret_bool);
    lua_pushinteger(lua,value);
    return 2;
}

int onDragInt2ImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    int value[2]           = { 0, 0};
    get_int_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"int table[2]");
    const float v_speed    = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const int v_min        = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
    const int v_max        = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
    const char * p_format  = top >= index_input ? lua_tostring(lua,index_input++) :  "%d";
    const bool ret_bool    = ImGui::DragInt2(p_label,value,v_speed,v_min,v_max,p_format);
    lua_pushboolean(lua,ret_bool);
    push_int_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onDragInt3ImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    int value[3]           = {0, 0, 0};
    get_int_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"int table[3]");
    const float v_speed    = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const int v_min        = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
    const int v_max        = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
    const char * p_format  = top >= index_input ? lua_tostring(lua,index_input++) :  "%d";
    const bool ret_bool    = ImGui::DragInt3(p_label,value,v_speed,v_min,v_max,p_format);
    lua_pushboolean(lua,ret_bool);
    push_int_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onDragInt4ImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    int value[4]           = {0, 0, 0,0};
    get_int_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"int table[4]");
    const float v_speed    = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const int v_min        = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
    const int v_max        = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
    const char * p_format  = top >= index_input ? lua_tostring(lua,index_input++) :  "%d";
    const bool ret_bool    = ImGui::DragInt4(p_label,value,v_speed,v_min,v_max,p_format);
    lua_pushboolean(lua,ret_bool);
    push_int_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onDragIntRange2ImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    int v_current_min          = luaL_checkinteger(lua,index_input++);
    int v_current_max          = luaL_checkinteger(lua,index_input++);
    const float v_speed        = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const int v_min            = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
    const int v_max            = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
    const char * p_format      = top >= index_input ? lua_tostring(lua,index_input++) :  "%d";
    const char * p_format_max  = top >= index_input ? lua_tostring(lua,index_input++) :  nullptr;
    const bool ret_bool        = ImGui::DragIntRange2(p_label,&v_current_min,&v_current_max,v_speed,v_min,v_max,p_format,p_format_max);
    lua_pushboolean(lua,ret_bool);
    lua_pushinteger(lua,v_current_min);
    lua_pushinteger(lua,v_current_max);
    return 3;
}

int onSliderFloatImGuiLua(lua_State *lua)
{
    //  Adjust format to decorate the value with a prefix or a suffix for in-slider labels or unit display.
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    float value                = luaL_checknumber(lua,index_input++);
    const float v_min          = luaL_checknumber(lua,index_input++);
    const float v_max          = luaL_checknumber(lua,index_input++);
    const char * p_format      = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::SliderFloat(p_label,&value,v_min,v_max,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushnumber(lua,value);
    return 2;
}

int onSliderFloat2ImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    float value[2]             = {0.0f,0.0f};
    get_float_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"float table[2]");
    const float v_min          = luaL_checknumber(lua,index_input++);
    const float v_max          = luaL_checknumber(lua,index_input++);
    const char * p_format      = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::SliderFloat2(p_label,value,v_min,v_max,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    push_float_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onSliderFloat3ImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    float value[3]             = {0.0f,0.0f,0.0f};
    get_float_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"float table[3]");
    const float v_min          = luaL_checknumber(lua,index_input++);
    const float v_max          = luaL_checknumber(lua,index_input++);
    const char * p_format      = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::SliderFloat3(p_label,value,v_min,v_max,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    push_float_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onSliderFloat4ImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    float value[4]             = {0.0f,0.0f,0.0f,0.0f};
    get_float_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"float table[4]");
    const float v_min          = luaL_checknumber(lua,index_input++);
    const float v_max          = luaL_checknumber(lua,index_input++);
    const char * p_format      = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::SliderFloat4(p_label,value,v_min,v_max,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    push_float_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onSliderAngleImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    float value                = luaL_checknumber(lua,index_input++);
    const float v_min          = luaL_checknumber(lua,index_input++);
    const float v_max          = luaL_checknumber(lua,index_input++);
    const char * p_format      = top >= index_input ? lua_tostring(lua,index_input++) :  "%.0f deg";
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::SliderAngle(p_label,&value,v_min,v_max,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushnumber(lua,value);
    return 2;
}

int onSliderIntImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    int value                  = luaL_checkinteger(lua,index_input++);
    const int v_min            = luaL_checkinteger(lua,index_input++);
    const int v_max            = luaL_checkinteger(lua,index_input++);
    const char * p_format      = top >= index_input ? lua_tostring(lua,index_input++) :  "%d";
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::SliderInt(p_label,&value,v_min,v_max,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushinteger(lua,value);
    return 2;
}

int onSliderInt2ImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    int value[2]               = {0,0};
    get_int_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"int table[2]");
    const int v_min            = luaL_checkinteger(lua,index_input++);
    const int v_max            = luaL_checkinteger(lua,index_input++);
    const char * p_format      = top >= index_input ? lua_tostring(lua,index_input++) :  "%d";
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::SliderInt2(p_label,value,v_min,v_max,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    push_int_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onSliderInt3ImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    int value[3]               = {0,0,0};
    get_int_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"int table[3]");
    const int v_min            = luaL_checkinteger(lua,index_input++);
    const int v_max            = luaL_checkinteger(lua,index_input++);
    const char * p_format      = top >= index_input ? lua_tostring(lua,index_input++) :  "%d";
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::SliderInt3(p_label,value,v_min,v_max,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    push_int_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onSliderInt4ImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    int value[4]               = {0,0,0,0};
    get_int_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"int table[4]");
    const int v_min            = luaL_checkinteger(lua,index_input++);
    const int v_max            = luaL_checkinteger(lua,index_input++);
    const char * p_format      = top >= index_input ? lua_tostring(lua,index_input++) :  "%d";
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::SliderInt4(p_label,value,v_min,v_max,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    push_int_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onVSliderFloatImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    ImVec2 size                = lua_pop_ImVec2(lua, index_input++);
    float value                = luaL_checknumber(lua,index_input++);
    const float v_min          = luaL_checknumber(lua,index_input++);
    const float v_max          = luaL_checknumber(lua,index_input++);
    const char * p_format      = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::VSliderFloat(p_label,size,&value,v_min,v_max,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushnumber(lua,value);
    return 2;
}

int onVSliderIntImGuiLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const char * p_label       = luaL_checkstring(lua,index_input++);
    ImVec2 size                = lua_pop_ImVec2(lua, index_input++);
    int value                  = luaL_checkinteger(lua,index_input++);
    const int v_min            = luaL_checkinteger(lua,index_input++);
    const int v_max            = luaL_checkinteger(lua,index_input++);
    const char * p_format      = top >= index_input ? lua_tostring(lua,index_input++) :  "%d";
    ImGuiSliderFlags flags     = lua_opt_flags(lua, top, index_input, ImGuiSliderFlags_None, sliderFlagsMap);
    const bool ret_bool        = ImGui::VSliderInt(p_label,size,&value,v_min,v_max,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushinteger(lua,value);
    return 2;
}

int onInputTextImGuiLua(lua_State *lua)
{
    int index_input                             = 1;
    const int top                               = lua_gettop(lua);
    const char * p_label                        = luaL_checkstring(lua,index_input++);
    std::string text                            = luaL_checkstring(lua,index_input++);
    ImGuiInputTextFlags flags                   = lua_opt_flags(lua, top, index_input, ImGuiInputTextFlags_None, inputTextFlagsMap);
    const size_t buf_size                       = text.size() + 256;
    text.resize(buf_size);
    bool ret_bool                               = ImGui::InputText(p_label, &text[0], buf_size, flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushstring(lua, text.c_str());
    return 2;
}


int onInputTextMultilineImGuiLua(lua_State *lua)
{
    int index_input                             = 1;
    const int top                               = lua_gettop(lua);
    const char * p_label                        = luaL_checkstring(lua,index_input++);
    std::string text                            = luaL_checkstring(lua,index_input++);
    const ImVec2 size                           = top >= index_input ? lua_pop_ImVec2(lua,index_input++) : ImVec2(0,0);
    ImGuiInputTextFlags flags                   = lua_opt_flags(lua, top, index_input, ImGuiInputTextFlags_None, inputTextFlagsMap);
    const size_t buf_size                       = text.size() + 1024;
    text.resize(buf_size);
    bool ret_bool                               = ImGui::InputTextMultiline(p_label, &text[0], buf_size, size, flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushstring(lua, text.c_str());
    return 2;
}

int onInputTextWithHintImGuiLua(lua_State *lua)
{
    int index_input                             = 1;
    const int top                               = lua_gettop(lua);
    const char * p_label                        = luaL_checkstring(lua,index_input++);
    std::string text                            = luaL_checkstring(lua,index_input++);
    const char* hint                            = luaL_checkstring(lua,index_input++);
    ImGuiInputTextFlags flags                   = lua_opt_flags(lua, top, index_input, ImGuiInputTextFlags_None, inputTextFlagsMap);
    const size_t buf_size                       = text.size() + 256;
    text.resize(buf_size);
    bool ret_bool                               = ImGui::InputTextWithHint(p_label, hint, &text[0], buf_size, flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushstring(lua, text.c_str());
    return 2;
}

int onInputFloatImGuiLua(lua_State *lua)
{
    int index_input                = 1;
    const int top                  = lua_gettop(lua);
    const char * p_label           = luaL_checkstring(lua,index_input++);
    float  value                   = luaL_checknumber(lua,index_input++);
    const float step               = top >= index_input ? luaL_checknumber(lua,index_input++) :  1;
    const float step_fast          = top >= index_input ? luaL_checknumber(lua,index_input++) :  100;
    const char * p_format          = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    ImGuiInputTextFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiInputTextFlags_None, inputTextFlagsMap);
    const bool ret_bool            = ImGui::InputFloat(p_label,&value,step,step_fast,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushnumber(lua,value);
    return 2;
}

int onInputIntImGuiLua(lua_State *lua)
{
    int index_input                = 1;
    const int top                  = lua_gettop(lua);
    const char * p_label           = luaL_checkstring(lua,index_input++);
    int  value                     = luaL_checkinteger(lua,index_input++);
    const int step                 = top >= index_input ? luaL_checkinteger(lua,index_input++) :  1;
    const int step_fast            = top >= index_input ? luaL_checkinteger(lua,index_input++) :  100;
    ImGuiInputTextFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiInputTextFlags_None, inputTextFlagsMap);
    const bool ret_bool            = ImGui::InputInt(p_label,&value,step,step_fast,flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushinteger(lua,value);
    return 2;
}

int onInputInt2ImGuiLua(lua_State *lua)
{
    int index_input                = 1;
    const int top                  = lua_gettop(lua);
    const char * p_label           = luaL_checkstring(lua,index_input++);
    int values[2]                  = {0,0};
    get_int_arrayFromTable(lua,index_input++,values,sizeof(values) / sizeof(values[0]) ,"int table[2]");
    ImGuiInputTextFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiInputTextFlags_None, inputTextFlagsMap);
    const bool ret_bool            = ImGui::InputInt2(p_label,values,flags);
    lua_pushboolean(lua,ret_bool);
    push_int_arrayFromTable(lua,values,sizeof(values) / sizeof(values[0]));
    return 2;
}

int onInputInt3ImGuiLua(lua_State *lua)
{
    int index_input                = 1;
    const int top                  = lua_gettop(lua);
    const char * p_label           = luaL_checkstring(lua,index_input++);
    int values[3]                  = {0,0,0};
    get_int_arrayFromTable(lua,index_input++,values,sizeof(values) / sizeof(values[0]) ,"int table[3]");
    ImGuiInputTextFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiInputTextFlags_None, inputTextFlagsMap);
    const bool ret_bool            = ImGui::InputInt3(p_label,values,flags);
    lua_pushboolean(lua,ret_bool);
    push_int_arrayFromTable(lua,values,sizeof(values) / sizeof(values[0]));
    return 2;
}

int onInputInt4ImGuiLua(lua_State *lua)
{
    int index_input                = 1;
    const int top                  = lua_gettop(lua);
    const char * p_label           = luaL_checkstring(lua,index_input++);
    int values[4]                  = {0,0,0,4};
    get_int_arrayFromTable(lua,index_input++,values,sizeof(values) / sizeof(values[0]) ,"int table[4]");
    ImGuiInputTextFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiInputTextFlags_None, inputTextFlagsMap);
    const bool ret_bool            = ImGui::InputInt4(p_label,values,flags);
    lua_pushboolean(lua,ret_bool);
    push_int_arrayFromTable(lua,values,sizeof(values) / sizeof(values[0]));
    return 2;
}

int onInputDoubleImGuiLua(lua_State *lua)
{
    int index_input                = 1;
    const int top                  = lua_gettop(lua);
    const char * p_label           = luaL_checkstring(lua,index_input++);
    double  value                  = luaL_checknumber(lua,index_input++);
    const float step               = top >= index_input ? luaL_checknumber(lua,index_input++) :  1;
    const float step_fast          = top >= index_input ? luaL_checknumber(lua,index_input++) :  100;
    const char * p_format          = top >= index_input ? lua_tostring(lua,index_input++) :  "%.6f";
    ImGuiInputTextFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiInputTextFlags_None, inputTextFlagsMap);
    const bool ret_bool            = ImGui::InputDouble(p_label,&value,step,step_fast,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushnumber(lua, static_cast<lua_Number>(value));
    return 2;
}

int onColorEdit3ImGuiLua(lua_State *lua)
{
    int index_input                = 1;
    const int top                  = lua_gettop(lua);
    const char * p_label           = luaL_checkstring(lua,index_input++);
    float p_col[4]                 = {1,1,1,1};
    lua_get_rgba_FromTable(lua, index_input++, p_col);
    ImGuiColorEditFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiColorEditFlags_None, colorEditFlagsMap);
    const bool ret_bool            = ImGui::ColorEdit3(p_label,p_col,flags);
    lua_pushboolean(lua,ret_bool);
    lua_push_rgba(lua,p_col);
    return 2;
}

int onColorEdit4ImGuiLua(lua_State *lua)
{
    int index_input                = 1;
    const int top                  = lua_gettop(lua);
    const char * p_label           = luaL_checkstring(lua,index_input++);
    float p_col[4]                 = {1,1,1,1};
    lua_get_rgba_FromTable(lua, index_input++, p_col);
    ImGuiColorEditFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiColorEditFlags_None, colorEditFlagsMap);
    const bool ret_bool            = ImGui::ColorEdit4(p_label,p_col,flags);
    lua_pushboolean(lua,ret_bool);
    lua_push_rgba(lua,p_col);
    return 2;
}

int onColorPicker3ImGuiLua(lua_State *lua)
{
    int index_input                = 1;
    const int top                  = lua_gettop(lua);
    const char * p_label           = luaL_checkstring(lua,index_input++);
    float  p_col [4]               = {1,1,1,1};
    if(top >= index_input)
        lua_get_rgba_FromTable(lua,index_input++,p_col);
    ImGuiColorEditFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiColorEditFlags_None, colorEditFlagsMap);
    const bool ret_bool            = ImGui::ColorPicker3(p_label,p_col,flags);
    lua_pushboolean(lua,ret_bool);
    lua_push_rgba(lua,p_col);
    return 2;
}

int onColorPicker4ImGuiLua(lua_State *lua)
{
    int index_input                = 1;
    const int top                  = lua_gettop(lua);
    const char * p_label           = luaL_checkstring(lua,index_input++);
    float  p_col [4]               = {1,1,1,1};
    if(top >= index_input)
        lua_get_rgba_FromTable(lua,index_input++,p_col);
    ImGuiColorEditFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiColorEditFlags_None, colorEditFlagsMap);
    float p_ref_col[4]             =  {1,1,1,1};
    if(top >= index_input)
        lua_get_rgba_FromTable(lua,index_input++,p_ref_col);
    const bool ret_bool            = ImGui::ColorPicker4(p_label,p_col,flags,p_ref_col);
    lua_pushboolean(lua,ret_bool);
    lua_push_rgba(lua,p_col);
    return 2;
}

int onColorButtonImGuiLua(lua_State *lua)
{
    //  Display a colored square/button, hover for details, return true when pressed.
    int index_input                = 1;
    const int top                  = lua_gettop(lua);
    const char * p_desc_id         = luaL_checkstring(lua,index_input++);
    float col[4]                   = {1,1,1,1};
    lua_get_rgba_FromTable(lua, index_input++, col);
    ImVec4  p_col(col[0],col[1],col[1],col[1]);
    ImGuiColorEditFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiColorEditFlags_None, colorEditFlagsMap);
    ImVec2 size                    = top >= index_input ? lua_pop_ImVec2(lua, index_input++) :  ImVec2(0,0);
    const bool ret_bool            = ImGui::ColorButton(p_desc_id,p_col,flags,size);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onSetColorEditOptionsImGuiLua(lua_State *lua)
{
    //  Initialize current options (generally on application startup) if you want to select a default format, picker type, etc. User will be able to change many settings, unless you pass the _NoOptions flag to your calls.
    int index_input            = 1;
    ImGuiColorEditFlags flags  = lua_check_flags(lua, index_input, colorEditFlagsMap);
    ImGui::SetColorEditOptions(flags);
    return 0;
}

int onTreeNodeImGuiLua(lua_State *lua)
{
    const int top         = lua_gettop(lua);
    int index_input       = 1;
    bool ret_bool         = false;
    if(top == 2)
    {
        const char * p_str_id = get_string_or_null(lua,index_input++);
        const char * p_label  = luaL_checkstring(lua,index_input++);
        if(p_str_id)
        {
            ret_bool          = ImGui::TreeNode(p_str_id,"%s", p_label);
        }
        else
        {
            ret_bool          = ImGui::TreeNode(p_label);
        }
    }
    else
    {
        const char * p_label  = luaL_checkstring(lua,index_input++);
        ret_bool              = ImGui::TreeNode(p_label);
    }
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onTreeNodeExImGuiLua(lua_State *lua)
{
    const int top                 = lua_gettop(lua);
    int index_input               = 1;
    const char * label            = luaL_checkstring(lua,index_input++);
    ImGuiTreeNodeFlags flags      = lua_check_flags(lua, index_input, treeNodeFlagsMap);
    const char * p_str_id         = top >= index_input ? luaL_checkstring(lua,index_input++) : nullptr;
    bool ret_bool                 = false;
    if(p_str_id)
    {
        ret_bool = ImGui::TreeNodeEx(p_str_id,flags,"%s", label);
    }
    else
    {
        ret_bool = ImGui::TreeNodeEx(label,flags);
    }
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onTreePushImGuiLua(lua_State *lua)
{
    //  ~Indent()+PushId(). Already called by TreeNode() when returning true, but you can call TreePush/TreePop yourself if desired.
    int index_input        = 1;
    const char * p_str_id  = get_string_or_null(lua,index_input++);
    ImGui::TreePush(p_str_id);
    return 0;
}

int onTreePopImGuiLua(lua_State *lua)
{
    //  ~Unindent()+PopId()
    ImGui::TreePop();
    return 0;
}

int onGetTreeNodeToLabelSpacingImGuiLua(lua_State *lua)
{
    //  Horizontal distance preceding label when using TreeNode*() or Bullet() == (g.FontSize + style.FramePadding.x*2) for a regular unframed TreeNode
    const float ret_float  = ImGui::GetTreeNodeToLabelSpacing();
    lua_pushnumber(lua,ret_float);
    return 1;
}

int onCollapsingHeaderImGuiLua(lua_State *lua)
{
    //  If returning 'true' the header is open. doesn't indent nor push on ID stack. user doesn't have to call TreePop().
    int index_input               = 1;
    const int top                 = lua_gettop(lua);
    const char * p_label          = luaL_checkstring(lua,index_input++);
    bool p_p_open                 = top >= index_input ? lua_toboolean(lua,index_input++) : false;
    ImGuiTreeNodeFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiTreeNodeFlags_None, treeNodeFlagsMap);
    const bool ret_bool           = ImGui::CollapsingHeader(p_label,p_p_open ? &p_p_open : nullptr,flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushboolean(lua,p_p_open);
    return 2;
}

int onSetNextItemOpenImGuiLua(lua_State *lua)
{
    //  Set next TreeNode/CollapsingHeader open state.
    int index_input     = 1;
    const int top       = lua_gettop(lua);
    const bool is_open  = lua_toboolean(lua,index_input++);
    ImGuiCond cond      = lua_opt_flags(lua, top, index_input, ImGuiCond_None, condFlagsMap);
    ImGui::SetNextItemOpen(is_open,cond);
    return 0;
}

int onSelectableImGuiLua(lua_State *lua)
{
    //  "bool selected" carry the selection state (read-only). Selectable() is clicked is returns true so you can modify your selection state. size.x==0.0: use remaining width, size.x>0.0: specify width. size.y==0.0: use label height, size.y>0.0: specify height
    int index_input                 = 1;
    const int top                   = lua_gettop(lua);
    const char * p_label            = luaL_checkstring(lua,index_input++);
    bool selected                   = top >= index_input ? lua_toboolean(lua,index_input++) :  false;
    ImGuiSelectableFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiSelectableFlags_None, selectableFlagsMap);
    ImVec2 size                     = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(0,0);
    const bool ret_bool             = ImGui::Selectable(p_label,static_cast<bool*>(&selected),flags,size);
    lua_pushboolean(lua,ret_bool);
    lua_pushboolean(lua,selected);
    return 2;
}

int onListBoxImGuiLua(lua_State *lua)
{
    int index_input                       = 1;
    const int top                         = lua_gettop(lua);
    const char * p_label                  = luaL_checkstring(lua,index_input++);
    int current_item                      = luaL_checkinteger(lua,index_input++);
    std::vector<std::string> lsItems      = get_string_arrayFromTable(lua,index_input++,"ListBox items");
    const int height_in_items             = top >= index_input ? luaL_checkinteger(lua,index_input++) :  -1;
    std::vector<const char*> items(lsItems.size());
    for(unsigned int i=0; i < lsItems.size(); ++i)
    {
        items[i] = lsItems[i].c_str();
    }
    const bool ret_bool = ImGui::ListBox(p_label, &current_item, items.data(), items.size(), height_in_items);
    lua_pushboolean(lua,ret_bool);
    lua_pushinteger(lua,current_item);
    if(current_item >=0 && current_item < static_cast<int>(items.size()))
        lua_pushstring(lua,items[current_item]);
    else
        lua_pushnil(lua);
    return 3;
}

int onBeginListBoxImGuiLua(lua_State *lua)
{
    //  Use if you want to reimplement ListBox() with custom data or interactions. if the function return true, you can output elements then call EndListBox() afterwards.
    int index_input       = 1;
    const int top         = lua_gettop(lua);
    const char * p_label  = luaL_checkstring(lua,index_input++);
    const ImVec2  size    = top >= index_input ? lua_pop_ImVec2(lua, index_input++) :  ImVec2(0,0);
    const bool ret_bool   = ImGui::BeginListBox(p_label,size);
    lua_pushboolean(lua,ret_bool);
    return 1;
}


int onEndListBoxImGuiLua(lua_State *lua)
{
    //  Terminate the scrolling region. only call EndListBox() if BeginListBox() returned true!
    ImGui::EndListBox();
    return 0;
}

int onPlotLinesImGuiLua(lua_State *lua)
{
    int index_input              = 1;
    const int top                = lua_gettop(lua);
    const char * p_label         = luaL_checkstring (lua,index_input++);
    if (lua_type(lua,index_input) == LUA_TTABLE)
    {
        const size_t values_count = lua_rawlen(lua,index_input);
        std::vector<float> values(values_count);
        get_float_arrayFromTable(lua,index_input++,values.data(),values.size(),"Lines");
        const int values_offset      = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
        const char* overlay_text     = get_string_or_null(lua,index_input++);
        const float scale_min        = top >= index_input ? luaL_checknumber(lua,index_input++) :  FLT_MAX;
        const float scale_max        = top >= index_input ? luaL_checknumber(lua,index_input++) :  FLT_MAX;
        const ImVec2 graph_size      = top >= index_input ? lua_pop_ImVec2(lua,index_input++) :  ImVec2(0,0);
        ImGui::PlotLines(p_label, values.data(), values_count, values_offset, overlay_text, scale_min, scale_max,graph_size,sizeof(float));
    }
    else
    {
        lua_log_error(lua,"Expected table with values of Lines");
    }
    return 0;
}

int onPlotHistogramImGuiLua(lua_State *lua)
{
    int index_input              = 1;
    const int top                = lua_gettop(lua);
    const char * p_label         = luaL_checkstring (lua,index_input++);
    if (lua_type(lua,index_input) == LUA_TTABLE)
    {
        const size_t values_count = lua_rawlen(lua,index_input);
        std::vector<float> values(values_count);
        get_float_arrayFromTable(lua,index_input++,values.data(),values.size(),"Histogram");
        const int values_offset      = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
        const char* overlay_text     = get_string_or_null(lua,index_input++);
        const float scale_min        = top >= index_input ? luaL_checknumber(lua,index_input++) :  FLT_MAX;
        const float scale_max        = top >= index_input ? luaL_checknumber(lua,index_input++) :  FLT_MAX;
        const ImVec2 graph_size      = top >= index_input ? lua_pop_ImVec2(lua,index_input++) :  ImVec2(0,0);
        ImGui::PlotHistogram(p_label, values.data(), values_count, values_offset, overlay_text, scale_min, scale_max,graph_size,sizeof(float));
    }
    else
    {
        lua_log_error(lua,"Expected table with values of Histogram");
    }
    return 0;
}

int onBeginMenuBarImGuiLua(lua_State *lua)
{
    //  Append to menu-bar of current window (requires ImGuiWindowFlags_MenuBar flag set on parent window).
    const bool ret_bool  = ImGui::BeginMenuBar();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onEndMenuBarImGuiLua(lua_State *lua)
{
    //  Only call EndMenuBar() if BeginMenuBar() returns true!
    ImGui::EndMenuBar();
    return 0;
}

int onBeginMainMenuBarImGuiLua(lua_State *lua)
{
    //  Create and append to a full screen menu-bar.
    const bool ret_bool  = ImGui::BeginMainMenuBar();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onEndMainMenuBarImGuiLua(lua_State *lua)
{
    //  Only call EndMainMenuBar() if BeginMainMenuBar() returns true!
    ImGui::EndMainMenuBar();
    return 0;
}

int onBeginMenuImGuiLua(lua_State *lua)
{
    //  Create a sub-menu entry. only call EndMenu() if this returns true!
    int index_input       = 1;
    const int top         = lua_gettop(lua);
    const char * p_label  = luaL_checkstring(lua,index_input++);
    const bool enabled    = top >= index_input ? lua_toboolean(lua,index_input++) :  true;
    const bool ret_bool   = ImGui::BeginMenu(p_label,enabled);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onEndMenuImGuiLua(lua_State *lua)
{
    //  Only call EndMenu() if BeginMenu() returns true!
    ImGui::EndMenu();
    return 0;
}

int onMenuItemImGuiLua(lua_State *lua)
{
    //  Return true when activated. shortcuts are displayed for convenience but not processed by ImGui at the moment
    int index_input          = 1;
    const int top            = lua_gettop(lua);
    const char * p_label     = luaL_checkstring(lua,index_input++);
    const char * p_shortcut  = get_string_or_null(lua,index_input++);
    bool selected            = top >= index_input ? lua_toboolean(lua,index_input++) :  false;
    const bool enabled       = top >= index_input ? lua_toboolean(lua,index_input++) :  true;
    const bool ret_bool      = ImGui::MenuItem(p_label,p_shortcut,&selected,enabled);
    lua_pushboolean(lua,ret_bool);
    lua_pushboolean(lua,selected);
    return 2;
}

int onBeginTooltipImGuiLua(lua_State *lua)
{
    //  Begin/append a tooltip window. to create full-featured tooltip (with any kind of items).
    ImGui::BeginTooltip();
    return 0;
}

int onEndTooltipImGuiLua(lua_State *lua)
{
    ImGui::EndTooltip();
    return 0;
}


int onMakeFlagsImGuiLua(lua_State *lua)
{
    std::vector<std::string> flags;
    int flag = 0; 
    if(lua_type(lua,1) == LUA_TTABLE)
    {
        flags = get_string_arrayFromTable(lua,1,"tFlags");
    }
    else
    {
        const int top            = lua_gettop(lua);
        for(int index_input = 1; index_input <= top; ++index_input)
        {
            flags.push_back(luaL_checkstring(lua,index_input));
        }
    }
    for(std::size_t i=0; i < flags.size(); ++i)
    {
        const auto itFlag     = allFlags.find(flags[i]);
        if(itFlag != allFlags.cend())
        {
            flag |= itFlag->second;
        }
        else
        {
            printf("Flag [%s] not found!\n",flags[i].c_str());
        }
    }
    lua_pushinteger(lua,flag);
    return 1;
}

static void filterFlags(const std::string & flag_name,std::map<std::string,int> & flagsOut)
{
    for (auto it = allFlags.cbegin(); it != allFlags.cend(); ++it)
    {
        if(it->first.find(flag_name) != std::string::npos)
        {
            flagsOut[it->first]   = it->second;
        }
    }
}

int onListFlagsImGuiLua(lua_State *lua)
{
    const int top            = lua_gettop(lua);
    std::map<std::string,int> flagsOut;

    if(top >= 1)
    {
        std::vector<std::string> flags;
        if(lua_type(lua,1) == LUA_TTABLE)
        {
            flags = get_string_arrayFromTable(lua,1,"tFlags");
        }
        else
        {
            const int top            = lua_gettop(lua);
            for(int index_input = 1; index_input <= top; ++index_input)
            {
                flags.push_back(luaL_checkstring(lua,index_input));
            }
        }
        for(std::size_t i=0; i < flags.size(); ++i)
        {
            filterFlags(flags[i],flagsOut);
        }
    }
    else
    {
        flagsOut = allFlags;
    }
    lua_newtable(lua);
    for (auto it = flagsOut.cbegin(); it != flagsOut.cend(); ++it)
    {
        lua_pushinteger(lua,it->second);
        lua_setfield(lua, -2, it->first.c_str());
    }
    return 1;
}

int onSetTooltipImGuiLua(lua_State *lua)
{
    //  Set a text-only tooltip, typically use with ImGui::IsItemHovered(). override any previous call to SetTooltip().
    int index_input                                                           = 1;
    const char * text                                                         = luaL_checkstring(lua,index_input++);
    ImGui::SetTooltip("%s",text);
    return 0;
}

int onOpenPopupImGuiLua(lua_State *lua)
{
    //  Call to mark popup as open (don't call every frame!). popups are closed when user click outside, or if CloseCurrentPopup() is called within a BeginPopup()/EndPopup() block. By default, Selectable()/MenuItem() are calling CloseCurrentPopup(). Popup identifiers are relative to the current ID-stack (so OpenPopup and BeginPopup needs to be at the same level).
    int index_input        = 1;
    const char * p_str_id  = luaL_checkstring(lua,index_input++);
    ImGui::OpenPopup(p_str_id);
    return 0;
}

int onBeginPopupImGuiLua(lua_State *lua)
{
    //  Return true if the popup is open, and you can start outputting to it. only call EndPopup() if BeginPopup() returns true!
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    const char * p_str_id       = get_string_or_null(lua,index_input++);
    ImGuiWindowFlags   flags    = lua_opt_flags(lua, top, index_input, ImGuiWindowFlags_None, windowFlagsMap);
    const bool ret_bool         = ImGui::BeginPopup(p_str_id,flags);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onBeginPopupContextItemImGuiLua(lua_State *lua)
{
    //  Helper to open and begin popup when clicked on last item. if you can pass a nullptr str_id only if the previous item had an id. If you want to use that on a non-interactive item such as Text() you need to pass in an explicit ID here. read comments in .cpp!
    int index_input                    = 1;
    const int top                      = lua_gettop(lua);
    const char * p_str_id              = get_string_or_null(lua,index_input++);
    ImGuiPopupFlags popup_flags        = lua_opt_flags(lua, top, index_input, ImGuiPopupFlags_None, popupFlagsMap);
    const bool ret_bool                = ImGui::BeginPopupContextItem(p_str_id,popup_flags);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onBeginPopupContextWindowImGuiLua(lua_State *lua)
{
    //  Helper to open and begin popup when clicked on current window.
    int index_input                    = 1;
    const int top                      = lua_gettop(lua);
    const char * p_str_id              = get_string_or_null(lua,index_input++);
    ImGuiPopupFlags popup_flags        = lua_opt_flags(lua, top, index_input, ImGuiPopupFlags_MouseButtonRight, popupFlagsMap);
    const bool ret_bool                = ImGui::BeginPopupContextWindow(p_str_id,popup_flags);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onBeginPopupContextVoidImGuiLua(lua_State *lua)
{
    //  Helper to open and begin popup when clicked in void (where there are no imgui windows).
    int index_input                    = 1;
    const int top                      = lua_gettop(lua);
    const char * p_str_id              = get_string_or_null(lua,index_input++);
    ImGuiPopupFlags popup_flags        = lua_opt_flags(lua, top, index_input, ImGuiPopupFlags_MouseButtonRight, popupFlagsMap);
    const bool ret_bool                = ImGui::BeginPopupContextVoid(p_str_id,popup_flags);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onBeginPopupModalImGuiLua(lua_State *lua)
{
    //  Modal dialog (regular window with title bar, block interactions behind the modal window, can't close the modal window by clicking outside)

    int index_input             = 1;
    const int top               = lua_gettop(lua);
    const char * p_name         = luaL_checkstring(lua,index_input++);
    bool   can_be_closed        = false;
    ImGuiWindowFlags flags      = 0;
    bool closeable              = false;
    if(top >= index_input)
    {
        closeable               = lua_toboolean(lua,index_input++);
        can_be_closed           = closeable;
    }
    if(top >= index_input)
    {
        flags                   = lua_check_flags(lua, index_input, windowFlagsMap);
    }
    const bool is_opened        = ImGui::BeginPopupModal(p_name,can_be_closed ? &closeable : nullptr,flags);
    lua_pushboolean(lua,is_opened);
    lua_pushboolean(lua,can_be_closed ? closeable == false : false);
    return 2;
}

int onEndPopupImGuiLua(lua_State *lua)
{
    //  Only call EndPopup() if BeginPopupXXX() returns true!
    ImGui::EndPopup();
    return 0;
}

int onOpenPopupOnItemClickImGuiLua(lua_State *lua)
{
    // Helper to open a popup if mouse button is released over the item
    // - This is essentially the same as BeginPopupContextItem() but without the trailing BeginPopup()
    int index_input                    = 1;
    const int top                      = lua_gettop(lua);
    const char * p_str_id              = get_string_or_null(lua,index_input++);
    ImGuiPopupFlags popup_flags        = lua_opt_flags(lua, top, index_input, ImGuiPopupFlags_MouseButtonRight, popupFlagsMap);
    ImGui::OpenPopupOnItemClick(p_str_id,popup_flags);
    return 0;
}
/*
namespace ImGui
{
    bool IsPopupOpen(ImGuiID id);
}*/

int onIsPopupOpenImGuiLua(lua_State *lua)
{
    //  Return true if the popup is open at the current begin-ed level of the popup stack.
    const int top          = lua_gettop(lua);
    int index_input        = 1;
    bool ret_bool          = false;
    int flag_index         = 2;
    const int flag         = top > 1 ? lua_get_flags(lua, flag_index, 0) : 0;
    switch(lua_type(lua,index_input))
    {
        case LUA_TNIL:
        {
            const char * p_str_id = nullptr;
            ret_bool              = ImGui::IsPopupOpen(p_str_id, flag);
        }
        break;
        case LUA_TSTRING:
        {
            const char * p_str_id = lua_tostring(lua,index_input);
            ret_bool              = ImGui::IsPopupOpen(p_str_id,flag);
        }
        break;
        default:
        {
            lua_log_error(lua,"Expected: <string> or <nil>");
        }
        break;
    }
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onCloseCurrentPopupImGuiLua(lua_State *lua)
{
    //  Close the popup we have begin-ed into. clicking on a MenuItem or Selectable automatically close the current popup.
    ImGui::CloseCurrentPopup();
    return 0;
}

// Columns API removed - deprecated in ImGui 1.92+. Use Tables API instead.

int onBeginTabBarImGuiLua(lua_State *lua)
{
    //  Create and append into a TabBar
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    const char * p_str_id       = get_string_or_null(lua,index_input++);
    ImGuiTabBarFlags   flags    = lua_opt_flags(lua, top, index_input, ImGuiTabBarFlags_None, tabBarFlagsMap);
    const bool ret_bool         = ImGui::BeginTabBar(p_str_id,flags);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onEndTabBarImGuiLua(lua_State *lua)
{
    //  Only call EndTabBar() if BeginTabBar() returns true!
    ImGui::EndTabBar();
    return 0;
}

int onBeginTabItemImGuiLua(lua_State *lua)
{
    //  Create a Tab. Returns true if the Tab is selected.
    int index_input              = 1;
    const int top                = lua_gettop(lua);
    const char * p_label         = luaL_checkstring(lua,index_input++);
    static bool var_bool_123     = 0;
    bool * p_p_open              =  nullptr;
    if(top >= index_input)
    {
        if(lua_type(lua,index_input+1) == LUA_TBOOLEAN)
        {
            var_bool_123             = lua_toboolean(lua,index_input++);
            p_p_open                 = &var_bool_123;
        }
        else
        {
            index_input++;
        }
    }
    ImGuiTabItemFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiTabItemFlags_None, tabItemFlagsMap);
    const bool ret_bool          = ImGui::BeginTabItem(p_label,p_p_open,flags);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onEndTabItemImGuiLua(lua_State *lua)
{
    //  Only call EndTabItem() if BeginTabItem() returns true!
    ImGui::EndTabItem();
    return 0;
}

int onSetTabItemClosedImGuiLua(lua_State *lua)
{
    //  Notify TabBar or Docking system of a closed tab/window ahead (useful to reduce visual flicker on reorderable tab bars). For tab-bar: call after BeginTabBar() and before Tab submissions. Otherwise call with a window name.
    int index_input                            = 1;
    const char * p_tab_or_docked_window_label  = luaL_checkstring(lua,index_input++);
    ImGui::SetTabItemClosed(p_tab_or_docked_window_label);
    return 0;
}

int onLogToTTYImGuiLua(lua_State *lua)
{
    //  Start logging to tty (stdout)
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const int auto_open_depth  = top >= index_input ? luaL_checkinteger(lua,index_input++) :  -1;
    ImGui::LogToTTY(auto_open_depth);
    return 0;
}

int onLogToFileImGuiLua(lua_State *lua)
{
    //  Start logging to file
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const int auto_open_depth  = top >= index_input ? luaL_checkinteger(lua,index_input++) :  -1;
    const char * p_filename    = top >= index_input ? lua_tostring(lua,index_input++) :  nullptr;
    ImGui::LogToFile(auto_open_depth,p_filename);
    return 0;
}

int onLogToClipboardImGuiLua(lua_State *lua)
{
    //  Start logging to OS clipboard
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const int auto_open_depth  = top >= index_input ? luaL_checkinteger(lua,index_input++) :  -1;
    ImGui::LogToClipboard(auto_open_depth);
    return 0;
}

int onLogFinishImGuiLua(lua_State *lua)
{
    //  Stop logging (close file, etc.)
    ImGui::LogFinish();
    return 0;
}

int onLogButtonsImGuiLua(lua_State *lua)
{
    //  Helper to display buttons for logging to tty/file/clipboard
    ImGui::LogButtons();
    return 0;
}

int onLogTextImGuiLua(lua_State *lua)
{
    //  Pass text data straight to log (without being displayed)
    int index_input                                                             = 1;
    const char * text                                                           = luaL_checkstring(lua,index_input++);
    ImGui::LogText("%s", text);
    return 0;
}

int onPushClipRectImGuiLua(lua_State *lua)
{
    int index_input                              = 1;
    ImVec2 clip_rect_min                         = lua_pop_ImVec2(lua, index_input++);
    ImVec2 clip_rect_max                         = lua_pop_ImVec2(lua, index_input++);
    const bool intersect_with_current_clip_rect  = lua_toboolean(lua,index_input++);
    ImGui::PushClipRect(clip_rect_min,clip_rect_max,intersect_with_current_clip_rect);
    return 0;
}

int onPopClipRectImGuiLua(lua_State *lua)
{
    ImGui::PopClipRect();
    return 0;
}

int onSetItemDefaultFocusImGuiLua(lua_State *lua)
{
    //  Make last item the default focused item of a window.
    ImGui::SetItemDefaultFocus();
    return 0;
}

int onSetKeyboardFocusHereImGuiLua(lua_State *lua)
{
    //  Focus keyboard on the next widget. Use positive 'offset' to access sub components of a multiple component widget. Use -1 to access previous widget.
    int index_input   = 1;
    const int top     = lua_gettop(lua);
    const int offset  = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
    ImGui::SetKeyboardFocusHere(offset);
    return 0;
}

int onIsItemHoveredImGuiLua(lua_State *lua)
{
    //  Is the last item hovered? (and usable, aka not blocked by a popup, etc.). See ImGuiHoveredFlags for more options.
    int index_input              = 1;
    const int top                = lua_gettop(lua);
    ImGuiHoveredFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiHoveredFlags_None, hoveredFlagsMap);
    const bool ret_bool          = ImGui::IsItemHovered(flags);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsItemActiveImGuiLua(lua_State *lua)
{
    //  Is the last item active? (e.g. button being held, text field being edited. This will continuously return true while holding mouse button on an item. Items that don't interact will always return false)
    const bool ret_bool  = ImGui::IsItemActive();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsItemFocusedImGuiLua(lua_State *lua)
{
    //  Is the last item focused for keyboard/gamepad navigation?
    const bool ret_bool  = ImGui::IsItemFocused();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsItemClickedImGuiLua(lua_State *lua)
{
    //  Is the last item clicked? (e.g. button/node just clicked on) == IsMouseClicked(mouse_button) && IsItemHovered()
    int index_input                    = 1;
    const int top                      = lua_gettop(lua);
    ImGuiMouseButton mouse_button      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    const bool ret_bool                = ImGui::IsItemClicked(mouse_button);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsItemVisibleImGuiLua(lua_State *lua)
{
    //  Is the last item visible? (items may be out of sight because of clipping/scrolling)
    const bool ret_bool  = ImGui::IsItemVisible();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsItemEditedImGuiLua(lua_State *lua)
{
    //  Did the last item modify its underlying value this frame? or was pressed? This is generally the same as the "bool" return value of many widgets.
    const bool ret_bool  = ImGui::IsItemEdited();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsItemActivatedImGuiLua(lua_State *lua)
{
    //  Was the last item just made active (item was previously inactive).
    const bool ret_bool  = ImGui::IsItemActivated();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsItemDeactivatedImGuiLua(lua_State *lua)
{
    //  Was the last item just made inactive (item was previously active). Useful for Undo/Redo patterns with widgets that requires continuous editing.
    const bool ret_bool  = ImGui::IsItemDeactivated();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsItemDeactivatedAfterEditImGuiLua(lua_State *lua)
{
    //  Was the last item just made inactive and made a value change when it was active? (e.g. Slider/Drag moved). Useful for Undo/Redo patterns with widgets that requires continuous editing. Note that you may get false positives (some widgets such as Combo()/ListBox()/Selectable() will return true even when clicking an already selected item).
    const bool ret_bool  = ImGui::IsItemDeactivatedAfterEdit();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsItemToggledOpenImGuiLua(lua_State *lua)
{
    //  Was the last item open state toggled? set by TreeNode().
    const bool ret_bool  = ImGui::IsItemToggledOpen();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsAnyItemHoveredImGuiLua(lua_State *lua)
{
    //  Is any item hovered?
    const bool ret_bool  = ImGui::IsAnyItemHovered();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsAnyItemActiveImGuiLua(lua_State *lua)
{
    //  Is any item active?
    const bool ret_bool  = ImGui::IsAnyItemActive();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsAnyItemFocusedImGuiLua(lua_State *lua)
{
    //  Is any item focused?
    const bool ret_bool  = ImGui::IsAnyItemFocused();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onGetItemRectMinImGuiLua(lua_State *lua)
{
    //  Get upper-left bounding rectangle of the last item (screen space)
    const ImVec2 ret_ImVec2  = ImGui::GetItemRectMin();
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

int onGetItemRectMaxImGuiLua(lua_State *lua)
{
    //  Get lower-right bounding rectangle of the last item (screen space)
    const ImVec2 ret_ImVec2  = ImGui::GetItemRectMax();
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

int onGetItemRectSizeImGuiLua(lua_State *lua)
{
    //  Get size of last item
    const ImVec2 ret_ImVec2  = ImGui::GetItemRectSize();
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

int onSetNextItemAllowOverlapImGuiLua(lua_State *lua)
{
    //  Set next item to allow being overlapped by a subsequent item. Call before item.
    ImGui::SetNextItemAllowOverlap();
    return 0;
}

int onIsRectVisibleImGuiLua(lua_State *lua)
{
    //  Test if rectangle (in screen space) is visible / not clipped. to perform coarse clipping on user's side.
    int index_input      = 1;
    ImVec2 rect_min      = lua_pop_ImVec2(lua, index_input++);
    ImVec2 rect_max      = lua_pop_ImVec2(lua, index_input++);
    const bool ret_bool  = ImGui::IsRectVisible(rect_min,rect_max);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

// GetTime, GetFrameCount, GetStyleColorName removed - not used


int onCalcTextSizeImGuiLua(lua_State *lua)
{
    int index_input                         = 1;
    const int top                           = lua_gettop(lua);
    const char * p_text                     = luaL_checkstring(lua,index_input++);
    const char * p_text_end                 = top >= index_input ? lua_tostring(lua,index_input++) :  nullptr;
    const bool hide_text_after_double_hash  = top >= index_input ? lua_toboolean(lua,index_input++) :  false;
    const float wrap_width                  = top >= index_input ? luaL_checknumber(lua,index_input++) :  -1.0f;
    const ImVec2 ret_ImVec2                 = ImGui::CalcTextSize(p_text,p_text_end,hide_text_after_double_hash,wrap_width);
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

// REMOVED: CalcListClipping - deprecated in ImGui 1.92
// Use ImGuiListClipper class instead for large lists

// REMOVED: BeginChildFrame - deprecated in ImGui 1.92
// Use BeginChild with PushStyleVar/PushStyleColor for frame styling instead

// REMOVED: EndChildFrame - deprecated in ImGui 1.92
// Use EndChild instead

int onColorConvertU32ToFloat4ImGuiLua(lua_State *lua)
{
    int index_input          = 1;
    ImU32 in                 = luaL_checkinteger(lua, index_input++);
    const ImVec4 ret_ImVec4  = ImGui::ColorConvertU32ToFloat4(in);
    lua_push_ImVec4(lua,ret_ImVec4);
    return 1;
}

int onColorConvertFloat4ToU32ImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    ImVec4 in              = lua_get_rgba_to_ImVec4_fromTable(lua,index_input);
    const ImU32 ret_ImU32  = ImGui::ColorConvertFloat4ToU32(in);
    lua_pushinteger(lua,ret_ImU32);
    return 1;
}

int onColorConvertRGBtoHSVImGuiLua(lua_State *lua)
{
    int index_input    = 1;
    const float r      = luaL_checknumber(lua,index_input++);
    const float g      = luaL_checknumber(lua,index_input++);
    const float b      = luaL_checknumber(lua,index_input++);
    float out_h        = 0.0f;
    float out_s        = 0.0f;
    float out_v        = 0.0f;
    ImGui::ColorConvertRGBtoHSV(r,g,b,out_h,out_s,out_v);
    lua_pushnumber(lua,out_h);
    lua_pushnumber(lua,out_s);
    lua_pushnumber(lua,out_v);
    return 3;
}

int onColorConvertHSVtoRGBImGuiLua(lua_State *lua)
{
    int index_input    = 1;
    const float h      = luaL_checknumber(lua,index_input++);
    const float s      = luaL_checknumber(lua,index_input++);
    const float v      = luaL_checknumber(lua,index_input++);
    float out_r        = 0.0f;
    float out_g        = 0.0f;
    float out_b        = 0.0f;
    ImGui::ColorConvertHSVtoRGB(h,s,v,out_r,out_g,out_b);
    lua_pushnumber(lua,out_r);
    lua_pushnumber(lua,out_g);
    lua_pushnumber(lua,out_b);
    return 3;
}

// GetKeyIndex removed - deprecated, ImGuiKey values can be used directly

int onIsKeyDownImGuiLua(lua_State *lua)
{
    //  Is key being held.
    int index_input           = 1;
    ImGuiKey imgui_key        = ImGuiKey_None;
    if(lua_type(lua,index_input) == LUA_TNUMBER)
    {
        imgui_key = static_cast<ImGuiKey>(luaL_checkinteger(lua, index_input++));
    }
    else
    {
        const char* key       = luaL_checkstring(lua,index_input++);
        const auto  it        = enumKeyMap.find(key);
        if(it != enumKeyMap.end())
        {
            imgui_key  = static_cast<ImGuiKey>(it->second);
        }
        else
        {
            char str [255] = "";
            snprintf(str,sizeof(str),"Key [%s] not found",key);
            lua_log_error(lua,str);
        }
    }
    const bool ret_bool       = ImGui::IsKeyDown(imgui_key);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsKeyPressedImGuiLua(lua_State *lua)
{
    //  Was key pressed (went from !Down to Down)? if repeat=true, uses io.KeyRepeatDelay / KeyRepeatRate
    int index_input           = 1;
    const int top             = lua_gettop(lua);
    ImGuiKey imgui_key        = ImGuiKey_None;
    if(lua_type(lua,index_input) == LUA_TNUMBER)
    {
        imgui_key = static_cast<ImGuiKey>(luaL_checkinteger(lua, index_input++));
    }
    else
    {
        const char* key       = luaL_checkstring(lua,index_input++);
        const auto  it        = enumKeyMap.find(key);
        if(it != enumKeyMap.end())
        {
            imgui_key  = static_cast<ImGuiKey>(it->second);
        }
        else
        {
            char str [255] = "";
            snprintf(str,sizeof(str),"Key [%s] not found",key);
            lua_log_error(lua,str);
        }
    }
    const bool repeat         = top >= index_input ? lua_toboolean(lua,index_input++) :  true;
    const bool ret_bool       = ImGui::IsKeyPressed(imgui_key,repeat);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsKeyReleasedImGuiLua(lua_State *lua)
{
    //  Was key released (went from Down to !Down)?
    int index_input           = 1;
    ImGuiKey imgui_key        = ImGuiKey_None;
    if(lua_type(lua,index_input) == LUA_TNUMBER)
    {
        imgui_key = static_cast<ImGuiKey>(luaL_checkinteger(lua, index_input++));
    }
    else
    {
        const char* key       = luaL_checkstring(lua,index_input++);
        const auto  it        = enumKeyMap.find(key);
        if(it != enumKeyMap.end())
        {
            imgui_key  = static_cast<ImGuiKey>(it->second);
        }
        else
        {
            char str [255] = "";
            snprintf(str,sizeof(str),"Key [%s] not found",key);
            lua_log_error(lua,str);
        }
    }
    const bool ret_bool       = ImGui::IsKeyReleased(imgui_key);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onGetKeyPressedAmountImGuiLua(lua_State *lua)
{
    //  Uses provided repeat rate/delay. return a count, most often 0 or 1 but might be >1 if RepeatRate is small enough that DeltaTime > RepeatRate
    int index_input           = 1;
    ImGuiKey imgui_key        = ImGuiKey_None;
    if(lua_type(lua,index_input) == LUA_TNUMBER)
    {
        imgui_key = static_cast<ImGuiKey>(luaL_checkinteger(lua, index_input++));
    }
    else
    {
        const char* key       = luaL_checkstring(lua,index_input++);
        const auto  it        = enumKeyMap.find(key);
        if(it != enumKeyMap.end())
        {
            imgui_key  = static_cast<ImGuiKey>(it->second);
        }
        else
        {
            char str [255] = "";
            snprintf(str,sizeof(str),"Key [%s] not found",key);
            lua_log_error(lua,str);
        }
    }
    const float repeat_delay  = luaL_checknumber(lua,index_input++);
    const float rate          = luaL_checknumber(lua,index_input++);
    const int ret_int         = ImGui::GetKeyPressedAmount(imgui_key,repeat_delay,rate);
    lua_pushinteger(lua,ret_int);
    return 1;
}

int onGetMainMenuBarHeightImGuiLua(lua_State *lua)
{
    // GetMainMenuBarHeight doesn't exist in ImGui 1.92 public API
    // Return the frame height which is typically the menu bar height
    float height = ImGui::GetFrameHeight();
    lua_pushnumber(lua,height);
    return 1;
}

int onCaptureKeyboardFromAppImGuiLua(lua_State *lua)
{
    //  Manually override io.WantCaptureKeyboard flag next frame (said flag is entirely left for your application to handle). e.g. force capture keyboard when your widget is being hovered.
    int index_input                         = 1;
    const int top                           = lua_gettop(lua);
    const bool want_capture_keyboard_value  = top >= index_input ? lua_toboolean(lua,index_input++) :  true;
    ImGuiIO& io = ImGui::GetIO();
    io.WantCaptureKeyboard = want_capture_keyboard_value;
    return 0;
}

int onIsMouseDownImGuiLua(lua_State *lua)
{
    //  Is mouse button held?
    int index_input              = 1;
    ImGuiMouseButton button      = luaL_checkinteger(lua, index_input++);
    const bool ret_bool          = ImGui::IsMouseDown(button);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsMouseClickedImGuiLua(lua_State *lua)
{
    //  Did mouse button clicked? (went from !Down to Down)
    int index_input              = 1;
    const int top                = lua_gettop(lua);
    ImGuiMouseButton button      = luaL_checkinteger(lua, index_input++);
    const bool repeat            = top >= index_input ? lua_toboolean(lua,index_input++) :  false;
    const bool ret_bool          = ImGui::IsMouseClicked(button,repeat);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsMouseReleasedImGuiLua(lua_State *lua)
{
    //  Did mouse button released? (went from Down to !Down)
    int index_input              = 1;
    ImGuiMouseButton     button  = luaL_checkinteger(lua, index_input++);
    const bool ret_bool          = ImGui::IsMouseReleased(button);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsMouseDoubleClickedImGuiLua(lua_State *lua)
{
    //  Did mouse button double-clicked? a double-click returns false in IsMouseClicked(). uses io.MouseDoubleClickTime.
    int index_input              = 1;
    ImGuiMouseButton button      = luaL_checkinteger(lua, index_input++);
    const bool ret_bool          = ImGui::IsMouseDoubleClicked(button);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsMouseHoveringRectImGuiLua(lua_State *lua)
{
    //  Is mouse hovering given bounding rect (in screen space). clipped by current clipping settings, but disregarding of other consideration of focus/window ordering/popup-block.
    int index_input      = 1;
    const int top        = lua_gettop(lua);
    ImVec2 r_min(0,0);
    lua_pop_ImVec2_pointer(lua, index_input++, &r_min);
    ImVec2 r_max(0,0);
    lua_pop_ImVec2_pointer(lua, index_input++, &r_max);
    const bool clip      = top >= index_input ? lua_toboolean(lua,index_input++) :  true;
    const bool ret_bool  = ImGui::IsMouseHoveringRect(r_min,r_max,clip);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsMousePosValidImGuiLua(lua_State *lua)
{
    //  By convention we use (-FLT_MAX,-FLT_MAX) to denote that there is no mouse available
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const ImVec2 mouse_pos = top >= index_input ? lua_pop_ImVec2(lua, index_input++) :  ImVec2(0,0);
    const bool ret_bool    = ImGui::IsMousePosValid(&mouse_pos);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsAnyMouseDownImGuiLua(lua_State *lua)
{
    //  Is any mouse button held?
    const bool ret_bool  = ImGui::IsAnyMouseDown();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onGetMousePosImGuiLua(lua_State *lua)
{
    //  Shortcut to ImGui::GetIO().MousePos provided by user, to be consistent with other calls
    const ImVec2 ret_ImVec2  = ImGui::GetMousePos();
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

int onGetMousePosOnOpeningCurrentPopupImGuiLua(lua_State *lua)
{
    //  Retrieve mouse position at the time of opening popup we have BeginPopup() into (helper to avoid user backing that value themselves)
    const ImVec2 ret_ImVec2  = ImGui::GetMousePosOnOpeningCurrentPopup();
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

int onIsMouseDraggingImGuiLua(lua_State *lua)
{
    //  Is mouse dragging? (if lock_threshold < -1.0f, uses io.MouseDraggingThreshold)
    int index_input              = 1;
    const int top                = lua_gettop(lua);
    ImGuiMouseButton button      = luaL_checkinteger(lua, index_input++);
    const float lock_threshold   = top >= index_input ? luaL_checknumber(lua,index_input++) :  -1.0f;
    const bool ret_bool          = ImGui::IsMouseDragging(button,lock_threshold);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onGetMouseDragDeltaImGuiLua(lua_State *lua)
{
    //  Return the delta from the initial clicking position while the mouse button is pressed or was just released. This is locked and return 0.0f until the mouse moves past a distance threshold at least once (if lock_threshold < -1.0f, uses io.MouseDraggingThreshold)
    int index_input              = 1;
    const int top                = lua_gettop(lua);
    ImGuiMouseButton button      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    const float lock_threshold   = top >= index_input ? luaL_checknumber(lua,index_input++) :  -1.0f;
    const ImVec2 ret_ImVec2      = ImGui::GetMouseDragDelta(button,lock_threshold);
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

int onResetMouseDragDeltaImGuiLua(lua_State *lua)
{
    // 
    int index_input              = 1;
    const int top                = lua_gettop(lua);
    ImGuiMouseButton button      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    ImGui::ResetMouseDragDelta(button);
    return 0;
}

int onGetMouseCursorImGuiLua(lua_State *lua)
{
    //  Get the cursor type as string, reset in ImGui::NewFrame(), this is updated during the frame. valid before Render(). If you use software rendering by setting io.MouseDrawCursor ImGui will render those for you
    const ImGuiMouseCursor ret_ImGuiMouseCursor  = ImGui::GetMouseCursor();
    for (auto it = enumMouseCursorMap.cbegin(); it != enumMouseCursorMap.cend(); ++it)
    {
        if(it->second == ret_ImGuiMouseCursor)
        {
            lua_pushstring(lua,it->first.c_str());
            break;
        }
    }
    return 1;
}

int onSetMouseCursorImGuiLua(lua_State *lua)
{
    //  Set desired cursor type
    ImGuiMouseCursor cursor_type(ImGuiMouseCursor_Arrow);
    if(lua_type(lua,1) == LUA_TSTRING)
    {
        const char * mouseCursor = luaL_checkstring(lua,1);
        const auto it = enumMouseCursorMap.find(mouseCursor);
        if(it == enumMouseCursorMap.end())
        {
            std::string msg("ImGuiMouseCursor not found [");
            msg += mouseCursor;
            msg += ']';
            lua_log_error(lua,msg.c_str());
        }
        else
        {
            cursor_type = it->second;
        }
    }
    else
    {
        cursor_type  = luaL_checkinteger(lua,1);
    }
    ImGui::SetMouseCursor(cursor_type);
    return 0;
}

int onCaptureMouseFromAppImGuiLua(lua_State *lua)
{
    //  Manually override io.WantCaptureMouse flag next frame (said flag is entirely left for your application to handle).
    int index_input                      = 1;
    const int top                        = lua_gettop(lua);
    const bool want_capture_mouse_value  = top >= index_input ? lua_toboolean(lua,index_input++) :  true;
    ImGuiIO& io = ImGui::GetIO();
    io.WantCaptureMouse = want_capture_mouse_value;
    return 0;
}

int onGetClipboardTextImGuiLua(lua_State *lua)
{
    const char *ret_char  = ImGui::GetClipboardText();
    lua_pushstring(lua,ret_char);
    return 1;
}

int onSetClipboardTextImGuiLua(lua_State *lua)
{
    int index_input      = 1;
    const char * p_text  = luaL_checkstring(lua,index_input++);
    ImGui::SetClipboardText(p_text);
    return 0;
}

// IniSettings functions removed - Load/SaveIniSettingsFromDisk/Memory not used

int onTreeAdvanceToLabelPosImGuiLua(lua_State *lua)
{
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetTreeNodeToLabelSpacing());
    return 0;
}

int onSetNextTreeNodeOpenImGuiLua(lua_State *lua)
{
    int index_input     = 1;
    const int top       = lua_gettop(lua);
    const bool open     = lua_toboolean(lua,index_input++);
    ImGuiCond cond      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    ImGui::SetNextItemOpen(open,cond);
    return 0;
}

// REMOVED: GetContentRegionAvailWidth - deprecated in ImGui 1.92
// Use GetContentRegionAvail().x instead

int onSetScrollHereImGuiLua(lua_State *lua)
{
    int index_input           = 1;
    const int top             = lua_gettop(lua);
    const float center_ratio  = top >= index_input ? luaL_checknumber(lua,index_input++) : 0.5f;
    ImGui::SetScrollHereY(center_ratio);
    return 0;
}

int onIsItemDeactivatedAfterChangeImGuiLua(lua_State *lua)
{
    const bool ret_bool  = ImGui::IsItemDeactivatedAfterEdit();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onInputFloat2ImGuiLua(lua_State *lua)
{
    int index_input                = 1;
    const int top                  = lua_gettop(lua);
    const char * p_label           = luaL_checkstring(lua,index_input++);
    float values[2]                = {0.0f,0.0f};
    get_float_arrayFromTable(lua,index_input++,values,sizeof(values) / sizeof(values[0]) ,"float table[2]");
    const char * p_format          = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    ImGuiInputTextFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiInputTextFlags_None, inputTextFlagsMap);
    const bool ret_bool            = ImGui::InputFloat2(p_label,values,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    push_float_arrayFromTable(lua,values,sizeof(values) / sizeof(values[0]));
    return 2;
}
   
int onInputFloat3ImGuiLua(lua_State *lua)
{
    int index_input                = 1;
    const int top                  = lua_gettop(lua);
    const char * p_label           = luaL_checkstring(lua,index_input++);
    float values[3]                = {0.0f,0.0f,0.0f};
    get_float_arrayFromTable(lua,index_input++,values,sizeof(values) / sizeof(values[0]) ,"float table[3]");
    const char * p_format          = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    ImGuiInputTextFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiInputTextFlags_None, inputTextFlagsMap);
    const bool ret_bool            = ImGui::InputFloat3(p_label,values,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    push_float_arrayFromTable(lua,values,sizeof(values) / sizeof(values[0]));
    return 2;
}

int onInputFloat4ImGuiLua(lua_State *lua)
{
    int index_input                = 1;
    const int top                  = lua_gettop(lua);
    const char * p_label           = luaL_checkstring(lua,index_input++);
    float values[4]                = {0.0f,0.0f,0.0f,0.0f};
    get_float_arrayFromTable(lua,index_input++,values,sizeof(values) / sizeof(values[0]) ,"float table[4]");
    const char * p_format          = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    ImGuiInputTextFlags flags      = lua_opt_flags(lua, top, index_input, ImGuiInputTextFlags_None, inputTextFlagsMap);
    const bool ret_bool            = ImGui::InputFloat4(p_label,values,p_format,flags);
    lua_pushboolean(lua,ret_bool);
    push_float_arrayFromTable(lua,values,sizeof(values) / sizeof(values[0]));
    return 2;
}

int onIsAnyWindowFocusedImGuiLua(lua_State *lua)
{
    const bool ret_bool  = ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsAnyWindowHoveredImGuiLua(lua_State *lua)
{
    const bool ret_bool  = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onHelpMarkerLua(lua_State *lua)
{
    const int top       = lua_gettop(lua);
    const char * desc   = luaL_checkstring(lua,1);
    const char * mark   = top >= 2 ? luaL_checkstring(lua,2) : "(?)";
    ImGui::TextDisabled("%s",mark);
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
    return 0;
}

int onPushTextureIDImDrawListLua(lua_State *lua)
{
    // NOTE: ImDrawList::PushTextureID() was removed in ImGui 1.89+.
    // Textures are now passed directly to each draw call (AddImage, etc.).
    // This function is now a no-op for backward compatibility.
    return 0;
}
    
int onPopTextureIDImDrawListLua(lua_State *lua)
{
    // NOTE: ImDrawList::PopTextureID() was removed in ImGui 1.89+.
    // This function is now a no-op for backward compatibility.
    return 0;
}
    
int onAddLineImDrawListLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const ImVec2 p1        = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p2        = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color      = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    const float thickness  = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    ImDrawList* draw_list  = GetImDrawListLua();
    draw_list->AddLine(p1,p2,color,thickness);
    return 0;
}

int onImDrawListToBackgroundLua(lua_State *lua)
{
    bDrawListToBackground = lua_toboolean(lua,1);
    return 0;
}

int onImDrawListToForegroundLua(lua_State *lua)
{
    bDrawListToForeground = lua_toboolean(lua,1);
    return 0;
}
    
int onAddRectImDrawListLua(lua_State *lua)
{
    //  A: upper-left, b: lower-right (== upper-left + size), rounding_corners_flags: 4 bits corresponding to which corner to round
    int index_input                         = 1;
    const int top                           = lua_gettop(lua);
    const ImVec2 p_min                      = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p_max                      = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color                       = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    const float rounding                    = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    ImDrawFlags rounding_corners            = lua_opt_flags(lua, top, index_input, ImDrawFlags_RoundCornersAll, drawFlagsMap);
    const float thickness                   = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    ImDrawList* draw_list                   = GetImDrawListLua();
    draw_list->AddRect(p_min,p_max,color,rounding,rounding_corners,thickness);
    return 0;
}
    
int onAddRectFilledImDrawListLua(lua_State *lua)
{
    //  A: upper-left, b: lower-right (== upper-left + size)
    int index_input                         = 1;
    const int top                           = lua_gettop(lua);
    const ImVec2 p_min                      = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p_max                      = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color                       = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    const float rounding                    = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    ImDrawFlags   rounding_corners          = lua_opt_flags(lua, top, index_input, ImDrawFlags_RoundCornersAll, drawFlagsMap);
    ImDrawList* draw_list                   = GetImDrawListLua();
    draw_list->AddRectFilled(p_min,p_max,color,rounding,rounding_corners);
    return 0;
}
    
int onAddRectFilledMultiColorImDrawListLua(lua_State *lua)
{
    int index_input           = 1;
    const ImVec2 p_min        = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p_max        = lua_pop_ImVec2(lua, index_input++);
    const ImU32 col_upr_left  = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    const ImU32 col_upr_right = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    const ImU32 col_bot_right = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    const ImU32 col_bot_left  = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    ImDrawList* draw_list     = GetImDrawListLua();
    draw_list->AddRectFilledMultiColor(p_min,p_max,col_upr_left,col_upr_right,col_bot_right,col_bot_left);
    return 0;
}
    
int onAddQuadImDrawListLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const ImVec2 p1        = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p2        = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p3        = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p4        = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color      = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    const float thickness  = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    ImDrawList* draw_list  = GetImDrawListLua();
    draw_list->AddQuad(p1,p2,p3,p4,color,thickness);
    return 0;
}
    
int onAddQuadFilledImDrawListLua(lua_State *lua)
{
    int index_input  = 1;
    const ImVec2 p1           = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p2           = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p3           = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p4           = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color         = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    ImDrawList* draw_list     = GetImDrawListLua();
    draw_list->AddQuadFilled(p1,p2,p3,p4,color);
    return 0;
}
    
int onAddTriangleImDrawListLua(lua_State *lua)
{
    int index_input         = 1;
    const int top           = lua_gettop(lua);
    const ImVec2 p1         = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p2         = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p3         = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color       = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    const float thickness   = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    ImDrawList* draw_list   = GetImDrawListLua();
    draw_list->AddTriangle(p1,p2,p3,color,thickness);
    return 0;
}
    
int onAddTriangleFilledImDrawListLua(lua_State *lua)
{
    int index_input           = 1;
    const ImVec2 p1           = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p2           = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p3           = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color         = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    ImDrawList* draw_list     = GetImDrawListLua();
    draw_list->AddTriangleFilled(p1,p2,p3,color);
    return 0;
}
    
int onAddCircleImDrawListLua(lua_State *lua)
{
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    const ImVec2 center         = lua_pop_ImVec2(lua, index_input++);
    const float radius          = luaL_checknumber(lua,index_input++);
    const ImU32 color           = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    const int num_segments      = top >= index_input ? luaL_checkinteger(lua,index_input++) :  12;
    const float thickness       = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    ImDrawList* draw_list       = GetImDrawListLua();
    draw_list->AddCircle(center,radius,color,num_segments,thickness);
    return 0;
}
    
int onAddCircleFilledImDrawListLua(lua_State *lua)
{
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    const ImVec2 center         = lua_pop_ImVec2(lua, index_input++);
    const float radius          = luaL_checknumber(lua,index_input++);
    const ImU32 color           = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    const int num_segments      = top >= index_input ? luaL_checkinteger(lua,index_input++) :  12;
    ImDrawList* draw_list       = GetImDrawListLua();
    draw_list->AddCircleFilled(center,radius,color,num_segments);
    return 0;
}
    
int onAddNgonImDrawListLua(lua_State *lua)
{
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    const ImVec2 center         = lua_pop_ImVec2(lua, index_input++);
    const float radius          = luaL_checknumber(lua,index_input++);
    const ImU32 color           = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    const int num_segments      = luaL_checkinteger(lua,index_input++);
    const float thickness       = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    ImDrawList* draw_list       = GetImDrawListLua();
    draw_list->AddNgon(center,radius,color,num_segments,thickness);
    return 0;
}
    
int onAddNgonFilledImDrawListLua(lua_State *lua)
{
    int index_input             = 1;
    const ImVec2 center         = lua_pop_ImVec2(lua, index_input++);
    const float radius          = luaL_checknumber(lua,index_input++);
    const ImU32 color           = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    const int num_segments      = luaL_checkinteger(lua,index_input++);
    ImDrawList* draw_list       = GetImDrawListLua();
    draw_list->AddNgonFilled(center,radius,color,num_segments);
    return 0;
}
    
int onAddTextImDrawListLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const ImVec2 pos           = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color          = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    const char * p_text_begin  = luaL_checkstring(lua,index_input++);
    const char * p_text_end    = top >= index_input ? lua_tostring(lua,index_input++) :  nullptr;
    ImDrawList* draw_list      = GetImDrawListLua();
    draw_list->AddText(pos,color,p_text_begin,p_text_end);
    return 0;
}
    
int onAddPolylineImDrawListLua(lua_State *lua)
{
    std::vector<ImVec2> points;
    int index_input             = 1;
    get_ImVec2_arrayFromTable(lua, index_input++,points,"ImVec2_points");
    const ImVec4 color          = lua_get_rgba_to_ImVec4_fromTable(lua,index_input++);
    const bool closed           = lua_toboolean(lua,index_input++);
    const float thickness       = luaL_checknumber(lua,index_input++);
    ImDrawList* draw_list       = GetImDrawListLua();
    draw_list->AddPolyline(points.data(),points.size(),ImGui::GetColorU32(color),closed,thickness);
    return 0;
}
    
int onAddConvexPolyFilledImDrawListLua(lua_State *lua)
{
    //  Note: Anti-aliased filling requires points to be in clockwise order.
    std::vector<ImVec2> points;
    int index_input              = 1;
    get_ImVec2_arrayFromTable(lua, index_input++,points,"ImVec2_points");
    const ImVec4 color           = lua_get_rgba_to_ImVec4_fromTable(lua,index_input++);
    ImDrawList* draw_list        = GetImDrawListLua();
    draw_list->AddConvexPolyFilled(points.data(),points.size(),ImGui::GetColorU32(color));
    return 0;
}
    
int onAddBezierCubicImDrawListLua(lua_State *lua)
{
    int index_input          = 1;
    const int top            = lua_gettop(lua);
    const ImVec2 p1          = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p2          = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p3          = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p4          = lua_pop_ImVec2(lua, index_input++);
    const ImVec4 color       = lua_get_rgba_to_ImVec4_fromTable(lua,index_input++);
    const float thickness    = luaL_checknumber(lua,index_input++);
    const int num_segments   = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
    ImDrawList* draw_list    = GetImDrawListLua();
    draw_list->AddBezierCubic(p1,p2,p3,p4,ImGui::GetColorU32(color),thickness,num_segments);
    return 0;
}
    
int onAddImageImDrawListLua(lua_State *lua)
{
    int index_input                 = 1;
    const int top                   = lua_gettop(lua);
    unsigned int tex_width          = 0;
    unsigned int tex_height         = 0;
    ImTextureID user_texture_id     = get_imgui_texture_id(lua, index_input, tex_width, tex_height);
    const ImVec2 p_min              = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p_max              = lua_pop_ImVec2(lua, index_input++);
    const ImU32 col                 = top >= index_input ? ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++)) :  IM_COL32_WHITE;
    const ImVec2 uv_min             = top >= index_input ? lua_pop_ImVec2(lua, index_input++) :  ImVec2(0, 0);
    const ImVec2 uv_max             = top >= index_input ? lua_pop_ImVec2(lua, index_input++) :  ImVec2(1, 1);
    ImDrawList* draw_list           = GetImDrawListLua();
    draw_list->AddImage(user_texture_id,p_min,p_max,uv_min,uv_max,col);
    return 0;
}

int onAddImageQuadImDrawListLua(lua_State *lua)
{
    int index_input                 = 1;
    const int top                   = lua_gettop(lua);
    unsigned int tex_width          = 0;
    unsigned int tex_height         = 0;
    ImTextureID user_texture_id     = get_imgui_texture_id(lua, index_input, tex_width, tex_height);
    const ImVec2 p1                 = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p2                 = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p3                 = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p4                 = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color               = top >= index_input ? ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++))  :  IM_COL32_WHITE;
    const ImVec2 uv1                = top >= index_input ? lua_pop_ImVec2(lua, index_input++) :  ImVec2(0, 0);
    const ImVec2 uv2                = top >= index_input ? lua_pop_ImVec2(lua, index_input++) :  ImVec2(1, 0);
    const ImVec2 uv3                = top >= index_input ? lua_pop_ImVec2(lua, index_input++) :  ImVec2(1, 1);
    const ImVec2 uv4                = top >= index_input ? lua_pop_ImVec2(lua, index_input++) :  ImVec2(0, 1);
    ImDrawList* draw_list           = GetImDrawListLua();
    draw_list->AddImageQuad(user_texture_id,p1,p2,p3,p4,uv1,uv2,uv3,uv4,color);
    return 0;
}
    
int onAddImageRoundedImDrawListLua(lua_State* lua)
{
    int index_input = 1;
    const int top = lua_gettop(lua);
    unsigned int tex_width = 0;
    unsigned int tex_height = 0;
    ImTextureID user_texture_id = get_imgui_texture_id(lua, index_input, tex_width, tex_height);
    const ImVec2 p_min = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p_max = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua, index_input++));
    const ImVec2 uv_min = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(0, 0);
    const ImVec2 uv_max = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(1, 1);
    const float rounding = top >= index_input ? luaL_checknumber(lua, index_input++) : 0.0f;
    ImDrawFlags rounding_corners = lua_opt_flags(lua, top, index_input, ImDrawFlags_RoundCornersAll, drawFlagsMap);
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->AddImageRounded(user_texture_id, p_min, p_max, uv_min, uv_max, color, rounding, rounding_corners);
    return 0;
}
   
int onAddDrawCmdImDrawListLua(lua_State *lua)
{
    //  This is useful if you need to forcefully create a new draw call (to allow for dependent rendering / blending). Otherwise primitives are merged into the same draw-call as much as possible
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->AddDrawCmd();
    return 0;
}

int onNewimguiLua(lua_State *lua)
{
    lua_settop(lua, 0);
    luaL_Reg regimguiMethods[]  = {  
        {"AlignTextToFramePadding",                   onAlignTextToFramePaddingImGuiLua }, // Not Tested, Window/Layout
        {"ArrowButton",                                           onArrowButtonImGuiLua },
        {"Begin",                                                       onBeginImGuiLua },
        {"BeginChild",                                             onBeginChildImGuiLua }, // Not Tested, Window/Layout
        // BeginChildFrame removed - deprecated in ImGui 1.92, use BeginChild with styling instead
        {"BeginCombo",                                             onBeginComboImGuiLua }, // Not Tested, Window/Layout
        {"BeginGroup",                                             onBeginGroupImGuiLua }, // Not Tested, Window/Layout
        {"BeginMainMenuBar",                                 onBeginMainMenuBarImGuiLua },
        {"BeginMenu",                                               onBeginMenuImGuiLua },
        {"BeginMenuBar",                                         onBeginMenuBarImGuiLua },
        {"BeginPopup",                                             onBeginPopupImGuiLua }, // Not Tested, Window/Layout
        {"BeginPopupContextItem",                       onBeginPopupContextItemImGuiLua },
        {"BeginPopupContextVoid",                       onBeginPopupContextVoidImGuiLua },
        {"BeginPopupContextWindow",                   onBeginPopupContextWindowImGuiLua }, // Not Tested, Window/Layout
        {"BeginPopupModal",                                   onBeginPopupModalImGuiLua },
        {"BeginTable",                                           onBeginTableImGuiLua },
        {"BeginTabBar",                                           onBeginTabBarImGuiLua },
        {"BeginTabItem",                                         onBeginTabItemImGuiLua },
        {"BeginTooltip",                                         onBeginTooltipImGuiLua },
        {"Bullet",                                                     onBulletImGuiLua }, // Not Tested, Text/Labels
        {"BulletText",                                             onBulletTextImGuiLua }, // Not Tested, Text/Labels
        {"Button",                                                     onButtonImGuiLua },
        {"CalcItemWidth",                                       onCalcItemWidthImGuiLua }, // Not Tested, Input Widgets
        // CalcListClipping removed - deprecated in ImGui 1.92, use ImGuiListClipper instead
        {"CalcTextSize",                                         onCalcTextSizeImGuiLua }, // Not Tested, Queries/State
        {"CaptureKeyboardFromApp",                     onCaptureKeyboardFromAppImGuiLua }, // Not Tested, Keyboard/Mouse
        {"CaptureMouseFromApp",                           onCaptureMouseFromAppImGuiLua }, // Not Tested, Keyboard/Mouse
        {"Checkbox",                                                 onCheckboxImGuiLua },
        {"CheckboxFlags",                                       onCheckboxFlagsImGuiLua }, // Not Tested, Input Widgets
        {"CloseCurrentPopup",                               onCloseCurrentPopupImGuiLua },
        {"CollapsingHeader",                                 onCollapsingHeaderImGuiLua }, // Not Tested, List/Plotting
        {"ColorButton",                                           onColorButtonImGuiLua }, // Not Tested, Color Widgets
        {"ColorConvertFloat4ToU32",                   onColorConvertFloat4ToU32ImGuiLua }, // Not Tested, Color Widgets
        {"ColorConvertHSVtoRGB",                         onColorConvertHSVtoRGBImGuiLua }, // Not Tested, Color Widgets
        {"ColorConvertRGBtoHSV",                         onColorConvertRGBtoHSVImGuiLua }, // Not Tested, Color Widgets
        {"ColorConvertU32ToFloat4",                   onColorConvertU32ToFloat4ImGuiLua }, // Not Tested, Color Widgets
        {"ColorEdit3",                                             onColorEdit3ImGuiLua },
        {"ColorEdit4",                                             onColorEdit4ImGuiLua },
        {"ColorPicker3",                                         onColorPicker3ImGuiLua }, // Not Tested, Color Widgets
        {"ColorPicker4",                                         onColorPicker4ImGuiLua }, // Not Tested, Color Widgets
        // Columns API removed - deprecated in ImGui 1.92+, use Tables API instead
        {"Combo",                                                       onComboImGuiLua },
        {"DragFloat",                                               onDragFloatImGuiLua }, // Not Tested, Input Widgets
        {"DragFloat2",                                             onDragFloat2ImGuiLua }, // Not Tested, Input Widgets
        {"DragFloat3",                                             onDragFloat3ImGuiLua }, // Not Tested, Input Widgets
        {"DragFloat4",                                             onDragFloat4ImGuiLua }, // Not Tested, Input Widgets
        {"DragFloatRange2",                                   onDragFloatRange2ImGuiLua }, // Not Tested, Input Widgets
        {"DragInt",                                                   onDragIntImGuiLua },
        {"DragInt2",                                                 onDragInt2ImGuiLua }, // Not Tested, Input Widgets
        {"DragInt3",                                                 onDragInt3ImGuiLua }, // Not Tested, Input Widgets
        {"DragInt4",                                                 onDragInt4ImGuiLua }, // Not Tested, Input Widgets
        {"DragIntRange2",                                       onDragIntRange2ImGuiLua }, // Not Tested, Input Widgets
        {"Dummy",                                                       onDummyImGuiLua },
        {"End",                                                           onEndImGuiLua },
        {"EndChild",                                                 onEndChildImGuiLua }, // Not Tested, Window/Layout
        // EndChildFrame removed - deprecated in ImGui 1.92, use EndChild instead
        {"EndCombo",                                                 onEndComboImGuiLua }, // Not Tested, Window/Layout
        {"EndGroup",                                                 onEndGroupImGuiLua }, // Not Tested, Window/Layout
        {"EndMainMenuBar",                                     onEndMainMenuBarImGuiLua },
        {"EndMenu",                                                   onEndMenuImGuiLua },
        {"EndMenuBar",                                             onEndMenuBarImGuiLua },
        {"EndPopup",                                                 onEndPopupImGuiLua },
        {"EndTable",                                                 onEndTableImGuiLua },
        {"EndTabBar",                                               onEndTabBarImGuiLua },
        {"EndTabItem",                                             onEndTabItemImGuiLua },
        {"EndTooltip",                                             onEndTooltipImGuiLua },
        {"Flags",                                                  onMakeFlagsImGuiLua  },
        {"FlagList",                                               onListFlagsImGuiLua  }, // Not Tested, Queries/State
        {"GetClipboardText",                                 onGetClipboardTextImGuiLua }, // Not Tested, Queries/State
        {"GetColorU32",                                           onGetColorU32ImGuiLua }, // Not Tested, Queries/State
        // GetColumn* removed - deprecated Columns API, use Tables API instead
        {"GetContentRegionAvail",                       onGetContentRegionAvailImGuiLua }, // Not Tested, Queries/State
        // GetContentRegionAvailWidth, GetContentRegionMax removed - deprecated in ImGui 1.89+
        {"GetCursorPos",                                         onGetCursorPosImGuiLua },
        {"GetCursorPosX",                                       onGetCursorPosXImGuiLua },
        {"GetCursorPosY",                                       onGetCursorPosYImGuiLua }, // Not Tested, Queries/State
        {"GetCursorScreenPos",                             onGetCursorScreenPosImGuiLua },
        {"GetCursorStartPos",                               onGetCursorStartPosImGuiLua }, // Not Tested, Queries/State
        {"GetItemRectMax",                                     onGetItemRectMaxImGuiLua }, // Not Tested, Queries/State
        {"GetItemRectMin",                                     onGetItemRectMinImGuiLua }, // Not Tested, Queries/State
        {"GetItemRectSize",                                   onGetItemRectSizeImGuiLua }, // Not Tested, Queries/State
        {"GetKeyPressedAmount",                           onGetKeyPressedAmountImGuiLua }, // Not Tested, Queries/State
        {"GetMainMenuBarHeight",                         onGetMainMenuBarHeightImGuiLua },
        {"GetMouseCursor",                                     onGetMouseCursorImGuiLua }, // Not Tested, Queries/State
        {"GetMouseDragDelta",                               onGetMouseDragDeltaImGuiLua }, // Not Tested, Queries/State
        {"GetMousePos",                                           onGetMousePosImGuiLua },
        {"GetMousePosOnOpeningCurrentPopup", onGetMousePosOnOpeningCurrentPopupImGuiLua }, // Not Tested, Queries/State
        {"GetScrollMaxX",                                       onGetScrollMaxXImGuiLua }, // Not Tested, Queries/State
        {"GetScrollMaxY",                                       onGetScrollMaxYImGuiLua }, // Not Tested, Queries/State
        {"GetScrollX",                                             onGetScrollXImGuiLua }, // Not Tested, Queries/State
        {"GetScrollY",                                             onGetScrollYImGuiLua },
        {"GetStyle",                                                 onGetStyleImGuiLua },
        // GetStyleColorName removed - not used
        {"GetStyleColorVec4",                               onGetStyleColorVec4ImGuiLua }, // Not Tested, Queries/State
        {"GetTextLineHeight",                               onGetTextLineHeightImGuiLua },
        {"GetTextLineHeightWithSpacing",         onGetTextLineHeightWithSpacingImGuiLua }, // Not Tested, Queries/State
        // GetTime removed - not used
        {"GetTreeNodeToLabelSpacing",               onGetTreeNodeToLabelSpacingImGuiLua }, // Not Tested, Queries/State
        {"GetVersion",                                             onGetVersionImGuiLua },
        // GetWindowContentRegionMin/Max removed - deprecated in ImGui 1.89+
        {"GetWindowHeight",                                   onGetWindowHeightImGuiLua }, // Not Tested, Queries/State
        {"GetWindowPos",                                         onGetWindowPosImGuiLua },
        {"GetWindowSize",                                       onGetWindowSizeImGuiLua },
        {"GetWindowWidth",                                     onGetWindowWidthImGuiLua }, // Not Tested, Queries/State
        {"GetZoom",                                                   onGetZoomImGuiLua },
        {"HelpMarker",                                                  onHelpMarkerLua },
        {"Image",                                                       onImageImGuiLua },
        {"ImageQuad",                                               onImageQuadImGuiLua },
        {"ImageButton",                                           onImageButtonImGuiLua },
        {"Indent",                                                     onIndentImGuiLua }, // Not Tested, Window/Layout
        {"InputDouble",                                           onInputDoubleImGuiLua }, // Not Tested, Input Widgets
        {"InputFloat",                                             onInputFloatImGuiLua },
        {"InputFloat2",                                           onInputFloat2ImGuiLua }, // Not Tested, Input Widgets
        {"InputFloat3",                                           onInputFloat3ImGuiLua }, // Not Tested, Input Widgets
        {"InputFloat4",                                           onInputFloat4ImGuiLua }, // Not Tested, Input Widgets
        {"InputInt",                                                 onInputIntImGuiLua },
        {"InputInt2",                                               onInputInt2ImGuiLua }, // Not Tested, Input Widgets
        {"InputInt3",                                               onInputInt3ImGuiLua }, // Not Tested, Input Widgets
        {"InputInt4",                                               onInputInt4ImGuiLua }, // Not Tested, Input Widgets
        {"InputText",                                               onInputTextImGuiLua },
        {"InputTextMultiline",                             onInputTextMultilineImGuiLua },
        {"InputTextWithHint",                               onInputTextWithHintImGuiLua },
        {"InvisibleButton",                                   onInvisibleButtonImGuiLua }, // Not Tested, Item State
        {"IsAnyItemActive",                                   onIsAnyItemActiveImGuiLua },
        {"IsAnyItemFocused",                                 onIsAnyItemFocusedImGuiLua }, // Not Tested, Item State
        {"IsAnyItemHovered",                                 onIsAnyItemHoveredImGuiLua }, // Not Tested, Item State
        {"IsAnyMouseDown",                                     onIsAnyMouseDownImGuiLua }, // Not Tested, Item State
        {"IsAnyWindowFocused",                             onIsAnyWindowFocusedImGuiLua }, // Not Tested, Item State
        {"IsAnyWindowHovered",                             onIsAnyWindowHoveredImGuiLua },
        {"IsItemActivated",                                   onIsItemActivatedImGuiLua }, // Not Tested, Item State
        {"IsItemActive",                                         onIsItemActiveImGuiLua }, // Not Tested, Item State
        {"IsItemClicked",                                       onIsItemClickedImGuiLua },
        {"IsItemDeactivated",                               onIsItemDeactivatedImGuiLua }, // Not Tested, Item State
        {"IsItemDeactivatedAfterChange",         onIsItemDeactivatedAfterChangeImGuiLua }, // Not Tested, Item State
        {"IsItemDeactivatedAfterEdit",             onIsItemDeactivatedAfterEditImGuiLua }, // Not Tested, Item State
        {"IsItemEdited",                                         onIsItemEditedImGuiLua }, // Not Tested, Item State
        {"IsItemFocused",                                       onIsItemFocusedImGuiLua }, // Not Tested, Item State
        {"IsItemHovered",                                       onIsItemHoveredImGuiLua },
        {"IsItemToggledOpen",                               onIsItemToggledOpenImGuiLua }, // Not Tested, Item State
        {"IsItemVisible",                                       onIsItemVisibleImGuiLua }, // Not Tested, Item State
        {"IsKeyDown",                                               onIsKeyDownImGuiLua },
        {"IsKeyPressed",                                         onIsKeyPressedImGuiLua }, // Not Tested, Item State
        {"IsKeyReleased",                                       onIsKeyReleasedImGuiLua }, // Not Tested, Item State
        {"IsMouseClicked",                                     onIsMouseClickedImGuiLua },
        {"IsMouseDoubleClicked",                         onIsMouseDoubleClickedImGuiLua }, // Not Tested, Item State
        {"IsMouseDown",                                           onIsMouseDownImGuiLua },
        {"IsMouseDragging",                                   onIsMouseDraggingImGuiLua }, // Not Tested, Item State
        {"IsMouseHoveringRect",                           onIsMouseHoveringRectImGuiLua },
        {"IsMousePosValid",                                   onIsMousePosValidImGuiLua }, // Not Tested, Item State
        {"IsMouseReleased",                                   onIsMouseReleasedImGuiLua },
        {"IsPopupOpen",                                           onIsPopupOpenImGuiLua }, // Not Tested, Item State
        {"IsRectVisible",                                       onIsRectVisibleImGuiLua }, // Not Tested, Item State
        {"IsScrollVisible",                                   onIsScrollVisibleImGuiLua },
        {"IsWindowAppearing",                               onIsWindowAppearingImGuiLua }, // Not Tested, Item State
        {"IsWindowCollapsed",                               onIsWindowCollapsedImGuiLua }, // Not Tested, Item State
        {"IsWindowFocused",                                   onIsWindowFocusedImGuiLua },
        {"IsWindowHovered",                                   onIsWindowHoveredImGuiLua },
        {"LabelText",                                               onLabelTextImGuiLua }, // Not Tested, Text/Labels
        {"BeginListBox",                                         onBeginListBoxImGuiLua }, // Not Tested, List/Plotting
        {"EndListBox",                                             onEndListBoxImGuiLua }, // Not Tested, List/Plotting
        {"ListBox",                                                   onListBoxImGuiLua }, // Not Tested, List/Plotting
        {"LogButtons",                                             onLogButtonsImGuiLua }, // Not Tested, Logging
        {"LogFinish",                                               onLogFinishImGuiLua }, // Not Tested, Logging
        {"LogText",                                                   onLogTextImGuiLua }, // Not Tested, Logging
        {"LogToClipboard",                                     onLogToClipboardImGuiLua }, // Not Tested, Logging
        {"LogToFile",                                               onLogToFileImGuiLua }, // Not Tested, Logging
        {"LogToTTY",                                                 onLogToTTYImGuiLua }, // Not Tested, Logging
        {"MenuItem",                                                 onMenuItemImGuiLua },
        {"NewLine",                                                   onNewLineImGuiLua },
        {"OpenPopup",                                               onOpenPopupImGuiLua },
        {"OpenPopupOnItemClick",                         onOpenPopupOnItemClickImGuiLua }, // Not Tested, Popups
        {"PlotHistogram",                                       onPlotHistogramImGuiLua }, // Not Tested, List/Plotting
        {"PlotLines",                                               onPlotLinesImGuiLua }, // Not Tested, List/Plotting
        // PopTabStop, PopButtonRepeat removed - deprecated in ImGui 1.90+, use PopItemFlag
        {"PopItemFlag",                                           onPopItemFlagImGuiLua },   // Not Tested, Stack/State, NEW: Generic item flag function
        {"PopClipRect",                                           onPopClipRectImGuiLua }, // Not Tested, Stack/State
        {"PopFont",                                                   onPopFontImGuiLua }, // Not Tested, Stack/State
        {"PopID",                                                       onPopIDImGuiLua }, // Not Tested, Stack/State
        {"PopItemWidth",                                         onPopItemWidthImGuiLua },
        {"PopStyleColor",                                       onPopStyleColorImGuiLua },
        {"PopStyleVar",                                           onPopStyleVarImGuiLua },
        {"PopTextWrapPos",                                     onPopTextWrapPosImGuiLua }, // Not Tested, Stack/State
        {"ProgressBar",                                           onProgressBarImGuiLua },
        // PushTabStop, PushButtonRepeat removed - deprecated in ImGui 1.90+, use PushItemFlag
        {"PushItemFlag",                                         onPushItemFlagImGuiLua },  // Not Tested, Stack/State, NEW: Generic item flag function
        {"PushClipRect",                                         onPushClipRectImGuiLua }, // Not Tested, Stack/State
        {"PushID",                                                     onPushIDImGuiLua }, // Not Tested, Stack/State
        {"PushItemWidth",                                       onPushItemWidthImGuiLua },
        {"PushStyleColor",                                     onPushStyleColorImGuiLua },
        {"PushStyleVar",                                         onPushStyleVarImGuiLua },
        {"PushTextWrapPos",                                   onPushTextWrapPosImGuiLua }, // Not Tested, Stack/State
        {"RadioButton",                                           onRadioButtonImGuiLua },
        {"ResetMouseDragDelta",                           onResetMouseDragDeltaImGuiLua }, // Not Tested, Keyboard/Mouse
        {"SameLine",                                                 onSameLineImGuiLua },
        {"Selectable",                                             onSelectableImGuiLua },
        {"Separator",                                               onSeparatorImGuiLua },
        {"SetClipboardText",                                 onSetClipboardTextImGuiLua }, // Not Tested, Clipboard
        {"SetColorEditOptions",                           onSetColorEditOptionsImGuiLua }, // Not Tested, Color Widgets
        {"SetCursorPos",                                         onSetCursorPosImGuiLua }, // Not Tested, Window
        {"SetCursorPosX",                                       onSetCursorPosXImGuiLua },
        {"SetCursorPosY",                                       onSetCursorPosYImGuiLua },
        {"SetCursorScreenPos",                             onSetCursorScreenPosImGuiLua },
        {"SetNextItemAllowOverlap",                       onSetNextItemAllowOverlapImGuiLua }, // Not Tested, Window
        {"SetItemDefaultFocus",                           onSetItemDefaultFocusImGuiLua },
        {"SetKeyboardFocusHere",                         onSetKeyboardFocusHereImGuiLua }, // Not Tested, Keyboard/Mouse
        {"SetMouseCursor",                                     onSetMouseCursorImGuiLua }, // Not Tested, Keyboard/Mouse
        {"SetNextItemOpen",                                   onSetNextItemOpenImGuiLua },
        {"SetNextItemWidth",                                 onSetNextItemWidthImGuiLua },
        {"SetNextTreeNodeOpen",                           onSetNextTreeNodeOpenImGuiLua }, // Not Tested, Window
        {"SetNextWindowBgAlpha",                         onSetNextWindowBgAlphaImGuiLua },
        {"SetNextWindowCollapsed",                     onSetNextWindowCollapsedImGuiLua },
        {"SetNextWindowContentSize",                 onSetNextWindowContentSizeImGuiLua }, // Not Tested, Window
        {"SetNextWindowFocus",                             onSetNextWindowFocusImGuiLua },
        {"SetNextWindowPos",                                 onSetNextWindowPosImGuiLua },
        {"SetNextWindowSize",                               onSetNextWindowSizeImGuiLua },
        {"SetNextWindowSizeConstraints",         onSetNextWindowSizeConstraintsImGuiLua },
        {"SetScrollFromPosX",                               onSetScrollFromPosXImGuiLua }, // Not Tested, Scroll
        {"SetScrollFromPosY",                               onSetScrollFromPosYImGuiLua }, // Not Tested, Scroll
        {"SetScrollHere",                                       onSetScrollHereImGuiLua }, // Not Tested, Scroll
        {"SetScrollHereX",                                     onSetScrollHereXImGuiLua }, // Not Tested, Scroll
        {"SetScrollHereY",                                     onSetScrollHereYImGuiLua }, // Not Tested, Scroll
        {"SetScrollX",                                             onSetScrollXImGuiLua }, // Not Tested, Scroll
        {"SetScrollY",                                             onSetScrollYImGuiLua }, // Not Tested, Scroll
        {"TableAngledHeadersRow",                         onTableAngledHeadersRowImGuiLua },
        {"TableGetColumnCount",                           onTableGetColumnCountImGuiLua },
        {"TableGetColumnFlags",                           onTableGetColumnFlagsImGuiLua },
        {"TableGetColumnIndex",                           onTableGetColumnIndexImGuiLua },
        {"TableGetColumnName",                            onTableGetColumnNameImGuiLua },
        {"TableGetHoveredColumn",                         onTableGetHoveredColumnImGuiLua },
        {"TableGetRowIndex",                              onTableGetRowIndexImGuiLua },
        {"TableHeader",                                   onTableHeaderImGuiLua },
        {"TableHeadersRow",                               onTableHeadersRowImGuiLua },
        {"TableNextColumn",                               onTableNextColumnImGuiLua },
        {"TableNextRow",                                  onTableNextRowImGuiLua },
        {"TableSetBgColor",                               onTableSetBgColorImGuiLua },
        {"TableSetColumnEnabled",                         onTableSetColumnEnabledImGuiLua },
        {"TableSetColumnIndex",                           onTableSetColumnIndexImGuiLua },
        {"TableSetupColumn",                              onTableSetupColumnImGuiLua },
        {"TableSetupScrollFreeze",                        onTableSetupScrollFreezeImGuiLua },
        {"SetTabItemClosed",                                 onSetTabItemClosedImGuiLua }, // Not Tested, Popups
        {"SetTooltip",                                             onSetTooltipImGuiLua }, // Not Tested, Window
        {"SetWindowCollapsed",                             onSetWindowCollapsedImGuiLua }, // Not Tested, Window
        {"SetWindowFocus",                                     onSetWindowFocusImGuiLua }, // Not Tested, Window
        // SetWindowFontScale removed - deprecated in ImGui 1.92
        {"SetWindowPos",                                         onSetWindowPosImGuiLua }, // Not Tested, Window
        {"SetWindowSize",                                       onSetWindowSizeImGuiLua }, // Not Tested, Window
#if defined DEBUG || defined _DEBUG        
#if !defined (ANDROID)
        {"ShowDemoWindow",                                     onShowDemoWindowImGuiLua }, // Not Tested, Demo/Debug
        {"ShowFontSelector",                                 onShowFontSelectorImGuiLua }, // Not Tested, Demo/Debug
        {"ShowStyleSelector",                               onShowStyleSelectorImGuiLua }, // Not Tested, Demo/Debug
        {"ShowUserGuide",                                       onShowUserGuideImGuiLua }, // Not Tested, Demo/Debug
#endif
#endif
        {"SliderAngle",                                           onSliderAngleImGuiLua }, // Not Tested, Input Widgets
        {"SliderFloat",                                           onSliderFloatImGuiLua },
        {"SliderFloat2",                                         onSliderFloat2ImGuiLua }, // Not Tested, Input Widgets
        {"SliderFloat3",                                         onSliderFloat3ImGuiLua }, // Not Tested, Input Widgets
        {"SliderFloat4",                                         onSliderFloat4ImGuiLua }, // Not Tested, Input Widgets
        {"SliderInt",                                               onSliderIntImGuiLua },
        {"SliderInt2",                                             onSliderInt2ImGuiLua }, // Not Tested, Input Widgets
        {"SliderInt3",                                             onSliderInt3ImGuiLua }, // Not Tested, Input Widgets
        {"SliderInt4",                                             onSliderInt4ImGuiLua }, // Not Tested, Input Widgets
        {"SmallButton",                                           onSmallButtonImGuiLua },
        {"Spacing",                                                   onSpacingImGuiLua }, // Not Tested, Window/Layout
        {"StyleColorsClassic",                             onStyleColorsClassicImGuiLua }, // Not Tested, Styles
        {"StyleColorsDark",                                   onStyleColorsDarkImGuiLua }, // Not Tested, Styles
        {"StyleColorsLight",                                 onStyleColorsLightImGuiLua }, // Not Tested, Styles
        {"Text",                                                         onTextImGuiLua },
        {"TextColored",                                           onTextColoredImGuiLua },
        {"TextDisabled",                                         onTextDisabledImGuiLua },
        {"TextWrapped",                                           onTextWrappedImGuiLua }, // Not Tested, Text/Labels
        {"TreeAdvanceToLabelPos",                       onTreeAdvanceToLabelPosImGuiLua }, // Not Tested, Tree
        {"TreeNode",                                                 onTreeNodeImGuiLua },
        {"TreeNodeEx",                                             onTreeNodeExImGuiLua },
        {"TreePop",                                                   onTreePopImGuiLua },
        {"TreePush",                                                 onTreePushImGuiLua }, // Not Tested, Tree
        {"Unindent",                                                 onUnindentImGuiLua }, // Not Tested, Window/Layout
        {"VSliderFloat",                                         onVSliderFloatImGuiLua }, // Not Tested, Input Widgets
        {"VSliderInt",                                             onVSliderIntImGuiLua }, // Not Tested, Input Widgets
        
        //ImDrawList
        {"AddBezierCubic",                     onAddBezierCubicImDrawListLua }, // Not Tested, ImDrawList
        {"AddCircle",                               onAddCircleImDrawListLua },
        {"AddCircleFilled",                   onAddCircleFilledImDrawListLua },
        {"AddConvexPolyFilled",           onAddConvexPolyFilledImDrawListLua }, // Not Tested, ImDrawList
        {"AddDrawCmd",                             onAddDrawCmdImDrawListLua }, // Not Tested, ImDrawList
        {"AddImage",                                 onAddImageImDrawListLua }, // Not Tested, ImDrawList
        {"AddImageQuad",                         onAddImageQuadImDrawListLua }, // Not Tested, ImDrawList
        {"AddImageRounded",                   onAddImageRoundedImDrawListLua }, // Not Tested, ImDrawList
        {"AddLine",                                   onAddLineImDrawListLua },
        {"AddNgon",                                   onAddNgonImDrawListLua },
        {"AddNgonFilled",                       onAddNgonFilledImDrawListLua }, // Not Tested, ImDrawList
        {"AddPolyline",                           onAddPolylineImDrawListLua }, // Not Tested, ImDrawList
        {"AddQuad",                                   onAddQuadImDrawListLua }, // Not Tested, ImDrawList
        {"AddQuadFilled",                       onAddQuadFilledImDrawListLua }, // Not Tested, ImDrawList
        {"AddRect",                                   onAddRectImDrawListLua },
        {"AddRectFilled",                       onAddRectFilledImDrawListLua },
        {"AddRectFilledMultiColor",   onAddRectFilledMultiColorImDrawListLua }, // Not Tested, ImDrawList
        {"AddText",                                   onAddTextImDrawListLua }, // Not Tested, ImDrawList
        {"AddTriangle",                           onAddTriangleImDrawListLua },
        {"AddTriangleFilled",               onAddTriangleFilledImDrawListLua },
        {"SetImDrawListToBackground",              onImDrawListToBackgroundLua }, // Not Tested, ImDrawList
        {"SetImDrawListToForeground",              onImDrawListToForegroundLua },
        {nullptr, nullptr}};

    luaL_newlib(lua, regimguiMethods);
    luaL_getmetatable(lua, "_mbmImGui_LUA");
    lua_setmetatable(lua, -2);
    auto **udata                = static_cast<IMGUI_LUA **>(lua_newuserdata(lua, sizeof(IMGUI_LUA *)));
    IMGUI_LUA * that            = new IMGUI_LUA();
    *udata                      = that;

    /* Make our class as plugin mbm compatible to the engine. */
    luaL_getmetatable(lua,"_usertype_plugin");//are we using the module in the mbm engine?

    if(lua_type(lua,-1) == LUA_TTABLE) //Yes
    {
        lua_rawgeti(lua,-1, 1);
        //this value is auto set by this module. It is set in the metatable to make sure that we can convert the userdata to ** IMGUI_LUA
        PLUGIN_IDENTIFIER  = lua_tointeger(lua,-1);//update the identifier of pluging
        lua_pop(lua,1);
    }
    else
    {
        lua_pop(lua, 1);
        mbm::lua_create_metatable_identifier(lua,"_usertype_plugin",PLUGIN_IDENTIFIER);//No, we just have to create a metatable to identify the module
    }
    lua_setmetatable(lua,-2);
    /* end plugin code*/

    lua_rawseti(lua, -2, 1);//set usedata as the first member in the table

    bool bRegistered                       = false;
    const int index_plugin                 = lua_gettop(lua);
    unsigned int index_plugin_subscription = 0xffffffff;
    lua_getglobal(lua,"mbm");//auto subscribe
    if(lua_type(lua,-1) == LUA_TTABLE)
    {
        lua_getfield(lua,-1,"subscribe");
        if(lua_isfunction(lua,-1))
        {
            lua_pushvalue(lua,index_plugin);
            constexpr int nargs    = 1;
            constexpr int nresults = 1; //index plugin registered
            if(lua_pcall(lua,nargs,nresults,0) == LUA_OK )
            {
                if(lua_type(lua,-1) == LUA_TNUMBER)
                {
                    unsigned int index_plugin_subscription = lua_tointeger(lua,-1);
                    if(index_plugin_subscription != 0xffffffff)
                    {
                        bRegistered = true;
                    }
                }
            }
        }
    }
    if(bRegistered)
    {
        const int total_in_stack = lua_gettop(lua);
        if(total_in_stack > index_plugin)
        {
            const int total_pop = total_in_stack - index_plugin;
            lua_pop(lua,total_pop);
        }
    }
    else
    {
        lua_settop(lua,0);
        luaL_error(lua,"Error registering plugin...\nModule is defined to use PLUGIN_CALLBACK however could not subscribe to mbm.subscribe function!\n index of subscription [%d]",index_plugin_subscription);
    }
    return 1;
}

void registerClassimgui(lua_State *lua)
{
    luaL_Reg regimguiMethods[]  = {{"new", onNewimguiLua}, {"__gc", onDestroyimguiLua}, {nullptr, nullptr}};
    luaL_newmetatable(lua, "_mbmImGui_LUA");
    luaL_setfuncs(lua, regimguiMethods, 0);
    // this is your table registered on lua. use: t_imgui = imgui.new()
    lua_setglobal(lua, "imgui"); 
    lua_settop(lua,0);
    printf("ImGui version %s\n", ImGui::GetVersion());
}

//The name of this C function is the string "luaopen_" concatenated with
//   a copy of the module name where each dot is replaced by an underscore.
//Moreover, if the module name has a hyphen, its prefix up to (and including) the
//   first hyphen is removed. For instance, if the module name is a.v1-b.c, the function name will be luaopen_b_c.
//
// Note that the name of this function is not flexible
int luaopen_ImGui (lua_State *lua)
{
    registerClassimgui(lua);
    return onNewimguiLua(lua);
}
//sometimes it is followed by "lib" -> "lib"imgui_lua
int luaopen_libImGui (lua_State *lua)
{
    return luaopen_ImGui(lua);
}


/*
TODO:
 in Demo > Tables & Columns, please check it out.
This has lots of features! Check demo and imgui.h for details.
One big difference with the Columns API is that you need to call TableNextRow() to begin a new row (you can also call TableNextCell() there and benefit of wrapping). Refer to Demo>Tables&Columns->Basic for a rundown of ways to use TableNextRow() / TableNextCell() /
TableSetColumnIndex().
*/
