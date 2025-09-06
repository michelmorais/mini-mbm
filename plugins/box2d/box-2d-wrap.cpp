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

#include "box-2d-wrap.h"
#include <core_mbm/scene.h>
#include <core_mbm/device.h>
#include <core_mbm/renderizable.h>

namespace mbm
{
    
    SHAPE_INFO::SHAPE_INFO(RENDERIZABLE* ptrMesh,const b2BodyType newType) noexcept:
        typePhysics(newType),
        ptr(ptrMesh)
    {
        this->body              =  nullptr;
    }

    SHAPE_INFO::~SHAPE_INFO()noexcept
    {
        this->body              =   nullptr;
    }

    INFO_JOINT::INFO_JOINT(SHAPE_INFO*  info_a,SHAPE_INFO*  info_b,b2Joint* _joint) noexcept
    {
        this->infoA =   info_a;
        this->infoB =   info_b;
        this->joint =   _joint;
    }
        
    INFO_JOINT::~INFO_JOINT()noexcept
    = default;

    
    PHYSICS_BOX2D::PHYSICS_BOX2D(SCENE* scene) noexcept:
    PHYSICS(scene->getIdScene())
    {
        this->world         =   nullptr;
        this->stopSimulate  =   false;
        DEVICE* device = DEVICE::getInstance();
        device->addPhysics(this);
        this->velocityIterations = 8;
        this->positionIterations = 3;
        this->scale = 10.0f;
        this->scalePercentage = 1.0f / this->scale;
        this->multiplyStep = 1.0f;
        this->on_box2d_BeginContact = nullptr;
        this->on_box2d_EndContact   = nullptr;
        this->on_box2d_PreSolve     = nullptr;
        this->on_box2d_PostSolve    = nullptr;
        this->on_box2d_DestroyBodyFromList = nullptr;
    }
        
    PHYSICS_BOX2D::~PHYSICS_BOX2D()
    {
        if(this->world)
        {
            this->world->SetContactListener(nullptr);
            const std::vector<INFO_JOINT*>::size_type sj = this->lsJoint.size();
            for(std::vector<INFO_JOINT*>::size_type i=0; i < sj; ++i)
            {
                INFO_JOINT* infoJoint = this->lsJoint[i];
                if(infoJoint->joint)
                    this->world->DestroyJoint(infoJoint->joint);
                delete infoJoint;
            }
            this->lsJoint.clear();
            const std::vector<SHAPE_INFO*>::size_type ss = this->lsShape.size();
            for(std::vector<SHAPE_INFO*>::size_type i = 0; i < ss; ++i)
            {
                SHAPE_INFO* info = this->lsShape[i];
                world->DestroyBody(info->body);
                delete info;
            }
            this->lsShape.clear();
            delete world;
        }
        this->on_box2d_BeginContact = nullptr;
        this->on_box2d_EndContact   = nullptr;
        this->on_box2d_PreSolve     = nullptr;
        this->on_box2d_PostSolve    = nullptr;
        this->on_box2d_DestroyBodyFromList = nullptr;
        world = nullptr;
        DEVICE* device = DEVICE::getInstance();
        device->removePhysics(this);
    }

    void PHYSICS_BOX2D::setScale(const float s)noexcept
    {
        if(s > 0.0f)
        {
            this->scale = s;
            this->scalePercentage = 1.0f / this->scale;
        }
    }
    float PHYSICS_BOX2D::getScale() const noexcept
    {
        return this->scale;
    }
        
    bool PHYSICS_BOX2D::testPoint(SHAPE_INFO* info,const b2Vec2& point)
    {
        if(info && info->body)
        {
            b2Fixture* bFix = info->body->GetFixtureList();
            b2Vec2 point_scaled(point.x / this->scale, point.y / this->scale);
            while (bFix)
            {
                if (bFix->TestPoint(point_scaled))
                    return true;
                bFix = bFix->GetNext();
            }
        }
        return false;
    }
        
    bool PHYSICS_BOX2D::destroyBody(SHAPE_INFO* info)
    {
        const std::vector<SHAPE_INFO*>::size_type sr = this->ls2RemoveBody.size();
        for(std::vector<SHAPE_INFO*>::size_type i = 0; i < sr; ++i)
        {
            SHAPE_INFO* otherInfo = this->ls2RemoveBody[i];
            if (otherInfo == info)
            {
                return false;
            }
        }
        this->ls2RemoveBody.push_back(info);
        return true;
    }
        
    bool PHYSICS_BOX2D::undoDestroyBody(SHAPE_INFO* info)
    {
        const std::vector<SHAPE_INFO*>::size_type sr = this->ls2RemoveBody.size();
        for(std::vector<SHAPE_INFO*>::size_type i = 0; i < sr; ++i)
        {
            SHAPE_INFO* otherInfo = this->ls2RemoveBody[i];
            if (otherInfo == info)
            {
                this->ls2RemoveBody.erase(this->ls2RemoveBody.begin() + i);
                return true;
            }
        }
        return false;
    }

