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

#ifndef GLES_DEBUG_H
#define GLES_DEBUG_H

#include "core-exports.h"

#if defined ANDROID
    #include <EGL/egl.h>
    #include <GLES2/gl2.h>

#elif defined __MINGW32__ | defined __CYGWIN__
    #include <gles/EGL/egl.h>
    #include <gles/GLES2/gl2.h>

#elif defined _WIN32
    #include <EGL/egl.h>
    #include <GLES2/gl2.h>
    #ifndef strcasecmp
        #define strcasecmp strcmpi
    #endif
#elif defined __linux__
    #include <EGL/egl.h>
    #include <GLES2/gl2.h>
#else
    #error unknown platform
#endif


namespace log_util
{
    API_IMPL void checkGlError(const char *fileName, const int numLine);
}


#ifdef _DEBUG
#define GLBindBuffer(target, buffer)                                                                                     \
    glBindBuffer(target, buffer);                                                                                        \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLBindBuffer(target, buffer) glBindBuffer(target, buffer)
#endif

#ifdef _DEBUG
#define GLBufferData(target, size, data, usage)                                                                          \
    glBufferData(target, size, data, usage);                                                                             \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLBufferData(target, size, data, usage) glBufferData(target, size, data, usage);
#endif

#ifdef _DEBUG
#define GLGenBuffers(n, buffers)                                                                                         \
    glGenBuffers(n, buffers);                                                                                            \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLGenBuffers(n, buffers) glGenBuffers(n, buffers);
#endif

#ifdef _DEBUG
#define GLGetUniformLocation(program, name)                                                                              \
    glGetUniformLocation(program, name);                                                                                 \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLGetUniformLocation(program, name) glGetUniformLocation(program, name);
#endif

#ifdef _DEBUG
#define GLDeleteProgram(program)                                                                                         \
    glDeleteProgram(program);                                                                                            \
    log_util::checkGlError(__FILE__, __LINE__);
#else
#define GLDeleteProgram(program) glDeleteProgram(program);
#endif

#ifdef _DEBUG
#define GLGetAttribLocation(program, name)                                                                               \
    glGetAttribLocation(program, name);                                                                                  \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLGetAttribLocation(program, name) glGetAttribLocation(program, name);
#endif

#ifdef _DEBUG
#define GLUseProgram(program)                                                                                            \
    glUseProgram(program);                                                                                               \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLUseProgram(program) glUseProgram(program);
#endif

#ifdef _DEBUG
#define GLEnableVertexAttribArray(index)                                                                                 \
    glEnableVertexAttribArray(index);                                                                                    \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLEnableVertexAttribArray(index) glEnableVertexAttribArray(index);
#endif

#ifdef _DEBUG
#define GLVertexAttribPointer(indx, size, type, normalized, stride, ptr)                                                 \
    glVertexAttribPointer(indx, size, type, normalized, stride, ptr);                                                    \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLVertexAttribPointer(indx, size, type, normalized, stride, ptr)                                                 \
    glVertexAttribPointer(indx, size, type, normalized, stride, ptr);
#endif

#ifdef _DEBUG
#define GLUniformMatrix4fv(location, count, transpose, value)                                                            \
    glUniformMatrix4fv(location, count, transpose, value);                                                               \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLUniformMatrix4fv(location, count, transpose, value) glUniformMatrix4fv(location, count, transpose, value);
#endif

#ifdef _DEBUG
#define GLActiveTexture(texture)                                                                                         \
    glActiveTexture(texture);                                                                                            \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLActiveTexture(texture) glActiveTexture(texture);
#endif

#ifdef _DEBUG
#define GLBindTexture(target, exture)                                                                                    \
    glBindTexture(target, exture);                                                                                       \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLBindTexture(target, exture) glBindTexture(target, exture);
#endif

#ifdef _DEBUG
#define GLUniform1i(location, x)                                                                                         \
    glUniform1i(location, x);                                                                                            \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLUniform1i(location, x) glUniform1i(location, x);
#endif

#ifdef _DEBUG
#define GLCreateShader(type)                                                                                             \
    glCreateShader(type);                                                                                                \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLCreateShader(type) glCreateShader(type);
#endif

#ifdef _DEBUG
#define GLShaderSource(shader, count, string, length)                                                                    \
    glShaderSource(shader, count, string, length);                                                                       \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLShaderSource(shader, count, string, length) glShaderSource(shader, count, string, length);
#endif

#ifdef _DEBUG
#define GLCompileShader(shader)                                                                                          \
    glCompileShader(shader);                                                                                             \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLCompileShader(shader) glCompileShader(shader);
#endif

#ifdef _DEBUG
#define GLGetShaderiv(shader, pname, params)                                                                             \
    glGetShaderiv(shader, pname, params);                                                                                \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLGetShaderiv(shader, pname, params) glGetShaderiv(shader, pname, params);
#endif

#ifdef _DEBUG
#define GLDeleteShader(shader)                                                                                           \
    glDeleteShader(shader);                                                                                              \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLDeleteShader(shader) glDeleteShader(shader);
#endif

