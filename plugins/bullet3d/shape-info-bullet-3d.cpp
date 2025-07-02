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

#include "shape-info-bullet-3d.h"
#include <core_mbm/renderizable.h>
#include <core_mbm/util.h>
#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>


namespace mbm
{
    SHAPE_INFO_3D::SHAPE_INFO_3D(RENDERIZABLE* ptrMesh,const bool _isStatiBody,const float* _scale):isStaticBody(_isStatiBody),scale(_scale)
    {
        this->ptr               =   ptrMesh;
        this->compound          =   nullptr;
        this->body              =   nullptr;
    }
        
    SHAPE_INFO_3D::~SHAPE_INFO_3D()
    {
        if (this->body)
            delete this->body;
        this->body = nullptr;
        if (this->compound)
            delete this->compound;
        this->compound = nullptr;
        for (int i = 0; i < this->lsCollisionShapes.size(); ++i)
        {
            btCollisionShape* shape = this->lsCollisionShapes[i];
            this->lsCollisionShapes[i] = nullptr;
            if (shape)
            {
                delete shape;
            }
        }
        this->lsCollisionShapes.clear();
    }
        
    void SHAPE_INFO_3D::getWorldTransform(btTransform& worldTrans)const
    {
        //const btVector3 pos(myObject->position.x * (*this->scaleWorld),
        //                  myObject->position.y * (*this->scaleWorld),
        //                  myObject->position.z * (*this->scaleWorld));
        //
        const btVector3 pos(ptr->position.x * (*scale),ptr->position.y * (*scale),ptr->position.z * (*scale));
        const btQuaternion QuatAroundX = btQuaternion(btVector3(1.0, 0.0, 0.0), ptr->angle.x);
        const btQuaternion QuatAroundY = btQuaternion(btVector3(0.0, 1.0, 0.0), ptr->angle.y);
        const btQuaternion QuatAroundZ = btQuaternion(btVector3(0.0, 0.0, 1.0), ptr->angle.z);
        const btQuaternion rotation = QuatAroundX * QuatAroundY * QuatAroundZ;
        worldTrans.setIdentity();
        worldTrans.setOrigin(pos);
        worldTrans.setRotation(rotation);
    }
        
    //Bullet only calls the update of worldtransform for active objects
    void    SHAPE_INFO_3D::setWorldTransform(const btTransform& worldTrans)
    {
        btQuaternion rotation(worldTrans.getRotation());
        const btVector3 origin = worldTrans.getOrigin();
        ptr->position.x = origin.getX() /  (*scale);
        ptr->position.y = origin.getY() /  (*scale);
        ptr->position.z = origin.getZ() /  (*scale);
        this->quaternionToEuler(&rotation, &ptr->angle);
    }
        
    void SHAPE_INFO_3D::quaternionToEuler(btQuaternion *quat, VEC3 *euler)
    {
        float matrix[3][3];
        float cx, cy;
        float sx, sy, yr;
        float cz, sz;
        matrix[1][2] = 0;
        matrix[0][0] = 1.0f - 2.0f * (quat->y() * quat->y() + quat->z() * quat->z());
        matrix[0][1] = (2.0f * quat->x() * quat->y()) - (2.0f * quat->w() * quat->z());
        matrix[1][0] = 2.0f * (quat->x() * quat->y() + quat->w() * quat->z());
        matrix[1][1] = 1.0f - (2.0f * quat->x() * quat->x()) - (2.0f * quat->z() * quat->z());
        matrix[2][0] = 2.0f * (quat->x() * quat->z() - quat->w() * quat->y());
        matrix[2][1] = 2.0f * (quat->y() * quat->z() + quat->w() * quat->x());
        matrix[2][2] = 1.0f - 2.0f * (quat->x() * quat->x() - quat->y() * quat->y());
        sy = -matrix[2][0];
        cy = sqrt(1 - (sy * sy));
        yr = atan2f(sy, cy);
        euler->y = util::degreeToRadian((yr * 180.0f) / static_cast<float>(M_PI));
        if (sy != 1.0f && sy != -1.0f) //-V550
        {
            cx = matrix[2][2] / cy;
            sx = matrix[2][1] / cy;
            euler->x = util::degreeToRadian((atan2f(sx, cx) * 180.0f) / static_cast<float>(M_PI));

            cz = matrix[0][0] / cy;
            sz = matrix[1][0] / cy;
            euler->z = util::degreeToRadian((atan2f(sz, cz) * 180.0f) / static_cast<float>(M_PI));
        }
        else
        {
            cx = matrix[1][1];
            sx = -matrix[1][2];
            euler->x = util::degreeToRadian((atan2f(sx, cx) * 180.0f) / static_cast<float>(M_PI));
            cz = 1.0f;
            sz = 0.0f;
            euler->z = util::degreeToRadian((atan2f(sz, cz) * 180.0f) / static_cast<float>(M_PI));
        }
    }
};
