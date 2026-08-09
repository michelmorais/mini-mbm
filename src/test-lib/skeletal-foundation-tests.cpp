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

#include "skeletal-foundation-tests.h"

#include <skeletal-animation-foundation.h>

#include <cmath>
#include <cstdio>
#include <limits>
#include <vector>

namespace
{
    using namespace mbm;
    using namespace mbm::skeletal;

    int failures = 0;

    void expect(const bool condition, const char *message)
    {
        if (!condition)
        {
            std::fprintf(stderr, "[skeletal-foundation] FAIL: %s\n", message);
            ++failures;
        }
    }

    bool hasDiagnostic(const COMPILED_SKELETON &skeleton, const DIAGNOSTIC_CODE code)
    {
        for (const DIAGNOSTIC &diagnostic : skeleton.diagnostics)
        {
            if (diagnostic.code == code)
                return true;
        }
        return false;
    }

    bool hasDiagnostic(const WEIGHT_VALIDATION_REPORT &report, const DIAGNOSTIC_CODE code)
    {
        for (const DIAGNOSTIC &diagnostic : report.diagnostics)
        {
            if (diagnostic.code == code)
                return true;
        }
        return false;
    }

    util::SKELETON_BONE_V11 makeBone(const char *name, const char *parent,
                                     const float x, const float y, const float z)
    {
        util::SKELETON_BONE_V11 bone;
        bone.name = name;
        bone.parentName = parent;
        bone.x = x;
        bone.y = y;
        bone.z = z;
        return bone;
    }

    void testTrsRoundTrip()
    {
        LOCAL_TRANSFORM source;
        source.translation = VEC3(3.0f, -2.0f, 7.0f);
        source.rotation = {0.18257418f, 0.36514837f, -0.18257418f, 0.89442719f};
        source.scale = VEC3(2.0f, 3.0f, 4.0f);
        const MATRIX matrix = buildTrsMatrix(source);

        LOCAL_TRANSFORM decomposed;
        bool negativeScale = false;
        bool shear = false;
        expect(decomposeTrsMatrix(matrix, decomposed, negativeScale, shear), "TRS matrix must decompose");
        expect(!negativeScale, "positive TRS must not report negative scale");
        expect(!shear, "pure TRS must not report shear");
        const MATRIX reconstructed = buildTrsMatrix(decomposed);
        expect(maximumMatrixDifference(matrix, reconstructed) <= matrixComparisonTolerance(matrix, reconstructed),
               "TRS decomposition must reconstruct the original matrix");
    }

    void testHierarchyAndIdentity()
    {
        std::vector<util::SKELETON_BONE_V11> legacy;
        legacy.push_back(makeBone("root", "", 2.0f, 3.0f, 4.0f));
        legacy.back().rotZ = 90.0f;
        legacy.push_back(makeBone("child", "root", 2.0f, 5.0f, 4.0f));

        COMPILED_SKELETON compiled;
        expect(compileLegacySkeleton(legacy, compiled), "valid parent-before-child skeleton must compile");
        expect(compiled.bones.size() == 2, "compiled skeleton must preserve bone count");
        if (compiled.bones.size() != 2)
            return;

        expect(compiled.bones[0].parentIndex == -1, "root parent index must be -1");
        expect(compiled.bones[0].parentBoneId == 0, "root parent ID must be zero");
        expect(compiled.bones[1].parentIndex == 0, "child must resolve its parent by compiled index");
        expect(compiled.bones[1].parentBoneId == compiled.bones[0].boneId,
               "child parent ID must match the resolved parent identity");
        expect(compiled.indexByName.at("child") == 1 &&
               compiled.indexById.at(compiled.bones[1].boneId) == 1,
               "compiled name and ID lookup tables must resolve the child");
        expect(compiled.maximumReconstructionError <= MATRIX_TOLERANCE,
               "local * parentGlobal must reconstruct child global bind");
        expect(compiled.maximumBindIdentityError <= MATRIX_TOLERANCE,
               "inverseGlobalBind * globalBind must be identity");

        MATRIX reconstructed;
        MatrixMultiply(&reconstructed, &compiled.bones[1].localBindMatrix,
                       &compiled.bones[0].globalBindMatrix);
        expect(maximumMatrixDifference(reconstructed, compiled.bones[1].globalBindMatrix) <= MATRIX_TOLERANCE,
               "row-vector hierarchy composition order must remain local * parentGlobal");

        COMPILED_SKELETON second;
        expect(compileLegacySkeleton(legacy, second), "same skeleton must compile repeatedly");
        expect(second.bones.size() == 2 &&
               second.bones[0].boneId == compiled.bones[0].boneId &&
               second.bones[1].boneId == compiled.bones[1].boneId,
               "bone IDs must be deterministic from hierarchy paths");
        expect(compiled.bones[0].boneId != compiled.bones[1].boneId, "distinct paths must produce distinct IDs");

        legacy[0].x = 200.0f;
        legacy[0].y = 300.0f;
        legacy[0].z = 400.0f;
        legacy[1].x = 200.0f;
        legacy[1].y = 500.0f;
        legacy[1].z = 400.0f;
        expect(compileLegacySkeleton(legacy, compiled), "scale-100 hierarchy must compile under scaled tolerance");
    }

