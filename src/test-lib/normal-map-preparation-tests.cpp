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

#include "normal-map-preparation-tests.h"
#include <private/normal-map-preparation.h>
#include <private/normal-map-asset.h>
#include <private/skeletal-gpu-lbs.h>
#include <cmath>
#include <cstdio>
#include <limits>

namespace
{
    using namespace mbm;
    using namespace mbm::normal_map;
    int failures = 0;

    void expect(bool value, const char *message)
    {
        if (!value)
        {
            std::fprintf(stderr, "[normal-map-preparation] FAIL: %s\n", message);
            ++failures;
        }
    }

    bool near(float a, float b) { return std::abs(a-b) < 1.0e-5f; }

    INPUT quad()
    {
        INPUT in;
        in.positions = {VEC3(0,0,0), VEC3(1,0,0), VEC3(0,1,0), VEC3(1,1,0)};
        in.normals.assign(4, VEC3(0,0,1));
        in.uv = {VEC2(0,0), VEC2(1,0), VEC2(0,1), VEC2(1,1)};
        SUBSET_INPUT subset;
        subset.requested = true;
        subset.indices = {0,1,2,2,1,3};
        in.subsets.push_back(subset);
        return in;
    }

    bool prepareChecked(const INPUT &in, PREPARED &out)
    {
        std::string error;
        const bool result = prepare(in, out, error);
        if (!result)
            std::fprintf(stderr, "[normal-map-preparation] unexpected error: %s\n", error.c_str());
        expect(result, "preparation succeeds");
        return result;
    }

    void testOptional()
    {
        INPUT in;
        PREPARED out;
        out.batches.emplace_back();
        expect(prepareChecked(in, out) && out.batches.empty(), "empty optional input produces no basis");
        in = quad();
        in.subsets[0].requested = false;
        expect(prepareChecked(in, out) && out.batches.empty(), "no map/request produces no tangents");
        in.subsets[0].requested = true;
        in.normals.clear();
        expect(prepareChecked(in, out) && out.batches.empty(), "missing normals require no tangent basis");
        in = quad();
        in.uv.clear();
        expect(prepareChecked(in, out) && out.batches.empty(), "missing UV requires no tangent basis");
    }

    void testPlaneAndTopology()
    {
        INPUT in = quad();
        PREPARED out;
        if (!prepareChecked(in, out) || out.batches.size() != 1)
        {
            expect(false, "plane has one batch");
            return;
        }
        const BATCH &b = out.batches[0];
        expect(b.sourceVertices.size() == 4 && b.indices.size() == 6, "plane shares compatible corners");
        for (const TANGENT &t : b.tangents)
            expect(near(t.x,1) && near(t.y,0) && near(t.z,0) && t.sign == 1, "plane T=+X B=+Y");
        expect(out.unusableTriangles == 0, "plane has no unusable basis");
        for (size_t i = 0; i < b.indices.size(); ++i)
            expect(b.sourceVertices[b.indices[i]] == in.subsets[0].indices[i], "source corner order preserved");
        expect(in.positions.size() == 4 && in.subsets[0].indices[3] == 2, "source mesh remains unchanged");
        in.subsets[0].topology = TOPOLOGY::TRIANGLE_STRIP;
        in.subsets[0].indices = {0,1,2,3};
        if (prepareChecked(in, out) && out.batches.size() == 1)
        {
            const uint32_t expected[] = {0,1,2,2,1,3};
            for (size_t i = 0; i < 6; ++i)
                expect(out.batches[0].sourceVertices[out.batches[0].indices[i]] == expected[i], "strip winding preserved");
        }
        in.subsets[0].topology = TOPOLOGY::TRIANGLE_FAN;
        in.subsets[0].indices = {0,1,3,2};
        if (prepareChecked(in, out) && out.batches.size() == 1)
        {
            const uint32_t expected[] = {0,1,3,0,3,2};
            for (size_t i = 0; i < 6; ++i)
                expect(out.batches[0].sourceVertices[out.batches[0].indices[i]] == expected[i], "fan order preserved");
        }
    }

    void testMirroredSeamAndSubsets()
    {
        INPUT in = quad();
        // The diagonal shares source vertices but the second triangle reverses UV orientation.
        in.uv[3] = VEC2(0,0);
        PREPARED out;
        if (!prepareChecked(in, out) || out.batches.empty())
            return;
        expect(out.batches[0].sourceVertices.size() == 6, "mirrored seam splits shared source vertices");
        const BATCH &b = out.batches[0];
        expect(b.tangents[b.indices[0]].sign == 1 && b.tangents[b.indices[3]].sign == -1,
               "opposite UV orientation retains opposite bitangent signs");
        in = quad();
        SUBSET_INPUT second = in.subsets[0];
        in.subsets[0].requested = false;
        in.subsets.push_back(second);
        if (prepareChecked(in, out))
            expect(out.batches.size() == 1 && out.batches[0].subset == 1, "unrequested subset has no tangent batch");
        in.subsets[0].requested = true;
        if (prepareChecked(in, out))
            expect(out.batches.size() == 2 && out.batches[0].subset == 0 && out.batches[1].subset == 1,
                   "subset boundaries remain explicit");
    }

