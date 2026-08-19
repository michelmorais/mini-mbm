/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------*/

#include "skeletal-parity-tests.h"

#include <skeletal-animation-foundation.h>
#include <skeletal-parity-asset.h>
#include <skeletal-render-capability.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/util-interface.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace mbm::skeletal::test
{
    namespace
    {
        constexpr float NORMAL_TOLERANCE = (2.0f / 255.0f) + 0.002f;

        float maxDifference(const VEC3 &a, const VEC3 &b) noexcept
        {
            return std::max({std::fabs(a.x - b.x), std::fabs(a.y - b.y),
                             std::fabs(a.z - b.z)});
        }

        const char *methodName(const SKELETAL_SHADER_METHOD method) noexcept
        {
            return method == SKELETAL_SHADER_METHOD::DQS_RIGID ? "dqs" : "lbs";
        }

        bool validateCase(const SKELETAL_PARITY_CASE &testCase, std::string &error)
        {
            const size_t sampleCount = testCase.positions.size();
            const size_t floatsPerBone = testCase.method == SKELETAL_SHADER_METHOD::DQS_RIGID ? 8u : 12u;
            if (sampleCount == 0 || testCase.paletteSize == 0)
            {
                error = "skeletal parity case has no samples or palette";
                return false;
            }
            if (testCase.normals.size() != sampleCount || testCase.influences.size() != sampleCount ||
                testCase.expectedPositions.size() != sampleCount ||
                testCase.expectedNormals.size() != sampleCount ||
                testCase.palette.size() != static_cast<size_t>(testCase.paletteSize) * floatsPerBone)
            {
                error = "skeletal parity case arrays or palette have inconsistent sizes";
                return false;
            }
            return true;
        }

        std::vector<uint32_t> selectMixedInfluenceSamples(const CANONICAL_WEIGHTS &weights,
                                                           const size_t maximumSamples)
        {
            struct CANDIDATE
            {
                uint32_t index;
                float secondWeight;
            };
            std::vector<CANDIDATE> candidates;
            for (uint32_t i = 0; i < weights.vertices.size(); ++i)
            {
                float first = 0.0f;
                float second = 0.0f;
                for (const float weight : weights.vertices[i].weight)
                {
                    if (weight > first)
                    {
                        second = first;
                        first = weight;
                    }
                    else if (weight > second)
                    {
                        second = weight;
                    }
                }
                if (second > MATRIX_TOLERANCE)
                    candidates.push_back({i, second});
            }
            std::sort(candidates.begin(), candidates.end(), [](const CANDIDATE &a, const CANDIDATE &b)
            {
                if (a.secondWeight != b.secondWeight)
                    return a.secondWeight > b.secondWeight;
                return a.index < b.index;
            });
            std::vector<uint32_t> selected;
            for (size_t i = 0; i < std::min(maximumSamples, candidates.size()); ++i)
                selected.push_back(candidates[i].index);
            std::sort(selected.begin(), selected.end());
            return selected;
        }

        bool appendSyntheticCases(std::vector<SKELETAL_PARITY_CASE> &cases, std::string &error)
        {
            CANONICAL_BONE root;
            root.boneId = 10;
            root.name = "root";
            CANONICAL_BONE child;
            child.boneId = 20;
            child.parentBoneId = 10;
            child.name = "child";
            child.localBind.translation = VEC3(1.0f, 0.0f, 0.0f);
            CANONICAL_SKELETON skeleton;
            skeleton.skeletonId = 100;
            skeleton.sourceBones = {root, child};
            if (!compileCanonicalSkeleton(skeleton.sourceBones, skeleton.compiled))
            {
                error = "synthetic skeletal parity skeleton did not compile";
                return false;
            }

            CANONICAL_WEIGHTS weights;
            weights.skeletonId = 100;
            weights.paletteBoneIds = {10, 20};
            weights.vertices.resize(2);
            weights.vertices[0].paletteIndex[0] = 0;
            weights.vertices[0].paletteIndex[1] = 1;
            weights.vertices[0].weight[0] = 0.25f;
            weights.vertices[0].weight[1] = 0.75f;
            weights.vertices[1].paletteIndex[0] = 0;
            weights.vertices[1].paletteIndex[1] = 1;
            weights.vertices[1].weight[0] = 0.60f;
            weights.vertices[1].weight[1] = 0.40f;

            const std::vector<VEC3> positions = {
                VEC3(1.5f, 0.2f, 0.0f), VEC3(0.4f, 0.8f, 0.2f)};
            const std::vector<VEC3> normals = {
                VEC3(0.0f, 1.0f, 0.0f), VEC3(0.0f, 0.0f, 1.0f)};
            LOCAL_TRANSFORM posedRoot = root.localBind;
            LOCAL_TRANSFORM posedChild = child.localBind;
            posedRoot.rotation = {0, 0, 0.2588190451f, 0.9659258263f};
            posedChild.rotation = {0, 0, -0.3420201433f, 0.9396926208f};
            SKELETAL_POSE pose;
            pose.localTransforms = {posedRoot, posedChild};
            const MATRIX rootGlobal = buildTrsMatrix(posedRoot);
            const MATRIX childLocal = buildTrsMatrix(posedChild);
            MATRIX childGlobal;
            MatrixMultiply(&childGlobal, &childLocal, &rootGlobal);
            pose.globalTransforms = {rootGlobal, childGlobal};

            std::vector<GPU_LBS_VERTEX> influences(2);
            for (size_t vertex = 0; vertex < influences.size(); ++vertex)
            {
                for (size_t influence = 0; influence < 4; ++influence)
                {
                    influences[vertex].boneIndex[influence] =
                        static_cast<float>(weights.vertices[vertex].paletteIndex[influence]);
                    influences[vertex].weight[influence] = weights.vertices[vertex].weight[influence];
                }
            }

            SKELETAL_PARITY_CASE lbs;
            lbs.fixtureName = "synthetic";
            lbs.method = SKELETAL_SHADER_METHOD::LBS;
            lbs.paletteSize = 2;
            lbs.positions = positions;
            lbs.normals = normals;
            lbs.influences = influences;
            lbs.sourceVertexIndices = {0, 1};
            if (!skinVerticesLbsReference(skeleton, weights, pose, positions, normals,
                                          lbs.expectedPositions, lbs.expectedNormals) ||
                buildLbsPalette(skeleton, pose, true, lbs.palette) != LBS_PALETTE_STATUS::READY)
            {
                error = "synthetic LBS parity reference could not be built";
                return false;
            }
            cases.push_back(std::move(lbs));

            SKELETAL_PARITY_CASE dqs;
            dqs.fixtureName = "synthetic";
            dqs.method = SKELETAL_SHADER_METHOD::DQS_RIGID;
            dqs.paletteSize = 2;
            dqs.positions = positions;
            dqs.normals = normals;
            dqs.influences = influences;
            dqs.sourceVertexIndices = {0, 1};
            if (!skinVerticesDqsRigidReference(skeleton, weights, pose, positions, normals,
                                               dqs.expectedPositions, dqs.expectedNormals) ||
                buildDqsPalette(skeleton, pose, dqs.palette) != DQS_PALETTE_STATUS::READY)
            {
                error = "synthetic DQS parity reference could not be built";
                return false;
            }
            cases.push_back(std::move(dqs));
            return true;
        }

        bool appendRealAssetCases(std::vector<SKELETAL_PARITY_CASE> &cases, std::string &error)
        {
            MESH_MBM_DEBUG mesh;
            if (!mesh.loadV11("Lorekeeper-walk.msh"))
            {
                error = "Lorekeeper-walk.msh could not be loaded for skeletal parity";
                return false;
            }
            CANONICAL_PARITY_ASSET asset;
            if (!copyCanonicalParityAsset(mesh, asset) || asset.animations.clips.empty())
            {
                error = "Lorekeeper canonical parity data or clips are unavailable";
                return false;
            }
            const std::vector<uint32_t> selected = selectMixedInfluenceSamples(asset.weights, 8);
            const VEC3 *allPositions = mesh.getPositionArray(asset.weights.frameIndex);
            const VEC3 *allNormals = mesh.getNormalArray(asset.weights.frameIndex);
            if (selected.size() != 8 || !allPositions || !allNormals)
            {
                error = "Lorekeeper parity sample selection is incomplete";
                return false;
            }

            SKINNING_CAPABILITY capability;
            capability.measured = true;
            capability.hasRequiredVertexAttributes = true;
            capability.lbsMatrixPaletteBones = std::numeric_limits<uint32_t>::max();
            capability.dqsRigidPaletteBones = std::numeric_limits<uint32_t>::max();
            GPU_SKINNING_INPUT prepared;
            if (prepareGpuSkinningInput(asset.skeleton, asset.weights, capability, prepared) !=
                GPU_SKINNING_PREPARATION_STATUS::READY)
            {
                error = "Lorekeeper GPU influence input could not be prepared";
                return false;
            }

            CANONICAL_WEIGHTS subsetWeights;
            subsetWeights.skeletonId = asset.weights.skeletonId;
            subsetWeights.paletteBoneIds = asset.weights.paletteBoneIds;
            std::vector<VEC3> positions;
            std::vector<VEC3> normals;
            std::vector<GPU_LBS_VERTEX> influences;
            for (const uint32_t index : selected)
            {
                subsetWeights.vertices.push_back(asset.weights.vertices[index]);
                positions.push_back(allPositions[index]);
                normals.push_back(allNormals[index]);
                influences.push_back(prepared.vertices[index]);
            }

            const SKELETAL_CLIP &clip = asset.animations.clips.front();
            const float sampleTime = clip.duration * 0.37f;
            SKELETAL_POSE pose;
            if (!sampleSkeletalClip(asset.skeleton.compiled, clip, sampleTime, pose))
            {
                error = "Lorekeeper parity clip could not be sampled";
                return false;
            }

            SKELETAL_PARITY_CASE lbs;
            lbs.fixtureName = "Lorekeeper";
            lbs.method = SKELETAL_SHADER_METHOD::LBS;
            lbs.paletteSize = static_cast<uint32_t>(asset.skeleton.compiled.bones.size());
            lbs.positions = positions;
            lbs.normals = normals;
            lbs.influences = influences;
            lbs.sourceVertexIndices = selected;
            if (!skinVerticesLbsReference(asset.skeleton, subsetWeights, pose, positions, normals,
                                          lbs.expectedPositions, lbs.expectedNormals) ||
                buildLbsPalette(asset.skeleton, pose, true, lbs.palette) != LBS_PALETTE_STATUS::READY)
            {
                error = "Lorekeeper LBS parity reference could not be built";
                return false;
            }
            cases.push_back(std::move(lbs));

            SKELETAL_PARITY_CASE dqs;
            dqs.fixtureName = "Lorekeeper";
            dqs.method = SKELETAL_SHADER_METHOD::DQS_RIGID;
            dqs.paletteSize = static_cast<uint32_t>(asset.skeleton.compiled.bones.size());
            dqs.positions = positions;
            dqs.normals = normals;
            dqs.influences = influences;
            dqs.sourceVertexIndices = selected;
            if (!skinVerticesDqsRigidReference(asset.skeleton, subsetWeights, pose, positions, normals,
                                               dqs.expectedPositions, dqs.expectedNormals) ||
                buildDqsPalette(asset.skeleton, pose, dqs.palette) != DQS_PALETTE_STATUS::READY)
            {
                error = "Lorekeeper DQS parity reference could not be built";
                return false;
            }
            cases.push_back(std::move(dqs));

            INFO_LOG("skeletal parity fixture: fixture=Lorekeeper clip=%s time=%.7f "
                     "vertices=%u,%u,%u,%u,%u,%u,%u,%u",
                     clip.name.c_str(), sampleTime, selected[0], selected[1], selected[2], selected[3],
                     selected[4], selected[5], selected[6], selected[7]);
            return true;
        }

        SKELETAL_PARITY_ENCODING buildEncoding(const SKELETAL_PARITY_CASE &testCase)
        {
            VEC3 minimum = testCase.expectedPositions[0];
            VEC3 maximum = testCase.expectedPositions[0];
            for (const VEC3 &position : testCase.expectedPositions)
            {
                minimum.x = std::min(minimum.x, position.x);
                minimum.y = std::min(minimum.y, position.y);
                minimum.z = std::min(minimum.z, position.z);
                maximum.x = std::max(maximum.x, position.x);
                maximum.y = std::max(maximum.y, position.y);
                maximum.z = std::max(maximum.z, position.z);
            }
            SKELETAL_PARITY_ENCODING encoding;
            encoding.positionCenter = VEC3((minimum.x + maximum.x) * 0.5f,
                                           (minimum.y + maximum.y) * 0.5f,
                                           (minimum.z + maximum.z) * 0.5f);
            encoding.positionExtent = std::max({maximum.x - minimum.x, maximum.y - minimum.y,
                                                maximum.z - minimum.z, 0.01f}) * 0.55f;
            encoding.positionTolerance = (encoding.positionExtent * 2.0f / 255.0f) +
                std::max(0.002f, encoding.positionExtent * 0.0001f);
            encoding.normalTolerance = NORMAL_TOLERANCE;
            return encoding;
        }

        bool decodePass(const std::vector<uint8_t> &pixels, const size_t sampleCount,
                        const VEC3 &offset, const float scale, std::vector<VEC3> &values)
        {
            if (pixels.size() != sampleCount * 4)
                return false;
            values.resize(sampleCount);
            for (size_t i = 0; i < sampleCount; ++i)
            {
                values[i] = VEC3(pixels[i * 4] / 255.0f * scale + offset.x,
                                 pixels[i * 4 + 1] / 255.0f * scale + offset.y,
                                 pixels[i * 4 + 2] / 255.0f * scale + offset.z);
            }
            return true;
        }

        bool compareAndReport(const char *backendName, const SKELETAL_PARITY_CASE &testCase,
                              const SKELETAL_PARITY_ENCODING &encoding,
                              const std::vector<uint8_t> &positionPixels,
                              const std::vector<uint8_t> &normalPixels,
                              std::string &error)
        {
            const size_t sampleCount = testCase.positions.size();
            std::vector<VEC3> actualPositions;
            std::vector<VEC3> actualNormals;
            const VEC3 positionOffset(encoding.positionCenter.x - encoding.positionExtent,
                                      encoding.positionCenter.y - encoding.positionExtent,
                                      encoding.positionCenter.z - encoding.positionExtent);
            if (!decodePass(positionPixels, sampleCount, positionOffset,
                            encoding.positionExtent * 2.0f, actualPositions) ||
                !decodePass(normalPixels, sampleCount, VEC3(-1.0f, -1.0f, -1.0f),
                            2.0f, actualNormals))
            {
                error = "skeletal parity capture returned an invalid RGBA8 sample count";
                return false;
            }

            float maxPositionError = 0.0f;
            float maxNormalError = 0.0f;
            size_t worstPosition = 0;
            size_t worstNormal = 0;
            for (size_t i = 0; i < sampleCount; ++i)
            {
                const float positionError =
                    maxDifference(actualPositions[i], testCase.expectedPositions[i]);
                const float normalError = maxDifference(actualNormals[i], testCase.expectedNormals[i]);
                if (positionError > maxPositionError)
                {
                    maxPositionError = positionError;
                    worstPosition = i;
                }
                if (normalError > maxNormalError)
                {
                    maxNormalError = normalError;
                    worstNormal = i;
                }
            }
            const bool passed = maxPositionError <= encoding.positionTolerance &&
                maxNormalError <= encoding.normalTolerance;
            INFO_LOG("skeletal GPU parity: backend=%s fixture=%s method=%s samples=%u "
                     "max-position-error=%.7f vertex=%u tolerance=%.7f "
                     "max-normal-error=%.7f vertex=%u tolerance=%.7f %s",
                     backendName, testCase.fixtureName.c_str(), methodName(testCase.method),
                     static_cast<unsigned>(sampleCount), maxPositionError,
                     static_cast<unsigned>(worstPosition), encoding.positionTolerance,
                     maxNormalError, static_cast<unsigned>(worstNormal), encoding.normalTolerance,
                     passed ? "PASS" : "FAIL");
            if (!passed)
                error = "skeletal GPU capture differs from the CPU reference";
            return passed;
        }
    }

    bool buildSkeletalParityCases(std::vector<SKELETAL_PARITY_CASE> &cases, std::string &error)
    {
        cases.clear();
        error.clear();
        util::addPath(__FILE__);
        if (!appendSyntheticCases(cases, error) || !appendRealAssetCases(cases, error))
        {
            cases.clear();
            return false;
        }
        for (const SKELETAL_PARITY_CASE &testCase : cases)
        {
            if (!validateCase(testCase, error))
            {
                cases.clear();
                return false;
            }
        }
        return true;
    }

    bool runSkeletalParitySuite(const char *backendName,
                                const SKELETAL_PARITY_CAPTURE_RGBA8 capture,
                                std::string &error)
    {
        if (!backendName || !backendName[0] || !capture)
        {
            error = "skeletal parity suite requires a backend name and capture callback";
            return false;
        }
        std::vector<SKELETAL_PARITY_CASE> cases;
        if (!buildSkeletalParityCases(cases, error))
            return false;
        for (const SKELETAL_PARITY_CASE &testCase : cases)
        {
            const SKELETAL_PARITY_ENCODING encoding = buildEncoding(testCase);
            std::vector<uint8_t> positionPixels;
            std::vector<uint8_t> normalPixels;
            if (!capture(testCase, encoding, positionPixels, normalPixels, error) ||
                !compareAndReport(backendName, testCase, encoding,
                                  positionPixels, normalPixels, error))
                return false;
        }
        INFO_LOG("skeletal GPU parity suite: backend=%s cases=%u PASS",
                 backendName, static_cast<unsigned>(cases.size()));
        return true;
    }
}
