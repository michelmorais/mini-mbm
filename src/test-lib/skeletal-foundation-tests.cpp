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

    void testUniformCanonicalAssetScale()
    {
        CANONICAL_BONE root;
        root.boneId = 10;
        root.name = "root";
        root.localBind.translation = VEC3(1.0f, 2.0f, 3.0f);
        root.radius = 0.25f;
        root.length = 2.0f;
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
                   std::fabs(scaledSkeleton.sourceBones[1].length - 150.0f) <= MATRIX_TOLERANCE,
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
                   0.1f, 1.5f, &addedIndex, reparentError, sizeof(reparentError)) &&
                   addedIndex == 2 && mesh.getSkeletonBindBone(addedIndex, edited) &&
                   edited.parentIndex == 1 && edited.boneId != 0 &&
                   std::fabs(edited.localTranslation.y - 2.0f) <= MATRIX_TOLERANCE,
               "canonical bone addition must append a valid child with a new stable ID");
        SKELETON_BIND_SUMMARY addSummary;
        expect(!mesh.addSkeletalBone(-1, "added-child", VEC3(), 0.1f, 1.0f,
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
        MESH_MBM_DEBUG staticMesh;
        expect(staticMesh.loadV11("src/test-lib/Crate.msh") &&
                   staticMesh.initializeSkeletalSkeleton("root",VEC3(0,0,0),0.1f,1.0f,
                       reparentError,sizeof(reparentError)) &&
                   staticMesh.getSkeletonBindSummary(addSummary) && addSummary.boneCount==1 &&
                   staticMesh.saveV11(initializedPath,false,false,false,reparentError,sizeof(reparentError)),
               "static mesh must initialize and save a one-root canonical skeleton");
        MESH_MBM_DEBUG initializedReload;
        expect(initializedReload.loadV11(initializedPath) &&
                   initializedReload.getSkeletonBindSummary(addSummary) && addSummary.boneCount==1,
               "initialized canonical skeleton must survive save/reload");
        expect(!staticMesh.initializeSkeletalSkeleton("other",VEC3(),0.1f,1.0f,
                   reparentError,sizeof(reparentError)),
               "initial skeleton creation must reject an asset that already has skeletal data");
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
        SKELETON_BIND_BONE_INFO referencedBone;
        uint32_t temporaryBoneIndex = 0;
        expect(mesh.addSkeletalBone(-1, "temporary-root", VEC3(), 0.1f, 1.0f,
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
                   remapMesh.addSkeletalBone(-1, "replacement-root", VEC3(), 0.1f, 1.0f,
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
                   animatedHierarchy.addSkeletalBone(0,"animated-child",VEC3(0,1,0),0.1f,1.0f,
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
                   input.requiredBoneCount == 2 && input.effectiveBoneCapacity == 60 &&
                   input.lbsBoneCapacity == 40 && input.dqsBoneCapacity == 60 &&
                   input.vertices.size() == 1 && input.vertices[0].boneIndex[0] == 1.0f &&
                   input.vertices[0].boneIndex[1] == 0.0f &&
                   std::fabs(input.vertices[0].weight[0] - 0.75f) <= MATRIX_TOLERANCE,
               "GPU LBS input must resolve stable palette IDs to compiled float attributes");

        GLES2_SKINNING_CAPABILITY dqsOnly = sufficient;
        dqsOnly.lbsMatrixPaletteBones = 1;
        dqsOnly.dqsRigidPaletteBones = 2;
        expect(prepareGles2LbsInput(skeleton, weights, dqsOnly, input) ==
                   GLES2_LBS_PREPARATION_STATUS::READY && input.ready() &&
                   !input.supports(SKELETAL_SHADER_METHOD::LBS) &&
                   input.supports(SKELETAL_SHADER_METHOD::DQS_RIGID),
               "GPU skeletal input must distinguish the LBS and rigid-DQS palette limits");

        GLES2_SKINNING_CAPABILITY tooSmall = sufficient;
        tooSmall.lbsMatrixPaletteBones = 1;
        tooSmall.dqsRigidPaletteBones = 1;
        expect(prepareGles2LbsInput(skeleton, weights, tooSmall, input) ==
                   GLES2_LBS_PREPARATION_STATUS::PALETTE_TOO_LARGE && input.vertices.empty() &&
                   input.requiredBoneCount == 2 && input.effectiveBoneCapacity == 1,
               "GPU LBS input must reject a skeleton larger than the measured uniform palette");
        expect(prepareGles2LbsInput(skeleton, weights, {}, input) ==
                   GLES2_LBS_PREPARATION_STATUS::CAPABILITY_UNAVAILABLE && input.vertices.empty(),
               "GPU LBS input must not claim readiness before backend capability measurement");

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

        pose.localTransforms = {root.localBind, child.localBind};
        pose.globalTransforms = {skeleton.compiled.bones[0].globalBindMatrix,
                                 skeleton.compiled.bones[1].globalBindMatrix};
        expect(buildGles2DqsPalette(skeleton, pose, palette) == GLES2_DQS_PALETTE_STATUS::READY &&
                   palette.size() == 16 && std::fabs(palette[3] - 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(palette[11] - 1.0f) <= MATRIX_TOLERANCE &&
                   std::fabs(palette[12]) <= MATRIX_TOLERANCE,
               "GPU DQS bind palette must pack identity real and zero dual quaternions");

        movedChild = child.localBind;
        movedChild.translation = VEC3(3, 0, 0);
        pose.globalTransforms[1] = buildTrsMatrix(movedChild);
        expect(buildGles2DqsPalette(skeleton, pose, palette) == GLES2_DQS_PALETTE_STATUS::READY &&
                   std::fabs(palette[12] - 1.5f) <= MATRIX_TOLERANCE,
               "GPU DQS palette must encode rigid translation in its dual quaternion");

        movedChild.translation = VEC3(0, 0, 0);
        movedChild.rotation = {0, 0, 0.70710678118f, 0.70710678118f};
        pose.globalTransforms[1] = buildTrsMatrix(movedChild);
        expect(buildGles2DqsPalette(skeleton, pose, palette) == GLES2_DQS_PALETTE_STATUS::READY &&
                   std::fabs(std::fabs(palette[10]) - 0.70710678118f) <= MATRIX_TOLERANCE &&
                   std::fabs(std::fabs(palette[11]) - 0.70710678118f) <= MATRIX_TOLERANCE,
               "GPU DQS palette must preserve a rigid 90-degree quaternion rotation");

        movedChild.scale = VEC3(2, 1, 1);
        pose.globalTransforms[1] = buildTrsMatrix(movedChild);
        expect(buildGles2DqsPalette(skeleton, pose, palette) ==
                   GLES2_DQS_PALETTE_STATUS::UNSUPPORTED_NON_RIGID_TRANSFORM && palette.empty(),
               "GPU rigid DQS palette must reject scale instead of silently discarding it");

        expect(sampleGles2DqsPalette(skeleton, clip, 0.5f, palette, &sampled) ==
                   GLES2_DQS_PALETTE_STATUS::READY && palette.size() == 16 &&
                   std::fabs(palette[12] - 1.0f) <= MATRIX_TOLERANCE &&
                   sampled.globalTransforms.size() == 2,
               "GPU DQS palette sampling must share canonical clip evaluation with LBS");
    }
}

int runSkeletalFoundationTests()
{
    failures = 0;
    testTrsRoundTrip();
    testCanonicalSkeletonCompilation();
    testUniformCanonicalAssetScale();
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
