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

#ifndef RENDER_2_TEXTURE_LUA_H
#define RENDER_2_TEXTURE_LUA_H

struct lua_State;

namespace mbm
{
    class RENDER_2_TEXTURE;
    class CAMERA_TARGET;

    RENDER_2_TEXTURE *getRender2TextureTargetFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    CAMERA_TARGET *getCameraRender2TextureTargetFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    int onDestroyRender2Texture(lua_State *lua);
    int onCreateRender2Texture(lua_State *lua);
    int onAddRender2Texture(lua_State *lua);
    int onSetPosCameraRender2TextureLua(lua_State *lua);
    int onGetPosCameraRender2TextureLua(lua_State *lua);
    int onGetFocusCameraRender2TextureLua(lua_State *lua);
    int onSetFocusCameraRender2TextureLua(lua_State *lua);
    int onMoveCameraRender2TextureLua(lua_State *lua);
    int onSetScaleCameraRender2TextureLua(lua_State *lua);
    int onSetAngleCameraRender2TextureLua(lua_State *lua);
    int onGetScaleCameraRender2TextureLua(lua_State *lua);
    int onGetAngleCameraRender2TextureLua(lua_State *lua);
    int onGetUpCameraRender2TextureLua(lua_State *lua);
    int onSetUpCameraRender2TextureLua(lua_State *lua);
    int onSetColorBackgroundRender2TextureLua(lua_State *lua);
    int onEnableFrameRender2TextureLua(lua_State *lua);
#ifdef USE_OPENGL_ES
    int onSaveRender2Texture(lua_State *lua);
#endif
    int onGetCameraRender2Texture(lua_State *lua);
    int onNewRender2Texture(lua_State *lua);
    void registerClassRender2TextureTarget(lua_State *lua);

};

#endif