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
#include "imgui_impl_opengl3.h"
#include "imgui_stdlib.h" 

#if defined _WIN32
    #include "imgui_impl_win32.h"
#elif defined(__linux__) && !defined (ANDROID)
    #ifndef XK_MISCELLANY
        #define XK_MISCELLANY
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

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#include <string>
#include <vector>
#include <map>
#include <stdio.h>

#if defined _WIN32
    #include <Windows.h>
#else
    #include <core_mbm/core-manager.h>
#endif

#include <core_mbm/util-interface.h>

class IMGUI_LUA;

/*
    This class is intended to be used as interface to mbm engine.
    If there is no intent to use this module in the engine there is no problem. It can be used as normal module in lua.
*/

#ifdef  PLUGIN_CALLBACK
    #include <core_mbm/plugin-callback.h>
#endif


static int PLUGIN_IDENTIFIER = 1; //this value is auto set by this module. It is set in the metatable to make sure that we can convert the userdata to ** IMGUI_LUA
static bool bDrawListToBackground = false;
static bool bDrawListToForeground = false;

namespace key_mouse
{
    constexpr int   size_mouse_struct_img   = sizeof(ImGuiIO::MouseClicked) / sizeof(ImGuiIO::MouseClicked[0]);
    constexpr int   size_key_struct_img     = sizeof(ImGuiIO::KeysDown)     / sizeof(ImGuiIO::KeysDown[0]);
    static float    mouse_wheel = 0;
    static bool     KeysDown[size_key_struct_img];
    static bool     KeyCtrl = 0;
    static bool     KeyShift = 0;
    static bool     KeyAlt = 0;
    static bool     KeySuper =0;
    static bool     KeyCapital = 0;
    static bool     MouseDown[size_mouse_struct_img];
};

ImDrawList* GetImDrawListLua()
{
    if(bDrawListToForeground)
        return ImGui::GetForegroundDrawList();
    if(bDrawListToForeground)
        return ImGui::GetBackgroundDrawList();
    return ImGui::GetWindowDrawList();
}

namespace ImGui
{
    void   IsScrollVisible(bool* x, bool * y);
    void   ImageQuad(ImTextureID user_texture_id, const ImVec2& size, const ImVec2& uv0 , const ImVec2& uv1, const ImVec2& uv2,const ImVec2& uv3 , const ImVec4& tint_col = ImVec4(1,1,1,1), const ImVec4& border_col = ImVec4(0,0,0,0));
    float  GetMainMenuBarHeight();
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
        {"ImGuiKey_KeyPadEnter",                              ImGuiKey_KeyPadEnter},
        {"ImGuiKey_A",                                        ImGuiKey_A},
        {"ImGuiKey_C",                                        ImGuiKey_C},
        {"ImGuiKey_V",                                        ImGuiKey_V},
        {"ImGuiKey_X",                                        ImGuiKey_X},
        {"ImGuiKey_Y",                                        ImGuiKey_Y},
        {"ImGuiKey_Z",                                        ImGuiKey_Z},
};

static const std::map<std::string,int> allFlags = {
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
        {"ImGuiWindowFlags_AlwaysUseWindowPadding",           ImGuiWindowFlags_AlwaysUseWindowPadding},
        {"ImGuiWindowFlags_NoNavInputs",                      ImGuiWindowFlags_NoNavInputs},
        {"ImGuiWindowFlags_NoNavFocus",                       ImGuiWindowFlags_NoNavFocus},
        {"ImGuiWindowFlags_UnsavedDocument",                  ImGuiWindowFlags_UnsavedDocument},
        {"ImGuiWindowFlags_NoNav",                            ImGuiWindowFlags_NoNav},
        {"ImGuiWindowFlags_NoDecoration",                     ImGuiWindowFlags_NoDecoration},
        {"ImGuiWindowFlags_NoInputs",                         ImGuiWindowFlags_NoInputs},
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
        {"ImGuiInputTextFlags_AlwaysInsertMode",              ImGuiInputTextFlags_AlwaysInsertMode},
        {"ImGuiInputTextFlags_ReadOnly",                      ImGuiInputTextFlags_ReadOnly},
        {"ImGuiInputTextFlags_Password",                      ImGuiInputTextFlags_Password},
        {"ImGuiInputTextFlags_NoUndoRedo",                    ImGuiInputTextFlags_NoUndoRedo},
        {"ImGuiInputTextFlags_CharsScientific",               ImGuiInputTextFlags_CharsScientific},
        {"ImGuiInputTextFlags_CallbackResize",                ImGuiInputTextFlags_CallbackResize},
        {"ImGuiTreeNodeFlags_None",                           ImGuiTreeNodeFlags_None},
        {"ImGuiTreeNodeFlags_Selected",                       ImGuiTreeNodeFlags_Selected},
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
        {"ImGuiWindowFlags_AlwaysUseWindowPadding",           ImGuiWindowFlags_AlwaysUseWindowPadding},
        {"ImGuiWindowFlags_NoNavInputs",                      ImGuiWindowFlags_NoNavInputs},
        {"ImGuiWindowFlags_NoNavFocus",                       ImGuiWindowFlags_NoNavFocus},
        {"ImGuiWindowFlags_UnsavedDocument",                  ImGuiWindowFlags_UnsavedDocument},
        {"ImGuiWindowFlags_NoNav",                            ImGuiWindowFlags_NoNav},
        {"ImGuiWindowFlags_NoDecoration",                     ImGuiWindowFlags_NoDecoration},
        {"ImGuiWindowFlags_NoInputs",                         ImGuiWindowFlags_NoInputs},
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
        {"ImGuiInputTextFlags_AlwaysInsertMode",              ImGuiInputTextFlags_AlwaysInsertMode},
        {"ImGuiInputTextFlags_ReadOnly",                      ImGuiInputTextFlags_ReadOnly},
        {"ImGuiInputTextFlags_Password",                      ImGuiInputTextFlags_Password},
        {"ImGuiInputTextFlags_NoUndoRedo",                    ImGuiInputTextFlags_NoUndoRedo},
        {"ImGuiInputTextFlags_CharsScientific",               ImGuiInputTextFlags_CharsScientific},
        {"ImGuiInputTextFlags_CallbackResize",                ImGuiInputTextFlags_CallbackResize},
        {"ImGuiTreeNodeFlags_None",                           ImGuiTreeNodeFlags_None},
        {"ImGuiTreeNodeFlags_Selected",                       ImGuiTreeNodeFlags_Selected},
        {"ImGuiTreeNodeFlags_Framed",                         ImGuiTreeNodeFlags_Framed},
        {"ImGuiTreeNodeFlags_AllowItemOverlap",               ImGuiTreeNodeFlags_AllowItemOverlap},
        {"ImGuiTreeNodeFlags_NoTreePushOnOpen",               ImGuiTreeNodeFlags_NoTreePushOnOpen},
        {"ImGuiTreeNodeFlags_NoAutoOpenOnLog",                ImGuiTreeNodeFlags_NoAutoOpenOnLog},
        {"ImGuiTreeNodeFlags_DefaultOpen",                    ImGuiTreeNodeFlags_DefaultOpen},
        {"ImGuiTreeNodeFlags_OpenOnDoubleClick",              ImGuiTreeNodeFlags_OpenOnDoubleClick},
        {"ImGuiTreeNodeFlags_OpenOnArrow",                    ImGuiTreeNodeFlags_OpenOnArrow},
        {"ImGuiTreeNodeFlags_Leaf",                           ImGuiTreeNodeFlags_Leaf},
        {"ImGuiTreeNodeFlags_Bullet",                         ImGuiTreeNodeFlags_Bullet},
        {"ImGuiTreeNodeFlags_FramePadding",                   ImGuiTreeNodeFlags_FramePadding},
        {"ImGuiTreeNodeFlags_SpanAvailWidth",                 ImGuiTreeNodeFlags_SpanAvailWidth},
        {"ImGuiTreeNodeFlags_SpanFullWidth",                  ImGuiTreeNodeFlags_SpanFullWidth},
        {"ImGuiTreeNodeFlags_NavLeftJumpsBackHere",           ImGuiTreeNodeFlags_NavLeftJumpsBackHere},
        {"ImGuiSelectableFlags_None",                         ImGuiSelectableFlags_None},
        {"ImGuiSelectableFlags_DontClosePopups",              ImGuiSelectableFlags_DontClosePopups},
        {"ImGuiSelectableFlags_SpanAllColumns",               ImGuiSelectableFlags_SpanAllColumns},
        {"ImGuiSelectableFlags_AllowDoubleClick",             ImGuiSelectableFlags_AllowDoubleClick},
        {"ImGuiSelectableFlags_Disabled",                     ImGuiSelectableFlags_Disabled},
        {"ImGuiSelectableFlags_AllowItemOverlap",             ImGuiSelectableFlags_AllowItemOverlap},
        {"ImGuiComboFlags_None",                              ImGuiComboFlags_None},
        {"ImGuiComboFlags_PopupAlignLeft",                    ImGuiComboFlags_PopupAlignLeft},
        {"ImGuiComboFlags_HeightSmall",                       ImGuiComboFlags_HeightSmall},
        {"ImGuiComboFlags_HeightRegular",                     ImGuiComboFlags_HeightRegular},
        {"ImGuiComboFlags_HeightLarge",                       ImGuiComboFlags_HeightLarge},
        {"ImGuiComboFlags_HeightLargest",                     ImGuiComboFlags_HeightLargest},
        {"ImGuiComboFlags_NoArrowButton",                     ImGuiComboFlags_NoArrowButton},
        {"ImGuiComboFlags_NoPreview",                         ImGuiComboFlags_NoPreview},
        {"ImGuiComboFlags_HeightMask_",                       ImGuiComboFlags_HeightMask_},
        {"ImGuiTabBarFlags_None",                             ImGuiTabBarFlags_None},
        {"ImGuiTabBarFlags_Reorderable",                      ImGuiTabBarFlags_Reorderable},
        {"ImGuiTabBarFlags_AutoSelectNewTabs",                ImGuiTabBarFlags_AutoSelectNewTabs},
        {"ImGuiTabBarFlags_TabListPopupButton",               ImGuiTabBarFlags_TabListPopupButton},
        {"ImGuiTabBarFlags_NoCloseWithMiddleMouseButton",     ImGuiTabBarFlags_NoCloseWithMiddleMouseButton},
        {"ImGuiTabBarFlags_NoTabListScrollingButtons",        ImGuiTabBarFlags_NoTabListScrollingButtons},
        {"ImGuiTabBarFlags_NoTooltip",                        ImGuiTabBarFlags_NoTooltip},
        {"ImGuiTabBarFlags_FittingPolicyResizeDown",          ImGuiTabBarFlags_FittingPolicyResizeDown},
        {"ImGuiTabBarFlags_FittingPolicyScroll",              ImGuiTabBarFlags_FittingPolicyScroll},
        {"ImGuiTabBarFlags_FittingPolicyMask_",               ImGuiTabBarFlags_FittingPolicyMask_},
        {"ImGuiTabBarFlags_FittingPolicyDefault_",            ImGuiTabBarFlags_FittingPolicyDefault_},
        {"ImGuiTabItemFlags_None",                            ImGuiTabItemFlags_None},
        {"ImGuiTabItemFlags_UnsavedDocument",                 ImGuiTabItemFlags_UnsavedDocument},
        {"ImGuiTabItemFlags_SetSelected",                     ImGuiTabItemFlags_SetSelected},
        {"ImGuiTabItemFlags_NoCloseWithMiddleMouseButton",    ImGuiTabItemFlags_NoCloseWithMiddleMouseButton},
        {"ImGuiTabItemFlags_NoPushId",                        ImGuiTabItemFlags_NoPushId},
        {"ImGuiFocusedFlags_None",                            ImGuiFocusedFlags_None},
        {"ImGuiFocusedFlags_ChildWindows",                    ImGuiFocusedFlags_ChildWindows},
        {"ImGuiFocusedFlags_RootWindow",                      ImGuiFocusedFlags_RootWindow},
        {"ImGuiFocusedFlags_AnyWindow",                       ImGuiFocusedFlags_AnyWindow},
        {"ImGuiFocusedFlags_RootAndChildWindows",             ImGuiFocusedFlags_RootAndChildWindows},
        {"ImGuiHoveredFlags_None",                            ImGuiHoveredFlags_None},
        {"ImGuiHoveredFlags_ChildWindows",                    ImGuiHoveredFlags_ChildWindows},
        {"ImGuiHoveredFlags_RootWindow",                      ImGuiHoveredFlags_RootWindow},
        {"ImGuiHoveredFlags_AnyWindow",                       ImGuiHoveredFlags_AnyWindow},
        {"ImGuiHoveredFlags_AllowWhenBlockedByPopup",         ImGuiHoveredFlags_AllowWhenBlockedByPopup},
        {"ImGuiHoveredFlags_AllowWhenBlockedByActiveItem",    ImGuiHoveredFlags_AllowWhenBlockedByActiveItem},
        {"ImGuiHoveredFlags_AllowWhenOverlapped",             ImGuiHoveredFlags_AllowWhenOverlapped},
        {"ImGuiHoveredFlags_AllowWhenDisabled",               ImGuiHoveredFlags_AllowWhenDisabled},
        {"ImGuiHoveredFlags_RectOnly",                        ImGuiHoveredFlags_RectOnly},
        {"ImGuiHoveredFlags_RootAndChildWindows",             ImGuiHoveredFlags_RootAndChildWindows},
        {"ImGuiDragDropFlags_None",                           ImGuiDragDropFlags_None},
        {"ImGuiDragDropFlags_SourceNoPreviewTooltip",         ImGuiDragDropFlags_SourceNoPreviewTooltip},
        {"ImGuiDragDropFlags_SourceNoDisableHover",           ImGuiDragDropFlags_SourceNoDisableHover},
        {"ImGuiDragDropFlags_SourceNoHoldToOpenOthers",       ImGuiDragDropFlags_SourceNoHoldToOpenOthers},
        {"ImGuiDragDropFlags_SourceAllowNullID",              ImGuiDragDropFlags_SourceAllowNullID},
        {"ImGuiDragDropFlags_SourceExtern",                   ImGuiDragDropFlags_SourceExtern},
        {"ImGuiDragDropFlags_SourceAutoExpirePayload",        ImGuiDragDropFlags_SourceAutoExpirePayload},
        {"ImGuiDragDropFlags_AcceptBeforeDelivery",           ImGuiDragDropFlags_AcceptBeforeDelivery},
        {"ImGuiDragDropFlags_AcceptNoDrawDefaultRect",        ImGuiDragDropFlags_AcceptNoDrawDefaultRect},
        {"ImGuiDragDropFlags_AcceptNoPreviewTooltip",         ImGuiDragDropFlags_AcceptNoPreviewTooltip},
        {"ImGuiDragDropFlags_AcceptPeekOnly",                 ImGuiDragDropFlags_AcceptPeekOnly},
        {"ImGuiDir_None",                                     ImGuiDir_None},
        {"ImGuiDir_Left",                                     ImGuiDir_Left},
        {"ImGuiDir_Right",                                    ImGuiDir_Right},
        {"ImGuiDir_Up",                                       ImGuiDir_Up},
        {"ImGuiDir_Down",                                     ImGuiDir_Down},
        {"ImGuiNavInput_Activate",                            ImGuiNavInput_Activate},
        {"ImGuiNavInput_Cancel",                              ImGuiNavInput_Cancel},
        {"ImGuiNavInput_Input",                               ImGuiNavInput_Input},
        {"ImGuiNavInput_Menu",                                ImGuiNavInput_Menu},
        {"ImGuiNavInput_DpadLeft",                            ImGuiNavInput_DpadLeft},
        {"ImGuiNavInput_DpadRight",                           ImGuiNavInput_DpadRight},
        {"ImGuiNavInput_DpadUp",                              ImGuiNavInput_DpadUp},
        {"ImGuiNavInput_DpadDown",                            ImGuiNavInput_DpadDown},
        {"ImGuiNavInput_LStickLeft",                          ImGuiNavInput_LStickLeft},
        {"ImGuiNavInput_LStickRight",                         ImGuiNavInput_LStickRight},
        {"ImGuiNavInput_LStickUp",                            ImGuiNavInput_LStickUp},
        {"ImGuiNavInput_LStickDown",                          ImGuiNavInput_LStickDown},
        {"ImGuiNavInput_FocusPrev",                           ImGuiNavInput_FocusPrev},
        {"ImGuiNavInput_FocusNext",                           ImGuiNavInput_FocusNext},
        {"ImGuiNavInput_TweakSlow",                           ImGuiNavInput_TweakSlow},
        {"ImGuiNavInput_TweakFast",                           ImGuiNavInput_TweakFast},
        {"ImGuiConfigFlags_None",                             ImGuiConfigFlags_None},
        {"ImGuiConfigFlags_NavEnableKeyboard",                ImGuiConfigFlags_NavEnableKeyboard},
        {"ImGuiConfigFlags_NavEnableGamepad",                 ImGuiConfigFlags_NavEnableGamepad},
        {"ImGuiConfigFlags_NavEnableSetMousePos",             ImGuiConfigFlags_NavEnableSetMousePos},
        {"ImGuiConfigFlags_NavNoCaptureKeyboard",             ImGuiConfigFlags_NavNoCaptureKeyboard},
        {"ImGuiConfigFlags_NoMouse",                          ImGuiConfigFlags_NoMouse},
        {"ImGuiConfigFlags_NoMouseCursorChange",              ImGuiConfigFlags_NoMouseCursorChange},
        {"ImGuiConfigFlags_IsSRGB",                           ImGuiConfigFlags_IsSRGB},
        {"ImGuiConfigFlags_IsTouchScreen",                    ImGuiConfigFlags_IsTouchScreen},
        {"ImGuiBackendFlags_None",                            ImGuiBackendFlags_None},
        {"ImGuiBackendFlags_HasGamepad",                      ImGuiBackendFlags_HasGamepad},
        {"ImGuiBackendFlags_HasMouseCursors",                 ImGuiBackendFlags_HasMouseCursors},
        {"ImGuiBackendFlags_HasSetMousePos",                  ImGuiBackendFlags_HasSetMousePos},
        {"ImGuiBackendFlags_RendererHasVtxOffset",            ImGuiBackendFlags_RendererHasVtxOffset},
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
        {"ImGuiCol_TabActive",                                ImGuiCol_TabActive},
        {"ImGuiCol_TabUnfocused",                             ImGuiCol_TabUnfocused},
        {"ImGuiCol_TabUnfocusedActive",                       ImGuiCol_TabUnfocusedActive},
        {"ImGuiCol_PlotLines",                                ImGuiCol_PlotLines},
        {"ImGuiCol_PlotLinesHovered",                         ImGuiCol_PlotLinesHovered},
        {"ImGuiCol_PlotHistogram",                            ImGuiCol_PlotHistogram},
        {"ImGuiCol_PlotHistogramHovered",                     ImGuiCol_PlotHistogramHovered},
        {"ImGuiCol_TextSelectedBg",                           ImGuiCol_TextSelectedBg},
        {"ImGuiCol_DragDropTarget",                           ImGuiCol_DragDropTarget},
        {"ImGuiCol_NavHighlight",                             ImGuiCol_NavHighlight},
        {"ImGuiCol_NavWindowingHighlight",                    ImGuiCol_NavWindowingHighlight},
        {"ImGuiCol_NavWindowingDimBg",                        ImGuiCol_NavWindowingDimBg},
        {"ImGuiCol_ModalWindowDimBg",                         ImGuiCol_ModalWindowDimBg},
        {"ImGuiStyleVar_Alpha",                               ImGuiStyleVar_Alpha},
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
        {"ImGuiStyleVar_ScrollbarSize",                       ImGuiStyleVar_ScrollbarSize},
        {"ImGuiStyleVar_ScrollbarRounding",                   ImGuiStyleVar_ScrollbarRounding},
        {"ImGuiStyleVar_GrabMinSize",                         ImGuiStyleVar_GrabMinSize},
        {"ImGuiStyleVar_GrabRounding",                        ImGuiStyleVar_GrabRounding},
        {"ImGuiStyleVar_TabRounding",                         ImGuiStyleVar_TabRounding},
        {"ImGuiStyleVar_ButtonTextAlign",                     ImGuiStyleVar_ButtonTextAlign},
        {"ImGuiStyleVar_SelectableTextAlign",                 ImGuiStyleVar_SelectableTextAlign},
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
        {"ImGuiColorEditFlags_AlphaPreview",                  ImGuiColorEditFlags_AlphaPreview},
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
        {"ImGuiColorEditFlags_InputHSV",                      ImGuiColorEditFlags_InputHSV},
        {"ImGuiColorEditFlags_DefaultOptions_",               ImGuiColorEditFlags_DefaultOptions_},
        {"ImGuiMouseButton_Left",                             ImGuiMouseButton_Left},
        {"ImGuiMouseButton_Right",                            ImGuiMouseButton_Right},
        {"ImGuiMouseButton_Middle",                           ImGuiMouseButton_Middle},
        {"ImGuiMouseButton_COUNT",                            ImGuiMouseButton_COUNT},
        {"ImGuiMouseCursor_None",                             ImGuiMouseCursor_None},
        {"ImGuiMouseCursor_Arrow",                            ImGuiMouseCursor_Arrow},
        {"ImGuiMouseCursor_TextInput",                        ImGuiMouseCursor_TextInput},
        {"ImGuiMouseCursor_ResizeAll",                        ImGuiMouseCursor_ResizeAll},
        {"ImGuiMouseCursor_ResizeNS",                         ImGuiMouseCursor_ResizeNS},
        {"ImGuiMouseCursor_ResizeEW",                         ImGuiMouseCursor_ResizeEW},
        {"ImGuiMouseCursor_ResizeNESW",                       ImGuiMouseCursor_ResizeNESW},
        {"ImGuiMouseCursor_ResizeNWSE",                       ImGuiMouseCursor_ResizeNWSE},
        {"ImGuiMouseCursor_Hand",                             ImGuiMouseCursor_Hand},
        {"ImGuiMouseCursor_NotAllowed",                       ImGuiMouseCursor_NotAllowed},
        {"ImGuiCond_Always",                                  ImGuiCond_Always},
        {"ImGuiCond_Once",                                    ImGuiCond_Once},
        {"ImGuiCond_FirstUseEver",                            ImGuiCond_FirstUseEver},
        {"ImGuiCond_Appearing",                               ImGuiCond_Appearing},
        {"ImDrawCornerFlags_None",                            ImDrawCornerFlags_None},
        {"ImDrawCornerFlags_TopLeft",                         ImDrawCornerFlags_TopLeft},
        {"ImDrawCornerFlags_TopRight",                        ImDrawCornerFlags_TopRight},
        {"ImDrawCornerFlags_BotLeft",                         ImDrawCornerFlags_BotLeft},
        {"ImDrawCornerFlags_BotRight",                        ImDrawCornerFlags_BotRight},
        {"ImDrawCornerFlags_Top",                             ImDrawCornerFlags_Top},
        {"ImDrawCornerFlags_Bot",                             ImDrawCornerFlags_Bot},
        {"ImDrawCornerFlags_Left",                            ImDrawCornerFlags_Left},
        {"ImDrawCornerFlags_Right",                           ImDrawCornerFlags_Right},
        {"ImDrawCornerFlags_All",                             ImDrawCornerFlags_All},
        {"ImDrawListFlags_None",                              ImDrawListFlags_None},
        {"ImDrawListFlags_AntiAliasedLines",                  ImDrawListFlags_AntiAliasedLines},
        {"ImDrawListFlags_AntiAliasedFill",                   ImDrawListFlags_AntiAliasedFill},
        {"ImDrawListFlags_AllowVtxOffset",                    ImDrawListFlags_AllowVtxOffset},
        {"ImGuiTreeNodeFlags_Framed",                         ImGuiTreeNodeFlags_Framed},
        {"ImGuiTreeNodeFlags_AllowItemOverlap",               ImGuiTreeNodeFlags_AllowItemOverlap},
        {"ImGuiTreeNodeFlags_NoTreePushOnOpen",               ImGuiTreeNodeFlags_NoTreePushOnOpen},
        {"ImGuiTreeNodeFlags_NoAutoOpenOnLog",                ImGuiTreeNodeFlags_NoAutoOpenOnLog},
        {"ImGuiTreeNodeFlags_DefaultOpen",                    ImGuiTreeNodeFlags_DefaultOpen},
        {"ImGuiTreeNodeFlags_OpenOnDoubleClick",              ImGuiTreeNodeFlags_OpenOnDoubleClick},
        {"ImGuiTreeNodeFlags_OpenOnArrow",                    ImGuiTreeNodeFlags_OpenOnArrow},
        {"ImGuiTreeNodeFlags_Leaf",                           ImGuiTreeNodeFlags_Leaf},
        {"ImGuiTreeNodeFlags_Bullet",                         ImGuiTreeNodeFlags_Bullet},
        {"ImGuiTreeNodeFlags_FramePadding",                   ImGuiTreeNodeFlags_FramePadding},
        {"ImGuiTreeNodeFlags_SpanAvailWidth",                 ImGuiTreeNodeFlags_SpanAvailWidth},
        {"ImGuiTreeNodeFlags_SpanFullWidth",                  ImGuiTreeNodeFlags_SpanFullWidth},
        {"ImGuiTreeNodeFlags_NavLeftJumpsBackHere",           ImGuiTreeNodeFlags_NavLeftJumpsBackHere},
        {"ImGuiSelectableFlags_None",                         ImGuiSelectableFlags_None},
        {"ImGuiSelectableFlags_DontClosePopups",              ImGuiSelectableFlags_DontClosePopups},
        {"ImGuiSelectableFlags_SpanAllColumns",               ImGuiSelectableFlags_SpanAllColumns},
        {"ImGuiSelectableFlags_AllowDoubleClick",             ImGuiSelectableFlags_AllowDoubleClick},
        {"ImGuiSelectableFlags_Disabled",                     ImGuiSelectableFlags_Disabled},
        {"ImGuiSelectableFlags_AllowItemOverlap",             ImGuiSelectableFlags_AllowItemOverlap},
        {"ImGuiComboFlags_None",                              ImGuiComboFlags_None},
        {"ImGuiComboFlags_PopupAlignLeft",                    ImGuiComboFlags_PopupAlignLeft},
        {"ImGuiComboFlags_HeightSmall",                       ImGuiComboFlags_HeightSmall},
        {"ImGuiComboFlags_HeightRegular",                     ImGuiComboFlags_HeightRegular},
        {"ImGuiComboFlags_HeightLarge",                       ImGuiComboFlags_HeightLarge},
        {"ImGuiComboFlags_HeightLargest",                     ImGuiComboFlags_HeightLargest},
        {"ImGuiComboFlags_NoArrowButton",                     ImGuiComboFlags_NoArrowButton},
        {"ImGuiComboFlags_NoPreview",                         ImGuiComboFlags_NoPreview},
        {"ImGuiComboFlags_HeightMask_",                       ImGuiComboFlags_HeightMask_},
        {"ImGuiTabBarFlags_None",                             ImGuiTabBarFlags_None},
        {"ImGuiTabBarFlags_Reorderable",                      ImGuiTabBarFlags_Reorderable},
        {"ImGuiTabBarFlags_AutoSelectNewTabs",                ImGuiTabBarFlags_AutoSelectNewTabs},
        {"ImGuiTabBarFlags_TabListPopupButton",               ImGuiTabBarFlags_TabListPopupButton},
        {"ImGuiTabBarFlags_NoCloseWithMiddleMouseButton",     ImGuiTabBarFlags_NoCloseWithMiddleMouseButton},
        {"ImGuiTabBarFlags_NoTabListScrollingButtons",        ImGuiTabBarFlags_NoTabListScrollingButtons},
        {"ImGuiTabBarFlags_NoTooltip",                        ImGuiTabBarFlags_NoTooltip},
        {"ImGuiTabBarFlags_FittingPolicyResizeDown",          ImGuiTabBarFlags_FittingPolicyResizeDown},
        {"ImGuiTabBarFlags_FittingPolicyScroll",              ImGuiTabBarFlags_FittingPolicyScroll},
        {"ImGuiTabBarFlags_FittingPolicyMask_",               ImGuiTabBarFlags_FittingPolicyMask_},
        {"ImGuiTabBarFlags_FittingPolicyDefault_",            ImGuiTabBarFlags_FittingPolicyDefault_},
        {"ImGuiTabItemFlags_None",                            ImGuiTabItemFlags_None},
        {"ImGuiTabItemFlags_UnsavedDocument",                 ImGuiTabItemFlags_UnsavedDocument},
        {"ImGuiTabItemFlags_SetSelected",                     ImGuiTabItemFlags_SetSelected},
        {"ImGuiTabItemFlags_NoCloseWithMiddleMouseButton",    ImGuiTabItemFlags_NoCloseWithMiddleMouseButton},
        {"ImGuiTabItemFlags_NoPushId",                        ImGuiTabItemFlags_NoPushId},
        {"ImGuiFocusedFlags_None",                            ImGuiFocusedFlags_None},
        {"ImGuiFocusedFlags_ChildWindows",                    ImGuiFocusedFlags_ChildWindows},
        {"ImGuiFocusedFlags_RootWindow",                      ImGuiFocusedFlags_RootWindow},
        {"ImGuiFocusedFlags_AnyWindow",                       ImGuiFocusedFlags_AnyWindow},
        {"ImGuiFocusedFlags_RootAndChildWindows",             ImGuiFocusedFlags_RootAndChildWindows},
        {"ImGuiHoveredFlags_None",                            ImGuiHoveredFlags_None},
        {"ImGuiHoveredFlags_ChildWindows",                    ImGuiHoveredFlags_ChildWindows},
        {"ImGuiHoveredFlags_RootWindow",                      ImGuiHoveredFlags_RootWindow},
        {"ImGuiHoveredFlags_AnyWindow",                       ImGuiHoveredFlags_AnyWindow},
        {"ImGuiHoveredFlags_AllowWhenBlockedByPopup",         ImGuiHoveredFlags_AllowWhenBlockedByPopup},
        {"ImGuiHoveredFlags_AllowWhenBlockedByActiveItem",    ImGuiHoveredFlags_AllowWhenBlockedByActiveItem},
        {"ImGuiHoveredFlags_AllowWhenOverlapped",             ImGuiHoveredFlags_AllowWhenOverlapped},
        {"ImGuiHoveredFlags_AllowWhenDisabled",               ImGuiHoveredFlags_AllowWhenDisabled},
        {"ImGuiHoveredFlags_RectOnly",                        ImGuiHoveredFlags_RectOnly},
        {"ImGuiHoveredFlags_RootAndChildWindows",             ImGuiHoveredFlags_RootAndChildWindows},
        {"ImGuiDragDropFlags_None",                           ImGuiDragDropFlags_None},
        {"ImGuiDragDropFlags_SourceNoPreviewTooltip",         ImGuiDragDropFlags_SourceNoPreviewTooltip},
        {"ImGuiDragDropFlags_SourceNoDisableHover",           ImGuiDragDropFlags_SourceNoDisableHover},
        {"ImGuiDragDropFlags_SourceNoHoldToOpenOthers",       ImGuiDragDropFlags_SourceNoHoldToOpenOthers},
        {"ImGuiDragDropFlags_SourceAllowNullID",              ImGuiDragDropFlags_SourceAllowNullID},
        {"ImGuiDragDropFlags_SourceExtern",                   ImGuiDragDropFlags_SourceExtern},
        {"ImGuiDragDropFlags_SourceAutoExpirePayload",        ImGuiDragDropFlags_SourceAutoExpirePayload},
        {"ImGuiDragDropFlags_AcceptBeforeDelivery",           ImGuiDragDropFlags_AcceptBeforeDelivery},
        {"ImGuiDragDropFlags_AcceptNoDrawDefaultRect",        ImGuiDragDropFlags_AcceptNoDrawDefaultRect},
        {"ImGuiDragDropFlags_AcceptNoPreviewTooltip",         ImGuiDragDropFlags_AcceptNoPreviewTooltip},
        {"ImGuiDragDropFlags_AcceptPeekOnly",                 ImGuiDragDropFlags_AcceptPeekOnly},
        {"ImGuiDir_None",                                     ImGuiDir_None},
        {"ImGuiDir_Left",                                     ImGuiDir_Left},
        {"ImGuiDir_Right",                                    ImGuiDir_Right},
        {"ImGuiDir_Up",                                       ImGuiDir_Up},
        {"ImGuiDir_Down",                                     ImGuiDir_Down},
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
        {"ImGuiKey_KeyPadEnter",                              ImGuiKey_KeyPadEnter},
        {"ImGuiKey_A",                                        ImGuiKey_A},
        {"ImGuiKey_C",                                        ImGuiKey_C},
        {"ImGuiKey_V",                                        ImGuiKey_V},
        {"ImGuiKey_X",                                        ImGuiKey_X},
        {"ImGuiKey_Y",                                        ImGuiKey_Y},
        {"ImGuiKey_Z",                                        ImGuiKey_Z},
        {"ImGuiNavInput_Activate",                            ImGuiNavInput_Activate},
        {"ImGuiNavInput_Cancel",                              ImGuiNavInput_Cancel},
        {"ImGuiNavInput_Input",                               ImGuiNavInput_Input},
        {"ImGuiNavInput_Menu",                                ImGuiNavInput_Menu},
        {"ImGuiNavInput_DpadLeft",                            ImGuiNavInput_DpadLeft},
        {"ImGuiNavInput_DpadRight",                           ImGuiNavInput_DpadRight},
        {"ImGuiNavInput_DpadUp",                              ImGuiNavInput_DpadUp},
        {"ImGuiNavInput_DpadDown",                            ImGuiNavInput_DpadDown},
        {"ImGuiNavInput_LStickLeft",                          ImGuiNavInput_LStickLeft},
        {"ImGuiNavInput_LStickRight",                         ImGuiNavInput_LStickRight},
        {"ImGuiNavInput_LStickUp",                            ImGuiNavInput_LStickUp},
        {"ImGuiNavInput_LStickDown",                          ImGuiNavInput_LStickDown},
        {"ImGuiNavInput_FocusPrev",                           ImGuiNavInput_FocusPrev},
        {"ImGuiNavInput_FocusNext",                           ImGuiNavInput_FocusNext},
        {"ImGuiNavInput_TweakSlow",                           ImGuiNavInput_TweakSlow},
        {"ImGuiNavInput_TweakFast",                           ImGuiNavInput_TweakFast},
        {"ImGuiConfigFlags_None",                             ImGuiConfigFlags_None},
        {"ImGuiConfigFlags_NavEnableKeyboard",                ImGuiConfigFlags_NavEnableKeyboard},
        {"ImGuiConfigFlags_NavEnableGamepad",                 ImGuiConfigFlags_NavEnableGamepad},
        {"ImGuiConfigFlags_NavEnableSetMousePos",             ImGuiConfigFlags_NavEnableSetMousePos},
        {"ImGuiConfigFlags_NavNoCaptureKeyboard",             ImGuiConfigFlags_NavNoCaptureKeyboard},
        {"ImGuiConfigFlags_NoMouse",                          ImGuiConfigFlags_NoMouse},
        {"ImGuiConfigFlags_NoMouseCursorChange",              ImGuiConfigFlags_NoMouseCursorChange},
        {"ImGuiConfigFlags_IsSRGB",                           ImGuiConfigFlags_IsSRGB},
        {"ImGuiConfigFlags_IsTouchScreen",                    ImGuiConfigFlags_IsTouchScreen},
        {"ImGuiBackendFlags_None",                            ImGuiBackendFlags_None},
        {"ImGuiBackendFlags_HasGamepad",                      ImGuiBackendFlags_HasGamepad},
        {"ImGuiBackendFlags_HasMouseCursors",                 ImGuiBackendFlags_HasMouseCursors},
        {"ImGuiBackendFlags_HasSetMousePos",                  ImGuiBackendFlags_HasSetMousePos},
        {"ImGuiBackendFlags_RendererHasVtxOffset",            ImGuiBackendFlags_RendererHasVtxOffset},
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
        {"ImGuiCol_TabActive",                                ImGuiCol_TabActive},
        {"ImGuiCol_TabUnfocused",                             ImGuiCol_TabUnfocused},
        {"ImGuiCol_TabUnfocusedActive",                       ImGuiCol_TabUnfocusedActive},
        {"ImGuiCol_PlotLines",                                ImGuiCol_PlotLines},
        {"ImGuiCol_PlotLinesHovered",                         ImGuiCol_PlotLinesHovered},
        {"ImGuiCol_PlotHistogram",                            ImGuiCol_PlotHistogram},
        {"ImGuiCol_PlotHistogramHovered",                     ImGuiCol_PlotHistogramHovered},
        {"ImGuiCol_TextSelectedBg",                           ImGuiCol_TextSelectedBg},
        {"ImGuiCol_DragDropTarget",                           ImGuiCol_DragDropTarget},
        {"ImGuiCol_NavHighlight",                             ImGuiCol_NavHighlight},
        {"ImGuiCol_NavWindowingHighlight",                    ImGuiCol_NavWindowingHighlight},
        {"ImGuiCol_NavWindowingDimBg",                        ImGuiCol_NavWindowingDimBg},
        {"ImGuiCol_ModalWindowDimBg",                         ImGuiCol_ModalWindowDimBg},
        {"ImGuiStyleVar_Alpha",                               ImGuiStyleVar_Alpha},
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
        {"ImGuiStyleVar_ScrollbarSize",                       ImGuiStyleVar_ScrollbarSize},
        {"ImGuiStyleVar_ScrollbarRounding",                   ImGuiStyleVar_ScrollbarRounding},
        {"ImGuiStyleVar_GrabMinSize",                         ImGuiStyleVar_GrabMinSize},
        {"ImGuiStyleVar_GrabRounding",                        ImGuiStyleVar_GrabRounding},
        {"ImGuiStyleVar_TabRounding",                         ImGuiStyleVar_TabRounding},
        {"ImGuiStyleVar_ButtonTextAlign",                     ImGuiStyleVar_ButtonTextAlign},
        {"ImGuiStyleVar_SelectableTextAlign",                 ImGuiStyleVar_SelectableTextAlign},
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
        {"ImGuiColorEditFlags_AlphaPreview",                  ImGuiColorEditFlags_AlphaPreview},
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
        {"ImGuiColorEditFlags_InputHSV",                      ImGuiColorEditFlags_InputHSV},
        {"ImGuiColorEditFlags_DefaultOptions_",               ImGuiColorEditFlags_DefaultOptions_},
        {"ImGuiMouseButton_Left",                             ImGuiMouseButton_Left},
        {"ImGuiMouseButton_Right",                            ImGuiMouseButton_Right},
        {"ImGuiMouseButton_Middle",                           ImGuiMouseButton_Middle},
        {"ImGuiMouseButton_COUNT",                            ImGuiMouseButton_COUNT},
        {"ImGuiMouseCursor_None",                             ImGuiMouseCursor_None},
        {"ImGuiMouseCursor_Arrow",                            ImGuiMouseCursor_Arrow},
        {"ImGuiMouseCursor_TextInput",                        ImGuiMouseCursor_TextInput},
        {"ImGuiMouseCursor_ResizeAll",                        ImGuiMouseCursor_ResizeAll},
        {"ImGuiMouseCursor_ResizeNS",                         ImGuiMouseCursor_ResizeNS},
        {"ImGuiMouseCursor_ResizeEW",                         ImGuiMouseCursor_ResizeEW},
        {"ImGuiMouseCursor_ResizeNESW",                       ImGuiMouseCursor_ResizeNESW},
        {"ImGuiMouseCursor_ResizeNWSE",                       ImGuiMouseCursor_ResizeNWSE},
        {"ImGuiMouseCursor_Hand",                             ImGuiMouseCursor_Hand},
        {"ImGuiMouseCursor_NotAllowed",                       ImGuiMouseCursor_NotAllowed},
        {"ImGuiCond_Always",                                  ImGuiCond_Always},
        {"ImGuiCond_Once",                                    ImGuiCond_Once},
        {"ImGuiCond_FirstUseEver",                            ImGuiCond_FirstUseEver},
        {"ImGuiCond_Appearing",                               ImGuiCond_Appearing},
        {"ImDrawCornerFlags_None",                            ImDrawCornerFlags_None},
        {"ImDrawCornerFlags_TopLeft",                         ImDrawCornerFlags_TopLeft},
        {"ImDrawCornerFlags_TopRight",                        ImDrawCornerFlags_TopRight},
        {"ImDrawCornerFlags_BotLeft",                         ImDrawCornerFlags_BotLeft},
        {"ImDrawCornerFlags_BotRight",                        ImDrawCornerFlags_BotRight},
        {"ImDrawCornerFlags_Top",                             ImDrawCornerFlags_Top},
        {"ImDrawCornerFlags_Bot",                             ImDrawCornerFlags_Bot},
        {"ImDrawCornerFlags_Left",                            ImDrawCornerFlags_Left},
        {"ImDrawCornerFlags_Right",                           ImDrawCornerFlags_Right},
        {"ImDrawCornerFlags_All",                             ImDrawCornerFlags_All},
        {"ImDrawListFlags_None",                              ImDrawListFlags_None},
        {"ImDrawListFlags_AntiAliasedLines",                  ImDrawListFlags_AntiAliasedLines},
        {"ImDrawListFlags_AntiAliasedFill",                   ImDrawListFlags_AntiAliasedFill},
        {"ImDrawListFlags_AllowVtxOffset",                    ImDrawListFlags_AllowVtxOffset}
    };

