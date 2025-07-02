
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

#ifndef FRUSTUM_H
#define FRUSTUM_H

#include "primitives.h"
#include "scene.h"
#include "shapes.h"
#include "core-exports.h"

namespace mbm
{
    class RENDERIZABLE;

    class API_IMPL FRUSTUM
    {
    
      public:
        PLANE planeFromCurrentFrustum[6]; // 6 Planos Do Frustum Indicando Onde O Frustum Da Câmera Está
        float zNear;
        float zFar;
        FRUSTUM() noexcept;
        virtual ~FRUSTUM();
        bool isPointAtTheFrustum(const VEC3 &position) noexcept;
        bool isPointAtTheFrustum(const float x, const float y, const float z) noexcept;
        bool isCubeAtFrustum(const VEC3 &position, const VEC3 &scale, const CUBE &cube) noexcept;
        bool isSphereAtFrustum(const VEC3 &position, const float ray) noexcept;
        bool isSphereAtFrustum(const SPHERE &sphere) noexcept;

      protected:
        bool updateFrustum(const MATRIX *matrixView, const MATRIX *matrixProj);
    };

    class API_IMPL IS_ON_FRUSTUM : public COMMON_DEVICE
    {
      public:
        IS_ON_FRUSTUM(const RENDERIZABLE *_renderizable) noexcept; 
        bool isOnFrustum(const bool is3d, const bool is2ds) const noexcept;
        virtual ~IS_ON_FRUSTUM() noexcept;
      private:
        const RENDERIZABLE *renderizable;
    };
}
#endif
