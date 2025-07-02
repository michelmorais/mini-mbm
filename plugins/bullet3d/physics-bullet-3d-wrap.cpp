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

#if defined BT_USE_DOUBLE_PRECISION
    #undef BT_USE_DOUBLE_PRECISION
#endif

#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>
#include "physics-bullet-3d-wrap.h"
#include "shape-info-bullet-3d.h"
#include <core_mbm/renderizable.h>
#include <core_mbm/util.h>
#include <core_mbm/scene.h>
#include <core_mbm/device.h>
#include <core_mbm/primitives.h>

namespace mbm
{
    
    INFO_CONSTRAINT_3D::INFO_CONSTRAINT_3D(SHAPE_INFO_3D*   info_a,SHAPE_INFO_3D*   info_b/*,b2Joint* _joint*/)
    {
        this->infoA =   info_a;
        this->infoB =   info_b;
        //this->joint   =   _joint;
    }
        
    INFO_CONSTRAINT_3D::~INFO_CONSTRAINT_3D()
    = default;
        
    PHYSICS_BULLET::PHYSICS_BULLET(SCENE* scene):PHYSICS(scene->getIdScene())
    {
        this->collisionConfiguration    = nullptr;
        this->dispatcher                = nullptr;
        this->overlappingPairCache      = nullptr;
        this->solver                    = nullptr;
        this->dynamicsWorld             = nullptr;
        this->stopSimulate              = false;
        DEVICE* device                  = DEVICE::getInstance();
        device->addPhysics(this);
        this->maxSubSteps               = 10;
        this->fixedTimeStep             = btScalar(1.)/btScalar(60.);
        this->scale                     = 0.2f;//10
        this->distantceToBeContact      = 1.0f;
        this->on_bullet3d_BeginContact  = nullptr;
        this->on_bullet3d_EndContact    = nullptr;
        this->on_bullet3d_KeepContact   = nullptr;
        this->callback_bullet3d_onQueryAABBBullet3d     =   nullptr;
    }
    
    PHYSICS_BULLET::~PHYSICS_BULLET()
    {
        if(this->dynamicsWorld)
        {
            for (auto infoJoint : this->lsJoint)
            {
                //if(infoJoint->joint)
                //  this->dynamicsWorld->DestroyJoint(infoJoint->joint);
                delete infoJoint;
            }
            this->lsJoint.clear();

            for (auto infoBullet : this->lsShape)
            {
                this->dynamicsWorld->removeCollisionObject(infoBullet->body);
                delete infoBullet;
            }
            this->lsShape.clear();

            delete this->dynamicsWorld ;
            delete this->solver ;
            delete this->overlappingPairCache ;
            delete this->dispatcher ;
        }
        this->collisionConfiguration    = nullptr;
        this->dispatcher                = nullptr;
        this->overlappingPairCache      = nullptr;
        this->solver                    = nullptr;
        this->dynamicsWorld             = nullptr;
        this->on_bullet3d_BeginContact  = nullptr;
        this->on_bullet3d_EndContact    = nullptr;
        this->on_bullet3d_KeepContact   = nullptr;
        this->callback_bullet3d_onQueryAABBBullet3d     =   nullptr;
        DEVICE* device = DEVICE::getInstance();
        device->removePhysics(this);
    }
    
    void PHYSICS_BULLET::destroyBody(SHAPE_INFO_3D* info)
    {
        this->ls2RemoveBody.push_back(info);
    }
    
    void PHYSICS_BULLET::enableDisableCollisionBody(SHAPE_INFO_3D* info,const bool enable)
    {
        if (enable)
            this->ls2EnableCollisionBody.push_back(info);
        else
            this->ls2DisableCollisionBody.push_back(info);
    }
    
    void PHYSICS_BULLET::removeObject(mbm::RENDERIZABLE* ptr)
    {
        for(unsigned int i=0; i< this->lsJoint.size(); ++i)
        {
            INFO_CONSTRAINT_3D* infoJoint = this->lsJoint[i];
            if(infoJoint->infoA->ptr == ptr)
            {
                //if(infoJoint->joint)
                //  this->dynamicsWorld->DestroyJoint(infoJoint->joint);
                delete infoJoint;
                this->lsJoint.erase(this->lsJoint.begin() + i);
                i--;
            }
        }
        for(unsigned int i=0; i< this->lsShape.size(); ++i)
        {
            SHAPE_INFO_3D* infoBullet = this->lsShape[i];
            if(ptr == infoBullet->ptr)
            {
                this->dynamicsWorld->removeCollisionObject(infoBullet->body);
                this->lsShape.erase(this->lsShape.begin() + i);
                delete infoBullet;
                break;
            }
        }
    }
    
