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

#ifndef SKELETAL_GPU_LBS_H
#define SKELETAL_GPU_LBS_H

#include "skeletal-animation-foundation.h"
#include "skeletal-render-capability.h"
#include <core_mbm/shader.h>

#include <vector>

namespace mbm::skeletal
{
    enum class GLES2_LBS_PREPARATION_STATUS : uint8_t
    {
        NO_SKELETAL_DATA,
        READY,
        CAPABILITY_UNAVAILABLE,
        INSUFFICIENT_VERTEX_ATTRIBUTES,
        PALETTE_TOO_LARGE,
        INVALID_CANONICAL_DATA
    };

    struct GPU_LBS_VERTEX
    {
        float boneIndex[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        float weight[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    };

    struct GLES2_LBS_INPUT
    {
        GLES2_LBS_PREPARATION_STATUS status = GLES2_LBS_PREPARATION_STATUS::NO_SKELETAL_DATA;
        uint32_t requiredBoneCount = 0;
        uint32_t effectiveBoneCapacity = 0;
        uint32_t lbsBoneCapacity = 0;
        uint32_t dqsBoneCapacity = 0;
        std::vector<GPU_LBS_VERTEX> vertices;
        const char *diagnostic = "no-skeletal-data";

        bool ready() const noexcept { return status == GLES2_LBS_PREPARATION_STATUS::READY; }
        bool supports(SKELETAL_SHADER_METHOD method) const noexcept
        {
            if (!ready()) return false;
            const uint32_t capacity = method == SKELETAL_SHADER_METHOD::DQS_RIGID
                ? dqsBoneCapacity : lbsBoneCapacity;
            return requiredBoneCount <= capacity;
        }
    };

    enum class GLES2_LBS_PALETTE_STATUS : uint8_t
    {
        READY,
        INVALID_POSE,
        UNSUPPORTED_NORMAL_TRANSFORM
    };

    enum class GLES2_DQS_PALETTE_STATUS : uint8_t
    {
        READY,
        INVALID_POSE,
        UNSUPPORTED_NON_RIGID_TRANSFORM
    };

    enum class DQS_COMPATIBILITY_STATUS : uint8_t
    {
        RIGID,
        BIND_CONTAINS_SCALE,
        CLIP_CONTAINS_SCALE
    };

    DQS_COMPATIBILITY_STATUS getDqsCompatibility(const CANONICAL_SKELETON &skeleton,
                                                  const CANONICAL_ANIMATIONS &animations) noexcept;
    const char *dqsCompatibilityStatusName(DQS_COMPATIBILITY_STATUS status) noexcept;

    GLES2_LBS_PREPARATION_STATUS prepareGles2LbsInput(const CANONICAL_SKELETON &skeleton,
                                                       const CANONICAL_WEIGHTS &weights,
                                                       const SKINNING_CAPABILITY &capability,
                                                       GLES2_LBS_INPUT &out) noexcept;
    const char *gles2LbsPreparationStatusName(GLES2_LBS_PREPARATION_STATUS status) noexcept;
    GLES2_LBS_PALETTE_STATUS buildGles2LbsPalette(const CANONICAL_SKELETON &skeleton,
                                                   const SKELETAL_POSE &pose,
                                                   bool requireCompactNormalTransform,
                                                   std::vector<float> &outRows) noexcept;
    GLES2_LBS_PALETTE_STATUS sampleGles2LbsPalette(const CANONICAL_SKELETON &skeleton,
                                                    const SKELETAL_CLIP &clip, float time,
                                                    bool requireCompactNormalTransform,
                                                    std::vector<float> &outRows,
                                                    SKELETAL_POSE *outPose = nullptr) noexcept;
    GLES2_DQS_PALETTE_STATUS buildGles2DqsPalette(const CANONICAL_SKELETON &skeleton,
                                                   const SKELETAL_POSE &pose,
                                                   std::vector<float> &outRows) noexcept;
    GLES2_DQS_PALETTE_STATUS sampleGles2DqsPalette(const CANONICAL_SKELETON &skeleton,
                                                    const SKELETAL_CLIP &clip, float time,
                                                    std::vector<float> &outRows,
                                                    SKELETAL_POSE *outPose = nullptr) noexcept;
}

#endif
