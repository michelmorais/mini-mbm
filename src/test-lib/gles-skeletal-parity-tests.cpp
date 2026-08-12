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

    constexpr float POSITION_RANGE = 4.0f;
    constexpr float POSITION_TOLERANCE = (POSITION_RANGE * 2.0f / 255.0f) + 0.002f;
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

    GLuint buildProgram(const bool dqs)
    {
        std::string vs =
            "attribute vec3 aPosition; attribute vec3 aNormal; attribute vec4 aBoneIndices;"
            "attribute vec4 aBoneWeights; attribute float aSample; uniform vec4 bonePalette[";
        vs += dqs ? "4];" : "6];";
        if (dqs)
        {
            vs +=
                "vec4 qmul(vec4 a,vec4 b){return vec4(a.w*b.xyz+b.w*a.xyz+cross(a.xyz,b.xyz),a.w*b.w-dot(a.xyz,b.xyz));}"
                "vec3 qrotate(vec3 v,vec4 q){return v+2.0*cross(q.xyz,cross(q.xyz,v)+q.w*v);}"
                "void addDq(float bone,float weight,vec4 refQ,inout vec4 realQ,inout vec4 dualQ){"
                "int first=int(bone)*2;vec4 r=bonePalette[first];float s=dot(r,refQ)<0.0?-1.0:1.0;"
                "realQ+=r*(weight*s);dualQ+=bonePalette[first+1]*(weight*s);}"
                "void deform(out vec3 p,out vec3 n){vec4 r=vec4(0.0),d=vec4(0.0);"
                "vec4 refQ=bonePalette[int(aBoneIndices.x)*2];"
                "addDq(aBoneIndices.x,aBoneWeights.x,refQ,r,d);addDq(aBoneIndices.y,aBoneWeights.y,refQ,r,d);"
                "addDq(aBoneIndices.z,aBoneWeights.z,refQ,r,d);addDq(aBoneIndices.w,aBoneWeights.w,refQ,r,d);"
                "float len=length(r);r/=len;d/=len;d-=r*dot(r,d);vec4 c=vec4(-r.xyz,r.w);"
                "p=qrotate(aPosition,r)+2.0*qmul(d,c).xyz;n=normalize(qrotate(aNormal,r));}";
        }
        else
        {
            vs +=
                "vec3 skinPoint(vec4 v,float bone){int first=int(bone)*3;return vec3(dot(v,bonePalette[first]),"
                "dot(v,bonePalette[first+1]),dot(v,bonePalette[first+2]));}"
                "vec3 skinVector(vec3 v,float bone){return skinPoint(vec4(v,0.0),bone);}"
                "void deform(out vec3 p,out vec3 n){p=skinPoint(vec4(aPosition,1.0),aBoneIndices.x)*aBoneWeights.x+"
                "skinPoint(vec4(aPosition,1.0),aBoneIndices.y)*aBoneWeights.y+"
                "skinPoint(vec4(aPosition,1.0),aBoneIndices.z)*aBoneWeights.z+"
                "skinPoint(vec4(aPosition,1.0),aBoneIndices.w)*aBoneWeights.w;"
                "n=normalize(skinVector(aNormal,aBoneIndices.x)*aBoneWeights.x+"
                "skinVector(aNormal,aBoneIndices.y)*aBoneWeights.y+"
                "skinVector(aNormal,aBoneIndices.z)*aBoneWeights.z+skinVector(aNormal,aBoneIndices.w)*aBoneWeights.w);}";
        }
        vs +=
            "varying vec3 vPosition;varying vec3 vNormal;void main(){deform(vPosition,vNormal);"
            "gl_Position=vec4(aSample,0.0,0.0,1.0);gl_PointSize=1.0;}";
        const char *fs =
            "precision highp float;varying vec3 vPosition;varying vec3 vNormal;uniform float outputNormal;"
            "void main(){vec3 value=outputNormal>0.5?vNormal*0.5+0.5:vPosition*0.125+0.5;"
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

    bool runMethod(const bool dqs, const std::vector<float> &palette,
                   const std::vector<VEC3> &positions, const std::vector<VEC3> &normals,
                   const CANONICAL_WEIGHTS &weights, const std::vector<VEC3> &cpuPositions,
                   const std::vector<VEC3> &cpuNormals)
    {
        const GLuint program = buildProgram(dqs);
        if (!program)
            return false;
        GLuint texture = 0, framebuffer = 0;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenFramebuffers(1, &framebuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            ERROR_LOG("skeletal GPU parity framebuffer is incomplete");
            glDeleteFramebuffers(1, &framebuffer); glDeleteTextures(1, &texture); glDeleteProgram(program);
            return false;
        }

        float boneIndices[8] = {}, boneWeights[8] = {};
        for (size_t vertex = 0; vertex < 2; ++vertex)
            for (size_t influence = 0; influence < 4; ++influence)
            {
                boneIndices[vertex * 4 + influence] = static_cast<float>(weights.vertices[vertex].paletteIndex[influence]);
                boneWeights[vertex * 4 + influence] = weights.vertices[vertex].weight[influence];
            }
        const float samples[2] = {-0.5f, 0.5f};
        glUseProgram(program);
        const auto bind = [program](const char *name, GLint size, const void *data)
        {
            const GLint location = glGetAttribLocation(program, name);
            glEnableVertexAttribArray(location);
            glVertexAttribPointer(location, size, GL_FLOAT, GL_FALSE, 0, data);
        };
        bind("aPosition", 3, positions.data()); bind("aNormal", 3, normals.data());
        bind("aBoneIndices", 4, boneIndices); bind("aBoneWeights", 4, boneWeights); bind("aSample", 1, samples);
        glUniform4fv(glGetUniformLocation(program, "bonePalette"), dqs ? 4 : 6, palette.data());
        glViewport(0, 0, 2, 1);
        unsigned char pixels[8] = {};
        float maxPositionError = 0.0f, maxNormalError = 0.0f;
        size_t worstPosition = 0, worstNormal = 0;
        for (int normalPass = 0; normalPass < 2; ++normalPass)
        {
            glUniform1f(glGetUniformLocation(program, "outputNormal"), static_cast<float>(normalPass));
            glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_POINTS, 0, 2);
            glReadPixels(0, 0, 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            for (size_t i = 0; i < 2; ++i)
            {
                const float scale = normalPass ? 2.0f : POSITION_RANGE * 2.0f;
                const float offset = normalPass ? -1.0f : -POSITION_RANGE;
                const VEC3 gpu(pixels[i * 4] / 255.0f * scale + offset,
                               pixels[i * 4 + 1] / 255.0f * scale + offset,
                               pixels[i * 4 + 2] / 255.0f * scale + offset);
                const float error = maxDifference(gpu, normalPass ? cpuNormals[i] : cpuPositions[i]);
                if (normalPass && error > maxNormalError) { maxNormalError = error; worstNormal = i; }
                if (!normalPass && error > maxPositionError) { maxPositionError = error; worstPosition = i; }
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &framebuffer); glDeleteTextures(1, &texture); glDeleteProgram(program);
        INFO_LOG("skeletal GPU parity: method=%s samples=2 max-position-error=%.7f vertex=%u tolerance=%.7f "
                 "max-normal-error=%.7f vertex=%u tolerance=%.7f %s",
                 dqs ? "dqs" : "lbs", maxPositionError, static_cast<unsigned>(worstPosition), POSITION_TOLERANCE,
                 maxNormalError, static_cast<unsigned>(worstNormal), NORMAL_TOLERANCE,
                 maxPositionError <= POSITION_TOLERANCE && maxNormalError <= NORMAL_TOLERANCE ? "PASS" : "FAIL");
        return maxPositionError <= POSITION_TOLERANCE && maxNormalError <= NORMAL_TOLERANCE;
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
    if (!skinVerticesLbsReference(skeleton, weights, pose, positions, normals, cpuPositions, cpuNormals) ||
        buildGles2LbsPalette(skeleton, pose, true, palette) != GLES2_LBS_PALETTE_STATUS::READY ||
        !runMethod(false, palette, positions, normals, weights, cpuPositions, cpuNormals)) return false;
    if (!skinVerticesDqsRigidReference(skeleton, weights, pose, positions, normals, cpuPositions, cpuNormals) ||
        buildGles2DqsPalette(skeleton, pose, palette) != GLES2_DQS_PALETTE_STATUS::READY ||
        !runMethod(true, palette, positions, normals, weights, cpuPositions, cpuNormals)) return false;
    return true;
}