    void PHYSICS_BULLET::removeObjectByIdSceneScene(const int _idScene)
    {
        for(unsigned int i=0; i< this->lsJoint.size(); ++i)
        {
            INFO_CONSTRAINT_3D* infoJoint = this->lsJoint[i];
            if(infoJoint->infoA->ptr->getIdScene() == _idScene)
            {
                //if(infoJoint->joint)
                //  this->dynamicsWorld->DestroyJoint(infoJoint->joint);
                delete infoJoint;
                this->lsJoint.erase(this->lsJoint.begin() + i);
                i--;
            }
        }
        for(unsigned int i=0; i< this->lsShape.size(); ++i)
        {
            SHAPE_INFO_3D* infoBullet = this->lsShape[i];
            if(infoBullet->ptr->getIdScene() == _idScene)
            {
                this->dynamicsWorld->removeCollisionObject(infoBullet->body);
                this->lsShape.erase(this->lsShape.begin() + i);
                delete infoBullet;
                i--;
            }
        }
    }
    
    INFO_CONSTRAINT_3D * PHYSICS_BULLET::getInfoJoint(SHAPE_INFO_3D* infoBullet)
    {
        if(this->dynamicsWorld && infoBullet)
        {
            for(auto infoJoint : this->lsJoint)
            {
                if(infoJoint->infoA == infoBullet || infoJoint->infoB == infoBullet)
                {
                    return infoJoint;
                }
            }
        }
        return nullptr;
    }
    
    VEC3 PHYSICS_BULLET::getGravity()
    {
        VEC3 result(0,0,0);
        if(this->dynamicsWorld)
        {
            const btVector3 gravity(this->dynamicsWorld->getGravity());
            result.x            =   gravity.getX() / this->scale;
            result.y            =   gravity.getY() / this->scale;
            result.z            =   gravity.getZ() / this->scale;
        }
        return result;
    }
    
    void PHYSICS_BULLET::setGravity(const VEC3 & gravity)
    {
        if(this->dynamicsWorld)
        {
            this->dynamicsWorld->setGravity(btVector3(  gravity.x * this->scale,
                                                        gravity.y * this->scale, 
                                                        gravity.z * this->scale));
        }
    }
    
    void PHYSICS_BULLET::init(const btVector3& gravity)//Inicia a f�sica indicando a gravidade
    {
        if(!this->dynamicsWorld)
        {
            // collision configuration contains default setup for memory , collision setup . Advanced users can create their own configuration .
            this->collisionConfiguration = new btDefaultCollisionConfiguration ();

            // use the default collision dispatcher . For parallel processing you can use a diffent dispatcher (see Extras / BulletMultiThreaded )
            this->dispatcher = new btCollisionDispatcher ( collisionConfiguration );

            // btDbvtBroadphase is a good general purpose broadphase . You can also try out btAxis3Sweep .
            this->overlappingPairCache = new btDbvtBroadphase ();

            // the default constraint solver . For parallel processing you can use a different solver (see Extras / BulletMultiThreaded )
            this->solver = new btSequentialImpulseConstraintSolver ;

            this->dynamicsWorld = new btDiscreteDynamicsWorld ( dispatcher , overlappingPairCache ,solver , collisionConfiguration );

            this->dynamicsWorld->setGravity (gravity);
        }
    }
    
    btRigidBody* PHYSICS_BULLET::addBody(RENDERIZABLE* controller,
                                const float mass ,
                                const float friction ,
                                const float reduceX ,
                                const float reduceY ,
                                const float reduceZ ,
                                const bool  isKinematic ,
                                const bool  isCharacter )
    {
        if(controller == nullptr || controller->isLoaded() == false)
            return nullptr;
        return this->completeBody(controller,mass,friction,reduceX,reduceY,reduceZ, isKinematic, isCharacter);
    }
    
    void PHYSICS_BULLET::applyForce(SHAPE_INFO_3D* infoBullet,const float x,const float y,float z)
    {
        if(infoBullet && infoBullet->body)
        {
            const btVector3 f(x,y,z);
            const btVector3 p(infoBullet->ptr->position.x, infoBullet->ptr->position.y, infoBullet->ptr->position.z);
            infoBullet->body->applyForce(f,p);
        }
    }
    
