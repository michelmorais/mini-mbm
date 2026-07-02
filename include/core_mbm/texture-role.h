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

#ifndef TEXTURE_ROLE_H
#define TEXTURE_ROLE_H

// Single, dependency-free definition of "what kind of texture is this" - shared by the runtime
// shader/texture layer (shader.h) and the v11 mesh file format (header-mesh.h's
// SUBSET_EXTRA_SLOT_V11 / SUBSET_DESC_V11). Kept tiny and standalone so header-mesh.h (deliberately
// lean: stdint.h, vector, string, primitives.h, core-exports.h) never has to pull in shader.h's
// heavier include graph (particle-control.h, TEXTURE/RENDERIZABLE forward decls) just to reference
// a texture role byte.

#include <stdint.h>

namespace mbm
{
    enum TEXTURE_ROLE : uint8_t
    {
        TEXTURE_ROLE_DIFFUSE = 0,
        TEXTURE_ROLE_ANIMATION_EFFECT = 1,
        TEXTURE_ROLE_NORMAL = 2,
        TEXTURE_ROLE_SPECULAR = 3,
        TEXTURE_ROLE_EMISSIVE = 4,
        TEXTURE_ROLE_MASK = 5
    };
}

#endif
