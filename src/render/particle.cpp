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
#include <mesh-manager.h>
#include <util-interface.h>
#include <shader-var-cfg.h>
#include <core_mbm/scene.h>
#include <shader-resource.h>
#include <static-resource/resource-particle.h>



namespace mbm
{
    PARTICLE::PARTICLE(const SCENE *scene, const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_PARTICLE, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->texture               = nullptr;
        this->_operatorShader       = '+';
        this->control.setOnEndAnimationParticleControl(PARTICLE::onEndAnimationParticleControl);
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->addRenderizable(this);
    }

    void PARTICLE::release()
    {
        this->releaseAnimation();
        this->bufferGl.release();
        this->control.release();
    }
    
    PARTICLE::~PARTICLE()
    {
        this->release();
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->removeRenderizable(this);
    }

    bool PARTICLE::load(const char* fileNameTextureOrMesh, const char* operatorShader, const char* newCodeLine, const unsigned int sizeOfParticle, const bool initializeParticleData)
    {
        this->release();
        const unsigned int totalParticleToLoad = sizeOfParticle ? sizeOfParticle : 1;
        this->texture = nullptr;

        if (bufferGl.loadParticleBuffer() == false)
            return false;
        if (fileNameTextureOrMesh == nullptr)
        {
            this->texture = TEXTURE_MANAGER::getInstance()->load(&resource_particle);
            if (this->texture)
            {
                fileNameTextureOrMesh = nickNameImageFromResource_particle;
            }
            else
            {
                ERROR_LOG("Could not load [%s]!", nickNameImageFromResource_particle);
                return false;
            }
        }
        operatorShader = operatorShader ? operatorShader : "*";
        const size_t lFile = strlen(fileNameTextureOrMesh);
        if (lFile > 4 && strcasecmp(&fileNameTextureOrMesh[lFile - 3], "ptl") == 0)//is particle from mesh
        {
            MESH_MBM* mesh = MESH_MANAGER::getInstance()->load(fileNameTextureOrMesh);
            if (mesh == nullptr)
                return false;
            this->texture = mesh->getTexture(0, 0);
            const auto lsParticleInfo = mesh->getInfoParticle();
            if (lsParticleInfo == nullptr)
            {
                ERROR_LOG("type of file is not particle!\ntype: %s", MESH_MANAGER::typeClassName(mesh->getTypeMesh()));
                return false;
            }
            char newOperator[2] = { '*',0 };
            for (auto& i : *lsParticleInfo)
            {
                if (control.addStageFromOther(i) == 1) // first
                {
                    newOperator[0] = i->_operator;
                }
            }
            
            if (!this->createAnimationAndShader2Particle(newOperator, newCodeLine))
            {
                ERROR_AT(__LINE__, __FILE__, "error on add animation!!");
                return false;
            }
            ANIMATION* anim = this->getAnimation();
            const util::INFO_ANIMATION::INFO_HEADER_ANIM* infoHead = mesh->infoAnimation.lsHeaderAnim[0];
            if (anim && mesh->infoAnimation.lsHeaderAnim.size() && infoHead->headerAnim)
            {
                anim->blendState = static_cast<BLEND_STATE>(infoHead->headerAnim->blendState);
                if (infoHead->effectShader)
                {
                    anim->fx.blendOperation = infoHead->effectShader->blendOperation;
                }
            }
        }
        else if (!this->createAnimationAndShader2Particle(operatorShader, newCodeLine))
        {
            ERROR_AT(__LINE__, __FILE__, "error on add animation!!");
            return false;
        }
        if (this->texture == nullptr)
            this->texture = TEXTURE_MANAGER::getInstance()->load(fileNameTextureOrMesh, true);
        if (this->texture)
        {
            bufferGl.setTextureByStage(this->texture, 0, 0);
            // When loading from .ptl with sizeOfParticle==0, use totalParticle from file stages
            unsigned int effectiveTotal = totalParticleToLoad;
            if (sizeOfParticle == 0 && this->control.getTotalStage() > 0)
            {
                unsigned int maxFromStages = 0;
                for (unsigned int i = 0; i < this->control.getTotalStage(); ++i)
                {
                    const util::STAGE_PARTICLE* s = this->control.getStageParticle(i);
                    if (s && s->totalParticle > maxFromStages)
                        maxFromStages = s->totalParticle;
                }
                if (maxFromStages > 0)
                    effectiveTotal = maxFromStages;
            }
            control.initializeBuffer(effectiveTotal, static_cast<float>(this->texture->getWidth()), static_cast<float>(this->texture->getHeight()));
            if (initializeParticleData)
            {
                if (this->control.getTotalStage() == 0)
                {
                    util::STAGE_PARTICLE sPart;
                    sPart.totalParticle = totalParticleToLoad;
                    this->control.addStageFromOther(&sPart);
                }
                else
                {
                    util::STAGE_PARTICLE * sPart = this->control.getStageParticle(0);
                    sPart->totalParticle = totalParticleToLoad ? totalParticleToLoad : effectiveTotal;
                }
                this->control.onResuscitate(this->control.getStageParticle(0), control.getTotalParticle());
            }
            char strTemp[255];
            snprintf(strTemp, sizeof(strTemp), "%s@%u@%s@%s", fileNameTextureOrMesh, effectiveTotal, operatorShader, newCodeLine ? newCodeLine : "");
            this->fileName = strTemp;
            this->enableRender = true;
            this->alwaysRenderize = true;
            // Only start with 0 alive when loading texture (not .ptl) without particle count - particles arise over time
            const bool isPtlFile = (lFile > 4 && strcasecmp(&fileNameTextureOrMesh[lFile - 3], "ptl") == 0);
            if (sizeOfParticle == 0 && !isPtlFile)
                this->control.setTotalAlive(0);
            this->updateAABB();
            return true;
        }
        return false;
    }

