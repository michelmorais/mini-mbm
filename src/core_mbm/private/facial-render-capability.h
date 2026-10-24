/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                             |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                    |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE  |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef FACIAL_RENDER_CAPABILITY_H
#define FACIAL_RENDER_CAPABILITY_H

#include <cstdint>

namespace mbm::facial
{
    struct GPU_MORPH_CAPABILITY
    {
        uint32_t maxVertexTextureUnits = 0;
        uint32_t maxTextureSize = 0;
        bool hasFloatTexture = false;
        bool supportsIndexedDeltas = false;
        bool measured = false;
    };

    GPU_MORPH_CAPABILITY calculateGpuMorphCapability(uint32_t maxVertexTextureUnits,
                                                       uint32_t maxTextureSize,
                                                       bool hasFloatTexture) noexcept;
    void setMeasuredGpuMorphCapability(uint32_t maxVertexTextureUnits,
                                       uint32_t maxTextureSize,
                                       bool hasFloatTexture) noexcept;
    GPU_MORPH_CAPABILITY getMeasuredGpuMorphCapability() noexcept;
}

#endif
