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
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef SKELETAL_RENDER_CAPABILITY_H
#define SKELETAL_RENDER_CAPABILITY_H

#include <cstdint>

namespace mbm::skeletal
{
    constexpr uint32_t GLES2_RESERVED_VERTEX_UNIFORM_VECTORS = 8;
    constexpr uint32_t GLES2_LBS_VECTORS_PER_BONE = 3;
    constexpr uint32_t GLES2_DQS_VECTORS_PER_BONE = 2;
    constexpr uint32_t GLES2_SKELETAL_VERTEX_ATTRIBUTES = 5;

    struct GLES2_SKINNING_CAPABILITY
    {
        uint32_t maxVertexUniformVectors = 0;
        uint32_t maxVertexAttributes = 0;
        uint32_t reservedVertexUniformVectors = GLES2_RESERVED_VERTEX_UNIFORM_VECTORS;
        uint32_t lbsMatrixPaletteBones = 0;
        uint32_t dqsRigidPaletteBones = 0;
        bool hasRequiredVertexAttributes = false;
        bool measured = false;
    };

    GLES2_SKINNING_CAPABILITY calculateGles2SkinningCapability(uint32_t maxVertexUniformVectors,
                                                               uint32_t maxVertexAttributes) noexcept;
    void setMeasuredGles2SkinningCapability(uint32_t maxVertexUniformVectors,
                                            uint32_t maxVertexAttributes) noexcept;
    GLES2_SKINNING_CAPABILITY getMeasuredGles2SkinningCapability() noexcept;
}

#endif
