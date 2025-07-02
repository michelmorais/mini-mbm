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


#ifndef PHYSICS_BULLET_WRAP_H
#define PHYSICS_BULLET_WRAP_H


#if defined BT_USE_DOUBLE_PRECISION
    #undef BT_USE_DOUBLE_PRECISION
#endif

#ifndef BULLET_VERSION
    #define BULLET_VERSION "2.8"
#endif

#include <core_mbm/physics.h>
#include <core_mbm/scene.h>
#include <map>

class btVector3;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btDiscreteDynamicsWorld;
class btSequentialImpulseConstraintSolver;
class btBroadphaseInterface;
class btRigidBody;

namespace mbm
{

    class RENDERIZABLE;
    struct VEC3;
    class SHAPE_INFO_3D;

    class INFO_CONSTRAINT_3D
    {
    public:
        SHAPE_INFO_3D*  infoA;
        SHAPE_INFO_3D*  infoB;
        INFO_CONSTRAINT_3D(SHAPE_INFO_3D*   info_a,SHAPE_INFO_3D*   info_b/*,b2Joint* _joint*/);
        virtual ~INFO_CONSTRAINT_3D();
    };

    class PHYSICS_BULLET : public PHYSICS
    {
    public:
        int     maxSubSteps;
        float   fixedTimeStep;
        float   scale;
        float   distantceToBeContact;
        bool    stopSimulate;
        typedef void (* On_bullet3d_Collision)(PHYSICS_BULLET*,RENDERIZABLE* info1,RENDERIZABLE* info2);
        typedef const bool (*Callback_bullet3d_onQueryAABBBullet3d)(RENDERIZABLE *info1, const char *callBackFunction);
        On_bullet3d_Collision   on_bullet3d_BeginContact;
        On_bullet3d_Collision   on_bullet3d_EndContact;
        On_bullet3d_Collision   on_bullet3d_KeepContact;
        Callback_bullet3d_onQueryAABBBullet3d       callback_bullet3d_onQueryAABBBullet3d;
        PHYSICS_BULLET(SCENE* scene);
        virtual ~PHYSICS_BULLET();
        void destroyBody(SHAPE_INFO_3D* info);
        void enableDisableCollisionBody(SHAPE_INFO_3D* info,const bool enable);
        void removeObject(mbm::RENDERIZABLE* ptr);
        void removeObjectByIdSceneScene(const int _idScene);
        INFO_CONSTRAINT_3D* getInfoJoint(SHAPE_INFO_3D* infoBullet);
        VEC3 getGravity();
        void setGravity(const VEC3 & gravity);
        void init(const btVector3& gravity);
        btRigidBody* addBody(RENDERIZABLE* controller,
                                    const float mass = 0.0f,
                                    const float friction = 0.3f,
                                    const float reduceX = 1.0f,
                                    const float reduceY = 1.0f,
                                    const float reduceZ = 1.0f,
                                    const bool  isKinematic = false,
                                    const bool  isCharacter = false);
        void applyForce(SHAPE_INFO_3D* infoBullet,const float x,const float y,float z);
        void setAwake(SHAPE_INFO_3D* infoBullet, const bool value);
        bool isAwake(SHAPE_INFO_3D* infoBullet);
        void applyTorque(SHAPE_INFO_3D* infoBullet,const float x,const float y, const float z);
        void setLinearVelocity(SHAPE_INFO_3D* infoBullet,const float x,const float y, const float z);
        void applyImpulse(SHAPE_INFO_3D* infoBullet,const float x,const float y,const float z);
        const bool isOnTheGround(SHAPE_INFO_3D* infoBullet);
        void setFriction(SHAPE_INFO_3D* infoBullet,const float friction);
        void setRestituition(SHAPE_INFO_3D* infoBullet,const float restituition);
        void setMass(SHAPE_INFO_3D* infoBullet,const float newMass, const float x, const float y, const float z);
        void interference(SHAPE_INFO_3D* infoBullet);
        mbm::RENDERIZABLE* rayCast(const btVector3 &startPoint,const btVector3 &endPoint,btVector3 &hit,btVector3 &normal);

    protected:
        void update(const float fps,const float delta);
        typedef std::map<SHAPE_INFO_3D*,SHAPE_INFO_3D*>::const_iterator itCollision;
        void checkColision();
        
    private:
        std::vector<SHAPE_INFO_3D*>     lsShape;
        std::vector<INFO_CONSTRAINT_3D*>        lsJoint;
        std::map<SHAPE_INFO_3D*,SHAPE_INFO_3D*> lsCallBackColision;

        btRigidBody* completeBody(  RENDERIZABLE* controller,
                                        const float mass,
                                        const float friction,
                                        const float reduceX,
                                        const float reduceY,
                                        const float reduceZ,
                                        const bool  isKinematic,
                                        const bool  isCharacter);
        void safeDestroyBody(SHAPE_INFO_3D* infoBullet);

        std::vector<SHAPE_INFO_3D*> ls2RemoveBody;
        std::vector<SHAPE_INFO_3D*> ls2EnableCollisionBody;
        std::vector<SHAPE_INFO_3D*> ls2DisableCollisionBody;
        // collision configuration contains default setup for memory , collision setup . Advanced users can create their own configuration .
        btDefaultCollisionConfiguration *   collisionConfiguration;
        // use the default collision dispatcher . For parallel processing you can use a diffent dispatcher (see Extras / BulletMultiThreaded )
        btCollisionDispatcher *             dispatcher;
        // btDbvtBroadphase is a good general purpose broadphase . You can also try out btAxis3Sweep .
        btBroadphaseInterface *             overlappingPairCache;
        // the default constraint solver . For parallel processing you can use a different solver (see Extras / BulletMultiThreaded )
        btSequentialImpulseConstraintSolver * solver;
        btDiscreteDynamicsWorld *           dynamicsWorld;
    };
};
#endif
