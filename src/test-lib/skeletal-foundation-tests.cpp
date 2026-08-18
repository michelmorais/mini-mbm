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
#include <skeletal-parity-asset.h>
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

    QUATERNION quaternionZ(const float radians) noexcept
    {
        return {0.0f, 0.0f, std::sin(radians * 0.5f), std::cos(radians * 0.5f)};
    }

    QUATERNION quaternionY(const float radians) noexcept
    {
        return {0.0f, std::sin(radians * 0.5f), 0.0f, std::cos(radians * 0.5f)};
    }

    QUATERNION quaternionX(const float radians) noexcept
    {
        return {std::sin(radians * 0.5f), 0.0f, 0.0f, std::cos(radians * 0.5f)};
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

    void testSkeletalCompletionNotification()
    {
        SKELETAL_CLIP clip;
        clip.duration = 1.0f;
        clip.loop = false;
        expect(shouldNotifySkeletalClipCompletion(clip, 0.1f, 1.0f, false),
               "non-looping skeletal clip must notify once at its end");
        expect(!shouldNotifySkeletalClipCompletion(clip, 0.1f, 1.0f, true),
               "completed skeletal clip must not notify repeatedly");
        expect(!shouldNotifySkeletalClipCompletion(clip, 0.0f, 1.0f, false),
               "seek-only evaluation must not notify skeletal completion");
        clip.loop = true;
        expect(!shouldNotifySkeletalClipCompletion(clip, 0.1f, 1.0f, false),
               "looping skeletal clip must not notify completion");
        clip.loop = false;
        clip.duration = 0.0f;
        expect(shouldNotifySkeletalClipCompletion(clip, 0.1f, 0.0f, false),
               "zero-duration non-looping skeletal clip must notify on its first advancing update");
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

    void testAbsolutePoseComposition()
    {
        CANONICAL_BONE root;
        root.boneId = 10;
        root.name = "root";
        CANONICAL_BONE child;
        child.boneId = 20;
        child.parentBoneId = 10;
        child.name = "child";
        child.localBind.translation = VEC3(0.0f, 2.0f, 0.0f);
        COMPILED_SKELETON skeleton;
        expect(compileCanonicalSkeleton({root, child}, skeleton),
               "absolute-composition fixture skeleton must compile");

        SKELETAL_POSE base;
        base.localTransforms = {root.localBind, child.localBind};
        SKELETAL_POSE layer = base;
        base.localTransforms[0].translation = VEC3(0.0f, 0.0f, 0.0f);
        layer.localTransforms[0].translation = VEC3(10.0f, 4.0f, -2.0f);
        base.localTransforms[0].scale = VEC3(1.0f, 1.0f, 1.0f);
        layer.localTransforms[0].scale = VEC3(3.0f, 2.0f, 1.0f);
        base.localTransforms[1].rotation = {0.0f, 0.0f, 0.996194698f, 0.087155743f};
        layer.localTransforms[1].rotation = {0.0f, 0.0f, -0.996194698f, 0.087155743f};

        SKELETAL_POSE composed;
        expect(composeSkeletalPosesAbsolute(skeleton, base, layer, 0.5f, composed) &&
                   composed.localTransforms.size() == 2 &&
                   composed.globalTransforms.size() == 2,
               "absolute composition must produce one complete local/global pose");
        expect(std::fabs(composed.localTransforms[0].translation.x - 5.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(composed.localTransforms[0].translation.y - 2.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(composed.localTransforms[0].translation.z + 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(composed.localTransforms[0].scale.x - 2.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(composed.localTransforms[0].scale.y - 1.5f) <= MATRIX_TOLERANCE,
               "absolute composition must linearly blend local translation and scale");
        expect(std::fabs(std::fabs(composed.localTransforms[1].rotation.z) - 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(composed.localTransforms[1].rotation.w) <= MATRIX_TOLERANCE,
               "absolute composition must blend +170/-170 degrees through the shortest path");
        expect(std::fabs(composed.globalTransforms[0]._41 - 5.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(composed.globalTransforms[0]._42 - 2.0f) <= MATRIX_TOLERANCE,
               "absolute composition must rebuild globals from composed locals");

        SKELETAL_POSE endpoint;
        expect(composeSkeletalPosesAbsolute(skeleton, base, layer, 0.0f, endpoint) &&
                   maximumMatrixDifference(endpoint.globalTransforms[0],
                                           buildTrsMatrix(base.localTransforms[0])) <= MATRIX_TOLERANCE,
               "zero layer weight must reproduce the base pose");
        expect(composeSkeletalPosesAbsolute(skeleton, base, layer, 1.0f, endpoint) &&
                   maximumMatrixDifference(endpoint.globalTransforms[0],
                                           buildTrsMatrix(layer.localTransforms[0])) <= MATRIX_TOLERANCE,
               "unit layer weight must reproduce the layer pose");
        expect(!composeSkeletalPosesAbsolute(skeleton, base, layer, -0.01f, endpoint) &&
                   endpoint.localTransforms.empty(),
               "absolute composition must reject weights outside [0,1]");
        const std::vector<float> rootOnlyMask = {1.0f, 0.0f};
        expect(composeSkeletalPosesAbsoluteMasked(skeleton, base, layer, 0.5f, rootOnlyMask,
                   endpoint) &&
                   std::fabs(endpoint.localTransforms[0].translation.x - 5.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(endpoint.localTransforms[1].rotation.z -
                       base.localTransforms[1].rotation.z) <= MATRIX_TOLERANCE &&
                   std::fabs(endpoint.localTransforms[1].rotation.w -
                       base.localTransforms[1].rotation.w) <= MATRIX_TOLERANCE,
               "absolute bone masks must multiply layer weight per stable skeleton slot");
        expect(!composeSkeletalPosesAbsoluteMasked(skeleton, base, layer, 0.5f, {1.0f}, endpoint) &&
                   endpoint.localTransforms.empty(),
               "absolute bone masks must reject a size that differs from the skeleton");
        expect(!composeSkeletalPosesAbsoluteMasked(skeleton, base, layer, 0.5f,
                   {1.0f, 1.01f}, endpoint) && endpoint.localTransforms.empty(),
               "absolute bone masks must reject values outside [0,1]");

        CANONICAL_SKELETON canonical;
        canonical.skeletonId = 1;
        canonical.sourceBones = {root, child};
        canonical.compiled = skeleton;
        base.localTransforms[0].scale = VEC3(1.0f, 1.0f, 1.0f);
        layer.localTransforms[0].scale = VEC3(1.0f, 1.0f, 1.0f);
        expect(composeSkeletalPosesAbsolute(skeleton, base, layer, 0.5f, endpoint),
               "rigid absolute composition must produce a palette-ready pose");
        std::vector<float> palette;
        expect(buildLbsPalette(canonical, endpoint, true, palette) ==
                   LBS_PALETTE_STATUS::READY && palette.size() == 24,
               "absolute composition must feed the existing LBS palette builder directly");
        expect(buildDqsPalette(canonical, endpoint, palette) ==
                   DQS_PALETTE_STATUS::READY && palette.size() == 16,
               "rigid absolute composition must feed the existing DQS palette builder directly");

        SKELETAL_CLIP baseClip;
        baseClip.clipId = 100;
        baseClip.name = "base";
        baseClip.duration = 2.0f;
        SKELETAL_TRACK baseTrack;
        baseTrack.boneId = 10;
        baseTrack.channelMask = SKELETAL_CHANNEL_TRANSLATION;
        SKELETAL_KEY baseStart;
        baseStart.local = root.localBind;
        SKELETAL_KEY baseEnd = baseStart;
        baseEnd.time = 2.0f;
        baseEnd.local.translation = VEC3(20.0f, 0.0f, 0.0f);
        baseTrack.keys = {baseStart, baseEnd};
        baseClip.tracks = {baseTrack};
        SKELETAL_CLIP layerClip = baseClip;
        layerClip.clipId = 200;
        layerClip.name = "layer";
        layerClip.duration = 1.0f;
        layerClip.loop = true;
        layerClip.tracks[0].keys[1].time = 1.0f;
        layerClip.tracks[0].keys[1].local.translation = VEC3(4.0f, 0.0f, 0.0f);
        float baseTime = 0.25f;
        float layerTime = 0.75f;
        expect(advanceSkeletalClipTime(baseClip, 1.0f, baseTime) &&
                   advanceSkeletalClipTime(layerClip, 0.5f, layerTime) &&
                   std::fabs(baseTime - 1.25f) <= MATRIX_TOLERANCE &&
                   std::fabs(layerTime - 0.25f) <= MATRIX_TOLERANCE,
               "base and Absolute layer times must advance and loop independently");
        expect(sampleSkeletalClipsAbsolute(skeleton, baseClip, baseTime, layerClip,
                   layerTime, 0.25f, endpoint) &&
                   std::fabs(endpoint.localTransforms[0].translation.x - 9.625f) <=
                       MATRIX_TOLERANCE,
               "two independently timed clips must sample before Absolute local-pose composition");
        expect(!advanceSkeletalClipTime(baseClip, -0.1f, baseTime),
               "clip time advancement must reject negative delta");

        float fadeElapsed = 0.0f;
        float fadeWeight = 0.25f;
        bool fadeComplete = false;
        expect(advanceSkeletalAbsoluteFade(0.25f, 1.0f, 0.5f, 0.2f,
                   fadeElapsed, fadeWeight, fadeComplete) && !fadeComplete &&
                   std::fabs(fadeElapsed - 0.2f) <= MATRIX_TOLERANCE &&
                   std::fabs(fadeWeight - 0.55f) <= MATRIX_TOLERANCE,
               "Absolute fade must advance linearly from its captured start weight");
        expect(advanceSkeletalAbsoluteFade(0.25f, 1.0f, 0.5f, 0.4f,
                   fadeElapsed, fadeWeight, fadeComplete) && fadeComplete &&
                   std::fabs(fadeElapsed - 0.5f) <= MATRIX_TOLERANCE &&
                   std::fabs(fadeWeight - 1.0f) <= MATRIX_TOLERANCE,
               "Absolute fade must clamp exactly to its target and duration");
        expect(!advanceSkeletalAbsoluteFade(0.0f, 1.0f, 0.0f, 0.1f,
                   fadeElapsed, fadeWeight, fadeComplete),
               "timed Absolute fade must reject zero duration");

        SKELETAL_POSE additiveBase;
        additiveBase.localTransforms = {skeleton.bones[0].localBind,
                                        skeleton.bones[1].localBind};
        SKELETAL_POSE additiveLayer = additiveBase;
        additiveLayer.localTransforms[0].translation = VEC3(4.0f, 2.0f, 0.0f);
        additiveLayer.localTransforms[0].rotation =
            {0.0f, 0.0f, 0.707106781f, 0.707106781f};
        additiveLayer.localTransforms[0].scale = VEC3(2.0f, 3.0f, 1.0f);
        SKELETAL_POSE additive;
        expect(composeSkeletalPosesAdditive(skeleton, additiveBase, additiveLayer, 0.5f,
                   additive) && std::fabs(additive.localTransforms[0].translation.x - 2.0f) <=
                       MATRIX_TOLERANCE &&
                   std::fabs(additive.localTransforms[0].translation.y - 1.0f) <=
                       MATRIX_TOLERANCE &&
                   std::fabs(additive.localTransforms[0].scale.x - 1.5f) <= MATRIX_TOLERANCE &&
                   std::fabs(additive.localTransforms[0].scale.y - 2.0f) <= MATRIX_TOLERANCE,
               "Additive composition must apply weighted bind-relative translation and scale");
        expect(std::fabs(std::fabs(additive.localTransforms[0].rotation.z) - 0.382683432f) <=
                   MATRIX_TOLERANCE &&
                   std::fabs(std::fabs(additive.localTransforms[0].rotation.w) - 0.923879533f) <=
                       MATRIX_TOLERANCE,
               "Additive composition must apply a shortest-path weighted rotation delta");
        expect(composeSkeletalPosesAdditive(skeleton, additiveBase, additiveLayer, 0.0f,
                   additive) && maximumMatrixDifference(additive.globalTransforms[0],
                       buildTrsMatrix(additiveBase.localTransforms[0])) <= MATRIX_TOLERANCE,
               "zero Additive weight must reproduce the base pose");
        expect(composeSkeletalPosesAdditive(skeleton, additiveBase, additiveLayer, 1.0f,
                   additive) && maximumMatrixDifference(additive.globalTransforms[0],
                       buildTrsMatrix(additiveLayer.localTransforms[0])) <= MATRIX_TOLERANCE,
               "unit Additive weight over bind must reproduce the layer pose exactly");
        additiveBase.localTransforms[0].translation = VEC3(10.0f, 0.0f, 0.0f);
        expect(composeSkeletalPosesAdditive(skeleton, additiveBase, additiveLayer, 0.5f,
                   additive) && std::fabs(additive.localTransforms[0].translation.x - 12.0f) <=
                       MATRIX_TOLERANCE,
               "Additive translation must offset an arbitrary base pose rather than replace it");
        const std::vector<float> childOnlyMask = {0.0f, 1.0f};
        expect(composeSkeletalPosesAdditiveMasked(skeleton, additiveBase, additiveLayer, 1.0f,
                   childOnlyMask, additive) &&
                   std::fabs(additive.localTransforms[0].translation.x - 10.0f) <= MATRIX_TOLERANCE,
               "Additive bone masks must leave a zero-weight bone at its base local transform");
        COMPILED_SKELETON singularReference = skeleton;
        singularReference.bones[0].localBind.scale.x = 0.0f;
        expect(!composeSkeletalPosesAdditive(singularReference, additiveBase, additiveLayer,
                   0.5f, additive) && additive.localTransforms.empty(),
               "Additive scale ratios must reject a singular bind-scale component");

        layer.localTransforms.pop_back();
        expect(!composeSkeletalPosesAbsolute(skeleton, base, layer, 0.5f, endpoint),
               "absolute composition must reject incomplete poses");
    }

    void testRootMotionPoseNeutralization()
    {
        CANONICAL_BONE root;
        root.boneId = 10;
        root.name = "root";
        root.localBind.translation = VEC3(1.0f, 2.0f, 3.0f);
        CANONICAL_BONE child;
        child.boneId = 20;
        child.parentBoneId = 10;
        child.name = "child";
        child.localBind.translation = VEC3(0.0f, 5.0f, 0.0f);
        COMPILED_SKELETON skeleton;
        expect(compileCanonicalSkeleton({root, child}, skeleton),
               "root-motion neutralization fixture skeleton must compile");

        SKELETAL_POSE pose;
        pose.localTransforms = {root.localBind, child.localBind};
        pose.localTransforms[0].translation = VEC3(9.0f, -4.0f, 6.0f);
        pose.localTransforms[0].rotation = quaternionZ(0.75f);
        pose.localTransforms[1].translation = VEC3(0.0f, 7.0f, 0.0f);
        expect(neutralizeSkeletalPoseLocalTranslation(skeleton, 0, pose),
               "root-motion neutralization must accept a complete pose and valid bone index");
        expect(std::fabs(pose.localTransforms[0].translation.x - 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(pose.localTransforms[0].translation.y - 2.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(pose.localTransforms[0].translation.z - 3.0f) <= MATRIX_TOLERANCE,
               "root-motion neutralization must restore only the selected bone local translation to bind");
        expect(std::fabs(pose.localTransforms[0].rotation.z - std::sin(0.75f * 0.5f)) <=
                   MATRIX_TOLERANCE,
               "translation-only root-motion neutralization must preserve selected local rotation");
        expect(std::fabs(pose.localTransforms[1].translation.y - 7.0f) <= MATRIX_TOLERANCE,
               "root-motion neutralization must preserve non-selected local translations");
        MATRIX expectedChildGlobal;
        const MATRIX expectedChildLocal = buildTrsMatrix(pose.localTransforms[1]);
        MatrixMultiply(&expectedChildGlobal, &expectedChildLocal, &pose.globalTransforms[0]);
        expect(std::fabs(pose.globalTransforms[0]._41 - 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(pose.globalTransforms[0]._42 - 2.0f) <= MATRIX_TOLERANCE &&
                   maximumMatrixDifference(expectedChildGlobal, pose.globalTransforms[1]) <=
                       matrixComparisonTolerance(expectedChildGlobal, pose.globalTransforms[1]),
               "root-motion neutralization must rebuild descendants from the neutralized hierarchy");

        pose.localTransforms.pop_back();
        expect(!neutralizeSkeletalPoseLocalTranslation(skeleton, 0, pose),
               "root-motion neutralization must reject incomplete poses");
    }

    void testRootMotionRotationDeltaAndNeutralization()
    {
        LOCAL_TRANSFORM previousRoot;
        previousRoot.rotation = quaternionX(0.45f);
        LOCAL_TRANSFORM currentRoot;
        currentRoot.rotation = quaternionY(-0.85f);
        QUATERNION delta;
        expect(computeSkeletalRootMotionRotationDelta(buildTrsMatrix(previousRoot),
                   buildTrsMatrix(currentRoot), delta),
               "root-motion rotation delta must accept normalized rigid rotations");
        const MATRIX previousMatrix = buildTrsMatrix(previousRoot);
        const MATRIX currentMatrix = buildTrsMatrix(currentRoot);
        LOCAL_TRANSFORM deltaTransform;
        deltaTransform.rotation = delta;
        MATRIX composed;
        const MATRIX deltaMatrix = buildTrsMatrix(deltaTransform);
        MatrixMultiply(&composed, &deltaMatrix, &previousMatrix);
        expect(maximumMatrixDifference(composed, currentMatrix) <=
                   matrixComparisonTolerance(composed, currentMatrix),
               "root-motion rotation delta must left-compose previous rotation to current rotation");

        LOCAL_TRANSFORM owner;
        owner.rotation = quaternionZ(0.35f);
        const MATRIX ownerMatrix = buildTrsMatrix(owner);
        MATRIX composedOwner;
        MatrixMultiply(&composedOwner, &deltaMatrix, &ownerMatrix);
        LOCAL_TRANSFORM ownerCurrent;
        bool ownerNegativeScale = false;
        bool ownerShear = false;
        expect(decomposeTrsMatrix(composedOwner, ownerCurrent, ownerNegativeScale, ownerShear) &&
                   !ownerShear,
               "root-motion rotation delta must compose into a non-identity owner orientation");
        const MATRIX rebuiltOwner = buildTrsMatrix(ownerCurrent);
        expect(maximumMatrixDifference(rebuiltOwner, composedOwner) <=
                   matrixComparisonTolerance(rebuiltOwner, composedOwner),
               "root-motion rotation delta owner composition must remain a valid rigid TRS");

        CANONICAL_BONE root;
        root.boneId = 10;
        root.name = "root";
        root.localBind.rotation = quaternionZ(0.125f);
        CANONICAL_BONE child;
        child.boneId = 20;
        child.parentBoneId = 10;
        child.name = "child";
        child.localBind.translation = VEC3(2.0f, 0.0f, 0.0f);
        COMPILED_SKELETON skeleton;
        expect(compileCanonicalSkeleton({root, child}, skeleton),
               "rotation root-motion neutralization fixture skeleton must compile");
        SKELETAL_POSE pose;
        pose.localTransforms = {root.localBind, child.localBind};
        pose.localTransforms[0].translation = VEC3(5.0f, 0.0f, 0.0f);
        pose.localTransforms[0].rotation = quaternionZ(1.25f);
        pose.localTransforms[1].translation = VEC3(3.0f, 0.0f, 0.0f);
        expect(neutralizeSkeletalPoseLocalTransform(skeleton, 0, true, true, pose),
               "root-motion transform neutralization must accept translation plus rotation");
        expect(std::fabs(pose.localTransforms[0].translation.x -
                   skeleton.bones[0].localBind.translation.x) <= MATRIX_TOLERANCE &&
                   std::fabs(pose.localTransforms[0].rotation.z -
                       skeleton.bones[0].localBind.rotation.z) <= MATRIX_TOLERANCE,
               "root-motion transform neutralization must restore selected translation and rotation to bind");
        const MATRIX expectedChildLocal = buildTrsMatrix(pose.localTransforms[1]);
        MATRIX expectedChildGlobal;
        MatrixMultiply(&expectedChildGlobal, &expectedChildLocal, &pose.globalTransforms[0]);
        expect(maximumMatrixDifference(expectedChildGlobal, pose.globalTransforms[1]) <=
                   matrixComparisonTolerance(expectedChildGlobal, pose.globalTransforms[1]),
               "root-motion transform neutralization must rebuild descendants after rotation reset");
    }

    void testMultipleRootSkeletonSemantics()
    {
        CANONICAL_BONE rootA;
        rootA.boneId = 10;
        rootA.name = "rootA";
        rootA.localBind.translation = VEC3(10.0f, 0.0f, 0.0f);
        CANONICAL_BONE childA;
        childA.boneId = 20;
        childA.parentBoneId = 10;
        childA.name = "childA";
        childA.localBind.translation = VEC3(0.0f, 2.0f, 0.0f);
        CANONICAL_BONE branchA;
        branchA.boneId = 30;
        branchA.parentBoneId = 10;
        branchA.name = "branchA";
        branchA.localBind.translation = VEC3(0.0f, 0.0f, 3.0f);
        CANONICAL_BONE rootB;
        rootB.boneId = 40;
        rootB.name = "rootB";
        rootB.localBind.translation = VEC3(-20.0f, 5.0f, 1.0f);
        CANONICAL_BONE childB;
        childB.boneId = 50;
        childB.parentBoneId = 40;
        childB.name = "childB";
        childB.localBind.translation = VEC3(0.0f, -4.0f, 0.0f);

        CANONICAL_SKELETON skeleton;
        skeleton.skeletonId = 500;
        skeleton.sourceBones = {rootA, childA, branchA, rootB, childB};
        expect(compileCanonicalSkeleton(skeleton.sourceBones, skeleton.compiled),
               "multiple-root fixture skeleton must compile");
        expect(skeleton.compiled.bones.size() == 5 &&
                   skeleton.compiled.bones[0].parentIndex == -1 &&
                   skeleton.compiled.bones[3].parentIndex == -1 &&
                   skeleton.compiled.bones[1].parentIndex == 0 &&
                   skeleton.compiled.bones[2].parentIndex == 0 &&
                   skeleton.compiled.bones[4].parentIndex == 3,
               "multiple roots must remain parentIndex=-1 while children resolve to their own root");
        expect(std::fabs(skeleton.compiled.bones[0].globalBindMatrix._41 - 10.0f) <=
                   MATRIX_TOLERANCE &&
                   std::fabs(skeleton.compiled.bones[3].globalBindMatrix._41 + 20.0f) <=
                   MATRIX_TOLERANCE &&
                   std::fabs(skeleton.compiled.bones[4].globalBindMatrix._41 + 20.0f) <=
                   MATRIX_TOLERANCE &&
                   std::fabs(skeleton.compiled.bones[4].globalBindMatrix._42 - 1.0f) <=
                   MATRIX_TOLERANCE,
               "multiple-root bind globals must not compose one root through another");

        SKELETAL_CLIP clip;
        clip.clipId = 600;
        clip.name = "both-roots";
        clip.duration = 1.0f;
        SKELETAL_KEY startA;
        startA.local = rootA.localBind;
        SKELETAL_KEY endA = startA;
        endA.time = 1.0f;
        endA.local.translation = VEC3(14.0f, 0.0f, 0.0f);
        SKELETAL_TRACK trackA;
        trackA.boneId = 10;
        trackA.channelMask = SKELETAL_CHANNEL_TRANSLATION;
        trackA.keys = {startA, endA};
        SKELETAL_KEY startB;
        startB.local = rootB.localBind;
        SKELETAL_KEY endB = startB;
        endB.time = 1.0f;
        endB.local.translation = VEC3(-17.0f, 8.0f, 1.0f);
        SKELETAL_TRACK trackB;
        trackB.boneId = 40;
        trackB.channelMask = SKELETAL_CHANNEL_TRANSLATION;
        trackB.keys = {startB, endB};
        clip.tracks = {trackA, trackB};

        SKELETAL_POSE sampled;
        expect(sampleSkeletalClip(skeleton.compiled, clip, 1.0f, sampled),
               "multiple-root clip must sample when both roots animate simultaneously");
        expect(std::fabs(sampled.globalTransforms[0]._41 - 14.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(sampled.globalTransforms[3]._41 + 17.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(sampled.globalTransforms[3]._42 - 8.0f) <= MATRIX_TOLERANCE,
               "sampled global roots must keep independent animated translations");
        expect(std::fabs(sampled.globalTransforms[1]._41 - sampled.globalTransforms[0]._41) <=
                   3.0f &&
                   std::fabs(sampled.globalTransforms[4]._41 - sampled.globalTransforms[3]._41) <=
                   5.0f,
               "sampled children must follow their own animated root hierarchy");

        std::vector<float> palette;
        expect(buildLbsPalette(skeleton, sampled, true, palette) ==
                   LBS_PALETTE_STATUS::READY && palette.size() == 60 &&
                   std::fabs(palette[3] - 4.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(palette[39] - 3.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(palette[43] - 3.0f) <= MATRIX_TOLERANCE,
               "LBS palette must stay in compiled order across independent roots");
        expect(buildDqsPalette(skeleton, sampled, palette) ==
                   DQS_PALETTE_STATUS::READY && palette.size() == 40,
               "DQS palette must accept and order the same independent-root pose");

        SKELETAL_POSE neutralized = sampled;
        const MATRIX rootBBefore = neutralized.globalTransforms[3];
        const MATRIX childBBefore = neutralized.globalTransforms[4];
        expect(neutralizeSkeletalPoseLocalTransform(skeleton.compiled, 0, true, true, neutralized),
               "root-motion neutralization must accept one selected root in a multi-root pose");
        expect(maximumMatrixDifference(rootBBefore, neutralized.globalTransforms[3]) <=
                   matrixComparisonTolerance(rootBBefore, neutralized.globalTransforms[3]) &&
                   maximumMatrixDifference(childBBefore, neutralized.globalTransforms[4]) <=
                   matrixComparisonTolerance(childBBefore, neutralized.globalTransforms[4]),
               "neutralizing one root must not alter the other root hierarchy");
        expect(std::fabs(neutralized.globalTransforms[0]._41 - 10.0f) <= MATRIX_TOLERANCE &&
                   maximumMatrixDifference(sampled.globalTransforms[1],
                                           neutralized.globalTransforms[1]) >
                   MATRIX_TOLERANCE,
               "neutralizing one root must rebuild only that selected root hierarchy");
    }

    void testUniformCanonicalAssetScale()
    {
        CANONICAL_BONE root;
        root.boneId = 10;
        root.name = "root";
        root.localBind.translation = VEC3(1.0f, 2.0f, 3.0f);
        root.radius = 0.25f;
        root.length = 2.0f;
        root.tailOffset = VEC3(0.5f, 2.0f, -0.25f);
        root.hasExplicitTail = true;
        CANONICAL_BONE child;
        child.boneId = 20;
        child.parentBoneId = 10;
        child.name = "child";
        child.localBind.translation = VEC3(0.0f, 4.0f, 0.0f);
        child.radius = 0.1f;
        child.length = 1.5f;
        CANONICAL_SKELETON skeleton;
        skeleton.skeletonId = 100;
        skeleton.sourceBones = {root, child};
        expect(compileCanonicalSkeleton(skeleton.sourceBones, skeleton.compiled),
               "canonical scale fixture skeleton must compile");

        SKELETAL_KEY key;
        key.local = child.localBind;
        key.local.translation = VEC3(2.0f, 6.0f, -1.0f);
        SKELETAL_TRACK track;
        track.boneId = child.boneId;
        track.channelMask = SKELETAL_CHANNEL_TRANSLATION;
        track.keys = {key};
        SKELETAL_CLIP clip;
        clip.clipId = 200;
        clip.name = "scaled";
        clip.tracks = {track};
        CANONICAL_ANIMATIONS animations;
        animations.skeletonId = skeleton.skeletonId;
        animations.clips = {clip};

        CANONICAL_SKELETON scaledSkeleton;
        CANONICAL_ANIMATIONS scaledAnimations;
        expect(buildUniformlyScaledCanonicalAsset(skeleton, animations, 100.0f,
                                                   scaledSkeleton, scaledAnimations),
               "positive uniform canonical asset scale must validate");
        expect(std::fabs(scaledSkeleton.sourceBones[0].localBind.translation.x - 100.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(scaledSkeleton.sourceBones[1].localBind.translation.y - 400.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(scaledSkeleton.sourceBones[0].radius - 25.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(scaledSkeleton.sourceBones[1].length - 150.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(scaledSkeleton.sourceBones[0].tailOffset.x - 50.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(scaledSkeleton.sourceBones[0].tailOffset.y - 200.0f) <= MATRIX_TOLERANCE &&
                   scaledSkeleton.sourceBones[0].hasExplicitTail,
               "uniform asset scale must update bind translations and bone display metadata");
        expect(std::fabs(scaledAnimations.clips[0].tracks[0].keys[0].local.translation.x - 200.0f) <=
                   MATRIX_TOLERANCE &&
                   std::fabs(scaledAnimations.clips[0].tracks[0].keys[0].local.translation.y - 600.0f) <=
                   MATRIX_TOLERANCE,
               "uniform asset scale must update clip translations");
        expect(scaledSkeleton.compiled.maximumBindIdentityError <= MATRIX_TOLERANCE,
               "uniform asset scale must rebuild a valid inverse bind");
        expect(!buildUniformlyScaledCanonicalAsset(skeleton, animations, 0.0f,
                                                    scaledSkeleton, scaledAnimations) &&
                   !buildUniformlyScaledCanonicalAsset(skeleton, animations, -1.0f,
                                                       scaledSkeleton, scaledAnimations),
               "zero and negative canonical asset scales must be rejected");
    }

    bool writeCanonicalSkeletonFixture(const char *path, const bool zeroQuaternion,
                                       const uint32_t sectionCount, const bool twoBones = false)
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
            ok = util::writeSectionV11Streamed(fp, header, [zeroQuaternion, twoBones](FILE *payload)
            {
                return util::le_io::writeU64LE(payload, 100) &&
                    util::le_io::writeU32LE(payload, twoBones ? 2 : 1) &&
                    util::le_io::writeU64LE(payload, 10) &&
                    util::le_io::writeU64LE(payload, 0) &&
                    util::writeStringV11(payload, "root") &&
                    util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                    util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                    util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                    util::le_io::writeF32LE(payload, zeroQuaternion ? 0.0f : 1.0f) &&
                    util::le_io::writeF32LE(payload, 1) && util::le_io::writeF32LE(payload, 1) &&
                    util::le_io::writeF32LE(payload, 1) && util::le_io::writeF32LE(payload, 0.1f) &&
                    util::le_io::writeF32LE(payload, 1.0f) &&
                    (!twoBones ||
                     (util::le_io::writeU64LE(payload, 20) && util::le_io::writeU64LE(payload, 10) &&
                      util::writeStringV11(payload, "child") &&
                      util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 2) &&
                      util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                      util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                      util::le_io::writeF32LE(payload, 1) && util::le_io::writeF32LE(payload, 1) &&
                      util::le_io::writeF32LE(payload, 1) && util::le_io::writeF32LE(payload, 1) &&
                      util::le_io::writeF32LE(payload, 0.1f) && util::le_io::writeF32LE(payload, 1.0f)));
            });
        }
        std::fclose(fp);
        return ok;
    }

    bool writeSkeletalSharingFixture(const char *path, const uint64_t skeletonId,
                                     const uint64_t childId, const uint64_t childParentId,
                                     const char *childName, const float childY,
                                     const bool includeChild)
    {
        FILE *fp = std::fopen(path, "wb+");
        if (!fp) return false;
        util::FILE_HEADER_V11 fileHeader;
        fileHeader.typeMesh = util::TYPE_MESH_3D;
        fileHeader.sectionCount = 1;
        bool ok = util::writeFileHeaderV11(fp, fileHeader);
        util::SECTION_HEADER_V11 header;
        header.type = util::SECTION_SKELETAL_SKELETON;
        header.sectionVersion = 1;
        ok = ok && util::writeSectionV11Streamed(fp, header, [=](FILE *payload)
        {
            return util::le_io::writeU64LE(payload, skeletonId) &&
                util::le_io::writeU32LE(payload, includeChild ? 2 : 1) &&
                util::le_io::writeU64LE(payload, 10) && util::le_io::writeU64LE(payload, 0) &&
                util::writeStringV11(payload, "root") &&
                util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                util::le_io::writeF32LE(payload, 1) && util::le_io::writeF32LE(payload, 1) &&
                util::le_io::writeF32LE(payload, 1) && util::le_io::writeF32LE(payload, 1) &&
                util::le_io::writeF32LE(payload, 0.1f) && util::le_io::writeF32LE(payload, 1.0f) &&
                (!includeChild ||
                 (util::le_io::writeU64LE(payload, childId) &&
                  util::le_io::writeU64LE(payload, childParentId) &&
                  util::writeStringV11(payload, childName) &&
                  util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, childY) &&
                  util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                  util::le_io::writeF32LE(payload, 0) && util::le_io::writeF32LE(payload, 0) &&
                  util::le_io::writeF32LE(payload, 1) && util::le_io::writeF32LE(payload, 1) &&
                  util::le_io::writeF32LE(payload, 1) && util::le_io::writeF32LE(payload, 1) &&
                  util::le_io::writeF32LE(payload, 0.1f) &&
                  util::le_io::writeF32LE(payload, 1.0f)));
        });
        std::fclose(fp);
        return ok;
    }

    void expectSharingReason(const MESH_MBM_DEBUG &left, const MESH_MBM_DEBUG &right,
                             const char *reason, const bool compatible)
    {
        SKELETAL_SHARING_COMPATIBILITY report;
        const bool result = left.getSkeletalSharingCompatibility(right, report);
        expect(result == compatible && report.compatible == compatible &&
                   std::string(report.reason) == reason,
               reason);
    }

    void testSkeletalSharingCompatibility()
    {
        const char *basePath = "/tmp/mini-mbm-skeletal-sharing-base.msh";
        const char *samePath = "/tmp/mini-mbm-skeletal-sharing-same.msh";
        const char *countPath = "/tmp/mini-mbm-skeletal-sharing-count.msh";
        const char *identityPath = "/tmp/mini-mbm-skeletal-sharing-identity.msh";
        const char *hierarchyPath = "/tmp/mini-mbm-skeletal-sharing-hierarchy.msh";
        const char *bindPath = "/tmp/mini-mbm-skeletal-sharing-bind.msh";
        expect(writeSkeletalSharingFixture(basePath, 100, 20, 10, "child", 2.0f, true) &&
                   writeSkeletalSharingFixture(samePath, 200, 20, 10, "child", 2.0f, true) &&
                   writeSkeletalSharingFixture(countPath, 300, 20, 10, "child", 2.0f, false) &&
                   writeSkeletalSharingFixture(identityPath, 400, 21, 10, "child", 2.0f, true) &&
                   writeSkeletalSharingFixture(hierarchyPath, 500, 20, 0, "child", 2.0f, true) &&
                   writeSkeletalSharingFixture(bindPath, 600, 20, 10, "child", 3.0f, true),
               "skeletal sharing fixtures must write");
        MESH_MBM_DEBUG base, same, count, identity, hierarchy, bind, missing;
        expect(base.loadV11(basePath) && same.loadV11(samePath) && count.loadV11(countPath) &&
                   identity.loadV11(identityPath) && hierarchy.loadV11(hierarchyPath) &&
                   bind.loadV11(bindPath),
               "skeletal sharing fixtures must load");
        expectSharingReason(base, same, "compatible", true);
        expectSharingReason(base, missing, "missing_skeleton", false);
        expectSharingReason(base, count, "bone_count_mismatch", false);
        expectSharingReason(base, identity, "bone_identity_mismatch", false);
        expectSharingReason(base, hierarchy, "hierarchy_mismatch", false);
        SKELETAL_SHARING_COMPATIBILITY report;
        expect(!base.getSkeletalSharingCompatibility(bind, report) &&
                   std::string(report.reason) == "bind_transform_mismatch" &&
                   report.boneIndex == 1 && std::string(report.boneName) == "child" &&
                   report.observedError > report.tolerance,
               "bind_transform_mismatch");
    }

    void testCanonicalSkeletonReader()
    {
        const char *validPath = "/tmp/mini-mbm-canonical-skeleton-valid.msh";
        const char *invalidPath = "/tmp/mini-mbm-canonical-skeleton-invalid.msh";
        const char *duplicatePath = "/tmp/mini-mbm-canonical-skeleton-duplicate.msh";
        expect(writeCanonicalSkeletonFixture(validPath, false, 1), "canonical fixture must write");
        MESH_MBM_DEBUG mesh;
        expect(mesh.loadV11(validPath), "both-loader canonical skeleton reader must accept valid payload");
        SKELETON_BIND_BONE_INFO legacyBone;
        expect(mesh.getSkeletonBindBone(0, legacyBone) && !legacyBone.hasExplicitTail &&
                   std::fabs(legacyBone.tailOffset.x) <= MATRIX_TOLERANCE &&
                   std::fabs(legacyBone.tailOffset.y - 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(legacyBone.tailOffset.z) <= MATRIX_TOLERANCE,
               "version-1 canonical skeleton must expose a marked local-+Y tail approximation");
        expect(writeCanonicalSkeletonFixture(invalidPath, true, 1), "invalid canonical fixture must write");
        expect(!mesh.loadV11(invalidPath), "canonical reader must reject zero bind quaternion");
        expect(writeCanonicalSkeletonFixture(duplicatePath, false, 2), "duplicate canonical fixture must write");
        expect(!mesh.loadV11(duplicatePath), "canonical reader must reject duplicate skeleton sections");
        const char *reparentPath = "/tmp/mini-mbm-canonical-skeleton-reparent.msh";
        expect(writeCanonicalSkeletonFixture(reparentPath, false, 1, true) && mesh.loadV11(reparentPath),
               "canonical reparent fixture must load");
        SKELETON_BIND_BONE_INFO before, after;
        char reparentError[255] = "";
        expect(!mesh.reparentSkeletalBone(0, 1, true, reparentError, sizeof(reparentError)),
               "canonical reparent must reject a hierarchy cycle without mutation");
        expect(mesh.getSkeletonBindBone(1, before) &&
                   mesh.reparentSkeletalBone(1, -1, true, reparentError, sizeof(reparentError)) &&
                   mesh.getSkeletonBindBone(1, after) && after.parentIndex == -1 &&
                   maximumMatrixDifference(before.globalBindMatrix, after.globalBindMatrix) <= MATRIX_TOLERANCE,
               "canonical reparent-to-root must preserve global bind when requested");
        SKELETON_BIND_BONE_INFO edited, rejected;
        expect(mesh.setSkeletalBoneBind(1, VEC3(2.0f, 3.0f, 4.0f),
                   0.0f, 0.0f, 0.0f, 2.0f, VEC3(1.0f, 1.0f, 1.0f), 0.25f, 2.0f,
                   reparentError, sizeof(reparentError)) && mesh.getSkeletonBindBone(1, edited) &&
                   std::fabs(edited.localTranslation.x - 2.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(edited.localRotationW - 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(edited.radius - 0.25f) <= MATRIX_TOLERANCE &&
                   std::fabs(edited.length - 2.0f) <= MATRIX_TOLERANCE,
               "canonical bind editing must commit normalized local TRS and metadata");
        expect(!mesh.setSkeletalBoneBind(1, VEC3(9.0f, 9.0f, 9.0f),
                   0.0f, 0.0f, 0.0f, 0.0f, VEC3(1.0f, 1.0f, 1.0f), 0.25f, 2.0f,
                   reparentError, sizeof(reparentError)) && mesh.getSkeletonBindBone(1, rejected) &&
                   maximumMatrixDifference(edited.localBindMatrix, rejected.localBindMatrix) <= MATRIX_TOLERANCE,
               "canonical bind editing must reject a zero quaternion without mutation");
        uint32_t addedIndex = 0;
        expect(mesh.addSkeletalBone(1, "added-child", VEC3(0.0f, 2.0f, 0.0f),
                   0.1f, 1.5f, true, false, &addedIndex, reparentError, sizeof(reparentError)) &&
                   addedIndex == 2 && mesh.getSkeletonBindBone(addedIndex, edited) &&
                   edited.parentIndex == 1 && edited.boneId != 0 &&
                   std::fabs(edited.localTranslation.y - 2.0f) <= MATRIX_TOLERANCE,
               "canonical bone addition must append a valid child with a new stable ID");
        SKELETON_BIND_SUMMARY addSummary;
        expect(!mesh.addSkeletalBone(-1, "added-child", VEC3(), 0.1f, 1.0f, true, false,
                   &addedIndex, reparentError, sizeof(reparentError)) &&
                   mesh.getSkeletonBindSummary(addSummary) && addSummary.boneCount == 3,
               "canonical bone addition must reject duplicate names without mutation");
        expect(!mesh.removeSkeletalBone(1, reparentError, sizeof(reparentError)) &&
                   mesh.getSkeletonBindSummary(addSummary) && addSummary.boneCount == 3,
               "strict canonical removal must reject a bone with children without mutation");
        expect(mesh.removeSkeletalBone(2, reparentError, sizeof(reparentError)) &&
                   mesh.getSkeletonBindSummary(addSummary) && addSummary.boneCount == 2,
               "strict canonical removal must delete an unreferenced leaf");
        expect(mesh.loadV11(reparentPath) && mesh.getSkeletonBindBone(1, before) &&
                   mesh.removeSkeletalBoneRemapped(0, 1, false, true,
                       reparentError, sizeof(reparentError)) &&
                   mesh.getSkeletonBindBone(0, after) && after.parentIndex == -1 &&
                   maximumMatrixDifference(before.globalBindMatrix, after.globalBindMatrix) <= MATRIX_TOLERANCE,
               "child-bearing removal must promote children while preserving global bind");
        const char *initializedPath = "/tmp/mini-mbm-initial-skeleton.msh";
        const char *jointOnlyPath = "/tmp/mini-mbm-joint-only-skeleton.msh";
        MESH_MBM_DEBUG jointOnlyMesh;
        expect(jointOnlyMesh.loadV11("src/test-lib/Crate.msh") &&
                   jointOnlyMesh.initializeSkeletalSkeleton("joint",VEC3(0,0,0),0.1f,0.0f,false,
                       reparentError,sizeof(reparentError)) &&
                   jointOnlyMesh.getSkeletonBindBone(0,after) && !after.hasExplicitTail &&
                   jointOnlyMesh.saveV11(jointOnlyPath,false,false,false,reparentError,sizeof(reparentError)),
               "transform-only joint creation must omit an explicit tail and save cleanly");
        MESH_MBM_DEBUG jointOnlyReload;
        expect(jointOnlyReload.loadV11(jointOnlyPath) &&
                   jointOnlyReload.getSkeletonBindBone(0,after) && !after.hasExplicitTail,
               "transform-only joint state must survive canonical save/reload");
        std::remove(jointOnlyPath);
        MESH_MBM_DEBUG extensionMesh;
        uint32_t extensionLastIndex=0;
        expect(extensionMesh.loadV11("src/test-lib/Crate.msh") &&
                   extensionMesh.initializeSkeletalSkeleton("extension_root",VEC3(0,0,0),
                       0.1f,2.0f,true,reparentError,sizeof(reparentError)) &&
                   extensionMesh.setSkeletalBoneTail(0,VEC3(2,0,0),true,true,
                       reparentError,sizeof(reparentError)) &&
                   extensionMesh.extendSkeletalBoneTail(0,10,0.1f,0.5f,&extensionLastIndex,
                       reparentError,sizeof(reparentError)) && extensionLastIndex==10 &&
                   extensionMesh.getSkeletonBindSummary(addSummary) && addSummary.boneCount==11 &&
                   extensionMesh.getSkeletonBindBone(10,after) && after.connectedToParent &&
                   std::fabs(after.globalBindMatrix.m[3][0]-6.5f)<=MATRIX_TOLERANCE &&
                   std::fabs(after.tailOffset.x-0.5f)<=MATRIX_TOLERANCE,
               "tail extension count must atomically create a connected directional chain");
        VEC3 connectionTailBefore,connectionTailAfter;
        expect(extensionMesh.setSkeletalBoneConnectedToParent(1,false,true,
                   reparentError,sizeof(reparentError)) &&
                   extensionMesh.setSkeletalBoneHead(1,VEC3(3,0,0),true,
                       reparentError,sizeof(reparentError)) &&
                   extensionMesh.getSkeletonBindBone(1,before) && !before.connectedToParent &&
                   vec3TransformCoord(&connectionTailBefore,&before.tailOffset,&before.globalBindMatrix) &&
                   extensionMesh.setSkeletalBoneConnectedToParent(1,true,true,
                       reparentError,sizeof(reparentError)) &&
                   extensionMesh.getSkeletonBindBone(1,after) && after.connectedToParent &&
                   std::fabs(after.localTranslation.x-2.0f)<=MATRIX_TOLERANCE &&
                   vec3TransformCoord(&connectionTailAfter,&after.tailOffset,&after.globalBindMatrix) &&
                   std::fabs(connectionTailAfter.x-connectionTailBefore.x)<=MATRIX_TOLERANCE &&
                   std::fabs(connectionTailAfter.y-connectionTailBefore.y)<=MATRIX_TOLERANCE &&
                   std::fabs(connectionTailAfter.z-connectionTailBefore.z)<=MATRIX_TOLERANCE,
               "explicit parent-tail connection must preserve the child global tail");
        expect(extensionMesh.setSkeletalBoneRadius(1,0.25f,true,
                   reparentError,sizeof(reparentError)) &&
                   extensionMesh.getSkeletonBindBone(0,before) &&
                   std::fabs(before.radius-0.1f)<=MATRIX_TOLERANCE &&
                   extensionMesh.getSkeletonBindBone(10,after) &&
                   std::fabs(after.radius-0.25f)<=MATRIX_TOLERANCE,
               "joint radius subtree edit must exclude ancestors and include all descendants");
        expect(!extensionMesh.setSkeletalBoneRadius(1,0.0f,false,
                   reparentError,sizeof(reparentError)) &&
                   extensionMesh.getSkeletonBindBone(1,after) &&
                   std::fabs(after.radius-0.25f)<=MATRIX_TOLERANCE,
               "joint radius edit must reject zero without mutation");
        MESH_MBM_DEBUG staticMesh;
        expect(staticMesh.loadV11("src/test-lib/Crate.msh") &&
                   staticMesh.initializeSkeletalSkeleton("root",VEC3(0,0,0),0.1f,1.0f,true,
                       reparentError,sizeof(reparentError)),
               "static mesh must initialize a one-root canonical skeleton");
        uint32_t chainLastIndex = 0;
        expect(staticMesh.addSkeletalBoneChain(0,"spine_",3,VEC3(0,1,0),0.1f,1.0f,
                   &chainLastIndex,reparentError,sizeof(reparentError)) && chainLastIndex==3 &&
                   staticMesh.getSkeletonBindSummary(addSummary) && addSummary.boneCount==4 &&
                   staticMesh.getSkeletonBindBone(3,after) && after.parentIndex==2 &&
                   after.connectedToParent &&
                   std::fabs(after.globalBindMatrix.m[3][1]-3.0f)<=MATRIX_TOLERANCE &&
                   staticMesh.saveV11(initializedPath,false,false,false,reparentError,sizeof(reparentError)),
               "canonical chain creation must append linked bones and save atomically");
        expect(!staticMesh.addSkeletalBoneChain(0,"spine_",2,VEC3(0,1,0),0.1f,1.0f,
                   &chainLastIndex,reparentError,sizeof(reparentError)) &&
                   staticMesh.getSkeletonBindSummary(addSummary) && addSummary.boneCount==4,
               "canonical chain creation must reject duplicate generated names without mutation");
        SKELETON_BIND_BONE_INFO preservedDescendantBefore,preservedDescendantAfter;
        expect(staticMesh.getSkeletonBindBone(2,preservedDescendantBefore) &&
                   staticMesh.setSkeletalBoneTail(0,VEC3(0.5f,1.5f,0.0f),true,true,
                   reparentError,sizeof(reparentError)) &&
                   staticMesh.getSkeletonBindBone(1,after) && after.connectedToParent &&
                   std::fabs(after.localTranslation.x-0.5f)<=MATRIX_TOLERANCE &&
                   std::fabs(after.localTranslation.y-1.5f)<=MATRIX_TOLERANCE &&
                   staticMesh.getSkeletonBindBone(2,preservedDescendantAfter) &&
                   maximumMatrixDifference(preservedDescendantBefore.globalBindMatrix,
                       preservedDescendantAfter.globalBindMatrix)<=MATRIX_TOLERANCE,
               "tail editing must move the shared joint while preserving other joints");
        VEC3 preservedTailBefore,preservedTailAfter;
        expect(staticMesh.getSkeletonBindBone(1,after) &&
                   vec3TransformCoord(&preservedTailBefore,&after.tailOffset,&after.globalBindMatrix) &&
                   staticMesh.getSkeletonBindBone(3,preservedDescendantBefore) &&
                   staticMesh.setSkeletalBoneHead(1,VEC3(0.75f,1.25f,0.0f),true,
                       reparentError,sizeof(reparentError)) &&
                   staticMesh.getSkeletonBindBone(1,after) && !after.connectedToParent &&
                   vec3TransformCoord(&preservedTailAfter,&after.tailOffset,&after.globalBindMatrix) &&
                   std::fabs(preservedTailAfter.x-preservedTailBefore.x)<=MATRIX_TOLERANCE &&
                   std::fabs(preservedTailAfter.y-preservedTailBefore.y)<=MATRIX_TOLERANCE &&
                   std::fabs(preservedTailAfter.z-preservedTailBefore.z)<=MATRIX_TOLERANCE &&
                   staticMesh.getSkeletonBindBone(3,preservedDescendantAfter) &&
                   maximumMatrixDifference(preservedDescendantBefore.globalBindMatrix,
                       preservedDescendantAfter.globalBindMatrix)<=MATRIX_TOLERANCE,
               "head editing must preserve its tail and all other joints in global bind space");
        VEC3 segmentHeadBefore,segmentHeadAfter,segmentTailBefore,segmentTailAfter;
        expect(staticMesh.getSkeletonBindBone(1,before) &&
                   (segmentHeadBefore=VEC3(before.globalBindMatrix.m[3][0],
                       before.globalBindMatrix.m[3][1],before.globalBindMatrix.m[3][2]),true) &&
                   vec3TransformCoord(&segmentTailBefore,&before.tailOffset,&before.globalBindMatrix) &&
                   staticMesh.getSkeletonBindBone(3,preservedDescendantBefore) &&
                   staticMesh.translateSkeletalBoneSegment(1,VEC3(1.0f,1.25f,0.0f),true,
                       reparentError,sizeof(reparentError)) &&
                   staticMesh.getSkeletonBindBone(1,after) &&
                   (segmentHeadAfter=VEC3(after.globalBindMatrix.m[3][0],
                       after.globalBindMatrix.m[3][1],after.globalBindMatrix.m[3][2]),true) &&
                   vec3TransformCoord(&segmentTailAfter,&after.tailOffset,&after.globalBindMatrix) &&
                   std::fabs((segmentHeadAfter.x-segmentHeadBefore.x)-
                       (segmentTailAfter.x-segmentTailBefore.x))<=MATRIX_TOLERANCE &&
                   std::fabs((segmentHeadAfter.y-segmentHeadBefore.y)-
                       (segmentTailAfter.y-segmentTailBefore.y))<=MATRIX_TOLERANCE &&
                   staticMesh.getSkeletonBindBone(3,preservedDescendantAfter) &&
                   maximumMatrixDifference(preservedDescendantBefore.globalBindMatrix,
                       preservedDescendantAfter.globalBindMatrix)<=MATRIX_TOLERANCE,
               "segment translation must move both endpoints rigidly and preserve other joints");
        expect(staticMesh.setSkeletalBoneBind(1,VEC3(1,1,0),0,0,0,1,VEC3(1,1,1),0.1f,1.0f,
                   reparentError,sizeof(reparentError)) &&
                   staticMesh.getSkeletonBindBone(1,after) && !after.connectedToParent,
               "mirror fixture must offset the source subtree from the reflection plane");
        uint32_t mirroredRootIndex = 0;
        expect(staticMesh.mirrorSkeletalBoneSubtree(1,0,"right_",&mirroredRootIndex,
                   reparentError,sizeof(reparentError)) && mirroredRootIndex==4 &&
                   staticMesh.getSkeletonBindSummary(addSummary) && addSummary.boneCount==7 &&
                   staticMesh.getSkeletonBindBone(mirroredRootIndex,after) && after.parentIndex==0 &&
                   std::string(staticMesh.getSkeletonBindBoneName(mirroredRootIndex))=="right_spine_1" &&
                   std::fabs(after.globalBindMatrix.m[3][0]+1.0f)<=MATRIX_TOLERANCE,
               "canonical subtree mirror must preserve hierarchy and reflect global bind position");
        expect(!staticMesh.mirrorSkeletalBoneSubtree(1,0,"right_",&mirroredRootIndex,
                   reparentError,sizeof(reparentError)) &&
                   staticMesh.getSkeletonBindSummary(addSummary) && addSummary.boneCount==7,
               "canonical subtree mirror must reject duplicate generated names without mutation");
        uint32_t initializedVertexCount = 0;
        expect(staticMesh.initializeSkeletalVertexWeights(0,&initializedVertexCount,
                   reparentError,sizeof(reparentError)) && initializedVertexCount>0,
               "local rig must initialize complete rigid canonical weights explicitly");
        const char *initialWeightBone=nullptr, *initialUnused1=nullptr, *initialUnused2=nullptr, *initialUnused3=nullptr;
        float initialWeight=0, initialWeight1=0, initialWeight2=0, initialWeight3=0;
        expect(staticMesh.getSkeletalVertexWeight(0,&initialWeightBone,&initialWeight,
                   &initialUnused1,&initialWeight1,&initialUnused2,&initialWeight2,
                   &initialUnused3,&initialWeight3) && initialWeightBone &&
                   std::string(initialWeightBone)=="root" && std::fabs(initialWeight-1.0f)<=MATRIX_TOLERANCE,
               "initialized weights must rigidly cover every vertex with the selected bone");
        expect(!staticMesh.initializeSkeletalVertexWeights(1,&initializedVertexCount,
                   reparentError,sizeof(reparentError)),
               "weight initialization must reject an asset that already has type-42 weights");
        expect(staticMesh.saveV11(initializedPath,false,false,false,reparentError,sizeof(reparentError)),
               "canonical chain and mirrored subtree must save");
        MESH_MBM_DEBUG initializedReload;
        expect(initializedReload.loadV11(initializedPath) &&
                   initializedReload.getSkeletonBindSummary(addSummary) && addSummary.boneCount==7 &&
                   initializedReload.hasSkeletalVertexWeights() &&
                   initializedReload.getSkeletonBindBone(0,after) && after.hasExplicitTail &&
                   std::fabs(after.tailOffset.x-0.5f)<=MATRIX_TOLERANCE &&
                   std::fabs(after.tailOffset.y-1.5f)<=MATRIX_TOLERANCE &&
                   initializedReload.getSkeletonBindBone(2,after) && after.connectedToParent,
               "initialized canonical skeleton, chain, mirror, and weights must survive save/reload");
        expect(!staticMesh.initializeSkeletalSkeleton("other",VEC3(),0.1f,1.0f,true,
                   reparentError,sizeof(reparentError)),
               "initial skeleton creation must reject an asset that already has skeletal data");
        uint32_t removedWeightVertices = 0;
        expect(initializedReload.removeSkeletalVertexWeights(&removedWeightVertices,
                   reparentError,sizeof(reparentError)) && removedWeightVertices==initializedVertexCount &&
                   !initializedReload.hasSkeletalVertexWeights() &&
                   initializedReload.getSkeletonBindSummary(addSummary) && addSummary.boneCount==7,
               "canonical weight removal must preserve the complete skeleton");
        expect(!initializedReload.removeSkeletalVertexWeights(&removedWeightVertices,
                   reparentError,sizeof(reparentError)),
               "canonical weight removal must reject an asset without type-42 data");
        expect(initializedReload.initializeSkeletalVertexWeights(0,&initializedVertexCount,
                   reparentError,sizeof(reparentError)),
               "weight initialization must remain available after explicit removal");
        uint32_t removedBones = 0, removedVertices = 0, removedClips = 0;
        expect(initializedReload.removeAllSkeletalData(&removedBones,&removedVertices,&removedClips,
                   reparentError,sizeof(reparentError)) && removedBones==7 &&
                   removedVertices==initializedVertexCount && removedClips==0 &&
                   !initializedReload.getSkeletonBindSummary(addSummary) &&
                   !initializedReload.hasSkeletalVertexWeights(),
               "complete skeletal removal must atomically clear canonical skeleton and weights");
        expect(!initializedReload.removeAllSkeletalData(&removedBones,&removedVertices,&removedClips,
                   reparentError,sizeof(reparentError)),
               "complete skeletal removal must reject an asset without a canonical skeleton");
        std::remove(initializedPath);
        std::remove(reparentPath);
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
        expect(writeCanonicalWeightedFixture(valid,100,1.0f,100,10,0,true) && mesh.loadV11(valid),
               "canonical weight reader must resolve sections independent of file order");
        SKELETON_BIND_SUMMARY summary;
        SKELETON_BIND_BONE_INFO bone;
        expect(mesh.getSkeletonBindSummary(summary) &&
                   summary.valid && summary.canonical && summary.boneCount == 1 && mesh.getSkeletonBindBone(0, bone) &&
                   std::string(mesh.getSkeletonBindBoneName(0)) == "root",
               "bind report must inspect canonical section 41 without populating legacy bones");
        const char *name0=nullptr, *name1=nullptr, *name2=nullptr, *name3=nullptr;
        float weight0=0, weight1=0, weight2=0, weight3=0;
        expect(mesh.hasSkeletalVertexWeights() && mesh.getTotalSkeletalWeightBones()==1 &&
                   mesh.getSkeletalVertexWeight(0,&name0,&weight0,&name1,&weight1,&name2,&weight2,
                                                &name3,&weight3) && name0 &&
                   std::string(name0)=="root" && std::fabs(weight0-1.0f)<=MATRIX_TOLERANCE,
               "canonical editor weight reader must resolve type-42 IDs to bone names");
        char editError[255]="";
        expect(mesh.setSkeletalVertexWeight(0,"root",1.0f,nullptr,0,nullptr,0,nullptr,0,
                                             editError,static_cast<int>(sizeof(editError))),
               "canonical editor weight mutation must accept a valid normalized influence");
        expect(!mesh.setSkeletalVertexWeight(0,"missing",1.0f,nullptr,0,nullptr,0,nullptr,0,
                                              editError,static_cast<int>(sizeof(editError))) &&
                   mesh.getSkeletalVertexWeight(0,&name0,&weight0,&name1,&weight1,&name2,&weight2,
                                                &name3,&weight3) && name0 && std::string(name0)=="root",
               "rejected canonical editor mutation must preserve the previous vertex weights");

        const char *batchFixture = "/tmp/mini-mbm-canonical-weights-batch.msh";
        MESH_MBM_DEBUG batchMesh;
        expect(writeCanonicalWeightedFixture(batchFixture, 100, 1.0f, 100, 10, 0, true) &&
                   batchMesh.loadV11(batchFixture) &&
                   batchMesh.addSkeletalBone(0, "child-a", VEC3(0, 1, 0), 0.1f, 1.0f, true, false,
                       nullptr, editError, static_cast<int>(sizeof(editError))) &&
                   batchMesh.addSkeletalBone(0, "child-b", VEC3(1, 0, 0), 0.1f, 1.0f, true, false,
                       nullptr, editError, static_cast<int>(sizeof(editError))) &&
                   batchMesh.addSkeletalBone(0, "child-c", VEC3(0, 0, 1), 0.1f, 1.0f, true, false,
                       nullptr, editError, static_cast<int>(sizeof(editError))),
               "canonical batch fixture must provide four stable influences and three vertices");
        const SKELETAL_VERTEX_WEIGHT_EDIT validBatch[2] = {
            {0, {"root", "child-a", "child-b", "child-c"}, {0.4f, 0.3f, 0.2f, 0.1f}},
            {1, {"child-a", "root", nullptr, nullptr}, {0.75f, 0.25f, 0.0f, 0.0f}}
        };
        expect(batchMesh.setSkeletalVertexWeightsBatch(validBatch, 2, editError,
                   static_cast<int>(sizeof(editError))) &&
                   batchMesh.getSkeletalVertexWeight(0, &name0, &weight0, &name1, &weight1,
                       &name2, &weight2, &name3, &weight3) && name3 &&
                   std::string(name0) == "root" && std::string(name3) == "child-c" &&
                   std::fabs(weight0 - 0.4f) <= MATRIX_TOLERANCE &&
                   std::fabs(weight3 - 0.1f) <= MATRIX_TOLERANCE,
               "canonical batch mutation must atomically accept four influences per vertex");
        const SKELETAL_VERTEX_WEIGHT_EDIT rejectedBatch[2] = {
            {0, {"child-c", nullptr, nullptr, nullptr}, {1.0f, 0.0f, 0.0f, 0.0f}},
            {2, {"missing", nullptr, nullptr, nullptr}, {1.0f, 0.0f, 0.0f, 0.0f}}
        };
        expect(!batchMesh.setSkeletalVertexWeightsBatch(rejectedBatch, 2, editError,
                    static_cast<int>(sizeof(editError))) &&
                   batchMesh.getSkeletalVertexWeight(0, &name0, &weight0, &name1, &weight1,
                       &name2, &weight2, &name3, &weight3) && name0 &&
                   std::string(name0) == "root" && std::fabs(weight0 - 0.4f) <= MATRIX_TOLERANCE,
               "a rejected canonical batch must preserve every earlier candidate edit");
        const SKELETAL_VERTEX_WEIGHT_EDIT duplicateBatch[2] = {
            {2, {"root", nullptr, nullptr, nullptr}, {1.0f, 0.0f, 0.0f, 0.0f}},
            {2, {"child-a", nullptr, nullptr, nullptr}, {1.0f, 0.0f, 0.0f, 0.0f}}
        };
        expect(!batchMesh.setSkeletalVertexWeightsBatch(duplicateBatch, 2, editError,
                    static_cast<int>(sizeof(editError))),
               "canonical batch mutation must reject duplicate vertex indices");
        const char *batchReloadPath = "/tmp/mini-mbm-canonical-weights-batch-edited.msh";
        MESH_MBM_DEBUG batchReloaded;
        expect(batchMesh.saveV11(batchReloadPath, false, false, false, editError,
                    static_cast<int>(sizeof(editError))) && batchReloaded.loadV11(batchReloadPath),
               "canonical batch mutation must save and reload");
        expect(batchReloaded.getSkeletalVertexWeight(0, &name0, &weight0, &name1, &weight1,
                       &name2, &weight2, &name3, &weight3) && name0 && name1 && name2 && name3 &&
                   std::string(name0) == "root" && std::string(name1) == "child-a" &&
                   std::string(name2) == "child-b" && std::string(name3) == "child-c" &&
                   std::fabs(weight0 - 0.4f) <= MATRIX_TOLERANCE &&
                   std::fabs(weight1 - 0.3f) <= MATRIX_TOLERANCE &&
                   std::fabs(weight2 - 0.2f) <= MATRIX_TOLERANCE &&
                   std::fabs(weight3 - 0.1f) <= MATRIX_TOLERANCE,
               "four-influence canonical batch edit must survive save and reload exactly");
        expect(batchReloaded.getSkeletalVertexWeight(1, &name0, &weight0, &name1, &weight1,
                       &name2, &weight2, &name3, &weight3) && name0 && name1 && !name2 && !name3 &&
                   std::string(name0) == "child-a" && std::string(name1) == "root" &&
                   std::fabs(weight0 - 0.75f) <= MATRIX_TOLERANCE &&
                   std::fabs(weight1 - 0.25f) <= MATRIX_TOLERANCE &&
                   std::fabs(weight2) <= MATRIX_TOLERANCE &&
                   std::fabs(weight3) <= MATRIX_TOLERANCE,
               "two-influence canonical batch edit must survive save and reload exactly");
        std::remove(batchFixture);
        std::remove(batchReloadPath);
        const char *edited="/tmp/mini-mbm-canonical-weights-edited.msh";
        MESH_MBM_DEBUG reloaded;
        expect(mesh.saveV11(edited,false,false,false,editError,static_cast<int>(sizeof(editError))) &&
                   reloaded.loadV11(edited) && reloaded.getSkeletalVertexWeight(0,&name0,&weight0,
                       &name1,&weight1,&name2,&weight2,&name3,&weight3) && name0 &&
                   std::string(name0)=="root" && std::fabs(weight0-1.0f)<=MATRIX_TOLERANCE,
               "canonical editor weight mutation must survive save and reload");
        std::remove(edited);
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
        MESH_MBM_DEBUG clipEditMesh;
        uint32_t editedClipIndex = 0;
        expect(clipEditMesh.loadV11(source) &&
                   clipEditMesh.addSkeletalClip("idle",2.0f,false,&editedClipIndex,error,sizeof(error)-1) &&
                   editedClipIndex==1 &&
                   clipEditMesh.updateSkeletalClip(editedClipIndex,"idle-loop",3.0f,true,
                       error,sizeof(error)-1),
               "canonical clip container authoring must allocate identity and update properties atomically");
        SKELETAL_CLIP_INFO editedClip;
        uint32_t editedTrackIndex = 0;
        uint32_t editedKeyIndex = 0;
        SKELETAL_TRACK_INFO editedTrack;
        SKELETAL_KEY_INFO bindSeedKey;
        expect(clipEditMesh.getSkeletalClip(editedClipIndex,editedClip) && editedClip.clipId!=0 &&
                   std::string(clipEditMesh.getSkeletalClipName(editedClipIndex))=="idle-loop" &&
                   std::fabs(editedClip.duration-3.0f)<=MATRIX_TOLERANCE && editedClip.loop &&
                   clipEditMesh.addSkeletalTrack(editedClipIndex,0,
                       SKELETAL_CHANNEL_TRANSLATION|SKELETAL_CHANNEL_ROTATION,
                       &editedTrackIndex,error,sizeof(error)-1) && editedTrackIndex==0 &&
                   clipEditMesh.getSkeletalTrack(editedClipIndex,editedTrackIndex,editedTrack) &&
                   editedTrack.boneId==10 && editedTrack.keyCount==1 &&
                   clipEditMesh.getSkeletalKey(editedClipIndex,editedTrackIndex,0,bindSeedKey) &&
                   std::fabs(bindSeedKey.time)<=MATRIX_TOLERANCE &&
                   std::fabs(bindSeedKey.localRotationW-1.0f)<=MATRIX_TOLERANCE &&
                   !clipEditMesh.addSkeletalTrack(editedClipIndex,0,SKELETAL_CHANNEL_TRANSLATION,
                       &editedTrackIndex,error,sizeof(error)-1) &&
                   clipEditMesh.updateSkeletalTrackChannels(editedClipIndex,editedTrackIndex,
                       SKELETAL_CHANNEL_TRANSLATION|SKELETAL_CHANNEL_ROTATION|SKELETAL_CHANNEL_SCALE,
                       error,sizeof(error)-1) &&
                   !clipEditMesh.updateSkeletalTrackChannels(editedClipIndex,editedTrackIndex,0,
                       error,sizeof(error)-1) &&
                   clipEditMesh.addSkeletalKey(editedClipIndex,editedTrackIndex,1.5f,
                       &editedKeyIndex,error,sizeof(error)-1) && editedKeyIndex==1 &&
                   clipEditMesh.updateSkeletalKey(editedClipIndex,editedTrackIndex,editedKeyIndex,2.0f,
                       VEC3(1,2,3),0,0,0,2,VEC3(1,1,1),4,0,0,1,1,error,sizeof(error)-1) &&
                   !clipEditMesh.updateSkeletalKey(editedClipIndex,editedTrackIndex,editedKeyIndex,0.0f,
                       VEC3(),0,0,0,1,VEC3(1,1,1),0,0,0,1,1,error,sizeof(error)-1),
               "canonical track/key authoring must seed sampled TRS, reject duplicates, and validate atomically");
        SKELETAL_POSE_BONE_INFO authoringBone;
        SKELETAL_KEY_INFO authoringOverride;
        authoringOverride.localTranslation=VEC3(4,5,6);
        authoringOverride.localRotationW=1.0f;
        authoringOverride.localScale=VEC3(1,1,1);
        std::vector<float> authoringPalette(12);
        expect(clipEditMesh.evaluateSkeletalAuthoringPose(editedClipIndex,1.0f,-1,nullptr,
                   SKELETAL_SHADER_METHOD::LBS,error,sizeof(error)-1) &&
                   clipEditMesh.getSkeletalAuthoringPoseBoneCount()==1 &&
                   clipEditMesh.getSkeletalAuthoringPaletteSize()==12 &&
                   clipEditMesh.getSkeletalAuthoringPoseBone(0,authoringBone) &&
                   clipEditMesh.copySkeletalAuthoringPalette(authoringPalette.data(),12) &&
                   clipEditMesh.evaluateSkeletalAuthoringPose(editedClipIndex,1.0f,0,&authoringOverride,
                       SKELETAL_SHADER_METHOD::LBS,error,sizeof(error)-1) &&
                   clipEditMesh.getSkeletalAuthoringPoseBone(0,authoringBone) &&
                   std::fabs(authoringBone.globalMatrix.p[12]-4.0f)<=MATRIX_TOLERANCE &&
                   std::fabs(authoringBone.globalMatrix.p[13]-5.0f)<=MATRIX_TOLERANCE &&
                   std::fabs(authoringBone.globalMatrix.p[14]-6.0f)<=MATRIX_TOLERANCE,
               "in-memory authoring evaluation must expose sampled pose/palette and compose a local override");
        SKELETAL_KEY_INFO committedLocal;
        committedLocal.localTranslation=VEC3(3,4,5);
        committedLocal.localRotationW=1.0f;
        committedLocal.localScale=VEC3(1,1,1);
        bool createdAuthoringKey=false;
        const uint32_t movedTracks[2]={editedTrackIndex,editedTrackIndex};
        const uint32_t movedKeys[2]={1,2};
        const uint32_t collisionTrack[1]={editedTrackIndex};
        const uint32_t collisionKey[1]={2};
        const uint32_t duplicateCollisionKey[1]={1};
        const uint32_t rippleKeys[2]={0,1};
        uint32_t rippleExtraIndex=0;
        uint32_t removedRangeKeys=0;
        expect(clipEditMesh.commitSkeletalAuthoringKey(editedClipIndex,0,1.0f,
                   SKELETAL_CHANNEL_TRANSLATION,committedLocal,&createdAuthoringKey,error,sizeof(error)-1) &&
                   createdAuthoringKey &&
                   clipEditMesh.getSkeletalTrack(editedClipIndex,editedTrackIndex,editedTrack) &&
                   editedTrack.keyCount==3 &&
                   clipEditMesh.moveSkeletalKeys(editedClipIndex,movedTracks,movedKeys,2,0.25f,
                       error,sizeof(error)-1) &&
                   clipEditMesh.duplicateSkeletalKeys(editedClipIndex,movedTracks,movedKeys,2,0.5f,
                       error,sizeof(error)-1) &&
                   clipEditMesh.removeSkeletalKey(editedClipIndex,editedTrackIndex,4,
                       error,sizeof(error)-1) &&
                   clipEditMesh.removeSkeletalKey(editedClipIndex,editedTrackIndex,2,
                       error,sizeof(error)-1) &&
                   !clipEditMesh.duplicateSkeletalKeys(editedClipIndex,collisionTrack,
                       duplicateCollisionKey,1,1.0f,error,sizeof(error)-1) &&
                   !clipEditMesh.moveSkeletalKeys(editedClipIndex,collisionTrack,collisionKey,1,-2.25f,
                       error,sizeof(error)-1) &&
                   clipEditMesh.updateSkeletalClip(editedClipIndex,"idle-loop",6.0f,true,
                       error,sizeof(error)-1) &&
                   clipEditMesh.addSkeletalKey(editedClipIndex,editedTrackIndex,4.0f,
                       &rippleExtraIndex,error,sizeof(error)-1) && rippleExtraIndex==3 &&
                   clipEditMesh.insertSkeletalKeysRipple(editedClipIndex,movedTracks,rippleKeys,2,2.5f,
                       error,sizeof(error)-1) &&
                   clipEditMesh.getSkeletalClip(editedClipIndex,editedClip) &&
                   editedClip.duration>7.25f && editedClip.duration<7.251f &&
                   clipEditMesh.removeSkeletalKey(editedClipIndex,editedTrackIndex,5,error,sizeof(error)-1) &&
                   clipEditMesh.removeSkeletalKey(editedClipIndex,editedTrackIndex,4,error,sizeof(error)-1) &&
                   clipEditMesh.removeSkeletalKey(editedClipIndex,editedTrackIndex,3,error,sizeof(error)-1) &&
                   clipEditMesh.insertSkeletalEmptyTime(editedClipIndex,2.0f,0.5f,
                       error,sizeof(error)-1) &&
                   clipEditMesh.getSkeletalClip(editedClipIndex,editedClip) &&
                   editedClip.duration>7.75f && editedClip.duration<7.751f &&
                   clipEditMesh.getSkeletalKey(editedClipIndex,editedTrackIndex,2,bindSeedKey) &&
                   std::fabs(bindSeedKey.time-2.75f)<=MATRIX_TOLERANCE &&
                   clipEditMesh.removeSkeletalTimeRange(editedClipIndex,2.0f,0.5f,
                       &removedRangeKeys,error,sizeof(error)-1) && removedRangeKeys==0 &&
                   clipEditMesh.getSkeletalKey(editedClipIndex,editedTrackIndex,2,bindSeedKey) &&
                   std::fabs(bindSeedKey.time-2.25f)<=MATRIX_TOLERANCE &&
                   !clipEditMesh.removeSkeletalTimeRange(editedClipIndex,0.0f,3.0f,
                       &removedRangeKeys,error,sizeof(error)-1) &&
                   clipEditMesh.updateSkeletalClip(editedClipIndex,"idle-loop",3.0f,true,
                       error,sizeof(error)-1) &&
                   clipEditMesh.getSkeletalKey(editedClipIndex,editedTrackIndex,1,committedLocal) &&
                   std::fabs(committedLocal.time-1.25f)<=MATRIX_TOLERANCE &&
                   std::fabs(committedLocal.localTranslation.x-3.0f)<=MATRIX_TOLERANCE &&
                   clipEditMesh.removeSkeletalKey(editedClipIndex,editedTrackIndex,1,error,sizeof(error)-1),
               "explicit authoring commit must atomically create a translation key from the temporary pose");
        uint32_t pasteClipIndex=0;
        SKELETAL_KEY_INFO pastedKeys[2];
        pastedKeys[0].time=1.0f;
        pastedKeys[0].localRotationW=1.0f;
        pastedKeys[0].localTranslation=VEC3(7,8,9);
        pastedKeys[1]=pastedKeys[0];
        pastedKeys[1].time=2.0f;
        pastedKeys[1].localTranslation=VEC3(10,11,12);
        const uint64_t pastedBoneIds[2]={10,10};
        const uint8_t pastedMasks[2]={SKELETAL_CHANNEL_TRANSLATION|
            SKELETAL_CHANNEL_ROTATION,SKELETAL_CHANNEL_TRANSLATION|
            SKELETAL_CHANNEL_ROTATION};
        const uint8_t incompatiblePasteMasks[2]={SKELETAL_CHANNEL_TRANSLATION,
            SKELETAL_CHANNEL_TRANSLATION};
        SKELETAL_KEY_INFO pastedResult;
        expect(clipEditMesh.addSkeletalClip("paste-target",3.0f,false,&pasteClipIndex,
                   error,sizeof(error)-1) &&
                   clipEditMesh.pasteSkeletalKeys(pasteClipIndex,pastedBoneIds,pastedMasks,
                       pastedKeys,2,1.0f,0.5f,error,sizeof(error)-1) &&
                   clipEditMesh.getSkeletalTrack(pasteClipIndex,0,editedTrack) &&
                   editedTrack.boneId==10 && editedTrack.keyCount==2 &&
                   editedTrack.channelMask==(SKELETAL_CHANNEL_TRANSLATION|
                       SKELETAL_CHANNEL_ROTATION) &&
                   clipEditMesh.getSkeletalKey(pasteClipIndex,0,1,pastedResult) &&
                   std::fabs(pastedResult.time-1.5f)<=MATRIX_TOLERANCE &&
                   std::fabs(pastedResult.localTranslation.x-10.0f)<=MATRIX_TOLERANCE &&
                   !clipEditMesh.pasteSkeletalKeys(pasteClipIndex,pastedBoneIds,
                       incompatiblePasteMasks,pastedKeys,2,1.0f,2.0f,error,sizeof(error)-1) &&
                   clipEditMesh.getSkeletalTrack(pasteClipIndex,0,editedTrack) &&
                   editedTrack.keyCount==2 &&
                   clipEditMesh.removeSkeletalClip(pasteClipIndex,error,sizeof(error)-1),
               "detached skeletal key paste must create missing tracks, preserve payloads, and reject incompatible candidates atomically");
        const char *trackRoundTrip="/tmp/mini-mbm-canonical-track-round-trip.msh";
        MESH_MBM_DEBUG trackReload;
        expect(clipEditMesh.saveV11(trackRoundTrip,false,false,false,error,sizeof(error)-1) &&
                   trackReload.loadV11(trackRoundTrip) &&
                   trackReload.getSkeletalTrack(editedClipIndex,editedTrackIndex,editedTrack) &&
                   editedTrack.channelMask==(SKELETAL_CHANNEL_TRANSLATION|
                       SKELETAL_CHANNEL_ROTATION|SKELETAL_CHANNEL_SCALE) && editedTrack.keyCount==2 &&
                   trackReload.getSkeletalKey(editedClipIndex,editedTrackIndex,1,bindSeedKey) &&
                   std::fabs(bindSeedKey.time-2.25f)<=MATRIX_TOLERANCE &&
                   std::fabs(bindSeedKey.localTranslation.x-1.0f)<=MATRIX_TOLERANCE &&
                   std::fabs(bindSeedKey.localRotationW-1.0f)<=MATRIX_TOLERANCE && bindSeedKey.easing==4 &&
                   clipEditMesh.removeSkeletalKey(editedClipIndex,editedTrackIndex,editedKeyIndex,
                       error,sizeof(error)-1) &&
                   !clipEditMesh.removeSkeletalKey(editedClipIndex,editedTrackIndex,0,
                       error,sizeof(error)-1) &&
                   clipEditMesh.removeSkeletalTrack(editedClipIndex,editedTrackIndex,
                       error,sizeof(error)-1) &&
                   !clipEditMesh.updateSkeletalClip(0,"idle-loop",1.0f,true,error,sizeof(error)-1) &&
                   clipEditMesh.removeSkeletalClip(editedClipIndex,error,sizeof(error)-1) &&
                   clipEditMesh.getTotalSkeletalClips()==1,
               "canonical track edits must survive save/reload and remove complete key ownership");
        std::remove(trackRoundTrip);
        const char *emptyAnimationPath="/tmp/mini-mbm-canonical-no-clips.msh";
        MESH_MBM_DEBUG emptyAnimationReload;
        expect(clipEditMesh.removeSkeletalClip(0,error,sizeof(error)-1) &&
                   clipEditMesh.getTotalSkeletalClips()==0 &&
                   clipEditMesh.saveV11(emptyAnimationPath,false,false,false,error,sizeof(error)-1) &&
                   emptyAnimationReload.loadV11(emptyAnimationPath) &&
                   emptyAnimationReload.getTotalSkeletalClips()==0,
               "removing the final canonical clip must omit type-43 cleanly across save/reload");
        std::remove(emptyAnimationPath);
        uint32_t completePoseClip=0;
        const uint64_t completePoseBoneIds[1]={10};
        SKELETAL_KEY_INFO completePoseLocals[1];
        completePoseLocals[0].localTranslation=VEC3(2,3,4);
        completePoseLocals[0].localRotationW=1.0f;
        expect(clipEditMesh.addSkeletalClip("complete-pose",2.0f,false,&completePoseClip,
                   error,sizeof(error)-1) &&
                   clipEditMesh.commitSkeletalAuthoringPose(completePoseClip,1.0f,
                       completePoseBoneIds,completePoseLocals,1,error,sizeof(error)-1) &&
                   clipEditMesh.getSkeletalTrack(completePoseClip,0,editedTrack) &&
                   editedTrack.channelMask==(SKELETAL_CHANNEL_TRANSLATION|
                       SKELETAL_CHANNEL_ROTATION|SKELETAL_CHANNEL_SCALE) &&
                   editedTrack.keyCount==2 &&
                   clipEditMesh.getSkeletalKey(completePoseClip,0,1,pastedResult) &&
                   std::fabs(pastedResult.localTranslation.x-2.0f)<=MATRIX_TOLERANCE &&
                   !clipEditMesh.commitSkeletalAuthoringPose(completePoseClip,1.5f,
                       completePoseBoneIds,completePoseLocals,0,error,sizeof(error)-1) &&
                   clipEditMesh.getSkeletalTrack(completePoseClip,0,editedTrack) &&
                   editedTrack.keyCount==2 &&
                   clipEditMesh.removeSkeletalClip(completePoseClip,error,sizeof(error)-1),
               "complete skeletal pose authoring must commit every bone atomically and reject incomplete candidates");
        SKELETON_BIND_BONE_INFO referencedBone;
        uint32_t temporaryBoneIndex = 0;
        expect(mesh.addSkeletalBone(-1, "temporary-root", VEC3(), 0.1f, 1.0f, true, false,
                   &temporaryBoneIndex, error, sizeof(error) - 1),
               "reference-removal fixture must add an independent unreferenced root");
        expect(mesh.getSkeletonBindBone(0, referencedBone) && referencedBone.weightPaletteReferenced &&
                   referencedBone.weightedVertexCount == 3 && referencedBone.animationTrackCount == 1 &&
                   !mesh.removeSkeletalBone(0, error, sizeof(error) - 1),
               "bind report must expose weight/track impact and strict removal must reject references");
        expect(mesh.removeSkeletalBone(temporaryBoneIndex, error, sizeof(error) - 1),
               "reference-removal fixture must remove its unreferenced temporary root");
        MESH_MBM_DEBUG remapMesh;
        expect(remapMesh.loadV11(source) &&
                   remapMesh.addSkeletalBone(-1, "replacement-root", VEC3(), 0.1f, 1.0f, true, false,
                       &temporaryBoneIndex, error, sizeof(error) - 1) &&
                   remapMesh.setSkeletalVertexWeight(0,"root",0.4f,"replacement-root",0.6f,
                       nullptr,0.0f,nullptr,0.0f,error,sizeof(error)-1) &&
                   !remapMesh.removeSkeletalBoneRemapped(0, temporaryBoneIndex, false, false,
                       error, sizeof(error) - 1),
               "referenced removal must require explicit track-discard confirmation");
        expect(remapMesh.removeSkeletalBoneRemapped(0, temporaryBoneIndex, true, false,
                   error, sizeof(error) - 1),
               "referenced leaf removal must transfer weights and explicitly discard tracks");
        const char *remappedName=nullptr, *remapUnused1=nullptr, *remapUnused2=nullptr, *remapUnused3=nullptr;
        float remappedWeight=0, remapWeight1=0, remapWeight2=0, remapWeight3=0;
        expect(remapMesh.getSkeletalVertexWeight(0,&remappedName,&remappedWeight,
                   &remapUnused1,&remapWeight1,&remapUnused2,&remapWeight2,
                   &remapUnused3,&remapWeight3) && remappedName &&
                   std::string(remappedName)=="replacement-root" &&
                   std::fabs(remappedWeight-1.0f)<=MATRIX_TOLERANCE,
               "referenced leaf removal must preserve normalized vertex coverage on replacement");
        MESH_MBM_DEBUG animatedHierarchy;
        const char *convertedHierarchy = "/tmp/mini-mbm-converted-child-tracks.msh";
        CANONICAL_PARITY_ASSET beforeConversion, afterConversion;
        expect(animatedHierarchy.loadV11(source) &&
                   animatedHierarchy.addSkeletalBone(0,"animated-child",VEC3(0,1,0),0.1f,1.0f,true,false,
                       &temporaryBoneIndex,error,sizeof(error)-1) &&
                   copyCanonicalParityAsset(animatedHierarchy,beforeConversion) &&
                   animatedHierarchy.removeSkeletalBoneRemapped(0,temporaryBoneIndex,true,true,
                       error,sizeof(error)-1) &&
                   copyCanonicalParityAsset(animatedHierarchy,afterConversion) &&
                   animatedHierarchy.saveV11(convertedHierarchy,false,false,false,error,sizeof(error)-1),
               "child reparent removal must bake animated parent motion into promoted child tracks");
        SKELETAL_POSE beforePose, afterPose;
        expect(sampleSkeletalClip(beforeConversion.skeleton.compiled,beforeConversion.animations.clips[0],
                   0.0f,beforePose) &&
                   sampleSkeletalClip(afterConversion.skeleton.compiled,afterConversion.animations.clips[0],
                   0.0f,afterPose) &&
                   maximumMatrixDifference(beforePose.globalTransforms[temporaryBoneIndex],
                       afterPose.globalTransforms[0])<=MATRIX_TOLERANCE,
               "baked child track must preserve the authored global pose sample");
        MESH_MBM_DEBUG convertedReload;
        expect(convertedReload.loadV11(convertedHierarchy),
               "converted child tracks must survive canonical save/reload validation");
        std::remove(convertedHierarchy);
        expect(mesh.scaleSkeletalAsset(100.0f, error, sizeof(error) - 1),
               "canonical editor must scale the complete skeletal asset transactionally");
        expect(mesh.renameSkeletalBone(0, "renamed-root", error, sizeof(error) - 1),
               "canonical editor must rename a bone while preserving its stable ID dependencies");
        expect(!mesh.renameSkeletalBone(0, "", error, sizeof(error) - 1),
               "canonical editor must reject an empty bone name without mutating the skeleton");
        expect(mesh.saveV11(roundTrip, false, false, false, error, sizeof(error) - 1),
               "canonical writer must save validated sections 41-43");

        MESH_MBM_DEBUG reloaded;
        expect(reloaded.loadV11(roundTrip),
               "canonical writer output must reload with all dependencies intact");
        SKELETAL_CLIP_INFO inspectedClip;
        SKELETAL_TRACK_INFO inspectedTrack;
        SKELETAL_KEY_INFO inspectedKey;
        expect(reloaded.getTotalSkeletalClips()==1 && reloaded.getSkeletalClip(0,inspectedClip) &&
                   std::string(reloaded.getSkeletalClipName(0))=="walk" && inspectedClip.clipId==200 &&
                   std::fabs(inspectedClip.duration-1.0f)<=MATRIX_TOLERANCE && inspectedClip.loop &&
                   inspectedClip.trackCount==1 && reloaded.getSkeletalTrack(0,0,inspectedTrack) &&
                   inspectedTrack.boneId==10 && inspectedTrack.boneIndex==0 &&
                   inspectedTrack.channelMask==SKELETAL_CHANNEL_TRANSLATION && inspectedTrack.keyCount==1 &&
                   reloaded.getSkeletalKey(0,0,0,inspectedKey) &&
                   std::fabs(inspectedKey.time)<=MATRIX_TOLERANCE && inspectedKey.easing==0,
               "canonical clip/track/key inspection must expose the persisted type-43 contract");
        SKELETON_BIND_BONE_INFO scaledBone;
        expect(reloaded.getSkeletonBindBone(0, scaledBone) &&
                   std::string(reloaded.getSkeletonBindBoneName(0)) == "renamed-root" &&
                   std::fabs(scaledBone.radius - 10.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(scaledBone.length - 100.0f) <= MATRIX_TOLERANCE,
               "canonical scale and rename must survive save/reload");
        const char *renamedWeightBone=nullptr, *unusedName1=nullptr, *unusedName2=nullptr, *unusedName3=nullptr;
        float renamedWeight=0, unusedWeight1=0, unusedWeight2=0, unusedWeight3=0;
        expect(reloaded.getSkeletalVertexWeight(0,&renamedWeightBone,&renamedWeight,
                   &unusedName1,&unusedWeight1,&unusedName2,&unusedWeight2,&unusedName3,&unusedWeight3) &&
                   renamedWeightBone && std::string(renamedWeightBone)=="renamed-root" &&
                   std::fabs(renamedWeight-1.0f)<=MATRIX_TOLERANCE,
               "canonical weights must follow a renamed bone by stable ID after save/reload");

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

    void testSkinningCapability()
    {
        const SKINNING_CAPABILITY minimum = calculateSkinningCapability(128, 8);
        expect(minimum.measured && minimum.hasRequiredVertexAttributes &&
                   minimum.reservedVertexShaderVectors == 8 &&
                   minimum.lbsMatrixPaletteBones == 40 && minimum.dqsRigidPaletteBones == 60,
               "GPU skinning capability must reserve scene matrices before calculating palettes");
        const SKINNING_CAPABILITY insufficientAttributes =
            calculateSkinningCapability(256, 4);
        expect(insufficientAttributes.measured && !insufficientAttributes.hasRequiredVertexAttributes &&
                   insufficientAttributes.lbsMatrixPaletteBones == 0 &&
                   insufficientAttributes.dqsRigidPaletteBones == 0,
               "GPU skinning capability must reject an insufficient vertex-attribute budget");
        const SKINNING_CAPABILITY unavailable = calculateSkinningCapability(0, 0);
        expect(!unavailable.measured && unavailable.lbsMatrixPaletteBones == 0,
               "zero GPU capability results must remain unmeasured rather than claiming support");
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

        GPU_SKINNING_INPUT input;
        const SKINNING_CAPABILITY sufficient = calculateSkinningCapability(128, 8);
        expect(prepareGpuSkinningInput(skeleton, weights, sufficient, input) ==
                   GPU_SKINNING_PREPARATION_STATUS::READY && input.ready() &&
                   input.requiredBoneCount == 2 && input.effectiveBoneCapacity == 60 &&
                   input.lbsBoneCapacity == 40 && input.dqsBoneCapacity == 60 &&
                   input.vertices.size() == 1 && input.vertices[0].boneIndex[0] == 1.0f &&
                   input.vertices[0].boneIndex[1] == 0.0f &&
                   std::fabs(input.vertices[0].weight[0] - 0.75f) <= MATRIX_TOLERANCE,
               "GPU LBS input must resolve stable palette IDs to compiled float attributes");

        SKINNING_CAPABILITY dqsOnly = sufficient;
        dqsOnly.lbsMatrixPaletteBones = 1;
        dqsOnly.dqsRigidPaletteBones = 2;
        expect(prepareGpuSkinningInput(skeleton, weights, dqsOnly, input) ==
                   GPU_SKINNING_PREPARATION_STATUS::READY && input.ready() &&
                   !input.supports(SKELETAL_SHADER_METHOD::LBS) &&
                   input.supports(SKELETAL_SHADER_METHOD::DQS_RIGID),
               "GPU skeletal input must distinguish the LBS and rigid-DQS palette limits");

        SKINNING_CAPABILITY tooSmall = sufficient;
        tooSmall.lbsMatrixPaletteBones = 1;
        tooSmall.dqsRigidPaletteBones = 1;
        expect(prepareGpuSkinningInput(skeleton, weights, tooSmall, input) ==
                   GPU_SKINNING_PREPARATION_STATUS::PALETTE_TOO_LARGE && input.vertices.empty() &&
                   input.requiredBoneCount == 2 && input.effectiveBoneCapacity == 1,
               "GPU LBS input must reject a skeleton larger than the measured uniform palette");
        expect(prepareGpuSkinningInput(skeleton, weights, {}, input) ==
                   GPU_SKINNING_PREPARATION_STATUS::CAPABILITY_UNAVAILABLE && input.vertices.empty(),
               "GPU LBS input must not claim readiness before backend capability measurement");
        CANONICAL_WEIGHTS missingWeights;
        expect(prepareGpuSkinningInput(skeleton,missingWeights,sufficient,input)==
                   GPU_SKINNING_PREPARATION_STATUS::INVALID_CANONICAL_DATA &&
                   std::string(input.diagnostic)=="canonical vertex weights are missing",
               "GPU skeletal input must distinguish a rig without deforming vertex weights");

        CANONICAL_ANIMATIONS rigidAnimations;
        expect(getDqsCompatibility(skeleton, rigidAnimations) == DQS_COMPATIBILITY_STATUS::RIGID,
               "auto skinning must select DQS for unit-scale bind and clips");
        skeleton.sourceBones[1].localBind.scale = VEC3(2.0f, 2.0f, 2.0f);
        expect(getDqsCompatibility(skeleton, rigidAnimations) ==
                   DQS_COMPATIBILITY_STATUS::BIND_CONTAINS_SCALE,
               "auto skinning must select LBS when bind contains scale");
        skeleton.sourceBones[1].localBind.scale = VEC3(1.0f, 1.0f, 1.0f);
        SKELETAL_CLIP scaledClip;
        SKELETAL_TRACK scaledTrack;
        scaledTrack.channelMask = SKELETAL_CHANNEL_SCALE;
        SKELETAL_KEY scaledKey;
        scaledKey.local.scale = VEC3(1.0f, 1.25f, 1.0f);
        scaledTrack.keys.push_back(scaledKey);
        scaledClip.tracks.push_back(scaledTrack);
        rigidAnimations.clips.push_back(scaledClip);
        expect(getDqsCompatibility(skeleton, rigidAnimations) ==
                   DQS_COMPATIBILITY_STATUS::CLIP_CONTAINS_SCALE,
               "auto skinning must select LBS when any clip contains scale");

        SKELETAL_POSE pose;
        pose.localTransforms = {root.localBind, child.localBind};
        pose.globalTransforms = {skeleton.compiled.bones[0].globalBindMatrix,
                                 skeleton.compiled.bones[1].globalBindMatrix};
        std::vector<float> palette;
        expect(buildLbsPalette(skeleton, pose, true, palette) ==
                   LBS_PALETTE_STATUS::READY && palette.size() == 24 &&
                   std::fabs(palette[0] - 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(palette[5] - 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(palette[10] - 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(palette[15]) <= MATRIX_TOLERANCE,
               "GPU LBS bind palette must pack identity in three row-vector output columns");

        LOCAL_TRANSFORM movedChild = child.localBind;
        movedChild.translation = VEC3(3, 0, 0);
        pose.globalTransforms[1] = buildTrsMatrix(movedChild);
        expect(buildLbsPalette(skeleton, pose, true, palette) ==
                   LBS_PALETTE_STATUS::READY &&
                   std::fabs(palette[15] - 3.0f) <= MATRIX_TOLERANCE,
               "GPU LBS palette must pack row-vector translation for the shader dot decoder");

        movedChild.scale = VEC3(2, 2, 2);
        pose.globalTransforms[1] = buildTrsMatrix(movedChild);
        expect(buildLbsPalette(skeleton, pose, true, palette) ==
                   LBS_PALETTE_STATUS::READY && palette.size() == 24,
               "compact GPU LBS normal palette must accept uniform scale");

        movedChild.scale = VEC3(2, 1, 1);
        pose.globalTransforms[1] = buildTrsMatrix(movedChild);
        expect(buildLbsPalette(skeleton, pose, true, palette) ==
                   LBS_PALETTE_STATUS::UNSUPPORTED_NORMAL_TRANSFORM && palette.empty(),
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
        expect(sampleLbsPalette(skeleton, clip, 0.5f, true, palette, &sampled) ==
                   LBS_PALETTE_STATUS::READY && palette.size() == 24 &&
                   std::fabs(palette[15] - 2.0f) <= MATRIX_TOLERANCE &&
                   sampled.globalTransforms.size() == 2,
               "GPU LBS palette sampling must evaluate local clips before packing skin matrices");

        pose.localTransforms = {root.localBind, child.localBind};
        pose.globalTransforms = {skeleton.compiled.bones[0].globalBindMatrix,
                                 skeleton.compiled.bones[1].globalBindMatrix};
        expect(buildDqsPalette(skeleton, pose, palette) == DQS_PALETTE_STATUS::READY &&
                   palette.size() == 16 && std::fabs(palette[3] - 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(palette[11] - 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(palette[12]) <= MATRIX_TOLERANCE,
               "GPU DQS bind palette must pack identity real and zero dual quaternions");

        movedChild = child.localBind;
        movedChild.translation = VEC3(3, 0, 0);
        pose.globalTransforms[1] = buildTrsMatrix(movedChild);
        expect(buildDqsPalette(skeleton, pose, palette) == DQS_PALETTE_STATUS::READY &&
                   std::fabs(palette[12] - 1.5f) <= MATRIX_TOLERANCE,
               "GPU DQS palette must encode rigid translation in its dual quaternion");

        movedChild.translation = VEC3(0, 0, 0);
        movedChild.rotation = {0, 0, 0.70710678118f, 0.70710678118f};
        pose.globalTransforms[1] = buildTrsMatrix(movedChild);
        expect(buildDqsPalette(skeleton, pose, palette) == DQS_PALETTE_STATUS::READY &&
                   std::fabs(std::fabs(palette[10]) - 0.70710678118f) <= MATRIX_TOLERANCE &&
                   std::fabs(std::fabs(palette[11]) - 0.70710678118f) <= MATRIX_TOLERANCE,
               "GPU DQS palette must preserve a rigid 90-degree quaternion rotation");

        movedChild.scale = VEC3(2, 1, 1);
        pose.globalTransforms[1] = buildTrsMatrix(movedChild);
        expect(buildDqsPalette(skeleton, pose, palette) ==
                   DQS_PALETTE_STATUS::UNSUPPORTED_NON_RIGID_TRANSFORM && palette.empty(),
               "GPU rigid DQS palette must reject scale instead of silently discarding it");

        expect(sampleDqsPalette(skeleton, clip, 0.5f, palette, &sampled) ==
                   DQS_PALETTE_STATUS::READY && palette.size() == 16 &&
                   std::fabs(palette[12] - 1.0f) <= MATRIX_TOLERANCE &&
                   sampled.globalTransforms.size() == 2,
               "GPU DQS palette sampling must share canonical clip evaluation with LBS");
    }
}

int runSkeletalFoundationTests()
{
    failures = 0;
    testTrsRoundTrip();
    testSkeletalCompletionNotification();
    testCanonicalSkeletonCompilation();
    testAbsolutePoseComposition();
    testRootMotionPoseNeutralization();
    testRootMotionRotationDeltaAndNeutralization();
    testMultipleRootSkeletonSemantics();
    testUniformCanonicalAssetScale();
    testCanonicalSkeletonReader();
    testSkeletalSharingCompatibility();
    testCanonicalWeightValidation();
    testCanonicalWeightReader();
    testCanonicalAnimationReader();
    testCanonicalAnimationValidation();
    testCanonicalWriterRoundTrip();
    testCpuLbsReference();
    testSkinningCapability();
    testGles2LbsInputPreparation();
    if (failures == 0)
        std::fprintf(stdout, "[skeletal-foundation] PASS\n");
    return failures == 0 ? 0 : 1;
}