static const std::map<std::string,ImGuiCol_> ImGuiCol_map = {
                {"ImGuiCol_Text"                       , ImGuiCol_Text},
                {"ImGuiCol_TextDisabled"               , ImGuiCol_TextDisabled},
                {"ImGuiCol_WindowBg"                   , ImGuiCol_WindowBg},
                {"ImGuiCol_ChildBg"                    , ImGuiCol_ChildBg},
                {"ImGuiCol_PopupBg"                    , ImGuiCol_PopupBg},
                {"ImGuiCol_Border"                     , ImGuiCol_Border},
                {"ImGuiCol_BorderShadow"               , ImGuiCol_BorderShadow},
                {"ImGuiCol_FrameBg"                    , ImGuiCol_FrameBg},
                {"ImGuiCol_FrameBgHovered"             , ImGuiCol_FrameBgHovered},
                {"ImGuiCol_FrameBgActive"              , ImGuiCol_FrameBgActive},
                {"ImGuiCol_TitleBg"                    , ImGuiCol_TitleBg},
                {"ImGuiCol_TitleBgActive"              , ImGuiCol_TitleBgActive},
                {"ImGuiCol_TitleBgCollapsed"           , ImGuiCol_TitleBgCollapsed},
                {"ImGuiCol_MenuBarBg"                  , ImGuiCol_MenuBarBg},
                {"ImGuiCol_ScrollbarBg"                , ImGuiCol_ScrollbarBg},
                {"ImGuiCol_ScrollbarGrab"              , ImGuiCol_ScrollbarGrab},
                {"ImGuiCol_ScrollbarGrabHovered"       , ImGuiCol_ScrollbarGrabHovered},
                {"ImGuiCol_ScrollbarGrabActive"        , ImGuiCol_ScrollbarGrabActive},
                {"ImGuiCol_CheckMark"                  , ImGuiCol_CheckMark},
                {"ImGuiCol_SliderGrab"                 , ImGuiCol_SliderGrab},
                {"ImGuiCol_SliderGrabActive"           , ImGuiCol_SliderGrabActive},
                {"ImGuiCol_Button"                     , ImGuiCol_Button},
                {"ImGuiCol_ButtonHovered"              , ImGuiCol_ButtonHovered},
                {"ImGuiCol_ButtonActive"               , ImGuiCol_ButtonActive},
                {"ImGuiCol_Header"                     , ImGuiCol_Header},
                {"ImGuiCol_HeaderHovered"              , ImGuiCol_HeaderHovered},
                {"ImGuiCol_HeaderActive"               , ImGuiCol_HeaderActive},
                {"ImGuiCol_Separator"                  , ImGuiCol_Separator},
                {"ImGuiCol_SeparatorHovered"           , ImGuiCol_SeparatorHovered},
                {"ImGuiCol_SeparatorActive"            , ImGuiCol_SeparatorActive},
                {"ImGuiCol_ResizeGrip"                 , ImGuiCol_ResizeGrip},
                {"ImGuiCol_ResizeGripHovered"          , ImGuiCol_ResizeGripHovered},
                {"ImGuiCol_ResizeGripActive"           , ImGuiCol_ResizeGripActive},
                {"ImGuiCol_Tab"                        , ImGuiCol_Tab},
                {"ImGuiCol_TabHovered"                 , ImGuiCol_TabHovered},
                {"ImGuiCol_TabActive"                  , ImGuiCol_TabActive},
                {"ImGuiCol_TabUnfocused"               , ImGuiCol_TabUnfocused},
                {"ImGuiCol_TabUnfocusedActive"         , ImGuiCol_TabUnfocusedActive},
                {"ImGuiCol_PlotLines"                  , ImGuiCol_PlotLines},
                {"ImGuiCol_PlotLinesHovered"           , ImGuiCol_PlotLinesHovered},
                {"ImGuiCol_PlotHistogram"              , ImGuiCol_PlotHistogram},
                {"ImGuiCol_PlotHistogramHovered"       , ImGuiCol_PlotHistogramHovered},
                {"ImGuiCol_TextSelectedBg"             , ImGuiCol_TextSelectedBg},
                {"ImGuiCol_DragDropTarget"             , ImGuiCol_DragDropTarget},
                {"ImGuiCol_NavHighlight"               , ImGuiCol_NavHighlight},
                {"ImGuiCol_NavWindowingHighlight"      , ImGuiCol_NavWindowingHighlight},
                {"ImGuiCol_NavWindowingDimBg"          , ImGuiCol_NavWindowingDimBg},
                {"ImGuiCol_ModalWindowDimBg"           , ImGuiCol_ModalWindowDimBg}};

/*const int get_texture_id(lua_State *lua,const char* texture_name,unsigned int & width_out, unsigned int & height_out)
{
    const int top  = lua_gettop(lua);
    int texture_id = 0;
    lua_getglobal(lua,"mbm");
    if(lua_type(lua,-1) == LUA_TTABLE)
    {
        lua_getfield(lua,-1,"getTextureId");
        if(lua_isfunction(lua,-1))
        {
            lua_pushstring(lua,texture_name);
            lua_pushboolean(lua,true);
            constexpr int nargs    = 2;
            constexpr int nresults = 3;
            if(lua_pcall(lua,nargs,nresults,0) == LUA_OK )
            {
                if(lua_type(lua,-1) == LUA_TNUMBER)
                    texture_id = lua_tointeger(lua,-3);
                if(lua_type(lua,-1) == LUA_TNUMBER)
                    width_out = lua_tointeger(lua,-2);
                if(lua_type(lua,-1) == LUA_TNUMBER)
                    height_out = lua_tointeger(lua,-1);
            }
            else
            {
                lua_log_error(lua, lua_tostring(lua,-1));
            }
        }
    }
    const int total_in_stack = lua_gettop(lua);
    if(total_in_stack > top)
    {
        const int total_pop = total_in_stack - top;
        lua_pop(lua,total_pop);
    }
    return texture_id;
}*/

void printStack(lua_State *lua, const char *fileName, const unsigned int numLine)
{
    std::string stack("\n**********************************"
                        "\nState of Stack\n");
    int top = lua_gettop(lua);
    for (int i = 1, k = top; i <= top; i++, --k)
    {
        char str[255];
        int  type = lua_type(lua, i);
        snprintf(str, sizeof(str), "\t%d| %8s |%d\n", -k, lua_typename(lua, type), i);
        stack += str;
    }
    stack += "**********************************\n\n";
    printf("%d:%s,%s", numLine, fileName, stack.c_str());
}

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
                printStack(lua,ar.short_src,ar.currentline);
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

ImFont * lua_pop_ImFontConfig_pointer(lua_State *lua, const int index, ImFont * in_out_ImFont);
ImFont * lua_pop_ImFont_pointer(lua_State *lua, const int index,ImFont * p_ImFont);
ImFont lua_pop_ImFont(lua_State *lua, const int index);
void lua_get_rgba_FromTable(lua_State * lua, int index, float p_col[4]);
ImVec4 lua_get_rgba_to_ImVec4_fromTable(lua_State * lua,const int index);
void lua_push_rgba(lua_State * lua, const float p_col[4]);
void lua_push_rgba(lua_State * lua, const ImVec4 & color);
ImFontAtlas * lua_pop_ImFontAtlas_pointer(lua_State *lua, const int index,ImFontAtlas * p_ImFontAtlas);
ImFontAtlas * lua_pop_ImFont_pointer(lua_State *lua, const int index, ImFontAtlas * in_out_ImFontAtlas);
ImFontAtlas lua_pop_ImFontAtlas(lua_State *lua, const int index);
ImFontConfig * lua_pop_ImFontConfig_pointer(lua_State *lua, const int index,ImFontConfig * p_ImFontConfig);
ImFontConfig * lua_pop_ImFont_pointer(lua_State *lua, const int index, ImFontConfig * in_out_ImFontConfig);
ImFontConfig lua_pop_ImFontConfig(lua_State *lua, const int index);
ImFontGlyph * lua_pop_ImFontGlyph_pointer(lua_State *lua, const int index,ImFontGlyph * p_ImFontGlyph);
ImFontGlyph * lua_pop_ImFont_pointer(lua_State *lua, const int index, ImFontGlyph * in_out_ImFontGlyph);
ImFontGlyph lua_pop_ImFontGlyph(lua_State *lua, const int index);
ImGuiStyle * lua_pop_ImGuiStyle_pointer(lua_State *lua, const int index,ImGuiStyle * p_ImGuiStyle);
ImVec2 * lua_pop_ImVec2_pointer(lua_State *lua, const int index,ImVec2 * p_ImVec2);
ImVec2 lua_pop_ImVec2(lua_State *lua, const int index);
ImVec4 * lua_pop_ImVec4_pointer(lua_State *lua, const int index,ImVec4 * p_ImVec4);
ImVec4 lua_pop_ImVec4(lua_State *lua, const int index);
ImWchar * lua_pop_ImFontConfig_pointer(lua_State *lua, const int index, ImWchar * in_out_ImWchar);
void lua_push_ImFont(lua_State *lua, const ImFont & in);
void lua_push_ImFontAtlas(lua_State *lua, const ImFontAtlas & in);
void lua_push_ImFontAtlas_pointer(lua_State *lua,const ImFontAtlas * p_ImFontAtlas);
void lua_push_ImFontConfig(lua_State *lua, const ImFontConfig & in);
void lua_push_ImFontConfig_pointer(lua_State *lua,const ImFontConfig * p_ImFontConfig);
void lua_push_ImFontGlyph(lua_State *lua, const ImFontGlyph & in);
void lua_push_ImFontGlyph_pointer(lua_State *lua,const ImFontGlyph * p_ImFontGlyph);
void lua_push_ImFont_pointer(lua_State *lua,const ImFont * p_ImFont);
void lua_push_ImGuiPayload(lua_State *lua, const ImGuiPayload & in);
void lua_push_ImGuiStyle(lua_State *lua, const ImGuiStyle & in);
void lua_push_ImVec2(lua_State *lua, const ImVec2 & in);
void lua_push_ImVec2_pointer(lua_State *lua,const ImVec2 * p_ImVec2);
void lua_push_ImVec4(lua_State *lua, const ImVec4 & in);
void lua_push_ImVec4_pointer(lua_State *lua,const ImVec4 * p_ImVec4);



#if defined(__linux__) && !defined (ANDROID)
    #define VKL_TAB          0x09
    #define VKL_LEFT         0x25
    #define VKL_RIGHT        0x27
    #define VKL_UP           0x26
    #define VKL_DOWN         0x28
    #define VKL_PRIOR        0x21
    #define VKL_NEXT         0x22
    #define VKL_HOME         0x24
    #define VKL_END          0x23
    #define VKL_INSERT       0x2D
    #define VKL_DELETE       0x2E
    #define VKL_BACK         0x08
    #define VKL_SPACE        0x20
    #define VKL_RETURN       0x0D
    #define VKL_ESCAPE       0x1B
    #define VKL_ENTER        0x0D
#endif

#ifdef  PLUGIN_CALLBACK
    class IMGUI_LUA : public PLUGIN // structure that represent the wrapper
#else
    class IMGUI_LUA
