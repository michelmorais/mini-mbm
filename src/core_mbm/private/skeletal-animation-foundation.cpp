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

#include "skeletal-animation-foundation.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace mbm::skeletal
{
    namespace
    {
        constexpr float DEGREES_TO_RADIANS = 0.017453292519943295769f;

        bool isFinite(const float value) noexcept
        {
            return std::isfinite(value);
        }

        bool isFinite(const util::SKELETON_BONE_V11 &bone) noexcept
        {
            return isFinite(bone.x) && isFinite(bone.y) && isFinite(bone.z) && isFinite(bone.radius) &&
                   isFinite(bone.rotX) && isFinite(bone.rotY) && isFinite(bone.rotZ) &&
                   isFinite(bone.scaleX) && isFinite(bone.scaleY) && isFinite(bone.scaleZ) &&
                   isFinite(bone.length);
        }

        float vectorLength(const float x, const float y, const float z) noexcept
        {
            return std::sqrt(x * x + y * y + z * z);
        }

        float determinant3x3(const MATRIX &matrix) noexcept
        {
            return matrix._11 * (matrix._22 * matrix._33 - matrix._23 * matrix._32) -
                   matrix._12 * (matrix._21 * matrix._33 - matrix._23 * matrix._31) +
                   matrix._13 * (matrix._21 * matrix._32 - matrix._22 * matrix._31);
        }

        QUATERNION quaternionFromRowRotation(const MATRIX &rowRotation) noexcept
        {
            // Apply the standard column-matrix extraction to transpose(rowRotation).
            const float m00 = rowRotation._11, m01 = rowRotation._21, m02 = rowRotation._31;
            const float m10 = rowRotation._12, m11 = rowRotation._22, m12 = rowRotation._32;
            const float m20 = rowRotation._13, m21 = rowRotation._23, m22 = rowRotation._33;
            QUATERNION result;
            const float trace = m00 + m11 + m22;
            if (trace > 0.0f)
            {
                const float s = std::sqrt(trace + 1.0f) * 2.0f;
                result.w = 0.25f * s;
                result.x = (m21 - m12) / s;
                result.y = (m02 - m20) / s;
                result.z = (m10 - m01) / s;
            }
            else if (m00 > m11 && m00 > m22)
            {
                const float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
                result.w = (m21 - m12) / s;
                result.x = 0.25f * s;
                result.y = (m01 + m10) / s;
                result.z = (m02 + m20) / s;
            }
            else if (m11 > m22)
            {
                const float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
                result.w = (m02 - m20) / s;
                result.x = (m01 + m10) / s;
                result.y = 0.25f * s;
                result.z = (m12 + m21) / s;
            }
            else
            {
                const float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
                result.w = (m10 - m01) / s;
                result.x = (m02 + m20) / s;
                result.y = (m12 + m21) / s;
                result.z = 0.25f * s;
            }
            const float norm = std::sqrt(result.x * result.x + result.y * result.y +
                                         result.z * result.z + result.w * result.w);
            if (norm > QUATERNION_ZERO_EPSILON)
            {
                result.x /= norm;
                result.y /= norm;
                result.z /= norm;
                result.w /= norm;
            }
            return result;
        }

        MATRIX buildLegacyGlobalMatrix(const util::SKELETON_BONE_V11 &bone) noexcept
        {
            MATRIX scale, rotationX, rotationY, rotationZ, rotation, result, translation;
            MatrixScaling(&scale, bone.scaleX, bone.scaleY, bone.scaleZ);
            MatrixRotationX(&rotationX, bone.rotX * DEGREES_TO_RADIANS);
            MatrixRotationY(&rotationY, bone.rotY * DEGREES_TO_RADIANS);
            MatrixRotationZ(&rotationZ, bone.rotZ * DEGREES_TO_RADIANS);
            MatrixMultiply(&rotation, &rotationX, &rotationY);
            MatrixMultiply(&rotation, &rotation, &rotationZ);
            MatrixMultiply(&result, &scale, &rotation);
            MatrixTranslation(&translation, bone.x, bone.y, bone.z);
            MatrixMultiply(&result, &result, &translation);
            return result;
        }

        uint64_t stableBoneId(const std::string &path) noexcept
        {
            constexpr uint64_t offset = 14695981039346656037ull;
            constexpr uint64_t prime = 1099511628211ull;
            uint64_t value = offset;
            const std::string domain = "mini-mbm.skeleton.bone/" + path;
            for (const unsigned char c : domain)
            {
                value ^= c;
                value *= prime;
            }
            return value == 0 ? 1 : value;
        }

        void addDiagnostic(COMPILED_SKELETON &out, const DIAGNOSTIC_CODE code, const uint32_t sourceIndex,
                           const std::string &name, const float error = 0.0f, const bool fatal = true)
        {
            DIAGNOSTIC diagnostic;
            diagnostic.code = code;
            diagnostic.sourceIndex = sourceIndex;
            diagnostic.boneName = name;
            diagnostic.observedError = error;
            diagnostic.fatal = fatal;
            out.diagnostics.push_back(std::move(diagnostic));
        }

        void addWeightDiagnostic(WEIGHT_VALIDATION_REPORT &out, const DIAGNOSTIC_CODE code,
                                 const uint32_t vertexIndex, const uint8_t slotIndex,
                                 const std::string &boneName, const float error = 0.0f,
                                 const bool fatal = true)
        {
            DIAGNOSTIC diagnostic;
            diagnostic.code = code;
            diagnostic.vertexIndex = vertexIndex;
            diagnostic.slotIndex = slotIndex;
            diagnostic.boneName = boneName;
            diagnostic.observedError = error;
            diagnostic.fatal = fatal;
            out.diagnostics.push_back(std::move(diagnostic));
        }
    }

    bool COMPILED_SKELETON::hasFatalDiagnostics() const noexcept
    {
        return std::any_of(diagnostics.begin(), diagnostics.end(),
                           [](const DIAGNOSTIC &diagnostic) { return diagnostic.fatal; });
    }

    bool WEIGHT_VALIDATION_REPORT::hasFatalDiagnostics() const noexcept
    {
        return std::any_of(diagnostics.begin(), diagnostics.end(),
                           [](const DIAGNOSTIC &diagnostic) { return diagnostic.fatal; });
    }

    MATRIX buildTrsMatrix(const LOCAL_TRANSFORM &transform) noexcept
    {
        QUATERNION quaternion = transform.rotation;
        const float norm = std::sqrt(quaternion.x * quaternion.x + quaternion.y * quaternion.y +
                                     quaternion.z * quaternion.z + quaternion.w * quaternion.w);
        if (norm > QUATERNION_ZERO_EPSILON)
        {
            quaternion.x /= norm;
            quaternion.y /= norm;
            quaternion.z /= norm;
            quaternion.w /= norm;
        }
        else
        {
            quaternion = {};
        }

        const float xx = quaternion.x * quaternion.x;
        const float yy = quaternion.y * quaternion.y;
        const float zz = quaternion.z * quaternion.z;
        const float xy = quaternion.x * quaternion.y;
        const float xz = quaternion.x * quaternion.z;
        const float yz = quaternion.y * quaternion.z;
        const float xw = quaternion.x * quaternion.w;
        const float yw = quaternion.y * quaternion.w;
        const float zw = quaternion.z * quaternion.w;

        MATRIX rotation;
        MatrixIdentity(&rotation);
        rotation._11 = 1.0f - 2.0f * (yy + zz);
        rotation._12 = 2.0f * (xy + zw);
        rotation._13 = 2.0f * (xz - yw);
        rotation._21 = 2.0f * (xy - zw);
        rotation._22 = 1.0f - 2.0f * (xx + zz);
        rotation._23 = 2.0f * (yz + xw);
        rotation._31 = 2.0f * (xz + yw);
        rotation._32 = 2.0f * (yz - xw);
        rotation._33 = 1.0f - 2.0f * (xx + yy);

        MATRIX scale, result, translation;
        MatrixScaling(&scale, transform.scale.x, transform.scale.y, transform.scale.z);
        MatrixMultiply(&result, &scale, &rotation);
        MatrixTranslation(&translation, transform.translation.x, transform.translation.y, transform.translation.z);
        MatrixMultiply(&result, &result, &translation);
        return result;
    }

    bool decomposeTrsMatrix(const MATRIX &matrix, LOCAL_TRANSFORM &out, bool &hasNegativeScale,
                            bool &hasShear) noexcept
    {
        out.translation = VEC3(matrix._41, matrix._42, matrix._43);
        float scale[3] = {
            vectorLength(matrix._11, matrix._12, matrix._13),
            vectorLength(matrix._21, matrix._22, matrix._23),
            vectorLength(matrix._31, matrix._32, matrix._33)
        };
        if (scale[0] <= SINGULAR_TOLERANCE || scale[1] <= SINGULAR_TOLERANCE ||
            scale[2] <= SINGULAR_TOLERANCE)
            return false;

        MATRIX rotation;
        MatrixIdentity(&rotation);
        rotation._11 = matrix._11 / scale[0]; rotation._12 = matrix._12 / scale[0]; rotation._13 = matrix._13 / scale[0];
        rotation._21 = matrix._21 / scale[1]; rotation._22 = matrix._22 / scale[1]; rotation._23 = matrix._23 / scale[1];
        rotation._31 = matrix._31 / scale[2]; rotation._32 = matrix._32 / scale[2]; rotation._33 = matrix._33 / scale[2];

        const float dot01 = rotation._11 * rotation._21 + rotation._12 * rotation._22 + rotation._13 * rotation._23;
        const float dot02 = rotation._11 * rotation._31 + rotation._12 * rotation._32 + rotation._13 * rotation._33;
        const float dot12 = rotation._21 * rotation._31 + rotation._22 * rotation._32 + rotation._23 * rotation._33;
        hasShear = std::max({std::fabs(dot01), std::fabs(dot02), std::fabs(dot12)}) > MATRIX_TOLERANCE;

        hasNegativeScale = determinant3x3(rotation) < 0.0f;
        if (hasNegativeScale)
        {
            int axis = 0;
            if (scale[1] > scale[axis]) axis = 1;
            if (scale[2] > scale[axis]) axis = 2;
            scale[axis] = -scale[axis];
            for (int column = 0; column < 3; ++column)
                rotation.m[axis][column] = -rotation.m[axis][column];
        }

        out.scale = VEC3(scale[0], scale[1], scale[2]);
        out.rotation = quaternionFromRowRotation(rotation);
        return true;
    }

    float maximumMatrixDifference(const MATRIX &left, const MATRIX &right) noexcept
    {
        float result = 0.0f;
        for (int i = 0; i < 16; ++i)
            result = std::max(result, std::fabs(left.p[i] - right.p[i]));
        return result;
    }

    float matrixComparisonTolerance(const MATRIX &left, const MATRIX &right) noexcept
    {
        float magnitude = 1.0f;
        for (int i = 0; i < 16; ++i)
            magnitude = std::max({magnitude, std::fabs(left.p[i]), std::fabs(right.p[i])});
        return MATRIX_TOLERANCE * magnitude;
    }

    const char *diagnosticCodeName(const DIAGNOSTIC_CODE code) noexcept
    {
        switch (code)
        {
            case DIAGNOSTIC_CODE::EMPTY_NAME: return "empty-name";
            case DIAGNOSTIC_CODE::DUPLICATE_NAME: return "duplicate-name";
            case DIAGNOSTIC_CODE::UNKNOWN_PARENT: return "unknown-parent";
            case DIAGNOSTIC_CODE::NON_FINITE_TRANSFORM: return "non-finite-transform";
            case DIAGNOSTIC_CODE::SINGULAR_TRANSFORM: return "singular-transform";
            case DIAGNOSTIC_CODE::NEGATIVE_SCALE: return "negative-scale";
            case DIAGNOSTIC_CODE::SHEAR_NOT_SUPPORTED: return "shear-not-supported";
            case DIAGNOSTIC_CODE::ID_COLLISION: return "id-collision";
            case DIAGNOSTIC_CODE::LOCAL_RECONSTRUCTION_MISMATCH: return "local-reconstruction-mismatch";
            case DIAGNOSTIC_CODE::BIND_IDENTITY_MISMATCH: return "bind-identity-mismatch";
            case DIAGNOSTIC_CODE::EMPTY_PALETTE_NAME: return "empty-palette-name";
            case DIAGNOSTIC_CODE::DUPLICATE_PALETTE_NAME: return "duplicate-palette-name";
            case DIAGNOSTIC_CODE::UNKNOWN_WEIGHT_BONE: return "unknown-weight-bone";
            case DIAGNOSTIC_CODE::VERTEX_COUNT_MISMATCH: return "vertex-count-mismatch";
            case DIAGNOSTIC_CODE::PALETTE_INDEX_OUT_OF_RANGE: return "palette-index-out-of-range";
            case DIAGNOSTIC_CODE::NON_FINITE_WEIGHT: return "non-finite-weight";
            case DIAGNOSTIC_CODE::NEGATIVE_WEIGHT: return "negative-weight";
            case DIAGNOSTIC_CODE::UNUSED_SLOT_NONZERO: return "unused-slot-nonzero";
            case DIAGNOSTIC_CODE::ZERO_WEIGHT_USED_SLOT: return "zero-weight-used-slot";
            case DIAGNOSTIC_CODE::DUPLICATE_BONE_INFLUENCE: return "duplicate-bone-influence";
            case DIAGNOSTIC_CODE::NO_EFFECTIVE_INFLUENCE: return "no-effective-influence";
            case DIAGNOSTIC_CODE::WEIGHT_SUM_MISMATCH: return "weight-sum-mismatch";
        }
        return "unknown";
    }

    bool compileLegacySkeleton(const std::vector<util::SKELETON_BONE_V11> &legacy,
                               COMPILED_SKELETON &out)
    {
        out = {};
        out.bones.reserve(legacy.size());
        std::unordered_map<std::string, int32_t> indexByName;
        std::unordered_map<std::string, std::string> pathByName;
        std::unordered_set<uint64_t> ids;
        indexByName.reserve(legacy.size());
        pathByName.reserve(legacy.size());
        ids.reserve(legacy.size());

        for (uint32_t i = 0; i < legacy.size(); ++i)
        {
            const util::SKELETON_BONE_V11 &source = legacy[i];
            if (source.name.empty())
            {
                addDiagnostic(out, DIAGNOSTIC_CODE::EMPTY_NAME, i, source.name);
                continue;
            }
            if (indexByName.find(source.name) != indexByName.end())
            {
                addDiagnostic(out, DIAGNOSTIC_CODE::DUPLICATE_NAME, i, source.name);
                continue;
            }
            if (!isFinite(source))
            {
                addDiagnostic(out, DIAGNOSTIC_CODE::NON_FINITE_TRANSFORM, i, source.name);
                continue;
            }

            int32_t parentIndex = -1;
            std::string path = source.name;
            if (!source.parentName.empty())
            {
                const auto parent = indexByName.find(source.parentName);
                if (parent == indexByName.end())
                {
                    addDiagnostic(out, DIAGNOSTIC_CODE::UNKNOWN_PARENT, i, source.name);
                    continue;
                }
                parentIndex = parent->second;
                path = pathByName[source.parentName] + "/" + source.name;
            }

            COMPILED_BONE compiled;
            compiled.name = source.name;
            compiled.parentIndex = parentIndex;
            compiled.sourceIndex = i;
            if (parentIndex >= 0)
                compiled.parentBoneId = out.bones[static_cast<size_t>(parentIndex)].boneId;
            compiled.boneId = stableBoneId(path);
            if (!ids.insert(compiled.boneId).second)
            {
                addDiagnostic(out, DIAGNOSTIC_CODE::ID_COLLISION, i, source.name);
                continue;
            }

            compiled.globalBindMatrix = buildLegacyGlobalMatrix(source);
            if (std::fabs(MatrixDeterminant(&compiled.globalBindMatrix)) <= SINGULAR_TOLERANCE)
            {
                addDiagnostic(out, DIAGNOSTIC_CODE::SINGULAR_TRANSFORM, i, source.name);
                continue;
            }

            if (parentIndex < 0)
                compiled.localBindMatrix = compiled.globalBindMatrix;
            else
            {
                MATRIX inverseParent;
                float determinant = 0.0f;
                MatrixInverse(&inverseParent, &determinant,
                              &out.bones[static_cast<size_t>(parentIndex)].globalBindMatrix);
                MatrixMultiply(&compiled.localBindMatrix, &compiled.globalBindMatrix, &inverseParent);
            }

            if (!decomposeTrsMatrix(compiled.localBindMatrix, compiled.localBind,
                                    compiled.hasNegativeScale, compiled.hasShear))
            {
                addDiagnostic(out, DIAGNOSTIC_CODE::SINGULAR_TRANSFORM, i, source.name);
                continue;
            }
            if (compiled.hasNegativeScale)
                addDiagnostic(out, DIAGNOSTIC_CODE::NEGATIVE_SCALE, i, source.name, 0.0f, false);
            if (compiled.hasShear)
                addDiagnostic(out, DIAGNOSTIC_CODE::SHEAR_NOT_SUPPORTED, i, source.name, 0.0f, false);

            MATRIX rebuiltGlobal;
            if (parentIndex < 0)
                rebuiltGlobal = compiled.localBindMatrix;
            else
                MatrixMultiply(&rebuiltGlobal, &compiled.localBindMatrix,
                               &out.bones[static_cast<size_t>(parentIndex)].globalBindMatrix);
            const float reconstructionError = maximumMatrixDifference(rebuiltGlobal, compiled.globalBindMatrix);
            out.maximumReconstructionError = std::max(out.maximumReconstructionError, reconstructionError);
            if (reconstructionError > matrixComparisonTolerance(rebuiltGlobal, compiled.globalBindMatrix))
                addDiagnostic(out, DIAGNOSTIC_CODE::LOCAL_RECONSTRUCTION_MISMATCH, i, source.name,
                              reconstructionError);

            float determinant = 0.0f;
            MatrixInverse(&compiled.inverseGlobalBindMatrix, &determinant, &compiled.globalBindMatrix);
            MATRIX bindIdentity;
            MatrixMultiply(&bindIdentity, &compiled.inverseGlobalBindMatrix, &compiled.globalBindMatrix);
            MATRIX identity;
            MatrixIdentity(&identity);
            const float identityError = maximumMatrixDifference(bindIdentity, identity);
            out.maximumBindIdentityError = std::max(out.maximumBindIdentityError, identityError);
            if (identityError > matrixComparisonTolerance(bindIdentity, identity))
                addDiagnostic(out, DIAGNOSTIC_CODE::BIND_IDENTITY_MISMATCH, i, source.name, identityError);

            const int32_t compiledIndex = static_cast<int32_t>(out.bones.size());
            indexByName.emplace(source.name, compiledIndex);
            pathByName.emplace(source.name, path);
            out.bones.push_back(std::move(compiled));
            out.indexByName.emplace(source.name, compiledIndex);
            out.indexById.emplace(out.bones.back().boneId, compiledIndex);
        }
        return out.bones.size() == legacy.size() && !out.hasFatalDiagnostics();
    }

    bool validateLegacyWeights(const COMPILED_SKELETON &skeleton,
                               const std::vector<std::string> &palette,
                               const std::vector<util::VERTEX_BONE_WEIGHT_V11> &weights,
                               const uint32_t expectedVertexCount,
                               WEIGHT_VALIDATION_REPORT &out)
    {
        out = {};
        out.paletteBoneIndices.assign(palette.size(), -1);
        if (weights.size() != expectedVertexCount)
        {
            addWeightDiagnostic(out, DIAGNOSTIC_CODE::VERTEX_COUNT_MISMATCH, UINT32_MAX, UINT8_MAX,
                                std::string(), static_cast<float>(weights.size()), true);
        }

        std::unordered_set<std::string> paletteNames;
        paletteNames.reserve(palette.size());
        for (size_t i = 0; i < palette.size(); ++i)
        {
            const std::string &name = palette[i];
            if (name.empty())
            {
                addWeightDiagnostic(out, DIAGNOSTIC_CODE::EMPTY_PALETTE_NAME, UINT32_MAX,
                                    static_cast<uint8_t>(i), name);
                continue;
            }
            if (!paletteNames.insert(name).second)
            {
                addWeightDiagnostic(out, DIAGNOSTIC_CODE::DUPLICATE_PALETTE_NAME, UINT32_MAX,
                                    static_cast<uint8_t>(i), name);
                continue;
            }
            const auto bone = skeleton.indexByName.find(name);
            if (bone == skeleton.indexByName.end())
            {
                addWeightDiagnostic(out, DIAGNOSTIC_CODE::UNKNOWN_WEIGHT_BONE, UINT32_MAX,
                                    static_cast<uint8_t>(i), name);
                continue;
            }
            out.paletteBoneIndices[i] = bone->second;
        }

        for (uint32_t vertexIndex = 0; vertexIndex < weights.size(); ++vertexIndex)
        {
            const util::VERTEX_BONE_WEIGHT_V11 &entry = weights[vertexIndex];
            float sum = 0.0f;
            uint8_t effectiveCount = 0;
            std::unordered_set<uint8_t> usedPaletteIndices;
            for (uint8_t slot = 0; slot < 4; ++slot)
            {
                const uint8_t paletteIndex = entry.paletteIndex[slot];
                const float weight = entry.weight[slot];
                if (!std::isfinite(weight))
                {
                    addWeightDiagnostic(out, DIAGNOSTIC_CODE::NON_FINITE_WEIGHT, vertexIndex, slot,
                                        std::string(), weight);
                    continue;
                }
                if (paletteIndex == UINT8_MAX)
                {
                    if (weight != 0.0f)
                        addWeightDiagnostic(out, DIAGNOSTIC_CODE::UNUSED_SLOT_NONZERO, vertexIndex,
                                            slot, std::string(), weight);
                    continue;
                }
                if (paletteIndex >= palette.size())
                {
                    addWeightDiagnostic(out, DIAGNOSTIC_CODE::PALETTE_INDEX_OUT_OF_RANGE,
                                        vertexIndex, slot, std::string(), paletteIndex);
                    continue;
                }
                const std::string &boneName = palette[paletteIndex];
                if (!usedPaletteIndices.insert(paletteIndex).second)
                    addWeightDiagnostic(out, DIAGNOSTIC_CODE::DUPLICATE_BONE_INFLUENCE,
                                        vertexIndex, slot, boneName);
                if (weight < 0.0f)
                {
                    addWeightDiagnostic(out, DIAGNOSTIC_CODE::NEGATIVE_WEIGHT, vertexIndex, slot,
                                        boneName, weight);
                    continue;
                }
                if (weight == 0.0f)
                {
                    addWeightDiagnostic(out, DIAGNOSTIC_CODE::ZERO_WEIGHT_USED_SLOT, vertexIndex,
                                        slot, boneName, 0.0f, false);
                    continue;
                }
                sum += weight;
                ++effectiveCount;
            }
            if (effectiveCount == 0)
            {
                ++out.verticesWithoutEffectiveInfluence;
                addWeightDiagnostic(out, DIAGNOSTIC_CODE::NO_EFFECTIVE_INFLUENCE, vertexIndex,
                                    UINT8_MAX, std::string(), 0.0f, false);
                continue;
            }
            const float sumError = std::fabs(sum - 1.0f);
            out.maximumWeightSumError = std::max(out.maximumWeightSumError, sumError);
            if (sumError > MATRIX_TOLERANCE)
            {
                ++out.verticesWithInvalidWeightSum;
                addWeightDiagnostic(out, DIAGNOSTIC_CODE::WEIGHT_SUM_MISMATCH, vertexIndex,
                                    UINT8_MAX, std::string(), sumError, false);
            }
        }
        return !out.hasFatalDiagnostics();
    }
}
