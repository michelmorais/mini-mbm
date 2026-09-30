/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef NORMAL_MAP_UPLOAD_H
#define NORMAL_MAP_UPLOAD_H

#include <cstdint>
#include <core_mbm/render-features.h>

namespace mbm
{
    class BUFFER_GL;
    struct VEC2;
    struct VEC3;

    namespace normal_map
    {
        struct PREPARED;

#if USE_NORMAL_MAPPING_3D
        // Main-thread backend hooks for validated static tangent batches.
        // Backends without tangent rendering accept the upload as a no-op so
        // prepared assets still load. Success does not imply rendering support.
        bool uploadStatic(BUFFER_GL *buffer, const VEC3 *positions, const VEC3 *normals,
                          const VEC2 *uv, const PREPARED &prepared);
        void setRenderSettings(BUFFER_GL *buffer, uint32_t subset, int greenSign, float strength);
#endif
    }
}
#endif