#endif
{
public:

    

    IMGUI_LUA():KEY_SPACE(' '),KEY_0('0'),KEY_1('1'),KEY_9('9'),KEY_A('A'),KEY_Z('Z')
    {
        imGuiContext            = nullptr;
        delta                   = 0;//updated each loop
        sx                      = 1.0f;
        sy                      = 1.0f;
        key_mouse::mouse_wheel  = 0.0f;
        key_mouse::KeyCtrl      = false;
        key_mouse::KeyShift     = false;
        key_mouse::KeyAlt       = false;
        key_mouse::KeySuper     = false;
        key_mouse::KeyCapital   = false;
        context                 = nullptr;

        memset(key_mouse::MouseDown,0,sizeof(key_mouse::MouseDown));
        memset(key_mouse::KeysDown,0,sizeof(key_mouse::KeysDown));
        MousePos.x     = 0;
        MousePos.y     = 0;
        MousePosPrev.x = 0;
        MousePosPrev.y = 0;
    }
    const int       KEY_SPACE,KEY_0,KEY_1,KEY_9,KEY_A,KEY_Z;
    float           delta,sx,sy;
    
    ImVec2 MousePos,MousePosPrev;

    ImGuiContext*   imGuiContext;
    #if defined(__linux__) && !defined (ANDROID)
        Display*    context;
    #elif defined(_WIN32)
        HWND        context;
    #elif defined(ANDROID)
        JNIEnv*     context;
    #endif

    #ifdef  PLUGIN_CALLBACK

    void onSubscribe(int width,int height, void * _context)
    {
        IMGUI_CHECKVERSION();
        imGuiContext = ImGui::CreateContext();
        if(imGuiContext != nullptr)
        {
            // Setup Dear ImGui style
            ImGui::StyleColorsDark();
            ImGuiIO& imGuIo = ImGui::GetIO();
            //imgui_impl_opengl3.cpp
            //ImGui_ImplOpenGL3_Init("#version 110");
            //#if defined _WIN32
            //    glShaderSource(g_VertHandle, 1, &vertex_shader, nullptr);
            //#else
            //    glShaderSource(g_VertHandle, 2, vertex_shader_with_version, nullptr);
            //#endif
            ImGui_ImplOpenGL3_Init("#version 100");

            #if defined _WIN32
                context = static_cast<HWND>(_context);
                ImGui_ImplWin32_Init(context);
            #elif defined(__linux__) && !defined (ANDROID)
                context = static_cast<Display*>(_context);
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

            
            // Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array.
        #if defined _WIN32
            imGuIo.KeyMap[ImGuiKey_Tab]         = VK_TAB;
            imGuIo.KeyMap[ImGuiKey_LeftArrow]   = VK_LEFT;
            imGuIo.KeyMap[ImGuiKey_RightArrow]  = VK_RIGHT;
            imGuIo.KeyMap[ImGuiKey_UpArrow]     = VK_UP;
            imGuIo.KeyMap[ImGuiKey_DownArrow]   = VK_DOWN;
            imGuIo.KeyMap[ImGuiKey_PageUp]      = VK_PRIOR;
            imGuIo.KeyMap[ImGuiKey_PageDown]    = VK_NEXT;
            imGuIo.KeyMap[ImGuiKey_Home]        = VK_HOME;
            imGuIo.KeyMap[ImGuiKey_End]         = VK_END;
            imGuIo.KeyMap[ImGuiKey_Insert]      = VK_INSERT;
            imGuIo.KeyMap[ImGuiKey_Delete]      = VK_DELETE;
            imGuIo.KeyMap[ImGuiKey_Backspace]   = VK_BACK;
            imGuIo.KeyMap[ImGuiKey_Space]       = VK_SPACE;
            imGuIo.KeyMap[ImGuiKey_Enter]       = VK_RETURN;
            imGuIo.KeyMap[ImGuiKey_Escape]      = VK_ESCAPE;
            imGuIo.KeyMap[ImGuiKey_KeyPadEnter] = 0XD;
            imGuIo.KeyMap[ImGuiKey_A]           = 0x41;
            imGuIo.KeyMap[ImGuiKey_C]           = 0x43;
            imGuIo.KeyMap[ImGuiKey_V]           = 0x56;
            imGuIo.KeyMap[ImGuiKey_X]           = 0x58;
            imGuIo.KeyMap[ImGuiKey_Y]           = 0x59;
            imGuIo.KeyMap[ImGuiKey_Z]           = 0x5A;
        #elif defined(__linux__) && !defined (ANDROID)
            imGuIo.KeyMap[ImGuiKey_Tab]         = VKL_TAB;//XK_Tab;
            imGuIo.KeyMap[ImGuiKey_LeftArrow]   = VKL_LEFT;//XK_Left;
            imGuIo.KeyMap[ImGuiKey_RightArrow]  = VKL_RIGHT;//XK_Right;
            imGuIo.KeyMap[ImGuiKey_UpArrow]     = VKL_UP;//XK_Up;
            imGuIo.KeyMap[ImGuiKey_DownArrow]   = VKL_DOWN;//XK_Down;
            imGuIo.KeyMap[ImGuiKey_PageUp]      = VKL_PRIOR;//XK_Page_Up;
            imGuIo.KeyMap[ImGuiKey_PageDown]    = VKL_NEXT;//XK_Page_Down;
            imGuIo.KeyMap[ImGuiKey_Home]        = VKL_HOME;//XK_Home;
            imGuIo.KeyMap[ImGuiKey_End]         = VKL_END;//XK_End;
            imGuIo.KeyMap[ImGuiKey_Insert]      = VKL_INSERT;//XK_Insert;
            imGuIo.KeyMap[ImGuiKey_Delete]      = VKL_DELETE;//XK_Delete;
            imGuIo.KeyMap[ImGuiKey_Backspace]   = VKL_BACK;//XK_BackSpace;
            imGuIo.KeyMap[ImGuiKey_Space]       = VKL_SPACE;//' ';
            imGuIo.KeyMap[ImGuiKey_Enter]       = VKL_RETURN;//XK_Return;
            imGuIo.KeyMap[ImGuiKey_Escape]      = VKL_ESCAPE;//XK_Escape;
            imGuIo.KeyMap[ImGuiKey_KeyPadEnter] = VKL_ENTER;//XK_KP_Enter;
            imGuIo.KeyMap[ImGuiKey_A]           = 'A';
            imGuIo.KeyMap[ImGuiKey_C]           = 'C';
            imGuIo.KeyMap[ImGuiKey_V]           = 'V';
            imGuIo.KeyMap[ImGuiKey_X]           = 'X';
            imGuIo.KeyMap[ImGuiKey_Y]           = 'Y';
            imGuIo.KeyMap[ImGuiKey_Z]           = 'Z';
        #endif
            key_mouse::KeyCapital               = isCapsLockOn();
            onBeginRender();//first render
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
        if(imGuiContext)
        {
            x *= sx;
            y *= sy;
            MousePos.x = x;
            MousePos.y = y;
            if(key >= 0 && key < key_mouse::size_mouse_struct_img)
            {
                key_mouse::MouseDown[key]          = true;//Input - Fill before calling NewFrame()
            }
        }
    }

    void onTouchUp(int key, float x, float y)
    {
        if(imGuiContext)
        {
            x *= sx;
            y *= sy;
            MousePos.x = x;
            MousePos.y = y;
            if(key >= 0 && key < key_mouse::size_mouse_struct_img)
            {
                key_mouse::MouseDown[key]     = false;
            }
        }
    }

    void onTouchMove(int key, float x, float y)
    {
        if(imGuiContext)
        {
            x *= sx;
            y *= sy;
            MousePosPrev.x  = MousePos.x;
            MousePosPrev.y  = MousePos.y;
            MousePos.x      = x;
            MousePos.y      = y;
        }
    }

    void onTouchZoom(float zoom)
    {
        key_mouse::mouse_wheel        = zoom;
    }
    
    void onKeyDown(int key)
    {
        if(imGuiContext)
        {
            ImGuiIO& imGuIo  = ImGui::GetIO();
            if (key >= 0 && key < key_mouse::size_key_struct_img)
            {
                key_mouse::KeysDown[key]   = true;
            }
            #if defined(__linux__) && !defined (ANDROID)
            else
            {
                switch(key)
                {
                    case XK_Tab:        key_mouse::KeysDown[VKL_TAB]       = true; break;
                    case XK_Left:       key_mouse::KeysDown[VKL_LEFT]      = true; break;
                    case XK_Right:      key_mouse::KeysDown[VKL_RIGHT]     = true; break;
                    case XK_Up:         key_mouse::KeysDown[VKL_UP]        = true; break;
                    case XK_Down:       key_mouse::KeysDown[VKL_DOWN]      = true; break;
                    case XK_Page_Up:    key_mouse::KeysDown[VKL_PRIOR]     = true; break;
                    case XK_Page_Down:  key_mouse::KeysDown[VKL_NEXT]      = true; break;
                    case XK_Home:       key_mouse::KeysDown[VKL_HOME]      = true; break;
                    case XK_End:        key_mouse::KeysDown[VKL_END]       = true; break;
                    case XK_Insert:     key_mouse::KeysDown[VKL_INSERT]    = true; break;
                    case XK_Delete:     key_mouse::KeysDown[VKL_DELETE]    = true; break;
                    case XK_BackSpace:  key_mouse::KeysDown[VKL_BACK]      = true; break;
                    case VKL_SPACE:     key_mouse::KeysDown[VKL_SPACE]     = true; break;
                    case XK_Return:     key_mouse::KeysDown[VKL_RETURN]    = true; break;
                    case XK_Escape:     key_mouse::KeysDown[VKL_ESCAPE]    = true; break;
                    case XK_KP_Enter:   key_mouse::KeysDown[VKL_RETURN]    = true; break;
                }
            }
            #endif
            
            #if defined(_WIN32)
            switch(key)
            {
                case VK_CONTROL:
                case VK_LCONTROL:
                case VK_RCONTROL:       key_mouse::KeyCtrl    = true;             break;
                case VK_SHIFT:
                case VK_LSHIFT:
                case VK_RSHIFT:         key_mouse::KeyShift   = true;             break;
                case VK_MENU:
                case VK_LMENU:
                case VK_RMENU:          key_mouse::KeyAlt     = true;             break;
                case VK_APPS:
                case VK_LWIN:
                case VK_RWIN:           key_mouse::KeySuper   = true;             break;
                case VK_CAPITAL:        key_mouse::KeyCapital = isCapsLockOn();   break;                  
                case VK_MULTIPLY:       imGuIo.AddInputCharacter('*'); break;
                case VK_ADD:            imGuIo.AddInputCharacter('+'); break;
                case VK_SUBTRACT:       imGuIo.AddInputCharacter('-'); break;
                case VK_DIVIDE:         imGuIo.AddInputCharacter('/'); break;
                case 0xC2:              imGuIo.AddInputCharacter('.'); break;
                case VK_DECIMAL:        imGuIo.AddInputCharacter('.'); break;
                case VK_SPACE:          imGuIo.AddInputCharacter(' '); break;
                case VK_OEM_102:        imGuIo.AddInputCharacter('\\'); break;
                case VK_OEM_1:
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter(':');
                    else
                        imGuIo.AddInputCharacter(';');
                };
                break;
                case VK_OEM_COMMA:
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('<');
                    else
                        imGuIo.AddInputCharacter(',');
                };
                break;
                case VK_OEM_MINUS:
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('_');
                    else
                        imGuIo.AddInputCharacter('-');
                };
                break;
                case VK_OEM_PERIOD:
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('>');
                    else
                        imGuIo.AddInputCharacter('.');
                };
                break;
                case VK_OEM_PLUS:
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('+');
                    else
                        imGuIo.AddInputCharacter('=');
                };
                break;
                case VK_OEM_2:
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('?');
                    else
                        imGuIo.AddInputCharacter('/');
                };
                break;
                case VK_OEM_3:
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('~');
                    else
                        imGuIo.AddInputCharacter('`');
                };
                break;
                case VK_OEM_4:
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('{');
                    else
                        imGuIo.AddInputCharacter('[');
                };
                break;
                case VK_OEM_5:
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('|');
                    else
                        imGuIo.AddInputCharacter('\\');
                }
                break;
                case VK_OEM_6:
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('}');
                    else
                        imGuIo.AddInputCharacter(']');
                };
            };
            #elif defined(__linux__) && !defined (ANDROID)
            switch(key)
            {
                case XK_Control_L:
                case XK_Control_R:      key_mouse::KeyCtrl     = true; break;
                case XK_Shift_L:
                case XK_Shift_R:        key_mouse::KeyShift    = true; break;
                case XK_Alt_L:
                case XK_Alt_R:          key_mouse::KeyAlt      = true; break;
                case XK_Super_L:
                case XK_Super_R:        key_mouse::KeySuper    = true; break;
                case XK_Caps_Lock:      key_mouse::KeyCapital  = isCapsLockOn();  break;                  
                case XK_KP_Multiply:    imGuIo.AddInputCharacter('*'); break; // ok
                case XK_KP_Add:         imGuIo.AddInputCharacter('+'); break; // ok
                case XK_KP_Subtract:    imGuIo.AddInputCharacter('-'); break; // ok
                case XK_KP_Divide:      imGuIo.AddInputCharacter('/'); break; // ok
                case XK_KP_Decimal:     imGuIo.AddInputCharacter('.'); break; // ok
                case XK_KP_Delete:      imGuIo.AddInputCharacter('.'); break; // ok
                case VKL_SPACE:         imGuIo.AddInputCharacter(' '); break; // ok
                case 0x003b: //XK_semicolon
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter(':');
                    else
                        imGuIo.AddInputCharacter(';');
                };
                break;
                case 0x002c: //XK_comma
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('<');
                    else
                        imGuIo.AddInputCharacter(',');
                };
                break;
                case 0x002d: //XK_minus
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('_');
                    else
                        imGuIo.AddInputCharacter('-');
                };
                break;
                case 0x002e: //XK_period
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('>');
                    else
                        imGuIo.AddInputCharacter('.');
                };
                break;
                case 0x003d: //XK_equal
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('+');
                    else
                        imGuIo.AddInputCharacter('=');
                };
                break;
                case 0x002f: //XK_slash
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('?');
                    else
                        imGuIo.AddInputCharacter('/');
                };
                break;
                case 0x0060: //XK_grave
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('~');
                    else
                        imGuIo.AddInputCharacter('`');
                };
                break;
                case 0x005b: // XK_bracketleft
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('{');
                    else
                        imGuIo.AddInputCharacter('[');
                };
                break;
                case 0x005c: // XK_backslash
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('|');
                    else
                        imGuIo.AddInputCharacter('\\');
                }
                break;
                case 0x005d: //XK_bracketright
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('}');
                    else
                        imGuIo.AddInputCharacter(']');
                };
                break;
                case 0x0027: //XK_apostrophe
                {
                    if(key_mouse::KeyShift)
                        imGuIo.AddInputCharacter('"');
                    else
                        imGuIo.AddInputCharacter('\'');
                };
            };
            #endif

            if (key >= KEY_0 && key <= KEY_9)// !"#$%&'()*+,-./ 0 - 9 Normal input from keyboard numeric
            {
                if(key_mouse::KeyShift == true )
                {
                    switch(key)
                    {
                        case '1': imGuIo.AddInputCharacter('!'); break;
                        case '2': imGuIo.AddInputCharacter('@'); break;
                        case '3': imGuIo.AddInputCharacter('#'); break;
                        case '4': imGuIo.AddInputCharacter('$'); break;
                        case '5': imGuIo.AddInputCharacter('%'); break;
                        case '6': imGuIo.AddInputCharacter('^'); break;
                        case '7': imGuIo.AddInputCharacter('&'); break;
                        case '8': imGuIo.AddInputCharacter('*'); break;
                        case '9': imGuIo.AddInputCharacter('('); break;
                        case '0': imGuIo.AddInputCharacter(')'); break;
                    }
                }
                else
                {
                    imGuIo.AddInputCharacter(key);
                }
            }
            else if(key >= KEY_A && key <= KEY_Z) // 'A' - 'Z'
            {
                if((key_mouse::KeyShift == true && key_mouse::KeyCapital == false)  || (key_mouse::KeyShift == false && key_mouse::KeyCapital == true))
                {
                    imGuIo.AddInputCharacter(key);
                }
                else
                {
                    const int nKey  = key - KEY_A;
                    const unsigned int c = static_cast<unsigned int>('a') + nKey;
                    imGuIo.AddInputCharacter(c);
                }
            }
            else// NUMPAD keys
            {
                #if defined ANDROID
                //Not implemented
                #elif defined __linux__
                // Linux Debian 8.3.0-6, Linux version 4.19.0-6-amd64
                switch (key)
                {
                    case 0xff9e: imGuIo.AddInputCharacter('0'); key_mouse::KeysDown['0'] = true; break; //
                    case 0xff9c: imGuIo.AddInputCharacter('1'); key_mouse::KeysDown['1'] = true; break; //
                    case 0xff99: imGuIo.AddInputCharacter('2'); key_mouse::KeysDown['2'] = true; break; //
                    case 0xff9b: imGuIo.AddInputCharacter('3'); key_mouse::KeysDown['3'] = true; break; //
                    case 0xff96: imGuIo.AddInputCharacter('4'); key_mouse::KeysDown['4'] = true; break; //
                    case 0xff9d: imGuIo.AddInputCharacter('5'); key_mouse::KeysDown['5'] = true; break; //
                    case 0xff98: imGuIo.AddInputCharacter('6'); key_mouse::KeysDown['6'] = true; break; //
                    case 0xff95: imGuIo.AddInputCharacter('7'); key_mouse::KeysDown['7'] = true; break; //
                    case 0xff97: imGuIo.AddInputCharacter('8'); key_mouse::KeysDown['8'] = true; break; //
                    case 0xff9a: imGuIo.AddInputCharacter('9'); key_mouse::KeysDown['9'] = true; break; //
                    default: break;
                }
                #elif defined _WIN32
                switch (key)
                {
                    case 0x60: imGuIo.AddInputCharacter('0');  key_mouse::KeysDown['0'] = true; break; // VK_NUMPAD0
                    case 0x61: imGuIo.AddInputCharacter('1');  key_mouse::KeysDown['1'] = true; break; // VK_NUMPAD1
                    case 0x62: imGuIo.AddInputCharacter('2');  key_mouse::KeysDown['2'] = true; break; // VK_NUMPAD2
                    case 0x63: imGuIo.AddInputCharacter('3');  key_mouse::KeysDown['3'] = true; break; // VK_NUMPAD3
                    case 0x64: imGuIo.AddInputCharacter('4');  key_mouse::KeysDown['4'] = true; break; // VK_NUMPAD4
                    case 0x65: imGuIo.AddInputCharacter('5');  key_mouse::KeysDown['5'] = true; break; // VK_NUMPAD5
                    case 0x66: imGuIo.AddInputCharacter('6');  key_mouse::KeysDown['6'] = true; break; // VK_NUMPAD6
                    case 0x67: imGuIo.AddInputCharacter('7');  key_mouse::KeysDown['7'] = true; break; // VK_NUMPAD7
                    case 0x68: imGuIo.AddInputCharacter('8');  key_mouse::KeysDown['8'] = true; break; // VK_NUMPAD8
                    case 0x69: imGuIo.AddInputCharacter('9');  key_mouse::KeysDown['9'] = true; break; // VK_NUMPAD9
                    default: break;
                }
                #endif
            }
        }
    }

    void onKeyUp(int key)
    {
        if (key >= 0 && key < key_mouse::size_key_struct_img)
        {
            key_mouse::KeysDown[key]   = false;
        }
        #if defined(__linux__) && !defined (ANDROID)
        else
        {
            switch(key)
            {
                case XK_Tab:            key_mouse::KeysDown[VKL_TAB]       = false; break;
                case XK_Left:           key_mouse::KeysDown[VKL_LEFT]      = false; break;
                case XK_Right:          key_mouse::KeysDown[VKL_RIGHT]     = false; break;
                case XK_Up:             key_mouse::KeysDown[VKL_UP]        = false; break;
                case XK_Down:           key_mouse::KeysDown[VKL_DOWN]      = false; break;
                case XK_Page_Up:        key_mouse::KeysDown[VKL_PRIOR]     = false; break;
                case XK_Page_Down:      key_mouse::KeysDown[VKL_NEXT]      = false; break;
                case XK_Home:           key_mouse::KeysDown[VKL_HOME]      = false; break;
                case XK_End:            key_mouse::KeysDown[VKL_END]       = false; break;
                case XK_Insert:         key_mouse::KeysDown[VKL_INSERT]    = false; break;
                case XK_Delete:         key_mouse::KeysDown[VKL_DELETE]    = false; break;
                case XK_BackSpace:      key_mouse::KeysDown[VKL_BACK]      = false; break;
                case VKL_SPACE:         key_mouse::KeysDown[VKL_SPACE]     = false; break;
                case XK_Return:         key_mouse::KeysDown[VKL_RETURN]    = false; break;
                case XK_Escape:         key_mouse::KeysDown[VKL_ESCAPE]    = false; break;
                case XK_KP_Enter:       key_mouse::KeysDown[VKL_RETURN]    = false; break;
                case XK_Control_L:
                case XK_Control_R:      key_mouse::KeyCtrl                 = false; break;
                case XK_Shift_L:
                case XK_Shift_R:        key_mouse::KeyShift                = false; break;
                case XK_Alt_L:
                case XK_Alt_R:          key_mouse::KeyAlt                  = false; break;
                case XK_Super_L:
                case XK_Super_R:        key_mouse::KeySuper                = false; break;
            }
        }
        #elif defined (_WIN32)
        switch(key)
        {
            case VK_CONTROL:
            case VK_LCONTROL:
            case VK_RCONTROL:           key_mouse::KeyCtrl    = false;             break;
            case VK_SHIFT:
            case VK_LSHIFT:
            case VK_RSHIFT:             key_mouse::KeyShift   = false;             break;
            case VK_MENU:
            case VK_LMENU:
            case VK_RMENU:              key_mouse::KeyAlt     = false;             break;
            case VK_APPS:
            case VK_LWIN:
            case VK_RWIN:               key_mouse::KeySuper   = false;             break;
            case VK_CAPITAL:            key_mouse::KeyCapital = isCapsLockOn();    break;
        }
        #endif
    }

    const bool isCapsLockOn()
    {
        #if defined(__linux__) || defined (ANDROID)
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
    void onBeginRender()
    {
        if(imGuiContext)
        {
            ImGuiIO& imGuIo         = ImGui::GetIO();
            imGuIo.MouseWheel       += key_mouse::mouse_wheel;
            imGuIo.KeyCtrl          = key_mouse::KeyCtrl;
            imGuIo.KeyShift         = key_mouse::KeyShift;
            imGuIo.KeyAlt           = key_mouse::KeyAlt;
            imGuIo.KeySuper         = key_mouse::KeySuper;

            memcpy(imGuIo.KeysDown,key_mouse::KeysDown,sizeof(key_mouse::KeysDown));
            memset(key_mouse::KeysDown,0,sizeof(key_mouse::KeysDown));

            ImGui_ImplOpenGL3_NewFrame();
            ImGui::NewFrame();

            key_mouse::mouse_wheel              = 0;
            imGuIo.DeltaTime                    = delta <= 0.0f ? 1.0f/60.0f : delta;
            imGuIo.WantCaptureMouse             = true;
            imGuIo.WantCaptureKeyboard          = true;

            memcpy(imGuIo.MouseDown,key_mouse::MouseDown,sizeof(key_mouse::MouseDown));
            imGuIo.MousePos.x      = MousePos.x;
            imGuIo.MousePos.y      = MousePos.y;
            imGuIo.MousePosPrev.x  = MousePosPrev.x;
            imGuIo.MousePosPrev.y  = MousePosPrev.y;
            
            mbm::DEVICE* device = mbm::DEVICE::getInstance();
            sx = device->camera.scale2d.x;
            sy = device->camera.scale2d.y;
        }
    }

    void onLoop(float d)
    {
        delta = d;
    }

    void onEndRender()
    {
        if(imGuiContext)
        {
            updateCursorMouse();
            ImGui::EndFrame();
            ImGui::Render();
            ImDrawData* draw_data = ImGui::GetDrawData();
            ImGui_ImplOpenGL3_RenderDrawData(draw_data);
        }
    }
    void onDestroy()
    {
        ImGui_ImplOpenGL3_Shutdown();
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
        #elif defined(__linux__) && !defined (ANDROID)
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
    #endif
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

void lua_push_ImGuiPayload(lua_State *lua, const ImGuiPayload & in)
{
    lua_newtable(lua);
    //lua_setfield(lua, -2, "Data");
    lua_pushinteger(lua,in.DataSize);
    lua_setfield(lua, -2, "DataSize");
    lua_pushinteger(lua,in.SourceId);
    lua_setfield(lua, -2, "SourceId");
    lua_pushinteger(lua,in.SourceParentId);
    lua_setfield(lua, -2, "SourceParentId");
    lua_pushinteger(lua,in.DataFrameCount);
    lua_setfield(lua, -2, "DataFrameCount");
    lua_pushlstring(lua,in.DataType,sizeof(in.DataType) / sizeof(in.DataType[0]));
    lua_setfield(lua, -2, "DataType");
    lua_pushboolean(lua,in.Preview);
    lua_setfield(lua, -2, "Preview");
    lua_pushboolean(lua,in.Delivery);
    lua_setfield(lua, -2, "Delivery");

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

void lua_push_ImFontConfig(lua_State *lua, const ImFontConfig & in)
{
    lua_newtable(lua);
    //const void FontData  = in.FontData != nullptr ? (*in.FontData) : 0;//TODO: 4 check here, apparently, "ImFontConfig.FontData" is a pointer
    //;
    //lua_setfield(lua, -2, "FontData");
    lua_pushinteger(lua,in.FontDataSize);
    lua_setfield(lua, -2, "FontDataSize");
    lua_pushboolean(lua,in.FontDataOwnedByAtlas);
    lua_setfield(lua, -2, "FontDataOwnedByAtlas");
    lua_pushinteger(lua,in.FontNo);
    lua_setfield(lua, -2, "FontNo");
    lua_pushnumber(lua,in.SizePixels);
    lua_setfield(lua, -2, "SizePixels");
    lua_pushinteger(lua,in.OversampleH);
    lua_setfield(lua, -2, "OversampleH");
    lua_pushinteger(lua,in.OversampleV);
    lua_setfield(lua, -2, "OversampleV");
    lua_pushboolean(lua,in.PixelSnapH);
    lua_setfield(lua, -2, "PixelSnapH");
    lua_push_ImVec2(lua,in.GlyphExtraSpacing);
    lua_setfield(lua, -2, "GlyphExtraSpacing");
    lua_push_ImVec2(lua,in.GlyphOffset);
    lua_setfield(lua, -2, "GlyphOffset");
    //lua_pushinteger_pointer(lua, in.GlyphRanges);//TODO: 6 check here, apparently, "ImFontConfig.GlyphRanges" is a pointer and might be nullptr 
    //lua_setfield(lua, -2, "GlyphRanges");
    lua_pushnumber(lua,in.GlyphMinAdvanceX);
    lua_setfield(lua, -2, "GlyphMinAdvanceX");
    lua_pushnumber(lua,in.GlyphMaxAdvanceX);
    lua_setfield(lua, -2, "GlyphMaxAdvanceX");
    lua_pushboolean(lua,in.MergeMode);
    lua_setfield(lua, -2, "MergeMode");
    lua_pushinteger(lua,in.FontBuilderFlags);
    lua_setfield(lua, -2, "FontBuilderFlags");
    lua_pushnumber(lua,in.RasterizerMultiply);
    lua_setfield(lua, -2, "RasterizerMultiply");
    lua_pushinteger(lua,in.EllipsisChar);
    lua_setfield(lua, -2, "EllipsisChar");
    lua_pushlstring(lua,in.Name ,sizeof(in.Name)-1);
    lua_setfield(lua, -2, "Name");
    //lua_push_ImFont_pointer(lua, in.DstFont);
    //lua_setfield(lua, -2, "DstFont");

}

void lua_push_ImFontAtlas(lua_State *lua, const ImFontAtlas & in)
{
    lua_newtable(lua);
    lua_pushboolean(lua,in.Locked);
    lua_setfield(lua, -2, "Locked");
    lua_pushinteger(lua,in.Flags);
    lua_setfield(lua, -2, "Flags");
    //if(in.TexID)
    //{
    //    ;
    //    lua_setfield(lua, -2, "TexID");
    //}
    lua_pushinteger(lua,in.TexDesiredWidth);
    lua_setfield(lua, -2, "TexDesiredWidth");
    lua_pushinteger(lua,in.TexGlyphPadding);
    lua_setfield(lua, -2, "TexGlyphPadding");
    //lua_pushstring(lua,in.TexPixelsAlpha8);
    //lua_setfield(lua, -2, "TexPixelsAlpha8");
    const int TexPixelsRGBA32  = in.TexPixelsRGBA32 != nullptr ? (*in.TexPixelsRGBA32) : 0;//TODO: 4 check here, apparently, "ImFontAtlas.TexPixelsRGBA32" is a pointer
    lua_pushinteger(lua,TexPixelsRGBA32);
    lua_setfield(lua, -2, "TexPixelsRGBA32");
    lua_pushinteger(lua,in.TexWidth);
    lua_setfield(lua, -2, "TexWidth");
    lua_pushinteger(lua,in.TexHeight);
    lua_setfield(lua, -2, "TexHeight");
    lua_push_ImVec2(lua,in.TexUvScale);
    lua_setfield(lua, -2, "TexUvScale");
    lua_push_ImVec2(lua,in.TexUvWhitePixel);
    lua_setfield(lua, -2, "TexUvWhitePixel");
    //#error "3 - (make_push_methods) Not found ImFontAtlas, do not know what to do!"
    //#error "3 - (make_push_methods) Not found ImFontAtlas, do not know what to do!"
    //#error "3 - (make_push_methods) Not found ImFontAtlas, do not know what to do!"
    //push_int_arrayFromTable(lua,in.CustomRectIds ,sizeof(in.CustomRectIds) / sizeof(int));//TODO: 5 check if the type is right
    //lua_setfield(lua, -2, "CustomRectIds");
    //#error "3 - (make_push_methods) Not found ImFontAtlas, do not know what to do!"
    //#error "3 - (make_push_methods) Not found ImFontAtlas, do not know what to do!"

}


void lua_push_ImFontGlyph(lua_State *lua, const ImFontGlyph & in)
{
    lua_newtable(lua);
    lua_pushinteger(lua,in.Codepoint);
    lua_setfield(lua, -2, "Codepoint");
    lua_pushnumber(lua,in.AdvanceX);
    lua_setfield(lua, -2, "AdvanceX");
    lua_pushnumber(lua,in.X0);
    lua_setfield(lua, -2, "X0");
    lua_pushnumber(lua,in.Y0);
    lua_setfield(lua, -2, "Y0");
    lua_pushnumber(lua,in.X1);
    lua_setfield(lua, -2, "X1");
    lua_pushnumber(lua,in.Y1);
    lua_setfield(lua, -2, "Y1");
    lua_pushnumber(lua,in.U0);
    lua_setfield(lua, -2, "U0");
    lua_pushnumber(lua,in.V0);
    lua_setfield(lua, -2, "V0");
    lua_pushnumber(lua,in.U1);
    lua_setfield(lua, -2, "U1");
    lua_pushnumber(lua,in.V1);
    lua_setfield(lua, -2, "V1");

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


void lua_push_ImFont(lua_State *lua, const ImFont & in)
{
    lua_newtable(lua);
    printStack(lua,__FILE__,__LINE__);
    //#error "3 - (make_push_methods) Not found ImFont, do not know what to do!"
    lua_pushnumber(lua,in.FallbackAdvanceX);
    printStack(lua,__FILE__,__LINE__);
    lua_setfield(lua, -2, "FallbackAdvanceX");
    printStack(lua,__FILE__,__LINE__);
    lua_pushnumber(lua,in.FontSize);
    printStack(lua,__FILE__,__LINE__);
    lua_setfield(lua, -2, "FontSize");
    printStack(lua,__FILE__,__LINE__);
    //#error "3 - (make_push_methods) Not found ImFont, do not know what to do!"
    //#error "3 - (make_push_methods) Not found ImFont, do not know what to do!"
    lua_push_ImFontGlyph_pointer(lua, in.FallbackGlyph);//TODO: 6 check here, apparently, "ImFont.FallbackGlyph" is a pointer and might be nullptr 
    printStack(lua,__FILE__,__LINE__);
    lua_setfield(lua, -2, "FallbackGlyph");
    printStack(lua,__FILE__,__LINE__);
    lua_push_ImFontAtlas_pointer(lua, in.ContainerAtlas);//TODO: 6 check here, apparently, "ImFont.ContainerAtlas" is a pointer and might be nullptr 
    lua_setfield(lua, -2, "ContainerAtlas");
    lua_push_ImFontConfig_pointer(lua, in.ConfigData);//TODO: 6 check here, apparently, "ImFont.ConfigData" is a pointer and might be nullptr 
    lua_setfield(lua, -2, "ConfigData");
    lua_pushinteger(lua,in.ConfigDataCount);
    lua_setfield(lua, -2, "ConfigDataCount");
    lua_pushinteger(lua,in.FallbackChar);
    lua_setfield(lua, -2, "FallbackChar");
    lua_pushinteger(lua,in.EllipsisChar);
    lua_setfield(lua, -2, "EllipsisChar");
    lua_pushnumber(lua,in.Scale);
    lua_setfield(lua, -2, "Scale");
    lua_pushnumber(lua,in.Ascent);
    lua_setfield(lua, -2, "Ascent");
    lua_pushnumber(lua,in.Descent);
    lua_setfield(lua, -2, "Descent");
    lua_pushinteger(lua,in.MetricsTotalSurface);
    lua_setfield(lua, -2, "MetricsTotalSurface");
    lua_pushboolean(lua,in.DirtyLookupTables);
    lua_setfield(lua, -2, "DirtyLookupTables");
    printStack(lua,__FILE__,__LINE__);
}


void lua_push_ImGuiStyle(lua_State *lua, const ImGuiStyle & in)
{
    lua_newtable(lua);
    lua_pushnumber(lua,in.Alpha);
    lua_setfield(lua, -2, "Alpha");
    lua_push_ImVec2(lua,in.WindowPadding);
    lua_setfield(lua, -2, "WindowPadding");
    lua_pushnumber(lua,in.WindowRounding);
    lua_setfield(lua, -2, "WindowRounding");
    lua_pushnumber(lua,in.WindowBorderSize);
    lua_setfield(lua, -2, "WindowBorderSize");
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
    lua_pushnumber(lua,in.GrabMinSize);
    lua_setfield(lua, -2, "GrabMinSize");
    lua_pushnumber(lua,in.GrabRounding);
    lua_setfield(lua, -2, "GrabRounding");
    lua_pushnumber(lua,in.TabRounding);
    lua_setfield(lua, -2, "TabRounding");
    lua_pushnumber(lua,in.TabBorderSize);
    lua_setfield(lua, -2, "TabBorderSize");
    lua_pushinteger(lua,in.ColorButtonPosition);
    lua_setfield(lua, -2, "ColorButtonPosition");
    lua_push_ImVec2(lua,in.ButtonTextAlign);
    lua_setfield(lua, -2, "ButtonTextAlign");
    lua_push_ImVec2(lua,in.SelectableTextAlign);
    lua_setfield(lua, -2, "SelectableTextAlign");
    lua_push_ImVec2(lua,in.DisplayWindowPadding);
    lua_setfield(lua, -2, "DisplayWindowPadding");
    lua_push_ImVec2(lua,in.DisplaySafeAreaPadding);
    lua_setfield(lua, -2, "DisplaySafeAreaPadding");
    lua_pushnumber(lua,in.MouseCursorScale);
    lua_setfield(lua, -2, "MouseCursorScale");
    lua_pushboolean(lua,in.AntiAliasedLines);
    lua_setfield(lua, -2, "AntiAliasedLines");
    lua_pushboolean(lua,in.AntiAliasedFill);
    lua_setfield(lua, -2, "AntiAliasedFill");
    lua_pushnumber(lua,in.CurveTessellationTol);
    lua_setfield(lua, -2, "CurveTessellationTol");
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


ImFontConfig lua_pop_ImFontConfig(lua_State *lua,const int index)
{
    ImFontConfig ImFontConfig_out;
    lua_check_is_table(lua, index, "ImFontConfig");
    //lua_getfield(lua, index, "FontData");
    //ImFontConfig_out.FontData              = ;
    //lua_pop(lua, 1);
    lua_getfield(lua, index, "FontDataSize");
    ImFontConfig_out.FontDataSize          = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "FontDataOwnedByAtlas");
    ImFontConfig_out.FontDataOwnedByAtlas  = lua_toboolean(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "FontNo");
    ImFontConfig_out.FontNo                = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "SizePixels");
    ImFontConfig_out.SizePixels            = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "OversampleH");
    ImFontConfig_out.OversampleH           = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "OversampleV");
    ImFontConfig_out.OversampleV           = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "PixelSnapH");
    ImFontConfig_out.PixelSnapH            = lua_toboolean(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "GlyphExtraSpacing");
    ImFontConfig_out.GlyphExtraSpacing     = lua_pop_ImVec2(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "GlyphOffset");
    ImFontConfig_out.GlyphOffset           = lua_pop_ImVec2(lua,-1);
    lua_pop(lua, 1);
    //lua_getfield(lua, index, "GlyphRanges");
    //ImFontConfig_out.GlyphRanges           = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "GlyphMinAdvanceX");
    ImFontConfig_out.GlyphMinAdvanceX      = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "GlyphMaxAdvanceX");
    ImFontConfig_out.GlyphMaxAdvanceX      = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "MergeMode");
    ImFontConfig_out.MergeMode             = lua_toboolean(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "FontBuilderFlags");
    ImFontConfig_out.FontBuilderFlags       = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "RasterizerMultiply");
    ImFontConfig_out.RasterizerMultiply    = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "EllipsisChar");
    ImFontConfig_out.EllipsisChar          = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "Name");
    const char* name = luaL_checkstring(lua,-1);
    snprintf(ImFontConfig_out.Name,sizeof(ImFontConfig_out.Name),"%s",name);
    //#warning "1 - (make_pop_methods) do not know what to do! it was NOT supposed be here: charName"
    //lua_getfield(lua, index, "DstFont");
    //ImFontConfig_out.DstFont               = lua_pop_ImFont(lua,-1);
    lua_pop(lua, 1);

    return ImFontConfig_out;
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

