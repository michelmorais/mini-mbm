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
#include <core-exports.h>

struct lua_State;

namespace mbm
{
    class FX;
    class ANIMATION;
    struct VAR_SHADER;
	class RENDERIZABLE;

    int onLoadNewShaderLua(lua_State *lua);
    int onSetPixelShaderLua(lua_State *lua);
    int onSetAllPixelShaderLua(lua_State *lua);
    int onSetPixelShaderMaxLua(lua_State *lua);
    int onSetPixelShaderMinLua(lua_State *lua);
    int onSetVertexShaderLua(lua_State *lua);
    int onSetAllVertexShaderLua(lua_State *lua);
    int onSetVertexShaderMaxLua(lua_State *lua);
    int onSetVertexShaderMinLua(lua_State *lua);
    int onGetPixelShaderLua(lua_State *lua);
    int onGetVertexShaderLua(lua_State *lua);
    int onSetBlendOperationLua(lua_State *lua);
    int onGetBlendOperationLua(lua_State *lua);
    #ifdef USE_OPENGL_ES
    void pushVarShader(lua_State *lua, std::vector<VAR_SHADER *> *lsVars);
    #else
    void pushVarShader(lua_State *lua, std::vector<VAR_SHADER *> *lsVars);
    #endif
    int onGetVarsShaderLua(lua_State *lua);
    int onGetCodeShaderLua(lua_State *lua);
    int onGetNamesShaderLua(lua_State *lua);
    int onGetTextureStage2AnimationLua(lua_State *lua);
    int onSetPixelShaderTimeLua(lua_State *lua);
    int onSetVertexShaderTimeLua(lua_State *lua);
    const int getNumberFromAnimationTypeShader(lua_State *lua,const int index);
    int errorAnimType(lua_State *lua);
    int onSetTypeAnimationPsShaderLua(lua_State *lua);
    int onSetTypeAnimationVsShaderLua(lua_State *lua);
    int onGetTypeAnimationPsShaderLua(lua_State *lua);
    int onGetTypeAnimationVsShaderLua(lua_State *lua);
    const float getDataPsVsTime(lua_State *lua,const bool pixelShader,const bool original);
    int onGetTimeAnimationVsShaderLua(lua_State *lua);
    int onGetTimeAnimationPsShaderLua(lua_State *lua);
    API_IMPL int onGetShaderTableRenderizableLuaNoGC(lua_State *lua);
};
#endif
