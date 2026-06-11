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
#include <core_mbm/util-interface.h>
#include <core_mbm/shader-var-cfg.h>
#include <core_mbm/scene.h>
#include <core_mbm/shader-resource.h>
#include <static-resource/resource-particle.h>

#if (defined _DEBUG || defined DEBUG)
    #include <core_mbm/log-util.h>
#endif


namespace mbm
{
    void STEERED_PARTICLE::release()
    {
        this->releaseAnimation();
        this->enableRender = false;
        this->bufferGl.release();
        for (unsigned int i = 0; i < this->lsParticleGroup.size(); ++i)
        {
            FLUID_GROUP* pGroup = lsParticleGroup[i];
            delete pGroup;
        }
        lsParticleGroup.clear();
    }

    STEERED_PARTICLE::STEERED_PARTICLE(const SCENE *scene, const bool _is3d, const bool _is2dScreen,const bool b_segmented,const float* _scale_physics_engine)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_STEERED_PARTICLE, _is3d && _is2dScreen == false, _is2dScreen),
        scale_physics_engine(_scale_physics_engine),
        texture(nullptr),
        segmented(b_segmented),
        radiusScale(1.0f),
        loadedColored(nullptr)
    {
        mbm::DEVICE* device         = mbm::DEVICE::getInstance();
        device->addRenderizable(this);
    }
    
    STEERED_PARTICLE::~STEERED_PARTICLE()
    {
        this->release();
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->removeRenderizable(this);
        infoPhysics.release();
        if (loadedColored)
            delete loadedColored;
        loadedColored = nullptr;
    }

    const COLOR STEERED_PARTICLE::getColor(const uint32_t index_group) const noexcept
    {
        if (index_group < this->lsParticleGroup.size())
        {
            FLUID_GROUP* pGroup = this->lsParticleGroup[index_group];
            if(pGroup->color)
                return *pGroup->color;
        }
        return COLOR(1.0f,1.0f,1.0f,1.0f);
    }
    
    void STEERED_PARTICLE::setColor(const COLOR& color, const uint32_t index_group) noexcept
    {
        if (loadedColored == nullptr)
        {
            ERROR_AT(__LINE__, __FILE__, "STEERED_PARTICLE is not loaded with color option!");
        }
        else if (index_group < this->lsParticleGroup.size())
        {
            FLUID_GROUP* pGroup = this->lsParticleGroup[index_group];
            if(pGroup->color)
                *pGroup->color = color;
            else
            {
                ERROR_AT(__LINE__, __FILE__, "STEERED_PARTICLE should be loaded with color option!");
            }
        }
    };

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

    uint32_t STEERED_PARTICLE::addGroup(const COLOR* color)
    {
		if (color && this->loadedColored == nullptr)
        {
            ERROR_AT(__LINE__, __FILE__, "STEERED_PARTICLE is not loaded with color option!");
            color = nullptr;
        }
        auto  group = new FLUID_GROUP(this->segmented,this->radiusScale, color ? color : this->loadedColored);
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
            snprintf(anim->nameAnimation, sizeof(anim->nameAnimation), "group:%d", 1);
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

    bool STEERED_PARTICLE::load(const char* fileNameTexture,
        const COLOR* p_color,
        const mbm::INFO_PHYSICS* const p_infoPhysics)
    {
        this->release();
        if (p_infoPhysics == nullptr)
            return false;
        if ((p_infoPhysics != &this->infoPhysics) && (infoPhysics.clone(p_infoPhysics) == false))
            return false;
        if (bufferGl.loadParticleBuffer() == false)
            return false;
        if (fileNameTexture == nullptr)
        {
            this->texture = TEXTURE_MANAGER::getInstance()->load(&resource_particle);
            fileNameTexture = nickNameImageFromResource_particle;
        }
        this->texture = TEXTURE_MANAGER::getInstance()->load(fileNameTexture, true);
        if (this->texture)
        {
            bufferGl.setTextureByStage(this->texture, 0, 0);
            if (!this->createAnimationAndShader2Particle(p_color))
            {
                ERROR_AT(__LINE__, __FILE__, "error on add animation!!");
                return false;
            }
            this->fileName = fileNameTexture;
            this->enableRender = true;
            this->alwaysRenderize = true;
            this->updateAABB();
            return true;
        }
        return false;
    }


    bool STEERED_PARTICLE::renderParticle(FLUID_GROUP* pGroup)
    {
        if (pGroup->totalParticleToRender == 0)
            return false;
        for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
        {
            const VEC3* particle = &pGroup->particle_positions[i];
            VEC3* vertex = &pGroup->vertex_particle[i * 4];
            pGroup->setVertex(particle, vertex);
        }
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        ANIMATION* anim = this->getAnimation();
        this->blend.set(anim->blendState);
        anim->updateAnimation(device->delta, this, nullptr, this->onEndFx);
        anim->fx.setBlendOp();
        anim->fx.shader.update();

        return anim->fx.shader.renderParticle(&this->bufferGl, pGroup);
    }

    bool STEERED_PARTICLE::loadParticleShader(const COLOR* p_color)
    {
        ANIMATION* anim = this->getAnimation();
        if (p_color)
        {
            if (this->loadedColored)
                delete this->loadedColored;
            this->loadedColored = new COLOR(*p_color);
            const char* defaultCodePs = getSteeredParticlePSCode(true);
            const char* defaultCodeVs = getSteeredParticleVSCode();

            const char* fileNamePs = "__steered_particle.ps";
            const char* fileNameVs = "__steered_particle.vs";

            anim->fx.fxPS->ptrCurrentShader = anim->fx.fxPS->loadEffect(fileNamePs, defaultCodePs, TYPE_ANIMATION_GROWING);
            anim->fx.fxVS->ptrCurrentShader = anim->fx.fxVS->loadEffect(fileNameVs, defaultCodeVs, TYPE_ANIMATION_GROWING);
            anim->fx.shader.releaseShader();
            if (!anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader, getFvfFromBuffer()))
                return false;

            const float defaultVar[4] = { p_color->r, p_color->g, p_color->b, p_color->a };
            if (anim->fx.fxPS->ptrCurrentShader)
            {
                if (anim->fx.fxPS->ptrCurrentShader->addVar("color", VAR_COLOR_RGBA, defaultVar, anim->fx.shader.ptrShaderSpecific, true) == false)
                {
                    PRINT_IF_DEBUG("failed to included variable [%s] shader [%s]!", "color", fileNamePs);
                }
                VAR_SHADER* colorVar = anim->fx.fxPS->ptrCurrentShader->getVarByName("color");
                if (colorVar)
                {
                    colorVar->set(defaultVar, defaultVar, 1.0f);
                }
            }
        }
        else
        {
            if (this->loadedColored)
                delete this->loadedColored;
            this->loadedColored = nullptr;
            const char* defaultCodePs = getSteeredParticlePSCode(false);
            const char* defaultCodeVs = getSteeredParticleVSCode();

            const char* fileNamePs = "__steered_particle_no_color.ps";
            const char* fileNameVs = "__steered_particle.vs";

            anim->fx.fxPS->ptrCurrentShader = anim->fx.fxPS->loadEffect(fileNamePs, defaultCodePs, TYPE_ANIMATION_PAUSED);
            anim->fx.fxVS->ptrCurrentShader = anim->fx.fxVS->loadEffect(fileNameVs, defaultCodeVs, TYPE_ANIMATION_PAUSED);
            anim->fx.shader.releaseShader();
            if (!anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader, getFvfFromBuffer()))
                return false;
        }
        return true;
    }

    FVF_PROVIDE_BY_ENGINE STEERED_PARTICLE::getFvfFromBuffer() const noexcept
    {
        return bufferGl.isLoadedBuffer() ? bufferGl.fvf : FVF_PROVIDE_BY_ENGINE::FVF_NONE;
    }
    
    bool STEERED_PARTICLE::render()
    {
        if (this->getTotalParticleToRender() > 0)
        {
            mbm::DEVICE* device = mbm::DEVICE::getInstance();
            const CAMERA &camera = device->getCamera();
            if (this->is3D)
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &camera.matrixPerspective);
            }
            else if (this->is2dS)
            {
                VEC3 positionScreen(this->position.x * camera.scaleScreen2d.x,
                    this->position.y * camera.scaleScreen2d.y, this->position.z);
                device->transformeScreen2dToWorld2d_scaled(this->position.x, this->position.y, positionScreen);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionScreen, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &camera.matrixPerspective2d);
            }
            else
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &camera.matrixPerspective2d);
            }
            for (FLUID_GROUP* pGroup : this->lsParticleGroup)
            {
                const bool ret = this->renderParticle(pGroup);
                if(ret == false)
                    return false;
            }
        }
        return true;
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
		std::vector<FLUID_GROUP*> lsParticleGroupBackup = std::move(this->lsParticleGroup);
        const char * fileNameTexture = this->fileName.c_str();
		mbm::INFO_PHYSICS otherInfoPhysics;
		otherInfoPhysics.clone(&this->infoPhysics);
		COLOR* p_color = lsParticleGroupBackup.size() > 0 ? lsParticleGroupBackup[0]->color : nullptr;
        const bool  ret              = this->load(fileNameTexture,p_color,&otherInfoPhysics);
        this->lsParticleGroup        = std::move(lsParticleGroupBackup);
        if (ret == false)
        {
            return this->releaseOnFail();
        }
        #if defined DEBUG
        PRINT_INFO_IF_DEBUG("Particle [%s] successfully restored",log_util::basename(fileNameTexture ));
        #endif
        return true;
    }

    bool STEERED_PARTICLE::setTexture(const MESH_MBM *,const char *fileNametexture, const uint32_t stage, const bool hasAlpha)
    {
        TEXTURE *newTex = TEXTURE_MANAGER::getInstance()->load(fileNametexture, hasAlpha);
        if (newTex)
        {
            if (stage == 0)
            {
                this->texture = newTex;
                bufferGl.setTextureByStage(this->texture, 0, 0);
                return true;
            }
            else
            {
                mbm::ANIMATION *anim = this->getAnimation();
                if(anim)
                {
                    anim->fx.textureOverrideStage2 = newTex;
                    bufferGl.setTextureByStage(newTex, stage, 0);
                    return true;
                }
            }
        }
        return false;
    }
    
    bool STEERED_PARTICLE::createAnimationAndShader2Particle(const COLOR *p_color)
    {
        this->releaseAnimation();
        auto anim = new ANIMATION();
        snprintf(anim->nameAnimation, sizeof(anim->nameAnimation), "group:%d", 1);
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

