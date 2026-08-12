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

#include "gles-skeletal-parity-tests.h"

#include <skeletal-animation-foundation.h>
#include <skeletal-gpu-lbs.h>
#include <skeletal-gles-shader-source.h>
#include <skeletal-parity-asset.h>
#include <skeletal-render-capability.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/util-interface.h>
#include <GLES2/gl2.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace
{
    using namespace mbm;
    using namespace mbm::skeletal;

    constexpr float NORMAL_TOLERANCE = (2.0f / 255.0f) + 0.002f;

    GLuint compileShader(const GLenum type, const char *source)
    {
        const GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);
        GLint ok = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (ok == GL_TRUE)
            return shader;
        char log[2048] = {};
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        ERROR_LOG("skeletal GPU parity shader compile failed: %s", log);
        glDeleteShader(shader);
        return 0;
    }

    GLuint buildProgram(const bool dqs, const uint32_t paletteSize)
    {
        std::string vs =
            "attribute vec4 aPosition; attribute vec3 aNormal; attribute vec4 aBoneIndices;"
            "attribute vec4 aBoneWeights; attribute float aSample;";
        const SKELETAL_SHADER_METHOD method = dqs ? SKELETAL_SHADER_METHOD::DQS_RIGID
                                                  : SKELETAL_SHADER_METHOD::LBS;
        appendGlesSkeletalFunctions(vs, paletteSize, method);
        vs += "varying vec3 vPosition;varying vec3 vNormal;void main(){";
        appendGlesSkeletalDeformation(vs, method, true);
        vs += "vPosition=skinnedPosition.xyz;vNormal=skinnedNormal;"
            "gl_Position=vec4(aSample,0.0,0.0,1.0);gl_PointSize=1.0;}";
        const char *fs =
            "precision highp float;varying vec3 vPosition;varying vec3 vNormal;uniform float outputNormal;"
            "uniform vec3 encodeCenter;uniform float encodeExtent;"
            "void main(){vec3 value=outputNormal>0.5?vNormal*0.5+0.5:"
            "(vPosition-encodeCenter)/(2.0*encodeExtent)+0.5;"
            "gl_FragColor=vec4(clamp(value,0.0,1.0),1.0);}";
        const GLuint vertex = compileShader(GL_VERTEX_SHADER, vs.c_str());
        const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fs);
        if (!vertex || !fragment)
            return 0;
        const GLuint program = glCreateProgram();
        glAttachShader(program, vertex);
        glAttachShader(program, fragment);
        glLinkProgram(program);
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        GLint ok = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &ok);
        if (ok == GL_TRUE)
            return program;
        char log[2048] = {};
        glGetProgramInfoLog(program, sizeof(log), nullptr, log);
        ERROR_LOG("skeletal GPU parity program link failed: %s", log);
        glDeleteProgram(program);
        return 0;
    }

    float maxDifference(const VEC3 &a, const VEC3 &b)
    {
        return std::max({std::fabs(a.x - b.x), std::fabs(a.y - b.y), std::fabs(a.z - b.z)});
    }

    bool runMethod(const char *fixtureName, bool dqs, uint32_t paletteSize,
                   const std::vector<float> &palette, const std::vector<VEC3> &positions,
                   const std::vector<VEC3> &normals, const std::vector<GPU_LBS_VERTEX> &gpuWeights,
                   const std::vector<VEC3> &cpuPositions, const std::vector<VEC3> &cpuNormals);

    std::vector<uint32_t> selectMixedInfluenceSamples(const CANONICAL_WEIGHTS &weights,
                                                       const size_t maximumSamples)
    {
        struct CANDIDATE { uint32_t index; float secondWeight; };
        std::vector<CANDIDATE> candidates;
        for (uint32_t i = 0; i < weights.vertices.size(); ++i)
        {
            float first = 0.0f, second = 0.0f;
            for (float weight : weights.vertices[i].weight)
            {
                if (weight > first) { second = first; first = weight; }
                else if (weight > second) second = weight;
            }
            if (second > MATRIX_TOLERANCE)
                candidates.push_back({i, second});
        }
        std::sort(candidates.begin(), candidates.end(), [](const CANDIDATE &a, const CANDIDATE &b)
        {
            if (a.secondWeight != b.secondWeight) return a.secondWeight > b.secondWeight;
            return a.index < b.index;
        });
        std::vector<uint32_t> selected;
        for (size_t i = 0; i < std::min(maximumSamples, candidates.size()); ++i)
            selected.push_back(candidates[i].index);
        std::sort(selected.begin(), selected.end());
        return selected;
    }

    bool runRealAssetParity()
    {
        MESH_MBM_DEBUG mesh;
        if (!mesh.loadV11("src/test-lib/Lorekeeper-walk.msh"))
            return false;
        CANONICAL_PARITY_ASSET asset;
        if (!copyCanonicalParityAsset(mesh, asset) || asset.animations.clips.empty())
            return false;
        const std::vector<uint32_t> selected = selectMixedInfluenceSamples(asset.weights, 8);
        const VEC3 *allPositions = mesh.getPositionArray(asset.weights.frameIndex);
        const VEC3 *allNormals = mesh.getNormalArray(asset.weights.frameIndex);
        if (selected.size() != 8 || !allPositions || !allNormals)
            return false;

        GLES2_SKINNING_CAPABILITY capability;
        capability.measured = true; capability.hasRequiredVertexAttributes = true;
        capability.lbsMatrixPaletteBones = UINT32_MAX; capability.dqsRigidPaletteBones = UINT32_MAX;
        GLES2_LBS_INPUT prepared;
        if (prepareGles2LbsInput(asset.skeleton, asset.weights, capability, prepared) !=
            GLES2_LBS_PREPARATION_STATUS::READY)
            return false;

        CANONICAL_WEIGHTS subsetWeights;
        subsetWeights.skeletonId = asset.weights.skeletonId;
        subsetWeights.paletteBoneIds = asset.weights.paletteBoneIds;
        std::vector<VEC3> positions, normals;
        std::vector<GPU_LBS_VERTEX> gpuWeights;
        for (const uint32_t index : selected)
        {
            subsetWeights.vertices.push_back(asset.weights.vertices[index]);
            positions.push_back(allPositions[index]); normals.push_back(allNormals[index]);
            gpuWeights.push_back(prepared.vertices[index]);
        }
        const SKELETAL_CLIP &clip = asset.animations.clips.front();
        const float sampleTime = clip.duration * 0.37f;
        SKELETAL_POSE pose;
        if (!sampleSkeletalClip(asset.skeleton.compiled, clip, sampleTime, pose))
            return false;
        std::vector<VEC3> cpuPositions, cpuNormals;
        std::vector<float> palette;
        const uint32_t paletteSize = static_cast<uint32_t>(asset.skeleton.compiled.bones.size());
        if (!skinVerticesLbsReference(asset.skeleton, subsetWeights, pose, positions, normals,
                                      cpuPositions, cpuNormals) ||
            buildGles2LbsPalette(asset.skeleton, pose, true, palette) != GLES2_LBS_PALETTE_STATUS::READY ||
            !runMethod("Lorekeeper", false, paletteSize, palette, positions, normals, gpuWeights,
                       cpuPositions, cpuNormals)) return false;
        if (!skinVerticesDqsRigidReference(asset.skeleton, subsetWeights, pose, positions, normals,
                                           cpuPositions, cpuNormals) ||
            buildGles2DqsPalette(asset.skeleton, pose, palette) != GLES2_DQS_PALETTE_STATUS::READY ||
            !runMethod("Lorekeeper", true, paletteSize, palette, positions, normals, gpuWeights,
                       cpuPositions, cpuNormals)) return false;
        INFO_LOG("skeletal GPU parity Lorekeeper selection: clip=%s time=%.7f vertices=%u,%u,%u,%u,%u,%u,%u,%u",
                 clip.name.c_str(), sampleTime, selected[0], selected[1], selected[2], selected[3],
                 selected[4], selected[5], selected[6], selected[7]);
        return true;
    }

    bool runMethod(const char *fixtureName, const bool dqs, const uint32_t paletteSize,
                   const std::vector<float> &palette,
                   const std::vector<VEC3> &positions, const std::vector<VEC3> &normals,
                   const std::vector<GPU_LBS_VERTEX> &gpuWeights, const std::vector<VEC3> &cpuPositions,
                   const std::vector<VEC3> &cpuNormals)
    {
        const size_t sampleCount = positions.size();
        if (sampleCount == 0 || normals.size() != sampleCount || gpuWeights.size() != sampleCount ||
            cpuPositions.size() != sampleCount || cpuNormals.size() != sampleCount)
            return false;
        const GLuint program = buildProgram(dqs, paletteSize);
        if (!program || palette.size() != paletteSize * (dqs ? 8u : 12u))
            return false;
        GLuint texture = 0, framebuffer = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(sampleCount), 1, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            ERROR_LOG("skeletal GPU parity framebuffer is incomplete");
            glDeleteFramebuffers(1, &framebuffer); glDeleteTextures(1, &texture); glDeleteProgram(program);
            return false;
        }

        std::vector<float> boneIndices(sampleCount * 4), boneWeights(sampleCount * 4), samples(sampleCount);
        for (size_t vertex = 0; vertex < sampleCount; ++vertex)
        {
            for (size_t influence = 0; influence < 4; ++influence)
            {
                boneIndices[vertex * 4 + influence] = gpuWeights[vertex].boneIndex[influence];
                boneWeights[vertex * 4 + influence] = gpuWeights[vertex].weight[influence];
            }
            samples[vertex] = ((static_cast<float>(vertex) + 0.5f) /
                               static_cast<float>(sampleCount)) * 2.0f - 1.0f;
        }
        VEC3 minimum = cpuPositions[0], maximum = cpuPositions[0];
        for (const VEC3 &position : cpuPositions)
        {
            minimum.x = std::min(minimum.x, position.x); minimum.y = std::min(minimum.y, position.y);
            minimum.z = std::min(minimum.z, position.z); maximum.x = std::max(maximum.x, position.x);
            maximum.y = std::max(maximum.y, position.y); maximum.z = std::max(maximum.z, position.z);
        }
        const VEC3 center((minimum.x + maximum.x) * 0.5f, (minimum.y + maximum.y) * 0.5f,
                          (minimum.z + maximum.z) * 0.5f);
        const float extent = std::max({maximum.x - minimum.x, maximum.y - minimum.y,
                                      maximum.z - minimum.z, 0.01f}) * 0.55f;
        const float positionTolerance = (extent * 2.0f / 255.0f) + std::max(0.002f, extent * 0.0001f);
        glUseProgram(program);
        const auto bind = [program](const char *name, GLint size, const void *data)
        {
            const GLint location = glGetAttribLocation(program, name);
            glEnableVertexAttribArray(location);
            glVertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, 0, data);
        };
        bind("aPosition", 3, positions.data()); bind("aNormal", 3, normals.data());
        bind("aBoneIndices", 4, boneIndices.data()); bind("aBoneWeights", 4, boneWeights.data());
        bind("aSample", 1, samples.data());
        glUniform4fv(glGetUniformLocation(program, "bonePalette"),
                     static_cast<GLsizei>(paletteSize * (dqs ? 2u : 3u)), palette.data());
        glUniform3f(glGetUniformLocation(program, "encodeCenter"), center.x, center.y, center.z);
        glUniform1f(glGetUniformLocation(program, "encodeExtent"), extent);
        glViewport(0, 0, static_cast<GLsizei>(sampleCount), 1);
        std::vector<unsigned char> pixels(sampleCount * 4);
        float maxPositionError = 0.0f, maxNormalError = 0.0f;
        size_t worstPosition = 0, worstNormal = 0;
        for (int normalPass = 0; normalPass < 2; ++normalPass)
        {
            glUniform1f(glGetUniformLocation(program, "outputNormal"), static_cast<float>(normalPass));
            glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(sampleCount));
            glReadPixels(0, 0, static_cast<GLsizei>(sampleCount), 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            for (size_t i = 0; i < sampleCount; ++i)
            {
                const float scale = normalPass ? 2.0f : extent * 2.0f;
                const VEC3 offset = normalPass ? VEC3(-1, -1, -1) :
                    VEC3(center.x - extent, center.y - extent, center.z - extent);
                const VEC3 gpu(pixels[i * 4] / 255.0f * scale + offset.x,
                               pixels[i * 4 + 1] / 255.0f * scale + offset.y,
                               pixels[i * 4 + 2] / 255.0f * scale + offset.z);
                const float error = maxDifference(gpu, normalPass ? cpuNormals[i] : cpuPositions[i]);
                if (normalPass && error > maxNormalError) { maxNormalError = error; worstNormal = i; }
                if (!normalPass && error > maxPositionError) { maxPositionError = error; worstPosition = i; }
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &framebuffer); glDeleteTextures(1, &texture); glDeleteProgram(program);
        INFO_LOG("skeletal GPU parity: fixture=%s method=%s samples=%u max-position-error=%.7f vertex=%u tolerance=%.7f "
                 "max-normal-error=%.7f vertex=%u tolerance=%.7f %s",
                 fixtureName, dqs ? "dqs" : "lbs", static_cast<unsigned>(sampleCount), maxPositionError,
                 static_cast<unsigned>(worstPosition), positionTolerance,
                 maxNormalError, static_cast<unsigned>(worstNormal), NORMAL_TOLERANCE,
                 maxPositionError <= positionTolerance && maxNormalError <= NORMAL_TOLERANCE ? "PASS" : "FAIL");
        return maxPositionError <= positionTolerance && maxNormalError <= NORMAL_TOLERANCE;
    }
}

