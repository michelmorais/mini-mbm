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

#ifndef CAMERA_2_LUA_H
#define CAMERA_2_LUA_H

#include <map>
#include <string>

struct lua_State;

namespace mbm
{
    struct DYNAMIC_VAR;

    int getVariableFromCam(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what);
    int setVariableFromCam(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what);
    int onScaleToScreen(lua_State *lua);
    int onSetPos2dCameraLua(lua_State *lua);
    int onGetPos2dCameraLua(lua_State *lua);
    int onSetPos3dCameraLua(lua_State *lua);
    int onSetUp3dCameraLua(lua_State *lua);
    int onGetPos3dCameraLua(lua_State *lua);
    int onGetFocusCameraLua(lua_State *lua);
    int onGetNormalDirectionCameraLua(lua_State *lua);
    int onSetFocusCameraLua(lua_State *lua);
    int onNewIndexCamera2d(lua_State *lua);
    int onNewIndexCamera3d(lua_State *lua);
    int onIndexCamera2d(lua_State *lua);
    int onIndexCamera3d(lua_State *lua);
    int onMoveCamera2d(lua_State *lua);
    int onMoveCamera3d(lua_State *lua);
    int onGetCamera(lua_State *lua);
    int onSetAngleOfView(lua_State *lua);
    int onSetFar(lua_State *lua);
    int onSetNear(lua_State *lua);
    void registerClassCamera(lua_State *lua);

};

#endif