    void PHYSICS_BOX2D::setActive(SHAPE_INFO* info,const bool enable)
    {
        if (enable)
            this->lsActiveCollisionBody.push_back(info);
        else
            this->lsDisableCollisionBody.push_back(info);
    }
    
    void PHYSICS_BOX2D::removeObject(RENDERIZABLE* ptr)
    {
        for(std::vector<INFO_JOINT*>::size_type i = 0; i < this->lsJoint.size(); ++i)//must be size()
        {
            INFO_JOINT* infoJoint = this->lsJoint[i];
            if(infoJoint->infoA->ptr == ptr)
            {
                if(infoJoint->joint)
                    this->world->DestroyJoint(infoJoint->joint);
                delete infoJoint;
                this->lsJoint.erase(this->lsJoint.begin() + i);
                i--;
            }
        }
        for(std::vector<SHAPE_INFO*>::size_type i = 0; i < this->lsShape.size(); ++i)//must be size()
        {
            SHAPE_INFO* info = this->lsShape[i];
            if(ptr == info->ptr)
            {
                world->DestroyBody(info->body);
                this->lsShape.erase(this->lsShape.begin() + i);
                delete info;
                i--;
            }
        }
    }
    
    void PHYSICS_BOX2D::removeObjectByIdSceneScene(const int _idScene)
    {
        for(std::vector<INFO_JOINT*>::size_type i = 0; i < this->lsJoint.size(); ++i)//must be size()
        {
            INFO_JOINT* infoJoint = this->lsJoint[i];
            if(infoJoint->infoA->ptr->getIdScene() == _idScene)
            {
                if(infoJoint->joint)
                    this->world->DestroyJoint(infoJoint->joint);
                infoJoint->joint = nullptr;
                delete infoJoint;
                this->lsJoint.erase(this->lsJoint.begin() + i);
                i--;
            }
        }
        for(std::vector<SHAPE_INFO*>::size_type i = 0; i < this->lsShape.size(); ++i)//must be size()
        {
            SHAPE_INFO* info = this->lsShape[i];
            if(info->ptr->getIdScene() == _idScene)
            {
                world->DestroyBody(info->body);
                this->lsShape.erase(this->lsShape.begin() + i);
                delete info;
                i--;
            }
        }
    }
    
    const b2Vec2 PHYSICS_BOX2D::getReactionForce(SHAPE_INFO* info,const float delta)
    {
        static b2Vec2 bRet(0,0);
        if(this->world && info)
        {
            const std::vector<INFO_JOINT*>::size_type sj = this->lsJoint.size();
            for(std::vector<INFO_JOINT*>::size_type i = 0; i < sj; ++i)
            {
                INFO_JOINT* infoJoint = this->lsJoint[i];
                if(infoJoint->infoA == info || infoJoint->infoB == info)
                {
                    return infoJoint->joint->GetReactionForce(delta);
                }
            }
        }
        return bRet;
    }
    
    void PHYSICS_BOX2D::queryAABB(const b2AABB &b2aabb,b2QueryCallback* pB2QueryCallback)
    {
        if(this->world)
        {
            this->world->QueryAABB(pB2QueryCallback,b2aabb);
        }
    }

    void PHYSICS_BOX2D::rayCast(const b2Vec2 &p1,const b2Vec2 &p2,b2RayCastCallback* pb2RayCastCallback)
    {
        if(this->world)
        {
            this->world->RayCast(pb2RayCastCallback,p1,p2);
        }
    }

    VEC2 PHYSICS_BOX2D::getGravity()
    {
        VEC2 result(0,0);
        if(world)
        {
            b2Vec2 gravity  =   world->GetGravity();
            result.x        =   gravity.x;
            result.y        =   gravity.y;
        }
        return result;
    }
    
    void PHYSICS_BOX2D::setGravity(const VEC2 * gravity)
    {
        if(world)
        {
            world->SetGravity(b2Vec2(gravity->x,gravity->y));
        }
    }
    
    void PHYSICS_BOX2D::init(const VEC2 * gravity)
    {
        if(!this->world)
        {
            b2Vec2 b2Gravity(gravity->x,gravity->y);
            this->world = new b2World(b2Gravity);
        }
    }
    
    unsigned int PHYSICS_BOX2D::createJoint(SHAPE_INFO* info1,SHAPE_INFO* info2,b2JointDef & pjd)
    {
        if(this->world && info1->body && info2->body)
        {
            pjd.bodyA = info1->body;
            pjd.bodyB = info2->body;
            b2Joint* m_joint = this->world->CreateJoint(&pjd);
            INFO_JOINT*  infoJoint = new INFO_JOINT(info1,info2,m_joint);
            m_joint->SetUserData(info1);
            this->lsJoint.push_back(infoJoint);
            return this->lsJoint.size();
        }
        return 0xffffffff;
    }