    void testValidation()
    {
        COMPILED_SKELETON compiled;
        std::vector<util::SKELETON_BONE_V11> duplicate = {
            makeBone("root", "", 0.0f, 0.0f, 0.0f),
            makeBone("root", "", 0.0f, 1.0f, 0.0f)
        };
        expect(!compileLegacySkeleton(duplicate, compiled), "duplicate bone names must fail compilation");
        expect(hasDiagnostic(compiled, DIAGNOSTIC_CODE::DUPLICATE_NAME), "duplicate name diagnostic must be explicit");

        std::vector<util::SKELETON_BONE_V11> forward = {
            makeBone("child", "root", 0.0f, 1.0f, 0.0f),
            makeBone("root", "", 0.0f, 0.0f, 0.0f)
        };
        expect(!compileLegacySkeleton(forward, compiled), "parent-after-child input must fail compilation");
        expect(hasDiagnostic(compiled, DIAGNOSTIC_CODE::UNKNOWN_PARENT), "unknown parent diagnostic must be explicit");

        std::vector<util::SKELETON_BONE_V11> nonFinite = {
            makeBone("root", "", 0.0f, 0.0f, 0.0f)
        };
        nonFinite[0].rotX = std::numeric_limits<float>::quiet_NaN();
        expect(!compileLegacySkeleton(nonFinite, compiled), "non-finite transforms must fail compilation");
        expect(hasDiagnostic(compiled, DIAGNOSTIC_CODE::NON_FINITE_TRANSFORM),
               "non-finite transform diagnostic must be explicit");

        std::vector<util::SKELETON_BONE_V11> singular = {
            makeBone("root", "", 0.0f, 0.0f, 0.0f)
        };
        singular[0].scaleY = 0.0f;
        expect(!compileLegacySkeleton(singular, compiled), "singular bind transforms must fail compilation");
        expect(hasDiagnostic(compiled, DIAGNOSTIC_CODE::SINGULAR_TRANSFORM),
               "singular transform diagnostic must be explicit");

        std::vector<util::SKELETON_BONE_V11> reflected = {
            makeBone("root", "", 0.0f, 0.0f, 0.0f)
        };
        reflected[0].scaleX = -1.0f;
        expect(compileLegacySkeleton(reflected, compiled),
               "negative scale must be retained in the compiled diagnostic representation");
        expect(compiled.bones.size() == 1 && compiled.bones[0].hasNegativeScale,
               "negative scale classification must be retained");
        expect(hasDiagnostic(compiled, DIAGNOSTIC_CODE::NEGATIVE_SCALE),
               "negative scale must produce a non-fatal capability diagnostic");
    }

    util::VERTEX_BONE_WEIGHT_V11 makeWeights(const uint8_t firstIndex, const float firstWeight,
                                             const uint8_t secondIndex = UINT8_MAX,
                                             const float secondWeight = 0.0f)
    {
        util::VERTEX_BONE_WEIGHT_V11 weights;
        weights.paletteIndex[0] = firstIndex;
        weights.weight[0] = firstWeight;
        weights.paletteIndex[1] = secondIndex;
        weights.weight[1] = secondWeight;
        return weights;
    }

