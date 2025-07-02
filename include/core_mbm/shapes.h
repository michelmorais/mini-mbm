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

#ifndef SHAPES_H
#define SHAPES_H

#include "core-exports.h"
#include "primitives.h"
#include <string>

#if defined _WIN32
	#pragma warning(push)
	#pragma warning(disable : 4201) // nonstandard extension used : nameless struct/union
#endif 

namespace util 
{
    struct DETAIL_LETTER;
}

namespace mbm
{
    struct API_IMPL CUBE
    {
      public:
        VEC3 halfDim;
        VEC3 absCenter;
        constexpr CUBE(const VEC3 &halfDim_, const VEC3 &absCenter_) noexcept : halfDim(halfDim_),absCenter(absCenter_)
        {
        }
        constexpr CUBE() noexcept : halfDim(0.5f, 0.5f, 0.5f), absCenter(0, 0, 0)
        {
        }
        constexpr CUBE(const int value) noexcept : halfDim(static_cast<float>(value), static_cast<float>(value), static_cast<float>(value)),absCenter(0.0f, 0.0f, 0.0f)
        {
        }
        constexpr CUBE(const float value) noexcept : halfDim(value, value, value), absCenter(0, 0, 0)
        {
        }
        constexpr CUBE(const float w, const float h, const float d) noexcept : halfDim(w * 0.5f, h * 0.5f, d * 0.5f),absCenter(0, 0, 0)
        {
        }
        inline void operator=(float value) noexcept
        {
            halfDim = std::move(VEC3(static_cast<float>(value), static_cast<float>(value), static_cast<float>(value)));
        }
        inline void operator=(int value) noexcept
        {
            halfDim = std::move(VEC3(static_cast<float>(value), static_cast<float>(value), static_cast<float>(value)));
        }
        
        inline bool collision(const VEC3 &position, const VEC3 &other, const CUBE &cube) noexcept
	    {	
		    if((other.x + cube.absCenter.x + cube.halfDim.x) < //Max x
			    (position.x + absCenter.x - halfDim.x))//Min x
			    return false; 
		    if((other.x + cube.absCenter.x - cube.halfDim.x) > //Min x
			    (position.x + absCenter.x + halfDim.x))//Max x
			    return false; 

		    if((other.y + cube.absCenter.y + cube.halfDim.y) < //Max y
			    (position.y + absCenter.y - halfDim.y))//Min y
			    return false; 
		    if((other.y + cube.absCenter.y - cube.halfDim.y) > //Min y
			    (position.y + absCenter.y + halfDim.y))//Max y
			    return false; 

		    if((other.z + cube.absCenter.z + cube.halfDim.z) < //Max z
			    (position.z + absCenter.z - halfDim.z))//Min z
			    return false; 
		    if((other.z + cube.absCenter.z - cube.halfDim.z) > //Min z
			    (position.z + absCenter.z + halfDim.z))//Max z
			    return false; 
		
		    return true;
	    }

	    inline bool collision2d(const VEC3 &position, const VEC3 &other, const CUBE &cube) noexcept
	    {	
		    if((other.x + cube.absCenter.x + cube.halfDim.x) < //Max x
			    (position.x + absCenter.x - halfDim.x))//Min x
			    return false; 
		    if((other.x + cube.absCenter.x - cube.halfDim.x) > //Min x
			    (position.x + absCenter.x + halfDim.x))//Max x
			    return false; 

		    if((other.y + cube.absCenter.y + cube.halfDim.y) < //Max y
			    (position.y + absCenter.y - halfDim.y))//Min y
			    return false; 
		    if((other.y + cube.absCenter.y - cube.halfDim.y) > //Min y
			    (position.y + absCenter.y + halfDim.y))//Max y
			    return false; 

		    return true;
	    }

        inline bool collision2d(const VEC2 &position, const VEC3 &other, const CUBE &cube) noexcept
        {
            const VEC3 p(position.x,position.y,0.0f);
            return collision2d(p,other,cube);
        }

        inline bool collision2d(const VEC3 &position, const VEC2 &other, const CUBE &cube) noexcept
        {
            const VEC3 p(other.x,other.y,0.0f);
            return collision2d(position,p,cube);
        }
    };

    struct _VEC3_POINT
    {
        float x,y,z;
        API_IMPL void set(const float _x, const float _y, const float _z) noexcept;
        API_IMPL void set(const VEC3 &vec)noexcept;
        API_IMPL operator VEC3()noexcept;
        API_IMPL operator VEC3 *()noexcept;
    };

    struct API_IMPL SPHERE
    {
        float ray;
        float absCenter[3];
        SPHERE()noexcept;
    };

    struct API_IMPL CUBE_COMPLEX // bounding cube type physics number 3 (individual cube points)
    {
      public:
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
        union {
            struct
            {
                _VEC3_POINT a, b, c, d, e, f, g, h;
            };
            struct
            {
                _VEC3_POINT p[8];
            };
        };
        CUBE_COMPLEX()noexcept;
        CUBE_COMPLEX(const float s)noexcept;
        CUBE_COMPLEX(VEC3 &s)noexcept;
        CUBE_COMPLEX(VEC3 &vmin, VEC3 &vmax)noexcept;
        void getBounds(VEC2 &vmax, VEC2 &vmin) const noexcept;
        void getBounds(VEC3 &vmax, VEC3 &vmin) const noexcept;
    };

    struct API_IMPL TRIANGLE
    {
        VEC3 point[3];
        VEC2 position;
        TRIANGLE();
        void getBounds(VEC2 &vmax, VEC2 &vmin) const noexcept;
        void getBounds(VEC3 &vmax, VEC3 &vmin) const noexcept;
    };

    struct API_IMPL RAY
    {
        VEC3 origin;
        VEC3 direction;
    };

    struct API_IMPL LETTER
    {
        util::DETAIL_LETTER *detail;
        LETTER()noexcept;
        ~LETTER()noexcept;
    };


    struct INFO_BOUND_FONT
    {
        std::string      fontName;
        int16_t          spaceXCharacter;
        int16_t          spaceYCharacter;
        uint16_t heightLetter;
        LETTER             letter[255];
        #if defined USE_EDITOR_FEATURES
        float letterDiffY[255];
        float letterDiffX[255];
        #endif
        API_IMPL INFO_BOUND_FONT()noexcept;
        API_IMPL void release();
        API_IMPL ~INFO_BOUND_FONT()noexcept;
    };
}

#if defined _WIN32
	#pragma warning (pop)//nonstandard extension used : nameless struct/union
#endif 

#endif
