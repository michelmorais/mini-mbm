/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                             |
|-----------------------------------------------------------------------------------------------------------------------*/

#if defined(USE_DIRECTX9)

#include "facial-gpu-upload.h"
#include "specific-directx9-buffer.h"
#include "specific-directx9-context.h"
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
        if (backend->facialMorphTexture &&
            backend->facialMorphTextureWidth == layout.textureWidth &&
            backend->facialMorphTextureHeight == layout.textureHeight &&
            backend->facialMorphRowsPerTarget == layout.rowsPerTarget &&
            backend->facialMorphTargetCount == layout.targetCount &&
            backend->facialMorphHasNormals == layout.hasNormals &&
            (buffer->isIndexBuffer() ? backend->facialMorphVertexIdBufferIB != nullptr :
                                       backend->facialMorphVertexIdBufferVB.size() == buffer->totalSubset))
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

        IDirect3DDevice9 *d3d = device->getSpecificContextDevice()->pd3dDevice;
        IDirect3DTexture9 *newTexture = nullptr;
        if (FAILED(d3d->CreateTexture(layout.textureWidth, layout.textureHeight, 1, 0,
                                      D3DFMT_A32B32G32R32F, D3DPOOL_MANAGED, &newTexture, nullptr)))
            return false;
        D3DLOCKED_RECT locked = {};
        if (FAILED(newTexture->LockRect(0, &locked, nullptr, 0)))
        {
            newTexture->Release();
            return false;
        }
        const size_t rowBytes = static_cast<size_t>(layout.textureWidth) * 4u * sizeof(float);
        for (uint32_t row = 0; row < layout.textureHeight; ++row)
            memcpy(static_cast<uint8_t *>(locked.pBits) + static_cast<size_t>(row) * locked.Pitch,
                   texture.data() + static_cast<size_t>(row) * layout.textureWidth * 4u, rowBytes);
        newTexture->UnlockRect(0);

        IDirect3DVertexBuffer9 *newIndexedIds = nullptr;
        std::vector<IDirect3DVertexBuffer9 *> newSubsetIds(buffer->totalSubset, nullptr);
        const auto createIdBuffer = [d3d](uint32_t first, uint32_t count, IDirect3DVertexBuffer9 **out) {
            if (!count || FAILED(d3d->CreateVertexBuffer(count * sizeof(float), D3DUSAGE_WRITEONLY,
                                                         0, D3DPOOL_MANAGED, out, nullptr)))
                return false;
            void *destination = nullptr;
            if (FAILED((*out)->Lock(0, count * sizeof(float), &destination, 0)))
            {
                (*out)->Release();
                *out = nullptr;
                return false;
            }
            float *ids = static_cast<float *>(destination);
            for (uint32_t index = 0; index < count; ++index)
                ids[index] = static_cast<float>(first + index);
            if (SUCCEEDED((*out)->Unlock()))
                return true;
            (*out)->Release();
            *out = nullptr;
            return false;
        };
        bool idsReady = false;
        if (buffer->isIndexBuffer())
            idsReady = createIdBuffer(0, asset.vertexCount, &newIndexedIds);
        else
        {
            idsReady = true;
            for (uint32_t subset = 0; subset < buffer->totalSubset; ++subset)
            {
                const uint32_t first = static_cast<uint32_t>(buffer->vertexStartVB[subset]);
                const uint32_t count = static_cast<uint32_t>(buffer->vertexCountVB[subset]);
                if (!createIdBuffer(first, count, &newSubsetIds[subset]))
                {
                    idsReady = false;
                    break;
                }
            }
        }
        if (!idsReady)
        {
            if (newIndexedIds) newIndexedIds->Release();
            for (IDirect3DVertexBuffer9 *ids : newSubsetIds)
                if (ids) ids->Release();
            newTexture->Release();
            return false;
        }

        backend->releaseFacialMorph();
        backend->facialMorphTexture = newTexture;
        backend->facialMorphVertexIdBufferIB = newIndexedIds;
        backend->facialMorphVertexIdBufferVB.swap(newSubsetIds);
        backend->facialMorphTextureWidth = layout.textureWidth;
        backend->facialMorphTextureHeight = layout.textureHeight;
        backend->facialMorphRowsPerTarget = layout.rowsPerTarget;
        backend->facialMorphTargetCount = layout.targetCount;
        backend->facialMorphHasNormals = layout.hasNormals;
        return true;
    }
}

#endif
