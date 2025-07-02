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

#ifndef PHYSICS_BOX2D_WRAP_H
#define PHYSICS_BOX2D_WRAP_H

#include <vector>
#include <core_mbm/physics.h>
#include <core_mbm/primitives.h>
#include <Box2D/Box2D.h>

class b2Body;
class b2Joint;

namespace mbm
{
class RENDERIZABLE;
class STEERED_PARTICLE;
class SCENE;
struct INFO_PHYSICS;
struct VEC2;
struct VEC3;

    struct SHAPE_INFO
    {
        const b2BodyType typePhysics;
        RENDERIZABLE *   ptr;
        b2Body *         body;
        SHAPE_INFO(RENDERIZABLE *ptrMesh, const b2BodyType newType) noexcept;
        virtual ~SHAPE_INFO() noexcept;
    };

    struct INFO_FLUID
    {
        RENDERIZABLE    * steered_particle;
        b2ParticleSystem*  particleSystem;
        b2ParticleGroupDef pd;
        INFO_FLUID(const bool is3d,const bool is2dScreen,const bool segmented,const float* _scale_physics_engine) noexcept;
        virtual ~INFO_FLUID() noexcept;
    };

    class INFO_JOINT
    {
    public:
        SHAPE_INFO* infoA;
        SHAPE_INFO* infoB;
        b2Joint*    joint;
        INFO_JOINT(SHAPE_INFO*  info_a,SHAPE_INFO*  info_b,b2Joint* _joint)noexcept;
        virtual ~INFO_JOINT()noexcept;
    };

    class PHYSICS_BOX2D;
    typedef void (* On_box2d_BeginContact)(PHYSICS_BOX2D*,SHAPE_INFO* info1,SHAPE_INFO* info2);
    typedef void (* On_box2d_EndContact)(PHYSICS_BOX2D*,SHAPE_INFO* info1,SHAPE_INFO* info2);
    typedef void (* On_box2d_PreSolve)(PHYSICS_BOX2D*,SHAPE_INFO* info1,SHAPE_INFO* info2,const b2Manifold* oldManifold);
    typedef void (* On_box2d_PostSolve)(PHYSICS_BOX2D*,SHAPE_INFO* info1,SHAPE_INFO* info2,const b2ContactImpulse* impulse);
    typedef void(*On_box2d_DestroyBodyFromList)(RENDERIZABLE* ptr);


    class PHYSICS_BOX2D : public PHYSICS, public b2ContactListener
    {
    public:
        int32   velocityIterations;
        int32   positionIterations;
        float   multiplyStep;
        bool    stopSimulate;

        On_box2d_BeginContact on_box2d_BeginContact;
        On_box2d_EndContact   on_box2d_EndContact;
        On_box2d_PreSolve     on_box2d_PreSolve ;
        On_box2d_PostSolve    on_box2d_PostSolve;
        On_box2d_DestroyBodyFromList on_box2d_DestroyBodyFromList;

        PHYSICS_BOX2D(SCENE* scene)noexcept;
        virtual ~PHYSICS_BOX2D();