    INFO_JOINT * PHYSICS_BOX2D::getInfoJoint(SHAPE_INFO* info,const unsigned int index)
    {
        if(this->world && info)
        {
            if(index < this->lsJoint.size())
                return this->lsJoint[index];
            const std::vector<INFO_JOINT*>::size_type sj = this->lsJoint.size();
            for(std::vector<INFO_JOINT*>::size_type i = 0; i < sj; ++i)
            {
                INFO_JOINT* infoJoint = this->lsJoint[i];
                if(infoJoint->infoA == info || infoJoint->infoB == info)
                {
                    return infoJoint;
                }
            }
        }
        return nullptr;
    }

    void PHYSICS_BOX2D::setAngularDamping(SHAPE_INFO* info,const float angularDamping)
    {
        if(this->world)
        {
            if(info)
            {
                if(info->body)
                    info->body->SetAngularDamping(angularDamping);
            }
            else
            {
                const std::vector<SHAPE_INFO*>::size_type ss = this->lsShape.size();
                for(std::vector<SHAPE_INFO*>::size_type i = 0; i < ss; ++i)
                {
                    info = this->lsShape[i];
                    if(info->body)
                        info->body->SetAngularDamping(angularDamping);
                }
            }
        }
    }
    
    void PHYSICS_BOX2D::setFilter(SHAPE_INFO* info,const b2Filter& filter)
    {
        if(this->world)
        {
            if(info)
            {
                if(info->body)
                {
                    b2Fixture* bFix =  info->body->GetFixtureList();
                    while(bFix)
                    {
                        bFix->SetFilterData(filter);
                        bFix = bFix->GetNext();
                    }
                }
            }
            else
            {
                const std::vector<SHAPE_INFO*>::size_type ss = this->lsShape.size();
                for(std::vector<SHAPE_INFO*>::size_type i = 0; i < ss; ++i)
                {
                    info = this->lsShape[i];
                    if(info->body)
                    {
                        b2Fixture* bFix =  info->body->GetFixtureList();
                        while(bFix)
                        {
                            bFix->SetFilterData(filter);
                            bFix = bFix->GetNext();
                        }
                    }
                }
            }
        }
    }
    
    void PHYSICS_BOX2D::setEnabled(SHAPE_INFO* info, const bool active)
    {
        if(this->world)
        {
            if (info && info->body)
            {
                const uint16 maskBits = active ? 0xFFFF : 0;
                b2Fixture* fixtureList = info->body->GetFixtureList();
                while (fixtureList)
                {
                    b2Filter oldFilter(fixtureList->GetFilterData());
                    oldFilter.maskBits = maskBits;
                    fixtureList->SetFilterData(oldFilter);
                    fixtureList = fixtureList->GetNext();
                }
            }
        }
    }
    
    void PHYSICS_BOX2D::setContactListener(b2ContactListener * ptrB2ContactListener)
    {
        if(this->world)
            this->world->SetContactListener(ptrB2ContactListener);
    }
    
    SHAPE_INFO * PHYSICS_BOX2D::addStaticBody(RENDERIZABLE* controller,
                                const float density ,
                                const float friction,
                                const float reduceX ,
                                const float reduceY ,
                                const bool  isSensor )
    {
        if(controller == nullptr || controller->isLoaded() == false)
            return nullptr;
        return this->completeStaticBody(controller,density,friction,reduceX,reduceY,isSensor);
    }
    
    SHAPE_INFO * PHYSICS_BOX2D::addDynamicBody( RENDERIZABLE* controller,
                                    const float density ,
                                    const float friction ,
                                    const float restitution,
                                    const float reduceX,
                                    const float reduceY,
                                    const bool  isSensor,
                                    const bool isBullet)
    {
        if(controller == nullptr || controller->isLoaded() == false)
            return nullptr;
        return this->completeDynamicBody(controller,density,friction,restitution,reduceX,reduceY,isSensor,false, isBullet);
    }
    
    SHAPE_INFO * PHYSICS_BOX2D::addKinematicBody(   RENDERIZABLE* controller,
                                    const float density ,
                                    const float friction ,
                                    const float restitution,
                                    const float reduceX,
                                    const float reduceY,
                                    const bool  isSensor)
    {
        if(controller == nullptr || controller->isLoaded() == false)
            return nullptr;
        return this->completeDynamicBody( controller, density, friction, restitution, reduceX, reduceY, isSensor, true,false);
    }

    void PHYSICS_BOX2D::setFixedRotation(SHAPE_INFO* info, bool value)
    {
        if(info && info->body)
        {
            info->body->SetFixedRotation(value);
        }
    }