    void PHYSICS_BULLET::setAwake(SHAPE_INFO_3D* infoBullet, const bool value)
    {
        if (infoBullet && infoBullet->body)
        {
            infoBullet->body->setActivationState(value ? ACTIVE_TAG : ISLAND_SLEEPING);//verify
        }
    }
    
    bool PHYSICS_BULLET::isAwake(SHAPE_INFO_3D* infoBullet)
    {
        if (infoBullet && infoBullet->body)
            return infoBullet->body->isActive();
        return false;
    }
    
    void PHYSICS_BULLET::applyTorque(SHAPE_INFO_3D* infoBullet,const float x,const float y, const float z)
    {
        if(infoBullet && infoBullet->body)
        {
            const float S = pow(this->scale,2);
            const btVector3 torque(x * S,y * S,z * S);
            infoBullet->body->applyTorque(torque);
        }
    }
    
    void PHYSICS_BULLET::setLinearVelocity(SHAPE_INFO_3D* infoBullet,const float x,const float y, const float z)
    {
        if(infoBullet && infoBullet->body)
        {
            const btVector3 v(x * this->scale,y * this->scale,z * this->scale);
            infoBullet->body->setLinearVelocity(v);
        }
    }
    
    void PHYSICS_BULLET::applyImpulse(SHAPE_INFO_3D* infoBullet,const float x,const float y,const float z)
    {
        if(infoBullet && infoBullet->body)
        {
            const btVector3 i(x * this->scale ,y * this->scale,z * this->scale);
            const btVector3 p(infoBullet->ptr->position.x, infoBullet->ptr->position.y, infoBullet->ptr->position.z);
            infoBullet->body->applyImpulse(i,p); 
        }
    }
    
    const bool PHYSICS_BULLET::isOnTheGround(SHAPE_INFO_3D* infoBullet)//is above any ground
    {
        if(this->on_bullet3d_BeginContact || this->on_bullet3d_EndContact || this->on_bullet3d_KeepContact)
        {
            itCollision ret = this->lsCallBackColision.find(infoBullet);
            if(ret != this->lsCallBackColision.cend())
                return true;
            return false;
        }
        else
        {
            int numManifolds = dispatcher->getNumManifolds();
            for (int i=0;i<numManifolds;++i)
            {
                btPersistentManifold* contactManifold =  dispatcher->getManifoldByIndexInternal(i);
                int numContacts = contactManifold->getNumContacts();
                if(numContacts)
                {
                    const auto* obA = static_cast<const btCollisionObject*>(contactManifold->getBody0());
                    const auto* obB = static_cast<const btCollisionObject*>(contactManifold->getBody1());
                    for (int j=0;j<numContacts;++j)
                    {
                        btManifoldPoint& manifoldPoint = contactManifold->getContactPoint(j);
                        const float dist  = manifoldPoint.getDistance();
                        if (dist <= this->distantceToBeContact)
                        {
                            auto* infoA = static_cast<SHAPE_INFO_3D*>(obA->getUserPointer());
                            auto* infoB = static_cast<SHAPE_INFO_3D*>(obB->getUserPointer());
                            if(infoA == infoBullet)
                                return true;
                            if(infoB == infoBullet)
                                return true;
                        }
                    }
                }
            }
        }
        return false;
    }
    
    void PHYSICS_BULLET::setFriction(SHAPE_INFO_3D* infoBullet,const float friction)//Change the friction
    {
        if(infoBullet && infoBullet->body)
        {
            infoBullet->body->setFriction(friction);
        }
    }
    
    void PHYSICS_BULLET::setRestituition(SHAPE_INFO_3D* infoBullet,const float restituition)//Change the restituition
    {
        if(infoBullet && infoBullet->body)
        {
            infoBullet->body->setRestitution(restituition);
        }
    }
    
    void PHYSICS_BULLET::setMass(SHAPE_INFO_3D* infoBullet,const float newMass, const float x, const float y, const float z)
    {
        if(infoBullet && infoBullet->body)
        {
            const btVector3 inertia(x,y,z);
            infoBullet->body->setMassProps(newMass, inertia);
        }
    }
    
