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

#if defined USE_DUMMY_BACK_END_ENGINE
#if defined USE_EDITOR_FEATURES

#include "dummy-engine.h" // for compiler_message, you can remove it after implement the functions

#include <mesh-manager.h>
#include <texture-manager.h>
#include <util-interface.h>
#include <shapes.h>
#include <shader.h>
#include <map>



namespace mbm
{
    bool MESH_MBM_DEBUG::loadDebugFromMemory(const MESH_MBM* meshMemory)
    {
        if(meshMemory == nullptr || meshMemory->isLoaded() == false)
            return log_util::onFailed(nullptr,__FILE__, __LINE__, "Mesh empty or not loaded...");
        compiler_message("TODO: check capabilities");
        this->release();
        fileName = meshMemory->getFilenameMesh();
        // step 1: Verificação do header
        // -------------------------------------------------------------------------------
        switch (meshMemory->getTypeMesh())
        {
            case util::TYPE_MESH_3D:       strncpy(headerMain.typeApp,"Mesh 3d mbm",sizeof(headerMain.typeApp)-1);  break;
            case util::TYPE_MESH_SHAPE:    strncpy(headerMain.typeApp,"Shape mbm",sizeof(headerMain.typeApp)-1);    break;
            case util::TYPE_MESH_USER:     strncpy(headerMain.typeApp,"User mbm",sizeof(headerMain.typeApp)-1);     break;
            case util::TYPE_MESH_SPRITE:   strncpy(headerMain.typeApp,"Sprite mbm",sizeof(headerMain.typeApp)-1);   break;
            case util::TYPE_MESH_TILE_MAP: strncpy(headerMain.typeApp,"Tile mbm", sizeof(headerMain.typeApp) - 1);  break;
            case util::TYPE_MESH_FONT:     strncpy(headerMain.typeApp,"Font mbm",sizeof(headerMain.typeApp)-1);     break;
            case util::TYPE_MESH_PARTICLE: strncpy(headerMain.typeApp,"Particle mbm",sizeof(headerMain.typeApp)-1); break;
            default:
                return log_util::onFailed(nullptr,__FILE__, __LINE__, "Mesh invalid type");
            break;
        }
        strncpy(headerMain.name, "mbm",sizeof(headerMain.name)-1);
        headerMain.version = CURRENT_VERSION_MBM_HEADER;
        headerMain.magic = 0x010203ff;
        typeMe = meshMemory->getTypeMesh();
        // step 2: --------------------------------------------------------------------------------------------------
        for(auto pCube : meshMemory->infoPhysics.lsCube)
        {
            auto cube = new CUBE(pCube->halfDim,pCube->absCenter);
            this->infoPhysics.lsCube.push_back(cube);
        }
        for(auto pBase : meshMemory->infoPhysics.lsSphere)
        {
            auto base        = new SPHERE();
            base->absCenter[0] = pBase->absCenter[0];
            base->absCenter[1] = pBase->absCenter[1];
            base->absCenter[2] = pBase->absCenter[2];
            base->ray          = pBase->ray;
            this->infoPhysics.lsSphere.push_back(base);
        }
        for(auto pComplex : meshMemory->infoPhysics.lsCubeComplex)
        {
            auto complex = new CUBE_COMPLEX();
            for(int k=0; k< 8; k++)
                complex->p[k] = pComplex->p[k];
            this->infoPhysics.lsCubeComplex.push_back(complex);
        }
        for(auto pTriangle : meshMemory->infoPhysics.lsTriangle)
        {
            auto triangle = new TRIANGLE();
            triangle->point[0] = pTriangle->point[0];
            triangle->point[1] = pTriangle->point[1];
            triangle->point[2] = pTriangle->point[2];
            this->infoPhysics.lsTriangle.push_back(triangle);
        }
        if(meshMemory->getInfoFont() != nullptr)
        {
            const INFO_BOUND_FONT *   pMemoryInfoFont = meshMemory->getInfoFont();
            headerMain.backBufferHeight     = pMemoryInfoFont->heightLetter;
            this->extraInfo					= new INFO_BOUND_FONT();
            auto *   infoFont	= static_cast<INFO_BOUND_FONT*>(this->extraInfo);
            util::DETAIL_HEADER_FONT headerFont;
            infoFont->fontName          = pMemoryInfoFont->fontName;
            infoFont->heightLetter      = pMemoryInfoFont->heightLetter;
            infoFont->spaceXCharacter   = pMemoryInfoFont->spaceXCharacter;
            infoFont->spaceYCharacter   = pMemoryInfoFont->spaceYCharacter;
            for (std::vector<util::DETAIL_LETTER*>::size_type j = 0; j < 255; ++j)
            {
                const util::DETAIL_LETTER *pDetailFont =  pMemoryInfoFont->letter[j].detail;
                if(pMemoryInfoFont->letter[j].detail)
                {
                    auto detailFont = new util::DETAIL_LETTER();
                    detailFont->heightLetter    = pDetailFont->heightLetter;
                    detailFont->indexFrame      = pDetailFont->indexFrame;
                    detailFont->letter          = pDetailFont->letter;
                    detailFont->widthLetter     = pDetailFont->widthLetter;
                    infoFont->letter[j].detail  = detailFont;
                }
            }
        }
        if(meshMemory->getInfoParticle() != nullptr)
        {
            const std::vector<util::STAGE_PARTICLE*>* thatParticleInfo = meshMemory->getInfoParticle();
            auto* lsParticleInfo = new std::vector<util::STAGE_PARTICLE*>();
            this->extraInfo = lsParticleInfo;
            for (auto thatStage : *thatParticleInfo)
            {
                auto* stage = new util::STAGE_PARTICLE(thatStage);
                lsParticleInfo->push_back(stage);
            }
        }
        if(meshMemory->getInfoTile() != nullptr)
        {
            const util::BTILE_INFO* thatInfoTile = meshMemory->getInfoTile();
            this->extraInfo = thatInfoTile->clone();
        }
        headerMesh.totalAnimation = meshMemory->infoAnimation.lsHeaderAnim.size();
        for(int i=0; i< headerMesh.totalAnimation; ++i)
        {
            const util::INFO_ANIMATION::INFO_HEADER_ANIM* pInfoAnim    = meshMemory->infoAnimation.lsHeaderAnim[i];
            auto  infoHead										= new util::INFO_ANIMATION::INFO_HEADER_ANIM();
            infoHead->headerAnim                                = new util::HEADER_ANIMATION();
            this->infoAnimation.lsHeaderAnim.push_back(infoHead);
            util::HEADER_ANIMATION *headerAnim  = infoHead->headerAnim;
            headerAnim->hasShaderEffect   = pInfoAnim->headerAnim->hasShaderEffect;
            headerAnim->blendState        = pInfoAnim->headerAnim->blendState;
            headerAnim->initialFrame      = pInfoAnim->headerAnim->initialFrame;
            headerAnim->finalFrame        = pInfoAnim->headerAnim->finalFrame;
            headerAnim->timeBetweenFrame  = pInfoAnim->headerAnim->timeBetweenFrame;
            headerAnim->typeAnimation     = pInfoAnim->headerAnim->typeAnimation;
            strncpy(headerAnim->nameAnimation,pInfoAnim->headerAnim->nameAnimation,sizeof(headerAnim->nameAnimation));
            headerAnim->hasShaderEffect             = (uint16_t)(infoHead->effetcShader ? 1 : 0);
            infoHead->headerAnim->blendState        = (uint16_t)headerAnim->blendState;
            //for(auto pInfoStepShader : pInfoAnim->lsStepEffetcShader)
            if(infoHead->effetcShader)
            {
                auto pInfoStepShader = pInfoAnim->effetcShader;
                //each step may has two shaders (PS and VS)
                auto infoStepShader  = new util::INFO_FX();
                infoHead->effetcShader = infoStepShader;
                infoStepShader->blendOperation = pInfoStepShader->blendOperation;
                
                if(pInfoStepShader->dataPS)
                {
                    infoStepShader->dataPS = new util::INFO_SHADER_DATA(
                        pInfoStepShader->dataPS->lenVars * 4,
                        strlen(pInfoStepShader->dataPS->fileNameShader) +1,
                        pInfoStepShader->dataPS->fileNameTextureStage2 ? strlen(pInfoStepShader->dataPS->fileNameTextureStage2) + 1 : 0);
                    strcpy(infoStepShader->dataPS->fileNameShader,pInfoStepShader->dataPS->fileNameShader);
                    if(infoStepShader->dataPS->fileNameTextureStage2)
                        strcpy(infoStepShader->dataPS->fileNameTextureStage2,pInfoStepShader->dataPS->fileNameTextureStage2);
                    infoStepShader->dataPS->timeAnimation = pInfoStepShader->dataPS->timeAnimation;
                    infoStepShader->dataPS->typeAnimation = pInfoStepShader->dataPS->typeAnimation;
                    for(int k=0; k < infoStepShader->dataPS->lenVars; ++k)
                    {
                        const int index = k * 4;
                        memcpy(&infoStepShader->dataPS->max[index],&pInfoStepShader->dataPS->max[index],sizeof(float) * 4);
                        memcpy(&infoStepShader->dataPS->min[index],&pInfoStepShader->dataPS->min[index],sizeof(float) * 4);
                        infoStepShader->dataPS->typeVars[k] = pInfoStepShader->dataPS->typeVars[k];
                    }
                }
                if(pInfoStepShader->dataVS)
                {
                    infoStepShader->dataVS = new util::INFO_SHADER_DATA(
                        pInfoStepShader->dataVS->lenVars * 4,
                        strlen(pInfoStepShader->dataVS->fileNameShader) +1,
                        pInfoStepShader->dataVS->fileNameTextureStage2 ? strlen(pInfoStepShader->dataVS->fileNameTextureStage2) + 1 : 0);
                    strcpy(infoStepShader->dataVS->fileNameShader,pInfoStepShader->dataVS->fileNameShader);
                    if(infoStepShader->dataVS->fileNameTextureStage2)
                        strcpy(infoStepShader->dataVS->fileNameTextureStage2,pInfoStepShader->dataVS->fileNameTextureStage2);
                    infoStepShader->dataVS->timeAnimation = pInfoStepShader->dataVS->timeAnimation;
                    infoStepShader->dataVS->typeAnimation = pInfoStepShader->dataVS->typeAnimation;
                    for(int k=0; k < infoStepShader->dataVS->lenVars; ++k)
                    {
                        const int index = k * 4;
                        memcpy(&infoStepShader->dataVS->max[index],&pInfoStepShader->dataVS->max[index],sizeof(float) * 4);
                        memcpy(&infoStepShader->dataVS->min[index],&pInfoStepShader->dataVS->min[index],sizeof(float) * 4);
                        infoStepShader->dataVS->typeVars[k] = pInfoStepShader->dataVS->typeVars[k];
                    }
                }
            }
        }
        
        headerMesh.totalFrames = meshMemory->getTotalFrame();
        std::map<int,float> lsLetterChangedValuesByLetterX;
        std::map<int,float> lsLetterChangedValuesByCurFrameX;
        std::map<int,float> lsLetterChangedValuesByLetterY;
        std::map<int,float> lsLetterChangedValuesByCurFrameY;
        if(meshMemory->getInfoFont() != nullptr)//TODO
        {
            const INFO_BOUND_FONT * pMemoryInfoFont = meshMemory->getInfoFont();
            const auto sL = static_cast<int>(sizeof(mbm::INFO_BOUND_FONT::letterDiffY) / sizeof(float));

            for(int i=0; i< sL; ++i)
            {
                if(pMemoryInfoFont->letterDiffY[i] != 0.0f)
                {
                    lsLetterChangedValuesByLetterY[i] = pMemoryInfoFont->letterDiffY[i];
                }
                if(pMemoryInfoFont->letterDiffX[i] != 0.0f)
                {
                    lsLetterChangedValuesByLetterX[i] = pMemoryInfoFont->letterDiffX[i];
                }
            }
            for(auto & j : pMemoryInfoFont->letter)
            {
                const util::DETAIL_LETTER *pDetailFont =  j.detail;
                if(pDetailFont)
                {
                    const float x = lsLetterChangedValuesByLetterX[pDetailFont->letter];
                    if(x != 0.0f)
                    {
                        lsLetterChangedValuesByCurFrameX[pDetailFont->indexFrame] = x;
                    }
                    const float y = lsLetterChangedValuesByLetterY[pDetailFont->letter];
                    if(y != 0.0f)
                    {
                        lsLetterChangedValuesByCurFrameY[pDetailFont->indexFrame] = y;
                    }
                }
            }
        }
        for (int currentFrame = 0; currentFrame < headerMesh.totalFrames; ++currentFrame)
        {
            auto pBuffer = new util::BUFFER_MESH_DEBUG();
            this->buffer.push_back(pBuffer);
            // 5 Sequencia lógica dos frames --------------------------------------------------------------------------
            // Cada header Frame
            // --------------------------------------------------------------------------------------------------
            util::HEADER_FRAME *headerFrame    = &pBuffer->headerFrame;
            const BUFFER_MESH* pBufferMesh     = meshMemory->getBuffer(currentFrame);
            const BUFFER_GL* pGl               = pBufferMesh->pBufferGL;
            if(pGl->isIndexBuffer)
            {
                strncpy(headerFrame->typeBuffer,"IB",sizeof(headerFrame->typeBuffer)-1);
                for(uint32_t i=0; i< pBufferMesh->pBufferGL->totalSubset; ++i)
                {
                    headerFrame->sizeIndexBuffer  += pBufferMesh->pBufferGL->indexCountIB[i];
                }
            }
            else
            {
                strncpy(headerFrame->typeBuffer,"VB",sizeof(headerFrame->typeBuffer)-1);
                for(uint32_t i=0; i< pBufferMesh->pBufferGL->totalSubset; ++i)
                {
                    headerFrame->sizeVertexBuffer  += pBufferMesh->pBufferGL->vertexCountVB[i];
                }
            }
            headerFrame->stride = 3;
            // 6 Todos os headers subset deste frame
            // -------------------------------------------------------------------------------
            headerFrame->totalSubset = pBufferMesh->pBufferGL->totalSubset;
            // 6.2 Vertex buffer e index buffer
            // -----------------------------------------------------------------------------------------
            if (headerFrame->sizeIndexBuffer && strcmp(headerFrame->typeBuffer, "IB") == 0)
            {
                pBuffer->indexBuffer = new uint16_t[headerFrame->sizeIndexBuffer];
                uint16_t acumulated = 0;
                for(uint32_t i=0; i< pBufferMesh->pBufferGL->totalSubset; ++i)
                {
                    auto pSubset = new util::SUBSET_DEBUG();
                    pBuffer->subset.push_back(pSubset);
                    uint16_t maxIndexSubset = 0;
                    pSubset->indexStart             = pGl->indexStartIB[i];
                    pSubset->indexCount             = pGl->indexCountIB[i];
                    pBuffer->subset[i]->indexStart  = pSubset->indexStart;
                    pBuffer->subset[i]->indexCount  = pSubset->indexCount;
                    compiler_message("TODO: implement get array from memory");
                    //GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pGl->vboIndexSubsetIB[i]);
                    //auto *indexBuffer = static_cast<uint16_t*>(glMapBufferOES(GL_ELEMENT_ARRAY_BUFFER,GL_WRITE_ONLY_OES));
                    //if(indexBuffer == nullptr)
                    //    return log_util::onFailed(nullptr,__FILE__, __LINE__, "Failed to get index at [glMapBufferOES] [%s]", meshMemory->getFilenameMesh());
                    for(int j=0; j< pSubset->indexCount; ++j)
                    {
                        const int index             = pSubset->indexStart + j;
                        pBuffer->indexBuffer[index] = indexBuffer[j];
                        maxIndexSubset = std::max(pBuffer->indexBuffer[index],maxIndexSubset);
                    }
                    //glUnmapBufferOES(GL_ELEMENT_ARRAY_BUFFER);
                    uint16_t vertexCount  = maxIndexSubset + 1;
                    pSubset->vertexCount            = vertexCount;
                    pSubset->vertexStart            = acumulated;
                    acumulated                      += vertexCount;

                    if(pBufferMesh->subset[i].texture)
                    {
                        pSubset->texture = pBufferMesh->subset[i].texture->getFileNameTexture();
                    }
                }
                headerFrame->sizeVertexBuffer = (acumulated);
                const uint32_t totalUv    = (acumulated) * 2;
                pBuffer->position   = new float[headerFrame->sizeVertexBuffer * 3];
                pBuffer->normal     = new float[headerFrame->sizeVertexBuffer * 3];
                pBuffer->uv         = new float[totalUv];
            }
            // 6.3 Vertex Buffer somente
            // ----------------------------------------------------------------------------------------------
            else if (strcmp(headerFrame->typeBuffer, "VB") == 0)
            {
                const uint32_t totalVertex = (headerFrame->sizeVertexBuffer) * 3;
                const uint32_t totalNormal = (headerFrame->sizeVertexBuffer) * 3;
                const uint32_t totalUv     = (headerFrame->sizeVertexBuffer) * 2;
                pBuffer->position   = new float[totalVertex];
                pBuffer->normal     = new float[totalNormal];
                pBuffer->uv         = new float[totalUv];
            }
            else
            {
                return log_util::onFailed(nullptr,__FILE__, __LINE__, "unknown buffer type [%s]", meshMemory->getFilenameMesh());
            }
            bool is_dynamic_shape   = false;
            float *pPosition        = nullptr;
            float *pNormal          = nullptr;
            float *pTexture         = nullptr;
            const util::DYNAMIC_SHAPE* infoShape =  meshMemory->getInfoShape(); //maybe is dynamic shape
            if(infoShape && infoShape->dynamicVertex)
            {
                pPosition           = infoShape->dynamicVertex;
                pNormal             = infoShape->dynamicNormal;
                pTexture            = infoShape->dynamicNormal;
                is_dynamic_shape    = pPosition != nullptr && pNormal != nullptr && pTexture != nullptr;
                if(is_dynamic_shape == false)
                    return log_util::onFailed(nullptr,__FILE__, __LINE__, "Dynamic shape has nullptr [%s]", meshMemory->getFilenameMesh());
                if(headerFrame->sizeVertexBuffer != static_cast<int>(infoShape->size_vertex / 3))
                    return log_util::onFailed(nullptr,__FILE__, __LINE__, "Dynamic shape has inconsistent vertex buffer [%s] sizeVertexBuffer: [%d] size_vertex [%d] ", meshMemory->getFilenameMesh(),headerFrame->sizeVertexBuffer,infoShape->size_vertex);
                if(headerFrame->sizeVertexBuffer != static_cast<int>(infoShape->size_normal / 3))
                    return log_util::onFailed(nullptr,__FILE__, __LINE__, "Dynamic shape has inconsistent normal buffer [%s] sizeVertexBuffer: [%d] size_normal [%d] ", meshMemory->getFilenameMesh(),headerFrame->sizeVertexBuffer,infoShape->size_normal);
                if(headerFrame->sizeVertexBuffer != static_cast<int>(infoShape->size_uv / 2))
                    return log_util::onFailed(nullptr,__FILE__, __LINE__, "Dynamic shape has inconsistent uv buffer [%s] sizeVertexBuffer: [%d] size_uv [%d] ", meshMemory->getFilenameMesh(),headerFrame->sizeVertexBuffer,infoShape->size_uv);
            }

            if(is_dynamic_shape == false)
            {
                compiler_message("TODO: implement get array from memory");
                //GLBindBuffer(GL_ARRAY_BUFFER,pGl->vboVertNorTexIB[0]);
                //pPosition = static_cast<float*>(glMapBufferOES(GL_ARRAY_BUFFER,GL_WRITE_ONLY_OES));
            }
            if(pPosition == nullptr)
            {
                return log_util::onFailed(nullptr,__FILE__, __LINE__, "Failed to get position at [glMapBufferOES] [%s]", meshMemory->getFilenameMesh());
            }
            memcpy(pBuffer->position,pPosition,sizeof(float) * 3 * static_cast<size_t>(headerFrame->sizeVertexBuffer));
            if(is_dynamic_shape == false)
            {
                //glUnmapBufferOES(GL_ARRAY_BUFFER);
            }
            if(meshMemory->getInfoFont() != nullptr)
            {
                const float letterDiffX = lsLetterChangedValuesByCurFrameX[currentFrame];
                const float letterDiffY = lsLetterChangedValuesByCurFrameY[currentFrame];
                const auto sL = static_cast<int>(sizeof(mbm::INFO_BOUND_FONT::letterDiffY) / sizeof(float));
                if(currentFrame < sL)
                {
                    if(letterDiffX != 0.0f)
                    {
                        const uint32_t ss = 3 * static_cast<size_t>(headerFrame->sizeVertexBuffer);
                        for(uint32_t ii = 0; ii < ss; ii+=3 )// [0 -> x, 1 -> y, 2 -> z] (first x coord == 0)
                        {
                            pBuffer->position[ii] += letterDiffX;
                        }
                    }
                    if(letterDiffY != 0.0f)
                    {
                        const uint32_t ss = 3 * static_cast<size_t>(headerFrame->sizeVertexBuffer);
                        for(uint32_t ii = 1; ii < ss; ii+=3 )// [0 -> x, 1 -> y, 2 -> z] (first y coord == 1)
                        {
                            pBuffer->position[ii] += letterDiffY;
                        }
                    }
                }
            }
            if(is_dynamic_shape == false)
            {
                compiler_message("TODO: implement get array from memory");
                //GLBindBuffer(GL_ARRAY_BUFFER,pGl->vboVertNorTexIB[1]);
                //pNormal   = static_cast<float*>(glMapBufferOES(GL_ARRAY_BUFFER,GL_WRITE_ONLY_OES));
            }
            if(pNormal == nullptr)
            {
                return log_util::onFailed(nullptr,__FILE__, __LINE__, "Failed to get normal at [glMapBufferOES] [%s]", meshMemory->getFilenameMesh());
            }
            memcpy(pBuffer->normal,pNormal,sizeof(float) * 3 * static_cast<size_t>(headerFrame->sizeVertexBuffer));
            if(is_dynamic_shape == false)
            {
                compiler_message("TODO: implement unmap array from memory");
                //glUnmapBufferOES(GL_ARRAY_BUFFER);
                //GLBindBuffer(GL_ARRAY_BUFFER,pGl->vboVertNorTexIB[2]);
                //pTexture  = static_cast<float*>(glMapBufferOES(GL_ARRAY_BUFFER,GL_WRITE_ONLY_OES));
            }
            if(pTexture == nullptr)
            {
                const util::DYNAMIC_SHAPE* infoShape =  meshMemory->getInfoShape(); //maybe is dynamic shape
                if(infoShape == nullptr || infoShape->dynamicUV == nullptr)
                    return log_util::onFailed(nullptr,__FILE__, __LINE__, "Failed to get uv at [glMapBufferOES] [%s]", meshMemory->getFilenameMesh());
                
            }
            memcpy(pBuffer->uv,pTexture,sizeof(float) * 2 * static_cast<size_t>(headerFrame->sizeVertexBuffer));
            if(is_dynamic_shape == false)
            {
                compiler_message("TODO: implement unmap array from memory");
                //glUnmapBufferOES(GL_ARRAY_BUFFER);
            }
        }
        positionOffset = VEC3(headerMesh.posX, headerMesh.posY, headerMesh.posZ);
        angleDefault   = VEC3(headerMesh.angleX, headerMesh.angleY, headerMesh.angleZ);
        this->sizeCoordTexFrame_0 = 0;
        if (this->coordTexFrame_0)
            delete[] this->coordTexFrame_0;
        this->coordTexFrame_0 = nullptr;
        return true;
    }
} //namespace mbm

#endif // USE_EDITOR_FEATURES
#endif //USE_DUMMY_BACK_END_ENGINE