    void testCurvedNormals()
    {
        INPUT in = quad();
        in.normals = {VEC3(0,0,1), VEC3(0.6f,0,0.8f), VEC3(0,0.6f,0.8f), VEC3(0,0,1)};
        PREPARED out;
        if (!prepareChecked(in, out) || out.batches.empty())
            return;
        const BATCH &b = out.batches[0];
        expect(out.unusableTriangles == 0, "curved normals generate usable bases");
        for (size_t i = 0; i < b.tangents.size(); ++i)
        {
            const TANGENT &t = b.tangents[i];
            const VEC3 &n = in.normals[b.sourceVertices[i]];
            expect(std::abs(t.x*n.x+t.y*n.y+t.z*n.z) < 0.002f, "curved tangent is orthogonal to source normal");
        }
    }

    void testDegenerateAndInvalid()
    {
        INPUT in = quad();
        in.uv.assign(4, VEC2(0,0));
        PREPARED out;
        if (prepareChecked(in, out))
        {
            expect(out.unusableTriangles == 2, "degenerate UVs mark triangles unusable");
            for (const BATCH &b : out.batches)
                for (const TANGENT &t : b.tangents)
                    expect(t.sign == 0 && t.x == 0 && t.y == 0 && t.z == 0, "finite fallback sentinel");
        }
        in = quad();
        in.positions[1] = in.positions[0];
        if (prepareChecked(in, out))
            expect(out.unusableTriangles == 1, "zero-area geometry has a finite fallback");
        in = quad();
        in.normals[0] = VEC3(0,0,0);
        if (prepareChecked(in, out))
            expect(out.unusableTriangles == 1, "zero normal has a finite fallback");
        const auto rejected = [&out](const INPUT &bad, const char *label)
        {
            out.batches.clear(); out.batches.emplace_back(); out.batches[0].subset = 123;
            std::string error;
            expect(!prepare(bad, out, error) && !error.empty(), label);
            expect(out.batches.size() == 1 && out.batches[0].subset == 123, "failure does not publish partial output");
        };
        in = quad(); in.subsets[0].indices[0] = 99; rejected(in, "reject invalid source index");
        in = quad(); in.normals.pop_back(); rejected(in, "reject mismatched normal count");
        in = quad(); in.uv[0].x = std::numeric_limits<float>::quiet_NaN(); rejected(in, "reject non-finite UV");
        in = quad(); in.subsets[0].indices.pop_back(); rejected(in, "reject incomplete triangle");
        in = quad(); in.subsets[0].topology = static_cast<TOPOLOGY>(99); rejected(in, "reject unknown topology");
    }