        void setScale(const float s)noexcept;
        float getScale() const noexcept;
        bool testPoint(SHAPE_INFO* info,const b2Vec2& point);
        bool destroyBody(SHAPE_INFO* info);
        bool undoDestroyBody(SHAPE_INFO* info);
        bool undoDestroyFluid(INFO_FLUID* info);
        void setActive(SHAPE_INFO* info,const bool enable);
        void removeObject(RENDERIZABLE* ptr);
        void removeObjectByIdSceneScene(const int _idScene);
        INFO_JOINT* getInfoJoint(SHAPE_INFO* info,const unsigned int index);
        const b2Vec2 getReactionForce(SHAPE_INFO* info,const float delta);
        void queryAABB(const b2AABB &b2aabb,b2QueryCallback* pB2QueryCallback);
        void rayCast(const b2Vec2 &p1,const b2Vec2 &p2,b2RayCastCallback* pb2RayCastCallback);
        VEC2 getGravity();
        void setGravity(const VEC2 * gravity);
        void init(const VEC2 * gravity);
        unsigned int createJoint(SHAPE_INFO* info1,SHAPE_INFO* info2,b2JointDef &pjd);
        void setAngularDamping(SHAPE_INFO* info,const float angularDamping);
        void setFilter(SHAPE_INFO* info,const b2Filter& filter);
        void setEnabled(SHAPE_INFO* info, const bool active);
        void setContactListener(b2ContactListener * ptrB2ContactListener);
        SHAPE_INFO* addStaticBody(RENDERIZABLE* controller,
                                    const float density = 0.0f,
                                    const float friction = 0.3f,
                                    const float reduceX = 1.0f,
                                    const float reduceY = 1.0f,
                                    const bool  isSensor = false);
        SHAPE_INFO* addDynamicBody( RENDERIZABLE* controller,
                                        const float density = 1.0f,
                                        const float friction = 10.0f,
                                        const float restitution = 0.1f,
                                        const float reduceX = 1.0f,
                                        const float reduceY = 1.0f,
                                        const bool  isSensor = false,
                                        const bool isBullet = false);
        SHAPE_INFO* addKinematicBody(   RENDERIZABLE* controller,
                                        const float density = 1.0f,
                                        const float friction = 0.3f,
                                        const float restitution = 0.1f,
                                        const float reduceX = 1.0f,
                                        const float reduceY = 1.0f,
                                        const bool  isSensor = false);
        INFO_FLUID* createRenderizableFluid(const INFO_PHYSICS* const physics,
                                        const VEC3 &position,
                                        const VEC3 &scale,
                                        const VEC2 &linearVelocity,
                                        const float angularVelocity,
                                        const float angle,
                                        const char* texture,
                                        const mbm::COLOR* color,
                                        const b2ParticleFlag flags,
                                        const b2ParticleGroupFlag groupFlags,
                                        const float lifetime,
                                        const float radius,
                                        const float damping,
                                        const float strength,
                                        const float stride,
                                        const bool is3d,
                                        const bool is2dScreen,
                                        const bool segmented,
                                        const float radiusScale);
        static int32 addParticleToFluid(INFO_FLUID* info,const INFO_PHYSICS* const infoPhysics, const VEC3 &position, const VEC3 &scale,const float scaleEngine);
        void applyForce(SHAPE_INFO* info,const float x,const float y,const float wx,const float wy);
        void applyTorque(SHAPE_INFO* info,const float torque,bool awake);
        void setLinearVelocity(SHAPE_INFO* info,const float x,const float y);
        void applyLinearImpulse(SHAPE_INFO* info,const float x,const float y,const float wx,const float wy);
        void applyAngularImpulse(SHAPE_INFO* info,const float impulse);
        bool isOnTheGround(SHAPE_INFO* info);//is above any ground
        void setFriction(SHAPE_INFO* info,const float friction,const bool update_contact_list = true);//Change the friction
        void setDensity(SHAPE_INFO* info,const float density,const bool reset_mass);//Change the density
        void setRestitution(SHAPE_INFO* info,const float restitution,const bool update_contact_list = true);//Change the restitution
        void setMass(SHAPE_INFO* info,const float newMass);//Change the mass
        void interference(SHAPE_INFO* info);
        void interference(b2Body* body,const VEC2 *newPosition,const float newAngleDegree);
        void interference(b2Body* body,const VEC3 *newPosition,const float newAngleDegree);
        bool removeJoint(SHAPE_INFO* info);
        bool destroyFluid(INFO_FLUID* info);
        void setFixedRotation(SHAPE_INFO* info, bool value);
        void setSleepingAllowed(SHAPE_INFO* info, bool value);
    
    protected:
        void update(const float fps,const float delta);
        void update_fluid(INFO_FLUID* info);
        void update_uv_fluid(INFO_FLUID* info);

    private:
        float                           scale,scalePercentage;
        b2World*                        world;
        std::vector<SHAPE_INFO*>        lsShape;
        std::vector<INFO_JOINT*>        lsJoint;
        std::vector<INFO_FLUID*>        lsFluid;
        
        SHAPE_INFO* completeStaticBody( RENDERIZABLE* controller,
                                        const float density,
                                        const float friction,
                                        const float reduceX,
                                        const float reduceY,
                                        const bool  isSensor);
        SHAPE_INFO* completeDynamicBody(RENDERIZABLE* controller,
                                        const float density,
                                        const float friction,
                                        const float restitution,
                                        const float reduceX,
                                        const float reduceY,
                                        const bool  isSensor,
                                        const bool iskinematicBody,
                                        const bool isBullet);
        void BeginContact(b2Contact* contact);
        void EndContact(b2Contact* contact);
        void PreSolve(b2Contact* contact,const b2Manifold* oldManifold);
        void PostSolve(b2Contact* contact, const b2ContactImpulse* impulse);
        void safeDestroyBody(SHAPE_INFO* infoBox2d);
        void safeRemoveJoint(SHAPE_INFO* infoBox2d);
        void safeDestroyFluid(INFO_FLUID* pInfoFluid);
        std::vector<SHAPE_INFO*>            ls2RemoveBody;
        std::vector<SHAPE_INFO*>            ls2RemoveJoint;
        std::vector<SHAPE_INFO*>            lsActiveCollisionBody;
        std::vector<SHAPE_INFO*>            lsDisableCollisionBody;
        std::vector<INFO_FLUID*>            ls2RemoveFluid;
    };

};
#endif
