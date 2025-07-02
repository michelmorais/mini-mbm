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

#ifndef BULLET_3D_TO_LUA_H
#define BULLET_3D_TO_LUA_H

struct lua_State;

namespace mbm
{
    class PHYSICS_BULLET;
    class RENDERIZABLE;
    struct USER_DATA_RENDER_LUA;

    PHYSICS_BULLET *getBulletFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    int onAddDynamicBodyBullet3d(lua_State *lua);
    int onAddDynamicCharacterBodyBullet3d(lua_State *lua);
    int onAddStaticBodyBullet3d(lua_State *lua);
    int onAddKinematicBodyBullet3d(lua_State *lua);
    int onSetGravityBullet3d(lua_State *lua);
    int onGetGravityBullet3d(lua_State *lua);
    int onApplyForceBodyBullet3d(lua_State *lua);
    int onApplyTorqueBodyBullet3d(lua_State *lua);
    int onApplyImpulseBodyBullet3d(lua_State *lua);
    int onSetLinearVelocityBullet3d(lua_State *lua);
    int onSetFrictionBullet3d(lua_State *lua);
    int onSetRestituitionBullet3d(lua_State *lua);
    int onSetMassBullet3d(lua_State *lua);
    int onInterfereBullet3d(lua_State *lua);
    int onSetAwakeBullet3d(lua_State *lua);
    int onIsAwakeBullet3d(lua_State *lua);
    int onSetContactListenerBullet3d(lua_State *lua);
    int onIsOnTheGroundBullet3d(lua_State *lua);
    int onRayCastBullet3d(lua_State *lua);
    int onNewBullet3dLua(lua_State *lua);
    int onDestroyBullet3dLua(lua_State *lua);
    const char* getVersionBullet();
    void registerClassBullet3d(lua_State *lua);
}
#endif