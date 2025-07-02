/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2022      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include "steered_particle.h"
#include <core_mbm/texture-manager.h>
#include <core_mbm/header-mesh.h>
#include <core_mbm/gles-debug.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/util-interface.h>
#include <core_mbm/shader-var-cfg.h>
#include <climits>

#if (defined _DEBUG || defined DEBUG_RESTORE)
    #include <core_mbm/log-util.h>
#endif


namespace mbm
{


    FLUID_GROUP::FLUID_GROUP(const bool b_segmented,const float _radiusScale):
    size_particle_array(0),
    totalParticleToRender(0),
    aSizeParticle(1.0f),
    radiusScale(_radiusScale),
    particle_positions(nullptr),
    vertex_particle(nullptr),
    uv(nullptr),
    segmented(b_segmented)
    {
    }

    FLUID_GROUP::~FLUID_GROUP()
    {
        if(vertex_particle)
            delete [] vertex_particle;
        vertex_particle = nullptr;

        if(particle_positions)
            delete [] particle_positions;
        particle_positions = nullptr;

        if(uv)
            delete [] uv;
        uv = nullptr;
    }

    void FLUID_GROUP::resizeParticleData(const uint32_t new_size)
    {
        
        if (new_size > this->size_particle_array)
        {
            //printf("resizeParticleData old %u new %u\n",this->size_particle_array,new_size);
            {
                auto particlesTemp = new VEC3[new_size];
                if(this->particle_positions)
                {
                    memcpy(static_cast<void*>(particlesTemp), this->particle_positions, this->size_particle_array * sizeof(VEC3));
                    delete[] this->particle_positions;
                }
                this->particle_positions = particlesTemp;
            }
            {
                const unsigned int newBufferSize = new_size * 4; // x4 because we have 4 vertex indexed 
                auto   tempVertex = new VEC3[newBufferSize];
                if(this->vertex_particle)
                {
                    memcpy(static_cast<void*>(tempVertex), this->vertex_particle, this->size_particle_array * sizeof(VEC3) * 4); // x4 because we have 4 vertex indexed 
                    delete[] this->vertex_particle;
                }
                this->vertex_particle      = tempVertex;
            }
            if(segmented)
            {
                const unsigned int newBufferSize = new_size * 4; // x4 because we have 4 vertex indexed 
                auto   tempUv = new VEC2[newBufferSize];
                if(this->uv)
                {
                    memcpy(static_cast<void*>(tempUv), this->uv, this->size_particle_array * sizeof(VEC2) * 4); // x4 because we have 4 vertex indexed 
                    delete[] this->uv;

                    for(uint32_t i = this->size_particle_array; i < new_size; ++i)
                    {
                        uint32_t index_buffer = i * 4;
                        VEC2 * p_uv = &tempUv[index_buffer];
                        p_uv[0].x = 0;
                        p_uv[0].y = 1;
                        p_uv[1].x = 0;
                        p_uv[1].y = 0;
                        p_uv[2].x = 1;
                        p_uv[2].y = 1;
                        p_uv[3].x = 1;
                        p_uv[3].y = 0;
                    }
                }
                this->uv      = tempUv;
            }
            else
            {
                if(this->uv == nullptr)
                {
                    this->uv = new VEC2[4];
                    this->uv[0].x = 0;
                    this->uv[0].y = 1;
                    this->uv[1].x = 0;
                    this->uv[1].y = 0;
                    this->uv[2].x = 1;
                    this->uv[2].y = 1;
                    this->uv[3].x = 1;
                    this->uv[3].y = 0;
                }
            }
            this->size_particle_array  = new_size;
        }
        this->totalParticleToRender = new_size;
    } 

    void FLUID_GROUP::setVertex(const VEC3 * const position, VEC3 pVertex[4])
    {
        const float halfSizeParticle = this->aSizeParticle * 0.5f * this->radiusScale;
        pVertex[0].x = position->x - halfSizeParticle;
        pVertex[0].y = position->y - halfSizeParticle;
        pVertex[0].z = position->z;

        pVertex[1].x = position->x - halfSizeParticle;
        pVertex[1].y = position->y + halfSizeParticle;
        pVertex[1].z = position->z;

        pVertex[2].x = position->x + halfSizeParticle;
        pVertex[2].y = position->y - halfSizeParticle;
        pVertex[2].z = position->z;

        pVertex[3].x = position->x + halfSizeParticle;
        pVertex[3].y = position->y + halfSizeParticle;
        pVertex[3].z = position->z;
    }