#ifdef _DEBUG
#define GLCreateProgram()                                                                                                \
    glCreateProgram();                                                                                                   \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLCreateProgram() glCreateProgram();
#endif

#ifdef _DEBUG
#define GLAttachShader(program, shader)                                                                                  \
    glAttachShader(program, shader);                                                                                     \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLAttachShader(program, shader) glAttachShader(program, shader);
#endif

#ifdef _DEBUG
#define GLLinkProgram(program)                                                                                           \
    glLinkProgram(program);                                                                                              \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLLinkProgram(program) glLinkProgram(program);
#endif

#ifdef _DEBUG
#define GLGetProgramiv(program, pname, params)                                                                           \
    glGetProgramiv(program, pname, params);                                                                              \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLGetProgramiv(program, pname, params) glGetProgramiv(program, pname, params);
#endif

#ifdef _DEBUG
#define GLGetProgramInfoLog(program, bufsize, length, infolog)                                                           \
    glGetProgramInfoLog(program, bufsize, length, infolog);                                                              \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLGetProgramInfoLog(program, bufsize, length, infolog) glGetProgramInfoLog(program, bufsize, length, infolog);
#endif

#ifdef _DEBUG
#define GLUniform1f(location, x)                                                                                         \
    glUniform1f(location, x);                                                                                            \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLUniform1f(location, x) glUniform1f(location, x);
#endif

#ifdef _DEBUG
#define GLUniform2f(location, x, y)                                                                                      \
    glUniform2f(location, x, y);                                                                                         \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLUniform2f(location, x, y) glUniform2f(location, x, y);
#endif

#ifdef _DEBUG
#define GLUniform3f(location, x, y, z)                                                                                   \
    glUniform3f(location, x, y, z);                                                                                      \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLUniform3f(location, x, y, z) glUniform3f(location, x, y, z);
#endif

#ifdef _DEBUG
#define GLUniform4f(location, x, y, z, w)                                                                                \
    glUniform4f(location, x, y, z, w);                                                                                   \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLUniform4f(location, x, y, z, w) glUniform4f(location, x, y, z, w);
#endif

#ifdef _DEBUG
#define GLBlendFunc(sfactor, dfactor)                                                                                    \
    glBlendFunc(sfactor, dfactor);                                                                                       \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLBlendFunc(sfactor, dfactor) glBlendFunc(sfactor, dfactor);
#endif

#ifdef _DEBUG
#define GLBindFramebuffer(target, framebuffer)                                                                           \
    glBindFramebuffer(target, framebuffer);                                                                              \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLBindFramebuffer(target, framebuffer) glBindFramebuffer(target, framebuffer);
#endif

#ifdef _DEBUG
#define GLBindRenderbuffer(target, renderbuffer)                                                                         \
    glBindRenderbuffer(target, renderbuffer);                                                                            \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLBindRenderbuffer(target, renderbuffer) glBindRenderbuffer(target, renderbuffer);
#endif

#ifdef _DEBUG
#define GLDisable(cap)                                                                                                   \
    glDisable(cap);                                                                                                      \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLDisable(cap) glDisable(cap);
#endif

#ifdef _DEBUG
#define GLEnable(cap)                                                                                                    \
    glEnable(cap);                                                                                                       \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLEnable(cap) glEnable(cap);
#endif

#ifdef _DEBUG
#define GLViewport(x, y, width, height)                                                                                  \
    glViewport(x, y, width, height);                                                                                     \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLViewport(x, y, width, height) glViewport(x, y, width, height);
#endif

#ifdef _DEBUG
#define GLClearColor(red, green, blue, alpha)                                                                            \
    glClearColor(red, green, blue, alpha);                                                                               \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLClearColor(red, green, blue, alpha) glClearColor(red, green, blue, alpha);
#endif

#ifdef _DEBUG
#define GLDepthRangef(zNear, zFar)                                                                                       \
    glDepthRangef(zNear, zFar);                                                                                          \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLDepthRangef(zNear, zFar) glDepthRangef(zNear, zFar);
#endif

#ifdef _DEBUG
#define GLCullFace(mode)                                                                                                 \
    glCullFace(mode);                                                                                                    \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLCullFace(mode) glCullFace(mode);
#endif

#ifdef _DEBUG
#define GLFrontFace(mode)                                                                                                \
    glFrontFace(mode);                                                                                                   \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLFrontFace(mode) glFrontFace(mode);
#endif

#ifdef _DEBUG
#define GLDepthFunc(func)                                                                                                \
    glDepthFunc(func);                                                                                                   \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLDepthFunc(func) glDepthFunc(func);
#endif

#ifdef _DEBUG
#define GLClearDepthf(depth)                                                                                             \
    glClearDepthf(depth);                                                                                                \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLClearDepthf(depth) glClearDepthf(depth);
#endif

#ifdef _DEBUG
#define GLFramebufferTexture2D(target, attachment, textarget, texture, level)                                            \
    glFramebufferTexture2D(target, attachment, textarget, texture, level);                                               \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLFramebufferTexture2D(target, attachment, textarget, texture, level)                                            \
    glFramebufferTexture2D(target, attachment, textarget, texture, level);
#endif

