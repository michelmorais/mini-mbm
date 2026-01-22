/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include <particle-control.h>
#include <header-mesh.h>
#include <util.h>
#include <animation.h>

namespace mbm
{

    PARTICLE_CONTROL::PARTICLE_CONTROL() noexcept :
    lenArrayParticlesData(0),
    totalAlive(0),
    indexStage(0),
    currentTimeArise(0.0f),
    wTexture(0.0f),
    hTexture(0.0f),
    minv(0.0f, 0.0f),
    maxv(1.0f, 1.0f),
    particles(nullptr),
    buffer(nullptr),
    onEndAnimationParticleControl(nullptr)
    {

    }

    PARTICLE_CONTROL::~PARTICLE_CONTROL()
    {
        release();
    }

    void PARTICLE_CONTROL::release()
    {
        lenArrayParticlesData = (0);
        totalAlive = (0);
        indexStage = (0);
        currentTimeArise = (0.0f);
        wTexture = (0.0f);
        hTexture = (0.0f);
        minv.x = 0.0f;
        minv.y = 0.0f;
        maxv.x = 1.0f;
        maxv.y = 1.0f;

        if (this->buffer)
            delete[] this->buffer;
        this->buffer = nullptr;
        if (this->particles)
            delete[] this->particles;
        this->particles = nullptr;

        for (unsigned int i = 0; i < this->lsParticleStage.size(); ++i)
        {
            util::STAGE_PARTICLE* sPart = lsParticleStage[i];
            delete sPart;
        }
        lsParticleStage.clear();
    }

    void PARTICLE_CONTROL::initializeBuffer(const uint32_t totalParticleToLoad, const float width, const float height)
    {
        this->lenArrayParticlesData = totalParticleToLoad;
        this->wTexture = width;
        this->hTexture = height;
        this->totalAlive = 0;
        if (this->particles)
            delete[] this->particles;
        this->particles = new ATT_PARTICLE[this->lenArrayParticlesData];
        if (this->buffer)
            delete[] this->buffer;
        this->buffer = new VERTEX_PARTICLE[this->lenArrayParticlesData * 4];
        this->minv.x = -this->wTexture * 0.5f;
        this->maxv.x =  this->wTexture * 0.5f;
        this->minv.y = -this->hTexture * 0.5f;
        this->maxv.y =  this->hTexture * 0.5f;
    }

