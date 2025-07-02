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

#include <physics.h>
#include <shapes.h>
#include <cfloat>

namespace mbm
{

    PHYSICS::PHYSICS(const int _idScene) noexcept :
        enablePhysics(true),
        userData(nullptr),
        userData3d(nullptr),
        idScene(_idScene)
    {
    }
    PHYSICS::~PHYSICS() noexcept = default;

    INFO_PHYSICS::INFO_PHYSICS() noexcept
    = default;
    INFO_PHYSICS::~INFO_PHYSICS() noexcept
    {
        this->release();
    }
    void INFO_PHYSICS::release()
    {
        for (auto cube : lsCube)
        {
            delete cube;
        }
        lsCube.clear();
        for (auto sphere : lsSphere)
        {
            delete sphere;
        }
        lsSphere.clear();
        for (auto cube : lsCubeComplex)
        {
            delete cube;
        }
        lsCubeComplex.clear();
        for (auto triangle : lsTriangle)
        {
            delete triangle;
        }
        lsTriangle.clear();
    }
    bool INFO_PHYSICS::getBounds(float *w, float *h) const
    {
        *w                     = 0;
        *h                     = 0;
        bool               ret = false;
        const size_t s1  = this->lsCube.size();
        const size_t s2  = this->lsSphere.size();
        const size_t s3  = this->lsCubeComplex.size();
        const size_t s4  = this->lsTriangle.size();
        if (s1)
        {
            float w1 = 0, h1 = 0;
            for(size_t i = 0; i < s1; ++i)
            {
                const CUBE* cube = this->lsCube[i];
                const float x1 = std::abs(cube->absCenter.x - cube->halfDim.x);
                const float x2 = std::abs(cube->absCenter.x + cube->halfDim.x);
                const float y1 = std::abs(cube->absCenter.y - cube->halfDim.y);
                const float y2 = std::abs(cube->absCenter.y + cube->halfDim.y);
                if(x1 > w1)
                    w1  =   x1;
                if(x2 > w1)
                    w1  =   x2;
                if(y1 > h1)
                    h1  =   y1;
                if(y2 > h1)
                    h1  =   y2;
            }
            *w = w1 * 2.0f;
            *h = h1 * 2.0f;
            ret = true;
        }
        if (s2)
        {
            for(size_t i = 0; i < s2; ++i)
            {
                const mbm::SPHERE* sphere = this->lsSphere[i];
                const float x1 = std::abs(sphere->absCenter[0] - sphere->ray);
                const float x2 = std::abs(sphere->absCenter[0] + sphere->ray);
                const float y1 = std::abs(sphere->absCenter[1] - sphere->ray);
                const float y2 = std::abs(sphere->absCenter[1] + sphere->ray);
                if(x1 > *w)
                    *w = x1;
                if(x2 > *w)
                    *w = x2;
                if(y1 > *h)
                    *h = y1;
                if(y2 > *h)
                    *h = y2;
            }
            *w *= 2.0f;
            *h *= 2.0f;
            ret = true;
        }
        if (s3)
        {
            VEC2 vmax(-FLT_MAX, -FLT_MAX);
            VEC2 vmin(FLT_MAX, FLT_MAX);
            for (size_t i = 0; i < s3; ++i)
            {
                const mbm::CUBE_COMPLEX *complex = this->lsCubeComplex[i];
                complex->getBounds(vmax,vmin);
            }
            *w = vmax.x - vmin.x;
            *h = vmax.y - vmin.y;
            ret = true;
        }
        if (s4)
        {
            VEC2 vmax(-FLT_MAX, -FLT_MAX);
            VEC2 vmin(FLT_MAX, FLT_MAX);
            for (size_t i = 0; i < s4; ++i)
            {
                const mbm::TRIANGLE *triangle = this->lsTriangle[i];
                triangle->getBounds(vmax, vmin);
            }
            *w = vmax.x - vmin.x;
            *h = vmax.y - vmin.y;
            ret = true;
        }
        return ret;
    }
    bool INFO_PHYSICS::getBounds(float *w, float *h, float *d) const
    {
        *w                     = 0;
        *h                     = 0;
        *d                     = 0;
        bool               ret = false;
        const size_t s1  = this->lsCube.size();
        const size_t s2  = this->lsSphere.size();
        const size_t s3  = this->lsCubeComplex.size();
        const size_t s4  = this->lsTriangle.size();
        if (s1)
        {
            float w1 = 0,h1 = 0, d1 = 0;
            for(size_t i = 0; i < s1; ++i)
            {
                const CUBE* cube = this->lsCube[i];
                const float x1 = std::abs(cube->absCenter.x - cube->halfDim.x);
                const float x2 = std::abs(cube->absCenter.x + cube->halfDim.x);
                const float y1 = std::abs(cube->absCenter.y - cube->halfDim.y);
                const float y2 = std::abs(cube->absCenter.y + cube->halfDim.y);
                const float z1 = std::abs(cube->absCenter.z - cube->halfDim.z);
                const float z2 = std::abs(cube->absCenter.z + cube->halfDim.z);
                if(x1 > w1)
                    w1  =   x1;
                if(x2 > w1)
                    w1  =   x2;
                if(y1 > h1)
                    h1  =   y1;
                if(y2 > h1)
                    h1  =   y2;
                if(z1 > d1)
                    d1  =   z1;
                if(z2 > d1)
                    d1  =   z2;
            }
            *w = w1 * 2.0f;
            *h = h1 * 2.0f;
            *d = d1 * 2.0f;
            ret = true;
        }
        if (s2)
        {
            for(size_t i = 0; i < s2; ++i)
            {
                const mbm::SPHERE* sphere = this->lsSphere[i];
                const float x1 = std::abs(sphere->absCenter[0] - sphere->ray);
                const float x2 = std::abs(sphere->absCenter[0] + sphere->ray);
                const float y1 = std::abs(sphere->absCenter[1] - sphere->ray);
                const float y2 = std::abs(sphere->absCenter[1] + sphere->ray);
                const float z1 = std::abs(sphere->absCenter[2] - sphere->ray);
                const float z2 = std::abs(sphere->absCenter[2] + sphere->ray);
                if(x1 > *w)
                    *w = x1;
                if(x2 > *w)
                    *w = x2;
                if(y1 > *h)
                    *h = y1;
                if(y2 > *h)
                    *h = y2;
                if(z1 > *d)
                    *d = z1;
                if(z2 > *d)
                    *d = z2;
            }
            ret = true;
        }
        if (s3)
        {
            VEC3 vmax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
            VEC3 vmin(FLT_MAX, FLT_MAX, FLT_MAX);
            for (size_t i = 0; i < s3; ++i)
            {
                const mbm::CUBE_COMPLEX *complex = this->lsCubeComplex[i];
                complex->getBounds(vmax,vmin);
            }
            *w = vmax.x - vmin.x;
            *h = vmax.y - vmin.y;
            *d = vmax.z - vmin.z;
            ret = true;
        }
        if (s4)
        {
            VEC3 vmax(-FLT_MAX, -FLT_MAX,-FLT_MAX);
            VEC3 vmin(FLT_MAX, FLT_MAX, FLT_MAX);
            for (size_t i = 0; i < s4; ++i)
            {
                const mbm::TRIANGLE *triangle = this->lsTriangle[i];
                triangle->getBounds(vmax,vmin);
            }
            *w = vmax.x - vmin.x;
            *h = vmax.y - vmin.y;
            *d = vmax.z - vmin.z;
            ret = true;
        }
        return ret;
    }

