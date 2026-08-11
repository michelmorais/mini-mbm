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

#include "skeletal-gpu-lbs.h"

#include <cmath>
#include <utility>

namespace mbm::skeletal
{
    GLES2_LBS_PREPARATION_STATUS prepareGles2LbsInput(const CANONICAL_SKELETON &skeleton,
                                                       const CANONICAL_WEIGHTS &weights,
                                                       const GLES2_SKINNING_CAPABILITY &capability,
                                                       GLES2_LBS_INPUT &out) noexcept
    {
        out = {};
        out.requiredBoneCount = static_cast<uint32_t>(skeleton.compiled.bones.size());
        out.effectiveBoneCapacity = capability.lbsMatrixPaletteBones;
        if (skeleton.skeletonId == 0 && weights.skeletonId == 0)
            return out.status;
        if (!capability.measured)
            return out.status = GLES2_LBS_PREPARATION_STATUS::CAPABILITY_UNAVAILABLE;
        if (!capability.hasRequiredVertexAttributes)
            return out.status = GLES2_LBS_PREPARATION_STATUS::INSUFFICIENT_VERTEX_ATTRIBUTES;
        if (out.requiredBoneCount > out.effectiveBoneCapacity)
            return out.status = GLES2_LBS_PREPARATION_STATUS::PALETTE_TOO_LARGE;
        if (skeleton.skeletonId == 0 || weights.skeletonId != skeleton.skeletonId ||
            !validateCanonicalWeights(skeleton, weights, static_cast<uint32_t>(weights.vertices.size())))
            return out.status = GLES2_LBS_PREPARATION_STATUS::INVALID_CANONICAL_DATA;

        std::vector<int32_t> paletteBoneIndices(weights.paletteBoneIds.size(), -1);
        for (size_t paletteIndex = 0; paletteIndex < weights.paletteBoneIds.size(); ++paletteIndex)
        {
            const auto found = skeleton.compiled.indexById.find(weights.paletteBoneIds[paletteIndex]);
            if (found == skeleton.compiled.indexById.end())
                return out.status = GLES2_LBS_PREPARATION_STATUS::INVALID_CANONICAL_DATA;
            paletteBoneIndices[paletteIndex] = found->second;
        }

        out.vertices.resize(weights.vertices.size());
        for (size_t vertexIndex = 0; vertexIndex < weights.vertices.size(); ++vertexIndex)
        {
            const CANONICAL_VERTEX_WEIGHT &source = weights.vertices[vertexIndex];
            GPU_LBS_VERTEX &target = out.vertices[vertexIndex];
            for (uint32_t slot = 0; slot < 4; ++slot)
            {
                if (source.paletteIndex[slot] == UINT16_MAX)
                    continue;
                if (source.paletteIndex[slot] >= paletteBoneIndices.size())
                {
                    out = {};
                    out.status = GLES2_LBS_PREPARATION_STATUS::INVALID_CANONICAL_DATA;
                    return out.status;
                }
                target.boneIndex[slot] = static_cast<float>(paletteBoneIndices[source.paletteIndex[slot]]);
                target.weight[slot] = source.weight[slot];
            }
        }
        return out.status = GLES2_LBS_PREPARATION_STATUS::READY;
    }

    const char *gles2LbsPreparationStatusName(const GLES2_LBS_PREPARATION_STATUS status) noexcept
    {
        switch (status)
        {
            case GLES2_LBS_PREPARATION_STATUS::NO_SKELETAL_DATA: return "no-skeletal-data";
            case GLES2_LBS_PREPARATION_STATUS::READY: return "ready";
            case GLES2_LBS_PREPARATION_STATUS::CAPABILITY_UNAVAILABLE: return "capability-unavailable";
            case GLES2_LBS_PREPARATION_STATUS::INSUFFICIENT_VERTEX_ATTRIBUTES: return "insufficient-vertex-attributes";
            case GLES2_LBS_PREPARATION_STATUS::PALETTE_TOO_LARGE: return "palette-too-large";
            case GLES2_LBS_PREPARATION_STATUS::INVALID_CANONICAL_DATA: return "invalid-canonical-data";
        }
        return "unknown";
    }

