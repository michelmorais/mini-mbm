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
#include <algorithm>

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
    // Was: derived size from `2 * max(|absCenter-halfDim|, |absCenter+halfDim|)` for CUBE/SPHERE --
    // only correct when absCenter==(0,0,0). For any off-center shape (an intentionally offset
    // hitbox, or the auto-generated single CUBE fillAtLeastOneBound() bakes for any mesh with no
    // hand-authored physics, whose absCenter is the mesh's own true center and is almost never
    // (0,0,0) for anything not pivoted at its own visual middle) this silently inflated the
    // reported size -- for a mesh anchored at one edge (absCenter on that axis exactly equals
    // halfDim on that axis), the formula returns exactly 2x the true dimension on that axis. Also
    // did not union across shape kinds when a mesh had more than one (each `if(sN)` block
    // overwrote the previous kind's *w/*h/*d rather than merging). Now derived from
    // getBoundsMinMax(), the same true-min/max accumulation CUBE_COMPLEX/TRIANGLE already used
    // correctly, applied uniformly and merged across every kind present. See MBM_VERSION 6.9.0.
    bool INFO_PHYSICS::getBounds(float *w, float *h) const
    {
        *w = 0;
        *h = 0;
        VEC2 vmin, vmax;
        if (!this->getBoundsMinMax(vmin, vmax))
            return false;
        *w = vmax.x - vmin.x;
        *h = vmax.y - vmin.y;
        return true;
    }
    bool INFO_PHYSICS::getBounds(float *w, float *h, float *d) const
    {
        *w = 0;
        *h = 0;
        *d = 0;
        VEC3 vmin, vmax;
        if (!this->getBoundsMinMax(vmin, vmax))
            return false;
        *w = vmax.x - vmin.x;
        *h = vmax.y - vmin.y;
        *d = vmax.z - vmin.z;
        return true;
    }

    // True accumulated min/max across every shape kind present, properly unioned. getBounds()
    // above derives its size from this directly; this function additionally exposes the min/max
    // themselves, which is what lets a caller (RENDERIZABLE::updateAABB()) derive a true AABB
    // CENTER, not just a size -- see MBM_VERSION 6.9.0. CUBE/SPHERE derive min/max directly from
    // absCenter+-halfDim/ray (their own stored data is already the true bound); CUBE_COMPLEX/
    // TRIANGLE reuse their own already-correct getBounds(vmax,vmin).
    bool INFO_PHYSICS::getBoundsMinMax(VEC2 &vmin, VEC2 &vmax) const
    {
        vmin = VEC2(FLT_MAX, FLT_MAX);
        vmax = VEC2(-FLT_MAX, -FLT_MAX);
        bool ret = false;
        for (const auto cube : this->lsCube)
        {
            vmin.x = std::min(vmin.x, cube->absCenter.x - cube->halfDim.x);
            vmin.y = std::min(vmin.y, cube->absCenter.y - cube->halfDim.y);
            vmax.x = std::max(vmax.x, cube->absCenter.x + cube->halfDim.x);
            vmax.y = std::max(vmax.y, cube->absCenter.y + cube->halfDim.y);
            ret = true;
        }
        for (const auto sphere : this->lsSphere)
        {
            vmin.x = std::min(vmin.x, sphere->absCenter[0] - sphere->ray);
            vmin.y = std::min(vmin.y, sphere->absCenter[1] - sphere->ray);
            vmax.x = std::max(vmax.x, sphere->absCenter[0] + sphere->ray);
            vmax.y = std::max(vmax.y, sphere->absCenter[1] + sphere->ray);
            ret = true;
        }
        for (const auto complex : this->lsCubeComplex)
        {
            complex->getBounds(vmax, vmin);
            ret = true;
        }
        for (const auto triangle : this->lsTriangle)
        {
            triangle->getBounds(vmax, vmin);
            ret = true;
        }
        return ret;
    }

    bool INFO_PHYSICS::getBoundsMinMax(VEC3 &vmin, VEC3 &vmax) const
    {
        vmin = VEC3(FLT_MAX, FLT_MAX, FLT_MAX);
        vmax = VEC3(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        bool ret = false;
        for (const auto cube : this->lsCube)
        {
            vmin.x = std::min(vmin.x, cube->absCenter.x - cube->halfDim.x);
            vmin.y = std::min(vmin.y, cube->absCenter.y - cube->halfDim.y);
            vmin.z = std::min(vmin.z, cube->absCenter.z - cube->halfDim.z);
            vmax.x = std::max(vmax.x, cube->absCenter.x + cube->halfDim.x);
            vmax.y = std::max(vmax.y, cube->absCenter.y + cube->halfDim.y);
            vmax.z = std::max(vmax.z, cube->absCenter.z + cube->halfDim.z);
            ret = true;
        }
        for (const auto sphere : this->lsSphere)
        {
            vmin.x = std::min(vmin.x, sphere->absCenter[0] - sphere->ray);
            vmin.y = std::min(vmin.y, sphere->absCenter[1] - sphere->ray);
            vmin.z = std::min(vmin.z, sphere->absCenter[2] - sphere->ray);
            vmax.x = std::max(vmax.x, sphere->absCenter[0] + sphere->ray);
            vmax.y = std::max(vmax.y, sphere->absCenter[1] + sphere->ray);
            vmax.z = std::max(vmax.z, sphere->absCenter[2] + sphere->ray);
            ret = true;
        }
        for (const auto complex : this->lsCubeComplex)
        {
            complex->getBounds(vmax, vmin);
            ret = true;
        }
        for (const auto triangle : this->lsTriangle)
        {
            triangle->getBounds(vmax, vmin);
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
            memcpy(to->point->v,from->point,sizeof(to->point));
            to->position = from->position;
            this->lsTriangle.emplace_back(to);
        }
        return (this->lsCube.size() > 0 || this->lsTriangle.size() > 0 || this->lsSphere.size() > 0 || this->lsCubeComplex.size() > 0);
    }

}

