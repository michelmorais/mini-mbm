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
#include <skeletal-render-capability.h>
#include <skeletal-gpu-lbs.h>
#include <core_mbm/mesh-manager.h>
#include <mesh-v11-io.h>
#include <mesh-io-primitives.h>

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

    bool hasDiagnostic(const std::vector<DIAGNOSTIC> &diagnostics, const DIAGNOSTIC_CODE code)
    {
        for (const DIAGNOSTIC &diagnostic : diagnostics)
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

        MATRIX sheared;
        MatrixIdentity(&sheared);
        sheared._12 = 0.25f;
        LOCAL_TRANSFORM decomposed;
        bool negativeScale = false;
        bool hasShear = false;
        expect(decomposeTrsMatrix(sheared, decomposed, negativeScale, hasShear),
               "invertible sheared matrix must remain diagnosable");
        expect(hasShear, "shear classification must be explicit");
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

        invalidWeights = {makeWeights(UINT8_MAX, 0.25f)};
        expect(!validateLegacyWeights(skeleton, palette, invalidWeights, 1, report),
               "nonzero weight in an unused slot must fail structural validation");
        expect(hasDiagnostic(report, DIAGNOSTIC_CODE::UNUSED_SLOT_NONZERO),
               "unused-slot sentinel misuse must be diagnosed");

        invalidWeights = {makeWeights(0, std::numeric_limits<float>::infinity())};
        expect(!validateLegacyWeights(skeleton, palette, invalidWeights, 1, report),
               "non-finite weight must fail structural validation");
        expect(hasDiagnostic(report, DIAGNOSTIC_CODE::NON_FINITE_WEIGHT),
               "non-finite weight must be diagnosed");

        const std::vector<std::string> duplicatePalette = {"root", "root"};
        invalidWeights = {makeWeights(0, 1.0f)};
        expect(!validateLegacyWeights(skeleton, duplicatePalette, invalidWeights, 1, report),
               "duplicate palette names must fail structural validation");
        expect(hasDiagnostic(report, DIAGNOSTIC_CODE::DUPLICATE_PALETTE_NAME),
               "duplicate palette name must be diagnosed");
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

        clip.tracks[0].keys[0].easing = SKELETAL_EASING::SMOOTHSTEP;
        expect(sampleSkeletalClip(skeleton, clip, 0.25f, pose), "smoothstep clip must sample");
        expect(std::fabs(pose.localTransforms[0].translation.x - 1.5625f) <= MATRIX_TOLERANCE,
               "segment easing must be owned by the departing key");
        clip.tracks[0].keys[0].easing = SKELETAL_EASING::LINEAR;

        expect(sampleSkeletalClip(skeleton, clip, -1.0f, pose), "non-looping clip must clamp negative time");
        expect(std::fabs(pose.localTransforms[0].translation.x) <= MATRIX_TOLERANCE,
               "negative non-looping sample time must clamp to clip start");

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

    void testClipCorruptionDiagnostics()
    {
        std::vector<util::SKELETON_BONE_V11> legacy = {makeBone("root", "", 0.0f, 0.0f, 0.0f)};
        COMPILED_SKELETON skeleton;
        expect(compileLegacySkeleton(legacy, skeleton), "clip corruption fixture skeleton must compile");

        SKELETAL_CLIP clip;
        clip.clipId = 1;
        clip.name = "invalid-cases";
        clip.duration = 1.0f;
        SKELETAL_TRACK track;
        track.boneId = skeleton.bones[0].boneId;
        track.keys.push_back(SKELETAL_KEY());
        clip.tracks.push_back(track);
        std::vector<DIAGNOSTIC> diagnostics;

        clip.tracks.push_back(track);
        expect(!validateSkeletalClip(skeleton, clip, diagnostics), "duplicate tracks must fail validation");
        expect(hasDiagnostic(diagnostics, DIAGNOSTIC_CODE::DUPLICATE_BONE_TRACK),
               "duplicate track target must be diagnosed");
        clip.tracks.resize(1);

        clip.tracks[0].boneId = 999;
        clip.tracks[0].channelMask = 0;
        expect(!validateSkeletalClip(skeleton, clip, diagnostics), "unknown target and empty mask must fail");
        expect(hasDiagnostic(diagnostics, DIAGNOSTIC_CODE::UNKNOWN_TRACK_BONE),
               "unknown track bone must be diagnosed");
        expect(hasDiagnostic(diagnostics, DIAGNOSTIC_CODE::INVALID_CHANNEL_MASK),
               "empty channel mask must be diagnosed");
        clip.tracks[0].boneId = skeleton.bones[0].boneId;
        clip.tracks[0].channelMask = SKELETAL_CHANNEL_ROTATION | SKELETAL_CHANNEL_SCALE;

        clip.tracks[0].keys[0].local.rotation = {0.0f, 0.0f, 0.0f, 0.0f};
        clip.tracks[0].keys[0].local.scale.y = 0.0f;
        expect(!validateSkeletalClip(skeleton, clip, diagnostics), "zero quaternion and scale must fail");
        expect(hasDiagnostic(diagnostics, DIAGNOSTIC_CODE::INVALID_KEY_QUATERNION),
               "zero quaternion must be diagnosed");
        expect(hasDiagnostic(diagnostics, DIAGNOSTIC_CODE::SINGULAR_KEY_SCALE),
               "singular animated scale must be diagnosed");

        clip.tracks[0].keys[0].local.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        clip.tracks[0].keys[0].local.scale = VEC3(1.0f, 2.0f, 1.0f);
        expect(validateSkeletalClip(skeleton, clip, diagnostics),
               "non-uniform scale must remain valid for an LBS-capable consumer");
        expect(hasDiagnostic(diagnostics, DIAGNOSTIC_CODE::NON_UNIFORM_KEY_SCALE),
               "non-uniform animated scale must produce a capability diagnostic");

        SKELETAL_POSE pose;
        expect(!sampleSkeletalClip(skeleton, clip, std::numeric_limits<float>::quiet_NaN(), pose, &diagnostics),
               "non-finite sample time must fail");
        expect(hasDiagnostic(diagnostics, DIAGNOSTIC_CODE::INVALID_SAMPLE_TIME),
               "non-finite sample time must be diagnosed");
    }

    VEC3 sampleScaleFixture(const float assetScale)
    {
        std::vector<util::SKELETON_BONE_V11> legacy = {
            makeBone("root", "", 2.0f * assetScale, 3.0f * assetScale, 4.0f * assetScale),
            makeBone("child", "root", 2.0f * assetScale, 5.0f * assetScale, 4.0f * assetScale)
        };
        COMPILED_SKELETON skeleton;
        expect(compileLegacySkeleton(legacy, skeleton), "scaled integration skeleton must compile");
        if (skeleton.bones.size() != 2)
            return VEC3();

        SKELETAL_CLIP clip;
        clip.clipId = 7;
        clip.name = "scaled-translation";
        clip.duration = 1.0f;
        SKELETAL_TRACK track;
        track.boneId = skeleton.bones[0].boneId;
        track.channelMask = SKELETAL_CHANNEL_TRANSLATION;
        SKELETAL_KEY start;
        start.local = skeleton.bones[0].localBind;
        SKELETAL_KEY end = start;
        end.time = 1.0f;
        end.local.translation.x += 2.0f * assetScale;
        track.keys = {start, end};
        clip.tracks.push_back(track);
        SKELETAL_POSE pose;
        expect(sampleSkeletalClip(skeleton, clip, 0.5f, pose), "scaled integration clip must sample");
        if (pose.globalTransforms.size() != 2)
            return VEC3();
        return VEC3(pose.globalTransforms[1]._41, pose.globalTransforms[1]._42,
                    pose.globalTransforms[1]._43);
    }

    void testScaleOneAndHundredEquivalence()
    {
        const VEC3 unit = sampleScaleFixture(1.0f);
        const VEC3 hundred = sampleScaleFixture(100.0f);
        expect(std::fabs(unit.x - hundred.x / 100.0f) <= MATRIX_TOLERANCE &&
               std::fabs(unit.y - hundred.y / 100.0f) <= MATRIX_TOLERANCE &&
               std::fabs(unit.z - hundred.z / 100.0f) <= MATRIX_TOLERANCE,
               "scale-1 and scale-100 fixtures must produce equivalent normalized global poses");
    }

    void testCanonicalSkeletonCompilation()
    {
        CANONICAL_BONE root;
        root.boneId = 10;
        root.name = "root";
        root.localBind.translation = VEC3(1.0f, 2.0f, 3.0f);
        CANONICAL_BONE child;
        child.boneId = 20;
        child.parentBoneId = 10;
        child.name = "child";
        child.localBind.translation = VEC3(0.0f, 2.0f, 0.0f);
        COMPILED_SKELETON compiled;
        expect(compileCanonicalSkeleton({root, child}, compiled),
               "valid canonical parent-first skeleton must compile");
        expect(compiled.bones.size() == 2 && compiled.bones[1].parentIndex == 0,
               "canonical skeleton must preserve IDs and resolve parent index");
        expect(compiled.maximumBindIdentityError <= MATRIX_TOLERANCE,
               "canonical inverse bind must produce identity");

        child.parentBoneId = 999;
        expect(!compileCanonicalSkeleton({root, child}, compiled) &&
               hasDiagnostic(compiled, DIAGNOSTIC_CODE::UNKNOWN_PARENT),
               "canonical skeleton must reject a forward or missing parent");
        child.parentBoneId = 10;
        child.localBind.rotation = {0.0f, 0.0f, 0.0f, 0.0f};
        expect(!compileCanonicalSkeleton({root, child}, compiled) &&
               hasDiagnostic(compiled, DIAGNOSTIC_CODE::INVALID_BIND_QUATERNION),
               "canonical skeleton must reject a zero bind quaternion");
        child = root;
        child.name = "duplicate-id";
        expect(!compileCanonicalSkeleton({root, child}, compiled) &&
               hasDiagnostic(compiled, DIAGNOSTIC_CODE::ID_COLLISION),
               "canonical skeleton must reject duplicate bone IDs");
    }

    bool writeCanonicalSkeletonFixture(const char *path, const bool zeroQuaternion,
                                       const uint32_t sectionCount)
    {
        FILE *fp = std::fopen(path, "wb+");
        if (!fp) return false;
        util::FILE_HEADER_V11 fileHeader;
        fileHeader.typeMesh = util::TYPE_MESH_3D;
        fileHeader.sectionCount = sectionCount;
        bool ok = util::writeFileHeaderV11(fp, fileHeader);
        for (uint32_t section = 0; ok && section < sectionCount; ++section)
        {
            util::SECTION_HEADER_V11 header;
            header.type = util::SECTION_SKELETAL_SKELETON;
            header.sectionVersion = 1;
            ok = util::writeSectionV11Streamed(fp, header, [zeroQuaternion](FILE *payload)
            {
                return util::le_io::writeU64LE(payload, 100) &&
                    util::le_io::writeU32LE(payload, 1) &&
                    util::le_io::writeU64LE(payload, 10) &&
                    util::le_io::writeU64LE(payload, 0) &&
                    util::writeStringV11(payload, "root") &&
                    util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                    util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                    util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                    util::le_io::writeF32LE(payload, zeroQuaternion ? 0.0f : 1.0f) &&
                    util::le_io::writeF32LE(payload, 1) && util::le_io::writeF32LE(payload, 1) &&
                    util::le_io::writeF32LE(payload, 1) && util::le_io::writeF32LE(payload, 0.1f) &&
                    util::le_io::writeF32LE(payload, 1.0f);
            });
        }
        std::fclose(fp);
        return ok;
    }

    void testCanonicalSkeletonReader()
    {
        const char *validPath = "/tmp/mini-mbm-canonical-skeleton-valid.msh";
        const char *invalidPath = "/tmp/mini-mbm-canonical-skeleton-invalid.msh";
        const char *duplicatePath = "/tmp/mini-mbm-canonical-skeleton-duplicate.msh";
        expect(writeCanonicalSkeletonFixture(validPath, false, 1), "canonical fixture must write");
        MESH_MBM_DEBUG mesh;
        expect(mesh.loadV11(validPath), "both-loader canonical skeleton reader must accept valid payload");
        expect(writeCanonicalSkeletonFixture(invalidPath, true, 1), "invalid canonical fixture must write");
        expect(!mesh.loadV11(invalidPath), "canonical reader must reject zero bind quaternion");
        expect(writeCanonicalSkeletonFixture(duplicatePath, false, 2), "duplicate canonical fixture must write");
        expect(!mesh.loadV11(duplicatePath), "canonical reader must reject duplicate skeleton sections");
        std::remove(validPath); std::remove(invalidPath); std::remove(duplicatePath);
    }

    void testCanonicalWeightValidation()
    {
        CANONICAL_BONE root;
        root.boneId = 10; root.name = "root";
        CANONICAL_SKELETON skeleton;
        skeleton.skeletonId = 100;
        skeleton.sourceBones = {root};
        expect(compileCanonicalSkeleton(skeleton.sourceBones, skeleton.compiled),
               "canonical weight fixture skeleton must compile");
        CANONICAL_WEIGHTS weights;
        weights.skeletonId = 100;
        weights.paletteBoneIds = {10};
        weights.vertices.resize(1);
        weights.vertices[0].paletteIndex[0] = 0;
        weights.vertices[0].weight[0] = 1.0f;
        expect(validateCanonicalWeights(skeleton, weights, 1),
               "complete canonical weights must validate");
        weights.skeletonId = 101;
        expect(!validateCanonicalWeights(skeleton, weights, 1),
               "canonical weights must match skeletonId");
        weights.skeletonId = 100;
        weights.vertices[0].weight[0] = 0.5f;
        expect(!validateCanonicalWeights(skeleton, weights, 1),
               "canonical weights must sum to one");
        weights.vertices[0].weight[0] = 1.0f;
        weights.paletteBoneIds[0] = 999;
        expect(!validateCanonicalWeights(skeleton, weights, 1),
               "canonical palette must target an existing bone ID");
        weights.paletteBoneIds[0] = 10;
        weights.vertices[0].paletteIndex[1] = 0;
        weights.vertices[0].weight[1] = 0.1f;
        expect(!validateCanonicalWeights(skeleton, weights, 1),
               "canonical vertex must not repeat a palette influence");
    }

    bool writeCanonicalWeightedFixture(const char *path, const uint64_t weightSkeletonId,
                                       const float weightValue, const uint64_t animationSkeletonId = 100,
                                       const uint64_t animationBoneId = 10, const uint8_t easing = 0,
                                       const bool writeRenderableSubset = false)
    {
        FILE *fp = std::fopen(path, "wb+");
        if (!fp) return false;
        util::FILE_HEADER_V11 fileHeader;
        fileHeader.typeMesh = util::TYPE_MESH_3D;
        fileHeader.sectionCount = 4;
        bool ok = util::writeFileHeaderV11(fp, fileHeader);

        util::SECTION_HEADER_V11 animationHeader;
        animationHeader.type = util::SECTION_SKELETAL_ANIMATION;
        animationHeader.sectionVersion = 1;
        ok = ok && util::writeSectionV11Streamed(fp, animationHeader,
            [animationSkeletonId, animationBoneId, easing](FILE *payload)
            {
                const uint8_t loop = 1, reserved[3] = {0, 0, 0}, channel = 1;
                return util::le_io::writeU64LE(payload, animationSkeletonId) &&
                    util::le_io::writeU32LE(payload, 1) && util::le_io::writeU64LE(payload, 200) &&
                    util::writeStringV11(payload, "walk") && util::le_io::writeF32LE(payload, 1) &&
                    util::le_io::writeBytes(payload, &loop, 1) && util::le_io::writeBytes(payload, reserved, 3) &&
                    util::le_io::writeU32LE(payload, 1) && util::le_io::writeU64LE(payload, animationBoneId) &&
                    util::le_io::writeBytes(payload, &channel, 1) && util::le_io::writeBytes(payload, reserved, 3) &&
                    util::le_io::writeU32LE(payload, 1) && util::le_io::writeF32LE(payload, 0) &&
                    util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                    util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                    util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                    util::le_io::writeF32LE(payload, 1) && util::le_io::writeF32LE(payload, 1) &&
                    util::le_io::writeF32LE(payload, 1) && util::le_io::writeF32LE(payload, 1) &&
                    util::le_io::writeBytes(payload, &easing, 1) && util::le_io::writeBytes(payload, reserved, 3) &&
                    util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                    util::le_io::writeF32LE(payload, 1) && util::le_io::writeF32LE(payload, 1);
            });

        util::SECTION_HEADER_V11 weightsHeader;
        weightsHeader.type = util::SECTION_SKELETAL_WEIGHTS;
        weightsHeader.sectionVersion = 1;
        ok = ok && util::writeSectionV11Streamed(fp, weightsHeader,
            [weightSkeletonId, weightValue, writeRenderableSubset](FILE *payload)
            {
                const uint32_t vertexCount = writeRenderableSubset ? 3 : 1;
                return util::le_io::writeU64LE(payload, weightSkeletonId) &&
                    util::le_io::writeU32LE(payload, 0) && util::le_io::writeU32LE(payload, vertexCount) &&
                    util::le_io::writeU32LE(payload, 1) && util::le_io::writeU64LE(payload, 10) &&
                    [&]()
                    {
                        for (uint32_t vertex = 0; vertex < vertexCount; ++vertex)
                        {
                            if (!util::le_io::writeU16LE(payload, 0) ||
                                !util::le_io::writeU16LE(payload, UINT16_MAX) ||
                                !util::le_io::writeU16LE(payload, UINT16_MAX) ||
                                !util::le_io::writeU16LE(payload, UINT16_MAX) ||
                                !util::le_io::writeF32LE(payload, weightValue) ||
                                !util::le_io::writeF32LE(payload, 0) || !util::le_io::writeF32LE(payload, 0) ||
                                !util::le_io::writeF32LE(payload, 0))
                                return false;
                        }
                        return true;
                    }();
            });

        util::SECTION_HEADER_V11 frameSection;
        frameSection.type = util::SECTION_FRAME_STATIC;
        frameSection.sectionVersion = 1;
        ok = ok && util::writeSectionV11Streamed(fp, frameSection, [writeRenderableSubset](FILE *payload)
        {
            util::FRAME_HEADER_V11 frame;
            frame.totalSubset = writeRenderableSubset ? 1 : 0;
            frame.vertexCount = writeRenderableSubset ? 3 : 1;
            frame.indexWidth = 16;
            frame.hasNormal = 0; frame.hasUv = 0; frame.uvSource = 0; frame.indexCount = 0;
            if (!util::writeFrameHeaderV11(payload, frame))
                return false;
            for (uint32_t vertex = 0; vertex < frame.vertexCount; ++vertex)
                if (!util::le_io::writeF32LE(payload, static_cast<float>(vertex == 1)) ||
                    !util::le_io::writeF32LE(payload, static_cast<float>(vertex == 2)) ||
                    !util::le_io::writeF32LE(payload, 0))
                    return false;
            if (!writeRenderableSubset)
                return true;
            util::SUBSET_DESC_V11 subset;
            subset.primaryTexture.storage = util::TEXTURE_REF_STORAGE_PATH;
            subset.primaryTexture.path = "#FFFFFFFF";
            subset.vertexCount = 3;
            subset.vertexStart = 0;
            subset.indexStart = 0;
            subset.indexCount = 0;
            subset.alphaColor[0] = 1;
            return util::writeSubsetDescV11(payload, subset);
        });

        util::SECTION_HEADER_V11 skeletonHeader;
        skeletonHeader.type = util::SECTION_SKELETAL_SKELETON;
        skeletonHeader.sectionVersion = 1;
        ok = ok && util::writeSectionV11Streamed(fp, skeletonHeader, [](FILE *payload)
        {
            return util::le_io::writeU64LE(payload, 100) && util::le_io::writeU32LE(payload, 1) &&
                util::le_io::writeU64LE(payload, 10) && util::le_io::writeU64LE(payload, 0) &&
                util::writeStringV11(payload, "root") &&
                util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                util::le_io::writeF32LE(payload, 1) && util::le_io::writeF32LE(payload, 1) &&
                util::le_io::writeF32LE(payload, 1) && util::le_io::writeF32LE(payload, 1) &&
                util::le_io::writeF32LE(payload, 0.1f) && util::le_io::writeF32LE(payload, 1.0f);
        });
        std::fclose(fp);
        return ok;
    }

    void testCanonicalWeightReader()
    {
        const char *valid = "/tmp/mini-mbm-canonical-weights-valid.msh";
        const char *wrongId = "/tmp/mini-mbm-canonical-weights-id.msh";
        const char *badSum = "/tmp/mini-mbm-canonical-weights-sum.msh";
        MESH_MBM_DEBUG mesh;
        expect(writeCanonicalWeightedFixture(valid, 100, 1.0f) && mesh.loadV11(valid),
               "canonical weight reader must resolve sections independent of file order");
        SKELETON_BIND_SUMMARY summary;
        SKELETON_BIND_BONE_INFO bone;
        expect(mesh.refreshSkeletonBindReport() && mesh.getSkeletonBindSummary(summary) &&
                   summary.valid && summary.canonical && summary.boneCount == 1 && mesh.getSkeletonBindBone(0, bone) &&
                   std::string(mesh.getSkeletonBindBoneName(0)) == "root",
               "bind report must inspect canonical section 41 without populating legacy bones");
        expect(mesh.getTotalBone() == 0,
               "canonical bind inspection must not create a legacy skeleton compatibility copy");
        expect(writeCanonicalWeightedFixture(wrongId, 101, 1.0f) && !mesh.loadV11(wrongId),
               "canonical weight reader must reject skeletonId mismatch");
        expect(writeCanonicalWeightedFixture(badSum, 100, 0.5f) && !mesh.loadV11(badSum),
               "canonical weight reader must reject non-unit vertex sums");
        std::remove(valid); std::remove(wrongId); std::remove(badSum);
    }

    void testCanonicalAnimationReader()
    {
        const char *wrongId = "/tmp/mini-mbm-canonical-animation-id.msh";
        const char *unknownBone = "/tmp/mini-mbm-canonical-animation-bone.msh";
        const char *badEasing = "/tmp/mini-mbm-canonical-animation-easing.msh";
        MESH_MBM_DEBUG mesh;
        expect(writeCanonicalWeightedFixture(wrongId, 100, 1.0f, 101) && !mesh.loadV11(wrongId),
               "canonical animation reader must reject skeletonId mismatch");
        expect(writeCanonicalWeightedFixture(unknownBone, 100, 1.0f, 100, 999) && !mesh.loadV11(unknownBone),
               "canonical animation reader must reject unknown track bone IDs");
        expect(writeCanonicalWeightedFixture(badEasing, 100, 1.0f, 100, 10, 9) && !mesh.loadV11(badEasing),
               "canonical animation reader must reject invalid easing values");
        std::remove(wrongId); std::remove(unknownBone); std::remove(badEasing);
    }

    void testCanonicalWriterRoundTrip()
    {
        const char *source = "/tmp/mini-mbm-canonical-writer-source.msh";
        const char *roundTrip = "/tmp/mini-mbm-canonical-writer-round-trip.msh";
        MESH_MBM_DEBUG mesh;
        char error[512] = "";
        expect(writeCanonicalWeightedFixture(source, 100, 1.0f, 100, 10, 0, true) && mesh.loadV11(source),
               "canonical writer source fixture must load");
        expect(mesh.saveV11(roundTrip, false, false, false, error, sizeof(error) - 1),
               "canonical writer must save validated sections 41-43");

        MESH_MBM_DEBUG reloaded;
        expect(reloaded.loadV11(roundTrip),
               "canonical writer output must reload with all dependencies intact");

        FILE *fp = std::fopen(roundTrip, "rb");
        util::FILE_HEADER_V11 fileHeader;
        uint32_t canonicalTypes[3] = {0, 0, 0};
        uint32_t canonicalCount = 0;
        bool inspected = fp && util::readFileHeaderV11(fp, fileHeader);
        for (uint32_t index = 0; inspected && index < fileHeader.sectionCount; ++index)
        {
            util::SECTION_HEADER_V11 sectionHeader;
            std::vector<uint8_t> payload;
            inspected = util::readSectionV11(fp, sectionHeader, payload);
            if (inspected && sectionHeader.type >= util::SECTION_SKELETAL_SKELETON &&
                sectionHeader.type <= util::SECTION_SKELETAL_ANIMATION)
            {
                if (canonicalCount < 3)
                    canonicalTypes[canonicalCount] = sectionHeader.type;
                ++canonicalCount;
            }
        }
        if (fp) std::fclose(fp);
        expect(inspected && canonicalCount == 3 &&
                   canonicalTypes[0] == util::SECTION_SKELETAL_SKELETON &&
                   canonicalTypes[1] == util::SECTION_SKELETAL_WEIGHTS &&
                   canonicalTypes[2] == util::SECTION_SKELETAL_ANIMATION,
               "canonical writer must emit exactly one ordered 41-42-43 section group");
        std::remove(source); std::remove(roundTrip);
    }

    void testCanonicalAnimationValidation()
    {
        CANONICAL_BONE root;
        root.boneId = 10; root.name = "root";
        CANONICAL_SKELETON skeleton;
        skeleton.skeletonId = 100; skeleton.sourceBones = {root};
        expect(compileCanonicalSkeleton(skeleton.sourceBones, skeleton.compiled),
               "canonical animation fixture skeleton must compile");
        SKELETAL_CLIP clip;
        clip.clipId = 200; clip.name = "idle"; clip.duration = 1.0f;
        SKELETAL_TRACK track;
        track.boneId = 10; track.channelMask = SKELETAL_CHANNEL_TRANSLATION;
        track.keys.push_back({});
        clip.tracks.push_back(track);
        CANONICAL_ANIMATIONS animations;
        animations.skeletonId = 100; animations.clips = {clip};
        expect(validateCanonicalAnimations(skeleton, animations),
               "valid canonical animation collection must validate");
        animations.clips.push_back(clip);
        expect(!validateCanonicalAnimations(skeleton, animations),
               "canonical animation collection must reject duplicate clip IDs and names");
    }

    void testCpuLbsReference()
    {
        CANONICAL_BONE root;
        root.boneId = 10; root.name = "root";
        CANONICAL_SKELETON skeleton;
        skeleton.skeletonId = 100; skeleton.sourceBones = {root};
        expect(compileCanonicalSkeleton(skeleton.sourceBones, skeleton.compiled),
               "CPU LBS fixture skeleton must compile");

        CANONICAL_WEIGHTS weights;
        weights.skeletonId = 100;
        weights.paletteBoneIds = {10};
        weights.vertices.resize(2);
        for (CANONICAL_VERTEX_WEIGHT &vertex : weights.vertices)
        {
            vertex.paletteIndex[0] = 0;
            vertex.weight[0] = 1.0f;
        }
        const std::vector<VEC3> positions = {VEC3(1, 2, 3), VEC3(-2, 0.5f, 4)};
        const std::vector<VEC3> normals = {VEC3(0, 1, 0), VEC3(1, 0, 0)};
        SKELETAL_POSE pose;
        pose.localTransforms = {root.localBind};
        pose.globalTransforms = {skeleton.compiled.bones[0].globalBindMatrix};
        std::vector<VEC3> skinnedPositions, skinnedNormals;
        std::vector<VEC3> dqsPositions, dqsNormals;
        expect(skinVerticesLbsReference(skeleton, weights, pose, positions, normals,
                                        skinnedPositions, skinnedNormals),
               "CPU LBS bind pose must evaluate");
        expect(std::fabs(skinnedPositions[0].x - positions[0].x) <= MATRIX_TOLERANCE &&
                   std::fabs(skinnedPositions[0].y - positions[0].y) <= MATRIX_TOLERANCE &&
                   std::fabs(skinnedPositions[0].z - positions[0].z) <= MATRIX_TOLERANCE &&
                   std::fabs(skinnedNormals[0].y - 1.0f) <= MATRIX_TOLERANCE,
               "CPU LBS bind pose must preserve positions and normals");
        expect(skinVerticesDqsRigidReference(skeleton, weights, pose, positions, normals,
                                             dqsPositions, dqsNormals) &&
                   maximumMatrixDifference(buildTrsMatrix(root.localBind),
                                           skeleton.compiled.bones[0].globalBindMatrix) <= MATRIX_TOLERANCE &&
                   std::fabs(dqsPositions[0].x - skinnedPositions[0].x) <= MATRIX_TOLERANCE &&
                   std::fabs(dqsPositions[0].y - skinnedPositions[0].y) <= MATRIX_TOLERANCE &&
                   std::fabs(dqsPositions[0].z - skinnedPositions[0].z) <= MATRIX_TOLERANCE,
               "CPU rigid DQS bind pose must match LBS");

        LOCAL_TRANSFORM moved = root.localBind;
        moved.translation = VEC3(2, 3, 4);
        pose.globalTransforms[0] = buildTrsMatrix(moved);
        expect(skinVerticesLbsReference(skeleton, weights, pose, positions, normals,
                                        skinnedPositions, skinnedNormals) &&
                   std::fabs(skinnedPositions[0].x - 3.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(skinnedPositions[0].y - 5.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(skinnedPositions[0].z - 7.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(skinnedNormals[0].x) <= MATRIX_TOLERANCE &&
                   std::fabs(skinnedNormals[0].y - 1.0f) <= MATRIX_TOLERANCE,
               "CPU LBS weight-one motion must match rigid translation without translating normals");
        expect(skinVerticesDqsRigidReference(skeleton, weights, pose, positions, normals,
                                             dqsPositions, dqsNormals) &&
                   std::fabs(dqsPositions[0].x - skinnedPositions[0].x) <= MATRIX_TOLERANCE &&
                   std::fabs(dqsPositions[0].y - skinnedPositions[0].y) <= MATRIX_TOLERANCE &&
                   std::fabs(dqsPositions[0].z - skinnedPositions[0].z) <= MATRIX_TOLERANCE &&
                   std::fabs(dqsNormals[0].y - skinnedNormals[0].y) <= MATRIX_TOLERANCE,
               "CPU rigid DQS weight-one motion must match LBS");

        moved.translation = VEC3(2, 3, 0);
        moved.rotation = {0, 0, 0.70710678118f, 0.70710678118f};
        pose.globalTransforms[0] = buildTrsMatrix(moved);
        expect(skinVerticesLbsReference(skeleton, weights, pose, positions, normals,
                                        skinnedPositions, skinnedNormals) &&
                   skinVerticesDqsRigidReference(skeleton, weights, pose, positions, normals,
                                                 dqsPositions, dqsNormals) &&
                   std::fabs(dqsPositions[0].x - skinnedPositions[0].x) <= MATRIX_TOLERANCE &&
                   std::fabs(dqsPositions[0].y - skinnedPositions[0].y) <= MATRIX_TOLERANCE &&
                   std::fabs(dqsPositions[0].z - skinnedPositions[0].z) <= MATRIX_TOLERANCE &&
                   std::fabs(dqsNormals[0].x - skinnedNormals[0].x) <= MATRIX_TOLERANCE &&
                   std::fabs(dqsNormals[0].y - skinnedNormals[0].y) <= MATRIX_TOLERANCE,
               "CPU rigid DQS rotation and translation must match weight-one LBS");

        moved.translation = VEC3(0, 0, 0);
        moved.rotation = {};
        moved.scale = VEC3(2, 1, 1);
        pose.globalTransforms[0] = buildTrsMatrix(moved);
        const float diagonal = 0.70710678118f;
        const std::vector<VEC3> diagonalNormal = {VEC3(diagonal, diagonal, 0), VEC3(1, 0, 0)};
        expect(skinVerticesLbsReference(skeleton, weights, pose, positions, diagonalNormal,
                                        skinnedPositions, skinnedNormals) &&
                   std::fabs(skinnedNormals[0].x - 0.4472135955f) <= MATRIX_TOLERANCE &&
                   std::fabs(skinnedNormals[0].y - 0.894427191f) <= MATRIX_TOLERANCE,
               "CPU LBS normals must use inverse-transpose under non-uniform scale");
        expect(!skinVerticesDqsRigidReference(skeleton, weights, pose, positions, diagonalNormal,
                                              dqsPositions, dqsNormals),
               "CPU rigid DQS must reject scale instead of silently discarding it");

        CANONICAL_BONE rootB;
        rootB.boneId = 20; rootB.name = "rootB";
        CANONICAL_SKELETON antipodalSkeleton;
        antipodalSkeleton.skeletonId = 200;
        antipodalSkeleton.sourceBones = {root, rootB};
        expect(compileCanonicalSkeleton(antipodalSkeleton.sourceBones, antipodalSkeleton.compiled),
               "DQS antipodal fixture skeleton must compile");
        CANONICAL_WEIGHTS antipodalWeights;
        antipodalWeights.skeletonId = 200;
        antipodalWeights.paletteBoneIds = {10, 20};
        antipodalWeights.vertices.resize(1);
        antipodalWeights.vertices[0].paletteIndex[0] = 0;
        antipodalWeights.vertices[0].paletteIndex[1] = 1;
        antipodalWeights.vertices[0].weight[0] = 0.5f;
        antipodalWeights.vertices[0].weight[1] = 0.5f;
        const float angle170 = 1.4835298642f;
        LOCAL_TRANSFORM plus170, minus170;
        plus170.rotation = {0, 0, std::sin(angle170), std::cos(angle170)};
        minus170.rotation = {0, 0, -std::sin(angle170), std::cos(angle170)};
        SKELETAL_POSE antipodalPose;
        antipodalPose.globalTransforms = {buildTrsMatrix(plus170), buildTrsMatrix(minus170)};
        const std::vector<VEC3> antipodalPosition = {VEC3(1, 0, 0)};
        expect(skinVerticesDqsRigidReference(antipodalSkeleton, antipodalWeights, antipodalPose,
                                             antipodalPosition, {}, dqsPositions, dqsNormals) &&
                   std::fabs(dqsPositions[0].x + 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(dqsPositions[0].y) <= MATRIX_TOLERANCE,
               "CPU rigid DQS must antipodally align equivalent hemisphere rotations before blending");
    }

    void testGles2SkinningCapability()
    {
        const GLES2_SKINNING_CAPABILITY minimum = calculateGles2SkinningCapability(128, 8);
        expect(minimum.measured && minimum.hasRequiredVertexAttributes &&
                   minimum.reservedVertexUniformVectors == 8 &&
                   minimum.lbsMatrixPaletteBones == 40 && minimum.dqsRigidPaletteBones == 60,
               "GLES2 minimum capability must reserve scene matrices before calculating palettes");
        const GLES2_SKINNING_CAPABILITY insufficientAttributes =
            calculateGles2SkinningCapability(256, 4);
        expect(insufficientAttributes.measured && !insufficientAttributes.hasRequiredVertexAttributes &&
                   insufficientAttributes.lbsMatrixPaletteBones == 0 &&
                   insufficientAttributes.dqsRigidPaletteBones == 0,
               "GLES2 skeletal capability must reject an insufficient vertex-attribute budget");
        const GLES2_SKINNING_CAPABILITY unavailable = calculateGles2SkinningCapability(0, 0);
        expect(!unavailable.measured && unavailable.lbsMatrixPaletteBones == 0,
               "GLES2 zero query results must remain unmeasured rather than claiming support");
    }

    void testGles2LbsInputPreparation()
    {
        CANONICAL_BONE root, child;
        root.boneId = 10; root.name = "root";
        child.boneId = 20; child.parentBoneId = 10; child.name = "child";
        CANONICAL_SKELETON skeleton;
        skeleton.skeletonId = 100; skeleton.sourceBones = {root, child};
        expect(compileCanonicalSkeleton(skeleton.sourceBones, skeleton.compiled),
               "GPU LBS input fixture skeleton must compile");
        CANONICAL_WEIGHTS weights;
        weights.skeletonId = 100;
        weights.paletteBoneIds = {20, 10};
        weights.vertices.resize(1);
        weights.vertices[0].paletteIndex[0] = 0;
        weights.vertices[0].paletteIndex[1] = 1;
        weights.vertices[0].weight[0] = 0.75f;
        weights.vertices[0].weight[1] = 0.25f;

        GLES2_LBS_INPUT input;
        const GLES2_SKINNING_CAPABILITY sufficient = calculateGles2SkinningCapability(128, 8);
        expect(prepareGles2LbsInput(skeleton, weights, sufficient, input) ==
                   GLES2_LBS_PREPARATION_STATUS::READY && input.ready() &&
                   input.requiredBoneCount == 2 && input.effectiveBoneCapacity == 40 &&
                   input.vertices.size() == 1 && input.vertices[0].boneIndex[0] == 1.0f &&
                   input.vertices[0].boneIndex[1] == 0.0f &&
                   std::fabs(input.vertices[0].weight[0] - 0.75f) <= MATRIX_TOLERANCE,
               "GPU LBS input must resolve stable palette IDs to compiled float attributes");

        GLES2_SKINNING_CAPABILITY tooSmall = sufficient;
        tooSmall.lbsMatrixPaletteBones = 1;
        expect(prepareGles2LbsInput(skeleton, weights, tooSmall, input) ==
                   GLES2_LBS_PREPARATION_STATUS::PALETTE_TOO_LARGE && input.vertices.empty() &&
                   input.requiredBoneCount == 2 && input.effectiveBoneCapacity == 1,
               "GPU LBS input must reject a skeleton larger than the measured uniform palette");
        expect(prepareGles2LbsInput(skeleton, weights, {}, input) ==
                   GLES2_LBS_PREPARATION_STATUS::CAPABILITY_UNAVAILABLE && input.vertices.empty(),
               "GPU LBS input must not claim readiness before backend capability measurement");

        SKELETAL_POSE pose;
        pose.localTransforms = {root.localBind, child.localBind};
        pose.globalTransforms = {skeleton.compiled.bones[0].globalBindMatrix,
                                 skeleton.compiled.bones[1].globalBindMatrix};
        std::vector<float> palette;
        expect(buildGles2LbsPalette(skeleton, pose, true, palette) ==
                   GLES2_LBS_PALETTE_STATUS::READY && palette.size() == 24 &&
                   std::fabs(palette[0] - 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(palette[5] - 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(palette[10] - 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(palette[15]) <= MATRIX_TOLERANCE,
               "GPU LBS bind palette must pack identity in three row-vector output columns");

        LOCAL_TRANSFORM movedChild = child.localBind;
        movedChild.translation = VEC3(3, 0, 0);
        pose.globalTransforms[1] = buildTrsMatrix(movedChild);
        expect(buildGles2LbsPalette(skeleton, pose, true, palette) ==
                   GLES2_LBS_PALETTE_STATUS::READY &&
                   std::fabs(palette[15] - 3.0f) <= MATRIX_TOLERANCE,
               "GPU LBS palette must pack row-vector translation for the shader dot decoder");

        movedChild.scale = VEC3(2, 1, 1);
        pose.globalTransforms[1] = buildTrsMatrix(movedChild);
        expect(buildGles2LbsPalette(skeleton, pose, true, palette) ==
                   GLES2_LBS_PALETTE_STATUS::UNSUPPORTED_NORMAL_TRANSFORM && palette.empty(),
               "compact GPU LBS normal palette must reject non-uniform scale");

        SKELETAL_CLIP clip;
        clip.clipId = 300; clip.name = "move"; clip.duration = 1.0f;
        SKELETAL_TRACK track;
        track.boneId = 20; track.channelMask = SKELETAL_CHANNEL_TRANSLATION;
        SKELETAL_KEY first, last;
        first.time = 0.0f; first.local = child.localBind;
        last.time = 1.0f; last.local = child.localBind; last.local.translation = VEC3(4, 0, 0);
        track.keys = {first, last}; clip.tracks = {track};
        SKELETAL_POSE sampled;
        expect(sampleGles2LbsPalette(skeleton, clip, 0.5f, true, palette, &sampled) ==
                   GLES2_LBS_PALETTE_STATUS::READY && palette.size() == 24 &&
                   std::fabs(palette[15] - 2.0f) <= MATRIX_TOLERANCE &&
                   sampled.globalTransforms.size() == 2,
               "GPU LBS palette sampling must evaluate local clips before packing skin matrices");
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
    testClipCorruptionDiagnostics();
    testScaleOneAndHundredEquivalence();
    testCanonicalSkeletonCompilation();
    testCanonicalSkeletonReader();
    testCanonicalWeightValidation();
    testCanonicalWeightReader();
    testCanonicalAnimationReader();
    testCanonicalAnimationValidation();
    testCanonicalWriterRoundTrip();
    testCpuLbsReference();
    testGles2SkinningCapability();
    testGles2LbsInputPreparation();
    if (failures == 0)
        std::fprintf(stdout, "[skeletal-foundation] PASS\n");
    return failures == 0 ? 0 : 1;
}
