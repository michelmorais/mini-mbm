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
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace mbm::skeletal
{
    namespace
    {

        bool isFinite(const float value) noexcept
        {
            return std::isfinite(value);
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

        float matrixProductIdentityTolerance(const MATRIX &left, const MATRIX &right) noexcept
        {
            float maximumAbsoluteProductSum = 1.0f;
            for (int row = 0; row < 4; ++row)
            {
                for (int column = 0; column < 4; ++column)
                {
                    float absoluteProductSum = 0.0f;
                    for (int inner = 0; inner < 4; ++inner)
                        absoluteProductSum += std::fabs(left.p[row * 4 + inner] *
                                                        right.p[inner * 4 + column]);
                    maximumAbsoluteProductSum = std::max(maximumAbsoluteProductSum, absoluteProductSum);
                }
            }
            // Four float products and three additions contribute rounding error to each output
            // element. Eight epsilons is a conservative gamma-style bound; MATRIX_TOLERANCE still
            // supplies the contract floor for unit-scale matrices.
            return MATRIX_TOLERANCE + 8.0f * std::numeric_limits<float>::epsilon() *
                   maximumAbsoluteProductSum;
        }


        bool isFinite(const LOCAL_TRANSFORM &transform) noexcept
        {
            return isFinite(transform.translation.x) && isFinite(transform.translation.y) &&
                   isFinite(transform.translation.z) && isFinite(transform.rotation.x) &&
                   isFinite(transform.rotation.y) && isFinite(transform.rotation.z) &&
                   isFinite(transform.rotation.w) && isFinite(transform.scale.x) &&
                   isFinite(transform.scale.y) && isFinite(transform.scale.z);
        }

        float quaternionNorm(const QUATERNION &value) noexcept
        {
            return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z + value.w * value.w);
        }

        QUATERNION normalizedQuaternion(QUATERNION value) noexcept
        {
            const float norm = quaternionNorm(value);
            if (norm > QUATERNION_ZERO_EPSILON)
            {
                value.x /= norm; value.y /= norm; value.z /= norm; value.w /= norm;
            }
            return value;
        }

        QUATERNION multiplyQuaternion(const QUATERNION &left, const QUATERNION &right) noexcept
        {
            return {
                left.w * right.x + left.x * right.w + left.y * right.z - left.z * right.y,
                left.w * right.y - left.x * right.z + left.y * right.w + left.z * right.x,
                left.w * right.z + left.x * right.y - left.y * right.x + left.z * right.w,
                left.w * right.w - left.x * right.x - left.y * right.y - left.z * right.z
            };
        }

        QUATERNION conjugateQuaternion(const QUATERNION &value) noexcept
        {
            return {-value.x, -value.y, -value.z, value.w};
        }

        VEC3 rotateVectorByQuaternion(const VEC3 &value, const QUATERNION &rotation) noexcept
        {
            const QUATERNION vector = {value.x, value.y, value.z, 0.0f};
            const QUATERNION rotated = multiplyQuaternion(
                multiplyQuaternion(rotation, vector), conjugateQuaternion(rotation));
            return VEC3(rotated.x, rotated.y, rotated.z);
        }

        QUATERNION interpolateQuaternion(const QUATERNION &start, QUATERNION end, const float factor) noexcept
        {
            QUATERNION begin = normalizedQuaternion(start);
            end = normalizedQuaternion(end);
            float dot = begin.x * end.x + begin.y * end.y + begin.z * end.z + begin.w * end.w;
            if (dot < 0.0f)
            {
                end.x = -end.x; end.y = -end.y; end.z = -end.z; end.w = -end.w;
                dot = -dot;
            }
            if (dot > 0.9995f)
            {
                return normalizedQuaternion({begin.x + (end.x - begin.x) * factor,
                                             begin.y + (end.y - begin.y) * factor,
                                             begin.z + (end.z - begin.z) * factor,
                                             begin.w + (end.w - begin.w) * factor});
            }
            dot = std::max(-1.0f, std::min(1.0f, dot));
            const float angle = std::acos(dot);
            const float sine = std::sin(angle);
            const float a = std::sin((1.0f - factor) * angle) / sine;
            const float b = std::sin(factor * angle) / sine;
            return {begin.x * a + end.x * b, begin.y * a + end.y * b,
                    begin.z * a + end.z * b, begin.w * a + end.w * b};
        }

        float applyEasing(float value, const SKELETAL_KEY &key) noexcept
        {
            value = std::max(0.0f, std::min(1.0f, value));
            if (key.easing == SKELETAL_EASING::EASE_IN) return value * value;
            if (key.easing == SKELETAL_EASING::EASE_OUT) return 1.0f - (1.0f - value) * (1.0f - value);
            if (key.easing == SKELETAL_EASING::EASE_IN_OUT)
                return value < 0.5f ? 2.0f * value * value : 1.0f - 2.0f * (1.0f - value) * (1.0f - value);
            if (key.easing == SKELETAL_EASING::SMOOTHSTEP) return value * value * (3.0f - 2.0f * value);
            if (key.easing != SKELETAL_EASING::CUBIC_BEZIER) return value;
            const auto cubic = [](const float t, const float p1, const float p2)
            {
                const float u = 1.0f - t;
                return 3.0f * u * u * t * p1 + 3.0f * u * t * t * p2 + t * t * t;
            };
            float low = 0.0f, high = 1.0f, parameter = value;
            for (int i = 0; i < 16; ++i)
            {
                const float x = cubic(parameter, key.bezierX1, key.bezierX2);
                if (x < value) low = parameter; else high = parameter;
                parameter = (low + high) * 0.5f;
            }
            return cubic(parameter, key.bezierY1, key.bezierY2);
        }

        void addClipDiagnostic(std::vector<DIAGNOSTIC> &out, const DIAGNOSTIC_CODE code,
                               const uint32_t trackIndex, const uint32_t keyIndex,
                               const float observedError = 0.0f, const bool fatal = true)
        {
            DIAGNOSTIC diagnostic;
            diagnostic.code = code;
            diagnostic.sourceIndex = trackIndex;
            diagnostic.keyIndex = keyIndex;
            diagnostic.observedError = observedError;
            diagnostic.fatal = fatal;
            out.push_back(std::move(diagnostic));
        }
    }

    bool COMPILED_SKELETON::hasFatalDiagnostics() const noexcept
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

    bool rigidDualQuaternionFromMatrix(const MATRIX &matrix, DUAL_QUATERNION &out) noexcept
    {
        LOCAL_TRANSFORM rigid;
        bool hasNegativeScale = false, hasShear = false;
        if (!decomposeTrsMatrix(matrix, rigid, hasNegativeScale, hasShear) ||
            hasNegativeScale || hasShear ||
            std::fabs(rigid.scale.x - 1.0f) > MATRIX_TOLERANCE ||
            std::fabs(rigid.scale.y - 1.0f) > MATRIX_TOLERANCE ||
            std::fabs(rigid.scale.z - 1.0f) > MATRIX_TOLERANCE)
            return false;
        out.real = normalizedQuaternion(rigid.rotation);
        const QUATERNION translation = {rigid.translation.x, rigid.translation.y,
                                        rigid.translation.z, 0.0f};
        out.dual = multiplyQuaternion(translation, out.real);
        out.dual.x *= 0.5f;
        out.dual.y *= 0.5f;
        out.dual.z *= 0.5f;
        out.dual.w *= 0.5f;
        return true;
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
            case DIAGNOSTIC_CODE::INVALID_BIND_QUATERNION: return "invalid-bind-quaternion";
            case DIAGNOSTIC_CODE::NON_UNIT_BIND_QUATERNION: return "non-unit-bind-quaternion";
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
            case DIAGNOSTIC_CODE::INVALID_CLIP_ID: return "invalid-clip-id";
            case DIAGNOSTIC_CODE::EMPTY_CLIP_NAME: return "empty-clip-name";
            case DIAGNOSTIC_CODE::INVALID_CLIP_DURATION: return "invalid-clip-duration";
            case DIAGNOSTIC_CODE::UNKNOWN_TRACK_BONE: return "unknown-track-bone";
            case DIAGNOSTIC_CODE::DUPLICATE_BONE_TRACK: return "duplicate-bone-track";
            case DIAGNOSTIC_CODE::INVALID_CHANNEL_MASK: return "invalid-channel-mask";
            case DIAGNOSTIC_CODE::EMPTY_TRACK_KEYS: return "empty-track-keys";
            case DIAGNOSTIC_CODE::INVALID_KEY_TIME: return "invalid-key-time";
            case DIAGNOSTIC_CODE::NON_INCREASING_KEY_TIME: return "non-increasing-key-time";
            case DIAGNOSTIC_CODE::NON_FINITE_KEY_TRANSFORM: return "non-finite-key-transform";
            case DIAGNOSTIC_CODE::INVALID_KEY_QUATERNION: return "invalid-key-quaternion";
            case DIAGNOSTIC_CODE::NON_UNIT_KEY_QUATERNION: return "non-unit-key-quaternion";
            case DIAGNOSTIC_CODE::SINGULAR_KEY_SCALE: return "singular-key-scale";
            case DIAGNOSTIC_CODE::NON_UNIFORM_KEY_SCALE: return "non-uniform-key-scale";
            case DIAGNOSTIC_CODE::NEGATIVE_KEY_SCALE: return "negative-key-scale";
            case DIAGNOSTIC_CODE::INVALID_EASING: return "invalid-easing";
            case DIAGNOSTIC_CODE::INVALID_BEZIER_CONTROL: return "invalid-bezier-control";
            case DIAGNOSTIC_CODE::INVALID_SAMPLE_TIME: return "invalid-sample-time";
        }
        return "unknown";
    }


    bool compileCanonicalSkeleton(const std::vector<CANONICAL_BONE> &source, COMPILED_SKELETON &out)
    {
        out = {};
        out.bones.reserve(source.size());
        std::unordered_set<uint64_t> ids;
        std::unordered_set<std::string> names;
        for (uint32_t i = 0; i < source.size(); ++i)
        {
            const CANONICAL_BONE &input = source[i];
            if (input.name.empty()) { addDiagnostic(out, DIAGNOSTIC_CODE::EMPTY_NAME, i, input.name); continue; }
            if (!names.insert(input.name).second) { addDiagnostic(out, DIAGNOSTIC_CODE::DUPLICATE_NAME, i, input.name); continue; }
            if (input.boneId == 0 || !ids.insert(input.boneId).second)
            { addDiagnostic(out, DIAGNOSTIC_CODE::ID_COLLISION, i, input.name); continue; }
            int32_t parentIndex = -1;
            if (input.parentBoneId != 0)
            {
                const auto parent = out.indexById.find(input.parentBoneId);
                if (parent == out.indexById.end())
                { addDiagnostic(out, DIAGNOSTIC_CODE::UNKNOWN_PARENT, i, input.name); continue; }
                parentIndex = parent->second;
            }
            const LOCAL_TRANSFORM &local = input.localBind;
            const bool finite = isFinite(local.translation.x) && isFinite(local.translation.y) &&
                isFinite(local.translation.z) && isFinite(local.rotation.x) && isFinite(local.rotation.y) &&
                isFinite(local.rotation.z) && isFinite(local.rotation.w) && isFinite(local.scale.x) &&
                isFinite(local.scale.y) && isFinite(local.scale.z) && isFinite(input.radius) && isFinite(input.length) &&
                isFinite(input.tailOffset.x) && isFinite(input.tailOffset.y) && isFinite(input.tailOffset.z);
            if (!finite) { addDiagnostic(out, DIAGNOSTIC_CODE::NON_FINITE_TRANSFORM, i, input.name); continue; }
            const float quaternionNorm = std::sqrt(local.rotation.x * local.rotation.x +
                local.rotation.y * local.rotation.y + local.rotation.z * local.rotation.z +
                local.rotation.w * local.rotation.w);
            if (quaternionNorm <= QUATERNION_ZERO_EPSILON)
            { addDiagnostic(out, DIAGNOSTIC_CODE::INVALID_BIND_QUATERNION, i, input.name, quaternionNorm); continue; }
            if (std::fabs(quaternionNorm - 1.0f) > MATRIX_TOLERANCE)
                addDiagnostic(out, DIAGNOSTIC_CODE::NON_UNIT_BIND_QUATERNION, i, input.name, quaternionNorm, false);
            if (std::fabs(local.scale.x) <= SINGULAR_TOLERANCE ||
                std::fabs(local.scale.y) <= SINGULAR_TOLERANCE || std::fabs(local.scale.z) <= SINGULAR_TOLERANCE)
            { addDiagnostic(out, DIAGNOSTIC_CODE::SINGULAR_TRANSFORM, i, input.name); continue; }

            COMPILED_BONE compiled;
            compiled.boneId = input.boneId; compiled.parentBoneId = input.parentBoneId;
            compiled.parentIndex = parentIndex; compiled.sourceIndex = i; compiled.name = input.name;
            compiled.localBind = local;
            compiled.localBind.rotation.x /= quaternionNorm; compiled.localBind.rotation.y /= quaternionNorm;
            compiled.localBind.rotation.z /= quaternionNorm; compiled.localBind.rotation.w /= quaternionNorm;
            compiled.hasNegativeScale = local.scale.x < 0.0f || local.scale.y < 0.0f || local.scale.z < 0.0f;
            if (compiled.hasNegativeScale)
                addDiagnostic(out, DIAGNOSTIC_CODE::NEGATIVE_SCALE, i, input.name, 0.0f, false);
            compiled.localBindMatrix = buildTrsMatrix(compiled.localBind);
            if (parentIndex < 0) compiled.globalBindMatrix = compiled.localBindMatrix;
            else MatrixMultiply(&compiled.globalBindMatrix, &compiled.localBindMatrix,
                &out.bones[static_cast<size_t>(parentIndex)].globalBindMatrix);
            float determinant = 0.0f;
            MatrixInverse(&compiled.inverseGlobalBindMatrix, &determinant, &compiled.globalBindMatrix);
            MATRIX identity, observed;
            MatrixIdentity(&identity);
            MatrixMultiply(&observed, &compiled.inverseGlobalBindMatrix, &compiled.globalBindMatrix);
            const float identityError = maximumMatrixDifference(observed, identity);
            out.maximumBindIdentityError = std::max(out.maximumBindIdentityError, identityError);
            if (identityError > matrixProductIdentityTolerance(compiled.inverseGlobalBindMatrix,
                                                               compiled.globalBindMatrix))
                addDiagnostic(out, DIAGNOSTIC_CODE::BIND_IDENTITY_MISMATCH, i, input.name, identityError);
            const int32_t compiledIndex = static_cast<int32_t>(out.bones.size());
            out.indexByName.emplace(input.name, compiledIndex);
            out.indexById.emplace(input.boneId, compiledIndex);
            out.bones.push_back(std::move(compiled));
        }
        return out.bones.size() == source.size() && !out.hasFatalDiagnostics();
    }

    bool validateCanonicalWeights(const CANONICAL_SKELETON &skeleton,
                                  const CANONICAL_WEIGHTS &weights,
                                  const uint32_t expectedVertexCount) noexcept
    {
        if (weights.skeletonId == 0 || weights.skeletonId != skeleton.skeletonId ||
            weights.frameIndex != 0 || weights.vertices.size() != expectedVertexCount ||
            weights.paletteBoneIds.size() > 65535)
            return false;
        std::unordered_set<uint64_t> paletteIds;
        for (const uint64_t boneId : weights.paletteBoneIds)
        {
            if (boneId == 0 || skeleton.compiled.indexById.find(boneId) == skeleton.compiled.indexById.end() ||
                !paletteIds.insert(boneId).second)
                return false;
        }
        for (const CANONICAL_VERTEX_WEIGHT &vertex : weights.vertices)
        {
            float sum = 0.0f;
            uint32_t influenceCount = 0;
            std::unordered_set<uint16_t> used;
            for (uint32_t slot = 0; slot < 4; ++slot)
            {
                const uint16_t paletteIndex = vertex.paletteIndex[slot];
                const float weight = vertex.weight[slot];
                if (!std::isfinite(weight) || weight < 0.0f)
                    return false;
                if (paletteIndex == UINT16_MAX)
                {
                    if (weight != 0.0f) return false;
                    continue;
                }
                if (paletteIndex >= weights.paletteBoneIds.size() || weight <= 0.0f ||
                    !used.insert(paletteIndex).second)
                    return false;
                sum += weight;
                ++influenceCount;
            }
            if (influenceCount == 0 || std::fabs(sum - 1.0f) > MATRIX_TOLERANCE)
                return false;
        }
        return true;
    }

    bool validateCanonicalAnimations(const CANONICAL_SKELETON &skeleton,
                                     const CANONICAL_ANIMATIONS &animations) noexcept
    {
        if (animations.skeletonId == 0 || animations.skeletonId != skeleton.skeletonId)
            return false;
        std::unordered_set<uint64_t> clipIds;
        std::unordered_set<std::string> clipNames;
        for (const SKELETAL_CLIP &clip : animations.clips)
        {
            if (!clipIds.insert(clip.clipId).second || !clipNames.insert(clip.name).second)
                return false;
            std::vector<DIAGNOSTIC> diagnostics;
            if (!validateSkeletalClip(skeleton.compiled, clip, diagnostics))
                return false;
        }
        return true;
    }

    bool buildUniformlyScaledCanonicalAsset(const CANONICAL_SKELETON &skeleton,
                                            const CANONICAL_ANIMATIONS &animations,
                                            const float scale,
                                            CANONICAL_SKELETON &scaledSkeleton,
                                            CANONICAL_ANIMATIONS &scaledAnimations)
    {
        if (skeleton.skeletonId == 0 || !std::isfinite(scale) || scale <= 0.0f)
            return false;

        scaledSkeleton = skeleton;
        scaledAnimations = animations;
        const auto scaleTranslation = [scale](VEC3 &translation)
        {
            translation.x *= scale;
            translation.y *= scale;
            translation.z *= scale;
            return std::isfinite(translation.x) && std::isfinite(translation.y) &&
                   std::isfinite(translation.z);
        };
        for (CANONICAL_BONE &bone : scaledSkeleton.sourceBones)
        {
            if (!scaleTranslation(bone.localBind.translation))
                return false;
            bone.radius *= scale;
            bone.length *= scale;
            bone.tailOffset.x *= scale;
            bone.tailOffset.y *= scale;
            bone.tailOffset.z *= scale;
            if (!std::isfinite(bone.radius) || !std::isfinite(bone.length) ||
                !std::isfinite(bone.tailOffset.x) || !std::isfinite(bone.tailOffset.y) ||
                !std::isfinite(bone.tailOffset.z))
                return false;
        }
        for (SKELETAL_CLIP &clip : scaledAnimations.clips)
        {
            for (SKELETAL_TRACK &track : clip.tracks)
            {
                for (SKELETAL_KEY &key : track.keys)
                {
                    if (!scaleTranslation(key.local.translation))
                        return false;
                }
            }
        }

        if (!compileCanonicalSkeleton(scaledSkeleton.sourceBones, scaledSkeleton.compiled))
            return false;
        return scaledAnimations.skeletonId == 0 ||
               validateCanonicalAnimations(scaledSkeleton, scaledAnimations);
    }


    bool validateSkeletalClip(const COMPILED_SKELETON &skeleton, const SKELETAL_CLIP &clip,
                              std::vector<DIAGNOSTIC> &diagnostics)
    {
        diagnostics.clear();
        if (clip.clipId == 0) addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::INVALID_CLIP_ID, UINT32_MAX, UINT32_MAX);
        if (clip.name.empty()) addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::EMPTY_CLIP_NAME, UINT32_MAX, UINT32_MAX);
        if (!std::isfinite(clip.duration) || clip.duration < 0.0f)
            addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::INVALID_CLIP_DURATION, UINT32_MAX, UINT32_MAX, clip.duration);
        std::unordered_set<uint64_t> targets;
        for (uint32_t trackIndex = 0; trackIndex < clip.tracks.size(); ++trackIndex)
        {
            const SKELETAL_TRACK &track = clip.tracks[trackIndex];
            if (skeleton.indexById.find(track.boneId) == skeleton.indexById.end())
                addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::UNKNOWN_TRACK_BONE, trackIndex, UINT32_MAX);
            if (!targets.insert(track.boneId).second)
                addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::DUPLICATE_BONE_TRACK, trackIndex, UINT32_MAX);
            if (track.channelMask == 0 || (track.channelMask & ~7u) != 0)
                addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::INVALID_CHANNEL_MASK, trackIndex, UINT32_MAX);
            if (track.keys.empty())
            {
                addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::EMPTY_TRACK_KEYS, trackIndex, UINT32_MAX);
                continue;
            }
            float previousTime = -std::numeric_limits<float>::infinity();
            for (uint32_t keyIndex = 0; keyIndex < track.keys.size(); ++keyIndex)
            {
                const SKELETAL_KEY &key = track.keys[keyIndex];
                if (!std::isfinite(key.time) || key.time < 0.0f || key.time > clip.duration + KEY_TIME_TOLERANCE)
                    addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::INVALID_KEY_TIME, trackIndex, keyIndex, key.time);
                if (keyIndex > 0 && key.time - previousTime <= KEY_TIME_TOLERANCE)
                    addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::NON_INCREASING_KEY_TIME, trackIndex, keyIndex,
                                      key.time - previousTime);
                previousTime = key.time;
                if (!isFinite(key.local) || !std::isfinite(key.bezierX1) || !std::isfinite(key.bezierY1) ||
                    !std::isfinite(key.bezierX2) || !std::isfinite(key.bezierY2))
                    addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::NON_FINITE_KEY_TRANSFORM, trackIndex, keyIndex);
                if ((track.channelMask & SKELETAL_CHANNEL_ROTATION) != 0 &&
                    quaternionNorm(key.local.rotation) <= QUATERNION_ZERO_EPSILON)
                    addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::INVALID_KEY_QUATERNION, trackIndex, keyIndex);
                else if ((track.channelMask & SKELETAL_CHANNEL_ROTATION) != 0 &&
                         std::fabs(quaternionNorm(key.local.rotation) - 1.0f) > MATRIX_TOLERANCE)
                    addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::NON_UNIT_KEY_QUATERNION,
                                      trackIndex, keyIndex,
                                      std::fabs(quaternionNorm(key.local.rotation) - 1.0f), false);
                if ((track.channelMask & SKELETAL_CHANNEL_SCALE) != 0 &&
                    (std::fabs(key.local.scale.x) <= SINGULAR_TOLERANCE ||
                     std::fabs(key.local.scale.y) <= SINGULAR_TOLERANCE ||
                     std::fabs(key.local.scale.z) <= SINGULAR_TOLERANCE))
                    addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::SINGULAR_KEY_SCALE, trackIndex, keyIndex);
                else if ((track.channelMask & SKELETAL_CHANNEL_SCALE) != 0)
                {
                    if (key.local.scale.x < 0.0f || key.local.scale.y < 0.0f || key.local.scale.z < 0.0f)
                        addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::NEGATIVE_KEY_SCALE,
                                          trackIndex, keyIndex, 0.0f, false);
                    const float spread = std::max({key.local.scale.x, key.local.scale.y, key.local.scale.z}) -
                                         std::min({key.local.scale.x, key.local.scale.y, key.local.scale.z});
                    if (std::fabs(spread) > MATRIX_TOLERANCE)
                        addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::NON_UNIFORM_KEY_SCALE,
                                          trackIndex, keyIndex, std::fabs(spread), false);
                }
                if (static_cast<uint8_t>(key.easing) > static_cast<uint8_t>(SKELETAL_EASING::CUBIC_BEZIER))
                    addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::INVALID_EASING, trackIndex, keyIndex);
                if (key.easing == SKELETAL_EASING::CUBIC_BEZIER &&
                    (key.bezierX1 < 0.0f || key.bezierX1 > 1.0f || key.bezierX2 < 0.0f || key.bezierX2 > 1.0f))
                    addClipDiagnostic(diagnostics, DIAGNOSTIC_CODE::INVALID_BEZIER_CONTROL, trackIndex, keyIndex);
            }
        }
        return !std::any_of(diagnostics.begin(), diagnostics.end(),
                            [](const DIAGNOSTIC &diagnostic) { return diagnostic.fatal; });
    }

    bool sampleSkeletalClip(const COMPILED_SKELETON &skeleton, const SKELETAL_CLIP &clip,
                            float time, SKELETAL_POSE &out, std::vector<DIAGNOSTIC> *diagnostics)
    {
        std::vector<DIAGNOSTIC> localDiagnostics;
        std::vector<DIAGNOSTIC> &resultDiagnostics = diagnostics ? *diagnostics : localDiagnostics;
        if (!validateSkeletalClip(skeleton, clip, resultDiagnostics))
            return false;
        if (!std::isfinite(time))
        {
            addClipDiagnostic(resultDiagnostics, DIAGNOSTIC_CODE::INVALID_SAMPLE_TIME, UINT32_MAX, UINT32_MAX, time);
            return false;
        }
        if (clip.loop && clip.duration > KEY_TIME_TOLERANCE)
        {
            time = std::fmod(time, clip.duration);
            if (time < 0.0f) time += clip.duration;
        }
        else
            time = std::max(0.0f, std::min(clip.duration, time));

        out.localTransforms.clear();
        out.globalTransforms.clear();
        out.localTransforms.reserve(skeleton.bones.size());
        out.globalTransforms.resize(skeleton.bones.size());
        for (const COMPILED_BONE &bone : skeleton.bones)
            out.localTransforms.push_back(bone.localBind);

        for (const SKELETAL_TRACK &track : clip.tracks)
        {
            const int32_t boneIndex = skeleton.indexById.at(track.boneId);
            const SKELETAL_KEY *a = &track.keys.front();
            const SKELETAL_KEY *b = a;
            if (time >= track.keys.back().time) a = b = &track.keys.back();
            else if (time > track.keys.front().time)
            {
                for (size_t i = 1; i < track.keys.size(); ++i)
                {
                    if (time <= track.keys[i].time)
                    {
                        a = &track.keys[i - 1]; b = &track.keys[i]; break;
                    }
                }
            }
            float factor = 0.0f;
            if (a != b) factor = applyEasing((time - a->time) / (b->time - a->time), *a);
            LOCAL_TRANSFORM &sample = out.localTransforms[static_cast<size_t>(boneIndex)];
            const auto lerp = [factor](const float x, const float y) { return x + (y - x) * factor; };
            if ((track.channelMask & SKELETAL_CHANNEL_TRANSLATION) != 0)
                sample.translation = VEC3(lerp(a->local.translation.x, b->local.translation.x),
                                          lerp(a->local.translation.y, b->local.translation.y),
                                          lerp(a->local.translation.z, b->local.translation.z));
            if ((track.channelMask & SKELETAL_CHANNEL_ROTATION) != 0)
                sample.rotation = interpolateQuaternion(a->local.rotation, b->local.rotation, factor);
            if ((track.channelMask & SKELETAL_CHANNEL_SCALE) != 0)
                sample.scale = VEC3(lerp(a->local.scale.x, b->local.scale.x),
                                    lerp(a->local.scale.y, b->local.scale.y),
                                    lerp(a->local.scale.z, b->local.scale.z));
        }

        for (size_t i = 0; i < skeleton.bones.size(); ++i)
        {
            const MATRIX local = buildTrsMatrix(out.localTransforms[i]);
            const int32_t parent = skeleton.bones[i].parentIndex;
            if (parent < 0) out.globalTransforms[i] = local;
            else MatrixMultiply(&out.globalTransforms[i], &local, &out.globalTransforms[static_cast<size_t>(parent)]);
        }
        return true;
    }

    bool composeSkeletalPosesAbsolute(const COMPILED_SKELETON &skeleton,
                                      const SKELETAL_POSE &basePose,
                                      const SKELETAL_POSE &layerPose,
                                      const float layerWeight,
                                      SKELETAL_POSE &out) noexcept
    {
        out = {};
        const size_t boneCount = skeleton.bones.size();
        if (!std::isfinite(layerWeight) || layerWeight < 0.0f || layerWeight > 1.0f ||
            basePose.localTransforms.size() != boneCount ||
            layerPose.localTransforms.size() != boneCount)
            return false;

        out.localTransforms.resize(boneCount);
        out.globalTransforms.resize(boneCount);
        const auto lerp = [layerWeight](const float a, const float b)
        {
            return a + (b - a) * layerWeight;
        };
        for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
        {
            const LOCAL_TRANSFORM &base = basePose.localTransforms[boneIndex];
            const LOCAL_TRANSFORM &layer = layerPose.localTransforms[boneIndex];
            if (!isFinite(base) || !isFinite(layer) ||
                quaternionNorm(base.rotation) <= QUATERNION_ZERO_EPSILON ||
                quaternionNorm(layer.rotation) <= QUATERNION_ZERO_EPSILON)
            {
                out = {};
                return false;
            }
            LOCAL_TRANSFORM &composed = out.localTransforms[boneIndex];
            composed.translation = VEC3(lerp(base.translation.x, layer.translation.x),
                                        lerp(base.translation.y, layer.translation.y),
                                        lerp(base.translation.z, layer.translation.z));
            composed.rotation = interpolateQuaternion(base.rotation, layer.rotation, layerWeight);
            composed.scale = VEC3(lerp(base.scale.x, layer.scale.x),
                                  lerp(base.scale.y, layer.scale.y),
                                  lerp(base.scale.z, layer.scale.z));
            if (!isFinite(composed))
            {
                out = {};
                return false;
            }
            const MATRIX local = buildTrsMatrix(composed);
            const int32_t parent = skeleton.bones[boneIndex].parentIndex;
            if (parent < 0)
                out.globalTransforms[boneIndex] = local;
            else if (static_cast<size_t>(parent) < boneIndex)
                MatrixMultiply(&out.globalTransforms[boneIndex], &local,
                               &out.globalTransforms[static_cast<size_t>(parent)]);
            else
            {
                out = {};
                return false;
            }
        }
        return true;
    }

    bool advanceSkeletalClipTime(const SKELETAL_CLIP &clip, const float delta,
                                 float &time) noexcept
    {
        if (!std::isfinite(delta) || delta < 0.0f || !std::isfinite(time) ||
            !std::isfinite(clip.duration) || clip.duration < 0.0f)
            return false;
        time += delta;
        if (clip.loop && clip.duration > 0.0f)
            time = std::fmod(time, clip.duration);
        else
            time = std::min(time, clip.duration);
        return true;
    }

    bool composeSkeletalPosesAdditive(const COMPILED_SKELETON &skeleton,
                                      const SKELETAL_POSE &basePose,
                                      const SKELETAL_POSE &layerPose,
                                      const float layerWeight,
                                      SKELETAL_POSE &out) noexcept
    {
        out = {};
        const size_t boneCount = skeleton.bones.size();
        if (!std::isfinite(layerWeight) || layerWeight < 0.0f || layerWeight > 1.0f ||
            basePose.localTransforms.size() != boneCount ||
            layerPose.localTransforms.size() != boneCount)
            return false;
        out.localTransforms.resize(boneCount);
        out.globalTransforms.resize(boneCount);
        const QUATERNION identity;
        for (size_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
        {
            const LOCAL_TRANSFORM &reference = skeleton.bones[boneIndex].localBind;
            const LOCAL_TRANSFORM &base = basePose.localTransforms[boneIndex];
            const LOCAL_TRANSFORM &layer = layerPose.localTransforms[boneIndex];
            if (!isFinite(reference) || !isFinite(base) || !isFinite(layer) ||
                quaternionNorm(reference.rotation) <= QUATERNION_ZERO_EPSILON ||
                quaternionNorm(base.rotation) <= QUATERNION_ZERO_EPSILON ||
                quaternionNorm(layer.rotation) <= QUATERNION_ZERO_EPSILON ||
                std::fabs(reference.scale.x) <= SINGULAR_TOLERANCE ||
                std::fabs(reference.scale.y) <= SINGULAR_TOLERANCE ||
                std::fabs(reference.scale.z) <= SINGULAR_TOLERANCE)
            {
                out = {};
                return false;
            }
            LOCAL_TRANSFORM &composed = out.localTransforms[boneIndex];
            composed.translation = VEC3(
                base.translation.x + (layer.translation.x - reference.translation.x) * layerWeight,
                base.translation.y + (layer.translation.y - reference.translation.y) * layerWeight,
                base.translation.z + (layer.translation.z - reference.translation.z) * layerWeight);
            const QUATERNION referenceRotation = normalizedQuaternion(reference.rotation);
            const QUATERNION layerRotation = normalizedQuaternion(layer.rotation);
            const QUATERNION deltaRotation = normalizedQuaternion(
                multiplyQuaternion(conjugateQuaternion(referenceRotation), layerRotation));
            const QUATERNION weightedDelta = interpolateQuaternion(identity, deltaRotation, layerWeight);
            composed.rotation = normalizedQuaternion(
                multiplyQuaternion(normalizedQuaternion(base.rotation), weightedDelta));
            composed.scale = VEC3(
                base.scale.x * (1.0f + (layer.scale.x / reference.scale.x - 1.0f) * layerWeight),
                base.scale.y * (1.0f + (layer.scale.y / reference.scale.y - 1.0f) * layerWeight),
                base.scale.z * (1.0f + (layer.scale.z / reference.scale.z - 1.0f) * layerWeight));
            if (!isFinite(composed))
            {
                out = {};
                return false;
            }
            const MATRIX local = buildTrsMatrix(composed);
            const int32_t parent = skeleton.bones[boneIndex].parentIndex;
            if (parent < 0)
                out.globalTransforms[boneIndex] = local;
            else if (static_cast<size_t>(parent) < boneIndex)
                MatrixMultiply(&out.globalTransforms[boneIndex], &local,
                               &out.globalTransforms[static_cast<size_t>(parent)]);
            else
            {
                out = {};
                return false;
            }
        }
        return true;
    }

    bool sampleSkeletalClipsAbsolute(const COMPILED_SKELETON &skeleton,
                                     const SKELETAL_CLIP &baseClip, const float baseTime,
                                     const SKELETAL_CLIP &layerClip, const float layerTime,
                                     const float layerWeight, SKELETAL_POSE &out)
    {
        SKELETAL_POSE basePose;
        SKELETAL_POSE layerPose;
        return sampleSkeletalClip(skeleton, baseClip, baseTime, basePose) &&
               sampleSkeletalClip(skeleton, layerClip, layerTime, layerPose) &&
               composeSkeletalPosesAbsolute(skeleton, basePose, layerPose, layerWeight, out);
    }

    bool sampleSkeletalClipsAdditive(const COMPILED_SKELETON &skeleton,
                                     const SKELETAL_CLIP &baseClip, const float baseTime,
                                     const SKELETAL_CLIP &layerClip, const float layerTime,
                                     const float layerWeight, SKELETAL_POSE &out)
    {
        SKELETAL_POSE basePose;
        SKELETAL_POSE layerPose;
        return sampleSkeletalClip(skeleton, baseClip, baseTime, basePose) &&
               sampleSkeletalClip(skeleton, layerClip, layerTime, layerPose) &&
               composeSkeletalPosesAdditive(skeleton, basePose, layerPose, layerWeight, out);
    }

    bool advanceSkeletalAbsoluteFade(const float startWeight, const float targetWeight,
                                     const float duration, const float delta, float &elapsed,
                                     float &weight, bool &complete) noexcept
    {
        complete = false;
        if (!std::isfinite(startWeight) || startWeight < 0.0f || startWeight > 1.0f ||
            !std::isfinite(targetWeight) || targetWeight < 0.0f || targetWeight > 1.0f ||
            !std::isfinite(duration) || duration <= 0.0f || !std::isfinite(delta) || delta < 0.0f ||
            !std::isfinite(elapsed) || elapsed < 0.0f || elapsed > duration)
            return false;
        elapsed = std::min(duration, elapsed + delta);
        const float ratio = elapsed / duration;
        weight = startWeight + (targetWeight - startWeight) * ratio;
        complete = elapsed >= duration;
        if (complete)
            weight = targetWeight;
        return true;
    }

    bool skinVerticesLbsReference(const CANONICAL_SKELETON &skeleton, const CANONICAL_WEIGHTS &weights,
                                  const SKELETAL_POSE &pose, const std::vector<VEC3> &bindPositions,
                                  const std::vector<VEC3> &bindNormals, std::vector<VEC3> &outPositions,
                                  std::vector<VEC3> &outNormals) noexcept
    {
        outPositions.clear();
        outNormals.clear();
        const size_t vertexCount = bindPositions.size();
        if (weights.vertices.size() != vertexCount ||
            (!bindNormals.empty() && bindNormals.size() != vertexCount) ||
            pose.globalTransforms.size() != skeleton.compiled.bones.size() ||
            !validateCanonicalWeights(skeleton, weights, static_cast<uint32_t>(vertexCount)))
            return false;

        std::vector<MATRIX> paletteMatrices(weights.paletteBoneIds.size());
        std::vector<MATRIX> paletteNormalMatrices;
        if (!bindNormals.empty())
            paletteNormalMatrices.resize(weights.paletteBoneIds.size());
        for (size_t paletteIndex = 0; paletteIndex < weights.paletteBoneIds.size(); ++paletteIndex)
        {
            const auto found = skeleton.compiled.indexById.find(weights.paletteBoneIds[paletteIndex]);
            if (found == skeleton.compiled.indexById.end())
                return false;
            const size_t boneIndex = static_cast<size_t>(found->second);
            MatrixMultiply(&paletteMatrices[paletteIndex],
                           &skeleton.compiled.bones[boneIndex].inverseGlobalBindMatrix,
                           &pose.globalTransforms[boneIndex]);
            if (!bindNormals.empty())
            {
                MATRIX inverse;
                float determinant = 0.0f;
                MatrixInverse(&inverse, &determinant, &paletteMatrices[paletteIndex]);
                if (!std::isfinite(determinant) || std::fabs(determinant) <= SINGULAR_TOLERANCE)
                    return false;
                MATRIX &normalMatrix = paletteNormalMatrices[paletteIndex];
                for (uint8_t row = 0; row < 4; ++row)
                    for (uint8_t column = 0; column < 4; ++column)
                        normalMatrix.m[row][column] = inverse.m[column][row];
            }
        }

        outPositions.resize(vertexCount);
        if (!bindNormals.empty())
            outNormals.resize(vertexCount);
        for (size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
        {
            VEC3 position(0.0f, 0.0f, 0.0f);
            VEC3 normal(0.0f, 0.0f, 0.0f);
            const CANONICAL_VERTEX_WEIGHT &influence = weights.vertices[vertexIndex];
            for (uint8_t slot = 0; slot < 4; ++slot)
            {
                if (influence.paletteIndex[slot] == UINT16_MAX)
                    continue;
                const MATRIX &matrix = paletteMatrices[influence.paletteIndex[slot]];
                const float weight = influence.weight[slot];
                const VEC3 &sourcePosition = bindPositions[vertexIndex];
                position.x += (sourcePosition.x * matrix._11 + sourcePosition.y * matrix._21 +
                               sourcePosition.z * matrix._31 + matrix._41) * weight;
                position.y += (sourcePosition.x * matrix._12 + sourcePosition.y * matrix._22 +
                               sourcePosition.z * matrix._32 + matrix._42) * weight;
                position.z += (sourcePosition.x * matrix._13 + sourcePosition.y * matrix._23 +
                               sourcePosition.z * matrix._33 + matrix._43) * weight;
                if (!bindNormals.empty())
                {
                    const MATRIX &normalMatrix = paletteNormalMatrices[influence.paletteIndex[slot]];
                    const VEC3 &sourceNormal = bindNormals[vertexIndex];
                    normal.x += (sourceNormal.x * normalMatrix._11 + sourceNormal.y * normalMatrix._21 +
                                 sourceNormal.z * normalMatrix._31) * weight;
                    normal.y += (sourceNormal.x * normalMatrix._12 + sourceNormal.y * normalMatrix._22 +
                                 sourceNormal.z * normalMatrix._32) * weight;
                    normal.z += (sourceNormal.x * normalMatrix._13 + sourceNormal.y * normalMatrix._23 +
                                 sourceNormal.z * normalMatrix._33) * weight;
                }
            }
            outPositions[vertexIndex] = position;
            if (!bindNormals.empty())
            {
                const float length = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                if (length <= QUATERNION_ZERO_EPSILON)
                    return false;
                outNormals[vertexIndex] = VEC3(normal.x / length, normal.y / length, normal.z / length);
            }
        }
        return true;
    }

    bool skinVerticesDqsRigidReference(const CANONICAL_SKELETON &skeleton, const CANONICAL_WEIGHTS &weights,
                                       const SKELETAL_POSE &pose, const std::vector<VEC3> &bindPositions,
                                       const std::vector<VEC3> &bindNormals, std::vector<VEC3> &outPositions,
                                       std::vector<VEC3> &outNormals) noexcept
    {
        outPositions.clear();
        outNormals.clear();
        const size_t vertexCount = bindPositions.size();
        if (weights.vertices.size() != vertexCount ||
            (!bindNormals.empty() && bindNormals.size() != vertexCount) ||
            pose.globalTransforms.size() != skeleton.compiled.bones.size() ||
            !validateCanonicalWeights(skeleton, weights, static_cast<uint32_t>(vertexCount)))
            return false;

        std::vector<DUAL_QUATERNION> palette(weights.paletteBoneIds.size());
        for (size_t paletteIndex = 0; paletteIndex < weights.paletteBoneIds.size(); ++paletteIndex)
        {
            const auto found = skeleton.compiled.indexById.find(weights.paletteBoneIds[paletteIndex]);
            if (found == skeleton.compiled.indexById.end())
                return false;
            const size_t boneIndex = static_cast<size_t>(found->second);
            MATRIX skinMatrix;
            MatrixMultiply(&skinMatrix, &skeleton.compiled.bones[boneIndex].inverseGlobalBindMatrix,
                           &pose.globalTransforms[boneIndex]);
            if (!rigidDualQuaternionFromMatrix(skinMatrix, palette[paletteIndex]))
                return false;
        }

        outPositions.resize(vertexCount);
        if (!bindNormals.empty())
            outNormals.resize(vertexCount);
        for (size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex)
        {
            QUATERNION blendedReal = {0, 0, 0, 0};
            QUATERNION blendedDual = {0, 0, 0, 0};
            QUATERNION reference;
            bool hasReference = false;
            const CANONICAL_VERTEX_WEIGHT &influence = weights.vertices[vertexIndex];
            for (uint8_t slot = 0; slot < 4; ++slot)
            {
                if (influence.paletteIndex[slot] == UINT16_MAX)
                    continue;
                const DUAL_QUATERNION &source = palette[influence.paletteIndex[slot]];
                if (!hasReference)
                {
                    reference = source.real;
                    hasReference = true;
                }
                const float dot = reference.x * source.real.x + reference.y * source.real.y +
                                  reference.z * source.real.z + reference.w * source.real.w;
                const float signedWeight = dot < 0.0f ? -influence.weight[slot] : influence.weight[slot];
                blendedReal.x += source.real.x * signedWeight;
                blendedReal.y += source.real.y * signedWeight;
                blendedReal.z += source.real.z * signedWeight;
                blendedReal.w += source.real.w * signedWeight;
                blendedDual.x += source.dual.x * signedWeight;
                blendedDual.y += source.dual.y * signedWeight;
                blendedDual.z += source.dual.z * signedWeight;
                blendedDual.w += source.dual.w * signedWeight;
            }
            const float norm = std::sqrt(blendedReal.x * blendedReal.x + blendedReal.y * blendedReal.y +
                                         blendedReal.z * blendedReal.z + blendedReal.w * blendedReal.w);
            if (!hasReference || norm <= QUATERNION_ZERO_EPSILON)
                return false;
            blendedReal.x /= norm; blendedReal.y /= norm; blendedReal.z /= norm; blendedReal.w /= norm;
            blendedDual.x /= norm; blendedDual.y /= norm; blendedDual.z /= norm; blendedDual.w /= norm;
            const float dualProjection = blendedReal.x * blendedDual.x + blendedReal.y * blendedDual.y +
                                         blendedReal.z * blendedDual.z + blendedReal.w * blendedDual.w;
            blendedDual.x -= blendedReal.x * dualProjection;
            blendedDual.y -= blendedReal.y * dualProjection;
            blendedDual.z -= blendedReal.z * dualProjection;
            blendedDual.w -= blendedReal.w * dualProjection;

            const QUATERNION translationQ = multiplyQuaternion(
                blendedDual, conjugateQuaternion(blendedReal));
            const VEC3 translation(2.0f * translationQ.x, 2.0f * translationQ.y,
                                   2.0f * translationQ.z);
            const VEC3 rotatedPosition = rotateVectorByQuaternion(bindPositions[vertexIndex], blendedReal);
            outPositions[vertexIndex] = VEC3(rotatedPosition.x + translation.x,
                                             rotatedPosition.y + translation.y,
                                             rotatedPosition.z + translation.z);
            if (!bindNormals.empty())
            {
                VEC3 normal = rotateVectorByQuaternion(bindNormals[vertexIndex], blendedReal);
                const float normalLength = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
                if (normalLength <= QUATERNION_ZERO_EPSILON)
                    return false;
                outNormals[vertexIndex] = VEC3(normal.x / normalLength, normal.y / normalLength,
                                               normal.z / normalLength);
            }
        }
        return true;
    }
}
