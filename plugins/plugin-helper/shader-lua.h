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

#ifndef SHADER_LUA_H
#define SHADER_LUA_H

#include <vector>
#include "plugin-helper.h"

struct lua_State;

namespace mbm
{
    struct VAR_SHADER;

    extern "C" PLUGIN_HELPER_API int onLoadNewShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onSetPixelShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onSetAllPixelShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onSetPixelShaderMaxLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onSetPixelShaderMinLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onSetVertexShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onSetAllVertexShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onSetVertexShaderMaxLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onSetVertexShaderMinLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onGetPixelShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onGetVertexShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onSetBlendOperationLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onGetBlendOperationLua(lua_State *lua);
    #ifdef USE_OPENGL_ES
    extern "C" PLUGIN_HELPER_API void pushVarShader(lua_State *lua, std::vector<VAR_SHADER *> *lsVars);
    #else
    extern "C" PLUGIN_HELPER_API void pushVarShader(lua_State *lua, std::vector<VAR_SHADER *> *lsVars);
    #endif
    extern "C" PLUGIN_HELPER_API int onGetVarsShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onGetCodeShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onGetNamesShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onGetTextureStage2AnimationLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onSetPixelShaderTimeLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onSetVertexShaderTimeLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API const int getNumberFromAnimationTypeShader(lua_State *lua,const int index);
    extern "C" PLUGIN_HELPER_API int errorAnimType(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onSetTypeAnimationPsShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onSetTypeAnimationVsShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onGetTypeAnimationPsShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onGetTypeAnimationVsShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API const float getDataPsVsTime(lua_State *lua,const bool pixelShader,const bool original);
    extern "C" PLUGIN_HELPER_API int onGetTimeAnimationVsShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onGetTimeAnimationPsShaderLua(lua_State *lua);
    extern "C" PLUGIN_HELPER_API int onGetShaderTableRenderizableLuaNoGC(lua_State *lua);
};
#endif
