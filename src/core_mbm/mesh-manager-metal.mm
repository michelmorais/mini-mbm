/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.                                    |
|-----------------------------------------------------------------------------------------------------------------------*/

// Metal mesh-manager — MESH_MBM_DEBUG::fillInSubsetDebug() implementation.
//
// This function is called by mesh-manager.cpp::loadDebugFromMemory() to read
// vertex and index data from GPU buffers back to CPU memory for the in-engine
// mesh debug / editor visualisation.
//
// Metal equivalent of the GL glMapBufferOES approach: since all Metal vertex and
// index buffers are created with MTLResourceStorageModeShared, their .contents
// pointer is directly CPU-readable without any blit or synchronisation command.
//
// The Metal backend uses an interleaved vertex format (see buildInterleavedVB in
// shader-metal.mm), so position / normal / UV are de-interleaved here.

#if defined(USE_METAL)

#import <Metal/Metal.h>

#include <mesh-manager.h>
#include <shader.h>           // BUFFER_GL full definition, FVF_PROVIDE_BY_ENGINE
#include <texture-manager.h>  // TEXTURE::getFileNameTexture()
#include "specific-metal-context.h"
#include "specific-metal-buffer.h"
#include <util-interface.h>
#include <header-mesh.h>      // HEADER_FRAME, BUFFER_MESH_DEBUG, SUBSET_DEBUG
#include <shapes.h>           // INFO_BOUND_FONT::letterDiffY
#include <map>

