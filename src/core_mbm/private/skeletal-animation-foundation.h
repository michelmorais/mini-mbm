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
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef SKELETAL_ANIMATION_FOUNDATION_H
#define SKELETAL_ANIMATION_FOUNDATION_H

#include <core_mbm/header-mesh.h>
#include <core_mbm/primitives.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace mbm::skeletal
{
    constexpr float QUATERNION_ZERO_EPSILON = 1.0e-8f;
    constexpr float MATRIX_TOLERANCE = 1.0e-5f;
    constexpr float SINGULAR_TOLERANCE = 1.0e-8f;
    constexpr float KEY_TIME_TOLERANCE = 1.0e-6f;

    struct QUATERNION
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float w = 1.0f;
    };

    struct LOCAL_TRANSFORM
    {
        VEC3 translation = VEC3(0.0f, 0.0f, 0.0f);
        QUATERNION rotation;
        VEC3 scale = VEC3(1.0f, 1.0f, 1.0f);
    };

    enum class DIAGNOSTIC_CODE
    {
        EMPTY_NAME,
        DUPLICATE_NAME,
        UNKNOWN_PARENT,
        NON_FINITE_TRANSFORM,
        SINGULAR_TRANSFORM,
        NEGATIVE_SCALE,
        SHEAR_NOT_SUPPORTED,
        ID_COLLISION,
        LOCAL_RECONSTRUCTION_MISMATCH,
        BIND_IDENTITY_MISMATCH,
        INVALID_BIND_QUATERNION,
        NON_UNIT_BIND_QUATERNION,
        EMPTY_PALETTE_NAME,
        DUPLICATE_PALETTE_NAME,
        UNKNOWN_WEIGHT_BONE,
        VERTEX_COUNT_MISMATCH,
        PALETTE_INDEX_OUT_OF_RANGE,
        NON_FINITE_WEIGHT,
        NEGATIVE_WEIGHT,
        UNUSED_SLOT_NONZERO,
        ZERO_WEIGHT_USED_SLOT,
        DUPLICATE_BONE_INFLUENCE,
        NO_EFFECTIVE_INFLUENCE,
        WEIGHT_SUM_MISMATCH,
        INVALID_CLIP_ID,
        EMPTY_CLIP_NAME,
        INVALID_CLIP_DURATION,
        UNKNOWN_TRACK_BONE,
        DUPLICATE_BONE_TRACK,
        INVALID_CHANNEL_MASK,
        EMPTY_TRACK_KEYS,
        INVALID_KEY_TIME,
        NON_INCREASING_KEY_TIME,
        NON_FINITE_KEY_TRANSFORM,
        INVALID_KEY_QUATERNION,
        NON_UNIT_KEY_QUATERNION,
        SINGULAR_KEY_SCALE,
        NON_UNIFORM_KEY_SCALE,
        NEGATIVE_KEY_SCALE,
        INVALID_EASING,
        INVALID_BEZIER_CONTROL,
        INVALID_SAMPLE_TIME
    };

    struct DIAGNOSTIC
    {
        DIAGNOSTIC_CODE code = DIAGNOSTIC_CODE::NON_FINITE_TRANSFORM;
        uint32_t sourceIndex = 0;
        uint32_t vertexIndex = UINT32_MAX;
        uint32_t keyIndex = UINT32_MAX;
        uint8_t slotIndex = UINT8_MAX;
        std::string boneName;
        float observedError = 0.0f;
        bool fatal = true;
    };

    struct COMPILED_BONE
    {
        uint64_t boneId = 0;
        uint64_t parentBoneId = 0;
        int32_t parentIndex = -1;
        uint32_t sourceIndex = 0;
        std::string name;
        LOCAL_TRANSFORM localBind;
        MATRIX localBindMatrix;
        MATRIX globalBindMatrix;
        MATRIX inverseGlobalBindMatrix;
        bool hasNegativeScale = false;
        bool hasShear = false;
    };

    struct COMPILED_SKELETON
    {
        std::vector<COMPILED_BONE> bones;
        std::vector<DIAGNOSTIC> diagnostics;
        std::unordered_map<std::string, int32_t> indexByName;
        std::unordered_map<uint64_t, int32_t> indexById;
        float maximumReconstructionError = 0.0f;
        float maximumBindIdentityError = 0.0f;

        bool hasFatalDiagnostics() const noexcept;
    };

    struct CANONICAL_BONE
    {
        uint64_t boneId = 0;
        uint64_t parentBoneId = 0;
        std::string name;
        LOCAL_TRANSFORM localBind;
        float radius = 0.0f;
        float length = 0.0f;
        // Bone-editor geometry in this bone's local bind space. Unlike an assumed +Y axis, this
        // survives arbitrary FBX coordinate-basis conversion and can be transformed by any pose.
        VEC3 tailOffset;
        bool hasExplicitTail = false;
        bool connectedToParent = false;
    };

    struct CANONICAL_SKELETON
    {
        uint64_t skeletonId = 0;
        std::vector<CANONICAL_BONE> sourceBones;
        COMPILED_SKELETON compiled;
    };

    struct CANONICAL_VERTEX_WEIGHT
    {
        uint16_t paletteIndex[4] = {UINT16_MAX, UINT16_MAX, UINT16_MAX, UINT16_MAX};
        float weight[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    };

    struct CANONICAL_WEIGHTS
    {
        uint64_t skeletonId = 0;
        uint32_t frameIndex = 0;
        std::vector<uint64_t> paletteBoneIds;
        std::vector<CANONICAL_VERTEX_WEIGHT> vertices;
    };


    enum SKELETAL_CHANNEL : uint8_t
    {
        SKELETAL_CHANNEL_TRANSLATION = 1,
        SKELETAL_CHANNEL_ROTATION = 2,
        SKELETAL_CHANNEL_SCALE = 4
    };

    enum class SKELETAL_EASING : uint8_t
    {
        LINEAR,
        EASE_IN,
        EASE_OUT,
        EASE_IN_OUT,
        SMOOTHSTEP,
        CUBIC_BEZIER
    };

    struct SKELETAL_KEY
    {
        float time = 0.0f;
        LOCAL_TRANSFORM local;
        SKELETAL_EASING easing = SKELETAL_EASING::LINEAR;
        float bezierX1 = 0.0f;
        float bezierY1 = 0.0f;
        float bezierX2 = 1.0f;
        float bezierY2 = 1.0f;
    };

    struct SKELETAL_TRACK
    {
        uint64_t boneId = 0;
        uint8_t channelMask = SKELETAL_CHANNEL_TRANSLATION | SKELETAL_CHANNEL_ROTATION | SKELETAL_CHANNEL_SCALE;
        std::vector<SKELETAL_KEY> keys;
    };

    struct SKELETAL_CLIP
    {
        uint64_t clipId = 0;
        std::string name;
        float duration = 0.0f;
        bool loop = false;
        std::vector<SKELETAL_TRACK> tracks;
    };

    struct CANONICAL_ANIMATIONS
    {
        uint64_t skeletonId = 0;
        std::vector<SKELETAL_CLIP> clips;
    };

    struct SKELETAL_POSE
    {
        std::vector<LOCAL_TRANSFORM> localTransforms;
        std::vector<MATRIX> globalTransforms;
    };

    struct DUAL_QUATERNION
    {
        QUATERNION real;
        QUATERNION dual;
    };

    MATRIX buildTrsMatrix(const LOCAL_TRANSFORM &transform) noexcept;
    bool decomposeTrsMatrix(const MATRIX &matrix, LOCAL_TRANSFORM &out, bool &hasNegativeScale,
                            bool &hasShear) noexcept;
    float maximumMatrixDifference(const MATRIX &left, const MATRIX &right) noexcept;
    float matrixComparisonTolerance(const MATRIX &left, const MATRIX &right) noexcept;
    bool rigidDualQuaternionFromMatrix(const MATRIX &matrix, DUAL_QUATERNION &out) noexcept;
    const char *diagnosticCodeName(DIAGNOSTIC_CODE code) noexcept;
    bool compileCanonicalSkeleton(const std::vector<CANONICAL_BONE> &source,
                                  COMPILED_SKELETON &out);
    bool validateCanonicalWeights(const CANONICAL_SKELETON &skeleton,
                                  const CANONICAL_WEIGHTS &weights,
                                  uint32_t expectedVertexCount) noexcept;
    bool validateCanonicalAnimations(const CANONICAL_SKELETON &skeleton,
                                     const CANONICAL_ANIMATIONS &animations) noexcept;
    bool buildUniformlyScaledCanonicalAsset(const CANONICAL_SKELETON &skeleton,
                                            const CANONICAL_ANIMATIONS &animations,
                                            float scale,
                                            CANONICAL_SKELETON &scaledSkeleton,
                                            CANONICAL_ANIMATIONS &scaledAnimations);
    bool validateSkeletalClip(const COMPILED_SKELETON &skeleton, const SKELETAL_CLIP &clip,
                              std::vector<DIAGNOSTIC> &diagnostics);
    bool sampleSkeletalClip(const COMPILED_SKELETON &skeleton, const SKELETAL_CLIP &clip,
                            float time, SKELETAL_POSE &out, std::vector<DIAGNOSTIC> *diagnostics = nullptr);
    bool skinVerticesLbsReference(const CANONICAL_SKELETON &skeleton, const CANONICAL_WEIGHTS &weights,
                                  const SKELETAL_POSE &pose, const std::vector<VEC3> &bindPositions,
                                  const std::vector<VEC3> &bindNormals, std::vector<VEC3> &outPositions,
                                  std::vector<VEC3> &outNormals) noexcept;
    bool skinVerticesDqsRigidReference(const CANONICAL_SKELETON &skeleton, const CANONICAL_WEIGHTS &weights,
                                       const SKELETAL_POSE &pose, const std::vector<VEC3> &bindPositions,
                                       const std::vector<VEC3> &bindNormals, std::vector<VEC3> &outPositions,
                                       std::vector<VEC3> &outNormals) noexcept;
}

#endif