    void testSkinRemapping()
    {
        using namespace mbm::skeletal;
        CANONICAL_BONE root, child;
        root.boneId = 10; root.name = "root";
        child.boneId = 20; child.parentBoneId = 10; child.name = "child";
        CANONICAL_SKELETON skeleton;
        skeleton.skeletonId = 100; skeleton.sourceBones = {root, child};
        if (!compileCanonicalSkeleton(skeleton.sourceBones, skeleton.compiled))
        { expect(false, "compile remap skeleton"); return; }
        CANONICAL_WEIGHTS weights;
        weights.skeletonId = 100;
        weights.paletteBoneIds = {20,10}; // Intentionally different from compiled bone order.
        INPUT in = quad();
        in.uv[3] = VEC2(0,0);
        weights.vertices.resize(4);
        for (uint32_t i = 0; i < 3; ++i)
        {
            weights.vertices[i].paletteIndex[0] = 0;
            weights.vertices[i].paletteIndex[1] = 1;
            weights.vertices[i].weight[0] = (i+1)*0.25f;
            weights.vertices[i].weight[1] = 1.0f-(i+1)*0.25f;
        }
        weights.vertices[3].paletteIndex[0] = 1;
        weights.vertices[3].weight[0] = 1.0f;
        PREPARED prepared;
        if (!prepareChecked(in, prepared)) return;
        std::string error;
        std::vector<CANONICAL_WEIGHTS> remapped;
        if (!remapSkinWeights(skeleton, weights, 0, 4, prepared, remapped, error))
        { expect(false, "remap mirrored seam weights"); return; }
        expect(remapped.size() == 1 && remapped[0].vertices.size() == 6,
               "skin influences duplicated at tangent seam");
        expect(weights.vertices.size() == 4 && in.subsets[0].indices == std::vector<uint32_t>({0,1,2,2,1,3}),
               "skin remap leaves authoring vertices and indices unchanged");
        const auto &batch = prepared.batches[0];
        const auto &mapped = remapped[0];
        expect(mapped.skeletonId == weights.skeletonId && mapped.frameIndex == 0 &&
               mapped.paletteBoneIds == weights.paletteBoneIds, "remap retains canonical identities");
        std::vector<VEC3> positions, normals;
        for (size_t i = 0; i < batch.sourceVertices.size(); ++i)
        {
            const uint32_t src = batch.sourceVertices[i];
            positions.push_back(in.positions[src]); normals.push_back(in.normals[src]);
            for (uint32_t slot = 0; slot < 4; ++slot)
                expect(mapped.vertices[i].paletteIndex[slot] == weights.vertices[src].paletteIndex[slot] &&
                       mapped.vertices[i].weight[slot] == weights.vertices[src].weight[slot],
                       "all four influence slots including unused sentinels preserved exactly");
        }
        SKELETAL_POSE pose;
        LOCAL_TRANSFORM a, b;
        a.translation = VEC3(1,2,3); a.rotation.x = std::sqrt(0.5f); a.rotation.w = std::sqrt(0.5f);
        b.translation = VEC3(-2,1,0); b.rotation.z = std::sqrt(0.5f); b.rotation.w = std::sqrt(0.5f);
        pose.globalTransforms = {buildTrsMatrix(a), buildTrsMatrix(b)};
        pose.localTransforms = {a,b};
        for (auto skin : {skinVerticesLbsReference, skinVerticesDqsRigidReference})
        {
            std::vector<VEC3> sourcePos, sourceNor, batchPos, batchNor;
            const bool ok = skin(skeleton, weights, pose, in.positions, in.normals, sourcePos, sourceNor) &&
                            skin(skeleton, mapped, pose, positions, normals, batchPos, batchNor);
            expect(ok, "LBS/DQS reference accepts remapped weights");
            if (!ok) continue;
            for (size_t i = 0; i < batchPos.size(); ++i)
            {
                const uint32_t src = batch.sourceVertices[i];
                expect(near(batchPos[i].x,sourcePos[src].x) && near(batchPos[i].y,sourcePos[src].y) &&
                       near(batchPos[i].z,sourcePos[src].z) && near(batchNor[i].x,sourceNor[src].x) &&
                       near(batchNor[i].y,sourceNor[src].y) && near(batchNor[i].z,sourceNor[src].z),
                       "deform then remap equals remap then deform at mirrored seam");
            }
        }
        GPU_SKINNING_INPUT sourceGpu, batchGpu;
        const auto capability = calculateSkinningCapability(128,8);
        expect(prepareGpuSkinningInput(skeleton,weights,capability,sourceGpu) == GPU_SKINNING_PREPARATION_STATUS::READY &&
               prepareGpuSkinningInput(skeleton,mapped,capability,batchGpu) == GPU_SKINNING_PREPARATION_STATUS::READY,
               "GPU attribute preparation accepts remapped weights");
        if (sourceGpu.ready() && batchGpu.ready())
            for (size_t i = 0; i < batchGpu.vertices.size(); ++i)
                for (uint32_t slot = 0; slot < 4; ++slot)
                    expect(batchGpu.vertices[i].boneIndex[slot] == sourceGpu.vertices[batch.sourceVertices[i]].boneIndex[slot] &&
                           batchGpu.vertices[i].weight[slot] == sourceGpu.vertices[batch.sourceVertices[i]].weight[slot],
                           "GPU palette resolution agrees with source vertices");
        const auto rejected = [&](const CANONICAL_WEIGHTS &badWeights, const PREPARED &badPrepared,
                                  uint32_t frame, uint32_t count)
        {
            remapped.resize(1); remapped[0].skeletonId = 999;
            expect(!remapSkinWeights(skeleton,badWeights,frame,count,badPrepared,remapped,error) && !error.empty(),
                   "reject invalid remap input");
            expect(remapped.size() == 1 && remapped[0].skeletonId == 999, "failed remap is atomic");
        };
        rejected(weights,prepared,1,4); rejected(weights,prepared,0,3);
        auto bad = prepared; bad.batches.push_back(batch); bad.batches.back().sourceVertices[0] = 4;
        rejected(weights,bad,0,4);
        bad = prepared; bad.batches[0].indices[0] = 99; rejected(weights,bad,0,4);
        auto badWeights = weights; badWeights.vertices[3] = {}; rejected(badWeights,prepared,0,4);
        badWeights = weights; badWeights.vertices[0].weight[0] = std::numeric_limits<float>::quiet_NaN();
        rejected(badWeights,prepared,0,4);
        expect(remapSkinWeights({}, {}, 0, 0, {}, remapped,error) && remapped.empty(),
               "no prepared batches require no skeleton or weight allocation");
    }