    void testWeightValidation()
    {
        std::vector<util::SKELETON_BONE_V11> legacy = {
            makeBone("root", "", 0.0f, 0.0f, 0.0f),
            makeBone("child", "root", 0.0f, 1.0f, 0.0f)
        };
        COMPILED_SKELETON skeleton;
        expect(compileLegacySkeleton(legacy, skeleton), "weight fixture skeleton must compile");

        const std::vector<std::string> palette = {"root", "child"};
        const std::vector<util::VERTEX_BONE_WEIGHT_V11> validWeights = {
            makeWeights(0, 1.0f),
            makeWeights(0, 0.25f, 1, 0.75f)
        };
        WEIGHT_VALIDATION_REPORT report;
        expect(validateLegacyWeights(skeleton, palette, validWeights, 2, report),
               "valid weights must pass structural validation");
        expect(report.diagnostics.empty(), "valid weights must have no quality diagnostics");
        expect(report.paletteBoneIndices.size() == 2 && report.paletteBoneIndices[0] == 0 &&
               report.paletteBoneIndices[1] == 1, "weight palette names must resolve to compiled bone indices");
        expect(validWeights[1].weight[0] == 0.25f && validWeights[1].weight[1] == 0.75f,
               "validation must not mutate source weights");

        std::vector<util::VERTEX_BONE_WEIGHT_V11> qualityWeights = {
            makeWeights(0, 0.25f),
            makeWeights(UINT8_MAX, 0.0f)
        };
        expect(validateLegacyWeights(skeleton, palette, qualityWeights, 2, report),
               "weight-quality findings must remain non-fatal for legacy editor data");
        expect(hasDiagnostic(report, DIAGNOSTIC_CODE::WEIGHT_SUM_MISMATCH),
               "unnormalized weights must be reported without normalization");
        expect(hasDiagnostic(report, DIAGNOSTIC_CODE::NO_EFFECTIVE_INFLUENCE),
               "unweighted legacy vertices must be reported without inventing an influence");
        expect(report.verticesWithInvalidWeightSum == 1 && report.verticesWithoutEffectiveInfluence == 1,
               "weight-quality aggregate counts must be deterministic");

        std::vector<util::VERTEX_BONE_WEIGHT_V11> invalidWeights = {
            makeWeights(2, 1.0f)
        };
        const std::vector<std::string> invalidPalette = {"missing"};
        expect(!validateLegacyWeights(skeleton, invalidPalette, invalidWeights, 2, report),
               "invalid references and vertex count must fail structural validation");
        expect(hasDiagnostic(report, DIAGNOSTIC_CODE::UNKNOWN_WEIGHT_BONE),
               "unknown palette bone must be diagnosed");
        expect(hasDiagnostic(report, DIAGNOSTIC_CODE::PALETTE_INDEX_OUT_OF_RANGE),
               "out-of-range palette index must be diagnosed");
        expect(hasDiagnostic(report, DIAGNOSTIC_CODE::VERTEX_COUNT_MISMATCH),
               "weight/geometry vertex-count mismatch must be diagnosed");

        invalidWeights = {makeWeights(0, -0.5f, 0, 1.5f)};
        expect(!validateLegacyWeights(skeleton, palette, invalidWeights, 1, report),
               "negative and duplicate influences must fail structural validation");
        expect(hasDiagnostic(report, DIAGNOSTIC_CODE::NEGATIVE_WEIGHT), "negative weight must be diagnosed");
        expect(hasDiagnostic(report, DIAGNOSTIC_CODE::DUPLICATE_BONE_INFLUENCE),
               "duplicate bone influence must be diagnosed");
    }