    void PHYSICS_BOX2D::setSleepingAllowed(SHAPE_INFO* info, bool value)
    {
        if(info && info->body)
        {
            info->body->SetSleepingAllowed(value);
        }
    }
    void PHYSICS_BOX2D::applyForce(SHAPE_INFO* info,const float x,const float y,const float wx,const float wy)
    {
        if(info && info->body)
        {
            const b2Vec2 f = info->body->GetWorldVector(b2Vec2(x,y));
            const b2Vec2 p = info->body->GetWorldPoint(b2Vec2(wx,wy));
            info->body->ApplyForce(f,p,true);
        }
    }
    
    void PHYSICS_BOX2D::applyTorque(SHAPE_INFO* info,const float torque,bool awake)
    {
        if(info && info->body)
            info->body->ApplyTorque(torque, awake);
    }
    
    void PHYSICS_BOX2D::setLinearVelocity(SHAPE_INFO* info,const float x,const float y)
    {
        if(info && info->body)
        {
            const b2Vec2 v(x,y);
            info->body->SetLinearVelocity(v);
        }
    }
    
    void PHYSICS_BOX2D::applyLinearImpulse(SHAPE_INFO* info,const float x,const float y,const float wx,const float wy)
    {
        if(info && info->body)
        {
            const b2Vec2 i = info->body->GetWorldVector(b2Vec2(x,y));
            const b2Vec2 p = info->body->GetWorldPoint(b2Vec2(wx,wy));
            info->body->ApplyLinearImpulse(i,p,true);
        }
    }
    
    void PHYSICS_BOX2D::applyAngularImpulse(SHAPE_INFO* info,const float impulse)
    {
        if(info && info->body)
        {
            info->body->ApplyAngularImpulse(impulse,true);
        }
    }
    
    bool PHYSICS_BOX2D::isOnTheGround(SHAPE_INFO* info)
    {
        if(info && info->body)
        {
            const b2ContactEdge* c = info->body->GetContactList();
            while(c)
            {
                if(c->contact->IsTouching())
                {
                    b2Manifold* m = c->contact->GetManifold();
                    if(m->localNormal.y < 0.0f)
                        return true;
                }
                c = c->next;
            }
        }
        return false;
    }
    
    void PHYSICS_BOX2D::setFriction(SHAPE_INFO* info,const float friction,const bool update_contact_list)
    {
        if(info && info->body)
        {
            b2Fixture* fix =  info->body->GetFixtureList();
            while(fix)
            {
                fix->SetFriction(friction);
                fix = fix->GetNext();
            }
            if(update_contact_list)
            {
                const b2ContactEdge* c = info->body->GetContactList();
                while(c)
                {
                    c->contact->SetFriction(friction);
                    c = c->next;
                }
            }
        }
    }
    
    void PHYSICS_BOX2D::setDensity(SHAPE_INFO* info,const float density,const bool reset_mass)
    {
        if(info && info->body)
        {
            b2Fixture* fix =  info->body->GetFixtureList();
            while(fix)
            {
                fix->SetDensity(density);
                fix = fix->GetNext();
            }
            if(reset_mass)
                info->body->ResetMassData();
        }
    }
    
    void PHYSICS_BOX2D::setRestitution(SHAPE_INFO* info,const float restitution,const bool update_contact_list)
    {
        if(info && info->body)
        {
            b2Fixture* fix =  info->body->GetFixtureList();
            while(fix)
            {
                fix->SetRestitution(restitution);
                fix = fix->GetNext();
            }
            if(update_contact_list)
            {
                const b2ContactEdge* c = info->body->GetContactList();
                while(c)
                {
                    c->contact->SetRestitution(restitution);
                    c = c->next;
                }
            }
        }
    }
    
    void PHYSICS_BOX2D::setMass(SHAPE_INFO* info,const float newMass)
    {
        if(info && info->body)
        {
            b2MassData mass = info->body->GetMassData();
            mass.mass = newMass;
            info->body->SetMassData(&mass);
        }
    }
    
    void PHYSICS_BOX2D::interference(SHAPE_INFO* info)
    {
        if(world && info)
        {
            if(info->body)
            {
                const b2Vec2 position(info->ptr->position.x * this->scalePercentage,info->ptr->position.y * this->scalePercentage);
                info->body->SetTransform(position,info->ptr->angle.z);
                info->body->SetAwake(true);
            }
        }
    }
    
    void PHYSICS_BOX2D::interference(b2Body* body,const VEC2 * newPosition,const float newAngleDegree)
    {
        if(world)
        {
            const b2Vec2 position(newPosition->x * this->scalePercentage,newPosition->y * this->scalePercentage);
            body->SetTransform(position,newAngleDegree);
        }
    }
    
    void PHYSICS_BOX2D::interference(b2Body* body,const VEC3 * newPosition,const float newAngleDegree)
    {
        if(world)
        {
            const b2Vec2 position(newPosition->x * this->scalePercentage,newPosition->y * this->scalePercentage);
            body->SetTransform(position,newAngleDegree);
        }
    }

