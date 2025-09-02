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

#ifndef BOX2D_LIQUID_FUN_TO_LUA_H
#define BOX2D_LIQUID_FUN_TO_LUA_H

class b2Body;
struct b2Vec2;
struct b2Manifold;
struct b2ContactImpulse;
struct lua_State;

namespace mbm
{
    class PHYSICS_BOX2D_LIQUID_FUN;
    class RENDERIZABLE;
    struct SHAPE_INFO_B2DLF;

    enum EVENT_CONTACT_B2 : short;

    void lua_box2dlf_BeginContact(PHYSICS_BOX2D_LIQUID_FUN *box2d, SHAPE_INFO_B2DLF *info1, SHAPE_INFO_B2DLF *info2);
    void lua_box2dlf_EndContact(PHYSICS_BOX2D_LIQUID_FUN *box2d, SHAPE_INFO_B2DLF *info1, SHAPE_INFO_B2DLF *info2);
    void lua_box2dlf_PreSolve(PHYSICS_BOX2D_LIQUID_FUN *box2d, SHAPE_INFO_B2DLF *info1, SHAPE_INFO_B2DLF *info2,const b2Manifold *oldManifold);
    void lua_box2dlf_PostSolve(PHYSICS_BOX2D_LIQUID_FUN *box2d, SHAPE_INFO_B2DLF *info1, SHAPE_INFO_B2DLF *info2,const b2ContactImpulse *impulse);

    PHYSICS_BOX2D_LIQUID_FUN *getBox2dlfFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    b2Body *getBodyBox2dlfFromRawTable(lua_State *lua,const int rawi, const int indexTable);
    SHAPE_INFO_B2DLF *getShapeInfobox2dlfFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    void lua_box2d_onBox2dlfDestroyBodyFromList(RENDERIZABLE* ptr);
    int onSetGravityBox2dlf(lua_State *lua);
    int onSetGravityScaleBodyBox2dlf(lua_State *lua);
    int onGetGravityBox2dlf(lua_State *lua);
    int onGetGravityScaleBodyBox2dlf(lua_State *lua);
    int onAddStaticBodyBox2dlf(lua_State *lua);
    int onAddBodyBox2dlf(lua_State *lua);
    int onAddDynamicBodyBox2dlf(lua_State *lua);
    int onAddKinematicBodyBox2dlf(lua_State *lua);
    int onApplyForceBodyBox2dlf(lua_State *lua);
    int onApplyForceToCenterBodyBox2dlfFromBody(lua_State *lua);
    int onSetLinearVelocityBox2dlf(lua_State *lua);
    int onGetLinearVelocityBox2dlf(lua_State *lua);
    int onSetAngularVelocityBox2dlf(lua_State *lua);
    int onGetAngularVelocityBox2dlf(lua_State *lua);
    int onGetInertiaBox2dFromBodylf(lua_State *lua);
    int onGetMassBox2dlf(lua_State *lua);
    int onGetManifoldBox2dlf(lua_State *lua);
    int onGetWorldManifoldBox2dlf(lua_State *lua);
    int onSetManifoldBox2dlf(lua_State *lua);
    int onGetPositionBox2dlf(lua_State *lua);
    int onApplyTorqueBodyBox2dlf(lua_State *lua);
    int onApplyLinearImpulseBodyBox2dlf(lua_State *lua);
    int onApplyLinearImpulseToCenterBodyBox2dlf(lua_State *lua);
    int onApplyAngularImpulseBodyBox2dlf(lua_State *lua);
    int onIsOnTheGroundBox2dlf(lua_State *lua);
    int onSetFrictionBox2dlf(lua_State *lua);
    int onSetRestitutionBox2dlf(lua_State *lua);
    int onSetTypeBodyBox2dlf(lua_State *lua);
    int onSetMassBox2dlf(lua_State *lua);
    int onSetDensityBox2dlf(lua_State *lua);
    int onInterfereBox2dlf(lua_State *lua);
    int onIsActiveBodyBox2dlf(lua_State *lua);
    int onSetAwakeBox2dlf(lua_State *lua);
    int onIsAwakeBox2dlf(lua_State *lua);
    int onSetBulletBox2dlf(lua_State *lua);
    int onSetEnabledBox2dlf(lua_State *lua);
    int onSetActiveCollisionBox2dlf(lua_State *lua);
    int onSetContactListenerBox2dlf(lua_State *lua);
    int onSetAngularDumpingBox2dlf(lua_State *lua);
    int onGetScaleBox2dlf(lua_State *lua);
    int onSetScaleBox2dlf(lua_State *lua);
    int onGetLocalCenterBodyBox2dlf(lua_State *lua);
    int onGetLocalPointBodyBox2dlf(lua_State *lua);
    int onGetTypeBodyBox2dlf(lua_State *lua);
    int onGetWorldCenterBox2dlf(lua_State *lua);
    int onGetWorldPointBox2dlf(lua_State *lua);
    int getVersionBox2dlf(lua_State *lua);
    int onGetWorldVectorBodyBox2dlf(lua_State *lua);
    bool lua_callback_box2d_onQueryAABBBox2dlf(PHYSICS_BOX2D_LIQUID_FUN *box2d, SHAPE_INFO_B2DLF *info1);
    int onQueryAABBBox2dlf(lua_State *lua);
    int onRayCastBox2dlf(lua_State *lua);
    int onGetJointBox2dlf(lua_State *lua);
    int onGetRenderizableFluidInterfaceBox2dlf(lua_State *lua,RENDERIZABLE * steered_particle);
    int onStopSimulateBox2dlf(lua_State *lua);
    int onResumeSimulateBox2dlf(lua_State *lua);
    int onDestroyBodyBox2dlf(lua_State *lua);
    int onTestPointBodyBox2dlf(lua_State *lua);
    int onSetFilterBox2dlf(lua_State *lua);
    int onSetFixedRotationBox2dlf(lua_State *lua);
    int onSetSleepingAllowedBox2dlf(lua_State *lua);
    int onCreateJointBox2dlf(lua_State *lua);
    int onDestroyBox2dlfLua(lua_State *lua);
    void registerClassBox2dLiquidFun(lua_State *lua);
    void lua_box2dlf_EventContact(PHYSICS_BOX2D_LIQUID_FUN *box2d, SHAPE_INFO_B2DLF *info1, SHAPE_INFO_B2DLF *info2, EVENT_CONTACT_B2 idEvent,
                                       const b2Manifold *oldManifold, const b2ContactImpulse *impulse);
    void lua_box2dlf_BeginContact(PHYSICS_BOX2D_LIQUID_FUN *box2d, SHAPE_INFO_B2DLF *info1, SHAPE_INFO_B2DLF *info2);
    void lua_box2dlf_EndContact(PHYSICS_BOX2D_LIQUID_FUN *box2d, SHAPE_INFO_B2DLF *info1, SHAPE_INFO_B2DLF *info2);
    void lua_box2dlf_PreSolve(PHYSICS_BOX2D_LIQUID_FUN *box2d, SHAPE_INFO_B2DLF *info1, SHAPE_INFO_B2DLF *info2,const b2Manifold *oldManifold);
    void lua_box2dlf_PostSolve(PHYSICS_BOX2D_LIQUID_FUN *box2d, SHAPE_INFO_B2DLF *info1, SHAPE_INFO_B2DLF *info2, const b2ContactImpulse *impulse);

};

#endif