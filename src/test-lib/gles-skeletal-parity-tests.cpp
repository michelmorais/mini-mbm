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
#include "skeletal-parity-tests.h"

#include <skeletal-gles-shader-source.h>
#include <core_mbm/util-interface.h>
#include <GLES2/gl2.h>

#include <string>
#include <vector>

namespace
{
    using namespace mbm;
    using namespace mbm::skeletal;
    using namespace mbm::skeletal::test;

    GLuint compileShader(const GLenum type, const char *source, std::string &error)
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
        error = std::string("OpenGL ES skeletal parity shader compile failed: ") + log;
        glDeleteShader(shader);
        return 0;
    }

    GLuint buildProgram(const SKELETAL_SHADER_METHOD method, const uint32_t paletteSize,
                        std::string &error)
    {
        std::string vertexSource =
            "attribute vec4 aPosition;attribute vec3 aNormal;attribute vec4 aBoneIndices;"
            "attribute vec4 aBoneWeights;attribute float aSample;";
        appendGlesSkeletalFunctions(vertexSource, paletteSize, method);
        vertexSource += "varying vec3 vPosition;varying vec3 vNormal;void main(){";
        appendGlesSkeletalDeformation(vertexSource, method, true);
        vertexSource += "vPosition=skinnedPosition.xyz;vNormal=skinnedNormal;"
            "gl_Position=vec4(aSample,0.0,0.0,1.0);gl_PointSize=1.0;}";
        const char *fragmentSource =
            "precision highp float;varying vec3 vPosition;varying vec3 vNormal;uniform float outputNormal;"
            "uniform vec3 encodeCenter;uniform float encodeExtent;"
            "void main(){vec3 value=outputNormal>0.5?vNormal*0.5+0.5:"
            "(vPosition-encodeCenter)/(2.0*encodeExtent)+0.5;"
            "gl_FragColor=vec4(clamp(value,0.0,1.0),1.0);}";
        const GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource.c_str(), error);
        if (!vertex)
            return 0;
        const GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource, error);
        if (!fragment)
        {
            glDeleteShader(vertex);
            return 0;
        }
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
        error = std::string("OpenGL ES skeletal parity program link failed: ") + log;
        glDeleteProgram(program);
        return 0;
    }

    bool captureRgba8(const SKELETAL_PARITY_CASE &testCase,
                      const SKELETAL_PARITY_ENCODING &encoding,
                      std::vector<uint8_t> &positionPixels,
                      std::vector<uint8_t> &normalPixels,
                      std::string &error)
    {
        const size_t sampleCount = testCase.positions.size();
        const GLuint program = buildProgram(testCase.method, testCase.paletteSize, error);
        if (!program)
            return false;

        GLuint texture = 0;
        GLuint framebuffer = 0;
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
            error = "OpenGL ES skeletal parity framebuffer is incomplete";
            glDeleteFramebuffers(1, &framebuffer);
            glDeleteTextures(1, &texture);
            glDeleteProgram(program);
            return false;
        }

        std::vector<float> boneIndices(sampleCount * 4);
        std::vector<float> boneWeights(sampleCount * 4);
        std::vector<float> samplePositions(sampleCount);
        for (size_t vertex = 0; vertex < sampleCount; ++vertex)
        {
            for (size_t influence = 0; influence < 4; ++influence)
            {
                boneIndices[vertex * 4 + influence] =
                    testCase.influences[vertex].boneIndex[influence];
                boneWeights[vertex * 4 + influence] =
                    testCase.influences[vertex].weight[influence];
            }
            samplePositions[vertex] = ((static_cast<float>(vertex) + 0.5f) /
                static_cast<float>(sampleCount)) * 2.0f - 1.0f;
        }

        glUseProgram(program);
        const auto bind = [program](const char *name, const GLint size, const void *data)
        {
            const GLint location = glGetAttribLocation(program, name);
            glEnableVertexAttribArray(static_cast<GLuint>(location));
            glVertexAttribPointer(static_cast<GLuint>(location), size, GL_FLOAT, GL_FALSE, 0, data);
        };
        bind("aPosition", 3, testCase.positions.data());
        bind("aNormal", 3, testCase.normals.data());
        bind("aBoneIndices", 4, boneIndices.data());
        bind("aBoneWeights", 4, boneWeights.data());
        bind("aSample", 1, samplePositions.data());
        const bool dqs = testCase.method == SKELETAL_SHADER_METHOD::DQS_RIGID;
        glUniform4fv(glGetUniformLocation(program, "bonePalette"),
                     static_cast<GLsizei>(testCase.paletteSize * (dqs ? 2u : 3u)),
                     testCase.palette.data());
        glUniform3f(glGetUniformLocation(program, "encodeCenter"),
                    encoding.positionCenter.x, encoding.positionCenter.y,
                    encoding.positionCenter.z);
        glUniform1f(glGetUniformLocation(program, "encodeExtent"), encoding.positionExtent);
        glViewport(0, 0, static_cast<GLsizei>(sampleCount), 1);
        positionPixels.resize(sampleCount * 4);
        normalPixels.resize(sampleCount * 4);
        for (int normalPass = 0; normalPass < 2; ++normalPass)
        {
            glUniform1f(glGetUniformLocation(program, "outputNormal"),
                        static_cast<float>(normalPass));
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawArrays(GL_POINTS, 0, static_cast<GLsizei>(sampleCount));
            std::vector<uint8_t> &pixels = normalPass ? normalPixels : positionPixels;
            glReadPixels(0, 0, static_cast<GLsizei>(sampleCount), 1,
                         GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        }
        const GLenum glError = glGetError();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteFramebuffers(1, &framebuffer);
        glDeleteTextures(1, &texture);
        glDeleteProgram(program);
        if (glError != GL_NO_ERROR)
        {
            error = "OpenGL ES skeletal parity capture reported a GL error";
            return false;
        }
        return true;
    }
}

bool runGlesSkeletalParityTests()
{
    std::string error;
    if (mbm::skeletal::test::runSkeletalParitySuite("OpenGL ES", captureRgba8, error))
        return true;
    ERROR_LOG("testLib: %s", error.c_str());
    return false;
}