    bool PHYSICS_BOX2D::removeJoint(SHAPE_INFO* info)
    {
        const std::vector<SHAPE_INFO*>::size_type sj = this->ls2RemoveJoint.size();
        for(std::vector<SHAPE_INFO*>::size_type i = 0; i < sj; ++i)
        {
            const SHAPE_INFO* otherInfo = this->ls2RemoveJoint[i];
            if (otherInfo == info)
            {
                return false;
            }
        }
        this->ls2RemoveJoint.push_back(info);
        return true;
    }

    void PHYSICS_BOX2D::update(const float fps,const float delta)
    {
        if(!this->world || this->stopSimulate)
            return;
        const std::vector<SHAPE_INFO*>::size_type ss = this->lsShape.size();
        for(std::vector<SHAPE_INFO*>::size_type i = 0; i < ss; ++i)
        {
            SHAPE_INFO* info  = this->lsShape[i];
            if(info->typePhysics != b2_staticBody)
            {
                const b2Vec2 pos        =   info->body->GetPosition();
                info->ptr->position.x   =   pos.x * this->scale;
                info->ptr->position.y   =   pos.y * this->scale;
                info->ptr->angle.z      =   info->body->GetAngle();
            }
        }
        // Instruct the world to perform a single step of simulation.
        // It is generally best to keep the time step and iterations fixed.
        //const float delta = fps == 0.0f  ? 0.0f : 1.0f / fps;
        //world->Step(delta, this->velocityIterations, this->positionIterations);
        if(fps == 0.0f) //-V550
            world->Step(0.0f, this->velocityIterations, this->positionIterations);
        else
            world->Step(delta * this->multiplyStep, this->velocityIterations, this->positionIterations);


        while(this->ls2RemoveBody.size())
        {
            SHAPE_INFO* info = this->ls2RemoveBody[0];
            RENDERIZABLE* ptr = info->ptr;
            this->safeDestroyBody(info);
            if(this->on_box2d_DestroyBodyFromList)
                this->on_box2d_DestroyBodyFromList(ptr);
            this->ls2RemoveBody.erase(this->ls2RemoveBody.begin());
        }

        while(this->ls2RemoveJoint.size())
        {
            SHAPE_INFO* info = this->ls2RemoveJoint[0];
            this->safeRemoveJoint(info);
            this->ls2RemoveJoint.erase(this->ls2RemoveJoint.begin());
        }

        while(this->lsActiveCollisionBody.size())
        {
            SHAPE_INFO* info = this->lsActiveCollisionBody[0];
            if (info && info->body)
                info->body->SetEnabled(true);
            this->lsActiveCollisionBody.erase(this->lsActiveCollisionBody.begin());
        }
        while (this->lsDisableCollisionBody.size())
        {
            SHAPE_INFO* info = this->lsDisableCollisionBody[0];
            if (info && info->body)
                info->body->SetEnabled(false);
            this->lsDisableCollisionBody.erase(this->lsDisableCollisionBody.begin());
        }
    }
    