    void PHYSICS_BULLET::interference(SHAPE_INFO_3D* infoBullet)
    {
        if(this->dynamicsWorld && infoBullet)
        {
            if(infoBullet->body)
            {
                //const float scalePerc = 1.0f / this->scale;
                btTransform transform;
                const btVector3 position(   infoBullet->ptr->position.x,
                                            infoBullet->ptr->position.y,
                                            infoBullet->ptr->position.z);
                transform.setIdentity();
                transform.setOrigin(position);

                const btQuaternion QuatAroundX = btQuaternion( btVector3(1.0,0.0,0.0), infoBullet->ptr->angle.x );
                const btQuaternion QuatAroundY = btQuaternion( btVector3(0.0,1.0,0.0), infoBullet->ptr->angle.y );
                const btQuaternion QuatAroundZ = btQuaternion( btVector3(0.0,0.0,1.0), infoBullet->ptr->angle.z );
                const btQuaternion rotation    = QuatAroundX * QuatAroundY * QuatAroundZ;

                transform.setRotation(rotation);
                //infoBullet->body->setWorldTransform(transform);
                infoBullet->setWorldTransform(transform);
                infoBullet->body->activate(true);
            }
        }
    }
    
    mbm::RENDERIZABLE * PHYSICS_BULLET::rayCast(const btVector3 &startPoint,const btVector3 &endPoint,btVector3 &hit,btVector3 &normal)
    {
        if(this->dynamicsWorld)
        {
            btCollisionWorld::ClosestRayResultCallback RayCallback(startPoint, endPoint);
            this->dynamicsWorld->rayTest(startPoint, endPoint, RayCallback);
            if(RayCallback.hasHit()) 
            {
                hit     = RayCallback.m_hitPointWorld;
                normal  = RayCallback.m_hitNormalWorld;
                auto* infoA = static_cast<SHAPE_INFO_3D*>(RayCallback.m_collisionObject->getUserPointer());
                return infoA->ptr;
            }
        }
        return nullptr;
    }
    
    void PHYSICS_BULLET::update(const float fps,const float delta)
    {
        if(!this->dynamicsWorld || this->stopSimulate)
            return;
        // Instruct the dynamicsWorld to perform a single step of simulation.
        // It is generally best to keep the time step and iterations fixed.
        //const float delta = fps == 0.0f  ? 0.0f : 1.0f / fps;
        if(fps == 0.0f) //-V550
            this->dynamicsWorld->stepSimulation(0.0f, this->maxSubSteps, this->fixedTimeStep);
        else
            this->dynamicsWorld->stepSimulation(delta, this->maxSubSteps, this->fixedTimeStep);

        this->checkColision();

        while(this->ls2RemoveBody.size())
        {
            SHAPE_INFO_3D* info = this->ls2RemoveBody[0];
            this->safeDestroyBody(info);
            this->ls2RemoveBody.erase(this->ls2RemoveBody.cbegin());
        }
        while(this->ls2EnableCollisionBody.size())
        {
            SHAPE_INFO_3D* infoBullet = this->ls2EnableCollisionBody[0];
            if (infoBullet && infoBullet->body)
                infoBullet->body->activate(true);
            this->ls2EnableCollisionBody.erase(this->ls2EnableCollisionBody.cbegin());
        }
        while (this->ls2DisableCollisionBody.size())
        {
            SHAPE_INFO_3D* infoBullet = this->ls2DisableCollisionBody[0];
            if (infoBullet && infoBullet->body)
                infoBullet->body->activate(false);
            this->ls2DisableCollisionBody.erase(this->ls2DisableCollisionBody.cbegin());
        }
    }
    
