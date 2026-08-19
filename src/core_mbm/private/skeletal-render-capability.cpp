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

#include <algorithm>
#include <mutex>

namespace mbm::skeletal
{
    namespace
    {
        std::mutex capabilityMutex;
        SKINNING_CAPABILITY measuredCapability;
    }

    SKINNING_CAPABILITY calculateSkinningCapability(const uint32_t maxVertexShaderVectors,
                                                    const uint32_t maxVertexAttributes) noexcept
    {
        SKINNING_CAPABILITY result;
        result.maxVertexShaderVectors = maxVertexShaderVectors;
        result.maxVertexAttributes = maxVertexAttributes;
        result.hasRequiredVertexAttributes = maxVertexAttributes >= GPU_SKELETAL_VERTEX_ATTRIBUTES;
        result.measured = maxVertexShaderVectors > 0 && maxVertexAttributes > 0;
        if (maxVertexShaderVectors > result.reservedVertexShaderVectors &&
            result.hasRequiredVertexAttributes)
        {
            const uint32_t available = maxVertexShaderVectors - result.reservedVertexShaderVectors;
            result.lbsMatrixPaletteBones = std::min(
                available / GPU_LBS_VECTORS_PER_BONE, GPU_MAX_BONES_PER_DRAW);
            result.dqsRigidPaletteBones = std::min(
                available / GPU_DQS_VECTORS_PER_BONE, GPU_MAX_BONES_PER_DRAW);
        }
        return result;
    }

    void setMeasuredSkinningCapability(const uint32_t maxVertexShaderVectors,
                                       const uint32_t maxVertexAttributes) noexcept
    {
        const SKINNING_CAPABILITY measured =
            calculateSkinningCapability(maxVertexShaderVectors, maxVertexAttributes);
        std::lock_guard<std::mutex> lock(capabilityMutex);
        measuredCapability = measured;
    }

    SKINNING_CAPABILITY getMeasuredSkinningCapability() noexcept
    {
        std::lock_guard<std::mutex> lock(capabilityMutex);
        return measuredCapability;
    }
}