    SHAPE_INFO * PHYSICS_BOX2D::completeStaticBody( RENDERIZABLE* controller,
                                    const float density,
                                    const float friction,
                                    const float reduceX,
                                    const float reduceY,
                                    const bool  isSensor)
    {
        b2FixtureDef    fd;
        fd.density      = density;
        fd.friction     = friction;
        fd.isSensor     = isSensor;
        
        auto  info = new SHAPE_INFO(controller,b2_staticBody);
        const mbm::INFO_PHYSICS* infoPhysics = controller->getInfoPhysics();
        if(infoPhysics == nullptr)
        {
            delete info;
            return nullptr;
        }
        if(infoPhysics->lsCube.size())
        {
            const std::vector<CUBE*>::size_type sizeSubset = infoPhysics->lsCube.size();
            for(std::vector<CUBE*>::size_type i = 0; i < sizeSubset; ++i)
            {
                b2PolygonShape  groundBox;
                b2BodyDef       groundBodyDef;
                fd.shape        = &groundBox; //-V506
                const CUBE* cube =  infoPhysics->lsCube[i];
                groundBodyDef.type = b2_staticBody;
                
                b2Vec2 center(cube->absCenter.x * this->scalePercentage,cube->absCenter.y * this->scalePercentage);
                groundBox.SetAsBox( cube->halfDim.x * controller->scale.x * this->scalePercentage * reduceX,
                                    cube->halfDim.y * controller->scale.y * this->scalePercentage * reduceY,
                                    center ,0);
                if(info->body)
                {
                    info->body->CreateFixture(&fd);
                    continue;
                }
                info->body = world->CreateBody(&groundBodyDef);
                info->body->CreateFixture(&fd);
                interference(info);
            }
        }
        if(infoPhysics->lsSphere.size())
        {
            const std::vector<SPHERE*>::size_type sizeSubset = infoPhysics->lsSphere.size();
            for(std::vector<SPHERE*>::size_type i = 0; i < sizeSubset; ++i)
            {
                const SPHERE* sphere =  infoPhysics->lsSphere[i];
                b2CircleShape   shape;
                fd.shape        = &shape; //-V506
                shape.m_radius  = sphere->ray * controller->scale.x * this->scalePercentage * reduceX;
                shape.m_p.Set(  sphere->absCenter[0] * controller->scale.x * this->scalePercentage,
                                sphere->absCenter[1] * controller->scale.y * this->scalePercentage);
                if(info->body)
                {
                    info->body->CreateFixture(&fd);
                    continue;
                }
                        
                b2BodyDef       bodyDef;
                bodyDef.type    = b2_staticBody;
                info->body = world->CreateBody(&bodyDef);
                info->body->CreateFixture(&fd);
                interference(info);
            }
        }
        if(infoPhysics->lsCubeComplex.size())
        {
            const std::vector<CUBE_COMPLEX*>::size_type sizeSubset = infoPhysics->lsCubeComplex.size();
            for(std::vector<CUBE_COMPLEX*>::size_type i = 0; i < sizeSubset; ++i)
            {
                const CUBE_COMPLEX* cube = infoPhysics->lsCubeComplex[i];
                b2PolygonShape  groundPolygn;
                b2Vec2 vertices[4];
                vertices[0].Set(cube->a.x * this->scalePercentage * reduceX * controller->scale.x,cube->a.y * this->scalePercentage * reduceY * controller->scale.y);
                vertices[1].Set(cube->b.x * this->scalePercentage * reduceX * controller->scale.x,cube->b.y * this->scalePercentage * reduceY * controller->scale.y);
                vertices[2].Set(cube->c.x * this->scalePercentage * reduceX * controller->scale.x,cube->c.y * this->scalePercentage * reduceY * controller->scale.y);
                vertices[3].Set(cube->d.x * this->scalePercentage * reduceX * controller->scale.x,cube->d.y * this->scalePercentage * reduceY * controller->scale.y);
                groundPolygn.Set(vertices, 4);
                                
                b2BodyDef       groundBodyDef;
                fd.shape = &groundPolygn; //-V506
                groundBodyDef.type = b2_staticBody;
                if(info->body)
                {
                    info->body->CreateFixture(&fd);
                    continue;
                }
                info->body = world->CreateBody(&groundBodyDef);
                info->body->CreateFixture(&fd);
                interference(info);
            }
        }
        if(infoPhysics->lsTriangle.size())
        {
            const std::vector<TRIANGLE*>::size_type sizeSubset = infoPhysics->lsTriangle.size();
            for(std::vector<TRIANGLE*>::size_type i = 0; i < sizeSubset; ++i)
            {
                const TRIANGLE* triangle = infoPhysics->lsTriangle[i];
                b2PolygonShape  groundTriangle;
                b2Vec2 vertices[3];
                vertices[0].Set(triangle->point[0].x * this->scalePercentage  * reduceX * controller->scale.x,triangle->point[0].y * this->scalePercentage * reduceY* controller->scale.y);
                vertices[1].Set(triangle->point[1].x * this->scalePercentage  * reduceX * controller->scale.x,triangle->point[1].y * this->scalePercentage * reduceY* controller->scale.y);
                vertices[2].Set(triangle->point[2].x * this->scalePercentage  * reduceX * controller->scale.x,triangle->point[2].y * this->scalePercentage * reduceY* controller->scale.y);
                groundTriangle.Set(vertices, 3);
                        
                b2BodyDef       groundBodyDef;
                fd.shape = &groundTriangle; //-V506
                groundBodyDef.type = b2_staticBody;
                if(info->body)
                {
                    info->body->CreateFixture(&fd);
                    continue;
                }
                info->body = world->CreateBody(&groundBodyDef);
                info->body->CreateFixture(&fd);
                interference(info);
            }
        }
        if(info->body == nullptr)
        {
            delete info;
            return nullptr;
        }
        this->lsShape.push_back(info);
        info->body->SetUserData(info);
        return info;
    }
    
