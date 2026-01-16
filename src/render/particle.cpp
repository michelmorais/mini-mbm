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
#include <climits>

#if (defined _DEBUG || defined DEBUG_RESTORE)
    #include <log-util.h>
#endif


namespace mbm
{
    PARTICLE::PARTICLE(const SCENE *scene, const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_PARTICLE, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->lenArrayParticlesData = 0;
        this->totalAlive            = 0;
        this->enableRender          = true;
        this->buffer                = nullptr;
        this->texture               = nullptr;
        this->indexStage = 0;
        this->wTexture  = 0.0f;
        this->hTexture  = 0.0f;
        this->particles = nullptr;
        this->currentTimeArise = 0.0f;
        this->minv = VEC2(0, 0);
        this->maxv = VEC2(1, 1);

        this->vboIndexBuffer        = 0;
        this->_operatorShader       = '+';
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->addRenderizable(this);
    }
    
    PARTICLE::~PARTICLE()
    {
        this->release();
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->removeRenderizable(this);
    }

    void PARTICLE::updateParticleStage(const util::STAGE_PARTICLE* sPart)
    {
        ANIMATION* anim = this->getAnimation();
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        anim->fx.shader.update();
        anim->fx.setBlendOp();
        anim->updateAnimation(device->delta, this, nullptr, this->onEndFx);
        const VEC2  dist(maxv - minv);
        const float diffSize = sPart->maxSizeParticle - sPart->minSizeParticle;
        const float rDiff = sPart->maxColor.x - sPart->minColor.x;
        const float gDiff = sPart->maxColor.y - sPart->minColor.y;
        const float bDiff = sPart->maxColor.z - sPart->minColor.z;
        for (unsigned int i = 0; i < this->totalAlive; ++i)
        {
            ATT_PARTICLE* particle = &this->particles[i];
            VERTEX_PARTICLE* vertex = &this->buffer[i * 4];
            particle->timeLifeCurrent += device->delta;
            if (particle->timeLifeCurrent > particle->timeLife)
            {
                if (sPart->revive)
                {
                    this->restartParticle(sPart, particle, vertex, &dist);
                }
                else
                {
                    if (this->totalAlive)
                        this->totalAlive--;
                    ATT_PARTICLE* lastParticle = &this->particles[this->totalAlive];
                    memcpy(static_cast<void*>(particle), lastParticle, sizeof(ATT_PARTICLE));
                }
            }
            else
            {
                const float x = particle->direction.x * device->delta * particle->speed;
                const float y = particle->direction.y * device->delta * particle->speed;
                const float z = particle->direction.z * device->delta * particle->speed;
                float       incrSize = 0.0f;
                if (sPart->sizeMin2Max)//grow
                {
                    if (particle->aSize < sPart->maxSizeParticle)
                    {
                        incrSize = (diffSize / particle->timeLife) * device->delta;
                        particle->aSize = vertex[2].x - vertex[0].x;
                        float perc = (particle->aSize - sPart->minSizeParticle) / diffSize;
                        particle->a = perc; // 0 -> 0,99
                        particle->r = (rDiff * perc) + sPart->minColor.x;
                        particle->g = (gDiff * perc) + sPart->minColor.y;
                        particle->b = (bDiff * perc) + sPart->minColor.z;
                    }
                }
                else
                {
                    if (particle->aSize > sPart->minSizeParticle)
                    {
                        incrSize = -(diffSize / particle->timeLife) * device->delta;
                        particle->aSize = vertex[2].x - vertex[0].x;
                        float perc = 1.0f - ((particle->aSize - sPart->minSizeParticle) / diffSize);
                        particle->a = perc; // 0,99 -> 0,0 => 0 -> 0,99
                        particle->r = (rDiff * perc) + sPart->minColor.x;
                        particle->g = (gDiff * perc) + sPart->minColor.y;
                        particle->b = (bDiff * perc) + sPart->minColor.z;
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
    }

    

    bool PARTICLE::loadParticleShader(const char* operatorShader, const char* newCodeLine)
    {
        const char* defaultCodePs_1 = "precision mediump float;\n"
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

        const char* defaultCodePs_2 = " texColor.rgb;\n";

        const char* defaultCodePs_3 = "  gl_FragColor = outColor;\n"
            "}\n";
        std::string defaultCodePs(defaultCodePs_1);
        operatorShader = operatorShader ? operatorShader : "*";
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
        //printf(defaultCodePs.c_str());
        /*
            precision mediump float;
            uniform vec4 color;
            uniform float enableAlphaFromColor;
            varying vec2 vTexCoord;
            uniform sampler2D sample0;
            void main()
            {
              vec4 texColor;
              vec4 outColor;
              texColor = texture2D( sample0, vTexCoord );
              if(enableAlphaFromColor > 0.5)
                 outColor.a = color.a;
              else
                 outColor.a = texColor.a;
              outColor.rgb = color.rgb * texColor.rgb;
              gl_FragColor = outColor;
            }
        */
        const char* defaultCodeVs = "attribute vec4 aPosition;"
            "attribute vec2 aTextCoord;"
            "uniform mat4 mvpMatrix;"
            "varying vec2 vTexCoord;"
            "void main()"
            "{"
            "     gl_Position = mvpMatrix * aPosition;"
            "     vTexCoord = aTextCoord;"
            "}";

        const char* fileNamePs = "__particle.ps";
        const char* fileNameVs = "__particle.vs";

        ANIMATION* anim = this->getAnimation();

        anim->fx.fxPS->ptrCurrentShader = anim->fx.fxPS->loadEffect(fileNamePs, defaultCodePs.c_str(), TYPE_ANIMATION_PAUSED);
        anim->fx.fxVS->ptrCurrentShader = anim->fx.fxVS->loadEffect(fileNameVs, defaultCodeVs, TYPE_ANIMATION_PAUSED);
        anim->fx.shader.releaseShader();
        if (!anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader))
            return false;
        float defaultVar[4] = { 1, 1, 1, 1 };
        if (anim->fx.fxPS->ptrCurrentShader == nullptr ||
            anim->fx.fxPS->ptrCurrentShader->addVar("color", VAR_COLOR_RGBA, defaultVar,
                anim->fx.shader.ptrProgramObject, true) == false)
        {
#if defined _DEBUG
            PRINT_IF_DEBUG("failed to included variable [%s] shader [%s]!", "color", fileNamePs);
#endif
        }
        if (anim->fx.fxPS->ptrCurrentShader == nullptr || anim->fx.fxPS->ptrCurrentShader->addVar("enableAlphaFromColor", VAR_FLOAT, defaultVar,
            anim->fx.shader.ptrProgramObject, true) == false)
        {
#if defined _DEBUG
            PRINT_IF_DEBUG("failed to included variable [%s] shader [%s]!", "enableAlphaFromColor", fileNamePs);
#endif
        }
        return true;
    }
    
    bool PARTICLE::addParticle(const unsigned int numParticles,const bool forceNow)
    {
        if (numParticles == 0 || this->buffer == nullptr)
            return false;
        util::STAGE_PARTICLE* sPart = nullptr;
        if (this->indexStage < this->lsParticleStage.size())
            sPart = this->lsParticleStage[this->indexStage];
        else if (this->lsParticleStage.size() == 0)
        {
            sPart = new util::STAGE_PARTICLE();
            this->lsParticleStage.push_back(sPart);
        }
        else
            sPart = this->lsParticleStage[0];
        if ((this->totalAlive + numParticles) > this->lenArrayParticlesData)
            sPart->totalParticle += this->totalAlive + numParticles - this->lenArrayParticlesData;
        if (forceNow)
        {
            const bool ret = this->_addParticle(numParticles);
            if (sPart->totalParticle < this->lenArrayParticlesData)
                sPart->totalParticle = this->lenArrayParticlesData;
            return ret;
        }
        const float n1 = (sPart->ariseTime != 0.0f ? sPart->ariseTime : 0.0001f);
        float n2 = sPart->totalParticle / n1;
        if (n2 <= 0.0f)
            n2 = 0.0001f;
        this->currentTimeArise = sPart->ariseTime - (numParticles / n2);
        if(this->currentTimeArise < 0.0f)
            this->currentTimeArise = 0.0f;
        return true;
    }
    
    unsigned int PARTICLE::getTotalParticleAlive() const
    {
        return this->totalAlive;
    }

    uint32_t PARTICLE::getTotalParticleByStage(const uint32_t index) const
    {
        if (index < this->lsParticleStage.size())
            return this->lsParticleStage[index]->totalParticle;
        return 0;
    }
    void PARTICLE::setTotalParticleByStage(const uint32_t index,const uint32_t numParticles)
    {
        if (index < this->lsParticleStage.size())
        {
            auto * sPart                 = this->lsParticleStage[index];
            if(this->lenArrayParticlesData < numParticles)
            {
                const auto diff = numParticles - this->lenArrayParticlesData;
                this->addParticle(diff,false);
                sPart->totalParticle = numParticles;
            }
            else
            {
                sPart->totalParticle = numParticles;
                if(this->totalAlive > numParticles)
                    this->totalAlive     = numParticles;
                this->currentTimeArise  = 0;
            }
        }
    }
    
    unsigned int PARTICLE::getTotalParticle() const
    {
        return this->lenArrayParticlesData;
    }

    util::STAGE_PARTICLE * PARTICLE::getStageParticle(const unsigned int index)
    {
        if (index < this->lsParticleStage.size())
            return this->lsParticleStage[index];
        return nullptr;
    }

    util::STAGE_PARTICLE * PARTICLE::getStageParticle()
    {
        if (this->indexStage < static_cast<unsigned int>(this->lsParticleStage.size()))
            return this->lsParticleStage[this->indexStage];
        return nullptr;
    }

    unsigned int PARTICLE::addStage()
    {
        auto  stage = new util::STAGE_PARTICLE();
        this->lsParticleStage.push_back(stage);
        return static_cast<unsigned int>(this->lsParticleStage.size());
    }

    unsigned int PARTICLE::getIndexStageParticle() const
    {
        return this->indexStage;
    }

    void PARTICLE::setIndexStageParticle(const unsigned int index)
    {
        if(index < static_cast<unsigned int>(lsParticleStage.size()))
            this->indexStage = index;
    }

    unsigned int PARTICLE::getTotalStage() const
    {
        return static_cast<unsigned int>(this->lsParticleStage.size());
    }

    void PARTICLE::restartAnimationParticle()
    {
        if (this->lenArrayParticlesData)
        {
            ANIMATION* anim = this->getAnimation();
            if (anim)
            {
                anim->isEndedThisAnimation = false;
                anim->currentWayGrowingOfAnimation = false;
                sprintf(anim->nameAnimation, "stage:%d", 1);
            }
            this->indexStage        = 0;
            this->totalAlive        = 0;
            this->currentTimeArise  = 0;
        }
    }
    const char* PARTICLE::getTextureFileName()const
    {
        if (this->texture)
            return this->texture->getFileNameTexture();
        return nullptr;
    }
    
    bool PARTICLE::_addParticle(const unsigned int numParticles)
    {
        if (this->lenArrayParticlesData == 0 || numParticles == 0 || this->buffer == nullptr)
            return false;
        util::STAGE_PARTICLE* sPart = nullptr;
        if (this->indexStage < this->lsParticleStage.size())
            sPart = this->lsParticleStage[this->indexStage];
        else if (this->lsParticleStage.size() == 0)
        {
            sPart = new util::STAGE_PARTICLE();
            this->lsParticleStage.push_back(sPart);
        }
        else
          sPart = this->lsParticleStage[0];
        if ((this->totalAlive + numParticles) <= this->lenArrayParticlesData)
        {
            this->onResuscitate(sPart, this->totalAlive + numParticles);
        }
        else
        {
            const unsigned int tTotalParticle = this->totalAlive + numParticles;
            auto particlesTemp = new ATT_PARTICLE[tTotalParticle];
            memcpy(static_cast<void*>(particlesTemp), this->particles, this->lenArrayParticlesData * sizeof(ATT_PARTICLE));
            delete[] this->particles;
            this->particles = particlesTemp;

            const unsigned int newBufferSize = tTotalParticle * 4; // x4 porque nosso quadrado possui 4 vertex indexados
            auto   tempVertex = new VERTEX_PARTICLE[newBufferSize];
            memcpy(static_cast<void*>(tempVertex), this->buffer, this->lenArrayParticlesData * sizeof(VERTEX_PARTICLE) * 4); // x4 porque nosso quadrado possui 4 vertex indexados
            delete[] this->buffer;
            this->buffer = tempVertex;
            this->lenArrayParticlesData = tTotalParticle;
            this->onResuscitate(sPart, tTotalParticle);
            char        strTemp[255];
            std::vector<std::string> result;
            util::split(result, this->fileName.c_str(), '@');
            if (result.size() == 4)
            {
                snprintf(strTemp,sizeof(strTemp), "%s@%u@%c@%s", result[0].c_str(), this->lenArrayParticlesData,this->_operatorShader, this->_newCodeLine.size() ? this->_newCodeLine.c_str() : "nullptr");
                this->fileName = strTemp;
            }
            else
            {
                const char *fileNameTexture = this->texture->getFileNameTexture();
                snprintf(strTemp,sizeof(strTemp), "%s@%u@%c@%s", fileNameTexture, this->lenArrayParticlesData,this->_operatorShader, this->_newCodeLine.size() ? this->_newCodeLine.c_str() : "nullptr");
                this->fileName = strTemp;
            }
        }
        return true;
    }
    
    void PARTICLE::onResuscitate(const util::STAGE_PARTICLE* sPart, const unsigned int total_To_Resuscitate)
    {
        const VEC2 dist(maxv - minv);
        while (this->totalAlive < total_To_Resuscitate)
        {
            unsigned int          index  = this->totalAlive;
            VERTEX_PARTICLE *vertex = &this->buffer[index * 4]; // x4 porque nosso quadrado possui 4 vertex indexados
            this->restartParticle(sPart,&this->particles[index], vertex, &dist);
            this->totalAlive++;
        }
    }

    void PARTICLE::restartParticle(const util::STAGE_PARTICLE* sPart, ATT_PARTICLE *particle, VERTEX_PARTICLE pPartBuffer[4], const VEC2 *dist)
    {
        particle->aSize              = sPart->sizeMin2Max ? sPart->minSizeParticle : sPart->maxSizeParticle;//grow
        const float halfSizeParticle = particle->aSize * 0.5f;
        const float x                = util::getRandomFloat(sPart->minOffsetPosition.x, sPart->maxOffsetPosition.x);
        const float y                = util::getRandomFloat(sPart->minOffsetPosition.y, sPart->maxOffsetPosition.y);
        const float z                = util::getRandomFloat(sPart->minOffsetPosition.z, sPart->maxOffsetPosition.z);

        pPartBuffer[0].x = x - halfSizeParticle;
        pPartBuffer[0].y = y - halfSizeParticle;
        pPartBuffer[0].z = z;

        pPartBuffer[1].x = x - halfSizeParticle;
        pPartBuffer[1].y = y + halfSizeParticle;
        pPartBuffer[1].z = z;

        pPartBuffer[2].x = x + halfSizeParticle;
        pPartBuffer[2].y = y - halfSizeParticle;
        pPartBuffer[2].z = z;

        pPartBuffer[3].x = x + halfSizeParticle;
        pPartBuffer[3].y = y + halfSizeParticle;
        pPartBuffer[3].z = z;

        const float ax    = util::getRandomFloat(sPart->minDirection.x, sPart->maxDirection.x);
        const float ay    = util::getRandomFloat(sPart->minDirection.y, sPart->maxDirection.y);
        const float angle = mbm::calcAzimuth(ax,ay);
        particle->direction.x = sinf(angle);
        particle->direction.y = cosf(angle);

        const float angleZ = mbm::calcAzimuth(ax,util::getRandomFloat(sPart->minDirection.z, sPart->maxDirection.z));
        particle->direction.z = cosf(angleZ);

        vec3Normalize(&particle->direction, &particle->direction);

        particle->speed = util::getRandomFloat(sPart->minSpeed, sPart->maxSpeed);

        particle->timeLife        = util::getRandomFloat(sPart->minTimeLife, sPart->maxTimeLife);
        particle->timeLifeCurrent = 0.0f;

        if (sPart->segmented)
        {
            pPartBuffer[0].u = (pPartBuffer[0].x - minv.x) / dist->x;
            pPartBuffer[0].v = (pPartBuffer[0].y - minv.y) / dist->y;

            pPartBuffer[1].u = (pPartBuffer[1].x - minv.x) / dist->x;
            pPartBuffer[1].v = (pPartBuffer[1].y - minv.y) / dist->y;

            pPartBuffer[2].u = (pPartBuffer[2].x - minv.x) / dist->x;
            pPartBuffer[2].v = (pPartBuffer[2].y - minv.y) / dist->y;

            pPartBuffer[3].u = (pPartBuffer[3].x - minv.x) / dist->x;
            pPartBuffer[3].v = (pPartBuffer[3].y - minv.y) / dist->y;
        }
        else
        {
            pPartBuffer[0].u = 0;
            pPartBuffer[0].v = 1;
            pPartBuffer[1].u = 0;
            pPartBuffer[1].v = 0;
            pPartBuffer[2].u = 1;
            pPartBuffer[2].v = 1;
            pPartBuffer[3].u = 1;
            pPartBuffer[3].v = 0;
        }
        particle->a = 0.0f;
        particle->r = 0.0f;
        particle->g = 0.0f;
        particle->b = 0.0f;
    }
    
    bool PARTICLE::isOnFrustum()
    {
        if (this->isRender2Texture)
            return false;
        if (this->lenArrayParticlesData && this->totalAlive)
        {
            const VEC2  dim(maxv - minv);
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
                    const float sw = this->wTexture * this->scale.x * 0.5f;
                    const float sh = this->hTexture * this->scale.y * 0.5f;
                    if (device->isSphereAtFrustum(this->position, sw > sh ? sw : sh))
                        return true;
                    if (device->isSphereAtFrustum(this->position, dim.x > dim.y ? dim.x : dim.y))
                        return true;
                }
                else
                {
                    CUBE   base;
                    const float sw = this->wTexture * this->scale.x * 0.5f;
                    const float sh = this->hTexture * this->scale.y * 0.5f;
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
                    const float sw = this->wTexture * this->scale.x * 0.5f;
                    const float sh = this->hTexture * this->scale.y * 0.5f;
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
                                                                           this->wTexture * this->scale.x,
                                                                           this->hTexture * this->scale.y))
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
                    const float sw = this->wTexture * this->scale.x * 0.5f;
                    const float sh = this->hTexture * this->scale.y * 0.5f;
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
                                                                          this->wTexture * this->scale.x,
                                                                          this->hTexture * this->scale.y))
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
        if (this->totalAlive)
        {
            mbm::DEVICE* device = mbm::DEVICE::getInstance();
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
            
            if (this->indexStage < this->lsParticleStage.size())
            {
                const util::STAGE_PARTICLE* sPart = this->lsParticleStage[this->indexStage];
                this->updateAnimationParticle();
                return this->renderParticle(sPart);
            }
        }
        else 
        {
            this->updateAnimationParticle();
        }
        return false;
    }
    
