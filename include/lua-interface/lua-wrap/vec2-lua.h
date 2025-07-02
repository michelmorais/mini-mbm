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

#ifndef VEC2_TO_LUA_H
#define VEC2_TO_LUA_H

struct lua_State;

namespace mbm
{
    struct VEC2;
    struct VEC3;
    VEC2 *getVec2FromRawTable(lua_State *lua, const int rawi, const int indexTable);
    int onSetVec2(lua_State *lua);
    int onGetVec2(lua_State *lua);
    int onLengthVec2(lua_State *lua);
    int onLerpVec2(lua_State *lua);
    int onMulVec2(lua_State *lua);
    int onAddVec2(lua_State *lua);
    int onDivVec2(lua_State *lua);
    int onSubVec2(lua_State *lua);
    int onDestroyVec2Lua(lua_State *lua);
    int onNewIndexVec2(lua_State *lua);
    int onIndexVec2(lua_State *lua);
    int onMoveVec2(lua_State *lua);
    int onDotVec2(lua_State *lua);
    int onNormalizeVec2(lua_State *lua);
    int onNewVec2Lua(lua_State *lua);
    int onNewVec2LuaNoGC(lua_State *lua, VEC2 *vec2);
    int onNewVec2LuaNoGC(lua_State *lua, VEC3 *vec3);
    int onToStringVec2(lua_State *lua);

    void registerClassVec2(lua_State *lua);
    void registerClassVec2NoGc(lua_State *lua);
};

#endif