bool runGlesSkeletalParityTests()
{
    CANONICAL_BONE root, child;
    root.boneId = 10; root.name = "root";
    child.boneId = 20; child.parentBoneId = 10; child.name = "child";
    child.localBind.translation = VEC3(1.0f, 0.0f, 0.0f);
    CANONICAL_SKELETON skeleton;
    skeleton.skeletonId = 100; skeleton.sourceBones = {root, child};
    if (!compileCanonicalSkeleton(skeleton.sourceBones, skeleton.compiled)) return false;
    CANONICAL_WEIGHTS weights;
    weights.skeletonId = 100; weights.paletteBoneIds = {10, 20}; weights.vertices.resize(2);
    weights.vertices[0].paletteIndex[0] = 0; weights.vertices[0].paletteIndex[1] = 1;
    weights.vertices[0].weight[0] = 0.25f; weights.vertices[0].weight[1] = 0.75f;
    weights.vertices[1].paletteIndex[0] = 0; weights.vertices[1].paletteIndex[1] = 1;
    weights.vertices[1].weight[0] = 0.60f; weights.vertices[1].weight[1] = 0.40f;
    const std::vector<VEC3> positions = {VEC3(1.5f, 0.2f, 0.0f), VEC3(0.4f, 0.8f, 0.2f)};
    const std::vector<VEC3> normals = {VEC3(0.0f, 1.0f, 0.0f), VEC3(0.0f, 0.0f, 1.0f)};
    LOCAL_TRANSFORM posedRoot = root.localBind, posedChild = child.localBind;
    posedRoot.rotation = {0, 0, 0.2588190451f, 0.9659258263f};
    posedChild.rotation = {0, 0, -0.3420201433f, 0.9396926208f};
    SKELETAL_POSE pose;
    pose.localTransforms = {posedRoot, posedChild};
    const MATRIX rootGlobal = buildTrsMatrix(posedRoot);
    const MATRIX childLocal = buildTrsMatrix(posedChild);
    MATRIX childGlobal;
    MatrixMultiply(&childGlobal, &childLocal, &rootGlobal);
    pose.globalTransforms = {rootGlobal, childGlobal};
    std::vector<VEC3> cpuPositions, cpuNormals;
    std::vector<float> palette;
    std::vector<GPU_LBS_VERTEX> gpuWeights(2);
    for (size_t vertex = 0; vertex < gpuWeights.size(); ++vertex)
        for (size_t influence = 0; influence < 4; ++influence)
        {
            gpuWeights[vertex].boneIndex[influence] =
                static_cast<float>(weights.vertices[vertex].paletteIndex[influence]);
            gpuWeights[vertex].weight[influence] = weights.vertices[vertex].weight[influence];
        }
    if (!skinVerticesLbsReference(skeleton, weights, pose, positions, normals, cpuPositions, cpuNormals) ||
        buildGles2LbsPalette(skeleton, pose, true, palette) != GLES2_LBS_PALETTE_STATUS::READY ||
        !runMethod("synthetic", false, 2, palette, positions, normals, gpuWeights,
                   cpuPositions, cpuNormals)) return false;
    if (!skinVerticesDqsRigidReference(skeleton, weights, pose, positions, normals, cpuPositions, cpuNormals) ||
        buildGles2DqsPalette(skeleton, pose, palette) != GLES2_DQS_PALETTE_STATUS::READY ||
        !runMethod("synthetic", true, 2, palette, positions, normals, gpuWeights,
                   cpuPositions, cpuNormals)) return false;
    return runRealAssetParity();
}