ImFont lua_pop_ImFont(lua_State *lua,const int index)
{
    ImFont ImFont_out;
    lua_check_is_table(lua, index, "ImFont");
    lua_getfield(lua, index, "FallbackAdvanceX");
    ImFont_out.FallbackAdvanceX     = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "FontSize");
    ImFont_out.FontSize             = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "ConfigDataCount");
    ImFont_out.ConfigDataCount      = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "FallbackChar");
    ImFont_out.FallbackChar         = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "EllipsisChar");
    ImFont_out.EllipsisChar         = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "Scale");
    ImFont_out.Scale                = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "Ascent");
    ImFont_out.Ascent               = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "Descent");
    ImFont_out.Descent              = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "MetricsTotalSurface");
    ImFont_out.MetricsTotalSurface  = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "DirtyLookupTables");
    ImFont_out.DirtyLookupTables    = lua_toboolean(lua,-1);
    lua_pop(lua, 1);

    return ImFont_out;
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


ImFontAtlas lua_pop_ImFontAtlas(lua_State *lua,const int index)
{
    ImFontAtlas ImFontAtlas_out;
    lua_check_is_table(lua, index, "ImFontAtlas");
    lua_getfield(lua, index, "Locked");
    ImFontAtlas_out.Locked           = lua_toboolean(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "Flags");
    ImFontAtlas_out.Flags            = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    //static void var_ImTextureID_136;//TODO: 3 check here, apparently, "ImFontAtlas.TexID" is a pointer
    //lua_getfield(lua, index, "TexID");
    //ImFontAtlas_out.TexID            = ;
    //lua_pop(lua, 1);
    lua_getfield(lua, index, "TexDesiredWidth");
    ImFontAtlas_out.TexDesiredWidth  = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "TexGlyphPadding");
    ImFontAtlas_out.TexGlyphPadding  = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    //lua_getfield(lua, index, "TexPixelsAlpha8");
    //ImFontAtlas_out.TexPixelsAlpha8  = luaL_checkinteger(lua,-1);
    //lua_pop(lua, 1);
    //lua_getfield(lua, index, "TexPixelsRGBA32");
    //ImFontAtlas_out.TexPixelsRGBA32  = luaL_checkinteger(lua,-1);
    //lua_pop(lua, 1);
    lua_getfield(lua, index, "TexWidth");
    ImFontAtlas_out.TexWidth         = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "TexHeight");
    ImFontAtlas_out.TexHeight        = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "TexUvScale");
    ImFontAtlas_out.TexUvScale       = lua_pop_ImVec2(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "TexUvWhitePixel");
    ImFontAtlas_out.TexUvWhitePixel  = lua_pop_ImVec2(lua,-1);
    lua_pop(lua, 1);
    //#error "1 - (make_pop_methods) Not found ImVector<ImFont>, do not know what to do for variables: Fonts, "
    //#error "1 - (make_pop_methods) Not found ImVector<ImFontAtlasCustomRect>, do not know what to do for variables: CustomRects, "
    //#error "1 - (make_pop_methods) Not found ImVector<ImFontConfig>, do not know what to do for variables: ConfigData, "
    //get_int_arrayFromTable(lua,index,ImFontAtlas_out->CustomRectIds ,sizeof(ImFontAtlas_out->CustomRectIds) / sizeof(int),"CustomRectIds");//TODO: 1 check if the type is right
    //#warning "1 - (make_pop_methods) do not know what to do! it was NOT supposed be here: intCustomRectIds"
    //#error "1 - (make_pop_methods) Not found typedef, do not know what to do for variables: ImFontAtlasCustomRect    CustomRect, "
    //#error "1 - (make_pop_methods) Not found typedef, do not know what to do for variables: ImFontGlyphRangesBuilder GlyphRangesBuilder, "

    return ImFontAtlas_out;
}

ImFontGlyph lua_pop_ImFontGlyph(lua_State *lua,const int index)
{
    ImFontGlyph ImFontGlyph_out;
    lua_check_is_table(lua, index, "ImFontGlyph");
    lua_getfield(lua, index, "Codepoint");
    ImFontGlyph_out.Codepoint  = luaL_checkinteger(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "AdvanceX");
    ImFontGlyph_out.AdvanceX   = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "X0");
    ImFontGlyph_out.X0         = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "Y0");
    ImFontGlyph_out.Y0         = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "X1");
    ImFontGlyph_out.X1         = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "Y1");
    ImFontGlyph_out.Y1         = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "U0");
    ImFontGlyph_out.U0         = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "V0");
    ImFontGlyph_out.V0         = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "U1");
    ImFontGlyph_out.U1         = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);
    lua_getfield(lua, index, "V1");
    ImFontGlyph_out.V1         = luaL_checknumber(lua,-1);
    lua_pop(lua, 1);

    return ImFontGlyph_out;
}

ImFontConfig * lua_pop_ImFontConfig_pointer(lua_State *lua, const int index, ImFontConfig * in_out_ImFontConfig)
{
    if (in_out_ImFontConfig == nullptr)
    {
        lua_log_error(lua,"ImFontConfig can not be null");
        return nullptr;
    }
    lua_check_is_table(lua, index, "ImFontConfig");
    //static void var_void_137                   = 0;//TODO: 9 check here, apparently, "ImFontConfig->FontData" is a pointer
    //in_out_ImFontConfig->FontData              = &var_void_137;
    //var_void_137                               = static_cast<void>(get_number_from_field(lua,index,static_cast<lua_Number>(var_void_137),"FontData"));
    in_out_ImFontConfig->FontDataSize          = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->FontDataSize),"FontDataSize"));
    in_out_ImFontConfig->FontDataOwnedByAtlas  = static_cast<bool>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->FontDataOwnedByAtlas),"FontDataOwnedByAtlas"));
    in_out_ImFontConfig->FontNo                = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->FontNo),"FontNo"));
    in_out_ImFontConfig->SizePixels            = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->SizePixels),"SizePixels"));
    in_out_ImFontConfig->OversampleH           = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->OversampleH),"OversampleH"));
    in_out_ImFontConfig->OversampleV           = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->OversampleV),"OversampleV"));
    in_out_ImFontConfig->PixelSnapH            = static_cast<bool>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->PixelSnapH),"PixelSnapH"));
    lua_getfield(lua, index, "GlyphExtraSpacing");
    in_out_ImFontConfig->GlyphExtraSpacing     = lua_pop_ImVec2(lua,index);
    lua_getfield(lua, index, "GlyphOffset");
    in_out_ImFontConfig->GlyphOffset           = lua_pop_ImVec2(lua,index);
    static ImWchar var_ImWchar_138;//TODO: 11 check here, apparently, "ImFontConfig->GlyphRanges" is a pointer
    in_out_ImFontConfig->GlyphRanges           = lua_pop_ImFontConfig_pointer(lua, index, &var_ImWchar_138);
    in_out_ImFontConfig->GlyphMinAdvanceX      = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->GlyphMinAdvanceX),"GlyphMinAdvanceX"));
    in_out_ImFontConfig->GlyphMaxAdvanceX      = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->GlyphMaxAdvanceX),"GlyphMaxAdvanceX"));
    in_out_ImFontConfig->MergeMode             = static_cast<bool>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->MergeMode),"MergeMode"));
    in_out_ImFontConfig->FontBuilderFlags      = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->FontBuilderFlags),"FontBuilderFlags"));
    in_out_ImFontConfig->RasterizerMultiply    = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->RasterizerMultiply),"RasterizerMultiply"));
    lua_getfield(lua, index, "EllipsisChar");
    in_out_ImFontConfig->EllipsisChar          = luaL_checkinteger(lua,index);
    lua_getfield(lua, index, "Name");
    const char* name = luaL_checkstring(lua,-1);
    snprintf(in_out_ImFontConfig->Name,sizeof(in_out_ImFontConfig->Name),"%s",name);
    //static ImFont var_ImFont_139;//TODO: 11 check here, apparently, "ImFontConfig->DstFont" is a pointer
    //in_out_ImFontConfig->DstFont               = lua_pop_ImFontConfig_pointer(lua, index, &var_ImFont_139);

    return in_out_ImFontConfig;
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

ImFont * lua_pop_ImFont_pointer(lua_State *lua, const int index, ImFont * in_out_ImFont)
{
    if (in_out_ImFont == nullptr)
    {
        lua_log_error(lua,"ImFont can not be null");
        return nullptr;
    }
    lua_check_is_table(lua, index, "ImFont");

     //#error "6 - Not found ImFont->ImVector<float>, do not know what to do for the variables: IndexAdvanceX, "
    in_out_ImFont->FallbackAdvanceX     = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->FallbackAdvanceX),"FallbackAdvanceX"));
    in_out_ImFont->FontSize             = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->FontSize),"FontSize"));

     //#error "6 - Not found ImFont->ImVector<ImWchar>, do not know what to do for the variables: IndexLookup, "

     //#error "6 - Not found ImFont->ImVector<ImFontGlyph>, do not know what to do for the variables: Glyphs, "
    static ImFontGlyph var_ImFontGlyph_140;//TODO: 11 check here, apparently, "ImFont->FallbackGlyph" is a pointer
    in_out_ImFont->FallbackGlyph        = lua_pop_ImFont_pointer(lua, index, &var_ImFontGlyph_140);
    static ImFontAtlas var_ImFontAtlas_141;//TODO: 11 check here, apparently, "ImFont->ContainerAtlas" is a pointer
    in_out_ImFont->ContainerAtlas       = lua_pop_ImFont_pointer(lua, index, &var_ImFontAtlas_141);
    static ImFontConfig var_ImFontConfig_142;//TODO: 11 check here, apparently, "ImFont->ConfigData" is a pointer
    in_out_ImFont->ConfigData           = lua_pop_ImFont_pointer(lua, index, &var_ImFontConfig_142);
    in_out_ImFont->ConfigDataCount      = static_cast<short>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->ConfigDataCount),"ConfigDataCount"));
    lua_getfield(lua, index, "FallbackChar");
    in_out_ImFont->FallbackChar         = luaL_checkinteger(lua,index);
    lua_getfield(lua, index, "EllipsisChar");
    in_out_ImFont->EllipsisChar         = luaL_checkinteger(lua,index);
    in_out_ImFont->Scale                = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->Scale),"Scale"));
    in_out_ImFont->Ascent               = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->Ascent),"Ascent"));
    in_out_ImFont->Descent              = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->Descent),"Descent"));
    in_out_ImFont->MetricsTotalSurface  = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->MetricsTotalSurface),"MetricsTotalSurface"));
    in_out_ImFont->DirtyLookupTables    = static_cast<bool>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->DirtyLookupTables),"DirtyLookupTables"));

    return in_out_ImFont;
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


ImFontAtlas * lua_pop_ImFontAtlas_pointer(lua_State *lua, const int index, ImFontAtlas * in_out_ImFontAtlas)
{
    if (in_out_ImFontAtlas == nullptr)
    {
        lua_log_error(lua,"ImFontAtlas can not be null");
        return nullptr;
    }
    lua_check_is_table(lua, index, "ImFontAtlas");
    in_out_ImFontAtlas->Locked           = static_cast<bool>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontAtlas->Locked),"Locked"));
    lua_getfield(lua, index, "Flags");
    in_out_ImFontAtlas->Flags            = luaL_checkinteger(lua,index);
    lua_pop(lua, 1);
    in_out_ImFontAtlas->TexDesiredWidth  = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontAtlas->TexDesiredWidth),"TexDesiredWidth"));
    in_out_ImFontAtlas->TexGlyphPadding  = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontAtlas->TexGlyphPadding),"TexGlyphPadding"));
    //in_out_ImFontAtlas->TexPixelsAlpha8  = get_string_from_field(lua, index, "TexPixelsAlpha8");
    //static int var_int_145               = 0;//TODO: 9 check here, apparently, "ImFontAtlas->TexPixelsRGBA32" is a pointer
    //in_out_ImFontAtlas->TexPixelsRGBA32  = &var_int_145;
    //var_int_145                          = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(var_int_145),"TexPixelsRGBA32"));
    in_out_ImFontAtlas->TexWidth         = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontAtlas->TexWidth),"TexWidth"));
    in_out_ImFontAtlas->TexHeight        = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontAtlas->TexHeight),"TexHeight"));
    lua_getfield(lua, index, "TexUvScale");
    in_out_ImFontAtlas->TexUvScale       = lua_pop_ImVec2(lua,index);
    lua_getfield(lua, index, "TexUvWhitePixel");
    in_out_ImFontAtlas->TexUvWhitePixel  = lua_pop_ImVec2(lua,index);

     //#error "6 - Not found ImFontAtlas->ImVector<ImFont>, do not know what to do for the variables: Fonts, "

     //#error "6 - Not found ImFontAtlas->ImVector<ImFontAtlasCustomRect>, do not know what to do for the variables: CustomRects, "

     //#error "6 - Not found ImFontAtlas->ImVector<ImFontConfig>, do not know what to do for the variables: ConfigData, "
    //get_int_arrayFromTable(lua,index,in_out_ImFontAtlas->CustomRectIds ,sizeof(in_out_ImFontAtlas->CustomRectIds) / sizeof(int),"CustomRectIds");//TODO: 10 check if the type is right

     //#error "6 - Not found ImFontAtlas->typedef, do not know what to do for the variables: ImFontAtlasCustomRect    CustomRect, "

     //#error "6 - Not found ImFontAtlas->typedef, do not know what to do for the variables: ImFontGlyphRangesBuilder GlyphRangesBuilder, "

    return in_out_ImFontAtlas;
}


ImFontGlyph * lua_pop_ImFontGlyph_pointer(lua_State *lua, const int index, ImFontGlyph * in_out_ImFontGlyph)
{
    if (in_out_ImFontGlyph == nullptr)
    {
        lua_log_error(lua,"ImFontGlyph can not be null");
        return nullptr;
    }
    lua_check_is_table(lua, index, "ImFontGlyph");
    lua_getfield(lua, index, "Codepoint");
    in_out_ImFontGlyph->Codepoint  = luaL_checkinteger(lua,index);
    in_out_ImFontGlyph->AdvanceX   = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->AdvanceX),"AdvanceX"));
    in_out_ImFontGlyph->X0         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->X0),"X0"));
    in_out_ImFontGlyph->Y0         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->Y0),"Y0"));
    in_out_ImFontGlyph->X1         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->X1),"X1"));
    in_out_ImFontGlyph->Y1         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->Y1),"Y1"));
    in_out_ImFontGlyph->U0         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->U0),"U0"));
    in_out_ImFontGlyph->V0         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->V0),"V0"));
    in_out_ImFontGlyph->U1         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->U1),"U1"));
    in_out_ImFontGlyph->V1         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->V1),"V1"));

    return in_out_ImFontGlyph;
}

ImGuiStyle * lua_pop_ImGuiStyle_pointer(lua_State *lua, const int index, ImGuiStyle * in_out_ImGuiStyle)
{
    if (in_out_ImGuiStyle == nullptr)
    {
        lua_log_error(lua,"ImGuiStyle can not be null");
        return nullptr;
    }
    lua_check_is_table(lua, index, "ImGuiStyle");
    in_out_ImGuiStyle->Alpha                     = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->Alpha),"Alpha"));
    lua_getfield(lua, index, "WindowPadding");
    in_out_ImGuiStyle->WindowPadding             = lua_pop_ImVec2(lua,index);
    in_out_ImGuiStyle->WindowRounding            = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->WindowRounding),"WindowRounding"));
    in_out_ImGuiStyle->WindowBorderSize          = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->WindowBorderSize),"WindowBorderSize"));
    lua_getfield(lua, index, "WindowMinSize");
    in_out_ImGuiStyle->WindowMinSize             = lua_pop_ImVec2(lua,index);
    lua_getfield(lua, index, "WindowTitleAlign");
    in_out_ImGuiStyle->WindowTitleAlign          = lua_pop_ImVec2(lua,index);
    lua_getfield(lua, index, "WindowMenuButtonPosition");
    in_out_ImGuiStyle->WindowMenuButtonPosition  = luaL_checkinteger(lua,index);
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
    lua_getfield(lua, index, "TouchExtraPadding");
    in_out_ImGuiStyle->TouchExtraPadding         = lua_pop_ImVec2(lua,index);
    in_out_ImGuiStyle->IndentSpacing             = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->IndentSpacing),"IndentSpacing"));
    in_out_ImGuiStyle->ColumnsMinSpacing         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->ColumnsMinSpacing),"ColumnsMinSpacing"));
    in_out_ImGuiStyle->ScrollbarSize             = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->ScrollbarSize),"ScrollbarSize"));
    in_out_ImGuiStyle->ScrollbarRounding         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->ScrollbarRounding),"ScrollbarRounding"));
    in_out_ImGuiStyle->GrabMinSize               = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->GrabMinSize),"GrabMinSize"));
    in_out_ImGuiStyle->GrabRounding              = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->GrabRounding),"GrabRounding"));
    in_out_ImGuiStyle->TabRounding               = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->TabRounding),"TabRounding"));
    in_out_ImGuiStyle->TabBorderSize             = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->TabBorderSize),"TabBorderSize"));
    lua_getfield(lua, index, "ColorButtonPosition");
    in_out_ImGuiStyle->ColorButtonPosition       = luaL_checkinteger(lua,index);
    lua_getfield(lua, index, "ButtonTextAlign");
    in_out_ImGuiStyle->ButtonTextAlign           = lua_pop_ImVec2(lua,index);
    lua_getfield(lua, index, "SelectableTextAlign");
    in_out_ImGuiStyle->SelectableTextAlign       = lua_pop_ImVec2(lua,index);
    lua_getfield(lua, index, "DisplayWindowPadding");
    in_out_ImGuiStyle->DisplayWindowPadding      = lua_pop_ImVec2(lua,index);
    lua_getfield(lua, index, "DisplaySafeAreaPadding");
    in_out_ImGuiStyle->DisplaySafeAreaPadding    = lua_pop_ImVec2(lua,index);
    in_out_ImGuiStyle->MouseCursorScale          = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->MouseCursorScale),"MouseCursorScale"));
    in_out_ImGuiStyle->AntiAliasedLines          = static_cast<bool>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->AntiAliasedLines),"AntiAliasedLines"));
    in_out_ImGuiStyle->AntiAliasedFill           = static_cast<bool>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->AntiAliasedFill),"AntiAliasedFill"));
    in_out_ImGuiStyle->CurveTessellationTol      = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImGuiStyle->CurveTessellationTol),"CurveTessellationTol"));
    lua_getfield(lua, index, "Colors");
    for(int i=0; i < ImGuiCol_COUNT; i++)
    {
        lua_rawgeti(lua,index+1,i+1);
        in_out_ImGuiStyle->Colors[i]    = lua_pop_ImVec4(lua,index+2);
    }
    return in_out_ImGuiStyle;
}