    bool INFO_PHYSICS::clone(const INFO_PHYSICS* const other)
    {
        if(other  == nullptr)
            return false;
        this->release();
        for( const auto from : other->lsCube)
        {
            mbm::CUBE* to = new mbm::CUBE();
            to->absCenter = from->absCenter;
            to->halfDim   = from->halfDim;
            this->lsCube.emplace_back(to);
        }
        for( const auto from : other->lsCubeComplex)
        {
            mbm::CUBE_COMPLEX* to = new mbm::CUBE_COMPLEX();
            memcpy(to->p,from->p,sizeof(to->p));
            this->lsCubeComplex.emplace_back(to);
        }
        for( const auto from : other->lsSphere)
        {
            mbm::SPHERE* to = new mbm::SPHERE();
            memcpy(to->absCenter,from->absCenter,sizeof(to->absCenter));
            to->ray = from->ray;
            this->lsSphere.emplace_back(to);
        }
        for( const auto from : other->lsTriangle)
        {
            mbm::TRIANGLE* to = new mbm::TRIANGLE();
            memcpy(to->point,from->point,sizeof(to->point));
            to->position = from->position;
            this->lsTriangle.emplace_back(to);
        }
        return (this->lsCube.size() > 0 || this->lsTriangle.size() > 0 || this->lsSphere.size() > 0 || this->lsCubeComplex.size() > 0);
    }

}

