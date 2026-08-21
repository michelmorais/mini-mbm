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

#if defined(USE_DUMMY_BACK_END_ENGINE) || defined(MBM_DIRECTX11_FOUNDATION_STUBS)


#include "dummy-engine.h" // for compiler_message, you can remove it after implement the functions

#include <mesh-manager.h>
#include <texture-manager.h>
#include <util-interface.h>
#include <shapes.h>
#include <shader.h>
#include <map>
#if defined(USE_DIRECTX11)
#include "specific-dummy-buffer.h"
#include "specific-directx11-context.h"
#include <device.h>
#include <algorithm>
#include <cstring>
#endif



namespace mbm
{
#if defined(USE_DIRECTX11)
    namespace
    {
        struct D3D11_DEBUG_VERTEX
        {
            VEC3 position;
            VEC3 normal;
            VEC2 uv;
        };
    }
#endif
    bool MESH_MBM_DEBUG::fillInSubsetDebug(const MESH_MBM* meshMemory,
        const int currentFrame,
        const std::map<int, float>& lsLetterChangedValuesByCurFrameX,
        const std::map<int, float>& lsLetterChangedValuesByCurFrameY,
        util::HEADER_FRAME* headerFrame,
        util::BUFFER_MESH_DEBUG* pBuffer)//need to be implemented by specific backend engine 
    {
#if defined(USE_DIRECTX11)
        if (!meshMemory || !headerFrame || !pBuffer)
            return log_util::onFailed(nullptr, __FILE__, __LINE__, "invalid DirectX11 mesh debug readback arguments");
        const BUFFER_MESH *bufferMesh = meshMemory->getBuffer(static_cast<uint32_t>(currentFrame));
        const BUFFER_GL *buffer = bufferMesh ? bufferMesh->getRenderBuffer() : nullptr;
        BUFFER_SPECIFIC *backend = buffer ? buffer->getBackendBuffer() : nullptr;
        SPECIFIC_AUX_CONTEXT_DEVICE *context = DEVICE::getInstance()->getSpecificContextDevice();
        if (!bufferMesh || !buffer || !backend || !backend->vertexBuffer || !context ||
            !context->device || !context->immediateContext)
            return log_util::onFailed(nullptr, __FILE__, __LINE__, "DirectX11 mesh debug buffer is unavailable");

        const bool hasNormals = buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR ||
                                buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV;
        const bool hasUv = buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_UV ||
                           buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV;
        auto readBuffer = [context](ID3D11Buffer *source, std::vector<uint8_t> &bytes) -> bool
        {
            if (!source)
                return false;
            D3D11_BUFFER_DESC description = {};
            source->GetDesc(&description);
            D3D11_BUFFER_DESC stagingDescription = description;
            stagingDescription.Usage = D3D11_USAGE_STAGING;
            stagingDescription.BindFlags = 0;
            stagingDescription.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            stagingDescription.MiscFlags = 0;
            ID3D11Buffer *staging = nullptr;
            if (FAILED(context->device->CreateBuffer(&stagingDescription, nullptr, &staging)))
                return false;
            context->immediateContext->CopyResource(staging, source);
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            const HRESULT result = context->immediateContext->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
            if (SUCCEEDED(result))
            {
                bytes.resize(description.ByteWidth);
                memcpy(bytes.data(), mapped.pData, description.ByteWidth);
                context->immediateContext->Unmap(staging, 0);
            }
            staging->Release();
            return SUCCEEDED(result);
        };

        if (headerFrame->sizeIndexBuffer && strcmp(headerFrame->typeBuffer, "IB") == 0)
        {
            std::vector<uint8_t> indexBytes;
            if (!readBuffer(backend->indexBuffer, indexBytes) ||
                indexBytes.size() < static_cast<size_t>(headerFrame->sizeIndexBuffer) * sizeof(uint16_t))
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "DirectX11 index-buffer readback failed");
            pBuffer->indexBuffer = new uint16_t[headerFrame->sizeIndexBuffer];
            memcpy(pBuffer->indexBuffer, indexBytes.data(),
                   static_cast<size_t>(headerFrame->sizeIndexBuffer) * sizeof(uint16_t));
            for (uint32_t subsetIndex = 0; subsetIndex < bufferMesh->getTotalSubsets(); ++subsetIndex)
            {
                auto *subset = new util::SUBSET_DEBUG();
                subset->indexStart = buffer->indexStartIB[subsetIndex];
                subset->indexCount = buffer->indexCountIB[subsetIndex];
                const util::SUBSET *runtimeSubset = bufferMesh->getSubset(subsetIndex);
                if (runtimeSubset)
                {
                    subset->vertexStart = runtimeSubset->vertexStart;
                    subset->vertexCount = runtimeSubset->vertexCount;
                    if (runtimeSubset->texture)
                        subset->texture = runtimeSubset->texture->getFileNameTexture();
                }
                pBuffer->subset.push_back(subset);
            }
            headerFrame->sizeVertexBuffer = static_cast<int>(buffer->sizeOfArrayVertex);
        }
        else if (strcmp(headerFrame->typeBuffer, "VB") != 0)
            return log_util::onFailed(nullptr, __FILE__, __LINE__, "unknown DirectX11 buffer type [%s]", headerFrame->typeBuffer);