    void PHYSICS_BULLET::checkColision()
    {
        if(this->on_bullet3d_BeginContact || this->on_bullet3d_EndContact || this->on_bullet3d_KeepContact)
        {
            std::map<SHAPE_INFO_3D*,SHAPE_INFO_3D*> lsCallBackColisionLoop;
            std::map<SHAPE_INFO_3D*,SHAPE_INFO_3D*> lsCallBackColisionLoopToErase;
            int numManifolds = dispatcher->getNumManifolds();
            for (int i=0;i<numManifolds;++i)
            {
                btPersistentManifold* contactManifold =  dispatcher->getManifoldByIndexInternal(i);
                int numContacts = contactManifold->getNumContacts();
                if(numContacts)
                {
                    //float totalImpact = 0.0f;
                    const auto* obA = static_cast<const btCollisionObject*>(contactManifold->getBody0());
                    const auto* obB = static_cast<const btCollisionObject*>(contactManifold->getBody1());
                    for (int j=0;j<numContacts;++j)
                    {
                        //obA->getUserPointer();
                        btManifoldPoint& manifoldPoint = contactManifold->getContactPoint(j);
                        //totalImpact += manifoldPoint.m_appliedImpulse;
                        float dist  = manifoldPoint.getDistance();
                        if (dist <= this->distantceToBeContact)
                        {
                            auto* infoA = static_cast<SHAPE_INFO_3D*>(obA->getUserPointer());
                            auto* infoB = static_cast<SHAPE_INFO_3D*>(obB->getUserPointer());
                            lsCallBackColisionLoop[infoA] = infoB;
                            itCollision ret = this->lsCallBackColision.find(infoA);
                            if(ret == this->lsCallBackColision.cend())
                            {
                                this->lsCallBackColision[infoA] = infoB;
                                this->on_bullet3d_BeginContact(this,infoA->ptr,infoB->ptr);
                            }
                            else
                            {
                                if(this->on_bullet3d_KeepContact)
                                    this->on_bullet3d_KeepContact(this,infoA->ptr,infoB->ptr);
                            }
                            //const btVector3& ptA = manifoldPoint.getPositionWorldOnA();
                            //const btVector3& ptB = manifoldPoint.getPositionWorldOnB();
                            //const btVector3& normalOnB = manifoldPoint.m_normalWorldOnB;
                        }
                    }
                }
            }
            for(auto it : this->lsCallBackColision)
            {
                itCollision ret = lsCallBackColisionLoop.find(it.first);
                if(ret == lsCallBackColisionLoop.cend())
                {
                    if(this->on_bullet3d_EndContact)
                        this->on_bullet3d_EndContact(this,it.first->ptr,it.second->ptr);
                    lsCallBackColisionLoopToErase[it.first] = it.second;
                }
            }
            for(auto it : lsCallBackColisionLoopToErase)
            {
                this->lsCallBackColision.erase(it.first);
            }
        }
    }
    