namespace mbm
{
    bool MESH_MBM_DEBUG::fillInSubsetDebug(
        const MESH_MBM*                   meshMemory,
        const int                          currentFrame,
        const std::map<int, float>&        lsLetterChangedValuesByCurFrameX,
        const std::map<int, float>&        lsLetterChangedValuesByCurFrameY,
        util::HEADER_FRAME*                headerFrame,
        util::BUFFER_MESH_DEBUG*           pBuffer)
    {
        const BUFFER_MESH* pBufferMesh = meshMemory->getBuffer((uint32_t)currentFrame);
        if (!pBufferMesh || !pBufferMesh->getRenderBuffer())
            return log_util::onFailed(nullptr, __FILE__, __LINE__,
                "No buffer for frame %d [%s]", currentFrame, meshMemory->getFilenameMesh());

        const BUFFER_GL*  pGl = pBufferMesh->getRenderBuffer();
        BUFFER_SPECIFIC *backendBuffer = pGl->getBackendBuffer();
        if (!backendBuffer)
            return log_util::onFailed(nullptr, __FILE__, __LINE__,
                "No BUFFER_SPECIFIC for frame %d [%s]", currentFrame, meshMemory->getFilenameMesh());

        const bool hasNormals = (pGl->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR ||
                                 pGl->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
        const bool hasUV      = (pGl->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_UV  ||
                                 pGl->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);

        // ------------------------------------------------------------------
        // Index Buffer path
        // ------------------------------------------------------------------
        if (headerFrame->sizeIndexBuffer && strcmp(headerFrame->typeBuffer, "IB") == 0)
        {
            if (!backendBuffer->indexBuffer)
                return log_util::onFailed(nullptr, __FILE__, __LINE__,
                    "No Metal index buffer for frame %d [%s]",
                    currentFrame, meshMemory->getFilenameMesh());

            const uint16_t* metalIdx =
                static_cast<const uint16_t*>(backendBuffer->indexBuffer.contents);
            if (!metalIdx)
                return log_util::onFailed(nullptr, __FILE__, __LINE__,
                    "Metal index buffer has no CPU contents [%s]",
                    meshMemory->getFilenameMesh());

            pBuffer->indexBuffer = new uint16_t[(size_t)headerFrame->sizeIndexBuffer];
            memcpy(pBuffer->indexBuffer, metalIdx,
                   (size_t)headerFrame->sizeIndexBuffer * sizeof(uint16_t));

            uint16_t accumulated = 0;
            const uint32_t totalSubsets = pBufferMesh->getTotalSubsets();
            for (uint32_t i = 0; i < totalSubsets; ++i)
            {
                auto* pSubset = new util::SUBSET_DEBUG();
                pBuffer->subset.push_back(pSubset);
                pSubset->indexStart = pGl->indexStartIB[i];
                pSubset->indexCount = pGl->indexCountIB[i];

                uint16_t maxIdx = 0;
                for (int j = 0; j < pSubset->indexCount; ++j)
                    maxIdx = std::max(pBuffer->indexBuffer[pSubset->indexStart + j], maxIdx);

                pSubset->vertexCount = (int)(maxIdx + 1);
                pSubset->vertexStart = (int)accumulated;
                accumulated += (uint16_t)pSubset->vertexCount;

                const util::SUBSET *runtimeSubset = pBufferMesh->getSubset(i);
                if (runtimeSubset && runtimeSubset->texture)
                    pSubset->texture =
                        runtimeSubset->texture->getFileNameTexture();
            }
            headerFrame->sizeVertexBuffer = (int)accumulated;
            pBuffer->position = new float[(size_t)headerFrame->sizeVertexBuffer * 3];
            pBuffer->normal   = hasNormals
                ? new float[(size_t)headerFrame->sizeVertexBuffer * 3] : nullptr;
            pBuffer->uv       = new float[(size_t)headerFrame->sizeVertexBuffer * 2];
        }
        // ------------------------------------------------------------------
        // Vertex Buffer only path
        // ------------------------------------------------------------------
        else if (strcmp(headerFrame->typeBuffer, "VB") == 0)
        {
            pBuffer->position = new float[(size_t)headerFrame->sizeVertexBuffer * 3];
            pBuffer->normal   = hasNormals
                ? new float[(size_t)headerFrame->sizeVertexBuffer * 3] : nullptr;
            pBuffer->uv       = new float[(size_t)headerFrame->sizeVertexBuffer * 2];
        }
        else
        {
            return log_util::onFailed(nullptr, __FILE__, __LINE__,
                "Unknown buffer type [%s] [%s]",
                headerFrame->typeBuffer, meshMemory->getFilenameMesh());
        }

        // ------------------------------------------------------------------
        // Read vertex data
        // ------------------------------------------------------------------
        const util::DYNAMIC_SHAPE* infoShape = meshMemory->getInfoShape();
        const bool isDynamic = infoShape && infoShape->dynamicVertex;

        if (isDynamic)
        {
            // Dynamic shapes (fonts, skinned meshes) keep CPU-side arrays — use directly.
            if (!infoShape->dynamicVertex || !infoShape->dynamicUV)
                return log_util::onFailed(nullptr, __FILE__, __LINE__,
                    "Dynamic shape has nullptr vertex/uv [%s]",
                    meshMemory->getFilenameMesh());

            memcpy(pBuffer->position, infoShape->dynamicVertex,
                   sizeof(float) * 3 * (size_t)headerFrame->sizeVertexBuffer);
            if (hasNormals && pBuffer->normal && infoShape->dynamicNormal)
                memcpy(pBuffer->normal, infoShape->dynamicNormal,
                       sizeof(float) * 3 * (size_t)headerFrame->sizeVertexBuffer);
            if (hasUV)
                memcpy(pBuffer->uv, infoShape->dynamicUV,
                       sizeof(float) * 2 * (size_t)headerFrame->sizeVertexBuffer);
        }
        else
        {
            // Static meshes: read from MTLBuffer (.contents is valid for Shared storage).
            if (!backendBuffer->vertexBuffer)
                return log_util::onFailed(nullptr, __FILE__, __LINE__,
                    "No Metal vertex buffer for frame %d [%s]",
                    currentFrame, meshMemory->getFilenameMesh());

            const uint8_t* raw =
                static_cast<const uint8_t*>(backendBuffer->vertexBuffer.contents);
            if (!raw)
                return log_util::onFailed(nullptr, __FILE__, __LINE__,
                    "Metal vertex buffer has no CPU contents [%s]",
                    meshMemory->getFilenameMesh());

            // Interleaved layout (matches buildInterleavedVB in shader-metal.mm):
            //   float3 position  (floats 0-2)
            //   float3 normal    (floats 3-5, only when hasNormals)
            //   float2 uv        (floats 3-4 or 6-7 depending on hasNormals)
            const int uvOffset = hasNormals ? 6 : 3; // float index of first UV component
            NSUInteger stride;
            switch (pGl->fvf)
            {
                case FVF_PROVIDE_BY_ENGINE::FVF_POS:        stride = 12; break;
                case FVF_PROVIDE_BY_ENGINE::FVF_POS_UV:     stride = 20; break;
                case FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR:    stride = 24; break;
                case FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV: stride = 32; break;
                default:                                     stride = 12; break;
            }

            const int n = headerFrame->sizeVertexBuffer;
            for (int vi = 0; vi < n; ++vi)
            {
                const float* v =
                    reinterpret_cast<const float*>(raw + (size_t)vi * stride);
                pBuffer->position[vi * 3 + 0] = v[0];
                pBuffer->position[vi * 3 + 1] = v[1];
                pBuffer->position[vi * 3 + 2] = v[2];
                if (hasNormals && pBuffer->normal)
                {
                    pBuffer->normal[vi * 3 + 0] = v[3];
                    pBuffer->normal[vi * 3 + 1] = v[4];
                    pBuffer->normal[vi * 3 + 2] = v[5];
                }
                if (hasUV)
                {
                    pBuffer->uv[vi * 2 + 0] = v[uvOffset];
                    pBuffer->uv[vi * 2 + 1] = v[uvOffset + 1];
                }
            }
        }

        // ------------------------------------------------------------------
        // Font letter offset adjustments (same logic as OpenGL backend)
        // ------------------------------------------------------------------
        if (meshMemory->getInfoFont() != nullptr)
        {
            auto itX = lsLetterChangedValuesByCurFrameX.find(currentFrame);
            auto itY = lsLetterChangedValuesByCurFrameY.find(currentFrame);
            if (itX == lsLetterChangedValuesByCurFrameX.end())
                return log_util::onFailed(nullptr, __FILE__, __LINE__,
                    "Missing letterDiffX for frame %d [%s]",
                    currentFrame, meshMemory->getFilenameMesh());
            if (itY == lsLetterChangedValuesByCurFrameY.end())
                return log_util::onFailed(nullptr, __FILE__, __LINE__,
                    "Missing letterDiffY for frame %d [%s]",
                    currentFrame, meshMemory->getFilenameMesh());

            const float dx = itX->second;
            const float dy = itY->second;
            const auto  sL =
                static_cast<int>(sizeof(mbm::INFO_BOUND_FONT::letterDiffY) / sizeof(float));
            if (currentFrame < sL)
            {
                const int total = headerFrame->sizeVertexBuffer;
                if (dx != 0.0f)
                    for (int ii = 0; ii < total; ++ii)
                        pBuffer->position[ii * 3] += dx;
                if (dy != 0.0f)
                    for (int ii = 0; ii < total; ++ii)
                        pBuffer->position[ii * 3 + 1] += dy;
            }
        }

        return true;
    }

} // namespace mbm

#endif // USE_METAL