    void testImportedAndPartition()
    {
        INPUT in = quad();
        in.policy = TANGENT_POLICY::IMPORT;
        // Explicitly rotated tangents are not silently regenerated from the quad UVs.
        in.subsets[0].importedCorners.assign(6, TANGENT{0,1,0,-1});
        PREPARED out;
        if (prepareChecked(in, out))
            for (const TANGENT &t : out.batches[0].tangents)
                expect(t.x == 0 && t.y == 1 && t.z == 0 && t.sign == -1, "imported tangent preserved");
        std::string error;
        in.subsets[0].importedCorners[0] = TANGENT{0,0,1,1};
        expect(!prepare(in, out, error), "reject imported tangent parallel to normal");
        in.subsets[0].importedCorners[0] = TANGENT{2,0,0,1};
        expect(!prepare(in, out, error), "reject non-unit imported tangent");
        in.subsets[0].importedCorners[0] = TANGENT{1,0,0,0};
        expect(!prepare(in, out, error), "reject invalid imported sign");
        in.subsets[0].importedCorners.assign(4, TANGENT{1,0,0,1});
        expect(!prepare(in, out, error), "reject per-source-vertex import in place of per-corner data");
        in = INPUT{}; in.policy = TANGENT_POLICY::IMPORT;
        in.subsets.emplace_back(); in.subsets[0].requested = true;
        for (uint32_t i = 0; i < 22000; ++i)
        {
            in.positions.insert(in.positions.end(), {VEC3(0,0,0), VEC3(1,0,0), VEC3(0,1,0)});
            in.normals.insert(in.normals.end(), 3, VEC3(0,0,1));
            in.uv.insert(in.uv.end(), {VEC2(0,0), VEC2(1,0), VEC2(0,1)});
            in.subsets[0].indices.insert(in.subsets[0].indices.end(), {i*3,i*3+1,i*3+2});
            in.subsets[0].importedCorners.insert(in.subsets[0].importedCorners.end(), 3, TANGENT{1,0,0,1});
        }
        if (!prepareChecked(in, out))
            return;
        expect(out.batches.size() == 2, "partition over 65536 vertices into 16-bit batches");
        uint32_t corner = 0;
        for (const BATCH &b : out.batches)
        {
            expect(b.sourceVertices.size() <= 65536 && b.indices.size()%3 == 0, "batch fits without splitting triangles");
            for (uint16_t index : b.indices)
            {
                expect(index < b.sourceVertices.size(), "batch index in range");
                expect(b.sourceVertices[index] == corner++, "partition preserves all source corners and draw order");
            }
        }
        expect(corner == 66000, "partition loses no geometry");
        skeletal::CANONICAL_BONE root;
        root.boneId = 10; root.name = "root";
        skeletal::CANONICAL_SKELETON skeleton;
        skeleton.skeletonId = 100; skeleton.sourceBones = {root};
        expect(skeletal::compileCanonicalSkeleton(skeleton.sourceBones,skeleton.compiled), "compile partition skeleton");
        skeletal::CANONICAL_WEIGHTS weights;
        weights.skeletonId = 100; weights.paletteBoneIds = {10};
        skeletal::CANONICAL_VERTEX_WEIGHT influence;
        influence.paletteIndex[0] = 0; influence.weight[0] = 1;
        weights.vertices.assign(66000,influence);
        std::vector<skeletal::CANONICAL_WEIGHTS> remapped;
        expect(remapSkinWeights(skeleton,weights,0,66000,out,remapped,error), "remap skin across 16-bit partitions");
        size_t vertices = 0;
        for (size_t i = 0; i < remapped.size(); ++i)
        {
            expect(remapped[i].vertices.size() == out.batches[i].sourceVertices.size(), "partition skin stream matches geometry");
            vertices += remapped[i].vertices.size();
        }
        expect(remapped.size() == 2 && vertices == 66000, "partition loses no skin influences");
    }
}

int runNormalMapPreparationTests()
{
    failures = 0;
    testOptional();
    testPlaneAndTopology();
    testMirroredSeamAndSubsets();
    testCurvedNormals();
    testDegenerateAndInvalid();
    testImportedAndPartition();
    testSkinRemapping();
    std::printf("[normal-map-preparation] %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