    GLES2_LBS_PALETTE_STATUS buildGles2LbsPalette(const CANONICAL_SKELETON &skeleton,
                                                   const SKELETAL_POSE &pose,
                                                   const bool requireCompactNormalTransform,
                                                   std::vector<float> &outRows) noexcept
    {
        outRows.clear();
        const size_t boneCount = skeleton.compiled.bones.size();
        if (skeleton.skeletonId == 0 || boneCount == 0 || pose.globalTransforms.size() != boneCount)
            return GLES2_LBS_PALETTE_STATUS::INVALID_POSE;
        outRows.resize(boneCount * 12u);
        for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
        {
            MATRIX skinMatrix;
            MatrixMultiply(&skinMatrix,
                           &skeleton.compiled.bones[boneIndex].inverseGlobalBindMatrix,
                           &pose.globalTransforms[boneIndex]);
            if (requireCompactNormalTransform)
            {
                LOCAL_TRANSFORM decomposed;
                bool hasNegativeScale = false, hasShear = false;
                if (!decomposeTrsMatrix(skinMatrix, decomposed, hasNegativeScale, hasShear) ||
                    hasNegativeScale || hasShear ||
                    std::fabs(decomposed.scale.x - decomposed.scale.y) > MATRIX_TOLERANCE ||
                    std::fabs(decomposed.scale.x - decomposed.scale.z) > MATRIX_TOLERANCE)
                {
                    outRows.clear();
                    return GLES2_LBS_PALETTE_STATUS::UNSUPPORTED_NORMAL_TRANSFORM;
                }
            }
            float *rows = &outRows[boneIndex * 12u];
            // The engine transforms row vectors. GLSL's dot-based decoder therefore receives the
            // three output columns, including row-vector translation from _41/_42/_43.
            rows[0] = skinMatrix._11; rows[1] = skinMatrix._21;
            rows[2] = skinMatrix._31; rows[3] = skinMatrix._41;
            rows[4] = skinMatrix._12; rows[5] = skinMatrix._22;
            rows[6] = skinMatrix._32; rows[7] = skinMatrix._42;
            rows[8] = skinMatrix._13; rows[9] = skinMatrix._23;
            rows[10] = skinMatrix._33; rows[11] = skinMatrix._43;
        }
        return GLES2_LBS_PALETTE_STATUS::READY;
    }

    GLES2_LBS_PALETTE_STATUS sampleGles2LbsPalette(const CANONICAL_SKELETON &skeleton,
                                                    const SKELETAL_CLIP &clip, const float time,
                                                    const bool requireCompactNormalTransform,
                                                    std::vector<float> &outRows,
                                                    SKELETAL_POSE *outPose) noexcept
    {
        SKELETAL_POSE sampled;
        if (!sampleSkeletalClip(skeleton.compiled, clip, time, sampled))
        {
            outRows.clear();
            return GLES2_LBS_PALETTE_STATUS::INVALID_POSE;
        }
        const GLES2_LBS_PALETTE_STATUS status = buildGles2LbsPalette(
            skeleton, sampled, requireCompactNormalTransform, outRows);
        if (status == GLES2_LBS_PALETTE_STATUS::READY && outPose)
            *outPose = std::move(sampled);
        return status;
    }

    GLES2_DQS_PALETTE_STATUS buildGles2DqsPalette(const CANONICAL_SKELETON &skeleton,
                                                   const SKELETAL_POSE &pose,
                                                   std::vector<float> &outRows) noexcept
    {
        outRows.clear();
        const size_t boneCount = skeleton.compiled.bones.size();
        if (skeleton.skeletonId == 0 || boneCount == 0 || pose.globalTransforms.size() != boneCount)
            return GLES2_DQS_PALETTE_STATUS::INVALID_POSE;
        outRows.resize(boneCount * 8u);
        for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
        {
            MATRIX skinMatrix;
            MatrixMultiply(&skinMatrix,
                           &skeleton.compiled.bones[boneIndex].inverseGlobalBindMatrix,
                           &pose.globalTransforms[boneIndex]);
            DUAL_QUATERNION dualQuaternion;
            if (!rigidDualQuaternionFromMatrix(skinMatrix, dualQuaternion))
            {
                outRows.clear();
                return GLES2_DQS_PALETTE_STATUS::UNSUPPORTED_NON_RIGID_TRANSFORM;
            }
            float *rows = &outRows[boneIndex * 8u];
            rows[0] = dualQuaternion.real.x; rows[1] = dualQuaternion.real.y;
            rows[2] = dualQuaternion.real.z; rows[3] = dualQuaternion.real.w;
            rows[4] = dualQuaternion.dual.x; rows[5] = dualQuaternion.dual.y;
            rows[6] = dualQuaternion.dual.z; rows[7] = dualQuaternion.dual.w;
        }
        return GLES2_DQS_PALETTE_STATUS::READY;
    }

    GLES2_DQS_PALETTE_STATUS sampleGles2DqsPalette(const CANONICAL_SKELETON &skeleton,
                                                    const SKELETAL_CLIP &clip, const float time,
                                                    std::vector<float> &outRows,
                                                    SKELETAL_POSE *outPose) noexcept
    {
        SKELETAL_POSE sampled;
        if (!sampleSkeletalClip(skeleton.compiled, clip, time, sampled))
        {
            outRows.clear();
            return GLES2_DQS_PALETTE_STATUS::INVALID_POSE;
        }
        const GLES2_DQS_PALETTE_STATUS status = buildGles2DqsPalette(skeleton, sampled, outRows);
        if (status == GLES2_DQS_PALETTE_STATUS::READY && outPose)
            *outPose = std::move(sampled);
        return status;
    }
}