#ifdef _DEBUG
#define GLFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer)                                  \
    glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);                                     \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer)                                  \
    glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
#endif

#ifdef _DEBUG
#define GLClear(mask)                                                                                                    \
    glClear(mask);                                                                                                       \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLClear(mask) glClear(mask);
#endif

#ifdef _DEBUG
#define GLCheckFramebufferStatus(target)                                                                                 \
    glCheckFramebufferStatus(target);                                                                                    \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLCheckFramebufferStatus(target) glCheckFramebufferStatus(target);
#endif

#ifdef _DEBUG
#define GLDeleteTextures(n, textures)                                                                                    \
    glDeleteTextures(n, textures);                                                                                       \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLDeleteTextures(n, textures) glDeleteTextures(n, textures);
#endif

#ifdef _DEBUG
#define GLPixelStorei(pname, param)                                                                                      \
    glPixelStorei(pname, param);                                                                                         \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLPixelStorei(pname, param) glPixelStorei(pname, param);
#endif

#ifdef _DEBUG
#define GLGenTextures(n, textures)                                                                                       \
    glGenTextures(n, textures);                                                                                          \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLGenTextures(n, textures) glGenTextures(n, textures);
#endif

#ifdef _DEBUG
#define GLTexImage2D(target, level, internalformat, width, height, border, format, type, pixels)                         \
    glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);                            \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLTexImage2D(target, level, internalformat, width, height, border, format, type, pixels)                         \
    glTexImage2D(target, level, internalformat, width, height, border, format, type, pixels);
#endif

#ifdef _DEBUG
#define GLTexParameteri(target, pname, param)                                                                            \
    glTexParameteri(target, pname, param);                                                                               \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLTexParameteri(target, pname, param) glTexParameteri(target, pname, param);
#endif

#ifdef _DEBUG
#define GLRenderbufferStorage(target, internalformat, width, height)                                                     \
    glRenderbufferStorage(target, internalformat, width, height);                                                        \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLRenderbufferStorage(target, internalformat, width, height)                                                     \
    glRenderbufferStorage(target, internalformat, width, height);
#endif

#ifdef _DEBUG
    #define GLGenFramebuffers(n, framebuffers)                                                                               \
    glGenFramebuffers(n, framebuffers);                                                                                  \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLGenFramebuffers(n, framebuffers) glGenFramebuffers(n, framebuffers);
#endif

#ifdef _DEBUG
#define GLGenRenderbuffers(n, renderbuffers)                                                                             \
    glGenRenderbuffers(n, renderbuffers);                                                                                \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLGenRenderbuffers(n, renderbuffers) glGenRenderbuffers(n, renderbuffers);
#endif

#ifdef _DEBUG
#define GLDrawArrays(mode, first, count)                                                                                 \
    glDrawArrays(mode, first, count);                                                                                    \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLDrawArrays(mode, first, count) glDrawArrays(mode, first, count);
#endif

#ifdef _DEBUG
#define GLGetShaderInfoLog(shader, bufsize, length, infolog)                                                             \
    glGetShaderInfoLog(shader, bufsize, length, infolog);                                                                \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLGetShaderInfoLog(shader, bufsize, length, infolog) glGetShaderInfoLog(shader, bufsize, length, infolog);
#endif

#ifdef _DEBUG
#define GLDrawElements(mode, count, type, indices)                                                                       \
    glDrawElements(mode, count, type, indices);                                                                          \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLDrawElements(mode, count, type, indices) glDrawElements(mode, count, type, indices);
#endif

#ifdef _DEBUG
#define GLDrawElements(mode, count, type, indices)                                                                       \
    glDrawElements(mode, count, type, indices);                                                                          \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLDrawElements(mode, count, type, indices) glDrawElements(mode, count, type, indices);
#endif

#ifdef _DEBUG
#define GLDeleteBuffers(n, buffers)                                                                                      \
    glDeleteBuffers(n, buffers);                                                                                         \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLDeleteBuffers(n, buffers) glDeleteBuffers(n, buffers);
#endif

#ifdef _DEBUG
#define GLGetIntegerv(pname, params)                                                                                     \
    glGetIntegerv(pname, params);                                                                                        \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLGetIntegerv(pname, params) glGetIntegerv(pname, params);
#endif

#ifdef _DEBUG
#define GLDeleteRenderbuffers(n, renderbuffers)                                                                          \
    glDeleteRenderbuffers(n, renderbuffers);                                                                             \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLDeleteRenderbuffers(n, renderbuffers) glDeleteRenderbuffers(n, renderbuffers);
#endif

#ifdef _DEBUG
#define GLDeleteFramebuffers(n, framebuffers)                                                                            \
    glDeleteFramebuffers(n, framebuffers);                                                                               \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLDeleteFramebuffers(n, framebuffers) glDeleteFramebuffers(n, framebuffers);
#endif

#ifdef _DEBUG
#define GLBlendEquation(mode)                                                                                            \
    glBlendEquation(mode);                                                                                               \
    log_util::checkGlError(__FILE__, __LINE__);
#else
    #define GLBlendEquation(mode) glBlendEquation(mode);
#endif

#endif