    btRigidBody * PHYSICS_BULLET::completeBody( RENDERIZABLE* controller,
                                    const float mass,
                                    const float friction,
                                    const float reduceX,
                                    const float reduceY,
                                    const float reduceZ,
                                    const bool  isKinematic,
                                    const bool  isCharacter)
    {
        //const float scalePerc = 1.0f;
        //this->scale = 1.0f;
        bool isStaticBody = (mass == 0.0f); //-V550
        const mbm::INFO_PHYSICS* infoPhysics = controller->getInfoPhysics();
        SHAPE_INFO_3D* infoBullet = nullptr;
        if(infoPhysics)
        {
            infoBullet = new SHAPE_INFO_3D(controller,isStaticBody,&this->scale);
            if(infoPhysics->lsCube.size())
            {
                if(infoPhysics->lsCube.size() > 1 )
                {
                    infoBullet->compound = new btCompoundShape();
                    const unsigned int sCube = infoPhysics->lsCube.size();
                    for(unsigned int i=0; i< sCube; ++i)
                    {
                        btCollisionShape* groundShape = nullptr;
                        CUBE* cube =  infoPhysics->lsCube[i];
                        const VEC3 halfSizeCube(    cube->halfDim.x * controller->scale.x * this->scale * reduceX,
                                                    cube->halfDim.y * controller->scale.y * this->scale * reduceY,
                                                    cube->halfDim.z * controller->scale.z * this->scale * reduceZ);
                        if(cube->absCenter.x != 0.0f || cube->absCenter.y != 0.0f || cube->absCenter.z != 0.0f) //-V550
                        {
                            const VEC3 pa(      (cube->absCenter.x * this->scale)  - halfSizeCube.x,
                                                (cube->absCenter.y * this->scale)  - halfSizeCube.y,
                                                (cube->absCenter.z * this->scale)  - halfSizeCube.z);
                            const VEC3 pb(      (cube->absCenter.x * this->scale)  - halfSizeCube.x,
                                                (cube->absCenter.y * this->scale)  + halfSizeCube.y,
                                                (cube->absCenter.z * this->scale)  - halfSizeCube.z);
                            const VEC3 pc(      (cube->absCenter.x * this->scale)  + halfSizeCube.x,
                                                (cube->absCenter.y * this->scale)  + halfSizeCube.y,
                                                (cube->absCenter.z * this->scale)  - halfSizeCube.z);
                            const VEC3 pd(      (cube->absCenter.x * this->scale)  + halfSizeCube.x,
                                                (cube->absCenter.y * this->scale)  - halfSizeCube.y,
                                                (cube->absCenter.z * this->scale)  - halfSizeCube.z);
                            const VEC3 pe(      (cube->absCenter.x * this->scale)  - halfSizeCube.x,
                                                (cube->absCenter.y * this->scale)  - halfSizeCube.y,
                                                (cube->absCenter.z * this->scale)  + halfSizeCube.z);
                            const VEC3 pf(      (cube->absCenter.x * this->scale)  - halfSizeCube.x,
                                                (cube->absCenter.y * this->scale)  + halfSizeCube.y,
                                                (cube->absCenter.z * this->scale)  + halfSizeCube.z);
                            const VEC3 pg(      (cube->absCenter.x * this->scale)  + halfSizeCube.x,
                                                (cube->absCenter.y * this->scale)  + halfSizeCube.y,
                                                (cube->absCenter.z * this->scale)  + halfSizeCube.z);
                            const VEC3 ph(      (cube->absCenter.x * this->scale)  + halfSizeCube.x,
                                                (cube->absCenter.y * this->scale)  - halfSizeCube.y,
                                                (cube->absCenter.z * this->scale)  + halfSizeCube.z);
                            const VEC3 p[8] = {pa,pb,pc,pd,pe,pf,pg,ph};
                            auto  btHull = new btConvexHullShape((float*)p,8,sizeof(VEC3));
                            groundShape = btHull;
                        }
                        else
                        {
                            const btVector3 btHalfSizeCube(halfSizeCube.x,halfSizeCube.y,halfSizeCube.z);
                            auto  btBox = new btBoxShape(btHalfSizeCube);
                            groundShape = btBox;
                        }
                        btTransform btTransform;
                        btTransform.setIdentity();
                        btTransform.setOrigin(btVector3(0,0,0));
                        infoBullet->compound->addChildShape(btTransform,groundShape);
                        infoBullet->lsCollisionShapes.push_back(groundShape);
                    }
                    btVector3 localInertia(0,0,0);
                    if (isStaticBody == false)
                        infoBullet->compound->calculateLocalInertia(mass,localInertia);
                    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,infoBullet, infoBullet->compound,localInertia);
                    infoBullet->getWorldTransform(rbInfo.m_startWorldTransform);
                    infoBullet->body = new btRigidBody(rbInfo);
                    infoBullet->body->setFriction(friction);
                    this->dynamicsWorld->addRigidBody(infoBullet->body);
                }
                else
                {
                    CUBE* cube =  infoPhysics->lsCube[0];
                    btCollisionShape* groundShape = nullptr;
                    btVector3 localInertia(0,0,0);
                    const VEC3 halfSizeCube(    cube->halfDim.x * controller->scale.x * this->scale * reduceX,
                                                cube->halfDim.y * controller->scale.y * this->scale * reduceY,
                                                cube->halfDim.z * controller->scale.z * this->scale * reduceZ);
                    if(cube->absCenter.x != 0.0f || cube->absCenter.y != 0.0f || cube->absCenter.z != 0.0f) //-V550
                    {
/* 
                                   f________________g
                                   /               /|
                                  /               / |
                               b /_______________/c |
                                |   |           |   |
                                |   |           |   |
                                |   |   back    |   |
                                |  e|___________|___|h
                                |  /            |  /
                                | /             | /
                                |/______________|/
                                a   front       d

*/
                            const VEC3 pa(      (cube->absCenter.x * this->scale) - halfSizeCube.x,
                                                (cube->absCenter.y * this->scale) - halfSizeCube.y,
                                                (cube->absCenter.z * this->scale) - halfSizeCube.z);
                            const VEC3 pb(      (cube->absCenter.x * this->scale) - halfSizeCube.x,
                                                (cube->absCenter.y * this->scale) + halfSizeCube.y,
                                                (cube->absCenter.z * this->scale) - halfSizeCube.z);
                            const VEC3 pc(      (cube->absCenter.x * this->scale) + halfSizeCube.x,
                                                (cube->absCenter.y * this->scale) + halfSizeCube.y,
                                                (cube->absCenter.z * this->scale) - halfSizeCube.z);
                            const VEC3 pd(      (cube->absCenter.x * this->scale) + halfSizeCube.x,
                                                (cube->absCenter.y * this->scale) - halfSizeCube.y,
                                                (cube->absCenter.z * this->scale) - halfSizeCube.z);
                            const VEC3 pe(      (cube->absCenter.x * this->scale) - halfSizeCube.x,
                                                (cube->absCenter.y * this->scale) - halfSizeCube.y,
                                                (cube->absCenter.z * this->scale) + halfSizeCube.z);
                            const VEC3 pf(      (cube->absCenter.x * this->scale) - halfSizeCube.x,
                                                (cube->absCenter.y * this->scale) + halfSizeCube.y,
                                                (cube->absCenter.z * this->scale) + halfSizeCube.z);
                            const VEC3 pg(      (cube->absCenter.x * this->scale) + halfSizeCube.x,
                                                (cube->absCenter.y * this->scale) + halfSizeCube.y,
                                                (cube->absCenter.z * this->scale) + halfSizeCube.z);
                            const VEC3 ph(      (cube->absCenter.x * this->scale) + halfSizeCube.x,
                                                (cube->absCenter.y * this->scale) - halfSizeCube.y,
                                                (cube->absCenter.z * this->scale) + halfSizeCube.z);
                        const VEC3 p[8] = {pa,pb,pc,pd,pe,pf,pg,ph};
                        auto  btHull = new btConvexHullShape((float*)p,8,sizeof(VEC3));
                        groundShape = btHull;
                    }
                    else
                    {
                        const btVector3 btHalfSizeCube(halfSizeCube.x,halfSizeCube.y,halfSizeCube.z);
                        auto  btBox = new btBoxShape(btHalfSizeCube);
                        groundShape = btBox;
                    }
                    if (isStaticBody == false)
                        groundShape->calculateLocalInertia(mass,localInertia);
                    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass,infoBullet,groundShape,localInertia);
                    infoBullet->getWorldTransform(rbInfo.m_startWorldTransform);
                    infoBullet->body = new btRigidBody(rbInfo);
                    infoBullet->body->setFriction(friction);
                    this->dynamicsWorld->addRigidBody(infoBullet->body);
                    infoBullet->lsCollisionShapes.push_back(groundShape);
                }
            }
            else if(infoPhysics->lsSphere.size())
            {
                const unsigned int sSphere = infoPhysics->lsSphere.size();
                if (sSphere > 1)
                {
                    infoBullet->compound = new btCompoundShape();
                    for (unsigned int i = 0; i < sSphere; ++i)
                    {
                        mbm::SPHERE* sphere = infoPhysics->lsSphere[i];
                        auto  btSphere = new btSphereShape(sphere->ray * this->scale);
                        btCollisionShape* groundShape = btSphere;
                        btTransform btTransform;
                        btTransform.setIdentity();
                        btTransform.setOrigin(btVector3(sphere->absCenter[0] * this->scale, 
                                                        sphere->absCenter[1] * this->scale, 
                                                        sphere->absCenter[2] * this->scale));
                        infoBullet->compound->addChildShape(btTransform, groundShape);
                        infoBullet->lsCollisionShapes.push_back(groundShape);
                    }
                    btVector3 localInertia(0, 0, 0);
                    if (isStaticBody == false)
                        infoBullet->compound->calculateLocalInertia(mass, localInertia);
                    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, infoBullet, infoBullet->compound, localInertia);
                    infoBullet->getWorldTransform(rbInfo.m_startWorldTransform);
                    infoBullet->body = new btRigidBody(rbInfo);
                    infoBullet->body->setFriction(friction);
                    this->dynamicsWorld->addRigidBody(infoBullet->body);
                }
                else
                {
                    mbm::SPHERE* sphere = infoPhysics->lsSphere[0];
                    auto  btSphere = new btSphereShape(sphere->ray * this->scale);
                    btCollisionShape* groundShape = btSphere;
                    btTransform btTransform;
                    btTransform.setIdentity();
                    btTransform.setOrigin(btVector3(sphere->absCenter[0] * this->scale,
                                                    sphere->absCenter[1] * this->scale, 
                                                    sphere->absCenter[2] * this->scale));
                    btVector3 localInertia(0, 0, 0);
                    if (isStaticBody == false)
                        groundShape->calculateLocalInertia(mass, localInertia);
                    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, infoBullet, groundShape, localInertia);
                    infoBullet->getWorldTransform(rbInfo.m_startWorldTransform);
                    infoBullet->body = new btRigidBody(rbInfo);
                    infoBullet->body->setFriction(friction);
                    this->dynamicsWorld->addRigidBody(infoBullet->body);
                    infoBullet->lsCollisionShapes.push_back(groundShape);
                }
            }
            else if (infoPhysics->lsCubeComplex.size())
            {
                if (infoPhysics->lsCubeComplex.size() > 1)
                {
                    infoBullet->compound = new btCompoundShape();
                    const unsigned int sCube = infoPhysics->lsCubeComplex.size();
                    for (unsigned int i = 0; i< sCube; ++i)
                    {
                        CUBE_COMPLEX* cube = infoPhysics->lsCubeComplex[i];
                        float p[8 * 3];
                        for(unsigned int j=0,k = 0; j< 8; j++, k+= 3)
                        {
                            p[k]   = cube->p[j].x * this->scale;
                            p[k+1] = cube->p[j].y * this->scale;
                            p[k+2] = cube->p[j].z * this->scale;
                        }
                        auto  btHull = new btConvexHullShape(p, 8, sizeof(_VEC3_POINT));
                        btCollisionShape* groundShape = btHull;
                        btTransform btTransform;
                        btTransform.setIdentity();
                        btTransform.setOrigin(btVector3(0, 0, 0));
                        infoBullet->compound->addChildShape(btTransform, groundShape);
                        infoBullet->lsCollisionShapes.push_back(groundShape);
                    }
                    btVector3 localInertia(0, 0, 0);
                    if (isStaticBody == false)
                        infoBullet->compound->calculateLocalInertia(mass, localInertia);
                    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, infoBullet, infoBullet->compound, localInertia);
                    infoBullet->getWorldTransform(rbInfo.m_startWorldTransform);
                    infoBullet->body = new btRigidBody(rbInfo);
                    infoBullet->body->setFriction(friction);
                    this->dynamicsWorld->addRigidBody(infoBullet->body);
                }
                else
                {
                    CUBE_COMPLEX* cube = infoPhysics->lsCubeComplex[0];
                    btVector3 localInertia(0, 0, 0);
                    float p[8 * 3];
                    for(unsigned int j=0,k = 0; j< 8; j++, k+= 3)
                    {
                        p[k]   = cube->p[j].x * this->scale;
                        p[k+1] = cube->p[j].y * this->scale;
                        p[k+2] = cube->p[j].z * this->scale;
                    }
                    auto  btHull = new btConvexHullShape(p, 8, sizeof(_VEC3_POINT));
                    btCollisionShape* groundShape = btHull;
                    if (isStaticBody == false)
                        groundShape->calculateLocalInertia(mass, localInertia);
                    btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, infoBullet, groundShape, localInertia);
                    infoBullet->getWorldTransform(rbInfo.m_startWorldTransform);
                    infoBullet->body = new btRigidBody(rbInfo);
                    infoBullet->body->setFriction(friction);
                    this->dynamicsWorld->addRigidBody(infoBullet->body);
                    infoBullet->lsCollisionShapes.push_back(groundShape);
                }
            }
        }
        if(infoBullet == nullptr || infoBullet->body == nullptr)
        {
            if(infoBullet)
                delete infoBullet;
            return nullptr;
        }
        if(isKinematic)
        {
            infoBullet->body->setCollisionFlags(infoBullet->body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            infoBullet->body->setActivationState(DISABLE_DEACTIVATION);
        }
        if(isCharacter)
        {
            infoBullet->body->setAngularFactor(0);
        }
        this->lsShape.push_back(infoBullet);
        infoBullet->body->setUserPointer(infoBullet);
        //controller->userData->infoBullet = infoBullet;
        return infoBullet->body;
    }
    
    void PHYSICS_BULLET::safeDestroyBody(SHAPE_INFO_3D* infoBullet)
    {
        if(infoBullet && infoBullet->body)
        {
            for(unsigned int i=0; i< this->lsJoint.size(); ++i)
            {
                INFO_CONSTRAINT_3D* infoJoint = this->lsJoint[i];
                if(infoJoint->infoA->ptr == infoBullet->ptr)
                {
                    //if(infoJoint->joint)
                    //  this->dynamicsWorld->DestroyJoint(infoJoint->joint);
                    delete infoJoint;
                    this->lsJoint.erase(this->lsJoint.begin() + i);
                    i--;
                }
            }
            for(unsigned int i=0; i< this->lsShape.size(); ++i)
            {
                SHAPE_INFO_3D* infoShape = this->lsShape[i];
                if(infoBullet->ptr == infoShape->ptr)
                {
                    this->dynamicsWorld->removeCollisionObject(infoBullet->body);
                    this->lsShape.erase(this->lsShape.begin() + i);
                    delete infoShape;
                    break;
                }
            }
            infoBullet = nullptr;
        }
    }
};