    void PARTICLE_CONTROL::updateParticleStage(const util::STAGE_PARTICLE* sPart, const float delta)
    {
        const VEC2  dist(maxv - minv);
        const float diffSize = sPart->maxSizeParticle - sPart->minSizeParticle;
        const float rDiff = sPart->maxColor.x - sPart->minColor.x;
        const float gDiff = sPart->maxColor.y - sPart->minColor.y;
        const float bDiff = sPart->maxColor.z - sPart->minColor.z;
        for (unsigned int i = 0; i < this->totalAlive; ++i)
        {
            ATT_PARTICLE* particle = &this->particles[i];
            VERTEX_PARTICLE* vertex = &this->buffer[i * 4];
            particle->timeLifeCurrent += delta;
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
                const float x = particle->direction.x * delta * particle->speed;
                const float y = particle->direction.y * delta * particle->speed;
                const float z = particle->direction.z * delta * particle->speed;
                float       incrSize = 0.0f;
                if (sPart->sizeMin2Max)//grow
                {
                    if (particle->aSize < sPart->maxSizeParticle)
                    {
                        incrSize = (diffSize / particle->timeLife) * delta;
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
                        incrSize = -(diffSize / particle->timeLife) * delta;
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

    void PARTICLE_CONTROL::restartParticle(const util::STAGE_PARTICLE* sPart, ATT_PARTICLE* particle, VERTEX_PARTICLE pPartBuffer[4], const VEC2* dist)
    {
        particle->aSize = sPart->sizeMin2Max ? sPart->minSizeParticle : sPart->maxSizeParticle;//grow
        const float halfSizeParticle = particle->aSize * 0.5f;
        const float x = util::getRandomFloat(sPart->minOffsetPosition.x, sPart->maxOffsetPosition.x);
        const float y = util::getRandomFloat(sPart->minOffsetPosition.y, sPart->maxOffsetPosition.y);
        const float z = util::getRandomFloat(sPart->minOffsetPosition.z, sPart->maxOffsetPosition.z);

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

        const float ax = util::getRandomFloat(sPart->minDirection.x, sPart->maxDirection.x);
        const float ay = util::getRandomFloat(sPart->minDirection.y, sPart->maxDirection.y);
        const float angle = mbm::calcAzimuth(ax, ay);
        particle->direction.x = sinf(angle);
        particle->direction.y = cosf(angle);

        const float angleZ = mbm::calcAzimuth(ax, util::getRandomFloat(sPart->minDirection.z, sPart->maxDirection.z));
        particle->direction.z = cosf(angleZ);

        vec3Normalize(&particle->direction, &particle->direction);

        particle->speed = util::getRandomFloat(sPart->minSpeed, sPart->maxSpeed);

        particle->timeLife = util::getRandomFloat(sPart->minTimeLife, sPart->maxTimeLife);
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

    void PARTICLE_CONTROL::onResuscitate(const util::STAGE_PARTICLE* sPart, const unsigned int total_To_Resuscitate)
    {
        const VEC2 dist(maxv - minv);
        while (this->totalAlive < total_To_Resuscitate)
        {
            unsigned int          index = this->totalAlive;
            VERTEX_PARTICLE* vertex = &this->buffer[index * 4]; // x4 porque nosso quadrado possui 4 vertex indexados
            this->restartParticle(sPart, &this->particles[index], vertex, &dist);
            this->totalAlive++;
        }
    }

    bool PARTICLE_CONTROL::addParticle(const unsigned int numParticles, const bool forceNow)
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
        if (this->currentTimeArise < 0.0f)
            this->currentTimeArise = 0.0f;
        return true;
    }

    uint32_t PARTICLE_CONTROL::getTotalParticleByStage(const uint32_t index) const
    {
        if (index < this->lsParticleStage.size())
            return this->lsParticleStage[index]->totalParticle;
        return 0;
    }

    void PARTICLE_CONTROL::setTotalParticleByStage(const uint32_t index, const uint32_t numParticles)
    {
        if (index < this->lsParticleStage.size())
        {
            auto* sPart = this->lsParticleStage[index];
            if (this->lenArrayParticlesData < numParticles)
            {
                const auto diff = numParticles - this->lenArrayParticlesData;
                this->addParticle(diff, false);
                sPart->totalParticle = numParticles;
            }
            else
            {
                sPart->totalParticle = numParticles;
                if (this->totalAlive > numParticles)
                    this->totalAlive = numParticles;
                this->currentTimeArise = 0;
            }
        }
    }

    util::STAGE_PARTICLE* PARTICLE_CONTROL::getStageParticle(const unsigned int index) const
    {
        if (index < this->lsParticleStage.size())
            return this->lsParticleStage[index];
        return nullptr;
    }

    util::STAGE_PARTICLE* PARTICLE_CONTROL::getStageParticle() const
    {
        if (this->indexStage < static_cast<unsigned int>(this->lsParticleStage.size()))
            return this->lsParticleStage[this->indexStage];
        return nullptr;
    }

    uint32_t PARTICLE_CONTROL::addStage()
    {
        auto  stage = new util::STAGE_PARTICLE();
        this->lsParticleStage.push_back(stage);
        return static_cast<uint32_t>(this->lsParticleStage.size());
    }

    uint32_t PARTICLE_CONTROL::getIndexStageParticle() const
    {
        return this->indexStage;
    }

    void PARTICLE_CONTROL::setIndexStageParticle(uint32_t index)
    {
        if (index < static_cast<uint32_t>(lsParticleStage.size()))
            this->indexStage = index;
    }

    uint32_t PARTICLE_CONTROL::getTotalStage() const
    {
        return static_cast<uint32_t>(this->lsParticleStage.size());
    }

    void PARTICLE_CONTROL::restartAnimationParticle()
    {
        this->indexStage = 0;
        this->totalAlive = 0;
        this->currentTimeArise = 0;
    }

    bool PARTICLE_CONTROL::_addParticle(const unsigned int numParticles)
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
        }
        return true;
    }

    uint32_t PARTICLE_CONTROL::addStageFromOther(const util::STAGE_PARTICLE* other)
    {
        auto  sPart = new util::STAGE_PARTICLE(other);
        this->lsParticleStage.push_back(sPart);
        return static_cast<uint32_t>(this->lsParticleStage.size());
    }

    void PARTICLE_CONTROL::setTotalAlive(uint32_t total)
    {
        if (total <= lenArrayParticlesData)
            this->totalAlive = total;
    }

    void PARTICLE_CONTROL::updateAnimationParticle(void* owner, ANIMATION* anim, const float delta)
    {
        if (this->lenArrayParticlesData)
        {
            if (this->indexStage < this->lsParticleStage.size())
            {
                const util::STAGE_PARTICLE* sPart = this->lsParticleStage[this->indexStage];
                const float prev = this->currentTimeArise;
                this->currentTimeArise += delta;
                if (this->totalAlive < sPart->totalParticle)
                {
                    if (this->currentTimeArise <= sPart->ariseTime || prev <= delta)
                    {
                        if (prev <= 0.0f && sPart->ariseTime <= 0.0f)
                        {
                            this->_addParticle(sPart->totalParticle);
                        }
                        else
                        {
                            const float expected = ((static_cast<float>(sPart->totalParticle) / (sPart->ariseTime <= 0.0f ? this->currentTimeArise : sPart->ariseTime)) * this->currentTimeArise);
                            const auto diff = static_cast<int>(ceil(expected) - this->totalAlive);
                            if (diff > 0 && diff < INT_MAX)
                                this->_addParticle(static_cast<unsigned int>(diff));
                        }
                    }
                }
                if (anim->isEndedThisAnimation == false && this->currentTimeArise > sPart->stageTime)
                {
                    anim->isEndedThisAnimation = true;
                    if (onEndAnimationParticleControl)
                    {
                        //onEndAnimation(anim->nameAnimation, this);
                        onEndAnimationParticleControl(owner, anim->nameAnimation);
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
                    this->currentTimeArise > delta &&
                    this->totalAlive == 0)
                {
                    anim->currentWayGrowingOfAnimation = true;
                    if (onEndAnimationParticleControl)
                    {
                        sprintf(anim->nameAnimation, "particle:0");
                        //onEndAnimation(anim->nameAnimation, this);
                        onEndAnimationParticleControl(owner, anim->nameAnimation);
                    }
                    if ((this->indexStage) < this->lsParticleStage.size())
                    {
                        sprintf(anim->nameAnimation, "stage:%d", static_cast<int>(this->indexStage));
                    }
                }
            }
        }
    }
}