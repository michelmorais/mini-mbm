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
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

#include <particle.h>

#if defined(USE_OPENGL_ES)

#include <texture-manager.h>
#include <header-mesh.h>
#include <specific-opengl_es.h>
#include <mesh-manager.h>
#include <util-interface.h>
#include <shader-var-cfg.h>
#include <core_mbm/scene.h>
#include <climits>

#if (defined _DEBUG || defined DEBUG_RESTORE)
    #include <log-util.h>
#endif


namespace mbm
{
   void PARTICLE::release()
    {
        this->releaseAnimation();
        this->enableRender = false;
        if (this->vboIndexBuffer)
        {
            GLDeleteBuffers(1, &this->vboIndexBuffer);
        }
        this->vboIndexBuffer = 0;

        if (this->buffer)
            delete[] this->buffer;
        this->buffer = nullptr;
        if (this->particles)
            delete[] this->particles;
        this->particles = nullptr;

        this->totalAlive            = 0;
        this->lenArrayParticlesData = 0;
        this->indexStage = 0;
        for (unsigned int i = 0; i < this->lsParticleStage.size(); ++i)
        {
            util::STAGE_PARTICLE* sPart = lsParticleStage[i];
            delete sPart;
        }
        lsParticleStage.clear();
        this->currentTimeArise = 0.0f;
    }