    void testClipSampling()
    {
        std::vector<util::SKELETON_BONE_V11> legacy = {
            makeBone("root", "", 0.0f, 0.0f, 0.0f),
            makeBone("child", "root", 0.0f, 1.0f, 0.0f)
        };
        COMPILED_SKELETON skeleton;
        expect(compileLegacySkeleton(legacy, skeleton), "clip fixture skeleton must compile");

        SKELETAL_CLIP clip;
        clip.clipId = 1;
        clip.name = "turn-and-move";
        clip.duration = 1.0f;
        SKELETAL_TRACK rootTrack;
        rootTrack.boneId = skeleton.bones[0].boneId;
        rootTrack.channelMask = SKELETAL_CHANNEL_TRANSLATION | SKELETAL_CHANNEL_ROTATION;
        SKELETAL_KEY start;
        start.local.translation = VEC3(0.0f, 0.0f, 0.0f);
        SKELETAL_KEY end = start;
        end.time = 1.0f;
        end.local.translation = VEC3(10.0f, 0.0f, 0.0f);
        end.local.rotation = {0.0f, 0.0f, 1.0f, 0.0f};
        rootTrack.keys = {start, end};
        clip.tracks.push_back(rootTrack);

        std::vector<DIAGNOSTIC> diagnostics;
        expect(validateSkeletalClip(skeleton, clip, diagnostics), "valid skeletal clip must pass validation");
        SKELETAL_POSE pose;
        expect(sampleSkeletalClip(skeleton, clip, 0.5f, pose, &diagnostics), "valid clip must sample");
        expect(pose.localTransforms.size() == 2 && pose.globalTransforms.size() == 2,
               "sampled pose must contain every skeleton bone");
        expect(std::fabs(pose.localTransforms[0].translation.x - 5.0f) <= MATRIX_TOLERANCE,
               "translation channel must interpolate at the requested time");
        expect(std::fabs(pose.localTransforms[0].scale.x - 1.0f) <= MATRIX_TOLERANCE,
               "an absent scale channel must preserve bind-local scale");
        expect(std::fabs(pose.globalTransforms[1]._41 - 4.0f) <= MATRIX_TOLERANCE &&
               std::fabs(pose.globalTransforms[1]._42) <= MATRIX_TOLERANCE,
               "sampled child global must use local * parentGlobal row-vector composition");

        clip.loop = true;
        SKELETAL_POSE wrapped;
        expect(sampleSkeletalClip(skeleton, clip, 1.25f, wrapped), "looping clip must sample wrapped time");
        expect(std::fabs(wrapped.localTransforms[0].translation.x - 2.5f) <= MATRIX_TOLERANCE,
               "loop sampling must wrap deterministically by clip duration");

        clip.tracks[0].keys[1].local.rotation = {0.0f, 0.0f, 0.0f, -1.0f};
        expect(sampleSkeletalClip(skeleton, clip, 0.5f, pose), "antipodal quaternion clip must sample");
        expect(std::fabs(pose.localTransforms[0].rotation.w - 1.0f) <= MATRIX_TOLERANCE,
               "antipodal quaternion interpolation must select the deterministic short path");

        clip.tracks[0].keys[1].local.rotation = {0.0f, 0.0f, 0.0f, 2.0f};
        expect(validateSkeletalClip(skeleton, clip, diagnostics),
               "non-unit nonzero quaternion must remain sampleable after normalization");
        bool foundQuaternionDiagnostic = false;
        for (const DIAGNOSTIC &diagnostic : diagnostics)
            foundQuaternionDiagnostic = foundQuaternionDiagnostic ||
                                        diagnostic.code == DIAGNOSTIC_CODE::NON_UNIT_KEY_QUATERNION;
        expect(foundQuaternionDiagnostic, "non-unit quaternion normalization must be diagnosed");

        clip.tracks[0].keys[1].time = 0.0f;
        expect(!validateSkeletalClip(skeleton, clip, diagnostics), "duplicate key times must fail validation");
        bool foundTimeDiagnostic = false;
        for (const DIAGNOSTIC &diagnostic : diagnostics)
            foundTimeDiagnostic = foundTimeDiagnostic || diagnostic.code == DIAGNOSTIC_CODE::NON_INCREASING_KEY_TIME;
        expect(foundTimeDiagnostic, "non-increasing key time must be diagnosed explicitly");
    }
}

int runSkeletalFoundationTests()
{
    failures = 0;
    testTrsRoundTrip();
    testHierarchyAndIdentity();
    testValidation();
    testWeightValidation();
    testClipSampling();
    if (failures == 0)
        std::fprintf(stdout, "[skeletal-foundation] PASS\n");
    return failures == 0 ? 0 : 1;
}
