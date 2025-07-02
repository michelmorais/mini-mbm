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

#include <shapes.h>
#include <header-mesh.h>
#include <cstring>

namespace mbm
{
	void _VEC3_POINT::set(const float _x, const float _y, const float _z) noexcept
    {
        x = _x;
        y = _y;
        z = _z;
    }
    void _VEC3_POINT::set(const VEC3 &vec) noexcept
    {
        x = vec.x;
        y = vec.y;
        z = vec.z;
    }
    _VEC3_POINT::operator VEC3() noexcept
    {
        return VEC3(x, y, z);
    }
    _VEC3_POINT::operator VEC3 *() noexcept
    {
        return reinterpret_cast<VEC3 *>(&x);
    }
    SPHERE::SPHERE() noexcept
    {
        ray = 0;
        memset(absCenter, 0, sizeof(absCenter));
    }

    CUBE_COMPLEX::CUBE_COMPLEX() noexcept
    {
        this->a.set(-1, -1, 1.0f);
        this->b.set(-1, 1, 1.0f);
        this->c.set(1, 1, 1.0f);
        this->d.set(1, -1, 1.0f);

        this->e.set(-1, -1, -1.0f);
        this->f.set(-1, 1, -1.0f);
        this->g.set(1, 1, -1.0f);
        this->h.set(1, -1, -1.0f);
    }
    CUBE_COMPLEX::CUBE_COMPLEX(const float s) noexcept //:a(0,0,0),b(0,0,0),c(0,0,0),d(0,0,0),e(0,0,0),f(0,0,0),g(0,0,0),h(0,0,0)
    {
        this->a.set(-1, -1, 1.0f);
        this->b.set(-1, 1, 1.0f);
        this->c.set(1, 1, 1.0f);
        this->d.set(1, -1, 1.0f);

        this->e.set(-1, -1, -1.0f);
        this->f.set(-1, 1, -1.0f);
        this->g.set(1, 1, -1.0f);
        this->h.set(1, -1, -1.0f);
        for (auto & i : this->p)
        {
            i.x *= s;
            i.y *= s;
            i.z *= s;
        }
    }
    CUBE_COMPLEX::CUBE_COMPLEX(VEC3 &s) noexcept
    {
        this->a.set(-1, -1, 1.0f);
        this->b.set(-1, 1, 1.0f);
        this->c.set(1, 1, 1.0f);
        this->d.set(1, -1, 1.0f);

        this->e.set(-1, -1, -1.0f);
        this->f.set(-1, 1, -1.0f);
        this->g.set(1, 1, -1.0f);
        this->h.set(1, -1, -1.0f);
        for (auto & i : this->p)
        {
            i.x *= s.x;
            i.y *= s.y;
            i.z *= s.z;
        }
    }
    CUBE_COMPLEX::CUBE_COMPLEX(VEC3 &vmin, VEC3 &vmax) noexcept
    {
        this->a.set(vmin);
        this->b.set(vmin.x, vmax.y, vmin.z);
        this->c.set(vmax.x, vmax.y, vmin.z);
        this->d.set(vmax.x, vmin.y, vmin.z);

        this->e.set(vmin.x, vmin.y, vmax.z);
        this->f.set(vmin.x, vmax.y, vmax.z);
        this->g.set(vmax);
        this->h.set(vmax.x, vmin.y, vmax.z);
    }
    void CUBE_COMPLEX::getBounds(VEC2 &vmax, VEC2 &vmin) const noexcept
    {
        for (auto i : this->p)
        {
            if (i.x > vmax.x)
                vmax.x = i.x;
            if (i.y > vmax.y)
                vmax.y = i.y;

            if (i.x < vmin.x)
                vmin.x = i.x;
            if (i.y < vmin.y)
                vmin.y = i.y;
        }
    }
    void CUBE_COMPLEX::getBounds(VEC3 &vmax, VEC3 &vmin) const noexcept
    {
        for (auto i : this->p)
        {
            if (i.x > vmax.x)
                vmax.x = i.x;
            if (i.y > vmax.y)
                vmax.y = i.y;
            if (i.z > vmax.z)
                vmax.z = i.z;

            if (i.x < vmin.x)
                vmin.x = i.x;
            if (i.y < vmin.y)
                vmin.y = i.y;
            if (i.z < vmin.z)
                vmin.z = i.z;
        }
    }

    TRIANGLE::TRIANGLE()
    = default;
    void TRIANGLE::getBounds(VEC2 &vmax, VEC2 &vmin) const noexcept
    {
        for (const auto & i : this->point)
        {
            if (i.x > vmax.x)
                vmax.x = i.x;
            if (i.y > vmax.y)
                vmax.y = i.y;

            if (i.x < vmin.x)
                vmin.x = i.x;
            if (i.y < vmin.y)
                vmin.y = i.y;
        }
    }
    void TRIANGLE::getBounds(VEC3 &vmax, VEC3 &vmin) const noexcept
    {
        for (const auto & i : this->point)
        {
            if (i.x > vmax.x)
                vmax.x = i.x;
            if (i.y > vmax.y)
                vmax.y = i.y;
            if (i.z > vmax.z)
                vmax.z = i.z;

            if (i.x < vmin.x)
                vmin.x = i.x;
            if (i.y < vmin.y)
                vmin.y = i.y;
            if (i.z < vmin.z)
                vmin.z = i.z;
        }
    }

    LETTER::LETTER() noexcept
    {
        detail = nullptr;
    }
    LETTER::~LETTER() noexcept
    {
        if (detail)
            delete detail;
    }

    INFO_BOUND_FONT::INFO_BOUND_FONT() noexcept
    {
        spaceXCharacter = 0;
        spaceYCharacter = 0;
        heightLetter    = 0;
        #if defined USE_EDITOR_FEATURES
        memset(letterDiffY,0,sizeof(letterDiffY));
        memset(letterDiffX,0,sizeof(letterDiffX));
        #endif
    }
    void INFO_BOUND_FONT::release()
    {
        spaceXCharacter = 0;
        spaceYCharacter = 0;
        heightLetter    = 0;
        for (auto & i : letter)
        {
            util::DETAIL_LETTER *sub = i.detail;
            if (sub)
                delete sub;
            i.detail = nullptr;
        }
    }
    INFO_BOUND_FONT::~INFO_BOUND_FONT() noexcept
    {
        release();
    }
}
