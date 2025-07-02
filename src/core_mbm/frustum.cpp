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

#include <frustum.h>
#include <renderizable.h>
#include <device.h>

namespace mbm
{
enum FRUSTUM_SIDE
{
    RIGHT  = 0, // The RIGHT side of the frustum
    LEFT   = 1, // The LEFT  side of the frustum
    BOTTOM = 2, // The BOTTOM side of the frustum
    TOP    = 3, // The TOP side of the frustum
    BACK   = 4, // The BACK side of the frustum
    FRONT  = 5  // The FRONT side of the frustum
};

// Like above, instead of saying a number for the ABC and D of the plane, we
// want to be more descriptive.
enum PLANE_DATA
{
    A = 0, // The X value of the plane's normal
    B = 1, // The Y value of the plane's normal
    C = 2, // The Z value of the plane's normal
    D = 3  // The distance the plane is from the origin
};

FRUSTUM::FRUSTUM() noexcept
{}
    
    FRUSTUM::~FRUSTUM()
    = default;
    
    bool FRUSTUM::isPointAtTheFrustum(const VEC3 &position) noexcept
    {
        for (auto & m : planeFromCurrentFrustum)
        {
            if (PlaneDotCoord(&m, &position) < 0.0f)
                return false;
        }
        return true;
    }
    
    bool FRUSTUM::isPointAtTheFrustum(const float x, const float y, const float z) noexcept
    {
        for (auto & m : planeFromCurrentFrustum)
        {
            VEC3 pos(x, y, z);
            if (PlaneDotCoord(&m, &pos) < 0.0f)
                return false;
        }
        return true;
    }
    
    bool FRUSTUM::isCubeAtFrustum(const VEC3 &position, const VEC3 &scale, const CUBE &cube) noexcept
    {
        VEC3        pointOne;
        const float x = cube.halfDim.x * scale.x;
        const float y = cube.halfDim.y * scale.y;
        const float z = cube.halfDim.z * scale.z;
        for (auto & m : planeFromCurrentFrustum)
        {
            pointOne = VEC3(position.x + cube.absCenter.x - x, position.y + cube.absCenter.y - y,
                            position.z + cube.absCenter.z - z); // Esquerdo baixo frente
            if (PlaneDotCoord(&m, &pointOne) >= 0.0f)
                continue;

            pointOne = VEC3(position.x + cube.absCenter.x + x, position.y + cube.absCenter.y - y,
                            position.z + cube.absCenter.z - z); // Direito baixo frente
            if (PlaneDotCoord(&m, &pointOne) >= 0.0f)
                continue;
            pointOne = VEC3(position.x + cube.absCenter.x - x, position.y + cube.absCenter.y + y,
                            position.z + cube.absCenter.z - z); // Esquerda cima frente
            if (PlaneDotCoord(&m, &pointOne) >= 0.0f)
                continue;
            pointOne = VEC3(position.x + cube.absCenter.x + x, position.y + cube.absCenter.y + y,
                            position.z + cube.absCenter.z - z); // Direita cima frente
            if (PlaneDotCoord(&m, &pointOne) >= 0.0f)
                continue;
            pointOne = VEC3(position.x + cube.absCenter.x - x, position.y + cube.absCenter.y - y,
                            position.z + cube.absCenter.z + z); // Esquerdo baixo tras
            if (PlaneDotCoord(&m, &pointOne) >= 0.0f)
                continue;
            pointOne = VEC3(position.x + cube.absCenter.x + x, position.y + cube.absCenter.y - y,
                            position.z + cube.absCenter.z + z); // Direito baixo tras
            if (PlaneDotCoord(&m, &pointOne) >= 0.0f)
                continue;
            pointOne = VEC3(position.x + cube.absCenter.x - x, position.y + cube.absCenter.y + y,
                            position.z + cube.absCenter.z + z); // Esquerdo cima tras
            if (PlaneDotCoord(&m, &pointOne) >= 0.0f)
                continue;
            pointOne = VEC3(position.x + cube.absCenter.x + x, position.y + cube.absCenter.y + y,
                            position.z + cube.absCenter.z + z); // Direito cima tras
            if (PlaneDotCoord(&m, &pointOne) >= 0.0f)
                continue;
            return false;
        }
        return true;
    }
    
    bool FRUSTUM::isSphereAtFrustum(const VEC3 &position, const float ray) noexcept
    {
        for (auto & m : planeFromCurrentFrustum)
        {
            if (PlaneDotCoord(&m, &position) < -ray)
                return false;
        }
        return true;
    }
    
