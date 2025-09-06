/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT);                                                                                                     |
| Copyright (C); 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                      |
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

#ifndef BOX2D_TO_LUA_H
#define BOX2D_TO_LUA_H

class b2Body;
struct b2Vec2;
struct b2Manifold;
struct b2ContactImpulse;
struct lua_State;

namespace mbm
{
    class PHYSICS_BOX2D;
    class RENDERIZABLE;
    struct SHAPE_INFO;

    enum EVENT_CONTACT_B2 : short;

    void lua_box2d_BeginContact(PHYSICS_BOX2D *box2d, SHAPE_INFO *info1, SHAPE_INFO *info2);
    void lua_box2d_EndContact(PHYSICS_BOX2D *box2d, SHAPE_INFO *info1, SHAPE_INFO *info2);
    void lua_box2d_PreSolve(PHYSICS_BOX2D *box2d, SHAPE_INFO *info1, SHAPE_INFO *info2,const b2Manifold *oldManifold);
    void lua_box2d_PostSolve(PHYSICS_BOX2D *box2d, SHAPE_INFO *info1, SHAPE_INFO *info2,const b2ContactImpulse *impulse);

    PHYSICS_BOX2D *getBox2dFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    b2Body *getBodyBox2dFromRawTable(lua_State *lua,const int rawi, const int indexTable);
    SHAPE_INFO *getShapeInfoFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    void lua_box2d_onBox2dDestroyBodyFromList(RENDERIZABLE* ptr);
    int onSetGravityBox2d(lua_State *lua);
    int onSetGravityScaleBodyBox2d(lua_State *lua);
    int onGetGravityBox2d(lua_State *lua);
    int onGetGravityScaleBodyBox2d(lua_State *lua);
    int onAddStaticBodyBox2d(lua_State *lua);
    int onAddBodyBox2d(lua_State *lua);
    int onAddDynamicBodyBox2d(lua_State *lua);
    int onAddKinematicBodyBox2d(lua_State *lua);
    int onApplyForceBodyBox2d(lua_State *lua);
    int onApplyForceToCenterBodyBox2dFromBody(lua_State *lua);
    int onSetLinearVelocityBox2d(lua_State *lua);
    int onGetLinearVelocityBox2d(lua_State *lua);
    int onSetAngularVelocityBox2d(lua_State *lua);
    int onGetAngularVelocityBox2d(lua_State *lua);
    int onGetInertiaBox2dFromBody(lua_State *lua);
    int onGetMassBox2d(lua_State *lua);
    int onGetManifoldBox2d(lua_State *lua);
    int onGetWorldManifoldBox2d(lua_State *lua);
    int onSetManifoldBox2d(lua_State *lua);
    int onGetPositionBox2d(lua_State *lua);
    int onApplyTorqueBodyBox2d(lua_State *lua);
    int onApplyLinearImpulseBodyBox2d(lua_State *lua);
    int onApplyLinearImpulseToCenterBodyBox2d(lua_State *lua);
    int onApplyAngularImpulseBodyBox2d(lua_State *lua);
    int onIsOnTheGroundBox2d(lua_State *lua);
    int onSetFrictionBox2d(lua_State *lua);
    int onSetRestitutionBox2d(lua_State *lua);
    int onSetTypeBodyBox2d(lua_State *lua);
    int onSetMassBox2d(lua_State *lua);
    int onSetDensityBox2d(lua_State *lua);
    int onInterfereBox2d(lua_State *lua);
    int onIsActiveBodyBox2d(lua_State *lua);
    int onSetAwakeBox2d(lua_State *lua);
    int onIsAwakeBox2d(lua_State *lua);
    int onSetBulletBox2d(lua_State *lua);
    int onSetEnabledBox2d(lua_State *lua);
    int onSetActiveCollisionBox2d(lua_State *lua);
    int onSetContactListenerBox2d(lua_State *lua);
    int onSetAngularDumpingBox2d(lua_State *lua);
    int onGetScaleBox2d(lua_State *lua);
    int onSetScaleBox2d(lua_State *lua);
    int onGetLocalCenterBodyBox2d(lua_State *lua);
    int onGetLocalPointBodyBox2d(lua_State *lua);
    int onGetTypeBodyBox2d(lua_State *lua);
    int onGetWorldCenterBox2d(lua_State *lua);
    int onGetWorldPointBox2d(lua_State *lua);
    int getVersionBox2d(lua_State *lua);
    int onGetWorldVectorBodyBox2d(lua_State *lua);
    bool lua_callback_box2d_onQueryAABBBox2d(PHYSICS_BOX2D *box2d, SHAPE_INFO *info1);
    int onQueryAABBBox2d(lua_State *lua);
    int onRayCastBox2d(lua_State *lua);
    int onGetJointBox2d(lua_State *lua);
    int onStopSimulateBox2d(lua_State *lua);
    int onResumeSimulateBox2d(lua_State *lua);
    int onDestroyBodyBox2d(lua_State *lua);
    int onTestPointBodyBox2d(lua_State *lua);
    int onSetFilterBox2d(lua_State *lua);
    int onSetFixedRotationBox2d(lua_State *lua);
    int onSetSleepingAllowedBox2d(lua_State *lua);
    int onCreateJointBox2d(lua_State *lua);
    int onDestroyBox2dLua(lua_State *lua);
    void registerClassBox2d(lua_State *lua);
    void lua_box2d_EventContact(PHYSICS_BOX2D *box2d, SHAPE_INFO *info1, SHAPE_INFO *info2, EVENT_CONTACT_B2 idEvent,
                                       const b2Manifold *oldManifold, const b2ContactImpulse *impulse);
    void lua_box2d_BeginContact(PHYSICS_BOX2D *box2d, SHAPE_INFO *info1, SHAPE_INFO *info2);
    void lua_box2d_EndContact(PHYSICS_BOX2D *box2d, SHAPE_INFO *info1, SHAPE_INFO *info2);
    void lua_box2d_PreSolve(PHYSICS_BOX2D *box2d, SHAPE_INFO *info1, SHAPE_INFO *info2,const b2Manifold *oldManifold);
    void lua_box2d_PostSolve(PHYSICS_BOX2D *box2d, SHAPE_INFO *info1, SHAPE_INFO *info2, const b2ContactImpulse *impulse);

};

#endif