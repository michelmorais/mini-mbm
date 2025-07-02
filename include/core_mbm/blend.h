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

#ifndef BLEND_STATE_CLASS_H
#define BLEND_STATE_CLASS_H

#include "core-exports.h"

namespace mbm
{

    enum BLEND_OPENGLES
    {
        BLEND_DISABLE      = 0,
        BLEND_ZERO         = 1,
        BLEND_ONE          = 2,
        BLEND_SRCCOLOR     = 3,
        BLEND_INVSRCCOLOR  = 4,
        BLEND_SRCALPHA     = 5,
        BLEND_INVSRCALPHA  = 6,
        BLEND_DESTALPHA    = 7,
        BLEND_INVDESTALPHA = 8,
        BLEND_DESTCOLOR    = 9,
        BLEND_INVDESTCOLOR = 10
    };

    struct API_IMPL RENDER_STATE
    {
        RENDER_STATE() noexcept = default;
        const char *getDesc(const BLEND_OPENGLES blendState) const noexcept;
        void set(const BLEND_OPENGLES blendState) const noexcept;
    };
}

#endif
