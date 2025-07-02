/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT);                                                                                                     |
| Copyright (C); 2020      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                      |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software");, to deal in the Software without restriction, including without limitation       |
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

#ifndef TILED_LAYER_HEADER_H
#define TILED_LAYER_HEADER_H

#include <string>
#include <memory>
#include <map>

#include <core_mbm/primitives.h>
#include <core_mbm/dynamic-var.h>
#include <core_mbm/shader.h>

#include "brick.h"

namespace mbm
{
    struct LAYER
    {
        LAYER():
            visible(true),
            tint_min(0.0f,0.0f,0.0f,0.0f),
            tint_max(0.0f,0.0f,0.0f,0.0f),
            fTimeTint(1.0f),
            typeTint(TYPE_ANIMATION_GROWING)
            {}

        bool                        visible;
        COLOR                       tint_min,tint_max;
        float                       fTimeTint;
        TYPE_ANIMATION              typeTint;
        VEC3                        offset;//z is layer deep
        FX					        fx;
        std::vector<std::shared_ptr<BRICK>>         bricks;
        std::map<std::string,std::shared_ptr<DYNAMIC_VAR>>  properties;

        bool createFx();
    };
}

#endif