    void PARTICLE::updateAnimationParticle()
    {
        if (this->lenArrayParticlesData)
        {
            ANIMATION* anim = this->getAnimation();
            if (this->indexStage < this->lsParticleStage.size())
            {
                mbm::DEVICE* device = mbm::DEVICE::getInstance();
                const util::STAGE_PARTICLE* sPart = this->lsParticleStage[this->indexStage];
                const float prev = this->currentTimeArise;
                this->currentTimeArise += device->delta;
                if (this->totalAlive < sPart->totalParticle)
                {
                    if (this->currentTimeArise <= sPart->ariseTime || prev <= device->delta)
                    {
                        if (prev <= 0.0f && sPart->ariseTime <= 0.0f)
                        {
                            this->_addParticle(sPart->totalParticle);
                        }
                        else
                        {
                            const float expected = ((static_cast<float>(sPart->totalParticle) / (sPart->ariseTime <= 0.0f ? this->currentTimeArise : sPart->ariseTime)) * this->currentTimeArise);
                            const auto diff = static_cast<int>(ceil(expected) - this->totalAlive);
                            if (diff > 0 && diff < INT_MAX )
                                this->_addParticle(static_cast<unsigned int>(diff));
                        }
                    }
                }
                if (anim->isEndedThisAnimation == false && this->currentTimeArise > sPart->stageTime)
                {
                    anim->isEndedThisAnimation = true;
                    if (onEndAnimation)
                    {
                        onEndAnimation(anim->nameAnimation, this);
                    }
                    if ((this->indexStage + 1) < this->lsParticleStage.size())
                    {
                        this->indexStage++;
                        sprintf(anim->nameAnimation, "stage:%d", static_cast<int>(this->indexStage + 1));
                        anim->isEndedThisAnimation = false;
                        anim->currentWayGrowingOfAnimation = false;
                        this->currentTimeArise = 0.0f;
                    }
                }
                if (anim->currentWayGrowingOfAnimation == false &&
                    this->currentTimeArise > device->delta &&
                    this->totalAlive == 0)
                {
                    anim->currentWayGrowingOfAnimation = true;
                    if (onEndAnimation)
                    {
                        sprintf(anim->nameAnimation, "particle:0");
                        onEndAnimation(anim->nameAnimation, this);
                    }
                    if ((this->indexStage) < this->lsParticleStage.size())
                    {
                        sprintf(anim->nameAnimation, "stage:%d", static_cast<int>(this->indexStage));
                    }
                }
            }
        }
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
        if (result.size() < 4)
            return this->releaseOnFail();
        if (this->lenArrayParticlesData == 0)
            return this->releaseOnFail();
        const unsigned int tTotal = this->totalAlive;
        const auto s = static_cast<const unsigned int>(std::atoi(result[1].c_str()));
        if (s != this->lenArrayParticlesData)
            return this->releaseOnFail();
        ATT_PARTICLE *particlesTemp      = this->particles;
        this->particles                  = nullptr;
        VERTEX_PARTICLE *vertexTemp      = this->buffer;
        this->buffer                     = nullptr;
        const VEC2 tMinv                 = this->minv;
        const VEC2 tMaxv                 = this->maxv;
        const char *newCodeLine = result[3].compare("nullptr") == 0 ? nullptr : result[3].c_str();
        bool enableAlphaFromColor = false;
        ANIMATION *       anim   = this->getAnimation();
        VAR_SHADER *varEnableAlphaFromColor = nullptr;
        if(anim && anim->fx.fxPS && anim->fx.fxPS->ptrCurrentShader)
        {
            varEnableAlphaFromColor = anim->fx.fxPS->ptrCurrentShader->getVarByName("enableAlphaFromColor");
            if (varEnableAlphaFromColor)
            {
                enableAlphaFromColor = varEnableAlphaFromColor->current[0] >= 0.5;
            }
        }
        const bool  ret         = this->load(result[0].c_str(), result[2].c_str(), newCodeLine, 0, true);
        if (ret && this->buffer && this->particles)
        {
            delete[] this->particles;
            delete[] this->buffer;
            this->particles = particlesTemp;
            this->buffer    = vertexTemp;
        }
        else
        {
            return this->releaseOnFail();
        }
        this->minv                    = tMinv;
        this->maxv                    = tMaxv;
        this->totalAlive              = tTotal;


        anim   = this->getAnimation();
        varEnableAlphaFromColor = (anim && anim->fx.fxPS && anim->fx.fxPS->ptrCurrentShader) ? anim->fx.fxPS->ptrCurrentShader->getVarByName("enableAlphaFromColor") : nullptr;
        if (varEnableAlphaFromColor)
        {
            if (enableAlphaFromColor)
            {
                const float data[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                memcpy(static_cast<void*>(varEnableAlphaFromColor->current), data, sizeof(data));
                varEnableAlphaFromColor->set(data, data, 1.0f);
            }
            else
            {
                const float data[4] = {0, 0, 0, 0};
                memcpy(static_cast<void*>(varEnableAlphaFromColor->current), data, sizeof(data));
                varEnableAlphaFromColor->set(data, data, 1.0f);
            }
        }
        
        #if defined DEBUG_RESTORE
        PRINT_IF_DEBUG("Particle [%s] successfully restored",log_util::basename( result[0].c_str()));
        #endif
        return ret;
    }

    bool PARTICLE::setTexture(const MESH_MBM *, const char *fileNametexture, const uint32_t stage, const bool hasAlpha)
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
    
    bool PARTICLE::createAnimationAndShader2Particle(const char *operatorShader, const char *newCodeLine)
    {
        this->releaseAnimation();
        auto anim = new ANIMATION();
        sprintf(anim->nameAnimation, "stage:%d", 1);
        anim->isEndedThisAnimation = false;
        anim->currentWayGrowingOfAnimation = false;
        anim->type = TYPE_ANIMATION_PAUSED;
        anim->blendState = (BLEND_ONE);
        this->lsAnimation.push_back(anim);
        if (!this->loadParticleShader(operatorShader, newCodeLine))
            return false;
        return true;
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
        return this->lenArrayParticlesData > 0;
    }

}