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


// Test-only ELF interposition. No engine code links against this library.
#include "gles-faults.h"
#include <GLES2/gl2.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
namespace
{
    mbm_test::STATS stats = {};
    int fault = mbm_test::NONE;
    template<typename T> T real(const char *name)
    {
        auto fn = reinterpret_cast<T>(dlsym(RTLD_NEXT,name));
        if (!fn) { std::fprintf(stderr,"GLES FAULT FAIL: missing %s\n",name); std::abort(); }
        return fn;
    }
    bool consume(int expected)
    {
        if (fault != expected) return false;
        fault = mbm_test::NONE;
        ++stats.injected;
        return true;
    }
}
#define TEST_EXPORT extern "C" __attribute__((visibility("default")))
TEST_EXPORT void mbm_test_arm_gles_fault(int next) { stats = {}; fault = next; }
TEST_EXPORT void mbm_test_read_gles_fault(mbm_test::STATS *out) { *out = stats; }
TEST_EXPORT GLuint GL_APIENTRY glCreateShader(GLenum type)
{
    const auto id = real<decltype(&glCreateShader)>("glCreateShader")(type);
    if (id) { if (stats.shadersCreated < 2) stats.shaderIds[stats.shadersCreated] = id; ++stats.shadersCreated; }
    return id;
}
TEST_EXPORT void GL_APIENTRY glDeleteShader(GLuint id)
{
    if (id) ++stats.shadersDeleted;
    real<decltype(&glDeleteShader)>("glDeleteShader")(id);
}
TEST_EXPORT void GL_APIENTRY glCompileShader(GLuint id)
{
    ++stats.compiles;
    GLint type = 0;
    real<decltype(&glGetShaderiv)>("glGetShaderiv")(id,GL_SHADER_TYPE,&type);
    if (consume(type == GL_VERTEX_SHADER ? mbm_test::VERTEX_COMPILE : mbm_test::FRAGMENT_COMPILE))
    {
        const char *invalid = "intentional invalid GLSL for recovery test";
        real<decltype(&glShaderSource)>("glShaderSource")(id,1,&invalid,nullptr);
    }
    real<decltype(&glCompileShader)>("glCompileShader")(id);
}
TEST_EXPORT GLuint GL_APIENTRY glCreateProgram()
{
    if (consume(mbm_test::PROGRAM_CREATE)) return 0;
    const auto id = real<decltype(&glCreateProgram)>("glCreateProgram")();
    if (id) { ++stats.programsCreated; stats.programId = id; }
    return id;
}
TEST_EXPORT void GL_APIENTRY glDeleteProgram(GLuint id)
{
    if (id) ++stats.programsDeleted;
    real<decltype(&glDeleteProgram)>("glDeleteProgram")(id);
}
TEST_EXPORT void GL_APIENTRY glLinkProgram(GLuint id)
{
    if (consume(mbm_test::PROGRAM_LINK))
    {
        GLuint shaders[2] = {}; GLsizei count = 0;
        real<decltype(&glGetAttachedShaders)>("glGetAttachedShaders")(id,2,&count,shaders);
        for (GLsizei i=0; i<count; ++i)
            real<decltype(&glDetachShader)>("glDetachShader")(id,shaders[i]);
    }
    real<decltype(&glLinkProgram)>("glLinkProgram")(id);
}
TEST_EXPORT void GL_APIENTRY glGenBuffers(GLsizei count, GLuint *ids)
{
    real<decltype(&glGenBuffers)>("glGenBuffers")(count,ids);
    for (GLsizei i=0; i<count; ++i)
        if (ids[i]) { if (stats.buffersCreated < 2) stats.bufferIds[stats.buffersCreated] = ids[i]; ++stats.buffersCreated; }
}
TEST_EXPORT void GL_APIENTRY glDeleteBuffers(GLsizei count, const GLuint *ids)
{
    for (GLsizei i=0; i<count; ++i) if (ids[i]) ++stats.buffersDeleted;
    real<decltype(&glDeleteBuffers)>("glDeleteBuffers")(count,ids);
}
TEST_EXPORT void GL_APIENTRY glBufferData(GLenum target, GLsizeiptr size, const void *data, GLenum usage)
{
    ++stats.uploads;
    // Negative size generates a real GL_INVALID_VALUE without exhausting host
    // memory. This exercises upload error handling, not physical driver OOM.
    if (consume(target == GL_ARRAY_BUFFER ? mbm_test::VERTEX_UPLOAD : mbm_test::INDEX_UPLOAD)) size = -1;
    real<decltype(&glBufferData)>("glBufferData")(target,size,data,usage);
}
