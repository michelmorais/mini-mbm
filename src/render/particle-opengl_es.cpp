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
#include <texture-manager.h>
#include <header-mesh.h>
#include <gles-debug.h>
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

    bool PARTICLE::load(const char *fileNameTextureOrMesh, const char *operatorShader, const char *newCodeLine,const unsigned int sizeOfParticle , const bool initializeParticleData )
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
                anim->blendState = static_cast<BLEND_OPENGLES>(infoHead->headerAnim->blendState);
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

        bool PARTICLE::renderParticle(const util::STAGE_PARTICLE * sPart)
    {
        ANIMATION *  anim   = this->getAnimation();
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        anim->fx.shader.update();
        anim->fx.setBlendOp();
        anim->updateAnimation(device->delta, this, nullptr, this->onEndFx);
        const VEC2  dist(maxv - minv);
        const float diffSize = sPart->maxSizeParticle - sPart->minSizeParticle;
        const float rDiff    = sPart->maxColor.x - sPart->minColor.x;
        const float gDiff    = sPart->maxColor.y - sPart->minColor.y;
        const float bDiff    = sPart->maxColor.z - sPart->minColor.z;
        for (unsigned int i = 0; i < this->totalAlive; ++i)
        {
            ATT_PARTICLE *   particle = &this->particles[i];
            VERTEX_PARTICLE *vertex   = &this->buffer[i * 4];
            particle->timeLifeCurrent += device->delta;
            if (particle->timeLifeCurrent > particle->timeLife)
            {
                if (sPart->revive)
                {
                    this->restartParticle(sPart,particle, vertex, &dist);
                }
                else
                {
                    if (this->totalAlive)
                        this->totalAlive--;
                    ATT_PARTICLE *lastParticle = &this->particles[this->totalAlive];
                    memcpy(static_cast<void*>(particle), lastParticle, sizeof(ATT_PARTICLE));
                }
            }
            else
            {
                const float x        = particle->direction.x * device->delta * particle->speed;
                const float y        = particle->direction.y * device->delta * particle->speed;
                const float z        = particle->direction.z * device->delta * particle->speed;
                float       incrSize = 0.0f;
                if (sPart->sizeMin2Max)//grow
                {
                    if (particle->aSize < sPart->maxSizeParticle)
                    {
                        incrSize        = (diffSize / particle->timeLife) * device->delta;
                        particle->aSize = vertex[2].x - vertex[0].x;
                        float perc      = (particle->aSize - sPart->minSizeParticle) / diffSize;
                        particle->a     = perc; // 0 -> 0,99
                        particle->r     = (rDiff * perc) + sPart->minColor.x;
                        particle->g     = (gDiff * perc) + sPart->minColor.y;
                        particle->b     = (bDiff * perc) + sPart->minColor.z;
                    }
                }
                else
                {
                    if (particle->aSize > sPart->minSizeParticle)
                    {
                        incrSize        = -(diffSize / particle->timeLife) * device->delta;
                        particle->aSize = vertex[2].x - vertex[0].x;
                        float perc      = 1.0f - ((particle->aSize - sPart->minSizeParticle) / diffSize);
                        particle->a     = perc; // 0,99 -> 0,0 => 0 -> 0,99
                        particle->r     = (rDiff * perc) + sPart->minColor.x;
                        particle->g     = (gDiff * perc) + sPart->minColor.y;
                        particle->b     = (bDiff * perc) + sPart->minColor.z;
                    }
                }

                if (sPart->invert_alpha)
                    particle->a = 1.0f - particle->a;
                if (sPart->invert_red)
                    particle->r = 1.0f - particle->r;
                if (sPart->invert_green)
                    particle->g = 1.0f - particle->g;
                if (sPart->invert_blue)
                    particle->b = 1.0f - particle->b;

                vertex[0].x += x - incrSize;
                vertex[0].y += y - incrSize;
                vertex[0].z += z;

                vertex[1].x += x - incrSize;
                vertex[1].y += y + incrSize;
                vertex[1].z += z;

                vertex[2].x += x + incrSize;
                vertex[2].y += y - incrSize;
                vertex[2].z += z;

                vertex[3].x += x + incrSize;
                vertex[3].y += y + incrSize;
                vertex[3].z += z;

                if (sPart->segmented)
                {
                    vertex[0].u = (vertex[0].x - minv.x) / dist.x;
                    vertex[0].v = (vertex[0].y - minv.y) / dist.y;

                    vertex[1].u = (vertex[1].x - minv.x) / dist.x;
                    vertex[1].v = (vertex[1].y - minv.y) / dist.y;

                    vertex[2].u = (vertex[2].x - minv.x) / dist.x;
                    vertex[2].v = (vertex[2].y - minv.y) / dist.y;

                    vertex[3].u = (vertex[3].x - minv.x) / dist.x;
                    vertex[3].v = (vertex[3].y - minv.y) / dist.y;
                }
                if (vertex->x < minv.x)
                    minv.x = vertex->x;
                if (vertex->y < minv.y)
                    minv.y = vertex->y;

                if (vertex->x > maxv.x)
                    maxv.x = vertex->x;
                if (vertex->y > maxv.y)
                    maxv.y = vertex->y;
            }
        }
        GLActiveTexture(GL_TEXTURE0);
        if (this->texture)
        {
            GLBindTexture(GL_TEXTURE_2D, this->texture->idTexture);
        }
        else
        {
            GLBindTexture(GL_TEXTURE_2D, 0);
        }
        GLUniform1i(anim->fx.shader.samplerHandle0, 0);
        if (anim->fx.textureOverrideStage2)
        {
            glActiveTexture(GL_TEXTURE1);
            GLBindTexture(GL_TEXTURE_2D, anim->fx.textureOverrideStage2->idTexture);
            glUniform1i(anim->fx.shader.samplerHandle1, 1);
        }
        else
        {
            GLActiveTexture(GL_TEXTURE1);
            GLBindTexture(GL_TEXTURE_2D, 0);
        }
        GLDisable(GL_DEPTH_TEST);
        this->blend.set(anim->blendState);
        GLUniformMatrix4fv(anim->fx.shader.mvpMatrixHandle, 1, GL_FALSE, SHADER::mvpMatrix.p);
        // if(fx->shader.mvMatrixHandle != -1)
        //  GLUniformMatrix4fv(fx->shader.mvMatrixHandle, 1, GL_FALSE,SHADER::mvpMatrix.p);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexBuffer);
        VAR_SHADER *var = anim->fx.fxPS->ptrCurrentShader
                                         ? anim->fx.fxPS->ptrCurrentShader->getVarByName("color")
                                         : nullptr;

        if (var)
        {
            for (unsigned int i = 0; i < this->totalAlive; ++i)
            {
                const float * vertex   = reinterpret_cast<float *>(&this->buffer[i * 4]);
                ATT_PARTICLE *particle = &this->particles[i];
                // GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexBuffer);
                GLUniform4f(var->handleVar, particle->r, particle->g, particle->b, particle->a);
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
        GLEnable(GL_DEPTH_TEST);
        return true;
    }

    bool PARTICLE::loadParticleShader(const char *operatorShader, const char *newCodeLine)
    {
        const char *defaultCodePs_1 = "precision mediump float;\n"
                                      "uniform vec4 color;\n"
                                      "uniform float enableAlphaFromColor;\n"
                                      "varying vec2 vTexCoord;\n"
                                      "uniform sampler2D sample0;\n"
                                      "void main()\n"
                                      "{\n"
                                      "  vec4 texColor;\n"
                                      "  vec4 outColor;\n"
                                      "  texColor = texture2D( sample0, vTexCoord );\n"
                                      "  if(enableAlphaFromColor > 0.5)\n"
                                      "     outColor.a = color.a;\n"
                                      "  else\n"
                                      "     outColor.a = texColor.a;\n"
                                      "  outColor.rgb = color.rgb ";

        const char *defaultCodePs_2 = " texColor.rgb;\n";

        const char *defaultCodePs_3 = "  gl_FragColor = outColor;\n"
                                      "}\n";
        std::string defaultCodePs(defaultCodePs_1);
        defaultCodePs += operatorShader;
        defaultCodePs += defaultCodePs_2;
        this->_operatorShader = operatorShader[0];
        if (newCodeLine)
        {
            defaultCodePs += newCodeLine;
            this->_newCodeLine = newCodeLine;
        }
        else
        {
            this->_newCodeLine.clear();
        }
        defaultCodePs += defaultCodePs_3;
        // printf(defaultCodePs.c_str());
        const char *defaultCodeVs = "attribute vec4 aPosition;"
                                    "attribute vec2 aTextCoord;"
                                    "uniform mat4 mvpMatrix;"
                                    "varying vec2 vTexCoord;"
                                    "void main()"
                                    "{"
                                    "     gl_Position = mvpMatrix * aPosition;"
                                    "     vTexCoord = aTextCoord;"
                                    "}";

        const char *fileNamePs = "__particle.ps";
        const char *fileNameVs = "__particle.vs";

        ANIMATION *  anim   = this->getAnimation();
        
        anim->fx.fxPS->ptrCurrentShader = anim->fx.fxPS->loadEffect(fileNamePs, defaultCodePs.c_str(), TYPE_ANIMATION_PAUSED);
        anim->fx.fxVS->ptrCurrentShader = anim->fx.fxVS->loadEffect(fileNameVs, defaultCodeVs, TYPE_ANIMATION_PAUSED);
        anim->fx.shader.releaseShader();
        if (!anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader))
            return false;
        float defaultVar[4] = {1, 1, 1, 1};
        if (anim->fx.fxPS->ptrCurrentShader == nullptr || 
            anim->fx.fxPS->ptrCurrentShader->addVar("color", VAR_COLOR_RGBA, defaultVar,
                                                       anim->fx.shader.programObject) == false)
        {
#if defined _DEBUG
            PRINT_IF_DEBUG("failed to included variable [%s] shader [%s]!", "color", fileNamePs);
#endif
        }
        if (anim->fx.fxPS->ptrCurrentShader == nullptr || anim->fx.fxPS->ptrCurrentShader->addVar("enableAlphaFromColor", VAR_FLOAT, defaultVar,
                                                       anim->fx.shader.programObject) == false)
        {
#if defined _DEBUG
            PRINT_IF_DEBUG("failed to included variable [%s] shader [%s]!", "enableAlphaFromColor",fileNamePs);
#endif
        }
        return true;
    }
}


