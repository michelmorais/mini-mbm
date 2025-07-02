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


#ifndef BULLET_INFO_SHAPE_3D_H
#define BULLET_INFO_SHAPE_3D_H

#include <btBulletCollisionCommon.h>
#include <btBulletDynamicsCommon.h>

namespace mbm
{
    class RENDERIZABLE;
    struct VEC3;
    
    class SHAPE_INFO_3D : public btMotionState
    {
    public:
        RENDERIZABLE*                           ptr;
        btCompoundShape *                       compound;
        btAlignedObjectArray<btCollisionShape*> lsCollisionShapes;
        btRigidBody*                            body;
        const bool                              isStaticBody;
        const float*                            scale;
        SHAPE_INFO_3D(RENDERIZABLE* ptrMesh,const bool _isStatiBody,const float* _scale);
        virtual ~SHAPE_INFO_3D();
        void    getWorldTransform(btTransform& worldTrans)const;
        void    setWorldTransform(const btTransform& worldTrans);
        void    quaternionToEuler(btQuaternion *quat, VEC3 *euler);
    };

};

#endif
