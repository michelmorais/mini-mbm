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

#if defined (USE_DIRECTX9)

#include <mesh-manager.h>
#include <texture-manager.h>
#include <util-interface.h>
#include <shapes.h>
#include <shader.h>
#include <map>
#include "specific-directx9-buffer.h"
#include <cstring>



namespace mbm
{
    bool MESH_MBM_DEBUG::fillInSubsetDebug(const MESH_MBM* meshMemory,
        const int currentFrame,
        const std::map<int, float>& lsLetterChangedValuesByCurFrameX,
        const std::map<int, float>& lsLetterChangedValuesByCurFrameY,
        util::HEADER_FRAME* headerFrame,
        util::BUFFER_MESH_DEBUG* pBuffer)
    {
        if (!meshMemory || !headerFrame || !pBuffer) return false;
        const auto *frame = meshMemory->getBuffer(static_cast<uint32_t>(currentFrame));
        const auto *buffer = frame ? frame->getRenderBuffer() : nullptr;
        const auto *backend = buffer ? buffer->getBackendBuffer() : nullptr;
        if (!backend || !backend->pVertexBuffer) return false;
        D3DVERTEXBUFFER_DESC description = {};
        if (FAILED(backend->pVertexBuffer->GetDesc(&description)) || (description.Usage & D3DUSAGE_WRITEONLY))
            return log_util::onFailed(nullptr,__FILE__,__LINE__,"DirectX9 extraction requires readable static source geometry");
        const bool normals = buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR ||
                             buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV;
        const bool uv = buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_UV ||
                        buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV;
        const size_t count = buffer->sizeOfArrayVertex;
        const size_t stride = backend->sizeStructVertexInBytes;
        const size_t expectedStride = sizeof(float)*(3u+(normals ? 3u : 0u)+(uv ? 2u : 0u));
        if (stride != expectedStride || count*stride > description.Size) return false;
        headerFrame->sizeVertexBuffer = static_cast<int>(count);
        if (buffer->isIndexBuffer())
        {
            D3DINDEXBUFFER_DESC indexDescription = {};
            const size_t bytes = static_cast<size_t>(headerFrame->sizeIndexBuffer)*sizeof(uint16_t);
            if (!backend->pIndexBuffer || FAILED(backend->pIndexBuffer->GetDesc(&indexDescription)) ||
                indexDescription.Format != D3DFMT_INDEX16 || (indexDescription.Usage & D3DUSAGE_WRITEONLY) ||
                bytes > indexDescription.Size) return false;
            pBuffer->indexBuffer = new uint16_t[headerFrame->sizeIndexBuffer];
            void *data = nullptr;
            if (FAILED(backend->pIndexBuffer->Lock(0,static_cast<UINT>(bytes),&data,D3DLOCK_READONLY))) return false;
            memcpy(pBuffer->indexBuffer,data,bytes);
            if (FAILED(backend->pIndexBuffer->Unlock())) return false;
        }
        for (uint32_t i = 0; i < frame->getTotalSubsets(); ++i)
        {
            const auto *source = frame->getSubset(i);
            auto *subset = new util::SUBSET_DEBUG();
            subset->vertexStart = source->vertexStart;
            subset->vertexCount = source->vertexCount;
            if (buffer->isIndexBuffer())
            {
                subset->indexStart = buffer->indexStartIB[i];
                subset->indexCount = buffer->indexCountIB[i];
            }
            subset->texture = source->texture ? source->texture->getFileNameTexture() : "default";
            pBuffer->subset.push_back(subset);
        }
        pBuffer->position = new float[count*3u];
        if (normals) pBuffer->normal = new float[count*3u];
        if (uv) pBuffer->uv = new float[count*2u];
        void *data = nullptr;
        if (FAILED(backend->pVertexBuffer->Lock(0,static_cast<UINT>(count*stride),&data,D3DLOCK_READONLY))) return false;
        const auto *vertices = static_cast<const uint8_t *>(data);
        for (size_t i = 0; i < count; ++i)
        {
            const auto *vertex = vertices+i*stride;
            memcpy(pBuffer->position+i*3u,vertex,sizeof(float)*3u);
            if (normals) memcpy(pBuffer->normal+i*3u,vertex+sizeof(float)*3u,sizeof(float)*3u);
            if (uv) memcpy(pBuffer->uv+i*2u,vertex+sizeof(float)*(normals ? 6u : 3u),sizeof(float)*2u);
        }
        if (FAILED(backend->pVertexBuffer->Unlock())) return false;
        if (meshMemory->getInfoFont())
        {
            const auto x = lsLetterChangedValuesByCurFrameX.find(currentFrame);
            const auto y = lsLetterChangedValuesByCurFrameY.find(currentFrame);
            if (x == lsLetterChangedValuesByCurFrameX.end() || y == lsLetterChangedValuesByCurFrameY.end()) return false;
            const int maximumFontFrames = static_cast<int>(sizeof(INFO_BOUND_FONT::letterDiffY)/sizeof(float));
            if (currentFrame < maximumFontFrames)
                for (size_t i = 0; i < count; ++i)
                {
                    pBuffer->position[i*3u] += x->second;
                    pBuffer->position[i*3u+1u] += y->second;
                }
        }
        return true;
    }
} //namespace mbm

#endif //USE_DIRECTX9