    bool PARTICLE::load(const char* fileNameTextureOrMesh, const char* operatorShader, const char* newCodeLine, const unsigned int sizeOfParticle, const bool initializeParticleData)
    {
        this->release();
        unsigned int             totalParticleToLoad = sizeOfParticle ? sizeOfParticle : 1;
        const unsigned short int index[6]            = {0, 1, 2, 2, 1, 3};
        const unsigned int       sizeIndexBuffer     = sizeof(index);
        GLGenBuffers(1, &this->vboIndexBuffer);
        if (!this->vboIndexBuffer)
            return false;
        this->texture = nullptr;
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexBuffer);
        GLBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeIndexBuffer, index, GL_STATIC_DRAW);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        fileNameTextureOrMesh = fileNameTextureOrMesh ? fileNameTextureOrMesh : "#FFFFFFFF";
        operatorShader = operatorShader ? operatorShader : "*";
        const size_t lFile = strlen(fileNameTextureOrMesh);
        if (lFile > 4 && strcasecmp(&fileNameTextureOrMesh[lFile - 3], "ptl") == 0)//is particle from mesh
        {
            MESH_MBM* mesh = MESH_MANAGER::getInstance()->load(fileNameTextureOrMesh);
            if (mesh == nullptr)
                return false;
            this->texture = mesh->getTexture(0, 0);
            const auto lsParticleInfo = mesh->getInfoParticle();
            if(lsParticleInfo == nullptr)
            {
                ERROR_LOG( "type of file is not particle!\ntype: %s",MESH_MANAGER::typeClassName(mesh->getTypeMesh()));
                return false;
            }
            for (auto & i : *lsParticleInfo)
            {
                auto  sPart = new util::STAGE_PARTICLE(i);
                this->lsParticleStage.push_back(sPart);
            }
            char newOperator[2] = {'*',0};
            if(this->lsParticleStage.size())
                newOperator[0] = this->lsParticleStage[0]->_operator;
            if (!this->createAnimationAndShader2Particle(newOperator, newCodeLine))
            {
                ERROR_AT(__LINE__,__FILE__, "error on add animation!!");
                return false;
            }
            ANIMATION* anim = this->getAnimation();
            util::INFO_ANIMATION::INFO_HEADER_ANIM* infoHead = mesh->infoAnimation.lsHeaderAnim[0];
            if (anim && mesh->infoAnimation.lsHeaderAnim.size() && infoHead->headerAnim)
            {
                anim->blendState = static_cast<BLEND_STATE>(infoHead->headerAnim->blendState);
                if (infoHead->effetcShader)
                {
                    anim->fx.blendOperation = infoHead->effetcShader->blendOperation;
                }
            }
        }
        else if (!this->createAnimationAndShader2Particle(operatorShader, newCodeLine))
        {
            ERROR_AT(__LINE__,__FILE__, "error on add animation!!");
            return false;
        }
        if(this->texture == nullptr)
            this->texture = TEXTURE_MANAGER::getInstance()->load(fileNameTextureOrMesh, true);
        if (this->texture)
        {
            this->lenArrayParticlesData = totalParticleToLoad;
            this->wTexture              = static_cast<float>(this->texture->getWidth());
            this->hTexture              = static_cast<float>(this->texture->getHeight());
            this->totalAlive            = 0;
            if (this->particles)
                delete[] this->particles;
            this->particles = new ATT_PARTICLE[this->lenArrayParticlesData];
            if (this->buffer)
                delete[] this->buffer;
            this->buffer = new VERTEX_PARTICLE[this->lenArrayParticlesData * 4];
            if (this->particles == nullptr || this->buffer == nullptr)
                return false;
            this->minv.x = -this->wTexture / 2.0f;
            this->maxv.x =  this->wTexture / 2.0f;
            this->minv.y = -this->hTexture / 2.0f;
            this->maxv.y =  this->hTexture / 2.0f;
            if (initializeParticleData)
            {
                util::STAGE_PARTICLE* sPart = nullptr;
                if (this->lsParticleStage.size() == 0)
                {
                    sPart = new util::STAGE_PARTICLE();
                    sPart->totalParticle = totalParticleToLoad;
                    this->lsParticleStage.push_back(sPart);
                }
                else
                {
                    sPart = this->lsParticleStage[0];
                    sPart->totalParticle = totalParticleToLoad;
                }
                this->onResuscitate(sPart,this->lenArrayParticlesData);
            }
            char strTemp[255];
            snprintf(strTemp,sizeof(strTemp), "%s@%u@%s@%s", fileNameTextureOrMesh, totalParticleToLoad, operatorShader,newCodeLine ? newCodeLine : "nullptr");
            this->fileName = strTemp;
            this->enableRender            = true;
            this->alwaysRenderize = true;
            if (sizeOfParticle == 0)
                this->totalAlive = 0;
            this->updateAABB();
            return true;
        }
        return false;
    }

    void PARTICLE::onStop()
    {
        if (this->vboIndexBuffer)
        {
            GLDeleteBuffers(1, &this->vboIndexBuffer);
        }
        this->vboIndexBuffer = 0;
    }

    bool PARTICLE::renderParticle(const util::STAGE_PARTICLE* sPart)
    {
        updateParticleStage(sPart);

        ANIMATION* anim = this->getAnimation();
        GLActiveTexture(GL_TEXTURE0);
        if (this->texture)
        {
            GLBindTexture(GL_TEXTURE_2D, this->texture->idTexture);
        }
        else
        {
            GLBindTexture(GL_TEXTURE_2D, 0);
        }
        GLint* psamplerHandle0 = static_cast<GLint*>(anim->fx.shader.ptrSamplerHandle0);

        GLUniform1i(*psamplerHandle0, 0);
        if (anim->fx.textureOverrideStage2)
        {
            GLint* psamplerHandle1 = static_cast<GLint*>(anim->fx.shader.ptrSamplerHandle1);
            glActiveTexture(GL_TEXTURE1);
            GLBindTexture(GL_TEXTURE_2D, anim->fx.textureOverrideStage2->idTexture);
            glUniform1i(*psamplerHandle1, 1);
        }
        else
        {
            GLActiveTexture(GL_TEXTURE1);
            GLBindTexture(GL_TEXTURE_2D, 0);
        }
        GLboolean depthTestEnabled = true;
        glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
        GLDisable(GL_DEPTH_TEST);
        this->blend.set(anim->blendState);
        GLint* imvpMatrixHandle = static_cast<GLint*>(anim->fx.shader.ptrMvpMatrixHandle);

        GLUniformMatrix4fv(*imvpMatrixHandle, 1, GL_FALSE, SHADER::mvpMatrix.p);
        // if(fx->shader.mvMatrixHandle != -1)
        //  GLUniformMatrix4fv(fx->shader.mvMatrixHandle, 1, GL_FALSE,SHADER::mvpMatrix.p);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexBuffer);
        VAR_SHADER *var = anim->fx.fxPS->ptrCurrentShader
                                         ? anim->fx.fxPS->ptrCurrentShader->getVarByName("color")
                                         : nullptr;

        if (var)
        {
            const int32_t handleVar = *static_cast<int32_t*>(var->ptrHandleVar);
            for (unsigned int i = 0; i < this->totalAlive; ++i)
            {
                const float * vertex   = reinterpret_cast<float *>(&this->buffer[i * 4]);
                ATT_PARTICLE *particle = &this->particles[i];
                // GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexBuffer);
                GLUniform4f(handleVar, particle->r, particle->g, particle->b, particle->a);
                GLEnableVertexAttribArray(anim->fx.shader.positionHandle);
                GLVertexAttribPointer(anim->fx.shader.positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VERTEX_PARTICLE),
                                      vertex);

                GLEnableVertexAttribArray(anim->fx.shader.texCoordHandle);
                GLVertexAttribPointer(anim->fx.shader.texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VERTEX_PARTICLE),
                                      &vertex[3]);

                GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
            }
        }
        else
        {
            for (unsigned int i = 0; i < this->totalAlive; ++i)
            {
                const float *vertex = reinterpret_cast<float *>(&this->buffer[i * 4]);
                GLEnableVertexAttribArray(anim->fx.shader.positionHandle);
                GLVertexAttribPointer(anim->fx.shader.positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VERTEX_PARTICLE),
                                      vertex);

                GLEnableVertexAttribArray(anim->fx.shader.texCoordHandle);
                GLVertexAttribPointer(anim->fx.shader.texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VERTEX_PARTICLE),
                                      &vertex[3]);

                GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
            }
        }
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        if (depthTestEnabled)
        {
            GLEnable(GL_DEPTH_TEST);
        }
        return true;
    }

}


#endif // USE_OPENGL_ES