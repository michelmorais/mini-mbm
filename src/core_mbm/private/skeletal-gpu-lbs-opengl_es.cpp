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
|-----------------------------------------------------------------------------------------------------------------------*/

#if defined(USE_OPENGL_ES)

#include "skeletal-gpu-lbs-opengl_es.h"

#include <shader.h>
#include <specific-opengl_es.h>
#include "specific-opengl_es-buffer.h"

namespace mbm::skeletal
{
    namespace
    {
        void splitStreams(const std::vector<GPU_LBS_VERTEX> &vertices,
                          std::vector<float> &indices, std::vector<float> &weights)
        {
            indices.resize(vertices.size() * 4u);
            weights.resize(vertices.size() * 4u);
            for (size_t i = 0; i < vertices.size(); ++i)
            {
                for (uint32_t slot = 0; slot < 4; ++slot)
                {
                    indices[(i * 4u) + slot] = vertices[i].boneIndex[slot];
                    weights[(i * 4u) + slot] = vertices[i].weight[slot];
                }
            }
        }
    }

    bool uploadGles2LbsVertexStreams(BUFFER_GL *buffer, const GLES2_LBS_INPUT &input) noexcept
    {
        if (!buffer || !input.ready() || input.vertices.empty() ||
            input.vertices.size() != buffer->sizeOfArrayVertex)
            return false;
        BUFFER_SPECIFIC *backend = buffer->getBackendBuffer();
        if (!backend)
            return false;
        std::vector<float> indices, weights;
        splitStreams(input.vertices, indices, weights);

        if (buffer->isIndexBuffer())
        {
            GLuint handles[2] = {0, 0};
            GLGenBuffers(2, handles);
            if (handles[0] == 0 || handles[1] == 0)
            {
                GLDeleteBuffers(2, handles);
                return false;
            }
            backend->vboBoneIndicesIB = handles[0];
            backend->vboBoneWeightsIB = handles[1];
            const GLsizeiptr bytes = static_cast<GLsizeiptr>(indices.size() * sizeof(float));
            GLBindBuffer(GL_ARRAY_BUFFER, backend->vboBoneIndicesIB);
            GLBufferData(GL_ARRAY_BUFFER, bytes, indices.data(), GL_STATIC_DRAW);
            GLBindBuffer(GL_ARRAY_BUFFER, backend->vboBoneWeightsIB);
            GLBufferData(GL_ARRAY_BUFFER, bytes, weights.data(), GL_STATIC_DRAW);
        }
        else
        {
            backend->skinSubsetCount = buffer->totalSubset;
            backend->vboBoneIndicesVB = new uint32_t[buffer->totalSubset]();
            backend->vboBoneWeightsVB = new uint32_t[buffer->totalSubset]();
            GLGenBuffers(static_cast<GLsizei>(buffer->totalSubset), backend->vboBoneIndicesVB);
            GLGenBuffers(static_cast<GLsizei>(buffer->totalSubset), backend->vboBoneWeightsVB);
            for (uint32_t subset = 0; subset < buffer->totalSubset; ++subset)
            {
                if (backend->vboBoneIndicesVB[subset] == 0 || backend->vboBoneWeightsVB[subset] == 0)
                    return false;
                const size_t first = static_cast<size_t>(buffer->vertexStartVB[subset]) * 4u;
                const GLsizeiptr bytes = static_cast<GLsizeiptr>(
                    static_cast<size_t>(buffer->vertexCountVB[subset]) * 4u * sizeof(float));
                GLBindBuffer(GL_ARRAY_BUFFER, backend->vboBoneIndicesVB[subset]);
                GLBufferData(GL_ARRAY_BUFFER, bytes, &indices[first], GL_STATIC_DRAW);
                GLBindBuffer(GL_ARRAY_BUFFER, backend->vboBoneWeightsVB[subset]);
                GLBufferData(GL_ARRAY_BUFFER, bytes, &weights[first], GL_STATIC_DRAW);
            }
        }
        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        return true;
    }
}

#endif
