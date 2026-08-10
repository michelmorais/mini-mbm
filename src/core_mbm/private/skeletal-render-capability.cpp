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

#include "skeletal-render-capability.h"

#include <mutex>

namespace mbm::skeletal
{
    namespace
    {
        std::mutex capabilityMutex;
        GLES2_SKINNING_CAPABILITY measuredCapability;
    }

    GLES2_SKINNING_CAPABILITY calculateGles2SkinningCapability(const uint32_t maxVertexUniformVectors,
                                                               const uint32_t maxVertexAttributes) noexcept
    {
        GLES2_SKINNING_CAPABILITY result;
        result.maxVertexUniformVectors = maxVertexUniformVectors;
        result.maxVertexAttributes = maxVertexAttributes;
        result.hasRequiredVertexAttributes = maxVertexAttributes >= GLES2_SKELETAL_VERTEX_ATTRIBUTES;
        result.measured = maxVertexUniformVectors > 0 && maxVertexAttributes > 0;
        if (maxVertexUniformVectors > result.reservedVertexUniformVectors &&
            result.hasRequiredVertexAttributes)
        {
            const uint32_t available = maxVertexUniformVectors - result.reservedVertexUniformVectors;
            result.lbsMatrixPaletteBones = available / GLES2_LBS_VECTORS_PER_BONE;
            result.dqsRigidPaletteBones = available / GLES2_DQS_VECTORS_PER_BONE;
        }
        return result;
    }

    void setMeasuredGles2SkinningCapability(const uint32_t maxVertexUniformVectors,
                                            const uint32_t maxVertexAttributes) noexcept
    {
        const GLES2_SKINNING_CAPABILITY measured =
            calculateGles2SkinningCapability(maxVertexUniformVectors, maxVertexAttributes);
        std::lock_guard<std::mutex> lock(capabilityMutex);
        measuredCapability = measured;
    }

    GLES2_SKINNING_CAPABILITY getMeasuredGles2SkinningCapability() noexcept
    {
        std::lock_guard<std::mutex> lock(capabilityMutex);
        return measuredCapability;
    }
}
