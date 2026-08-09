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
        BIND_IDENTITY_MISMATCH
    };

    struct DIAGNOSTIC
    {
        DIAGNOSTIC_CODE code = DIAGNOSTIC_CODE::NON_FINITE_TRANSFORM;
        uint32_t sourceIndex = 0;
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

    MATRIX buildTrsMatrix(const LOCAL_TRANSFORM &transform) noexcept;
    bool decomposeTrsMatrix(const MATRIX &matrix, LOCAL_TRANSFORM &out, bool &hasNegativeScale,
                            bool &hasShear) noexcept;
    float maximumMatrixDifference(const MATRIX &left, const MATRIX &right) noexcept;
    float matrixComparisonTolerance(const MATRIX &left, const MATRIX &right) noexcept;
    const char *diagnosticCodeName(DIAGNOSTIC_CODE code) noexcept;
    bool compileLegacySkeleton(const std::vector<util::SKELETON_BONE_V11> &legacy,
                               COMPILED_SKELETON &out);
}

#endif