        const size_t vertexCount = static_cast<size_t>(headerFrame->sizeVertexBuffer);
        pBuffer->position = new float[vertexCount * 3u];
        pBuffer->normal = hasNormals ? new float[vertexCount * 3u] : nullptr;
        pBuffer->uv = new float[vertexCount * 2u];
        const util::DYNAMIC_SHAPE *shape = meshMemory->getInfoShape();
        const bool dynamic = shape && shape->dynamicVertex;
        if (dynamic)
        {
            if (!shape->dynamicUV || shape->size_vertex / 3u != vertexCount ||
                shape->size_uv / 2u != vertexCount ||
                (hasNormals && (!shape->dynamicNormal || shape->size_normal / 3u != vertexCount)))
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "inconsistent DirectX11 dynamic mesh debug buffer");
            memcpy(pBuffer->position, shape->dynamicVertex, vertexCount * sizeof(float) * 3u);
            if (hasNormals)
                memcpy(pBuffer->normal, shape->dynamicNormal, vertexCount * sizeof(float) * 3u);
            memcpy(pBuffer->uv, shape->dynamicUV, vertexCount * sizeof(float) * 2u);
        }
        else
        {
            std::vector<uint8_t> vertexBytes;
            if (!readBuffer(backend->vertexBuffer, vertexBytes) ||
                vertexBytes.size() < vertexCount * backend->vertexStride)
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "DirectX11 vertex-buffer readback failed");
            for (size_t index = 0; index < vertexCount; ++index)
            {
                const D3D11_DEBUG_VERTEX *vertex = reinterpret_cast<const D3D11_DEBUG_VERTEX *>(
                    vertexBytes.data() + (index * backend->vertexStride));
                memcpy(pBuffer->position + (index * 3u), &vertex->position.x, sizeof(float) * 3u);
                if (hasNormals)
                    memcpy(pBuffer->normal + (index * 3u), &vertex->normal.x, sizeof(float) * 3u);
                if (hasUv)
                    memcpy(pBuffer->uv + (index * 2u), &vertex->uv.x, sizeof(float) * 2u);
            }
        }

        if (meshMemory->getInfoFont())
        {
            const auto x = lsLetterChangedValuesByCurFrameX.find(currentFrame);
            const auto y = lsLetterChangedValuesByCurFrameY.find(currentFrame);
            if (x == lsLetterChangedValuesByCurFrameX.end() || y == lsLetterChangedValuesByCurFrameY.end())
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "missing DirectX11 font frame offsets");
            const int maximumFontFrames = static_cast<int>(sizeof(INFO_BOUND_FONT::letterDiffY) / sizeof(float));
            if (currentFrame < maximumFontFrames)
            {
                for (size_t index = 0; index < vertexCount; ++index)
                {
                    pBuffer->position[index * 3u] += x->second;
                    pBuffer->position[(index * 3u) + 1u] += y->second;
                }
            }
        }
        return true;
#else
        REMINDER_TODO
        return log_util::onFailed(nullptr, __FILE__, __LINE__, "Not implemented in Dummy, as workaround, use OPENGL_ES or load mesh from file!");
#endif
    }
} //namespace mbm

#endif // USE_DUMMY_BACK_END_ENGINE
