/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2025 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#if defined(USE_OPENGL_ES)
#include <mesh-manager.h>
#include <texture-manager.h>
#include <util-interface.h>
#include <shapes.h>
#include <shader.h>
#include <specific-opengl_es.h>
#include "specific-opengl_es-buffer.h"
#include <GLES2/gl2ext.h>
#include <climits>
#include <cstdio>
#include <cstring>
#include <limits>

namespace mbm
{
namespace
{
    struct BUFFER_BINDINGS
    {
        GLint vertex = 0, index = 0;
        BUFFER_BINDINGS() noexcept
        {
            glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &vertex);
            glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &index);
        }
        ~BUFFER_BINDINGS()
        {
            GLBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(vertex));
            GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(index));
        }
    };

    bool hasExtension(const char *extensions, const char *name)
    {
        if (!extensions) return false;
        const size_t length = std::strlen(name);
        const char *match = extensions;
        while ((match = std::strstr(match, name)))
        {
            if ((match == extensions || match[-1] == ' ') && (match[length] == 0 || match[length] == ' ')) return true;
            match += length;
        }
        return false;
    }

    struct BUFFER_READER
    {
        PFNGLMAPBUFFERRANGEEXTPROC map = nullptr;
        PFNGLUNMAPBUFFEROESPROC unmap = nullptr;
        BUFFER_READER()
        {
            const char *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
            int major = 0;
            if (version) std::sscanf(version, "OpenGL ES %d", &major);
            if (major >= 3)
            {
                map = reinterpret_cast<PFNGLMAPBUFFERRANGEEXTPROC>(eglGetProcAddress("glMapBufferRange"));
                unmap = reinterpret_cast<PFNGLUNMAPBUFFEROESPROC>(eglGetProcAddress("glUnmapBuffer"));
            }
            else
            {
                const char *extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
                if (hasExtension(extensions, "GL_EXT_map_buffer_range") && hasExtension(extensions, "GL_OES_mapbuffer"))
                {
                    map = reinterpret_cast<PFNGLMAPBUFFERRANGEEXTPROC>(eglGetProcAddress("glMapBufferRangeEXT"));
                    unmap = reinterpret_cast<PFNGLUNMAPBUFFEROESPROC>(eglGetProcAddress("glUnmapBufferOES"));
                }
            }
        }
        bool read(GLenum target, GLuint buffer, void *destination, size_t bytes) const
        {
            if (bytes == 0) return true;
            if (!map || !unmap)
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "Mesh readback requires ES3 or EXT_map_buffer_range + OES_mapbuffer");
            if (!buffer || !destination || bytes > static_cast<size_t>(std::numeric_limits<GLsizeiptr>::max()))
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "Invalid mesh readback buffer or size");
            GLBindBuffer(target, buffer);
            GLint available = 0;
            glGetBufferParameteriv(target, GL_BUFFER_SIZE, &available);
            if (available < 0 || bytes > static_cast<size_t>(available))
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "Mesh readback exceeds buffer %u (%zu > %d)", buffer, bytes, available);
            const void *mapped = map(target, 0, static_cast<GLsizeiptr>(bytes), GL_MAP_READ_BIT_EXT);
            if (!mapped)
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "Mesh readback failed: buffer %u, GL error 0x%x", buffer, glGetError());
            std::memcpy(destination, mapped, bytes);
            if (!unmap(target))
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "Mesh readback contents invalidated: buffer %u", buffer);
            return true;
        }
    };
}