    SHAPE_INFO * PHYSICS_BOX2D::completeDynamicBody(RENDERIZABLE* controller,
                                    const float density,
                                    const float friction,
                                    const float restitution,
                                    const float reduceX,
                                    const float reduceY,
                                    const bool  isSensor,
                                    const bool iskinematicBody,
                                    const bool isBullet)
    {
        b2FixtureDef    fd;
        fd.density      =   density;
        fd.friction     =   friction;
        fd.restitution  =   restitution;
        fd.isSensor     =   isSensor;
        auto  info = new SHAPE_INFO(controller, iskinematicBody ? b2_kinematicBody : b2_dynamicBody);
        const mbm::INFO_PHYSICS* infoPhysics = controller->getInfoPhysics();
        if(infoPhysics == nullptr)
        {
            delete info;
            return nullptr;
        }
        if(infoPhysics->lsCube.size())
        {
            const std::vector<CUBE*>::size_type sizeSubset = infoPhysics->lsCube.size();
            for(std::vector<CUBE*>::size_type i = 0; i < sizeSubset; ++i)
            {
                const CUBE* cube = infoPhysics->lsCube[i];
                b2PolygonShape  dynamicBox;
                fd.shape        = &dynamicBox; //-V506
                b2Vec2 center(  cube->absCenter.x * controller->scale.x * this->scalePercentage,
                                cube->absCenter.y * controller->scale.y * this->scalePercentage);
                dynamicBox.SetAsBox(cube->halfDim.x * controller->scale.x * this->scalePercentage * reduceX,
                                    cube->halfDim.y * controller->scale.y * this->scalePercentage * reduceY,
                    center,0);
                if(info->body)
                {
                    info->body->CreateFixture(&fd);
                    continue;
                }
                b2BodyDef       bodyDef;
                bodyDef.bullet = isBullet;
                bodyDef.type = iskinematicBody ? b2_kinematicBody : b2_dynamicBody;
                bodyDef.position.Set(info->ptr->position.x * this->scalePercentage,info->ptr->position.y * this->scalePercentage);
                info->body = world->CreateBody(&bodyDef);
                info->body->CreateFixture(&fd);
                interference(info);
            }
        }
        if(infoPhysics->lsSphere.size())
        {
            const std::vector<SPHERE*>::size_type sizeSubset = infoPhysics->lsSphere.size();
            for(std::vector<SPHERE*>::size_type i = 0; i < sizeSubset; ++i)
            {
                const mbm::SPHERE* sphere = infoPhysics->lsSphere[i];
                b2CircleShape   shape;
                fd.shape        = &shape; //-V506
                shape.m_radius  = sphere->ray * controller->scale.x * this->scalePercentage  * reduceX;
                shape.m_p.Set(  sphere->absCenter[0] * controller->scale.x * this->scalePercentage,
                                sphere->absCenter[1] * controller->scale.y * this->scalePercentage);
                if(info->body)
                {
                    info->body->CreateFixture(&fd);
                    continue;
                }
                b2BodyDef       bodyDef;
                bodyDef.bullet = isBullet;
                bodyDef.type = iskinematicBody ? b2_kinematicBody : b2_dynamicBody;
                bodyDef.position.Set(   info->ptr->position.x * this->scalePercentage,
                                        info->ptr->position.y * this->scalePercentage);
                info->body = world->CreateBody(&bodyDef);
                info->body->CreateFixture(&fd);
                interference(info);
            }
        }
        if(infoPhysics->lsCubeComplex.size())
        {
            const std::vector<CUBE_COMPLEX*>::size_type sizeSubset = infoPhysics->lsCubeComplex.size();
            for(std::vector<CUBE_COMPLEX*>::size_type i = 0; i < sizeSubset; ++i)
            {
                const CUBE_COMPLEX* cube = infoPhysics->lsCubeComplex[i];
                b2PolygonShape  groundPolygn;
                b2Vec2 vertices[4];
                vertices[0].Set(cube->a.x * this->scalePercentage * reduceX * controller->scale.x,cube->a.y * this->scalePercentage * reduceY * controller->scale.y);
                vertices[1].Set(cube->b.x * this->scalePercentage * reduceX * controller->scale.x,cube->b.y * this->scalePercentage * reduceY * controller->scale.y);
                vertices[2].Set(cube->c.x * this->scalePercentage * reduceX * controller->scale.x,cube->c.y * this->scalePercentage * reduceY * controller->scale.y);
                vertices[3].Set(cube->d.x * this->scalePercentage * reduceX * controller->scale.x,cube->d.y * this->scalePercentage * reduceY * controller->scale.y);
                groundPolygn.Set(vertices, 4);
                                
                b2BodyDef       groundBodyDef;
                groundBodyDef.bullet = isBullet;
                fd.shape = &groundPolygn; //-V506
                groundBodyDef.type = iskinematicBody ? b2_kinematicBody : b2_dynamicBody;
                if(info->body)
                {
                    info->body->CreateFixture(&fd);
                    continue;
                }
                info->body = world->CreateBody(&groundBodyDef);
                info->body->CreateFixture(&fd);
                interference(info);
            }
        }
        if(infoPhysics->lsTriangle.size())
        {
            const std::vector<TRIANGLE*>::size_type sizeSubset = infoPhysics->lsTriangle.size();
            for(std::vector<TRIANGLE*>::size_type i = 0; i < sizeSubset; ++i)
            {
                const TRIANGLE* triangle = infoPhysics->lsTriangle[i];
                b2PolygonShape  groundTriangle;
                b2Vec2 vertices[3];
                vertices[0].Set(triangle->point[0].x* this->scalePercentage * reduceX * controller->scale.x,triangle->point[0].y* this->scalePercentage * reduceY * controller->scale.y);
                vertices[1].Set(triangle->point[1].x* this->scalePercentage * reduceX * controller->scale.x,triangle->point[1].y* this->scalePercentage * reduceY * controller->scale.y);
                vertices[2].Set(triangle->point[2].x* this->scalePercentage * reduceX * controller->scale.x,triangle->point[2].y* this->scalePercentage * reduceY * controller->scale.y);
                groundTriangle.Set(vertices, 3);
            
                b2BodyDef       groundBodyDef;
                groundBodyDef.bullet = isBullet;
                fd.shape = &groundTriangle; //-V506
                groundBodyDef.type = iskinematicBody ? b2_kinematicBody : b2_dynamicBody;
                if(info->body)
                {
                    info->body->CreateFixture(&fd);
                    continue;
                }
                info->body = world->CreateBody(&groundBodyDef);
                info->body->CreateFixture(&fd);
                interference(info);
            }
        }
        if(info->body == nullptr)
        {
            delete info;
            return nullptr;
        }
        this->lsShape.push_back(info);
        info->body->SetUserData(info);
        return info;
    }
    
