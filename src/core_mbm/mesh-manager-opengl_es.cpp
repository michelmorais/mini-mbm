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
#include <string>
#include <GLES2/gl2ext.h>

#include <map>



namespace mbm
{
    #if defined ANDROID //ANDROID //TODO fix issue not found EGL lib on ANDOID 
        typedef void* (PFNGLMAPBUFFEROESPROC_TODO)       (GLenum target, GLenum access);
        typedef GLboolean (PFNGLUNMAPBUFFEROESPROC_TODO) (GLenum target);
    #endif

        bool MESH_MBM_DEBUG::fillInSubsetDebug(const MESH_MBM* meshMemory,
                                               const int currentFrame,
                                               const std::map<int, float>& lsLetterChangedValuesByCurFrameX,
                                               const std::map<int, float>& lsLetterChangedValuesByCurFrameY,
                                               util::HEADER_FRAME* headerFrame,
                                               util::BUFFER_MESH_DEBUG* pBuffer)//need to be implemented by specific backend engine 
        {
            const BUFFER_MESH* pBufferMesh = meshMemory->getBuffer(currentFrame);
            const BUFFER_GL* pGl           = pBufferMesh ? pBufferMesh->getRenderBuffer() : nullptr;
            BUFFER_SPECIFIC *backendBuffer = pGl ? pGl->getBackendBuffer() : nullptr;
            if (!backendBuffer)
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "backend buffer is null");
            auto* extensionString          = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
            if (strstr(extensionString, "GL_OES_mapbuffer") == nullptr)
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "extension [GL_OES_mapbuffer] not supported!");
    #if defined ANDROID //ANDROID //TODO fix issue not found EGL lib on ANDOID 
            PRINT_IF_DEBUG("loadDebugFromMemory is not working on ANDOID");
            PRINT_IF_DEBUG("TODO: fix issue not found EGL lib on ANDOID");
            PFNGLMAPBUFFEROESPROC_TODO* glMapBufferOES = nullptr;
            PFNGLUNMAPBUFFEROESPROC_TODO* glUnmapBufferOES = nullptr;
    #else //ANDROID //TODO fix issue not found EGL lib on ANDOID 
            auto glMapBufferOES   = (PFNGLMAPBUFFEROESPROC)eglGetProcAddress("glMapBufferOES");
            auto glUnmapBufferOES = (PFNGLUNMAPBUFFEROESPROC)eglGetProcAddress("glUnmapBufferOES");
    #endif
            if (glMapBufferOES == nullptr)
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "extension [glMapBufferOES] not supported!");
            if (glUnmapBufferOES == nullptr)
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "extension [glUnmapBufferOES] not supported!");
            // 6.2 Vertex buffer e index buffer
                // -----------------------------------------------------------------------------------------
            if (headerFrame->sizeIndexBuffer && strcmp(headerFrame->typeBuffer, "IB") == 0)
            {
                pBuffer->indexBuffer = new uint16_t[headerFrame->sizeIndexBuffer];
                uint16_t acumulated = 0;
                const uint32_t totalSubsets = pBufferMesh->getTotalSubsets();
                for (uint32_t i = 0; i < totalSubsets; ++i)
                {
                    auto pSubset = new util::SUBSET_DEBUG();
                    pBuffer->subset.push_back(pSubset);
                    uint16_t maxIndexSubset = 0;
                    pSubset->indexStart = pGl->indexStartIB[i];
                    pSubset->indexCount = pGl->indexCountIB[i];
                    pBuffer->subset[i]->indexStart = pSubset->indexStart;
                    pBuffer->subset[i]->indexCount = pSubset->indexCount;
                    GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, backendBuffer->vboIndexSubsetIB[i]);
                    auto* indexBuffer = static_cast<uint16_t*>(glMapBufferOES(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY_OES));
                    if (indexBuffer == nullptr)
                        return log_util::onFailed(nullptr, __FILE__, __LINE__, "Failed to get index at [glMapBufferOES] ");
                    for (int j = 0; j < pSubset->indexCount; ++j)
                    {
                        const int index = pSubset->indexStart + j;
                        pBuffer->indexBuffer[index] = indexBuffer[j];
                        maxIndexSubset = std::max(pBuffer->indexBuffer[index], maxIndexSubset);
                    }
                    glUnmapBufferOES(GL_ELEMENT_ARRAY_BUFFER);
                    uint16_t vertexCount = maxIndexSubset + 1;
                    pSubset->vertexCount = vertexCount;
                    pSubset->vertexStart = acumulated;
                    acumulated += vertexCount;

                    const util::SUBSET *runtimeSubset = pBufferMesh->getSubset(i);
                    if (runtimeSubset && runtimeSubset->texture)
                    {
                        pSubset->texture = runtimeSubset->texture->getFileNameTexture();
                    }
                }
                headerFrame->sizeVertexBuffer = (acumulated);
                const uint32_t totalUv = (acumulated) * 2;
                pBuffer->position = new float[headerFrame->sizeVertexBuffer * 3];
                const bool hasNormals = (pGl->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR || pGl->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
                pBuffer->normal = hasNormals ? new float[headerFrame->sizeVertexBuffer * 3] : nullptr;
                pBuffer->uv = new float[totalUv];
            }
            // 6.3 Vertex Buffer somente
            // ----------------------------------------------------------------------------------------------
            else if (strcmp(headerFrame->typeBuffer, "VB") == 0)
            {
                const uint32_t totalVertex = (headerFrame->sizeVertexBuffer) * 3;
                const uint32_t totalNormal = (headerFrame->sizeVertexBuffer) * 3;
                const uint32_t totalUv = (headerFrame->sizeVertexBuffer) * 2;
                pBuffer->position = new float[totalVertex];
                const bool hasNormals = (pGl->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR || pGl->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
                pBuffer->normal = hasNormals ? new float[totalNormal] : nullptr;
                pBuffer->uv = new float[totalUv];
            }
            else
            {
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "unknown buffer type [%s]", headerFrame->typeBuffer);
            }

            bool is_dynamic_shape = false;
            float* pPosition = nullptr;
            float* pNormal = nullptr;
            float* pTexture = nullptr;
            const util::DYNAMIC_SHAPE* infoShape = meshMemory->getInfoShape(); //maybe is dynamic shape
            if (infoShape && infoShape->dynamicVertex)
            {
                pPosition = infoShape->dynamicVertex;
                pNormal = (infoShape->size_normal > 0) ? infoShape->dynamicNormal : nullptr;
                pTexture = infoShape->dynamicUV;
                is_dynamic_shape = pPosition != nullptr && pTexture != nullptr;
                if (is_dynamic_shape == false)
                    return log_util::onFailed(nullptr, __FILE__, __LINE__, "Dynamic shape has nullptr (vertex or uv) [%s]", meshMemory->getFilenameMesh());
                if (headerFrame->sizeVertexBuffer != static_cast<int>(infoShape->size_vertex / 3))
                    return log_util::onFailed(nullptr, __FILE__, __LINE__, "Dynamic shape has inconsistent vertex buffer [%s] sizeVertexBuffer: [%d] size_vertex [%d] ", meshMemory->getFilenameMesh(), headerFrame->sizeVertexBuffer, infoShape->size_vertex);
                if (infoShape->size_normal > 0 && headerFrame->sizeVertexBuffer != static_cast<int>(infoShape->size_normal / 3))
                    return log_util::onFailed(nullptr, __FILE__, __LINE__, "Dynamic shape has inconsistent normal buffer [%s] sizeVertexBuffer: [%d] size_normal [%d] ", meshMemory->getFilenameMesh(), headerFrame->sizeVertexBuffer, infoShape->size_normal);
                if (headerFrame->sizeVertexBuffer != static_cast<int>(infoShape->size_uv / 2))
                    return log_util::onFailed(nullptr, __FILE__, __LINE__, "Dynamic shape has inconsistent uv buffer [%s] sizeVertexBuffer: [%d] size_uv [%d] ", meshMemory->getFilenameMesh(), headerFrame->sizeVertexBuffer, infoShape->size_uv);
            }
            
            if (is_dynamic_shape == false)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboVertNorTexIB[0]);
                pPosition = static_cast<float*>(glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES));
            }
            if (pPosition == nullptr)
            {
                return log_util::onFailed(nullptr, __FILE__, __LINE__, "Failed to get position at [glMapBufferOES] [%s]", meshMemory->getFilenameMesh());
            }
            memcpy(pBuffer->position, pPosition, sizeof(float) * 3 * static_cast<size_t>(headerFrame->sizeVertexBuffer));
            if (is_dynamic_shape == false)
                glUnmapBufferOES(GL_ARRAY_BUFFER);

            if (meshMemory->getInfoFont() != nullptr)
            {
                auto itFrameX = lsLetterChangedValuesByCurFrameX.find(currentFrame);
                if (itFrameX == lsLetterChangedValuesByCurFrameX.end())
                {
                    return log_util::onFailed(nullptr, __FILE__, __LINE__, "Failed to find letterDiffX for currentFrame [%d] [%s]", currentFrame, meshMemory->getFilenameMesh());
                }
                auto itFrameY = lsLetterChangedValuesByCurFrameY.find(currentFrame);
                if (itFrameY == lsLetterChangedValuesByCurFrameY.end())
                {
                    return log_util::onFailed(nullptr, __FILE__, __LINE__, "Failed to find letterDiffY for currentFrame [%d] [%s]", currentFrame, meshMemory->getFilenameMesh());
                }
                const float letterDiffX = itFrameX->second;
                const float letterDiffY = itFrameY->second;
                const auto sL = static_cast<int>(sizeof(mbm::INFO_BOUND_FONT::letterDiffY) / sizeof(float));
                if (currentFrame < sL)
                {
                    if (letterDiffX != 0.0f)
                    {
                        const uint32_t ss = 3 * static_cast<size_t>(headerFrame->sizeVertexBuffer);
                        for (uint32_t ii = 0; ii < ss; ii += 3)// [0 -> x, 1 -> y, 2 -> z] (first x coord == 0)
                        {
                            pBuffer->position[ii] += letterDiffX;
                        }
                    }
                    if (letterDiffY != 0.0f)
                    {
                        const uint32_t ss = 3 * static_cast<size_t>(headerFrame->sizeVertexBuffer);
                        for (uint32_t ii = 1; ii < ss; ii += 3)// [0 -> x, 1 -> y, 2 -> z] (first y coord == 1)
                        {
                            pBuffer->position[ii] += letterDiffY;
                        }
                    }
                }
            }
            const bool hasNormals = (pBuffer->normal != nullptr);
            if (hasNormals)
            {
                if (is_dynamic_shape == false)
                {
                    GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboVertNorTexIB[1]);
                    pNormal = static_cast<float*>(glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES));
                }
                if (pNormal == nullptr)
                {
                    return log_util::onFailed(nullptr, __FILE__, __LINE__, "Failed to get normal at [glMapBufferOES] [%s]", meshMemory->getFilenameMesh());
                }
                memcpy(pBuffer->normal, pNormal, sizeof(float) * 3 * static_cast<size_t>(headerFrame->sizeVertexBuffer));
                if (is_dynamic_shape == false)
                {
                    glUnmapBufferOES(GL_ARRAY_BUFFER);
                }
            }
            if (is_dynamic_shape == false)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboVertNorTexIB[2]);
                pTexture = static_cast<float*>(glMapBufferOES(GL_ARRAY_BUFFER, GL_WRITE_ONLY_OES));
            }
            if (pTexture == nullptr)
            {
                const util::DYNAMIC_SHAPE* infoShape = meshMemory->getInfoShape(); //maybe is dynamic shape
                if (infoShape == nullptr || infoShape->dynamicUV == nullptr)
                    return log_util::onFailed(nullptr, __FILE__, __LINE__, "Failed to get uv at [glMapBufferOES] [%s]", meshMemory->getFilenameMesh());

            }
            memcpy(pBuffer->uv, pTexture, sizeof(float) * 2 * static_cast<size_t>(headerFrame->sizeVertexBuffer));
            if (is_dynamic_shape == false)
                glUnmapBufferOES(GL_ARRAY_BUFFER);
            return true;
        }
} //namespace mbm

#endif //USE_OPENGL_ES