bool MESH_MBM_DEBUG::fillInSubsetDebug(const MESH_MBM *meshMemory, const int currentFrame,
                                     const std::map<int, float> &letterX,
                                     const std::map<int, float> &letterY,
                                     util::HEADER_FRAME *headerFrame, util::BUFFER_MESH_DEBUG *output)
{
    const BUFFER_MESH *frame = meshMemory->getBuffer(currentFrame);
    const BUFFER_GL *geometry = frame ? frame->getRenderBuffer() : nullptr;
    const BUFFER_SPECIFIC *backend = geometry ? geometry->getBackendBuffer() : nullptr;
    if (!backend || geometry->sizeOfArrayVertex > INT_MAX)
        return log_util::onFailed(nullptr, __FILE__, __LINE__, "Invalid mesh buffer for readback");
    BUFFER_BINDINGS bindings;
    BUFFER_READER reader;
    const bool indexed = geometry->isIndexBuffer();
    const bool normals = geometry->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR || geometry->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV;
    const bool uvs = geometry->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_UV || geometry->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV;
    const size_t count = geometry->sizeOfArrayVertex;
    headerFrame->sizeVertexBuffer = static_cast<int>(count);
    output->position = new float[count*3]();
    output->normal = normals ? new float[count*3]() : nullptr;
    output->uv = new float[count*2]();
    if (indexed) output->indexBuffer = new uint16_t[headerFrame->sizeIndexBuffer]();

    // Subset ranges are authoring metadata. Inferring vertex counts from maxima of
    // global indices duplicates vertices across subsets and loses unused vertices.
    for (uint32_t i = 0; i < frame->getTotalSubsets(); ++i)
    {
        const util::SUBSET *source = frame->getSubset(i);
        if (!source || source->vertexStart < 0 || source->vertexCount < 0 ||
            static_cast<size_t>(source->vertexStart) > count ||
            static_cast<size_t>(source->vertexCount) > count - static_cast<size_t>(source->vertexStart))
            return log_util::onFailed(nullptr, __FILE__, __LINE__, "Invalid source subset vertex range");
        auto *subset = new util::SUBSET_DEBUG();
        output->subset.push_back(subset);
        subset->vertexStart = source->vertexStart;
        subset->vertexCount = source->vertexCount;
        subset->indexStart = indexed ? geometry->indexStartIB[i] : 0;
        subset->indexCount = indexed ? geometry->indexCountIB[i] : 0;
        subset->texture = source->texture ? source->texture->getFileNameTexture() : "default";
        if (indexed)
        {
            if (subset->indexStart < 0 || subset->indexCount < 0 || subset->indexStart > headerFrame->sizeIndexBuffer ||
                subset->indexCount > headerFrame->sizeIndexBuffer - subset->indexStart || !backend->vboIndexSubsetIB)
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "Invalid source subset index range");
            if (!reader.read(GL_ELEMENT_ARRAY_BUFFER, backend->vboIndexSubsetIB[i],
                             output->indexBuffer + subset->indexStart, static_cast<size_t>(subset->indexCount)*sizeof(uint16_t))) return false;
        }
    }

    const util::DYNAMIC_SHAPE *shape = meshMemory->getInfoShape();
    if (shape && shape->dynamicVertex)
    {
        if (shape->size_vertex != count*3 || (normals && (!shape->dynamicNormal || shape->size_normal != count*3)) ||
            (uvs && (!shape->dynamicUV || shape->size_uv != count*2)))
            return log_util::onFailed(nullptr, __FILE__, __LINE__, "Inconsistent dynamic mesh readback arrays");
        std::memcpy(output->position, shape->dynamicVertex, count*3*sizeof(float));
        if (normals) std::memcpy(output->normal, shape->dynamicNormal, count*3*sizeof(float));
        if (uvs) std::memcpy(output->uv, shape->dynamicUV, count*2*sizeof(float));
    }
    else if (indexed)
    {
        if (!reader.read(GL_ARRAY_BUFFER, backend->vboVertNorTexIB[0], output->position, count*3*sizeof(float)) ||
            (normals && !reader.read(GL_ARRAY_BUFFER, backend->vboVertNorTexIB[1], output->normal, count*3*sizeof(float))) ||
            (uvs && !reader.read(GL_ARRAY_BUFFER, backend->vboVertNorTexIB[2], output->uv, count*2*sizeof(float)))) return false;
    }
    else
    {
        if (!backend->vboVertexSubsetVB || (normals && !backend->vboNormalSubsetVB) || (uvs && !backend->vboTextureSubsetVB))
            return log_util::onFailed(nullptr, __FILE__, __LINE__, "Missing non-indexed subset buffers");
        for (uint32_t i = 0; i < frame->getTotalSubsets(); ++i)
        {
            const auto &subset = *output->subset[i];
            const size_t start = static_cast<size_t>(subset.vertexStart), size = static_cast<size_t>(subset.vertexCount);
            if (!reader.read(GL_ARRAY_BUFFER, backend->vboVertexSubsetVB[i], output->position + start*3, size*3*sizeof(float)) ||
                (normals && !reader.read(GL_ARRAY_BUFFER, backend->vboNormalSubsetVB[i], output->normal + start*3, size*3*sizeof(float))) ||
                (uvs && !reader.read(GL_ARRAY_BUFFER, backend->vboTextureSubsetVB[i], output->uv + start*2, size*2*sizeof(float)))) return false;
        }
    }
    if (meshMemory->getInfoFont())
    {
        const auto x = letterX.find(currentFrame), y = letterY.find(currentFrame);
        if (x == letterX.end() || y == letterY.end())
            return log_util::onFailed(nullptr, __FILE__, __LINE__, "Missing font offset for frame %d", currentFrame);
        for (size_t i = 0; i < count; ++i)
        {
            output->position[i*3] += x->second;
            output->position[i*3+1] += y->second;
        }
    }
    return true;
}
}
#endif
