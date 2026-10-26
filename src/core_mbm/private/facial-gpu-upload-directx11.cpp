/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                             |
|-----------------------------------------------------------------------------------------------------------------------*/

#if defined(USE_DIRECTX11)

#include "facial-gpu-upload.h"
#include "specific-directx11-buffer.h"
#include "specific-directx11-context.h"
#include <core_mbm/device.h>
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
        DEVICE *device = DEVICE::getInstance();
        if (!backend || !device || !device->getSpecificContextDevice() ||
            buffer->sizeOfArrayVertex != asset.vertexCount)
            return false;
        const bool facialIdsReady = buffer->isIndexBuffer() ?
            backend->facialMorphVertexIdBuffer != nullptr :
            backend->facialMorphVertexIdBufferVB.size() == buffer->totalSubset;
        if (backend->facialMorphTexture && backend->facialMorphTextureView && facialIdsReady &&
            backend->facialMorphTextureWidth == layout.textureWidth &&
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

        ID3D11Device *d3d = device->getSpecificContextDevice()->device;
        D3D11_TEXTURE2D_DESC textureDescription = {};
        textureDescription.Width = layout.textureWidth;
        textureDescription.Height = layout.textureHeight;
        textureDescription.MipLevels = 1;
        textureDescription.ArraySize = 1;
        textureDescription.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        textureDescription.SampleDesc.Count = 1;
        textureDescription.Usage = D3D11_USAGE_DEFAULT;
        textureDescription.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA textureData = {};
        textureData.pSysMem = texture.data();
        textureData.SysMemPitch = layout.textureWidth * 4u * sizeof(float);
        ID3D11Texture2D *newTexture = nullptr;
        ID3D11ShaderResourceView *newView = nullptr;
        D3D11_SHADER_RESOURCE_VIEW_DESC viewDescription = {};
        viewDescription.Format = textureDescription.Format;
        viewDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        viewDescription.Texture2D.MipLevels = 1;
        if (FAILED(d3d->CreateTexture2D(&textureDescription, &textureData, &newTexture)) ||
            FAILED(d3d->CreateShaderResourceView(newTexture, &viewDescription, &newView)))
        {
            if (newView) newView->Release();
            if (newTexture) newTexture->Release();
            return false;
        }

        ID3D11Buffer *newIndexedIdBuffer = nullptr;
        std::vector<ID3D11Buffer *> newSubsetIdBuffers(buffer->totalSubset, nullptr);
        const auto createIdBuffer = [d3d](uint32_t first, uint32_t count, ID3D11Buffer **out) {
            if (!count || !out || count > (std::numeric_limits<UINT>::max() / sizeof(float)))
                return false;
            std::vector<float> ids(count);
            for (uint32_t index = 0; index < count; ++index)
                ids[index] = static_cast<float>(first + index);
            D3D11_BUFFER_DESC description = {};
            description.ByteWidth = static_cast<UINT>(ids.size() * sizeof(float));
            description.Usage = D3D11_USAGE_DEFAULT;
            description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            D3D11_SUBRESOURCE_DATA data = {};
            data.pSysMem = ids.data();
            return SUCCEEDED(d3d->CreateBuffer(&description, &data, out));
        };
        bool idsReady = false;
        if (buffer->isIndexBuffer())
            idsReady = createIdBuffer(0, asset.vertexCount, &newIndexedIdBuffer);
        else
        {
            idsReady = true;
            for (uint32_t subset = 0; subset < buffer->totalSubset; ++subset)
            {
                const uint32_t first = static_cast<uint32_t>(buffer->vertexStartVB[subset]);
                const uint32_t count = static_cast<uint32_t>(buffer->vertexCountVB[subset]);
                if (!createIdBuffer(first, count, &newSubsetIdBuffers[subset]))
                {
                    idsReady = false;
                    break;
                }
            }
        }
        if (!idsReady)
        {
            if (newIndexedIdBuffer) newIndexedIdBuffer->Release();
            for (ID3D11Buffer *ids : newSubsetIdBuffers)
                if (ids) ids->Release();
            newView->Release();
            newTexture->Release();
            return false;
        }

        backend->releaseFacialMorph();
        backend->facialMorphTexture = newTexture;
        backend->facialMorphTextureView = newView;
        backend->facialMorphVertexIdBuffer = newIndexedIdBuffer;
        backend->facialMorphVertexIdBufferVB.swap(newSubsetIdBuffers);
        backend->facialMorphTextureWidth = layout.textureWidth;
        backend->facialMorphTextureHeight = layout.textureHeight;
        backend->facialMorphRowsPerTarget = layout.rowsPerTarget;
        backend->facialMorphTargetCount = layout.targetCount;
        backend->facialMorphHasNormals = layout.hasNormals;
        return true;
    }
}

#endif
