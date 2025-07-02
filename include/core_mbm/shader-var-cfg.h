/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef SHADERS_VAR_CFG_GLES_H
#define SHADERS_VAR_CFG_GLES_H

#include <string>
#include "core-exports.h"

namespace mbm
{

    enum TYPE_VAR_SHADER : char
    {
        VAR_FLOAT      = 1,
        VAR_VECTOR     = 2,
        VAR_VECTOR2    = 3,
        VAR_COLOR_RGB  = 4,
        VAR_COLOR_RGBA = 5
    };

    struct VAR_SHADER
    {
        TYPE_VAR_SHADER typeVar;
        int             handleVar;
        std::string     name;
        int             sizeVar;
        float           current[4];
        float           min[4];
        float           max[4];
        float           step[4];
        bool            control[4];
        bool            granThen[4]; // Test <=
        API_IMPL VAR_SHADER(const TYPE_VAR_SHADER TypeVar) noexcept;
        API_IMPL void set(const float newMin[4], const float newMax[4],const float timeAnim);
    };
}

#endif