    bool FRUSTUM::isSphereAtFrustum(const SPHERE &sphere) noexcept
    {
        const VEC3 position(sphere.absCenter[0], sphere.absCenter[1], sphere.absCenter[2]);
        for (auto & m : planeFromCurrentFrustum)
        {
            if (PlaneDotCoord(&m, &position) < -sphere.ray)
                return false;
        }
        return true;
    }
    
    bool FRUSTUM::updateFrustum(const MATRIX *matrixView, const MATRIX *matrixProj)
    {
        MATRIX matrixFinal;
        MatrixMultiply(&matrixFinal, matrixView, matrixProj);

        planeFromCurrentFrustum[0].a = matrixFinal._14 + matrixFinal._13;
        planeFromCurrentFrustum[0].b = matrixFinal._24 + matrixFinal._23;
        planeFromCurrentFrustum[0].c = matrixFinal._34 + matrixFinal._33;
        planeFromCurrentFrustum[0].d = matrixFinal._44 + matrixFinal._43;
        PlaneNormalize(&planeFromCurrentFrustum[0], &planeFromCurrentFrustum[0]);

        planeFromCurrentFrustum[1].a = matrixFinal._14 - matrixFinal._13;
        planeFromCurrentFrustum[1].b = matrixFinal._24 - matrixFinal._23;
        planeFromCurrentFrustum[1].c = matrixFinal._34 - matrixFinal._33;
        planeFromCurrentFrustum[1].d = matrixFinal._44 - matrixFinal._43;
        PlaneNormalize(&planeFromCurrentFrustum[1], &planeFromCurrentFrustum[1]);

        planeFromCurrentFrustum[2].a = matrixFinal._14 + matrixFinal._11;
        planeFromCurrentFrustum[2].b = matrixFinal._24 + matrixFinal._21;
        planeFromCurrentFrustum[2].c = matrixFinal._34 + matrixFinal._31;
        planeFromCurrentFrustum[2].d = matrixFinal._44 + matrixFinal._41;
        PlaneNormalize(&planeFromCurrentFrustum[2], &planeFromCurrentFrustum[2]);

        planeFromCurrentFrustum[3].a = matrixFinal._14 - matrixFinal._11;
        planeFromCurrentFrustum[3].b = matrixFinal._24 - matrixFinal._21;
        planeFromCurrentFrustum[3].c = matrixFinal._34 - matrixFinal._31;
        planeFromCurrentFrustum[3].d = matrixFinal._44 - matrixFinal._41;
        PlaneNormalize(&planeFromCurrentFrustum[3], &planeFromCurrentFrustum[3]);

        planeFromCurrentFrustum[4].a = matrixFinal._14 - matrixFinal._12;
        planeFromCurrentFrustum[4].b = matrixFinal._24 - matrixFinal._22;
        planeFromCurrentFrustum[4].c = matrixFinal._34 - matrixFinal._32;
        planeFromCurrentFrustum[4].d = matrixFinal._44 - matrixFinal._42;
        PlaneNormalize(&planeFromCurrentFrustum[4], &planeFromCurrentFrustum[4]);

        planeFromCurrentFrustum[5].a = matrixFinal._14 + matrixFinal._12;
        planeFromCurrentFrustum[5].b = matrixFinal._24 + matrixFinal._22;
        planeFromCurrentFrustum[5].c = matrixFinal._34 + matrixFinal._32;
        planeFromCurrentFrustum[5].d = matrixFinal._44 + matrixFinal._42;
        PlaneNormalize(&planeFromCurrentFrustum[5], &planeFromCurrentFrustum[5]);
        return true;
    }

    IS_ON_FRUSTUM::IS_ON_FRUSTUM(const mbm::RENDERIZABLE *_renderizable) noexcept : renderizable(_renderizable)
    {
    }
    
    bool IS_ON_FRUSTUM::isOnFrustum(const bool is3d, const bool is2ds) const noexcept
    {
        float w = 0;
        float h = 0;
        if (is3d)
        {
            float d = 0;
            this->renderizable->getAABB(&w, &h, &d);
            const mbm::CUBE cube(w, h, d);
            const VEC3 scale(1,1,1);
            if (this->device->isCubeAtFrustum(renderizable->position, scale, cube))
                return true;
            return false;
        }
        else if (is2ds)
        {
            this->renderizable->getAABB(&w, &h);
            if (device->isRectangleScreen2dOnScreen2D_scaled(renderizable->position.x, renderizable->position.y, w, h))
                return true;
            return false;
        }
        else
        {
            this->renderizable->getAABB(&w, &h);
            if (device->isRectangleWorld2dOnScreen2D_scaled(renderizable->position.x, renderizable->position.y, w, h))
                return true;
            return false;
        }
    }

    IS_ON_FRUSTUM::~IS_ON_FRUSTUM() noexcept
    = default;
}
