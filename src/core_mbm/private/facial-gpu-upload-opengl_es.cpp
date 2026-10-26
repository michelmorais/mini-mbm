/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                             |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| permit persons to whom the Software is furnished to do so, subject to the following conditions:                       |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE  |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR        |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------*/

#if defined(USE_OPENGL_ES)

#include "facial-gpu-upload.h"

#include <shader.h>
#include "specific-opengl_es-buffer.h"

#include <limits>
#include <vector>

namespace mbm::facial
{
    bool uploadGpuMorphStream(BUFFER_GL *buffer, const ASSET &asset,
                              const GPU_MORPH_LAYOUT &layout) noexcept
    {
        if (!buffer || !layout.valid || asset.targets.size() != layout.targetCount ||
            asset.vertexCount != layout.vertexCount || !asset.vertexCount)
            return false;
        BUFFER_SPECIFIC *backend = buffer->getBackendBuffer();
        if (!backend || !buffer->sizeOfArrayVertex || buffer->sizeOfArrayVertex != asset.vertexCount)
            return false;
        if (backend->facialMorphTexture && backend->facialMorphTextureWidth == layout.textureWidth &&
            backend->facialMorphTextureHeight == layout.textureHeight &&
            backend->facialMorphRowsPerTarget == layout.rowsPerTarget &&
            backend->facialMorphTargetCount == layout.targetCount &&
            backend->facialMorphHasNormals == layout.hasNormals)
            return true;

        const uint64_t texelCount = static_cast<uint64_t>(layout.textureWidth) * layout.textureHeight;
        if (!texelCount || texelCount > (std::numeric_limits<size_t>::max() / 4u))
            return false;
        std::vector<float> texture(static_cast<size_t>(texelCount) * 4u, 0.0f);
        const uint32_t blockRows = layout.hasNormals ? layout.rowsPerTarget / 2u : layout.rowsPerTarget;
        if (!blockRows) return false;
        for (uint32_t targetIndex = 0; targetIndex < layout.targetCount; ++targetIndex)
        {
            const TARGET &target = asset.targets[targetIndex];
            if (target.positions.size() != asset.vertexCount ||
                (layout.hasNormals && target.normals.size() != asset.vertexCount))
                return false;
            const uint32_t targetRow = targetIndex * layout.rowsPerTarget;
            for (uint32_t vertex = 0; vertex < asset.vertexCount; ++vertex)
            {
                const uint32_t column = vertex % layout.textureWidth;
                const uint32_t row = vertex / layout.textureWidth;
                const size_t positionOffset = (static_cast<size_t>(targetRow + row) * layout.textureWidth + column) * 4u;
                const VEC3 &position = target.positions[vertex];
                texture[positionOffset + 0u] = position.x;
                texture[positionOffset + 1u] = position.y;
                texture[positionOffset + 2u] = position.z;
                if (layout.hasNormals)
                {
                    const size_t normalOffset = (static_cast<size_t>(targetRow + blockRows + row) *
                                                  layout.textureWidth + column) * 4u;
                    const VEC3 &normal = target.normals[vertex];
                    texture[normalOffset + 0u] = normal.x;
                    texture[normalOffset + 1u] = normal.y;
                    texture[normalOffset + 2u] = normal.z;
                }
            }
        }

        backend->releaseFacialMorph();
        GLGenTextures(1, &backend->facialMorphTexture);
        if (!backend->facialMorphTexture) return false;
        GLActiveTexture(GL_TEXTURE6);
        GLBindTexture(GL_TEXTURE_2D, backend->facialMorphTexture);
        GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(layout.textureWidth),
                     static_cast<GLsizei>(layout.textureHeight), 0, GL_RGBA, GL_FLOAT, texture.data());
        GLBindTexture(GL_TEXTURE_2D, 0);

        if (buffer->isIndexBuffer())
        {
            std::vector<float> ids(asset.vertexCount);
            for (uint32_t index = 0; index < asset.vertexCount; ++index)
                ids[index] = static_cast<float>(index);
            GLGenBuffers(1, &backend->vboFacialVertexIdIB);
            if (!backend->vboFacialVertexIdIB) return false;
            GLBindBuffer(GL_ARRAY_BUFFER, backend->vboFacialVertexIdIB);
            GLBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(ids.size() * sizeof(float)), ids.data(), GL_STATIC_DRAW);
        }
        else
        {
            backend->facialMorphSubsetCount = buffer->totalSubset;
            backend->vboFacialVertexIdVB = new uint32_t[buffer->totalSubset]();
            GLGenBuffers(static_cast<GLsizei>(buffer->totalSubset), backend->vboFacialVertexIdVB);
            for (uint32_t subset = 0; subset < buffer->totalSubset; ++subset)
            {
                if (!backend->vboFacialVertexIdVB[subset]) return false;
                const uint32_t first = static_cast<uint32_t>(buffer->vertexStartVB[subset]);
                const uint32_t count = static_cast<uint32_t>(buffer->vertexCountVB[subset]);
                std::vector<float> ids(count);
                for (uint32_t index = 0; index < count; ++index)
                    ids[index] = static_cast<float>(first + index);
                GLBindBuffer(GL_ARRAY_BUFFER, backend->vboFacialVertexIdVB[subset]);
                GLBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(ids.size() * sizeof(float)), ids.data(), GL_STATIC_DRAW);
            }
        }
        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        backend->facialMorphTextureWidth = layout.textureWidth;
        backend->facialMorphTextureHeight = layout.textureHeight;
        backend->facialMorphRowsPerTarget = layout.rowsPerTarget;
        backend->facialMorphTargetCount = layout.targetCount;
        backend->facialMorphHasNormals = layout.hasNormals;
        return true;
    }
}

#elif !defined(USE_DIRECTX9) && !defined(USE_DIRECTX11)

#include "facial-gpu-upload.h"

namespace mbm::facial
{
    bool uploadGpuMorphStream(BUFFER_GL *, const ASSET &,
                              const GPU_MORPH_LAYOUT &) noexcept
    {
        return false;
    }
}

#endif
