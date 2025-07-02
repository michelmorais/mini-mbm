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

#include <order-render.h>

namespace mbm

{
    ORDER_RENDER::ORDER_RENDER() noexcept : nextZOrderControl3d(0.0f),
                                        nextZOrderControl2d(0.0f),
                                        nextZOrderControl2dBackground(200.0f),
                                        Z2dsFont(-9.0f),
                                        Z2ds(0.1f),
                                        Z2dw(0.0f)
    {
    }
    void ORDER_RENDER::reInit() noexcept
    {
        this->nextZOrderControl3d           = 0.0f;
        this->nextZOrderControl2d           = 0.0f;
        this->nextZOrderControl2dBackground = 200.0f;
        this->Z2dsFont                      = -9.0f;
        this->Z2ds                          = 0.1f;
        this->Z2dw                          = 0.0f;
    }
    float ORDER_RENDER::getNextZOrderControl3d() noexcept
    {
        this->nextZOrderControl3d += 0.01f;
        return this->nextZOrderControl3d;
    }
    float ORDER_RENDER::getNextZOrderControl2d(const bool is2dS, const bool isFont) noexcept
    {
        if (is2dS && isFont)
        {
            this->Z2dsFont += 0.1f;
            return Z2dsFont;
        }
        else if (is2dS)
        {
            this->Z2ds -= 0.1f;
            return this->Z2ds;
        }
        else
        {
            this->Z2dw -= 0.1f;
            return this->Z2dw;
        }
    }
    float ORDER_RENDER::getNextZOrderControl2dBackground() noexcept
    {
        this->nextZOrderControl2dBackground -= 0.1f;
        return this->nextZOrderControl2dBackground;
    }

}