    void FLUID_GROUP::setUv(VEC2 pUv[4], const VEC2 & pos,const VEC2 & halParticleSizeInUv)
    {
        pUv[0].x = pos.x - halParticleSizeInUv.x;
        pUv[0].y = pos.y + halParticleSizeInUv.y;

        pUv[1].x = pos.x - halParticleSizeInUv.x;
        pUv[1].y = pos.y - halParticleSizeInUv.y;

        pUv[2].x = pos.x + halParticleSizeInUv.x;
        pUv[2].y = pos.y + halParticleSizeInUv.y;

        pUv[3].x = pos.x + halParticleSizeInUv.x;
        pUv[3].y = pos.y - halParticleSizeInUv.y;
    }



    
    STEERED_PARTICLE::STEERED_PARTICLE(const SCENE *scene, const bool _is3d, const bool _is2dScreen,const bool b_segmented,const float* _scale_physics_engine)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_STEERED_PARTICLE, _is3d && _is2dScreen == false, _is2dScreen),
        scale_physics_engine(_scale_physics_engine),
        segmented(b_segmented)
    {
        this->enableRender          = true;
        this->texture               = nullptr;
        this->vboIndexBuffer        = 0;
        this->device->addRenderizable(this);
    }
    
    STEERED_PARTICLE::~STEERED_PARTICLE()
    {
        this->release();
        this->device->removeRenderizable(this);
        infoPhysics.release();
    }

    void STEERED_PARTICLE::release()
    {
        this->releaseAnimation();
        this->enableRender = false;
        if (this->vboIndexBuffer)
        {
            GLDeleteBuffers(1, &this->vboIndexBuffer);
        }
        this->vboIndexBuffer = 0;
        for (unsigned int i = 0; i < this->lsParticleGroup.size(); ++i)
        {
            FLUID_GROUP* pGroup = lsParticleGroup[i];
            delete pGroup;
        }
        lsParticleGroup.clear();
    }
    
    bool STEERED_PARTICLE::load(const char *fileNameTexture,
                                const COLOR *p_color,
                                const mbm::INFO_PHYSICS* const p_infoPhysics)
    {
        this->release();
        if(p_infoPhysics == nullptr)
            return false;
        if((p_infoPhysics != &this->infoPhysics) && (infoPhysics.clone(p_infoPhysics) == false))
            return false;
        const unsigned short int index[6]            = {0, 1, 2, 2, 1, 3};
        const unsigned int       sizeIndexBuffer     = sizeof(index);
        GLGenBuffers(1, &this->vboIndexBuffer);
        if (!this->vboIndexBuffer)
            return false;
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexBuffer);
        GLBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeIndexBuffer, index, GL_STATIC_DRAW);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        this->texture = TEXTURE_MANAGER::getInstance()->load(fileNameTexture, true);
        if (this->texture)
        {
            if (!this->createAnimationAndShader2Particle(p_color))
            {
                ERROR_AT(__LINE__,__FILE__, "error on add animation!!");
                return false;
            }
            this->fileName        = fileNameTexture;
            this->enableRender    = true;
            this->alwaysRenderize = true;
            this->updateAABB();
            return true;
        }
        return false;
    }

    bool STEERED_PARTICLE::addParticle(const uint32_t numParticle,const uint32_t index_group)
    {
        if (index_group < this->lsParticleGroup.size())
        {
            FLUID_GROUP* pGroup = this->lsParticleGroup[index_group];
            pGroup->resizeParticleData(numParticle);
            return true;
        }
        ERROR_AT(__LINE__,__FILE__, "Invalid index group %u/%u",index_group,this->lsParticleGroup.size());
        return false;
    }

    uint32_t STEERED_PARTICLE::getTotalParticleToRender() const
    {
        uint32_t totalParticleToRender = 0;
        for (const auto & pGroup : this->lsParticleGroup)
        {
            totalParticleToRender += pGroup->totalParticleToRender;
        }
        return totalParticleToRender;
    }

    uint32_t STEERED_PARTICLE::getTotalParticleByGroup(const uint32_t index) const
    {
        if (index < this->lsParticleGroup.size())
            return this->lsParticleGroup[index]->totalParticleToRender;
        return 0;
    }
    void STEERED_PARTICLE::setTotalParticleByGroup(const uint32_t index,const uint32_t numParticle)
    {
        if (index < this->lsParticleGroup.size())
        {
            auto * pGroup  = this->lsParticleGroup[index];
            pGroup->resizeParticleData(numParticle);
        }
    }

    void STEERED_PARTICLE::getSizeTexture(uint32_t &width,uint32_t &height) const
    {
        if(this->texture)
        {
            width  = this->texture->getWidth();
            height = this->texture->getHeight();
        }
    };
    
    FLUID_GROUP * STEERED_PARTICLE::getParticleGroup(const uint32_t index)
    {
        if (index < this->lsParticleGroup.size())
            return this->lsParticleGroup[index];
        return nullptr;
    }

    uint32_t STEERED_PARTICLE::addGroup()
    {
        auto  group = new FLUID_GROUP(this->segmented,this->radiusScale);
        this->lsParticleGroup.push_back(group);
        return static_cast<uint32_t>(this->lsParticleGroup.size());
    }

    void STEERED_PARTICLE::removeGroup(const uint32_t index)
    {
        if(index < static_cast<uint32_t>(lsParticleGroup.size()))
        {
            auto  group = lsParticleGroup[index];
            lsParticleGroup.erase(lsParticleGroup.begin() + index);
            delete group;
        }
    }

    uint32_t STEERED_PARTICLE::getTotalGroup() const
    {
        return static_cast<uint32_t>(this->lsParticleGroup.size());
    }

    void STEERED_PARTICLE::restartAnimationParticle()
    {
        ANIMATION* anim = this->getAnimation();
        if (anim)
        {
            anim->isEndedThisAnimation = false;
            anim->currentWayGrowingOfAnimation = false;
            sprintf(anim->nameAnimation, "group:%d", 1);
        }
    }

    const char* STEERED_PARTICLE::getTextureFileName()const
    {
        if (this->texture)
            return this->texture->getFileNameTexture();
        return nullptr;
    }
    
    bool STEERED_PARTICLE::isOnFrustum()
    {
        if (this->isRender2Texture)
            return false;
        if (this->getTotalParticleToRender() > 0)
            return true;
        return false;
    }
    
    bool STEERED_PARTICLE::render()
    {
        if (this->getTotalParticleToRender() > 0)
        {
            if (this->is3D)
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective);
            }
            else if (this->is2dS)
            {
                VEC3 positionScreen(this->position.x * this->device->camera.scaleScreen2d.x,
                    this->position.y * this->device->camera.scaleScreen2d.y, this->position.z);
                this->device->transformeScreen2dToWorld2d_scaled(this->position.x, this->position.y, positionScreen);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionScreen, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective2d);
            }
            else
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective2d);
            }
            for (FLUID_GROUP* pGroup : this->lsParticleGroup)
            {
                return this->renderParticle(pGroup);
            }
        }
        return false;
    }
    
    void STEERED_PARTICLE::onStop()
    {
        if (this->vboIndexBuffer)
        {
            GLDeleteBuffers(1, &this->vboIndexBuffer);
        }
        this->vboIndexBuffer = 0;
    }
    
    bool STEERED_PARTICLE::releaseOnFail()
    {
        this->release();
        return false;
    }
    
    bool STEERED_PARTICLE::onRestoreDevice()
    {
        this->texture = nullptr;
        if (this->lsParticleGroup.size() == 0)
            return this->releaseOnFail();
        std::vector<FLUID_GROUP*> lsParticleGroupBackup(this->lsParticleGroup);
        this->lsParticleGroup.clear();
        const char * fileNameTexture = this->fileName.c_str();
        ANIMATION *       anim   = this->getAnimation();
        COLOR * p_color = nullptr;
        if(anim && anim->fx.fxPS && anim->fx.fxPS->ptrCurrentShader)
        {
            VAR_SHADER *varColor = anim->fx.fxPS->ptrCurrentShader->getVarByName("color");
            if(varColor)
            {
                p_color = &shader_color;
            }
        }
        const bool  ret              = this->load(fileNameTexture,p_color,&this->infoPhysics);
        this->lsParticleGroup        = std::move(lsParticleGroupBackup);
        if (ret == false)
        {
            return this->releaseOnFail();
        }
        #if defined DEBUG_RESTORE
        PRINT_IF_DEBUG("Particle [%s] successfully restored",log_util::basename(fileNameTexture ));
        #endif
        return true;
    }
    
    bool STEERED_PARTICLE::renderParticle(FLUID_GROUP * pGroup)
    {
        if(pGroup->totalParticleToRender == 0)
            return false;
        for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
        {
            const VEC3 * particle  = &pGroup->particle_positions[i];
            VEC3 *vertex           = &pGroup->vertex_particle[i * 4];
            pGroup->setVertex( particle, vertex);
        }
        ANIMATION *  anim   = this->getAnimation();
        this->blend.set(anim->blendState);
        anim->updateAnimation(this->device->delta, this, nullptr, this->onEndFx);
        anim->fx.setBlendOp();
        anim->fx.shader.update();
        GLDisable(GL_DEPTH_TEST);
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
        
        GLUniformMatrix4fv(anim->fx.shader.mvpMatrixHandle, 1, GL_FALSE, SHADER::mvpMatrix.p);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexBuffer);
        VAR_SHADER *var = anim->fx.fxPS->ptrCurrentShader
                                         ? anim->fx.fxPS->ptrCurrentShader->getVarByName("color")
                                         : nullptr;
        if(this->segmented)
        {
            if (var)
            {
                GLUniform4f(var->handleVar, this->shader_color.r, this->shader_color.g, this->shader_color.b, this->shader_color.a);
                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    const float * vertex  = reinterpret_cast<float *>(&pGroup->vertex_particle[i * 4]);
                    const float * uv      = reinterpret_cast<float *>(&pGroup->uv[i * 4]);
                    GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexBuffer);
                    GLEnableVertexAttribArray(anim->fx.shader.positionHandle);
                    GLVertexAttribPointer(anim->fx.shader.positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3),vertex);
                    GLEnableVertexAttribArray(anim->fx.shader.texCoordHandle);
                    GLVertexAttribPointer(anim->fx.shader.texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2),uv);
                    GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                }
            }
            else
            {
                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    //if(i != 168/2 && i != 168/2+1 && i != 168/2+2 && i != 168/2+4)
                    {
                        const float *vertex = reinterpret_cast<float *>(&pGroup->vertex_particle[i * 4]);
                        const float * uv      = reinterpret_cast<float *>(&pGroup->uv[i * 4]);
                        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexBuffer);
                        GLEnableVertexAttribArray(anim->fx.shader.positionHandle);
                        GLVertexAttribPointer(anim->fx.shader.positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3),vertex);
                        GLEnableVertexAttribArray(anim->fx.shader.texCoordHandle);
                        GLVertexAttribPointer(anim->fx.shader.texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2),uv);
                        GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                    }
                }
            }
        }
        else
        {
            if (var)
            {
                GLUniform4f(var->handleVar, this->shader_color.r, this->shader_color.g, this->shader_color.b, this->shader_color.a);
                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    const float * vertex  = reinterpret_cast<float *>(&pGroup->vertex_particle[i * 4]);
                    const float * uv      = reinterpret_cast<float *>(pGroup->uv);
                    GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexBuffer);
                    GLEnableVertexAttribArray(anim->fx.shader.positionHandle);
                    GLVertexAttribPointer(anim->fx.shader.positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3),vertex);
                    GLEnableVertexAttribArray(anim->fx.shader.texCoordHandle);
                    GLVertexAttribPointer(anim->fx.shader.texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2),uv);
                    GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                }
            }
            else
            {
                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    const float * vertex  = reinterpret_cast<float *>(&pGroup->vertex_particle[i * 4]);
                    const float * uv      = reinterpret_cast<float *>(pGroup->uv);
                    GLEnableVertexAttribArray(anim->fx.shader.positionHandle);
                    GLVertexAttribPointer(anim->fx.shader.positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3),vertex);
                    GLEnableVertexAttribArray(anim->fx.shader.texCoordHandle);
                    GLVertexAttribPointer(anim->fx.shader.texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2),uv);
                    GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                }
            }
        }
        
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        GLEnable(GL_DEPTH_TEST);
        return true;
    }

    bool STEERED_PARTICLE::setTexture(
        const MESH_MBM *,
        const char *fileNametexture, const uint32_t stage, const bool hasAlpha)
        {
            TEXTURE *newTex = TEXTURE_MANAGER::getInstance()->load(fileNametexture, hasAlpha);
            if (newTex)
            {
                if (stage == 0)
                {
                    this->texture = newTex;
                    return true;
                }
                else
                {
                    mbm::ANIMATION *anim = this->getAnimation();
                    if(anim)
                    {
                        anim->fx.textureOverrideStage2 = newTex;
                        return true;
                    }
                }
            }
            return false;
        }
    
    bool STEERED_PARTICLE::loadParticleShader(const COLOR *p_color)
    {
        if(p_color)
        {
            this->shader_color = *p_color;
            const char *defaultCodePs_1 = "precision mediump float;\n"
                                        "uniform vec4 color;\n"
                                        "varying vec2 vTexCoord;\n"
                                        "uniform sampler2D sample0;\n"
                                        "void main()\n"
                                        "{\n"
                                        "  vec4 texColor = texture2D( sample0, vTexCoord );\n"
                                        "  gl_FragColor = color * texColor;\n"
                                        "}\n";
            std::string defaultCodePs(defaultCodePs_1);
            const char *defaultCodeVs = "attribute vec4 aPosition;"
                                        "attribute vec2 aTextCoord;"
                                        "uniform mat4 mvpMatrix;"
                                        "varying vec2 vTexCoord;"
                                        "void main()"
                                        "{"
                                        "     gl_Position = mvpMatrix * aPosition;"
                                        "     vTexCoord = aTextCoord;"
                                        "}";

            const char *fileNamePs = "__steered_particle.ps";
            const char *fileNameVs = "__steered_particle.vs";

            ANIMATION *  anim   = this->getAnimation();
            
            anim->fx.fxPS->ptrCurrentShader = anim->fx.fxPS->loadEffect(fileNamePs, defaultCodePs.c_str(), TYPE_ANIMATION_GROWING);
            anim->fx.fxVS->ptrCurrentShader = anim->fx.fxVS->loadEffect(fileNameVs, defaultCodeVs, TYPE_ANIMATION_GROWING);
            anim->fx.shader.releaseShader();
            if (!anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader))
                return false;
            const float defaultVar[4] = {this->shader_color.r, this->shader_color.g, this->shader_color.b, this->shader_color.a};
            if (anim->fx.fxPS->ptrCurrentShader)
            {
                if(anim->fx.fxPS->ptrCurrentShader->addVar("color", VAR_COLOR_RGBA, defaultVar,anim->fx.shader.programObject) == false)
                {
                    PRINT_IF_DEBUG("failed to included variable [%s] shader [%s]!", "color", fileNamePs);
                }
                VAR_SHADER * colorVar = anim->fx.fxPS->ptrCurrentShader->getVarByName("color");
                if(colorVar)
                {
                    colorVar->set(defaultVar,defaultVar,1.0f);
                }
            }
        }
        else
        {
            const char *defaultCodePs = "precision mediump float;\n"
                                        "varying vec2 vTexCoord;\n"
                                        "uniform sampler2D sample0;\n"
                                        "void main()\n"
                                        "{\n"
                                        "  gl_FragColor = texture2D( sample0, vTexCoord );\n"
                                        "}\n";
            const char *defaultCodeVs = "attribute vec4 aPosition;"
                                        "attribute vec2 aTextCoord;"
                                        "uniform mat4 mvpMatrix;"
                                        "varying vec2 vTexCoord;"
                                        "void main()"
                                        "{"
                                        "     gl_Position = mvpMatrix * aPosition;"
                                        "     vTexCoord = aTextCoord;"
                                        "}";

            const char *fileNamePs = "__steered_particle.ps";
            const char *fileNameVs = "__steered_particle.vs";

            ANIMATION *  anim   = this->getAnimation();
            
            anim->fx.fxPS->ptrCurrentShader = anim->fx.fxPS->loadEffect(fileNamePs, defaultCodePs, TYPE_ANIMATION_PAUSED);
            anim->fx.fxVS->ptrCurrentShader = anim->fx.fxVS->loadEffect(fileNameVs, defaultCodeVs, TYPE_ANIMATION_PAUSED);
            anim->fx.shader.releaseShader();
            if (!anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader))
                return false;
        }
        return true;
    }
    
    bool STEERED_PARTICLE::createAnimationAndShader2Particle(const COLOR *p_color)
    {
        this->releaseAnimation();
        auto anim = new ANIMATION();
        sprintf(anim->nameAnimation, "group:%d", 1);
        anim->isEndedThisAnimation = false;
        anim->currentWayGrowingOfAnimation = false;
        anim->type = TYPE_ANIMATION_PAUSED;
        anim->blendState = BLEND_ONE;
        this->lsAnimation.push_back(anim);
        if (!this->loadParticleShader(p_color))
            return false;
        return true;
    }
    
    const INFO_PHYSICS * STEERED_PARTICLE::getInfoPhysics() const
    {
        return &infoPhysics;
    }
    
    const MESH_MBM * STEERED_PARTICLE::getMesh() const
    {
        return nullptr;
    }

	FX*  STEERED_PARTICLE::getFx()const
	{
		auto * anim = getAnimation();
		if (anim)
			return &anim->fx;
		return nullptr;
	}

	ANIMATION_MANAGER*  STEERED_PARTICLE::getAnimationManager()
	{
		return this;
	}
    
    bool STEERED_PARTICLE::isLoaded() const
    {
        return this->texture != nullptr;
    }

    bool STEERED_PARTICLE::clonePhysics(const mbm::INFO_PHYSICS * const new_info_physics)
    {
        return infoPhysics.clone(new_info_physics);
    }
}