void lua_push_ImFontConfig_pointer(lua_State *lua, const ImFontConfig * p_in_ImFontConfig)
{
    if (p_in_ImFontConfig == nullptr)
    {
        lua_log_error(lua,"ImFontConfig can not be null");
    }
    else
    {
        lua_newtable(lua);
        //if(p_in_ImFontConfig->FontData)
        //{
        //    ;
        //    lua_setfield(lua, -2, "FontData");
        //}
        lua_pushinteger(lua,p_in_ImFontConfig->FontDataSize);
        lua_setfield(lua, -2, "FontDataSize");
        lua_pushboolean(lua,p_in_ImFontConfig->FontDataOwnedByAtlas);
        lua_setfield(lua, -2, "FontDataOwnedByAtlas");
        lua_pushinteger(lua,p_in_ImFontConfig->FontNo);
        lua_setfield(lua, -2, "FontNo");
        lua_pushnumber(lua,p_in_ImFontConfig->SizePixels);
        lua_setfield(lua, -2, "SizePixels");
        lua_pushinteger(lua,p_in_ImFontConfig->OversampleH);
        lua_setfield(lua, -2, "OversampleH");
        lua_pushinteger(lua,p_in_ImFontConfig->OversampleV);
        lua_setfield(lua, -2, "OversampleV");
        lua_pushboolean(lua,p_in_ImFontConfig->PixelSnapH);
        lua_setfield(lua, -2, "PixelSnapH");
        lua_push_ImVec2(lua,p_in_ImFontConfig->GlyphExtraSpacing);
        lua_setfield(lua, -2, "GlyphExtraSpacing");
        lua_push_ImVec2(lua,p_in_ImFontConfig->GlyphOffset);
        lua_setfield(lua, -2, "GlyphOffset");
        if(p_in_ImFontConfig->GlyphRanges)
        {
            lua_pushinteger(lua,*p_in_ImFontConfig->GlyphRanges);
            lua_setfield(lua, -2, "GlyphRanges");
        }
        lua_pushnumber(lua,p_in_ImFontConfig->GlyphMinAdvanceX);
        lua_setfield(lua, -2, "GlyphMinAdvanceX");
        lua_pushnumber(lua,p_in_ImFontConfig->GlyphMaxAdvanceX);
        lua_setfield(lua, -2, "GlyphMaxAdvanceX");
        lua_pushboolean(lua,p_in_ImFontConfig->MergeMode);
        lua_setfield(lua, -2, "MergeMode");
        lua_pushinteger(lua,p_in_ImFontConfig->FontBuilderFlags);
        lua_setfield(lua, -2, "FontBuilderFlags");
        lua_pushnumber(lua,p_in_ImFontConfig->RasterizerMultiply);
        lua_setfield(lua, -2, "RasterizerMultiply");
        lua_pushinteger(lua,p_in_ImFontConfig->EllipsisChar);
        lua_setfield(lua, -2, "EllipsisChar");
        lua_pushlstring(lua,p_in_ImFontConfig->Name ,sizeof(p_in_ImFontConfig->Name) / sizeof(char));
        lua_setfield(lua, -2, "Name");
        /*if(p_in_ImFontConfig->DstFont) //recursive??
        {
            lua_push_ImFont(lua,*p_in_ImFontConfig->DstFont);
            lua_setfield(lua, -2, "DstFont");
        }*/

    }
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


void lua_push_ImFont_pointer(lua_State *lua, const ImFont * p_in_ImFont)
{
    if (p_in_ImFont == nullptr)
    {
        lua_log_error(lua,"ImFont can not be null");
    }
    else
    {
        lua_newtable(lua);

         //#error "5 - Not found ImFont->ImVector<float>, do not know what to do for the variables: IndexAdvanceX, "
        lua_pushnumber(lua,p_in_ImFont->FallbackAdvanceX);
        lua_setfield(lua, -2, "FallbackAdvanceX");
        lua_pushnumber(lua,p_in_ImFont->FontSize);
        lua_setfield(lua, -2, "FontSize");

         //#error "5 - Not found ImFont->ImVector<ImWchar>, do not know what to do for the variables: IndexLookup, "

         //#error "5 - Not found ImFont->ImVector<ImFontGlyph>, do not know what to do for the variables: Glyphs, "
        if(p_in_ImFont->FallbackGlyph)
        {
            lua_push_ImFontGlyph(lua,*p_in_ImFont->FallbackGlyph);
            lua_setfield(lua, -2, "FallbackGlyph");
        }
        if(p_in_ImFont->ContainerAtlas)
        {
            lua_push_ImFontAtlas(lua,*p_in_ImFont->ContainerAtlas);
            lua_setfield(lua, -2, "ContainerAtlas");
        }
        if(p_in_ImFont->ConfigData)
        {
            lua_push_ImFontConfig(lua,*p_in_ImFont->ConfigData);
            lua_setfield(lua, -2, "ConfigData");
        }
        lua_pushinteger(lua,p_in_ImFont->ConfigDataCount);
        lua_setfield(lua, -2, "ConfigDataCount");
        lua_pushinteger(lua,p_in_ImFont->FallbackChar);
        lua_setfield(lua, -2, "FallbackChar");
        lua_pushinteger(lua,p_in_ImFont->EllipsisChar);
        lua_setfield(lua, -2, "EllipsisChar");
        lua_pushnumber(lua,p_in_ImFont->Scale);
        lua_setfield(lua, -2, "Scale");
        lua_pushnumber(lua,p_in_ImFont->Ascent);
        lua_setfield(lua, -2, "Ascent");
        lua_pushnumber(lua,p_in_ImFont->Descent);
        lua_setfield(lua, -2, "Descent");
        lua_pushinteger(lua,p_in_ImFont->MetricsTotalSurface);
        lua_setfield(lua, -2, "MetricsTotalSurface");
        lua_pushboolean(lua,p_in_ImFont->DirtyLookupTables);
        lua_setfield(lua, -2, "DirtyLookupTables");

    }
}


void lua_push_ImFontGlyph_pointer(lua_State *lua, const ImFontGlyph * p_in_ImFontGlyph)
{
    if (p_in_ImFontGlyph == nullptr)
    {
        lua_log_error(lua,"ImFontGlyph can not be null");
    }
    else
    {
        lua_newtable(lua);
        lua_pushinteger(lua,p_in_ImFontGlyph->Codepoint);
        lua_setfield(lua, -2, "Codepoint");
        lua_pushnumber(lua,p_in_ImFontGlyph->AdvanceX);
        lua_setfield(lua, -2, "AdvanceX");
        lua_pushnumber(lua,p_in_ImFontGlyph->X0);
        lua_setfield(lua, -2, "X0");
        lua_pushnumber(lua,p_in_ImFontGlyph->Y0);
        lua_setfield(lua, -2, "Y0");
        lua_pushnumber(lua,p_in_ImFontGlyph->X1);
        lua_setfield(lua, -2, "X1");
        lua_pushnumber(lua,p_in_ImFontGlyph->Y1);
        lua_setfield(lua, -2, "Y1");
        lua_pushnumber(lua,p_in_ImFontGlyph->U0);
        lua_setfield(lua, -2, "U0");
        lua_pushnumber(lua,p_in_ImFontGlyph->V0);
        lua_setfield(lua, -2, "V0");
        lua_pushnumber(lua,p_in_ImFontGlyph->U1);
        lua_setfield(lua, -2, "U1");
        lua_pushnumber(lua,p_in_ImFontGlyph->V1);
        lua_setfield(lua, -2, "V1");

    }
}

void lua_push_ImFontAtlas_pointer(lua_State *lua, const ImFontAtlas * p_in_ImFontAtlas)
{
    if (p_in_ImFontAtlas == nullptr)
    {
        lua_log_error(lua,"ImFontAtlas can not be null");
    }
    else
    {
        lua_newtable(lua);
        lua_pushboolean(lua,p_in_ImFontAtlas->Locked);
        lua_setfield(lua, -2, "Locked");
        lua_pushinteger(lua,p_in_ImFontAtlas->Flags);
        lua_setfield(lua, -2, "Flags");
        //if(p_in_ImFontAtlas->TexID)
        //{
        //    ;
        //    lua_setfield(lua, -2, "TexID");
        //}
        lua_pushinteger(lua,p_in_ImFontAtlas->TexDesiredWidth);
        lua_setfield(lua, -2, "TexDesiredWidth");
        lua_pushinteger(lua,p_in_ImFontAtlas->TexGlyphPadding);
        lua_setfield(lua, -2, "TexGlyphPadding");
        //if(p_in_ImFontAtlas->TexPixelsAlpha8)
        //{
        //    lua_pushstring(lua, p_in_ImFontAtlas->TexPixelsAlpha8);
        //    lua_setfield(lua, -2, "TexPixelsAlpha8");
        //}
        if(p_in_ImFontAtlas->TexPixelsRGBA32)
        {
            lua_pushinteger(lua,*p_in_ImFontAtlas->TexPixelsRGBA32);
            lua_setfield(lua, -2, "TexPixelsRGBA32");
        }
        lua_pushinteger(lua,p_in_ImFontAtlas->TexWidth);
        lua_setfield(lua, -2, "TexWidth");
        lua_pushinteger(lua,p_in_ImFontAtlas->TexHeight);
        lua_setfield(lua, -2, "TexHeight");
        lua_push_ImVec2(lua,p_in_ImFontAtlas->TexUvScale);
        lua_setfield(lua, -2, "TexUvScale");
        lua_push_ImVec2(lua,p_in_ImFontAtlas->TexUvWhitePixel);
        lua_setfield(lua, -2, "TexUvWhitePixel");

         //#error "5 - Not found ImFontAtlas->ImVector<ImFont>, do not know what to do for the variables: Fonts, "

         //#error "5 - Not found ImFontAtlas->ImVector<ImFontAtlasCustomRect>, do not know what to do for the variables: CustomRects, "

         //#error "5 - Not found ImFontAtlas->ImVector<ImFontConfig>, do not know what to do for the variables: ConfigData, "
        //push_int_arrayFromTable(lua,p_in_ImFontAtlas->CustomRectIds ,sizeof(p_in_ImFontAtlas->CustomRectIds) / sizeof(int));//TODO: 8 check if the type is right
        //lua_setfield(lua, -2, "CustomRectIds");

         //#error "5 - Not found ImFontAtlas->typedef, do not know what to do for the variables: ImFontAtlasCustomRect    CustomRect, "

         //#error "5 - Not found ImFontAtlas->typedef, do not know what to do for the variables: ImFontGlyphRangesBuilder GlyphRangesBuilder, "

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


ImFontConfig * lua_pop_ImFont_pointer(lua_State *lua, const int index, ImFontConfig * in_out_ImFontConfig)
{
    if (in_out_ImFontConfig == nullptr)
    {
        lua_log_error(lua,"ImFontConfig can not be null");
        return nullptr;
    }
    lua_check_is_table(lua, index, "ImFontConfig");
    //static void var_void_146                   = 0;//TODO: 9 check here, apparently, "ImFontConfig->FontData" is a pointer
    //in_out_ImFontConfig->FontData              = &var_void_146;
    //var_void_146                               = static_cast<void>(get_number_from_field(lua,index,static_cast<lua_Number>(var_void_146),"FontData"));
    in_out_ImFontConfig->FontDataSize          = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->FontDataSize),"FontDataSize"));
    in_out_ImFontConfig->FontDataOwnedByAtlas  = static_cast<bool>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->FontDataOwnedByAtlas),"FontDataOwnedByAtlas"));
    in_out_ImFontConfig->FontNo                = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->FontNo),"FontNo"));
    in_out_ImFontConfig->SizePixels            = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->SizePixels),"SizePixels"));
    in_out_ImFontConfig->OversampleH           = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->OversampleH),"OversampleH"));
    in_out_ImFontConfig->OversampleV           = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->OversampleV),"OversampleV"));
    in_out_ImFontConfig->PixelSnapH            = static_cast<bool>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->PixelSnapH),"PixelSnapH"));
    lua_getfield(lua, index, "GlyphExtraSpacing");
    in_out_ImFontConfig->GlyphExtraSpacing     = lua_pop_ImVec2(lua,index);
    lua_getfield(lua, index, "GlyphOffset");
    in_out_ImFontConfig->GlyphOffset           = lua_pop_ImVec2(lua,index);
    static ImWchar var_ImWchar_147;//TODO: 11 check here, apparently, "ImFontConfig->GlyphRanges" is a pointer
    in_out_ImFontConfig->GlyphRanges           = lua_pop_ImFontConfig_pointer(lua, index, &var_ImWchar_147);
    in_out_ImFontConfig->GlyphMinAdvanceX      = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->GlyphMinAdvanceX),"GlyphMinAdvanceX"));
    in_out_ImFontConfig->GlyphMaxAdvanceX      = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->GlyphMaxAdvanceX),"GlyphMaxAdvanceX"));
    in_out_ImFontConfig->MergeMode             = static_cast<bool>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->MergeMode),"MergeMode"));
    in_out_ImFontConfig->FontBuilderFlags      = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->FontBuilderFlags),"FontBuilderFlags"));
    in_out_ImFontConfig->RasterizerMultiply    = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontConfig->RasterizerMultiply),"RasterizerMultiply"));
    lua_getfield(lua, index, "EllipsisChar");
    in_out_ImFontConfig->EllipsisChar          = luaL_checkinteger(lua,index);
    lua_getfield(lua, index, "Name");
    const char* name = luaL_checkstring(lua,-1);
    snprintf(in_out_ImFontConfig->Name,sizeof(in_out_ImFontConfig->Name),"%s",name);
    //static ImFont var_ImFont_148;//TODO: 11 check here, apparently, "ImFontConfig->DstFont" is a pointer
    //in_out_ImFontConfig->DstFont               = lua_pop_ImFontConfig_pointer(lua, index, &var_ImFont_148);

    return in_out_ImFontConfig;
}

ImFontAtlas * lua_pop_ImFont_pointer(lua_State *lua, const int index, ImFontAtlas * in_out_ImFontAtlas)
{
    if (in_out_ImFontAtlas == nullptr)
    {
        lua_log_error(lua,"ImFontAtlas can not be null");
        return nullptr;
    }
    lua_check_is_table(lua, index, "ImFontAtlas");
    in_out_ImFontAtlas->Locked           = static_cast<bool>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontAtlas->Locked),"Locked"));
    lua_getfield(lua, index, "Flags");
    in_out_ImFontAtlas->Flags            = luaL_checkinteger(lua,index);
    lua_pop(lua, 1);
    in_out_ImFontAtlas->TexDesiredWidth  = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontAtlas->TexDesiredWidth),"TexDesiredWidth"));
    in_out_ImFontAtlas->TexGlyphPadding  = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontAtlas->TexGlyphPadding),"TexGlyphPadding"));
    //in_out_ImFontAtlas->TexPixelsAlpha8  = get_string_from_field(lua, index, "TexPixelsAlpha8");
    //static int var_int_151               = 0;//TODO: 9 check here, apparently, "ImFontAtlas->TexPixelsRGBA32" is a pointer
    //in_out_ImFontAtlas->TexPixelsRGBA32  = &var_int_151;
    //var_int_151                          = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(var_int_151),"TexPixelsRGBA32"));
    in_out_ImFontAtlas->TexWidth         = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontAtlas->TexWidth),"TexWidth"));
    in_out_ImFontAtlas->TexHeight        = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontAtlas->TexHeight),"TexHeight"));
    lua_getfield(lua, index, "TexUvScale");
    in_out_ImFontAtlas->TexUvScale       = lua_pop_ImVec2(lua,index);
    lua_getfield(lua, index, "TexUvWhitePixel");
    in_out_ImFontAtlas->TexUvWhitePixel  = lua_pop_ImVec2(lua,index);

     //#error "6 - Not found ImFontAtlas->ImVector<ImFont>, do not know what to do for the variables: Fonts, "

     //#error "6 - Not found ImFontAtlas->ImVector<ImFontAtlasCustomRect>, do not know what to do for the variables: CustomRects, "

     //#error "6 - Not found ImFontAtlas->ImVector<ImFontConfig>, do not know what to do for the variables: ConfigData, "
    //get_int_arrayFromTable(lua,index,in_out_ImFontAtlas->CustomRectIds ,sizeof(in_out_ImFontAtlas->CustomRectIds) / sizeof(int),"CustomRectIds");//TODO: 10 check if the type is right

     //#error "6 - Not found ImFontAtlas->typedef, do not know what to do for the variables: ImFontAtlasCustomRect    CustomRect, "

     //#error "6 - Not found ImFontAtlas->typedef, do not know what to do for the variables: ImFontGlyphRangesBuilder GlyphRangesBuilder, "

    return in_out_ImFontAtlas;
}


ImFontGlyph * lua_pop_ImFont_pointer(lua_State *lua, const int index, ImFontGlyph * in_out_ImFontGlyph)
{
    if (in_out_ImFontGlyph == nullptr)
    {
        lua_log_error(lua,"ImFontGlyph can not be null");
        return nullptr;
    }
    lua_check_is_table(lua, index, "ImFontGlyph");
    lua_getfield(lua, index, "Codepoint");
    in_out_ImFontGlyph->Codepoint  = luaL_checkinteger(lua,index);
    in_out_ImFontGlyph->AdvanceX   = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->AdvanceX),"AdvanceX"));
    in_out_ImFontGlyph->X0         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->X0),"X0"));
    in_out_ImFontGlyph->Y0         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->Y0),"Y0"));
    in_out_ImFontGlyph->X1         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->X1),"X1"));
    in_out_ImFontGlyph->Y1         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->Y1),"Y1"));
    in_out_ImFontGlyph->U0         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->U0),"U0"));
    in_out_ImFontGlyph->V0         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->V0),"V0"));
    in_out_ImFontGlyph->U1         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->U1),"U1"));
    in_out_ImFontGlyph->V1         = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFontGlyph->V1),"V1"));

    return in_out_ImFontGlyph;
}


ImWchar * lua_pop_ImFontConfig_pointer(lua_State *lua, const int index, ImWchar * in_out_ImWchar)
{
    if (in_out_ImWchar == nullptr)
    {
        lua_log_error(lua,"ImWchar can not be null");
        return nullptr;
    }
    lua_check_is_table(lua, index, "ImWchar");

    return in_out_ImWchar;
}

ImFont * lua_pop_ImFontConfig_pointer(lua_State *lua, const int index, ImFont * in_out_ImFont)
{
    if (in_out_ImFont == nullptr)
    {
        lua_log_error(lua,"ImFont can not be null");
        return nullptr;
    }
    lua_check_is_table(lua, index, "ImFont");

     //#error "6 - Not found ImFont->ImVector<float>, do not know what to do for the variables: IndexAdvanceX, "
    in_out_ImFont->FallbackAdvanceX     = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->FallbackAdvanceX),"FallbackAdvanceX"));
    in_out_ImFont->FontSize             = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->FontSize),"FontSize"));

     //#error "6 - Not found ImFont->ImVector<ImWchar>, do not know what to do for the variables: IndexLookup, "

     //#error "6 - Not found ImFont->ImVector<ImFontGlyph>, do not know what to do for the variables: Glyphs, "
    static ImFontGlyph var_ImFontGlyph_156;//TODO: 11 check here, apparently, "ImFont->FallbackGlyph" is a pointer
    in_out_ImFont->FallbackGlyph        = lua_pop_ImFont_pointer(lua, index, &var_ImFontGlyph_156);
    static ImFontAtlas var_ImFontAtlas_157;//TODO: 11 check here, apparently, "ImFont->ContainerAtlas" is a pointer
    in_out_ImFont->ContainerAtlas       = lua_pop_ImFont_pointer(lua, index, &var_ImFontAtlas_157);
    static ImFontConfig var_ImFontConfig_158;//TODO: 11 check here, apparently, "ImFont->ConfigData" is a pointer
    in_out_ImFont->ConfigData           = lua_pop_ImFont_pointer(lua, index, &var_ImFontConfig_158);
    in_out_ImFont->ConfigDataCount      = static_cast<short>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->ConfigDataCount),"ConfigDataCount"));
    lua_getfield(lua, index, "FallbackChar");
    in_out_ImFont->FallbackChar         = luaL_checkinteger(lua,index);
    lua_getfield(lua, index, "EllipsisChar");
    in_out_ImFont->EllipsisChar         = luaL_checkinteger(lua,index);
    in_out_ImFont->Scale                = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->Scale),"Scale"));
    in_out_ImFont->Ascent               = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->Ascent),"Ascent"));
    in_out_ImFont->Descent              = static_cast<float>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->Descent),"Descent"));
    in_out_ImFont->MetricsTotalSurface  = static_cast<int>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->MetricsTotalSurface),"MetricsTotalSurface"));
    in_out_ImFont->DirtyLookupTables    = static_cast<bool>(get_number_from_field(lua,index,static_cast<lua_Number>(in_out_ImFont->DirtyLookupTables),"DirtyLookupTables"));

    return in_out_ImFont;
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


void lua_pop_ImFont_pointer(lua_State *lua, const ImFontConfig * p_in_ImFontConfig)
{
    if (p_in_ImFontConfig == nullptr)
    {
        lua_log_error(lua,"ImFontConfig can not be null");
    }
    else
    {
        lua_newtable(lua);
        //if(p_in_ImFontConfig->FontData)
        //{
        //    ;
        //    lua_setfield(lua, -2, "FontData");
        //}
        lua_pushinteger(lua,p_in_ImFontConfig->FontDataSize);
        lua_setfield(lua, -2, "FontDataSize");
        lua_pushboolean(lua,p_in_ImFontConfig->FontDataOwnedByAtlas);
        lua_setfield(lua, -2, "FontDataOwnedByAtlas");
        lua_pushinteger(lua,p_in_ImFontConfig->FontNo);
        lua_setfield(lua, -2, "FontNo");
        lua_pushnumber(lua,p_in_ImFontConfig->SizePixels);
        lua_setfield(lua, -2, "SizePixels");
        lua_pushinteger(lua,p_in_ImFontConfig->OversampleH);
        lua_setfield(lua, -2, "OversampleH");
        lua_pushinteger(lua,p_in_ImFontConfig->OversampleV);
        lua_setfield(lua, -2, "OversampleV");
        lua_pushboolean(lua,p_in_ImFontConfig->PixelSnapH);
        lua_setfield(lua, -2, "PixelSnapH");
        lua_push_ImVec2(lua,p_in_ImFontConfig->GlyphExtraSpacing);
        lua_setfield(lua, -2, "GlyphExtraSpacing");
        lua_push_ImVec2(lua,p_in_ImFontConfig->GlyphOffset);
        lua_setfield(lua, -2, "GlyphOffset");
        if(p_in_ImFontConfig->GlyphRanges)
        {
            lua_pushinteger(lua,*p_in_ImFontConfig->GlyphRanges);
            lua_setfield(lua, -2, "GlyphRanges");
        }
        lua_pushnumber(lua,p_in_ImFontConfig->GlyphMinAdvanceX);
        lua_setfield(lua, -2, "GlyphMinAdvanceX");
        lua_pushnumber(lua,p_in_ImFontConfig->GlyphMaxAdvanceX);
        lua_setfield(lua, -2, "GlyphMaxAdvanceX");
        lua_pushboolean(lua,p_in_ImFontConfig->MergeMode);
        lua_setfield(lua, -2, "MergeMode");
        lua_pushinteger(lua,p_in_ImFontConfig->FontBuilderFlags);
        lua_setfield(lua, -2, "FontBuilderFlags");
        lua_pushnumber(lua,p_in_ImFontConfig->RasterizerMultiply);
        lua_setfield(lua, -2, "RasterizerMultiply");
        lua_pushinteger(lua,p_in_ImFontConfig->EllipsisChar);
        lua_setfield(lua, -2, "EllipsisChar");
        lua_pushlstring(lua,p_in_ImFontConfig->Name,sizeof(p_in_ImFontConfig->Name)-1);
        lua_setfield(lua, -2, "Name");
        /*if(p_in_ImFontConfig->DstFont)
        {
            lua_push_ImFont(lua,*p_in_ImFontConfig->DstFont);
            lua_setfield(lua, -2, "DstFont");
        }*/

    }
}

void lua_pop_ImFont_pointer(lua_State *lua, const ImFontAtlas * p_in_ImFontAtlas)
{
    if (p_in_ImFontAtlas == nullptr)
    {
        lua_log_error(lua,"ImFontAtlas can not be null");
    }
    else
    {
        lua_newtable(lua);
        lua_pushboolean(lua,p_in_ImFontAtlas->Locked);
        lua_setfield(lua, -2, "Locked");
        lua_pushinteger(lua,p_in_ImFontAtlas->Flags);
        lua_setfield(lua, -2, "Flags");
        //if(p_in_ImFontAtlas->TexID)
        //{
        //    ;
        //    lua_setfield(lua, -2, "TexID");
        //}
        lua_pushinteger(lua,p_in_ImFontAtlas->TexDesiredWidth);
        lua_setfield(lua, -2, "TexDesiredWidth");
        lua_pushinteger(lua,p_in_ImFontAtlas->TexGlyphPadding);
        lua_setfield(lua, -2, "TexGlyphPadding");
        //if(p_in_ImFontAtlas->TexPixelsAlpha8)
        //{
        //    lua_pushstring(lua, p_in_ImFontAtlas->TexPixelsAlpha8);
        //    lua_setfield(lua, -2, "TexPixelsAlpha8");
        //}
        if(p_in_ImFontAtlas->TexPixelsRGBA32)
        {
            lua_pushinteger(lua,*p_in_ImFontAtlas->TexPixelsRGBA32);
            lua_setfield(lua, -2, "TexPixelsRGBA32");
        }
        lua_pushinteger(lua,p_in_ImFontAtlas->TexWidth);
        lua_setfield(lua, -2, "TexWidth");
        lua_pushinteger(lua,p_in_ImFontAtlas->TexHeight);
        lua_setfield(lua, -2, "TexHeight");
        lua_push_ImVec2(lua,p_in_ImFontAtlas->TexUvScale);
        lua_setfield(lua, -2, "TexUvScale");
        lua_push_ImVec2(lua,p_in_ImFontAtlas->TexUvWhitePixel);
        lua_setfield(lua, -2, "TexUvWhitePixel");

         //#error "5 - Not found ImFontAtlas->ImVector<ImFont>, do not know what to do for the variables: Fonts, "

         //#error "5 - Not found ImFontAtlas->ImVector<ImFontAtlasCustomRect>, do not know what to do for the variables: CustomRects, "

         //#error "5 - Not found ImFontAtlas->ImVector<ImFontConfig>, do not know what to do for the variables: ConfigData, "
        //push_int_arrayFromTable(lua,p_in_ImFontAtlas->CustomRectIds ,sizeof(p_in_ImFontAtlas->CustomRectIds) / sizeof(int));//TODO: 8 check if the type is right
        //lua_setfield(lua, -2, "CustomRectIds");

         //#error "5 - Not found ImFontAtlas->typedef, do not know what to do for the variables: ImFontAtlasCustomRect    CustomRect, "

         //#error "5 - Not found ImFontAtlas->typedef, do not know what to do for the variables: ImFontGlyphRangesBuilder GlyphRangesBuilder, "

    }
}


void lua_pop_ImFont_pointer(lua_State *lua, const ImFontGlyph * p_in_ImFontGlyph)
{
    if (p_in_ImFontGlyph == nullptr)
    {
        lua_log_error(lua,"ImFontGlyph can not be null");
    }
    else
    {
        lua_newtable(lua);
        lua_pushinteger(lua,p_in_ImFontGlyph->Codepoint);
        lua_setfield(lua, -2, "Codepoint");
        lua_pushnumber(lua,p_in_ImFontGlyph->AdvanceX);
        lua_setfield(lua, -2, "AdvanceX");
        lua_pushnumber(lua,p_in_ImFontGlyph->X0);
        lua_setfield(lua, -2, "X0");
        lua_pushnumber(lua,p_in_ImFontGlyph->Y0);
        lua_setfield(lua, -2, "Y0");
        lua_pushnumber(lua,p_in_ImFontGlyph->X1);
        lua_setfield(lua, -2, "X1");
        lua_pushnumber(lua,p_in_ImFontGlyph->Y1);
        lua_setfield(lua, -2, "Y1");
        lua_pushnumber(lua,p_in_ImFontGlyph->U0);
        lua_setfield(lua, -2, "U0");
        lua_pushnumber(lua,p_in_ImFontGlyph->V0);
        lua_setfield(lua, -2, "V0");
        lua_pushnumber(lua,p_in_ImFontGlyph->U1);
        lua_setfield(lua, -2, "U1");
        lua_pushnumber(lua,p_in_ImFontGlyph->V1);
        lua_setfield(lua, -2, "V1");

    }
}

