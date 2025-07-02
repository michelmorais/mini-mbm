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

#ifndef ANIMATION_MANAGER_LUA_H
#define ANIMATION_MANAGER_LUA_H

struct lua_State;

namespace mbm
{
    class ANIMATION_MANAGER;
    class RENDERIZABLE;

    ANIMATION_MANAGER *getAnimationManagerFromRawTable(lua_State *lua, const int rawi, const int indexTable,RENDERIZABLE **renderizable);
    int onGetAnimationsManagerLua(lua_State *lua);
    int onSetAnimationsManagerLua(lua_State *lua);
    int onGetIndexFrameAnimationsManagerLua(lua_State *lua);
    int onRestartAnimationsManagerLua(lua_State *lua);
    int onIsEndedAnimationsManagerLua(lua_State *lua);
    void onEndAnimationCallBackAnimationsLua(const char *fileNameAnimation, RENDERIZABLE *renderer);
    void onEndEffectCallBackAnimationsLua(const char *shaderName, RENDERIZABLE *renderizable);
    int setCallBackEndAnimationsManagerLua(lua_State *lua);
    int setCallBackEndEffectLua(lua_State *lua);
    int onSetTextureAnimationLua(lua_State *lua);
    int onSetAnimationTypeLua(lua_State *lua);
    const char *getNextNameAnim();
    int onAddAnimationsManagerLua(lua_State *lua);
	int onForceEndAnimFxRenderizable(lua_State *lua);
    int onGetTotalFrameAnimationsManagerLua(lua_State *lua);
    int onGetTotalAnimationsManagerLua(lua_State *lua);
    int onSetRenderRenderizableState(lua_State *lua);
    int onGetRenderRenderizableState(lua_State *lua);

};
#endif