    bool PARTICLE::loadParticleShader(const char* operatorShader, const char* newCodeLine)
    {
        std::string defaultCodePs(getParticlePSCode());
        operatorShader = operatorShader ? operatorShader : "*";
        const std::size_t pOperator = defaultCodePs.find('?');

        if (pOperator == std::string::npos)
        {
            ERROR_AT(__LINE__, __FILE__, "Expected shader with question mark '?' but it was not found!\n shader:%s", defaultCodePs.c_str());
            return false;
        }
        defaultCodePs[pOperator] = operatorShader[0];
        this->_operatorShader = operatorShader[0];
        const std::size_t pNewCode = defaultCodePs.find('#');
        if (pNewCode == std::string::npos)
        {
            ERROR_AT(__LINE__, __FILE__, "Expected shader with hashtag '#' but it was not found!\n shader:%s", defaultCodePs.c_str());
            return false;
        }
        std::string newCode = defaultCodePs.substr(0, pNewCode);
        
        if (newCodeLine)
        {
            newCode += newCodeLine;
            this->_newCodeLine = newCodeLine;
        }
        else
        {
            this->_newCodeLine.clear();
        }
        newCode += defaultCodePs.substr(pNewCode + 1);
        defaultCodePs = newCode;

        const char* defaultCodeVs = getParticleVSCode();
        const char* fileNamePs = "__particle.ps";
        const char* fileNameVs = "__particle.vs";

        ANIMATION* anim = this->getAnimation();

        anim->fx.fxPS->ptrCurrentShader = anim->fx.fxPS->loadEffect(fileNamePs, defaultCodePs.c_str(), TYPE_ANIMATION_PAUSED);
        anim->fx.fxVS->ptrCurrentShader = anim->fx.fxVS->loadEffect(fileNameVs, defaultCodeVs, TYPE_ANIMATION_PAUSED);
        anim->fx.shader.releaseShader();
        if (!anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader, getFvfFromBuffer()))
            return false;
        float defaultVar[4] = { 1, 1, 1, 1 };
        if (anim->fx.fxPS->ptrCurrentShader == nullptr ||
            anim->fx.fxPS->ptrCurrentShader->addVar("color", VAR_COLOR_RGBA, defaultVar,
                anim->fx.shader.ptrShaderSpecific, true) == false)
        {
#if defined _DEBUG
            PRINT_IF_DEBUG("failed to included variable [%s] shader [%s]!", "color", fileNamePs);
#endif
        }
        if (anim->fx.fxPS->ptrCurrentShader == nullptr || anim->fx.fxPS->ptrCurrentShader->addVar("enableAlphaFromColor", VAR_FLOAT, defaultVar,
            anim->fx.shader.ptrShaderSpecific, true) == false)
        {
#if defined _DEBUG
            PRINT_IF_DEBUG("failed to included variable [%s] shader [%s]!", "enableAlphaFromColor", fileNamePs);
#endif
        }
        return true;
    }
    
    bool PARTICLE::addParticle(const unsigned int numParticles,const bool forceNow)
    {
        return control.addParticle(numParticles, forceNow);
    }
    
    unsigned int PARTICLE::getTotalParticleAlive() const
    {
        return this->control.getTotalParticleAlive();
    }

    uint32_t PARTICLE::getTotalParticleByStage(const uint32_t index) const
    {
        return control.getTotalParticleByStage(index);
    }
    void PARTICLE::setTotalParticleByStage(const uint32_t index,const uint32_t numParticles)
    {
        control.setTotalParticleByStage(index, numParticles);
    }
    
    unsigned int PARTICLE::getTotalParticle() const
    {
        return control.getTotalParticle();
    }

    util::STAGE_PARTICLE * PARTICLE::getStageParticle(const unsigned int index)
    {
        return control.getStageParticle(index);
    }

    util::STAGE_PARTICLE * PARTICLE::getStageParticle()
    {
        return control.getStageParticle();
    }

    unsigned int PARTICLE::addStage()
    {
        return control.addStage();
    }

    unsigned int PARTICLE::getIndexStageParticle() const
    {
        return control.getIndexStageParticle();
    }

    void PARTICLE::setIndexStageParticle(const unsigned int index)
    {
        control.setIndexStageParticle(index);
    }

    unsigned int PARTICLE::getTotalStage() const
    {
        return control.getTotalStage();
    }

    void PARTICLE::restartAnimationParticle()
    {
        if (this->control.getTotalParticle() > 0)
        {
            ANIMATION* anim = this->getAnimation();
            if (anim)
            {
                anim->isEndedThisAnimation = false;
                anim->currentWayGrowingOfAnimation = false;
                snprintf(anim->nameAnimation, sizeof(anim->nameAnimation), "stage:%d", 1);
            }
            this->control.restartAnimationParticle();
        }
    }
    const char* PARTICLE::getTextureFileName()const
    {
        if (this->texture)
            return this->texture->getFileNameTexture();
        return nullptr;
    }
    
    bool PARTICLE::isOnFrustum()
    {
        if (this->isRender2Texture)
            return false;
        if (this->control.getTotalParticle() >0  && this->control.getTotalParticleAlive() > 0)
        {
            const VEC2  dim(this->control.getDim());
            mbm::DEVICE* device = mbm::DEVICE::getInstance();
            const float w5 = device->getScaleBackBufferWidth() * 0.5f;
            const float h5 = device->getScaleBackBufferHeight() * 0.5f;
            if ((dim.x * this->scale.x) > (w5))
                this->alwaysRenderize = true;
            else if ((dim.y * this->scale.y) > (h5))
                this->alwaysRenderize = true;
            if (this->is3D)
            {
                if (this->angle.z != 0.0f || this->angle.y != 0.0f || this->angle.x != 0.0f)
                {
                    const float sw = this->control.getWTexture() * this->scale.x * 0.5f;
                    const float sh = this->control.getHTexture() * this->scale.y * 0.5f;
                    if (device->isSphereAtFrustum(this->position, sw > sh ? sw : sh))
                        return true;
                    if (device->isSphereAtFrustum(this->position, dim.x > dim.y ? dim.x : dim.y))
                        return true;
                }
                else
                {
                    CUBE   base;
                    const float sw = this->control.getWTexture() * this->scale.x * 0.5f;
                    const float sh = this->control.getHTexture() * this->scale.y * 0.5f;
                    base.halfDim.x = sw;
                    base.halfDim.y = sh;
                    base.halfDim.z = sw > sh ? sw : sh;
                    if (device->isCubeAtFrustum(this->position, this->scale, base))
                        return true;
                    base.halfDim.x = dim.x;
                    base.halfDim.y = dim.y;
                    base.halfDim.z = dim.x > dim.y ? dim.x : dim.y;
                    if (device->isCubeAtFrustum(this->position, this->scale, base))
                        return true;
                }
            }
            else if (this->is2dS)
            {
                if (this->angle.z != 0.0f) // check as circle
                {
                    const float sw = this->control.getWTexture() * this->scale.x * 0.5f;
                    const float sh = this->control.getHTexture() * this->scale.y * 0.5f;
                    if (device->isCircleScreen2dOnScreen2D_scaled(this->position.x, this->position.y,
                                                                        sw > sh ? sw : sh))
                        return true;
                    if (device->isCircleScreen2dOnScreen2D_scaled(this->position.x, this->position.y,
                                                                        dim.x > dim.y ? dim.x : dim.y))
                        return true;
                }
                else
                {
                    if (device->isRectangleScreen2dOnScreen2D_scaled(this->position.x, this->position.y,
                                                                           this->control.getWTexture() * this->scale.x,
                                                                           this->control.getHTexture() * this->scale.y))
                        return true;
                    if (device->isRectangleScreen2dOnScreen2D_scaled(this->position.x, this->position.y,
                                                                           dim.x * this->scale.x, dim.y * this->scale.y))
                        return true;
                }
            }
            else
            {
                if (this->angle.z != 0.0f) // check as circle
                {
                    const float sw = this->control.getWTexture() * this->scale.x * 0.5f;
                    const float sh = this->control.getHTexture() * this->scale.y * 0.5f;
                    if (device->isCircleWorld2dOnScreen2D_scaled(this->position.x, this->position.y,
                                                                       sw > sh ? sw : sh))
                        return true;
                    if (device->isCircleWorld2dOnScreen2D_scaled(this->position.x, this->position.y,
                                                                       dim.x > dim.y ? dim.x : dim.y))
                        return true;
                }
                else
                {
                    if (device->isRectangleWorld2dOnScreen2D_scaled(this->position.x, this->position.y,
                                                                          this->control.getWTexture() * this->scale.x,
                                                                          this->control.getHTexture() * this->scale.y))
                        return true;
                    if (device->isRectangleWorld2dOnScreen2D_scaled(this->position.x, this->position.y,
                                                                          dim.x * this->scale.x, dim.y * this->scale.y))
                        return true;
                }
            }
        }
        return false;
    }
    
    bool PARTICLE::render()
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        if (this->control.getTotalAlive())
        {
            if (this->is3D)
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &device->camera.matrixPerspective);
            }
            else if (this->is2dS)
            {
                VEC3 positionScreen(this->position.x * device->camera.scaleScreen2d.x,
                    this->position.y * device->camera.scaleScreen2d.y, this->position.z);
                device->transformeScreen2dToWorld2d_scaled(this->position.x, this->position.y, positionScreen);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionScreen, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &device->camera.matrixPerspective2d);
            }
            else
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &device->camera.matrixPerspective2d);
            }
            
            const util::STAGE_PARTICLE* sPart = control.getStageParticle();
            if (sPart)
            {
                
                this->control.updateAnimationParticle(this, this->getAnimation(),device->delta);
                return this->renderParticle(sPart);
            }
        }
        else 
        {
            this->control.updateAnimationParticle(this, this->getAnimation(), device->delta);
        }
        return false;
    }

    void PARTICLE::onEndAnimationParticleControl(void * that, const char* nameAnimation)
    {
        PARTICLE* theParticle = static_cast<PARTICLE*>(that);
        RENDERIZABLE* renderizable = reinterpret_cast<RENDERIZABLE*>(theParticle);
        if (theParticle->onEndAnimation)
        {
            theParticle->onEndAnimation(nameAnimation, renderizable);
        }
    }

    FVF_PROVIDE_BY_ENGINE PARTICLE::getFvfFromBuffer() const noexcept
    {
        return bufferGl.isLoadedBuffer() ? bufferGl.fvf : FVF_PROVIDE_BY_ENGINE::FVF_NONE;
    }

    bool PARTICLE::releaseOnFail()
    {
        this->release();
        return false;
    }
    
    bool PARTICLE::onRestoreDevice()
    {
        std::vector<std::string> result;
        this->texture = nullptr;
        util::split(result, this->fileName.c_str(), '@');
        if (result.size() < 3)
            return this->releaseOnFail();

        PARTICLE_CONTROL backupControl;
        backupControl.moveFrom(this->control);//backup copy move
        const char * newCodeLineBackup =  nullptr;
        if (result.size() > 3)
        {
            newCodeLineBackup = result[3].size() == 0 ? nullptr : result[3].c_str();
        }
        const uint32_t _sizeOfParticle = std::max(static_cast<uint32_t>(std::stoul(result[1])), this->control.getTotalParticle());
        if(this->load(result[0].c_str(),
                      result[2].c_str(),
                      newCodeLineBackup,
                      _sizeOfParticle,
            false) == false)
        {
#if defined DEBUG_RESTORE
            PRINT_IF_DEBUG("Particle [%s] failed to restore", log_util::basename(result[0].c_str()));
#endif
            return this->releaseOnFail();
        }
        this->control.moveFrom(backupControl);//restore backup copy move
        
        #if defined DEBUG_RESTORE
        PRINT_INFO_IF_DEBUG("Particle [%s] successfully restored",log_util::basename( result[0].c_str()));
        #endif
        return true;
    }

    bool PARTICLE::setTexture(const MESH_MBM *, const char *fileNametexture, const uint32_t stage, const bool hasAlpha)
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
    
    bool PARTICLE::createAnimationAndShader2Particle(const char *operatorShader, const char *newCodeLine)
    {
        this->releaseAnimation();
        auto anim = new ANIMATION();
        snprintf(anim->nameAnimation, sizeof(anim->nameAnimation), "stage:%d", 1);
        anim->isEndedThisAnimation = false;
        anim->currentWayGrowingOfAnimation = false;
        anim->type = TYPE_ANIMATION_PAUSED;
        anim->blendState = (BLEND_ONE);
        this->lsAnimation.push_back(anim);
        if (!this->loadParticleShader(operatorShader, newCodeLine))
            return false;
        return true;
    }

    bool PARTICLE::renderParticle(const util::STAGE_PARTICLE* sPart)
    {
        ANIMATION* anim = this->getAnimation();
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        anim->fx.shader.update();
        anim->fx.setBlendOp();
        this->blend.set(anim->blendState);
        anim->updateAnimation(device->delta, this, nullptr, this->onEndFx);
        this->control.updateParticleStage(sPart, device->delta);

        return anim->fx.shader.renderParticle(&this->bufferGl, &this->control);
    }
    
    const INFO_PHYSICS * PARTICLE::getInfoPhysics() const
    {
        return nullptr;
    }
    
    const MESH_MBM * PARTICLE::getMesh() const
    {
        return nullptr;
    }

    FX*  PARTICLE::getFx()const
    {
        auto * anim = getAnimation();
        if (anim)
            return &anim->fx;
        return nullptr;
    }

    ANIMATION_MANAGER*  PARTICLE::getAnimationManager()
    {
        return this;
    }
    
    bool PARTICLE::isLoaded() const
    {
        return this->control.getTotalParticle() > 0;
    }
}
