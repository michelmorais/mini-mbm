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

#ifndef PHYSICS_CLASS_H
#define PHYSICS_CLASS_H

#include "core-exports.h"
#include "primitives.h"
#include <vector>

namespace mbm
{

    class RENDERIZABLE;
    struct CUBE;
    struct CUBE_COMPLEX;
    struct SPHERE;
    struct TRIANGLE;
    struct INFO_BOUND_FONT;
    struct USER_DATA_PHYSICS_2D;
    struct USER_DATA_PHYSICS_3D;

    class PHYSICS
    {
        friend class CORE_MANAGER;
      public:
        bool  enablePhysics;
        USER_DATA_PHYSICS_2D * userData;
        USER_DATA_PHYSICS_3D * userData3d;
        API_IMPL PHYSICS(const int idScene)noexcept;
        API_IMPL virtual ~PHYSICS() noexcept;
        API_IMPL virtual void removeObject(RENDERIZABLE *ptr)               = 0;
        API_IMPL virtual void removeObjectByIdSceneScene(const int idScene) = 0;
      protected:
        virtual void update(const float fps,const float delta) = 0;
        int idScene;
    };

    struct INFO_PHYSICS
    {
        std::vector<CUBE *>         lsCube;
        std::vector<CUBE_COMPLEX *> lsCubeComplex;
        std::vector<SPHERE *>       lsSphere;
        std::vector<TRIANGLE *>     lsTriangle;
        
        API_IMPL INFO_PHYSICS()noexcept;
        API_IMPL ~INFO_PHYSICS()noexcept;
        API_IMPL void release();
        API_IMPL bool clone(const INFO_PHYSICS* const other);
        // Total size only (derived from getBoundsMinMax() below: max-min on each axis). Prior to
        // MBM_VERSION 6.9.0 this instead assumed CUBE/SPHERE shapes were centered at local origin
        // (wrong whenever absCenter != 0 -- see getBoundsMinMax's own comment) and did not union
        // bounds across shape kinds when more than one was present on the same mesh; both are
        // fixed now that this derives from the same true min/max getBoundsMinMax computes.
        API_IMPL bool getBounds(float *w, float *h) const ;
        API_IMPL bool getBounds(float *w, float *h, float *d) const ;
        // True accumulated min/max across every shape kind present (cube, sphere, cube-complex,
        // triangle), properly unioned. Unlike getBounds() above, this also exposes the min/max
        // themselves, not just their difference -- letting a caller derive the AABB's true CENTER,
        // not just its size (see RENDERIZABLE::getBoundingAABBCenterOffset(), MBM_VERSION 6.9.0).
        // Returns false if there are no physics shapes at all.
        API_IMPL bool getBoundsMinMax(VEC2 &vmin, VEC2 &vmax) const ;
        API_IMPL bool getBoundsMinMax(VEC3 &vmin, VEC3 &vmax) const ;
    };

}

#endif
