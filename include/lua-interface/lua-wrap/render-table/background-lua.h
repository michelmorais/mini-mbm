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

#ifndef BACKGROUND_2_LUA_H
#define BACKGROUND_2_LUA_H

struct lua_State;

namespace mbm
{
    class BACKGROUND;
	class RENDERIZABLE;

    BACKGROUND *getBackGroundFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    int onDestroyBackGroundLua(lua_State *lua);
    int onLoadBackGroundLua(lua_State *lua);
    int onSetFront3dGroundLua(lua_State *lua);
    int onSetFront2dGroundLua(lua_State *lua);
    int onNewBackGroundLua(lua_State *lua);
	int onNewBackGroundNoGcLua(lua_State *lua,RENDERIZABLE * renderizable);
    void registerClassBackGround(lua_State *lua);

};

#endif