void lua_pop_ImFontConfig_pointer(lua_State *lua, const ImFont * p_in_ImFont)
{
    if (p_in_ImFont == nullptr)
    {
        lua_log_error(lua,"ImFont can not be null");
    }
    else
    {
        lua_newtable(lua);

         //#error "5 - Not found ImFont->ImVector<float>, do not know what to do for the variables: IndexAdvanceX, "
        lua_pushnumber(lua,p_in_ImFont->FallbackAdvanceX);
        lua_setfield(lua, -2, "FallbackAdvanceX");
        lua_pushnumber(lua,p_in_ImFont->FontSize);
        lua_setfield(lua, -2, "FontSize");

         //#error "5 - Not found ImFont->ImVector<ImWchar>, do not know what to do for the variables: IndexLookup, "

         //#error "5 - Not found ImFont->ImVector<ImFontGlyph>, do not know what to do for the variables: Glyphs, "
        if(p_in_ImFont->FallbackGlyph)
        {
            lua_push_ImFontGlyph(lua,*p_in_ImFont->FallbackGlyph);
            lua_setfield(lua, -2, "FallbackGlyph");
        }
        if(p_in_ImFont->ContainerAtlas)
        {
            lua_push_ImFontAtlas(lua,*p_in_ImFont->ContainerAtlas);
            lua_setfield(lua, -2, "ContainerAtlas");
        }
        if(p_in_ImFont->ConfigData)
        {
            lua_push_ImFontConfig(lua,*p_in_ImFont->ConfigData);
            lua_setfield(lua, -2, "ConfigData");
        }
        lua_pushinteger(lua,p_in_ImFont->ConfigDataCount);
        lua_setfield(lua, -2, "ConfigDataCount");
        lua_pushinteger(lua,p_in_ImFont->FallbackChar);
        lua_setfield(lua, -2, "FallbackChar");
        lua_pushinteger(lua,p_in_ImFont->EllipsisChar);
        lua_setfield(lua, -2, "EllipsisChar");
        lua_pushnumber(lua,p_in_ImFont->Scale);
        lua_setfield(lua, -2, "Scale");
        lua_pushnumber(lua,p_in_ImFont->Ascent);
        lua_setfield(lua, -2, "Ascent");
        lua_pushnumber(lua,p_in_ImFont->Descent);
        lua_setfield(lua, -2, "Descent");
        lua_pushinteger(lua,p_in_ImFont->MetricsTotalSurface);
        lua_setfield(lua, -2, "MetricsTotalSurface");
        lua_pushboolean(lua,p_in_ImFont->DirtyLookupTables);
        lua_setfield(lua, -2, "DirtyLookupTables");

    }
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
            if(strcmp(sNext,"Alpha")                        == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.Alpha);
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
            else if(strcmp(sNext,"GrabMinSize")              == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.GrabMinSize);
            }
            else if(strcmp(sNext,"GrabRounding")             == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.GrabRounding);
            }
            else if(strcmp(sNext,"TabRounding")              == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.TabRounding);
            }
            else if(strcmp(sNext,"TabBorderSize")            == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.TabBorderSize);
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
            else if(strcmp(sNext,"AntiAliasedFill")          == 0)
            {
                lua_pushboolean(lua,ret_ImGuiStyle.AntiAliasedFill);
            }
            else if(strcmp(sNext,"CurveTessellationTol")     == 0)
            {
                lua_pushnumber(lua,ret_ImGuiStyle.CurveTessellationTol);
            }
            else if(strncmp(sNext,"ImGuiCol_",9)             == 0)
            {
                const auto it = ImGuiCol_map.find(sNext);
                if(it != ImGuiCol_map.end())
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

int onGetVersionImGuiLua(lua_State *lua)
{
    //  Get the compiled version string e.g. "1.23" (essentially the compiled value for IMGUI_VERSION)
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
        flags                   = luaL_checkinteger(lua,index_input++);
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
    ImGuiWindowFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    const bool ret_bool         = ImGui::BeginChild(p_str_id,size,border,flags);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onEndChildImGuiLua(lua_State *)
{
    ImGui::EndChild();
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
    ImGuiFocusedFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) : 0;
    const bool ret_bool          = ImGui::IsWindowFocused(flags);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsWindowHoveredImGuiLua(lua_State *lua)
{
    //  Is current window hovered (and typically: not blocked by a popup/modal)? see flags for options. NB: If you are trying to check whether your mouse should be dispatched to imgui or to your app, you should use the 'io.WantCaptureMouse' boolean for that! Please read the FAQ!
    int index_input              = 1;
    const int top                = lua_gettop(lua);
    ImGuiHoveredFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) : 0;
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
    const float zoom  = key_mouse::mouse_wheel;
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

#ifdef  PLUGIN_CALLBACK

int onSetNextWindowSizeConstraintsImGuiLua(lua_State *lua)
{
    //  Set next window size limits. use -1,-1 on either X/Y axis to preserve the current size. Sizes will be rounded down. Use callback to apply non-trivial programmatic constraints.
    int index_input                             = 1;
    ImVec2 size_min                             = lua_pop_ImVec2(lua, index_input++);
    ImVec2 size_max                             = lua_pop_ImVec2(lua, index_input++);
    ImGui::SetNextWindowSizeConstraints(size_min,size_max,nullptr,nullptr);
    return 0;
}
#endif

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


int onSetWindowFontScaleImGuiLua(lua_State *lua)
{
    //  Set font scale. Adjust IO.FontGlobalScale if you want to scale all windows. This is an old API! For correct scaling, prefer to reload font + rebuild ImFontAtlas + call style.ScaleAllSizes().
    int index_input    = 1;
    const float scale  = luaL_checknumber(lua,index_input++);
    ImGui::SetWindowFontScale(scale);
    return 0;
}
    
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

int onGetContentRegionMaxImGuiLua(lua_State *lua)
{
    //  Current content boundaries (typically window boundaries including scrolling, or current column boundaries), in windows coordinates
    const ImVec2 ret_ImVec2  = ImGui::GetContentRegionMax();
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

int onGetContentRegionAvailImGuiLua(lua_State *lua)
{
    //  == GetContentRegionMax() - GetCursorPos()
    const ImVec2 ret_ImVec2  = ImGui::GetContentRegionAvail();
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

int onGetWindowContentRegionMinImGuiLua(lua_State *lua)
{
    //  Content boundaries min (roughly (0,0)-Scroll), in window coordinates
    const ImVec2 ret_ImVec2  = ImGui::GetWindowContentRegionMin();
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

int onGetWindowContentRegionMaxImGuiLua(lua_State *lua)
{
    //  Content boundaries max (roughly (0,0)+Size-Scroll) where Size can be override with SetNextWindowContentSize(), in window coordinates
    const ImVec2 ret_ImVec2  = ImGui::GetWindowContentRegionMax();
    lua_push_ImVec2(lua,ret_ImVec2);
    return 1;
}

int onGetWindowContentRegionWidthImGuiLua(lua_State *lua)
{
    // 
    const float ret_float  = ImGui::GetWindowContentRegionWidth();
    lua_pushnumber(lua,ret_float);
    return 1;
}

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
    /*
    void IsScrollVisible(bool* x, bool * y)
    {
        ImGuiWindow* window = GImGui->CurrentWindow;
        *x = window->ScrollbarX;
        *y = window->ScrollbarY;
    }
    */
    bool x = false;
    bool y = false;
    ImGui::IsScrollVisible(&x,&y);
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

int onPushFontImGuiLua(lua_State *lua)
{
    //  Use nullptr as a shortcut to push default font
    int index_input  = 1;
    ImFont font;
    ImFont * p_font  =  lua_type(lua,index_input) == LUA_TNIL ? nullptr : lua_pop_ImFont_pointer(lua, index_input, &font);
    ImGui::PushFont(p_font);
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
        const auto it = ImGuiCol_map.find(styleCol);
        if(it != ImGuiCol_map.end())
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

int onGetFontImGuiLua(lua_State *lua)
{
    //  Get current font
    const ImFont* ret_ImFont  = ImGui::GetFont();
    if(ret_ImFont)
        lua_push_ImFont(lua,*ret_ImFont);
    return ret_ImFont != nullptr ? 1 : 0;
}

int onGetFontSizeImGuiLua(lua_State *lua)
{
    //  Get current font size (= height in pixels) of current font with current scale applied
    const float ret_float  = ImGui::GetFontSize();
    lua_pushnumber(lua,ret_float);
    return 1;
}

int onGetFontTexUvWhitePixelImGuiLua(lua_State *lua)
{
    //  Get UV coordinate for a while pixel, useful to draw custom shapes via the ImDrawList API
    const ImVec2 ret_ImVec2  = ImGui::GetFontTexUvWhitePixel();
    lua_push_ImVec2(lua,ret_ImVec2);
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

int onPushAllowKeyboardFocusImGuiLua(lua_State *lua)
{
    //  Allow focusing using TAB/Shift-TAB, enabled by default but you can disable it for certain widgets
    int index_input                  = 1;
    const bool allow_keyboard_focus  = lua_toboolean(lua,index_input++);
    ImGui::PushAllowKeyboardFocus(allow_keyboard_focus);
    return 0;
}

int onPopAllowKeyboardFocusImGuiLua(lua_State *lua)
{
    ImGui::PopAllowKeyboardFocus();
    return 0;
}

int onPushButtonRepeatImGuiLua(lua_State *lua)
{
    //  In 'repeat' mode, Button*() functions return repeated true in a typematic manner (using io.KeyRepeatDelay/io.KeyRepeatRate setting). Note that you can call IsItemActive() after any Button() to tell if the button is held in the current frame.
    int index_input    = 1;
    const bool repeat  = lua_toboolean(lua,index_input++);
    ImGui::PushButtonRepeat(repeat);
    return 0;
}

int onPopButtonRepeatImGuiLua(lua_State *lua)
{
    ImGui::PopButtonRepeat();
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

int onGetFrameHeightImGuiLua(lua_State *lua)
{
    //  ~FontSize + style.FramePadding.y * 2
    const float ret_float  = ImGui::GetFrameHeight();
    lua_pushnumber(lua,ret_float);
    return 1;
}

int onGetFrameHeightWithSpacingImGuiLua(lua_State *lua)
{
    //  ~FontSize + style.FramePadding.y * 2 + style.ItemSpacing.y (distance in pixels between 2 consecutive lines of framed widgets)
    const float ret_float  = ImGui::GetFrameHeightWithSpacing();
    lua_pushnumber(lua,ret_float);
    return 1;
}

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

int onGetIDImGuiLua(lua_State *lua)
{
    //  Calculate unique ID (hash of whole ID stack + given parameter). e.g. if you want to query into ImGuiStorage yourself
    int index_input            = 1;
    const char * p_str_id      = luaL_checkstring(lua,index_input++);
    const ImGuiID ret_ImGuiID  = ImGui::GetID(p_str_id);
    lua_pushinteger(lua,ret_ImGuiID);
    return 1;
}

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
    ImGuiDir dir           = luaL_checkinteger(lua, index_input++);
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
    ImTextureID user_texture_id         = reinterpret_cast<ImTextureID>(0);
    if(lua_type(lua,index_input) == LUA_TNUMBER)
        user_texture_id                 = reinterpret_cast<ImTextureID>(lua_tointeger(lua,index_input++));
    else
        user_texture_id                 = reinterpret_cast<ImTextureID>(get_texture_id(lua,luaL_checkstring(lua,index_input++),width,height));
    ImVec2 size (static_cast<float>(width),static_cast<float>(height));
    if(top >= index_input && lua_type(lua,index_input) != LUA_TNIL)
        size                                = lua_pop_ImVec2(lua, index_input);
    
    index_input++;
    const ImVec2 uv0                    = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(0,0);
    const ImVec2 uv1                    = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(1,1);
    const ImVec4 bg_col                 = top >= index_input ? lua_get_rgba_to_ImVec4_fromTable(lua, index_input++) : ImVec4(1,1,1,1);
    const ImVec4 line_color             = top >= index_input ? lua_get_rgba_to_ImVec4_fromTable(lua, index_input++) : ImVec4(0,0,0,1);
    ImGui::Image(user_texture_id, size, uv0 , uv1 , bg_col, line_color );
    return 0;
}

int onImageQuadImGuiLua(lua_State *lua)
{
    int index_input                     = 1;
    const int top                       = lua_gettop(lua);
    unsigned int width                  = 0;
    unsigned int height                 = 0;
    ImTextureID user_texture_id         = reinterpret_cast<ImTextureID>(0);
    if(lua_type(lua,index_input) == LUA_TNUMBER)
        user_texture_id                 = reinterpret_cast<ImTextureID>(lua_tointeger(lua,index_input++));
    else
        user_texture_id                 = reinterpret_cast<ImTextureID>(get_texture_id(lua,luaL_checkstring(lua,index_input++),width,height));
    ImVec2 size (static_cast<float>(width),static_cast<float>(height));
    if(top >= index_input && lua_type(lua,index_input) != LUA_TNIL)
        size                                = lua_pop_ImVec2(lua, index_input);
    
    index_input++;
    const ImVec2 uv0                    = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 uv1                    = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 uv2                    = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 uv3                    = lua_pop_ImVec2(lua, index_input++);
    const ImVec4 bg_col                 = top >= index_input ? lua_get_rgba_to_ImVec4_fromTable(lua, index_input++) : ImVec4(1,1,1,1);
    const ImVec4 line_color             = top >= index_input ? lua_get_rgba_to_ImVec4_fromTable(lua, index_input++) : ImVec4(0,0,0,1);
    ImGui::ImageQuad(user_texture_id, size, uv0 , uv1 , uv2, uv3,  bg_col, line_color );
    return 0;
}

int onImageButtonImGuiLua(lua_State *lua)
{
    int index_input                     = 1;
    const int top                       = lua_gettop(lua);
    unsigned int width                  = 0;
    unsigned int height                 = 0;
    ImTextureID user_texture_id         = reinterpret_cast<ImTextureID>(0);
    if(lua_type(lua,index_input) == LUA_TNUMBER)
        user_texture_id                 = reinterpret_cast<ImTextureID>(lua_tointeger(lua,index_input++));
    else
        user_texture_id                 = reinterpret_cast<ImTextureID>(get_texture_id(lua,luaL_checkstring(lua,index_input++),width,height));
    ImVec2 size (static_cast<float>(width),static_cast<float>(height));
    if(top >= index_input && lua_type(lua,index_input) != LUA_TNIL)
        size                                = lua_pop_ImVec2(lua, index_input);
    
    index_input++;
    const ImVec2 uv0                    = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(0,0);
    const ImVec2 uv1                    = top >= index_input ? lua_pop_ImVec2(lua, index_input++) : ImVec2(1,1);
    const int frame_padding             = top >= index_input ? luaL_checkinteger(lua, index_input++): -1;
    const ImVec4 bg_col                 = top >= index_input ? lua_get_rgba_to_ImVec4_fromTable(lua, index_input++) : ImVec4(1,1,1,1);
    const ImVec4 tint_col               = top >= index_input ? lua_get_rgba_to_ImVec4_fromTable(lua, index_input++) : ImVec4(1,1,1,1);
    const bool result                   = ImGui::ImageButton(user_texture_id, size, uv0 , uv1 , frame_padding,  bg_col, tint_col );
    lua_pushboolean(lua,result);
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
    unsigned int p_flags   = luaL_checkinteger(lua,index_input++);
    const int flags_value  = luaL_checkinteger(lua,index_input++);
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
    ImGuiComboFlags flags         = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    float value            = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const float v_speed    = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const float v_min      = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const float v_max      = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const char * p_format  = top >= index_input ? luaL_checkstring(lua,index_input++) :  "%.3f";
    const float power      = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const bool ret_bool    = ImGui::DragFloat(p_label,&value,v_speed,v_min,v_max,p_format,power);
    lua_pushboolean(lua,ret_bool);
    lua_pushnumber(lua,value);
    return 2;
}

int onDragFloat2ImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    float value[2]         = {0, 0};
    get_float_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"float table[2]");
    const float v_speed    = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const float v_min      = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const float v_max      = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const char * p_format  = top >= index_input ? luaL_checkstring(lua,index_input++) :  "%.3f";
    const float power      = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const bool ret_bool    = ImGui::DragFloat2(p_label,value,v_speed,v_min,v_max,p_format,power);
    lua_pushboolean(lua,ret_bool);
    push_float_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onDragFloat3ImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    float value[3]         = {0, 0, 0};
    get_float_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"float table[3]");
    const float v_speed    = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const float v_min      = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const float v_max      = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const char * p_format  = top >= index_input ? luaL_checkstring(lua,index_input++) :  "%.3f";
    const float power      = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const bool ret_bool    = ImGui::DragFloat3(p_label,value,v_speed,v_min,v_max,p_format,power);
    lua_pushboolean(lua,ret_bool);
    push_float_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onDragFloat4ImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    float value[4]         = {0, 0, 0, 0};
    get_float_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"float table[4]");
    const float v_speed    = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const float v_min      = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const float v_max      = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    const char * p_format  = top >= index_input ? luaL_checkstring(lua,index_input++) :  "%.3f";
    const float power      = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const bool ret_bool    = ImGui::DragFloat4(p_label,value,v_speed,v_min,v_max,p_format,power);
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
    const float power          = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const bool ret_bool        = ImGui::DragFloatRange2(p_label,&v_current_min,&v_current_max,v_speed,v_min,v_max,p_format,p_format_max,power);
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
    //  Adjust format to decorate the value with a prefix or a suffix for in-slider labels or unit display. Use power!=1.0 for power curve sliders
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    float value            = luaL_checknumber(lua,index_input++);
    const float v_min      = luaL_checknumber(lua,index_input++);
    const float v_max      = luaL_checknumber(lua,index_input++);
    const char * p_format  = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    const float power      = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const bool ret_bool    = ImGui::SliderFloat(p_label,&value,v_min,v_max,p_format,power);
    lua_pushboolean(lua,ret_bool);
    lua_pushnumber(lua,value);
    return 2;
}

int onSliderFloat2ImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    float value[2]         = {0.0f,0.0f};
    get_float_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"float table[2]");
    const float v_min      = luaL_checknumber(lua,index_input++);
    const float v_max      = luaL_checknumber(lua,index_input++);
    const char * p_format  = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    const float power      = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const bool ret_bool    = ImGui::SliderFloat2(p_label,value,v_min,v_max,p_format,power);
    lua_pushboolean(lua,ret_bool);
    push_float_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onSliderFloat3ImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    float value[3]         = {0.0f,0.0f,0.0f};
    get_float_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"float table[3]");
    const float v_min      = luaL_checknumber(lua,index_input++);
    const float v_max      = luaL_checknumber(lua,index_input++);
    const char * p_format  = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    const float power      = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const bool ret_bool    = ImGui::SliderFloat3(p_label,value,v_min,v_max,p_format,power);
    lua_pushboolean(lua,ret_bool);
    push_float_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onSliderFloat4ImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    float value[4]         = {0.0f,0.0f,0.0f,0.0f};
    get_float_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"float table[4]");
    const float v_min      = luaL_checknumber(lua,index_input++);
    const float v_max      = luaL_checknumber(lua,index_input++);
    const char * p_format  = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    const float power      = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    const bool ret_bool    = ImGui::SliderFloat4(p_label,value,v_min,v_max,p_format,power);
    lua_pushboolean(lua,ret_bool);
    push_float_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onSliderAngleImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    float value            = luaL_checknumber(lua,index_input++);
    const float v_min      = luaL_checknumber(lua,index_input++);
    const float v_max      = luaL_checknumber(lua,index_input++);
    const char * p_format  = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    const bool ret_bool    = ImGui::SliderAngle(p_label,&value,v_min,v_max,p_format);
    lua_pushboolean(lua,ret_bool);
    lua_pushnumber(lua,value);
    return 2;
}

int onSliderIntImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    int value              = luaL_checkinteger(lua,index_input++);
    const int v_min        = luaL_checkinteger(lua,index_input++);
    const int v_max        = luaL_checkinteger(lua,index_input++);
    const char * p_format  = top >= index_input ? lua_tostring(lua,index_input++) :  "%d";
    const bool ret_bool    = ImGui::SliderInt(p_label,&value,v_min,v_max,p_format);
    lua_pushboolean(lua,ret_bool);
    lua_pushinteger(lua,value);
    return 2;
}

int onSliderInt2ImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    int value[2]           = {0,0};
    get_int_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"int table[2]");
    const int v_min        = luaL_checkinteger(lua,index_input++);
    const int v_max        = luaL_checkinteger(lua,index_input++);
    const char * p_format  = top >= index_input ? lua_tostring(lua,index_input++) :  "%d";
    const bool ret_bool    = ImGui::SliderInt2(p_label,value,v_min,v_max,p_format);
    lua_pushboolean(lua,ret_bool);
    push_int_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onSliderInt3ImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    int value[3]           = {0,0,0};
    get_int_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"int table[3]");
    const int v_min        = luaL_checkinteger(lua,index_input++);
    const int v_max        = luaL_checkinteger(lua,index_input++);
    const char * p_format  = top >= index_input ? lua_tostring(lua,index_input++) :  "%d";
    const bool ret_bool    = ImGui::SliderInt3(p_label,value,v_min,v_max,p_format);
    lua_pushboolean(lua,ret_bool);
    push_int_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onSliderInt4ImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    int value[4]           = {0,0,0,0};
    get_int_arrayFromTable(lua,index_input++,value,sizeof(value) / sizeof(value[0]) ,"int table[4]");
    const int v_min        = luaL_checkinteger(lua,index_input++);
    const int v_max        = luaL_checkinteger(lua,index_input++);
    const char * p_format  = top >= index_input ? lua_tostring(lua,index_input++) :  "%d";
    const bool ret_bool    = ImGui::SliderInt4(p_label,value,v_min,v_max,p_format);
    lua_pushboolean(lua,ret_bool);
    push_int_arrayFromTable(lua,value,sizeof(value) / sizeof(value[0]));
    return 2;
}

int onVSliderFloatImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    ImVec2 size            = lua_pop_ImVec2(lua, index_input++);
    float value            = luaL_checknumber(lua,index_input++);
    const float v_min      = luaL_checknumber(lua,index_input++);
    const float v_max      = luaL_checknumber(lua,index_input++);
    const char * p_format  = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    const float power      = top >= index_input ? luaL_checknumber(lua,index_input++) : 1.0f;
    const bool ret_bool    = ImGui::VSliderFloat(p_label,size,&value,v_min,v_max,p_format,power);
    lua_pushboolean(lua,ret_bool);
    lua_pushnumber(lua,value);
    return 2;
}

int onVSliderIntImGuiLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const char * p_label   = luaL_checkstring(lua,index_input++);
    ImVec2 size            = lua_pop_ImVec2(lua, index_input++);
    int value              = luaL_checkinteger(lua,index_input++);
    const int v_min        = luaL_checkinteger(lua,index_input++);
    const int v_max        = luaL_checkinteger(lua,index_input++);
    const char * p_format  = top >= index_input ? lua_tostring(lua,index_input++) :  "%d";
    const bool ret_bool    = ImGui::VSliderInt(p_label,size,&value,v_min,v_max,p_format);
    lua_pushboolean(lua,ret_bool);
    lua_pushinteger(lua,value);
    return 2;
}

#ifdef  PLUGIN_CALLBACK