    //CallBack - b2ContactListener
    void PHYSICS_BOX2D::BeginContact(b2Contact* contact)
    {
        if(this->on_box2d_BeginContact)
        {
            auto* info1 = static_cast<SHAPE_INFO*>(contact->GetFixtureA()->GetBody()->GetUserData());
            auto* info2 = static_cast<SHAPE_INFO*>(contact->GetFixtureB()->GetBody()->GetUserData());
            this->on_box2d_BeginContact(this,info1,info2);
        }
    }
    
    void PHYSICS_BOX2D::EndContact(b2Contact* contact)
    {
        if(this->on_box2d_EndContact)
        {
            auto* info1 = static_cast<SHAPE_INFO*>(contact->GetFixtureA()->GetBody()->GetUserData());
            auto* info2 = static_cast<SHAPE_INFO*>(contact->GetFixtureB()->GetBody()->GetUserData());
            this->on_box2d_EndContact(this,info1,info2);
        }
    }
    
    void PHYSICS_BOX2D::PreSolve(b2Contact* contact,const b2Manifold* oldManifold)
    {
        if(this->on_box2d_PreSolve)
        {
            auto* info1 = static_cast<SHAPE_INFO*>(contact->GetFixtureA()->GetBody()->GetUserData());
            auto* info2 = static_cast<SHAPE_INFO*>(contact->GetFixtureB()->GetBody()->GetUserData());
            this->on_box2d_PreSolve(this,info1,info2,oldManifold);
            
        }
    }
    
    void PHYSICS_BOX2D::PostSolve(b2Contact* contact, const b2ContactImpulse* impulse)
    {
        if(this->on_box2d_PostSolve)
        {
            auto* info1 = static_cast<SHAPE_INFO*>(contact->GetFixtureA()->GetBody()->GetUserData());
            auto* info2 = static_cast<SHAPE_INFO*>(contact->GetFixtureB()->GetBody()->GetUserData());
            this->on_box2d_PostSolve(this,info1,info2,impulse);
        }
    }

    void PHYSICS_BOX2D::safeDestroyBody(SHAPE_INFO* infoBox2d)
    {
        if(infoBox2d && infoBox2d->body)
        {
            for(std::vector<INFO_JOINT*>::size_type i = 0; i < this->lsJoint.size(); ++i)//must be size()
            {
                INFO_JOINT* infoJoint = this->lsJoint[i];
                if(infoJoint->infoA->ptr == infoBox2d->ptr)
                {
                    if(infoJoint->joint)
                        this->world->DestroyJoint(infoJoint->joint);
                    infoJoint->joint = nullptr;
                    delete infoJoint;
                    this->lsJoint.erase(this->lsJoint.begin() + i);
                    --i;
                }
            }
            for(std::vector<SHAPE_INFO*>::size_type i = 0; i < this->lsShape.size(); ++i)//must be size()
            {
                SHAPE_INFO* infoShape = this->lsShape[i];
                if(infoBox2d->ptr == infoShape->ptr)
                {
                    world->DestroyBody(infoShape->body);
                    infoShape->body = nullptr;
                    this->lsShape.erase(this->lsShape.begin() + i);
                    delete infoShape;
                    --i;
                }
            }
            infoBox2d= nullptr;
        }
    }

    void PHYSICS_BOX2D::safeRemoveJoint(SHAPE_INFO* infoBox2d)
    {
        if(infoBox2d && infoBox2d->body)
        {
            for(std::vector<INFO_JOINT*>::size_type i = 0; i < this->lsJoint.size(); ++i)//must be size()
            {
                INFO_JOINT* infoJoint = this->lsJoint[i];
                if(infoJoint->infoA->ptr == infoBox2d->ptr)
                {
                    if(infoJoint->joint)
                        this->world->DestroyJoint(infoJoint->joint);
                    infoJoint->joint = nullptr;
                    delete infoJoint;
                    this->lsJoint.erase(this->lsJoint.begin() + i);
                    --i;
                }
            }
        }
    }
};

