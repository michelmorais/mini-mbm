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

#ifndef VEC3_TO_LUA_H
#define VEC3_TO_LUA_H

struct lua_State;

namespace mbm
{
    struct VEC3;
    VEC3 *getVec3FromRawTable(lua_State *lua, const int rawi, const int indexTable);
    int onDotVec3(lua_State *lua);
    int onLerpVec3(lua_State *lua);
    int onCrossVec3(lua_State *lua);
    int onNormalizeVec3(lua_State *lua);
    int onLengthVec3(lua_State *lua);
    int onSetVec3(lua_State *lua);
    int onGetVec3(lua_State *lua);
    int onMulVec3(lua_State *lua);
    int onAddVec3(lua_State *lua);
    int onDivVec3(lua_State *lua);
    int onSubVec3(lua_State *lua);
    int onDestroyVec3Lua(lua_State *lua);
    int onNewIndexVec3(lua_State *lua);
    int onIndexVec3(lua_State *lua);
    int onToStringVec3(lua_State *lua);
    int onMoveVec3(lua_State *lua);
    int onNewVec3Lua(lua_State *lua);
    int onNewVec3LuaNoGC(lua_State *lua, VEC3 *vec3);

    void registerClassVec3(lua_State *lua);
    void registerClassVec3NoGc(lua_State *lua);
};

#endif