int onInputTextImGuiLua(lua_State *lua)
{
    int index_input                             = 1;
    const int top                               = lua_gettop(lua);
    const char * p_label                        = luaL_checkstring(lua,index_input++);
    std::string text                            = luaL_checkstring(lua,index_input++);
    ImGuiInputTextFlags flags                   = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    bool ret_bool                               = ImGui::InputText(p_label,&text,flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushstring(lua,text.c_str());
    return 2;
}


int onInputTextMultilineImGuiLua(lua_State *lua)
{
    int index_input                             = 1;
    const int top                               = lua_gettop(lua);
    const char * p_label                        = luaL_checkstring(lua,index_input++);
    std::string text                            = luaL_checkstring(lua,index_input++);
    const ImVec2 size                           = top >= index_input ? lua_pop_ImVec2(lua,index_input++) : ImVec2(0,0);
    ImGuiInputTextFlags flags                   = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    bool ret_bool                               = ImGui::InputTextMultiline(p_label,&text,size,flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushstring(lua,text.c_str());
    return 2;
}

int onInputTextWithHintImGuiLua(lua_State *lua)
{
    int index_input                             = 1;
    const int top                               = lua_gettop(lua);
    const char * p_label                        = luaL_checkstring(lua,index_input++);
    std::string text                            = luaL_checkstring(lua,index_input++);
    const char* hint                            = luaL_checkstring(lua,index_input++);
    ImGuiInputTextFlags flags                   = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    bool ret_bool                               = ImGui::InputTextWithHint(p_label,hint,&text,flags);
    lua_pushboolean(lua,ret_bool);
    lua_pushstring(lua,text.c_str());
    return 2;
}

#endif

int onInputFloatImGuiLua(lua_State *lua)
{
    int index_input                = 1;
    const int top                  = lua_gettop(lua);
    const char * p_label           = luaL_checkstring(lua,index_input++);
    float  value                   = luaL_checknumber(lua,index_input++);
    const float step               = top >= index_input ? luaL_checknumber(lua,index_input++) :  1;
    const float step_fast          = top >= index_input ? luaL_checknumber(lua,index_input++) :  100;
    const char * p_format          = top >= index_input ? lua_tostring(lua,index_input++) :  "%.3f";
    ImGuiInputTextFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiInputTextFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiInputTextFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiInputTextFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiInputTextFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiInputTextFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiColorEditFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiColorEditFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiColorEditFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiColorEditFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiColorEditFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    ImVec2 size                    = top >= index_input ? lua_pop_ImVec2(lua, index_input++) :  ImVec2(0,0);
    const bool ret_bool            = ImGui::ColorButton(p_desc_id,p_col,flags,size);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onSetColorEditOptionsImGuiLua(lua_State *lua)
{
    //  Initialize current options (generally on application startup) if you want to select a default format, picker type, etc. User will be able to change many settings, unless you pass the _NoOptions flag to your calls.
    int index_input            = 1;
    ImGuiColorEditFlags flags  = luaL_checkinteger(lua, index_input++);
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
    ImGuiTreeNodeFlags flags      = luaL_checkinteger(lua, index_input++);
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
    ImGuiTreeNodeFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiCond cond      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiSelectableFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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

int onListBoxHeaderImGuiLua(lua_State *lua)
{
    //  Use if you want to reimplement ListBox() will custom data or interactions. if the function return true, you can output elements then call ListBoxFooter() afterwards.
    int index_input       = 1;
    const int top         = lua_gettop(lua);
    const char * p_label  = luaL_checkstring(lua,index_input++);
    bool ret_bool         = false;
    if(lua_type(lua,index_input) == LUA_TTABLE)
    {
        const ImVec2  size         = top >= index_input ? lua_pop_ImVec2(lua, index_input++) :  ImVec2(0,0);
        ret_bool                   = ImGui::ListBoxHeader(p_label,size);
    }
    else
    {
        const int items_count      = luaL_checkinteger(lua,index_input++);
        const int height_in_items  = top >= index_input ? luaL_checkinteger(lua,index_input++) :  -1;
        ret_bool                   = ImGui::ListBoxHeader(p_label,items_count,height_in_items);
    }
    lua_pushboolean(lua,ret_bool);
    return 1;
}


int onListBoxFooterImGuiLua(lua_State *lua)
{
    //  Terminate the scrolling region. only call ListBoxFooter() if ListBoxHeader() returned true!
    ImGui::ListBoxFooter();
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
    ImGuiWindowFlags   flags    = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiMouseButton  mouse_button     = top >= index_input ? luaL_checkinteger(lua, index_input++) :  1;
    const bool ret_bool                = ImGui::BeginPopupContextItem(p_str_id,mouse_button);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onBeginPopupContextWindowImGuiLua(lua_State *lua)
{
    //  Helper to open and begin popup when clicked on current window.
    int index_input                    = 1;
    const int top                      = lua_gettop(lua);
    const char * p_str_id              = get_string_or_null(lua,index_input++);
    ImGuiMouseButton mouse_button      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  1;
    const bool also_over_items         = top >= index_input ? lua_toboolean(lua,index_input++) :  true;
    const bool ret_bool                = ImGui::BeginPopupContextWindow(p_str_id,mouse_button,also_over_items);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onBeginPopupContextVoidImGuiLua(lua_State *lua)
{
    //  Helper to open and begin popup when clicked in void (where there are no imgui windows).
    int index_input                    = 1;
    const int top                      = lua_gettop(lua);
    const char * p_str_id              = get_string_or_null(lua,index_input++);
    ImGuiMouseButton mouse_button      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  1;
    const bool ret_bool                = ImGui::BeginPopupContextVoid(p_str_id,mouse_button);
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
        flags                   = luaL_checkinteger(lua,index_input++);
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
    //  Helper to open popup when clicked on last item (note: actually triggers on the mouse _released_ event to be consistent with popup behaviors). return true when just opened.
    int index_input                    = 1;
    const int top                      = lua_gettop(lua);
    const char * p_str_id              = get_string_or_null(lua,index_input++);
    ImGuiMouseButton mouse_button      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    ImGui::OpenPopupOnItemClick(p_str_id,mouse_button);
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
    const int flag         = top > 1 ? luaL_checkinteger(lua,2) : 0;
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

int onColumnsImGuiLua(lua_State *lua)
{
    int index_input    = 1;
    const int top      = lua_gettop(lua);
    const int count    = top >= index_input ? luaL_checkinteger(lua,index_input++) :  1;
    const char * p_id  = top >= index_input ? lua_tostring(lua,index_input++) :  nullptr;
    const bool border  = top >= index_input ? lua_toboolean(lua,index_input++) :  true;
    ImGui::Columns(count,p_id,border);
    return 0;
}

int onNextColumnImGuiLua(lua_State *lua)
{
    //  Next column, defaults to current row or next row if the current row is finished
    ImGui::NextColumn();
    return 0;
}

int onGetColumnIndexImGuiLua(lua_State *lua)
{
    //  Get current column index
    const int ret_int  = ImGui::GetColumnIndex();
    lua_pushinteger(lua,ret_int);
    return 1;
}

int onGetColumnWidthImGuiLua(lua_State *lua)
{
    //  Get column width (in pixels). pass -1 to use current column
    int index_input         = 1;
    const int top           = lua_gettop(lua);
    const int column_index  = top >= index_input ? luaL_checkinteger(lua,index_input++) :  -1;
    const float ret_float   = ImGui::GetColumnWidth(column_index);
    lua_pushnumber(lua,ret_float);
    return 1;
}

int onSetColumnWidthImGuiLua(lua_State *lua)
{
    //  Set column width (in pixels). pass -1 to use current column
    int index_input         = 1;
    const int column_index  = luaL_checkinteger(lua,index_input++);
    const float width       = luaL_checknumber(lua,index_input++);
    ImGui::SetColumnWidth(column_index,width);
    return 0;
}

int onGetColumnOffsetImGuiLua(lua_State *lua)
{
    //  Get position of column line (in pixels, from the left side of the contents region). pass -1 to use current column, otherwise 0..GetColumnsCount() inclusive. column 0 is typically 0.0f
    int index_input         = 1;
    const int top           = lua_gettop(lua);
    const int column_index  = top >= index_input ? luaL_checkinteger(lua,index_input++) :  -1;
    const float ret_float   = ImGui::GetColumnOffset(column_index);
    lua_pushnumber(lua,ret_float);
    return 1;
}

int onSetColumnOffsetImGuiLua(lua_State *lua)
{
    //  Set position of column line (in pixels, from the left side of the contents region). pass -1 to use current column
    int index_input         = 1;
    const int column_index  = luaL_checkinteger(lua,index_input++);
    const float offset_x    = luaL_checknumber(lua,index_input++);
    ImGui::SetColumnOffset(column_index,offset_x);
    return 0;
}

int onGetColumnsCountImGuiLua(lua_State *lua)
{
    const int ret_int  = ImGui::GetColumnsCount();
    lua_pushinteger(lua,ret_int);
    return 1;
}

int onBeginTabBarImGuiLua(lua_State *lua)
{
    //  Create and append into a TabBar
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    const char * p_str_id       = get_string_or_null(lua,index_input++);
    ImGuiTabBarFlags   flags    = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiTabItemFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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

int onBeginDragDropSourceImGuiLua(lua_State *lua)
{
    //  Call when the current item is active. If this return true, you can call SetDragDropPayload() + EndDragDropSource()
    int index_input               = 1;
    const int top                 = lua_gettop(lua);
    ImGuiDragDropFlags   flags    = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    const bool ret_bool           = ImGui::BeginDragDropSource(flags);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onSetDragDropPayloadImGuiLua(lua_State *lua)
{
    //  Type is a user defined string of maximum 32 characters. Strings starting with '_' are reserved for dear imgui internal types. Data is copied and held by imgui.
    int index_input           = 1;
    const int top             = lua_gettop(lua);
    std::string str_p_type    = luaL_checkstring(lua,index_input++);
    if(str_p_type.size() > 32)
        str_p_type.resize(32);
    std::string  p_data       = top >= index_input ? luaL_checkstring(lua,index_input++) : "";
    ImGuiCond  cond           = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    const bool ret_bool       = ImGui::SetDragDropPayload(str_p_type.c_str(),p_data.data(),p_data.size(),cond);
    lua_pushboolean(lua,ret_bool);
    return 0;
}

int onEndDragDropSourceImGuiLua(lua_State *lua)
{
    //  Only call EndDragDropSource() if BeginDragDropSource() returns true!
    ImGui::EndDragDropSource();
    return 0;
}

int onBeginDragDropTargetImGuiLua(lua_State *lua)
{
    //  Call after submitting an item that may receive a payload. If this returns true, you can call AcceptDragDropPayload() + EndDragDropTarget()
    const bool ret_bool  = ImGui::BeginDragDropTarget();
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onAcceptDragDropPayloadImGuiLua(lua_State *lua)
{
    //  Accept contents of a given type. If ImGuiDragDropFlags_AcceptBeforeDelivery is set you can peek into the payload before the mouse button is released.
    int index_input                      = 1;
    const int top                        = lua_gettop(lua);
    const char * p_type                  = luaL_checkstring(lua,index_input++);
    ImGuiDragDropFlags flags             = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    const ImGuiPayload *ret_ImGuiPayload = ImGui::AcceptDragDropPayload(p_type,flags);
    if(ret_ImGuiPayload)
        lua_push_ImGuiPayload(lua,*ret_ImGuiPayload);
    return ret_ImGuiPayload == nullptr ? 0 : 1;
}

int onEndDragDropTargetImGuiLua(lua_State *lua)
{
    //  Only call EndDragDropTarget() if BeginDragDropTarget() returns true!
    ImGui::EndDragDropTarget();
    return 0;
}

int onGetDragDropPayloadImGuiLua(lua_State *lua)
{
    //  Peek directly into the current payload from anywhere. may return nullptr. use ImGuiPayload::IsDataType() to test for the payload type.
    const ImGuiPayload* ret_ImGuiPayload  = ImGui::GetDragDropPayload();
    if(ret_ImGuiPayload)
    {
        lua_push_ImGuiPayload(lua,*ret_ImGuiPayload);
        return 1;
    }
    lua_pushnil(lua);
    return 1;
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
    ImGuiHoveredFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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

int onSetItemAllowOverlapImGuiLua(lua_State *lua)
{
    //  Allow last item to be overlapped by a subsequent item. sometimes useful with invisible buttons, selectables, etc. to catch unused area.
    ImGui::SetItemAllowOverlap();
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

int onGetTimeImGuiLua(lua_State *lua)
{
    //  Get global imgui time. incremented by io.DeltaTime every frame.
    const double ret_double  = ImGui::GetTime();
    lua_pushnumber(lua,static_cast<lua_Number>(ret_double));
    return 1;
}

int onGetFrameCountImGuiLua(lua_State *lua)
{
    //  Get global imgui frame count. incremented by 1 every frame.
    const int ret_int  = ImGui::GetFrameCount();
    lua_pushinteger(lua,ret_int);
    return 1;
}

int onGetStyleColorNameImGuiLua(lua_State *lua)
{
    //  Get a string corresponding to the enum value (for display, saving, etc.).
    int index_input       = 1;
    ImGuiCol idx          = luaL_checkinteger(lua, index_input++);
    const char* ret_char  = ImGui::GetStyleColorName(idx);
    lua_pushstring(lua,ret_char);
    return 1;
}


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

int onCalcListClippingImGuiLua(lua_State *lua)
{
    //  Calculate coarse clipping for large list of evenly sized items. Prefer using the ImGuiListClipper higher-level helper if you can.
    int index_input                  = 1;
    const int items_count            = luaL_checkinteger(lua,index_input++);
    const float items_height         = luaL_checknumber(lua,index_input++);
    int   p_out_items_display_start  = 0;
    int   p_out_items_display_end    = 0;
    ImGui::CalcListClipping(items_count,items_height,&p_out_items_display_start,&p_out_items_display_end);
    lua_pushinteger(lua,p_out_items_display_start);
    lua_pushinteger(lua,p_out_items_display_end);
    return 2;
}

int onBeginChildFrameImGuiLua(lua_State *lua)
{
    //  Helper to create a child window / scrolling region that looks like a normal widget frame
    int index_input             = 1;
    const int top               = lua_gettop(lua);
    ImGuiID id                  = luaL_checkinteger(lua, index_input++);
    ImVec2 size(0,0);
    lua_pop_ImVec2_pointer(lua, index_input++, &size);
    ImGuiWindowFlags   flags    = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
    const bool ret_bool         = ImGui::BeginChildFrame(id,size,flags);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onEndChildFrameImGuiLua(lua_State *lua)
{
    //  Always call EndChildFrame() regardless of BeginChildFrame() return values (which indicates a collapsed/clipped window)
    ImGui::EndChildFrame();
    return 0;
}

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

int onGetKeyIndexImGuiLua(lua_State *lua)
{
    //  Map ImGuiKey_* values into user's key index. == io.KeyMap[key]
    int index_input         = 1;
    ImGuiKey imgui_key      = 0;
    if(lua_type(lua,index_input) == LUA_TNUMBER)
        imgui_key      = luaL_checkinteger(lua, index_input++);
    else
    {
        const char* key = luaL_checkstring(lua,index_input++);
        const auto  it  = enumKeyMap.find(key);
        if(it != enumKeyMap.end())
        {
            ImGuiIO& imGuIo = ImGui::GetIO();
            imgui_key = imGuIo.KeyMap[it->second];
        }
        else
        {
            char str [255] = "";
            snprintf(str,sizeof(str),"Key [%s] not found",key);
            lua_log_error(lua,str);
        }
    }
    const int ret_int       = ImGui::GetKeyIndex(imgui_key);
    lua_pushinteger(lua,ret_int);
    return 1;
}

int onIsKeyDownImGuiLua(lua_State *lua)
{
    //  Is key being held. == io.KeysDown[user_key_index].
    int index_input           = 1;
    int user_key_index        = 0;
    if(lua_type(lua,index_input) == LUA_TNUMBER)
        user_key_index        = luaL_checkinteger(lua, index_input++);
    else
    {
        const char* key       = luaL_checkstring(lua,index_input++);
        const auto  it        = enumKeyMap.find(key);
        if(it != enumKeyMap.end())
        {
            ImGuiIO& imGuIo = ImGui::GetIO();
            user_key_index  = static_cast<int>(imGuIo.KeyMap[it->second]);
        }
        else
        {
            char str [255] = "";
            snprintf(str,sizeof(str),"Key [%s] not found",key);
            lua_log_error(lua,str);
        }
    }
    const bool ret_bool       = ImGui::IsKeyDown(user_key_index); //key_mouse::KeysDown[user_key_index];
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsKeyPressedImGuiLua(lua_State *lua)
{
    //  Was key pressed (went from !Down to Down)? if repeat=true, uses io.KeyRepeatDelay / KeyRepeatRate
    int index_input           = 1;
    const int top             = lua_gettop(lua);
    int user_key_index        = 0;
    if(lua_type(lua,index_input) == LUA_TNUMBER)
        user_key_index        = luaL_checkinteger(lua, index_input++);
    else
    {
        const char* key       = luaL_checkstring(lua,index_input++);
        const auto  it        = enumKeyMap.find(key);
        if(it != enumKeyMap.end())
        {
            ImGuiIO& imGuIo = ImGui::GetIO();
            user_key_index  = static_cast<int>(imGuIo.KeyMap[it->second]);
        }
        else
        {
            char str [255] = "";
            snprintf(str,sizeof(str),"Key [%s] not found",key);
            lua_log_error(lua,str);
        }
    }
    const bool repeat         = top >= index_input ? lua_toboolean(lua,index_input++) :  true;
    const bool ret_bool       = ImGui::IsKeyPressed(user_key_index,repeat);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onIsKeyReleasedImGuiLua(lua_State *lua)
{
    //  Was key released (went from Down to !Down)?
    int index_input           = 1;
    int user_key_index        = 0;
    if(lua_type(lua,index_input) == LUA_TNUMBER)
        user_key_index        = luaL_checkinteger(lua, index_input++);
    else
    {
        const char* key       = luaL_checkstring(lua,index_input++);
        const auto  it        = enumKeyMap.find(key);
        if(it != enumKeyMap.end())
        {
            ImGuiIO& imGuIo = ImGui::GetIO();
            user_key_index  = static_cast<int>(imGuIo.KeyMap[it->second]);
        }
        else
        {
            char str [255] = "";
            snprintf(str,sizeof(str),"Key [%s] not found",key);
            lua_log_error(lua,str);
        }
    }
    const bool ret_bool       = ImGui::IsKeyReleased(user_key_index);
    lua_pushboolean(lua,ret_bool);
    return 1;
}

int onGetKeyPressedAmountImGuiLua(lua_State *lua)
{
    //  Uses provided repeat rate/delay. return a count, most often 0 or 1 but might be >1 if RepeatRate is small enough that DeltaTime > RepeatRate
    int index_input           = 1;
    int user_key_index        = 0;
    if(lua_type(lua,index_input) == LUA_TNUMBER)
        user_key_index        = luaL_checkinteger(lua, index_input++);
    else
    {
        const char* key       = luaL_checkstring(lua,index_input++);
        const auto  it        = enumKeyMap.find(key);
        if(it != enumKeyMap.end())
        {
            ImGuiIO& imGuIo = ImGui::GetIO();
            user_key_index  = static_cast<int>(imGuIo.KeyMap[it->second]);
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
    const int ret_int         = ImGui::GetKeyPressedAmount(user_key_index,repeat_delay,rate);
    lua_pushinteger(lua,ret_int);
    return 1;
}

int onGetMainMenuBarHeightImGuiLua(lua_State *lua)
{
    /* implementation
    float ImGui::GetMainMenuBarHeight()
    {
        float height = 0;
        if (ImGuiWindow* window = FindWindowByName("##MainMenuBar"))
            height = window->Size.y;
        return height;
    }
    */
    float height = ImGui::GetMainMenuBarHeight();
    lua_pushnumber(lua,height);
    return 1;
}

int onCaptureKeyboardFromAppImGuiLua(lua_State *lua)
{
    //  Attention: misleading name! manually override io.WantCaptureKeyboard flag next frame (said flag is entirely left for your application to handle). e.g. force capture keyboard when your widget is being hovered. This is equivalent to setting "io.WantCaptureKeyboard = want_capture_keyboard_value"; after the next NewFrame() call.
    int index_input                         = 1;
    const int top                           = lua_gettop(lua);
    const bool want_capture_keyboard_value  = top >= index_input ? lua_toboolean(lua,index_input++) :  true;
    ImGui::CaptureKeyboardFromApp(want_capture_keyboard_value);
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
    //  Attention: misleading name! manually override io.WantCaptureMouse flag next frame (said flag is entirely left for your application to handle). This is equivalent to setting "io.WantCaptureMouse = want_capture_mouse_value;" after the next NewFrame() call.
    int index_input                      = 1;
    const int top                        = lua_gettop(lua);
    const bool want_capture_mouse_value  = top >= index_input ? lua_toboolean(lua,index_input++) :  true;
    ImGui::CaptureMouseFromApp(want_capture_mouse_value);
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

int onLoadIniSettingsFromDiskImGuiLua(lua_State *lua)
{
    //  Call after CreateContext() and before the first call to NewFrame(). NewFrame() automatically calls LoadIniSettingsFromDisk(io.IniFilename).
    int index_input              = 1;
    const char * p_ini_filename  = luaL_checkstring(lua,index_input++);
    ImGui::LoadIniSettingsFromDisk(p_ini_filename);
    return 0;
}

int onLoadIniSettingsFromMemoryImGuiLua(lua_State *lua)
{
    //  Call after CreateContext() and before the first call to NewFrame() to provide .ini data from your own data source.
    int index_input          = 1;
    const char * p_ini_data  = luaL_checkstring(lua,index_input++);
    const size_t ini_size    = p_ini_data ? strlen(p_ini_data) : 0;
    ImGui::LoadIniSettingsFromMemory(p_ini_data,ini_size);
    return 0;
}

int onSaveIniSettingsToDiskImGuiLua(lua_State *lua)
{
    //  This is automatically called (if io.IniFilename is not empty) a few seconds after any modification that should be reflected in the .ini file (and also by DestroyContext).
    int index_input              = 1;
    const char * p_ini_filename  = luaL_checkstring(lua,index_input++);
    ImGui::SaveIniSettingsToDisk(p_ini_filename);
    return 0;
}

int onSaveIniSettingsToMemoryImGuiLua(lua_State *lua)
{
    //  Return a zero-terminated string with the .ini data which you can save by your own mean. call when io.WantSaveIniSettings is set, then save data by your own mean and clear io.WantSaveIniSettings.
    size_t out_ini_size           =  0;
    const char * ret_char         = ImGui::SaveIniSettingsToMemory(&out_ini_size);
    lua_pushstring(lua,ret_char);
    return 1;
}

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

int onGetContentRegionAvailWidthImGuiLua(lua_State *lua)
{
    const float ret_float  = ImGui::GetContentRegionAvail().x;
    lua_pushnumber(lua,ret_float);
    return 1;
}

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
    ImGuiInputTextFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiInputTextFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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
    ImGuiInputTextFlags flags      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  0;
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

void lua_create_metatable_identifier(lua_State *lua,const char* _metatable_plugin,const int value)
{
    luaL_newmetatable(lua, _metatable_plugin);
    lua_pushinteger(lua,value);
    lua_rawseti(lua,-2,1);
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


/*int onPushClipRectImDrawListLua(lua_State *lua)
{
    //  Render-level scissoring. This is passed down to your render function but not used for CPU-side coarse clipping. Prefer using higher-level ImGui::PushClipRect() to affect logic (hit-testing and widget culling)
    int index_input                              = 1;
    const int top                                = lua_gettop(lua);
    const ImVec2 clip_rect_min                   = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 clip_rect_max                   = lua_pop_ImVec2(lua, index_input++);
    const bool intersect_with_current_clip_rect  = top >= index_input ? lua_toboolean(lua,index_input++) :  false;
    ImDrawList* draw_list                        = GetImDrawListLua();
    draw_list->PushClipRect(clip_rect_min,clip_rect_max,intersect_with_current_clip_rect);
    return 0;
}
    
int onPushClipRectFullScreenImDrawListLua(lua_State *lua)
{
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->PushClipRectFullScreen();
    return 0;
}
    
int onPopClipRectImDrawListLua(lua_State *lua)
{
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->PopClipRect();
    return 0;
}*/
    
int onPushTextureIDImDrawListLua(lua_State *lua)
{
    int index_input                     = 1;
    ImTextureID user_texture_id         = reinterpret_cast<ImTextureID>(0);
    if(lua_type(lua,index_input) == LUA_TNUMBER)
        user_texture_id                 = reinterpret_cast<ImTextureID>(lua_tointeger(lua,index_input++));
    else
        user_texture_id                 = reinterpret_cast<ImTextureID>(get_texture_id(lua,luaL_checkstring(lua,index_input++)));
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->PushTextureID(user_texture_id);
    return 0;
}
    
int onPopTextureIDImDrawListLua(lua_State *lua)
{
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->PopTextureID();
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
    ImDrawCornerFlags rounding_corners      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  ImDrawCornerFlags_All;
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
    ImDrawCornerFlags   rounding_corners    = top >= index_input ? luaL_checkinteger(lua, index_input++) :  ImDrawCornerFlags_All;
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
    
int onAddBezierCurveImDrawListLua(lua_State *lua)
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
    draw_list->AddBezierCurve(p1,p2,p3,p4,ImGui::GetColorU32(color),thickness,num_segments);
    return 0;
}
    
int onAddImageImDrawListLua(lua_State *lua)
{
    int index_input                 = 1;
    const int top                   = lua_gettop(lua);
    ImTextureID user_texture_id     = reinterpret_cast<ImTextureID>(0);
    if(lua_type(lua,index_input) == LUA_TNUMBER)
        user_texture_id             = reinterpret_cast<ImTextureID>(lua_tointeger(lua,index_input++));
    else
        user_texture_id             = reinterpret_cast<ImTextureID>(get_texture_id(lua,luaL_checkstring(lua,index_input++)));
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
    ImTextureID user_texture_id     = reinterpret_cast<ImTextureID>(0);
    if(lua_type(lua,index_input) == LUA_TNUMBER)
        user_texture_id             = reinterpret_cast<ImTextureID>(lua_tointeger(lua,index_input++));
    else
        user_texture_id             = reinterpret_cast<ImTextureID>(get_texture_id(lua,luaL_checkstring(lua,index_input++)));
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
    
int onAddImageRoundedImDrawListLua(lua_State *lua)
{
    int index_input                         = 1;
    const int top                           = lua_gettop(lua);
    ImTextureID user_texture_id             = reinterpret_cast<ImTextureID>(0);
    if(lua_type(lua,index_input) == LUA_TNUMBER)
        user_texture_id                     = reinterpret_cast<ImTextureID>(lua_tointeger(lua,index_input++));
    else
        user_texture_id                     = reinterpret_cast<ImTextureID>(get_texture_id(lua,luaL_checkstring(lua,index_input++)));
    const ImVec2 p_min                      = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p_max                      = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color                       = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++));
    const ImVec2 uv_min                     = top >= index_input ? lua_pop_ImVec2(lua, index_input++) :  ImVec2(0, 0);
    const ImVec2 uv_max                     = top >= index_input ? lua_pop_ImVec2(lua, index_input++) :  ImVec2(1, 1);
    const float rounding                    = top >= index_input ? luaL_checknumber(lua,index_input++) : 0.0f;
    ImDrawCornerFlags rounding_corners      = top >= index_input ? luaL_checkinteger(lua, index_input++) :  ImDrawCornerFlags_All;
    ImDrawList* draw_list                   = GetImDrawListLua();
    draw_list->AddImageRounded(user_texture_id,p_min,p_max,uv_min,uv_max,color,rounding,rounding_corners);
    return 0;
}
 /*   
int onPathClearImDrawListLua(lua_State *lua)
{
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->PathClear();
    return 0;
}
    
int onPathLineToImDrawListLua(lua_State *lua)
{
    const ImVec2 pos      = lua_pop_ImVec2(lua, 1);
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->PathLineTo(pos);
    return 0;
}
    
int onPathLineToMergeDuplicateImDrawListLua(lua_State *lua)
{
    const ImVec2 pos      = lua_pop_ImVec2(lua, 1);
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->PathLineToMergeDuplicate(pos);
    return 0;
}
    
int onPathFillConvexImDrawListLua(lua_State *lua)
{
    //  Note: Anti-aliased filling requires points to be in clockwise order.
    const ImU32 color     = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,1));
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->PathFillConvex(color);
    return 0;
}
    
int onPathStrokeImDrawListLua(lua_State *lua)
{
    int index_input        = 1;
    const int top          = lua_gettop(lua);
    const ImU32 color      = ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,1));
    const bool closed      = lua_toboolean(lua,index_input++);
    const float thickness  = top >= index_input ? luaL_checknumber(lua,index_input++) :  1.0f;
    ImDrawList* draw_list  = GetImDrawListLua();
    draw_list->PathStroke(color,closed,thickness);
    return 0;
}
    
int onPathArcToImDrawListLua(lua_State *lua)
{
    int index_input            = 1;
    const int top              = lua_gettop(lua);
    const ImVec2 center        = lua_pop_ImVec2(lua, index_input++);
    const float radius         = luaL_checknumber(lua,index_input++);
    const float a_min          = luaL_checknumber(lua,index_input++);
    const float a_max          = luaL_checknumber(lua,index_input++);
    const int num_segments     = top >= index_input ? luaL_checkinteger(lua,index_input++) :  10;
    ImDrawList* draw_list      = GetImDrawListLua();
    draw_list->PathArcTo(center,radius,a_min,a_max,num_segments);
    return 0;
}
    
int onPathArcToFastImDrawListLua(lua_State *lua)
{
    //  Use precomputed angles for a 12 steps circle
    int index_input              = 1;
    const ImVec2 center          = lua_pop_ImVec2(lua, index_input++);
    const float radius           = luaL_checknumber(lua,index_input++);
    const int a_min_of_12        = luaL_checkinteger(lua,index_input++);
    const int a_max_of_12        = luaL_checkinteger(lua,index_input++);
    ImDrawList* draw_list        = GetImDrawListLua();
    draw_list->PathArcToFast(center,radius,a_min_of_12,a_max_of_12);
    return 0;
}
    
int onPathBezierCurveToImDrawListLua(lua_State *lua)
{
    int index_input         = 1;
    const int top           = lua_gettop(lua);
    const ImVec2 p2         = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p3         = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 p4         = lua_pop_ImVec2(lua, index_input++);
    const int num_segments  = top >= index_input ? luaL_checkinteger(lua,index_input++) :  0;
    ImDrawList* draw_list   = GetImDrawListLua();
    draw_list->PathBezierCurveTo(p2,p3,p4,num_segments);
    return 0;
}
    
int onPathRectImDrawListLua(lua_State *lua)
{
    int index_input                         = 1;
    const int top                           = lua_gettop(lua);
    const ImVec2 rect_min                   = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 rect_max                   = lua_pop_ImVec2(lua, index_input++);
    const float rounding                    = top >= index_input ? luaL_checknumber(lua,index_input++) :  0.0f;
    ImDrawCornerFlags   rounding_corners    = top >= index_input ? luaL_checkinteger(lua, index_input++) :  ImDrawCornerFlags_All;
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->PathRect(rect_min,rect_max,rounding,rounding_corners);
    return 0;
}
    */
/*int onAddCallbackImDrawListLua(lua_State *lua)
{
    //  Your rendering function must check for 'UserCallback' in ImDrawCmd and call the function instead of rendering triangles.
    int index_input                                                           = 1;
    #error "8 - do not know what to do for this: type:nil variable:var_nil_1  = nil"
    static void var_void_2                                                    = 0;
    void * p_callback_data                                                    = nullptr;
    if(top >= index_input)
    {
        var_void_2                                                            = ;
        p_callback_data                                                       = &var_void_2;
    }
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->AddCallback(var_nil_1,p_callback_data);
    return 0;
}*/
    
int onAddDrawCmdImDrawListLua(lua_State *lua)
{
    //  This is useful if you need to forcefully create a new draw call (to allow for dependent rendering / blending). Otherwise primitives are merged into the same draw-call as much as possible
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->AddDrawCmd();
    return 0;
}
    
/*int onCloneOutputImDrawListLua(lua_State *lua)
{
    //  Create a clone of the CmdBuffer/IdxBuffer/VtxBuffer.
    int index_input                                                           = 1;
    #error "8 - do not know what to do for this: type:nil variable:var_nil_3  = nil"
    const ImDrawList ret_ImDrawList                                           = CloneOutput(var_nil_3);
    lua_push_ImDrawList(lua,ret_ImDrawList);
    return 1;
}*/
    /*
int onChannelsSplitImDrawListLua(lua_State *lua)
{
    const int count       = luaL_checkinteger(lua,1);
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->ChannelsSplit(count);
    return 0;
}
    
int onChannelsMergeImDrawListLua(lua_State *lua)
{
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->ChannelsMerge();
    return 0;
}
    
int onChannelsSetCurrentImDrawListLua(lua_State *lua)
{
    const int n           = luaL_checkinteger(lua,1);
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->ChannelsSetCurrent(n);
    return 0;
}*/
    
/*int onClearImDrawListLua(lua_State *lua)
{
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->
    return 0;
}*/
    
/*int onClearFreeMemoryImDrawListLua(lua_State *lua)
{
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->ClearFreeMemory();
    return 0;
}
    
int onPrimReserveImDrawListLua(lua_State *lua)
{
    int index_input       = 1;
    const int idx_count   = luaL_checkinteger(lua,index_input++);
    const int vtx_count   = luaL_checkinteger(lua,index_input++);
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->PrimReserve(idx_count,vtx_count);
    return 0;
}
    
int onPrimUnreserveImDrawListLua(lua_State *lua)
{
    int index_input       = 1;
    const int idx_count   = luaL_checkinteger(lua,index_input++);
    const int vtx_count   = luaL_checkinteger(lua,index_input++);
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->PrimUnreserve(idx_count,vtx_count);
    return 0;
}
    
int onPrimRectImDrawListLua(lua_State *lua)
{
    //  Axis aligned rectangle (composed of two triangles)
    int index_input       = 1;
    const int top         = lua_gettop(lua);
    const ImVec2 a        = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 b        = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color     = top >= index_input ? ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++))  :  IM_COL32_WHITE;
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->PrimRect(a,b,color);
    return 0;
}
    
int onPrimRectUVImDrawListLua(lua_State *lua)
{
    int index_input           = 1;
    const int top             = lua_gettop(lua);
    const ImVec2 a            = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 b            = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 uv_a         = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 uv_b         = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color         = top >= index_input ? ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++))  :  IM_COL32_WHITE;
    ImDrawList* draw_list     = GetImDrawListLua();
    draw_list->PrimRectUV(a,b,uv_a,uv_b,color);
    return 0;
}
    
int onPrimQuadUVImDrawListLua(lua_State *lua)
{
    int index_input           = 1;
    const int top             = lua_gettop(lua);
    const ImVec2 a            = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 b            = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 c            = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 d            = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 uv_a         = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 uv_b         = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 uv_c         = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 uv_d         = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color         = top >= index_input ? ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++))  :  IM_COL32_WHITE;
    ImDrawList* draw_list     = GetImDrawListLua();
    draw_list->PrimQuadUV(a,b,c,d,uv_a,uv_b,uv_c,uv_d,color);
    return 0;
}
    
int onPrimWriteVtxImDrawListLua(lua_State *lua)
{
    int index_input           = 1;
    const int top             = lua_gettop(lua);
    const ImVec2 pos          = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 uv           = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color         = top >= index_input ? ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++))  :  IM_COL32_WHITE;
    ImDrawList* draw_list     = GetImDrawListLua();
    draw_list->PrimWriteVtx(pos,uv,color);
    return 0;
}
    
int onPrimWriteIdxImDrawListLua(lua_State *lua)
{
    const int idx         = luaL_checkinteger(lua,1);
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->PrimWriteIdx(idx);
    return 0;
}
    
int onPrimVtxImDrawListLua(lua_State *lua)
{
    int index_input           = 1;
    const int top             = lua_gettop(lua);
    const ImVec2 pos          = lua_pop_ImVec2(lua, index_input++);
    const ImVec2 uv           = lua_pop_ImVec2(lua, index_input++);
    const ImU32 color         = top >= index_input ? ImGui::GetColorU32(lua_get_rgba_to_ImVec4_fromTable(lua,index_input++))  :  IM_COL32_WHITE;
    ImDrawList* draw_list     = GetImDrawListLua();
    draw_list->PrimVtx(pos,uv,color);
    return 0;
}
    
int onUpdateClipRectImDrawListLua(lua_State *lua)
{
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->UpdateClipRect();
    return 0;
}
    
int onUpdateTextureIDImDrawListLua(lua_State *lua)
{
    ImDrawList* draw_list = GetImDrawListLua();
    draw_list->UpdateTextureID();
    return 0;
}*/
int onNewimguiLua(lua_State *lua)
{
    lua_settop(lua, 0);
    luaL_Reg regimguiMethods[]  = {  
        {"AcceptDragDropPayload",                       onAcceptDragDropPayloadImGuiLua },
        {"AlignTextToFramePadding",                   onAlignTextToFramePaddingImGuiLua },
        {"ArrowButton",                                           onArrowButtonImGuiLua },
        {"Begin",                                                       onBeginImGuiLua },
        {"BeginChild",                                             onBeginChildImGuiLua },
        {"BeginChildFrame",                                   onBeginChildFrameImGuiLua },
        {"BeginCombo",                                             onBeginComboImGuiLua },
        {"BeginDragDropSource",                           onBeginDragDropSourceImGuiLua },
        {"BeginDragDropTarget",                           onBeginDragDropTargetImGuiLua },
        {"BeginGroup",                                             onBeginGroupImGuiLua },
        {"BeginMainMenuBar",                                 onBeginMainMenuBarImGuiLua },
        {"BeginMenu",                                               onBeginMenuImGuiLua },
        {"BeginMenuBar",                                         onBeginMenuBarImGuiLua },
        {"BeginPopup",                                             onBeginPopupImGuiLua },
        {"BeginPopupContextItem",                       onBeginPopupContextItemImGuiLua },
        {"BeginPopupContextVoid",                       onBeginPopupContextVoidImGuiLua },
        {"BeginPopupContextWindow",                   onBeginPopupContextWindowImGuiLua },
        {"BeginPopupModal",                                   onBeginPopupModalImGuiLua },
        {"BeginTabBar",                                           onBeginTabBarImGuiLua },
        {"BeginTabItem",                                         onBeginTabItemImGuiLua },
        {"BeginTooltip",                                         onBeginTooltipImGuiLua },
        {"Bullet",                                                     onBulletImGuiLua },
        {"BulletText",                                             onBulletTextImGuiLua },
        {"Button",                                                     onButtonImGuiLua },
        {"CalcItemWidth",                                       onCalcItemWidthImGuiLua },
        {"CalcListClipping",                                 onCalcListClippingImGuiLua },
        {"CalcTextSize",                                         onCalcTextSizeImGuiLua },
        {"CaptureKeyboardFromApp",                     onCaptureKeyboardFromAppImGuiLua },
        {"CaptureMouseFromApp",                           onCaptureMouseFromAppImGuiLua },
        {"Checkbox",                                                 onCheckboxImGuiLua },
        {"CheckboxFlags",                                       onCheckboxFlagsImGuiLua },
        {"CloseCurrentPopup",                               onCloseCurrentPopupImGuiLua },
        {"CollapsingHeader",                                 onCollapsingHeaderImGuiLua },
        {"ColorButton",                                           onColorButtonImGuiLua },
        {"ColorConvertFloat4ToU32",                   onColorConvertFloat4ToU32ImGuiLua },
        {"ColorConvertHSVtoRGB",                         onColorConvertHSVtoRGBImGuiLua },
        {"ColorConvertRGBtoHSV",                         onColorConvertRGBtoHSVImGuiLua },
        {"ColorConvertU32ToFloat4",                   onColorConvertU32ToFloat4ImGuiLua },
        {"ColorEdit3",                                             onColorEdit3ImGuiLua },
        {"ColorEdit4",                                             onColorEdit4ImGuiLua },
        {"ColorPicker3",                                         onColorPicker3ImGuiLua },
        {"ColorPicker4",                                         onColorPicker4ImGuiLua },
        {"Columns",                                                   onColumnsImGuiLua },
        {"Combo",                                                       onComboImGuiLua },
        {"DragFloat",                                               onDragFloatImGuiLua },
        {"DragFloat2",                                             onDragFloat2ImGuiLua },
        {"DragFloat3",                                             onDragFloat3ImGuiLua },
        {"DragFloat4",                                             onDragFloat4ImGuiLua },
        {"DragFloatRange2",                                   onDragFloatRange2ImGuiLua },
        {"DragInt",                                                   onDragIntImGuiLua },
        {"DragInt2",                                                 onDragInt2ImGuiLua },
        {"DragInt3",                                                 onDragInt3ImGuiLua },
        {"DragInt4",                                                 onDragInt4ImGuiLua },
        {"DragIntRange2",                                       onDragIntRange2ImGuiLua },
        {"Dummy",                                                       onDummyImGuiLua },
        {"End",                                                           onEndImGuiLua },
        {"EndChild",                                                 onEndChildImGuiLua },
        {"EndChildFrame",                                       onEndChildFrameImGuiLua },
        {"EndCombo",                                                 onEndComboImGuiLua },
        {"EndDragDropSource",                               onEndDragDropSourceImGuiLua },
        {"EndDragDropTarget",                               onEndDragDropTargetImGuiLua },
        {"EndGroup",                                                 onEndGroupImGuiLua },
        {"EndMainMenuBar",                                     onEndMainMenuBarImGuiLua },
        {"EndMenu",                                                   onEndMenuImGuiLua },
        {"EndMenuBar",                                             onEndMenuBarImGuiLua },
        {"EndPopup",                                                 onEndPopupImGuiLua },
        {"EndTabBar",                                               onEndTabBarImGuiLua },
        {"EndTabItem",                                             onEndTabItemImGuiLua },
        {"EndTooltip",                                             onEndTooltipImGuiLua },
        {"Flags",                                                  onMakeFlagsImGuiLua  },
        {"FlagList",                                               onListFlagsImGuiLua  },
        {"GetClipboardText",                                 onGetClipboardTextImGuiLua },
        {"GetColorU32",                                           onGetColorU32ImGuiLua },
        {"GetColumnIndex",                                     onGetColumnIndexImGuiLua },
        {"GetColumnOffset",                                   onGetColumnOffsetImGuiLua },
        {"GetColumnWidth",                                     onGetColumnWidthImGuiLua },
        {"GetColumnsCount",                                   onGetColumnsCountImGuiLua },
        {"GetContentRegionAvail",                       onGetContentRegionAvailImGuiLua },
        {"GetContentRegionAvailWidth",             onGetContentRegionAvailWidthImGuiLua },
        {"GetContentRegionMax",                           onGetContentRegionMaxImGuiLua },
        {"GetCursorPos",                                         onGetCursorPosImGuiLua },
        {"GetCursorPosX",                                       onGetCursorPosXImGuiLua },
        {"GetCursorPosY",                                       onGetCursorPosYImGuiLua },
        {"GetCursorScreenPos",                             onGetCursorScreenPosImGuiLua },
        {"GetCursorStartPos",                               onGetCursorStartPosImGuiLua },
        {"GetDragDropPayload",                             onGetDragDropPayloadImGuiLua },
        {"GetFont",                                                   onGetFontImGuiLua },
        {"GetFontSize",                                           onGetFontSizeImGuiLua },
        {"GetFontTexUvWhitePixel",                     onGetFontTexUvWhitePixelImGuiLua },
        {"GetFrameCount",                                       onGetFrameCountImGuiLua },
        {"GetFrameHeight",                                     onGetFrameHeightImGuiLua },
        {"GetFrameHeightWithSpacing",               onGetFrameHeightWithSpacingImGuiLua },
        {"GetID",                                                       onGetIDImGuiLua },
        {"GetItemRectMax",                                     onGetItemRectMaxImGuiLua },
        {"GetItemRectMin",                                     onGetItemRectMinImGuiLua },
        {"GetItemRectSize",                                   onGetItemRectSizeImGuiLua },
        {"GetKeyIndex",                                           onGetKeyIndexImGuiLua },
        {"GetKeyPressedAmount",                           onGetKeyPressedAmountImGuiLua },
        {"GetMainMenuBarHeight",                         onGetMainMenuBarHeightImGuiLua },
        {"GetMouseCursor",                                     onGetMouseCursorImGuiLua },
        {"GetMouseDragDelta",                               onGetMouseDragDeltaImGuiLua },
        {"GetMousePos",                                           onGetMousePosImGuiLua },
        {"GetMousePosOnOpeningCurrentPopup", onGetMousePosOnOpeningCurrentPopupImGuiLua },
        {"GetScrollMaxX",                                       onGetScrollMaxXImGuiLua },
        {"GetScrollMaxY",                                       onGetScrollMaxYImGuiLua },
        {"GetScrollX",                                             onGetScrollXImGuiLua },
        {"GetScrollY",                                             onGetScrollYImGuiLua },
        {"GetStyle",                                                 onGetStyleImGuiLua },
        {"GetStyleColorName",                               onGetStyleColorNameImGuiLua },
        {"GetStyleColorVec4",                               onGetStyleColorVec4ImGuiLua },
        {"GetTextLineHeight",                               onGetTextLineHeightImGuiLua },
        {"GetTextLineHeightWithSpacing",         onGetTextLineHeightWithSpacingImGuiLua },
        {"GetTime",                                                   onGetTimeImGuiLua },
        {"GetTreeNodeToLabelSpacing",               onGetTreeNodeToLabelSpacingImGuiLua },
        {"GetVersion",                                             onGetVersionImGuiLua },
        {"GetWindowContentRegionMax",               onGetWindowContentRegionMaxImGuiLua },
        {"GetWindowContentRegionMin",               onGetWindowContentRegionMinImGuiLua },
        {"GetWindowContentRegionWidth",           onGetWindowContentRegionWidthImGuiLua },
        {"GetWindowHeight",                                   onGetWindowHeightImGuiLua },
        {"GetWindowPos",                                         onGetWindowPosImGuiLua },
        {"GetWindowSize",                                       onGetWindowSizeImGuiLua },
        {"GetWindowWidth",                                     onGetWindowWidthImGuiLua },
        {"GetZoom",                                                   onGetZoomImGuiLua },
        {"HelpMarker",                                                  onHelpMarkerLua },
        {"Image",                                                       onImageImGuiLua },
        {"ImageQuad",                                               onImageQuadImGuiLua },
        {"ImageButton",                                           onImageButtonImGuiLua },
        {"Indent",                                                     onIndentImGuiLua },
        {"InputDouble",                                           onInputDoubleImGuiLua },
        {"InputFloat",                                             onInputFloatImGuiLua },
        {"InputFloat2",                                           onInputFloat2ImGuiLua },
        {"InputFloat3",                                           onInputFloat3ImGuiLua },
        {"InputFloat4",                                           onInputFloat4ImGuiLua },
        {"InputInt",                                                 onInputIntImGuiLua },
        {"InputInt2",                                               onInputInt2ImGuiLua },
        {"InputInt3",                                               onInputInt3ImGuiLua },
        {"InputInt4",                                               onInputInt4ImGuiLua },
        {"InputText",                                               onInputTextImGuiLua },
        {"InputTextMultiline",                             onInputTextMultilineImGuiLua },
        {"InputTextWithHint",                               onInputTextWithHintImGuiLua },
        {"InvisibleButton",                                   onInvisibleButtonImGuiLua },
        {"IsAnyItemActive",                                   onIsAnyItemActiveImGuiLua },
        {"IsAnyItemFocused",                                 onIsAnyItemFocusedImGuiLua },
        {"IsAnyItemHovered",                                 onIsAnyItemHoveredImGuiLua },
        {"IsAnyMouseDown",                                     onIsAnyMouseDownImGuiLua },
        {"IsAnyWindowFocused",                             onIsAnyWindowFocusedImGuiLua },
        {"IsAnyWindowHovered",                             onIsAnyWindowHoveredImGuiLua },
        {"IsItemActivated",                                   onIsItemActivatedImGuiLua },
        {"IsItemActive",                                         onIsItemActiveImGuiLua },
        {"IsItemClicked",                                       onIsItemClickedImGuiLua },
        {"IsItemDeactivated",                               onIsItemDeactivatedImGuiLua },
        {"IsItemDeactivatedAfterChange",         onIsItemDeactivatedAfterChangeImGuiLua },
        {"IsItemDeactivatedAfterEdit",             onIsItemDeactivatedAfterEditImGuiLua },
        {"IsItemEdited",                                         onIsItemEditedImGuiLua },
        {"IsItemFocused",                                       onIsItemFocusedImGuiLua },
        {"IsItemHovered",                                       onIsItemHoveredImGuiLua },
        {"IsItemToggledOpen",                               onIsItemToggledOpenImGuiLua },
        {"IsItemVisible",                                       onIsItemVisibleImGuiLua },
        {"IsKeyDown",                                               onIsKeyDownImGuiLua },
        {"IsKeyPressed",                                         onIsKeyPressedImGuiLua },
        {"IsKeyReleased",                                       onIsKeyReleasedImGuiLua },
        {"IsMouseClicked",                                     onIsMouseClickedImGuiLua },
        {"IsMouseDoubleClicked",                         onIsMouseDoubleClickedImGuiLua },
        {"IsMouseDown",                                           onIsMouseDownImGuiLua },
        {"IsMouseDragging",                                   onIsMouseDraggingImGuiLua },
        {"IsMouseHoveringRect",                           onIsMouseHoveringRectImGuiLua },
        {"IsMousePosValid",                                   onIsMousePosValidImGuiLua },
        {"IsMouseReleased",                                   onIsMouseReleasedImGuiLua },
        {"IsPopupOpen",                                           onIsPopupOpenImGuiLua },
        {"IsRectVisible",                                       onIsRectVisibleImGuiLua },
        {"IsScrollVisible",                                   onIsScrollVisibleImGuiLua },
        {"IsWindowAppearing",                               onIsWindowAppearingImGuiLua },
        {"IsWindowCollapsed",                               onIsWindowCollapsedImGuiLua },
        {"IsWindowFocused",                                   onIsWindowFocusedImGuiLua },
        {"IsWindowHovered",                                   onIsWindowHoveredImGuiLua },
        {"LabelText",                                               onLabelTextImGuiLua },
        {"ListBox",                                                   onListBoxImGuiLua },
        {"ListBoxFooter",                                       onListBoxFooterImGuiLua },
        {"ListBoxHeader",                                       onListBoxHeaderImGuiLua },
        {"LoadIniSettingsFromDisk",                   onLoadIniSettingsFromDiskImGuiLua },
        {"LoadIniSettingsFromMemory",               onLoadIniSettingsFromMemoryImGuiLua },
        {"LogButtons",                                             onLogButtonsImGuiLua },
        {"LogFinish",                                               onLogFinishImGuiLua },
        {"LogText",                                                   onLogTextImGuiLua },
        {"LogToClipboard",                                     onLogToClipboardImGuiLua },
        {"LogToFile",                                               onLogToFileImGuiLua },
        {"LogToTTY",                                                 onLogToTTYImGuiLua },
        {"MenuItem",                                                 onMenuItemImGuiLua },
        {"NewLine",                                                   onNewLineImGuiLua },
        {"NextColumn",                                             onNextColumnImGuiLua },
        {"OpenPopup",                                               onOpenPopupImGuiLua },
        {"OpenPopupOnItemClick",                         onOpenPopupOnItemClickImGuiLua },
        {"PlotHistogram",                                       onPlotHistogramImGuiLua },
        {"PlotLines",                                               onPlotLinesImGuiLua },
        {"PopAllowKeyboardFocus",                       onPopAllowKeyboardFocusImGuiLua },
        {"PopButtonRepeat",                                   onPopButtonRepeatImGuiLua },
        {"PopClipRect",                                           onPopClipRectImGuiLua },
        {"PopFont",                                                   onPopFontImGuiLua },
        {"PopID",                                                       onPopIDImGuiLua },
        {"PopItemWidth",                                         onPopItemWidthImGuiLua },
        {"PopStyleColor",                                       onPopStyleColorImGuiLua },
        {"PopStyleVar",                                           onPopStyleVarImGuiLua },
        {"PopTextWrapPos",                                     onPopTextWrapPosImGuiLua },
        {"ProgressBar",                                           onProgressBarImGuiLua },
        {"PushAllowKeyboardFocus",                     onPushAllowKeyboardFocusImGuiLua },
        {"PushButtonRepeat",                                 onPushButtonRepeatImGuiLua },
        {"PushClipRect",                                         onPushClipRectImGuiLua },
        {"PushFont",                                                 onPushFontImGuiLua },
        {"PushID",                                                     onPushIDImGuiLua },
        {"PushItemWidth",                                       onPushItemWidthImGuiLua },
        {"PushStyleColor",                                     onPushStyleColorImGuiLua },
        {"PushStyleVar",                                         onPushStyleVarImGuiLua },
        {"PushTextWrapPos",                                   onPushTextWrapPosImGuiLua },
        {"RadioButton",                                           onRadioButtonImGuiLua },
        {"ResetMouseDragDelta",                           onResetMouseDragDeltaImGuiLua },
        {"SameLine",                                                 onSameLineImGuiLua },
        {"SaveIniSettingsToDisk",                       onSaveIniSettingsToDiskImGuiLua },
        {"SaveIniSettingsToMemory",                   onSaveIniSettingsToMemoryImGuiLua },
        {"Selectable",                                             onSelectableImGuiLua },
        {"Separator",                                               onSeparatorImGuiLua },
        {"SetClipboardText",                                 onSetClipboardTextImGuiLua },
        {"SetColorEditOptions",                           onSetColorEditOptionsImGuiLua },
        {"SetColumnOffset",                                   onSetColumnOffsetImGuiLua },
        {"SetColumnWidth",                                     onSetColumnWidthImGuiLua },
        {"SetCursorPos",                                         onSetCursorPosImGuiLua },
        {"SetCursorPosX",                                       onSetCursorPosXImGuiLua },
        {"SetCursorPosY",                                       onSetCursorPosYImGuiLua },
        {"SetCursorScreenPos",                             onSetCursorScreenPosImGuiLua },
        {"SetDragDropPayload",                             onSetDragDropPayloadImGuiLua },
        {"SetItemAllowOverlap",                           onSetItemAllowOverlapImGuiLua },
        {"SetItemDefaultFocus",                           onSetItemDefaultFocusImGuiLua },
        {"SetKeyboardFocusHere",                         onSetKeyboardFocusHereImGuiLua },
        {"SetMouseCursor",                                     onSetMouseCursorImGuiLua },
        {"SetNextItemOpen",                                   onSetNextItemOpenImGuiLua },
        {"SetNextItemWidth",                                 onSetNextItemWidthImGuiLua },
        {"SetNextTreeNodeOpen",                           onSetNextTreeNodeOpenImGuiLua },
        {"SetNextWindowBgAlpha",                         onSetNextWindowBgAlphaImGuiLua },
        {"SetNextWindowCollapsed",                     onSetNextWindowCollapsedImGuiLua },
        {"SetNextWindowContentSize",                 onSetNextWindowContentSizeImGuiLua },
        {"SetNextWindowFocus",                             onSetNextWindowFocusImGuiLua },
        {"SetNextWindowPos",                                 onSetNextWindowPosImGuiLua },
        {"SetNextWindowSize",                               onSetNextWindowSizeImGuiLua },
        {"SetNextWindowSizeConstraints",         onSetNextWindowSizeConstraintsImGuiLua },
        {"SetScrollFromPosX",                               onSetScrollFromPosXImGuiLua },
        {"SetScrollFromPosY",                               onSetScrollFromPosYImGuiLua },
        {"SetScrollHere",                                       onSetScrollHereImGuiLua },
        {"SetScrollHereX",                                     onSetScrollHereXImGuiLua },
        {"SetScrollHereY",                                     onSetScrollHereYImGuiLua },
        {"SetScrollX",                                             onSetScrollXImGuiLua },
        {"SetScrollY",                                             onSetScrollYImGuiLua },
        {"SetTabItemClosed",                                 onSetTabItemClosedImGuiLua },
        {"SetTooltip",                                             onSetTooltipImGuiLua },
        {"SetWindowCollapsed",                             onSetWindowCollapsedImGuiLua },
        {"SetWindowFocus",                                     onSetWindowFocusImGuiLua },
        {"SetWindowFontScale",                             onSetWindowFontScaleImGuiLua },
        {"SetWindowPos",                                         onSetWindowPosImGuiLua },
        {"SetWindowSize",                                       onSetWindowSizeImGuiLua },
#if !defined (ANDROID)
        {"ShowDemoWindow",                                     onShowDemoWindowImGuiLua },
        {"ShowFontSelector",                                 onShowFontSelectorImGuiLua },
        {"ShowStyleSelector",                               onShowStyleSelectorImGuiLua },
        {"ShowUserGuide",                                       onShowUserGuideImGuiLua },
#endif
        {"SliderAngle",                                           onSliderAngleImGuiLua },
        {"SliderFloat",                                           onSliderFloatImGuiLua },
        {"SliderFloat2",                                         onSliderFloat2ImGuiLua },
        {"SliderFloat3",                                         onSliderFloat3ImGuiLua },
        {"SliderFloat4",                                         onSliderFloat4ImGuiLua },
        {"SliderInt",                                               onSliderIntImGuiLua },
        {"SliderInt2",                                             onSliderInt2ImGuiLua },
        {"SliderInt3",                                             onSliderInt3ImGuiLua },
        {"SliderInt4",                                             onSliderInt4ImGuiLua },
        {"SmallButton",                                           onSmallButtonImGuiLua },
        {"Spacing",                                                   onSpacingImGuiLua },
        {"StyleColorsClassic",                             onStyleColorsClassicImGuiLua },
        {"StyleColorsDark",                                   onStyleColorsDarkImGuiLua },
        {"StyleColorsLight",                                 onStyleColorsLightImGuiLua },
        {"Text",                                                         onTextImGuiLua },
        {"TextColored",                                           onTextColoredImGuiLua },
        {"TextDisabled",                                         onTextDisabledImGuiLua },
        {"TextWrapped",                                           onTextWrappedImGuiLua },
        {"TreeAdvanceToLabelPos",                       onTreeAdvanceToLabelPosImGuiLua },
        {"TreeNode",                                                 onTreeNodeImGuiLua },
        {"TreeNodeEx",                                             onTreeNodeExImGuiLua },
        {"TreePop",                                                   onTreePopImGuiLua },
        {"TreePush",                                                 onTreePushImGuiLua },
        {"Unindent",                                                 onUnindentImGuiLua },
        {"VSliderFloat",                                         onVSliderFloatImGuiLua },
        {"VSliderInt",                                             onVSliderIntImGuiLua },
        
        //ImDrawList
        {"AddBezierCurve",                     onAddBezierCurveImDrawListLua },
        //{"AddCallback",                           onAddCallbackImDrawListLua },
        {"AddCircle",                               onAddCircleImDrawListLua },
        {"AddCircleFilled",                   onAddCircleFilledImDrawListLua },
        {"AddConvexPolyFilled",           onAddConvexPolyFilledImDrawListLua },
        {"AddDrawCmd",                             onAddDrawCmdImDrawListLua },
        {"AddImage",                                 onAddImageImDrawListLua },
        {"AddImageQuad",                         onAddImageQuadImDrawListLua },
        {"AddImageRounded",                   onAddImageRoundedImDrawListLua },
        {"AddLine",                                   onAddLineImDrawListLua },
        {"AddNgon",                                   onAddNgonImDrawListLua },
        {"AddNgonFilled",                       onAddNgonFilledImDrawListLua },
        {"AddPolyline",                           onAddPolylineImDrawListLua },
        {"AddQuad",                                   onAddQuadImDrawListLua },
        {"AddQuadFilled",                       onAddQuadFilledImDrawListLua },
        {"AddRect",                                   onAddRectImDrawListLua },
        {"AddRectFilled",                       onAddRectFilledImDrawListLua },
        {"AddRectFilledMultiColor",   onAddRectFilledMultiColorImDrawListLua },
        {"AddText",                                   onAddTextImDrawListLua },
        {"AddTriangle",                           onAddTriangleImDrawListLua },
        {"AddTriangleFilled",               onAddTriangleFilledImDrawListLua },
        //{"ChannelsMerge",                       onChannelsMergeImDrawListLua },
        //{"ChannelsSetCurrent",             onChannelsSetCurrentImDrawListLua },
        //{"ChannelsSplit",                       onChannelsSplitImDrawListLua },
        //{"Clear",                                       onClearImDrawListLua },
        //{"ClearFreeMemory",                   onClearFreeMemoryImDrawListLua },
        //{"CloneOutput",                           onCloneOutputImDrawListLua },
        //{"PathArcTo",                               onPathArcToImDrawListLua },
        //{"PathArcToFast",                       onPathArcToFastImDrawListLua },
        //{"PathBezierCurveTo",               onPathBezierCurveToImDrawListLua },
        //{"PathClear",                               onPathClearImDrawListLua },
        //{"PathFillConvex",                     onPathFillConvexImDrawListLua },
        //{"PathLineTo",                             onPathLineToImDrawListLua },
        //{"PathLineToMergeDuplicate", onPathLineToMergeDuplicateImDrawListLua },
        //{"PathRect",                                 onPathRectImDrawListLua },
        //{"PathStroke",                             onPathStrokeImDrawListLua },
        //{"PopClipRectIDraw",                      onPopClipRectImDrawListLua },
        {"PopTextureID",                         onPopTextureIDImDrawListLua },
        //{"PrimQuadUV",                             onPrimQuadUVImDrawListLua },
        //{"PrimRect",                                 onPrimRectImDrawListLua },
        //{"PrimRectUV",                             onPrimRectUVImDrawListLua },
        //{"PrimReserve",                           onPrimReserveImDrawListLua },
        //{"PrimUnreserve",                       onPrimUnreserveImDrawListLua },
        //{"PrimVtx",                                   onPrimVtxImDrawListLua },
        //{"PrimWriteIdx",                         onPrimWriteIdxImDrawListLua },
        //{"PrimWriteVtx",                         onPrimWriteVtxImDrawListLua },
        //{"PushClipRectIDraw",                    onPushClipRectImDrawListLua },
        //{"PushClipRectFullScreen",     onPushClipRectFullScreenImDrawListLua },
        {"PushTextureID",                         onPushTextureIDImDrawListLua },
        {"SetImDrawListToBackground",              onImDrawListToBackgroundLua },
        {"SetImDrawListToForeground",              onImDrawListToForegroundLua },
        //{"UpdateClipRect",                     onUpdateClipRectImDrawListLua },
        //{"UpdateTextureID",                   onUpdateTextureIDImDrawListLua },
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
        PLUGIN_IDENTIFIER  = lua_tointeger(lua,-1);//update the identifier of pluging
        lua_pop(lua,1);
    }
    else
    {
        lua_pop(lua, 1);
        lua_create_metatable_identifier(lua,"_usertype_plugin",PLUGIN_IDENTIFIER);//No, we just have to create a metatable to identify the module
    }
    lua_setmetatable(lua,-2);
    /* end plugin code*/

    lua_rawseti(lua, -2, 1);//set usedata as the first member in the table

    #ifdef  PLUGIN_CALLBACK
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
    #endif
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