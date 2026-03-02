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

#include <animation.h>
#include <shader-var-cfg.h>
#include <header-mesh.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <device.h>
#include <renderizable.h>
#include <util-interface.h>


namespace mbm
{

    EFFECT_SHADER::EFFECT_SHADER()
        noexcept : statusFx(FX_GROWING), typeAnim(TYPE_ANIMATION_PAUSED), ptrCurrentShader(nullptr), timeAnimation(1.0f)
    {
    }

    EFFECT_SHADER::~EFFECT_SHADER()
    {
        this->ptrCurrentShader = nullptr;
        for (const auto & i : this->lsPtrShader)
        {
            BASE_SHADER *ptr = i.second;
            delete ptr;
        }
        this->lsPtrShader.clear();
    }

    BASE_SHADER * EFFECT_SHADER::loadEffect(const char *fileNameShader, const char *code, const TYPE_ANIMATION typeAnimationShader)
    {
        if (fileNameShader == nullptr || code == nullptr)
        {
            this->disableEffect();
            return nullptr;
        }
        this->typeAnim = typeAnimationShader;
        BASE_SHADER *ptr = this->lsPtrShader[fileNameShader];
        if (ptr)
        {
            this->ptrCurrentShader = ptr;
            this->restart();
            return ptr;
        }
        auto psNew = new BASE_SHADER();
        this->ptrCurrentShader = psNew;
        if (!this->ptrCurrentShader->loadShader(fileNameShader, code))
        {
            delete psNew;
            this->ptrCurrentShader = nullptr;
            return nullptr;
        }
        this->lsPtrShader[fileNameShader] = psNew;
        return psNew;
    }

    void EFFECT_SHADER::disableEffect()
    {
        this->ptrCurrentShader = nullptr;
        this->typeAnim = TYPE_ANIMATION_PAUSED;
    }

    void EFFECT_SHADER::restart()
    {
        if (this->ptrCurrentShader)
        {
            switch (this->typeAnim)
            {
                case TYPE_ANIMATION_PAUSED:
                case TYPE_ANIMATION_GROWING:
                case TYPE_ANIMATION_GROWING_LOOP:
                case TYPE_ANIMATION_RECURSIVE:
                case TYPE_ANIMATION_RECURSIVE_LOOP:
                {
                    this->statusFx = FX_GROWING;
                    for (uint32_t i = 0; i < this->ptrCurrentShader->getTotalVar(); ++i)
                    {
                        VAR_SHADER *var = this->ptrCurrentShader->getVar(i);
                        for (int j = 0; j < var->sizeVar; ++j)
                        {
                            var->control[j] = true;
                            var->current[j] = var->min[j];
                        }
                    }
                }
                break;
                case TYPE_ANIMATION_DECREASING:
                case TYPE_ANIMATION_DECREASING_LOOP:
                {
                    this->statusFx = FX_DECREASING;
                    for (uint32_t i = 0; i < this->ptrCurrentShader->getTotalVar(); ++i)
                    {
                        VAR_SHADER *var = this->ptrCurrentShader->getVar(i);
                        for (int j = 0; j < var->sizeVar; ++j)
                        {
                            var->control[j] = true;
                            var->current[j] = var->max[j];
                        }
                    }
                }
                break;
                default:
                { /*do nothing*/
                }
                break;
            }
        }
    }

    void EFFECT_SHADER::updateEffect(const float delta)
    {
        if (this->ptrCurrentShader)
        {
            switch (this->typeAnim)
            {
                case TYPE_ANIMATION_PAUSED:
                {
                    const uint32_t iTotalVars = this->ptrCurrentShader->getTotalVar();
                    for (uint32_t i = 0; i < iTotalVars; ++i)
                    {
                        VAR_SHADER *var = this->ptrCurrentShader->getVar(i);
                        for (int j = 0; j < var->sizeVar; ++j)
                        {
                            var->current[j] = var->min[j];
                        }
                    }
                }
                break;
                case TYPE_ANIMATION_GROWING:
                {
                    uint32_t numStopped = 0;
                    const uint32_t iTotalVars = this->ptrCurrentShader->getTotalVar();
                    for (uint32_t i = 0; i < iTotalVars; ++i)
                    {
                        int iTotalEndStep = 0;
                        VAR_SHADER *var = this->ptrCurrentShader->getVar(i);
                        for (int j = 0; j < var->sizeVar; ++j)
                        {
                            if (var->control[j])
                            {
                                const float incr = delta * var->step[j];
                                var->current[j] += incr;
                                if (var->granThen[j])
                                {
                                    if (var->current[j] >= var->max[j])
                                    {
                                        var->current[j] = var->max[j];
                                        var->control[j] = false;
                                    }
                                }
                                else
                                {
                                    if (var->current[j] <= var->max[j])
                                    {
                                        var->current[j] = var->max[j];
                                        var->control[j] = false;
                                    }
                                }
                            }
                            else
                            {
                                ++iTotalEndStep;
                            }
                        }
                        if (iTotalEndStep == var->sizeVar)
                            ++numStopped;
                    }
                    if (this->statusFx != FX_END_CALLBACK && numStopped == iTotalVars)
                        this->statusFx = FX_END;
                }
                break;
                case TYPE_ANIMATION_GROWING_LOOP:
                {
                    for (uint32_t i = 0; i < this->ptrCurrentShader->getTotalVar(); ++i)
                    {
                        VAR_SHADER *var = this->ptrCurrentShader->getVar(i);
                        for (int j = 0; j < var->sizeVar; ++j)
                        {
                            const float incr = delta * var->step[j];
                            var->current[j] += incr;
                            if (var->granThen[j])
                            {
                                if (var->current[j] >= var->max[j])
                                    var->current[j] = var->min[j];
                            }
                            else
                            {
                                if (var->current[j] <= var->max[j])
                                    var->current[j] = var->min[j];
                            }
                        }
                    }
                }
                break;
                case TYPE_ANIMATION_DECREASING:
                {
                    uint32_t numStopped = 0;
                    const uint32_t iTotalVars = this->ptrCurrentShader->getTotalVar();
                    for (uint32_t i = 0; i < iTotalVars; ++i)
                    {
                        int iTotalEndStep = 0;
                        VAR_SHADER *var = this->ptrCurrentShader->getVar(i);
                        for (int j = 0; j < var->sizeVar; ++j)
                        {
                            if (var->control[j])
                            {
                                const float incr = delta * var->step[j];
                                var->current[j] -= incr;
                                if (var->granThen[j])
                                {
                                    if (var->current[j] <= var->min[j])
                                    {
                                        var->current[j] = var->min[j];
                                        var->control[j] = false;
                                    }
                                }
                                else
                                {
                                    if (var->current[j] >= var->min[j])
                                    {
                                        var->current[j] = var->min[j];
                                        var->control[j] = false;
                                    }
                                }
                            }
                        }
                        if (iTotalEndStep == var->sizeVar)
                            ++numStopped;
                    }
                    if (this->statusFx != FX_END_CALLBACK && numStopped == iTotalVars)
                        this->statusFx = FX_END;
                }
                break;
                case TYPE_ANIMATION_DECREASING_LOOP:
                {
                    for (uint32_t i = 0; i < this->ptrCurrentShader->getTotalVar(); ++i)
                    {
                        VAR_SHADER *var = this->ptrCurrentShader->getVar(i);
                        for (int j = 0; j < var->sizeVar; ++j)
                        {
                            const float incr = delta * var->step[j];
                            var->current[j] -= incr;
                            if (var->granThen[j])
                            {
                                if (var->current[j] <= var->min[j])
                                    var->current[j] = var->max[j];
                            }
                            else
                            {
                                if (var->current[j] >= var->min[j])
                                    var->current[j] = var->max[j];
                            }
                        }
                    }
                }
                break;
                case TYPE_ANIMATION_RECURSIVE:
                {
                    switch (this->statusFx)
                    {
                        case FX_GROWING:
                        {
                            int totalFinished = 0;
                            int totalExpected = 0;
                            for (uint32_t i = 0; i < this->ptrCurrentShader->getTotalVar(); ++i)
                            {
                                VAR_SHADER *var = this->ptrCurrentShader->getVar(i);
                                totalExpected += var->sizeVar;
                                for (int j = 0; j < var->sizeVar; ++j)
                                {
                                    if (var->control[j])
                                    {
                                        const float incr = delta * var->step[j];
                                        var->current[j] += incr;
                                        if (var->granThen[j])
                                        {
                                            if (var->current[j] >= var->max[j])
                                            {
                                                var->current[j] = var->max[j];
                                                var->control[j] = false;
                                            }
                                        }
                                        else
                                        {
                                            if (var->current[j] <= var->max[j])
                                            {
                                                var->current[j] = var->max[j];
                                                var->control[j] = false;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        totalFinished++;
                                    }
                                }
                            }
                            if (totalFinished == totalExpected)
                            {
                                for (uint32_t i = 0; i < this->ptrCurrentShader->getTotalVar(); ++i)
                                {
                                    VAR_SHADER *var = this->ptrCurrentShader->getVar(i);
                                    memset(var->control, 1, sizeof(var->control));
                                }
                                this->statusFx = FX_DECREASING;
                            }
                        }
                        break;
                        case FX_DECREASING:
                        {
                            int totalFinished = 0;
                            int totalExpected = 0;
                            for (uint32_t i = 0; i < this->ptrCurrentShader->getTotalVar(); ++i)
                            {
                                VAR_SHADER *var = this->ptrCurrentShader->getVar(i);
                                totalExpected += var->sizeVar;
                                for (int j = 0; j < var->sizeVar; ++j)
                                {
                                    if (var->control[j])
                                    {
                                        const float incr = delta * var->step[j];
                                        var->current[j] -= incr;
                                        if (var->granThen[j])
                                        {
                                            if (var->current[j] <= var->min[j])
                                            {
                                                var->current[j] = var->min[j];
                                                var->control[j] = false;
                                            }
                                        }
                                        else
                                        {
                                            if (var->current[j] >= var->min[j])
                                            {
                                                var->current[j] = var->min[j];
                                                var->control[j] = false;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        totalFinished++;
                                    }
                                }
                            }
                            if (totalFinished == totalExpected)
                            {
                                for (uint32_t i = 0; i < this->ptrCurrentShader->getTotalVar(); ++i)
                                {
                                    VAR_SHADER *var = this->ptrCurrentShader->getVar(i);
                                    memset(var->control, 1, sizeof(var->control));
                                }
                                this->statusFx = FX_END;
                            }
                        }
                        break;
                        case FX_END:
                        case FX_END_CALLBACK: // End
                        {
                            // Do Nothing
                        }
                        break;
                        default: { this->statusFx = FX_GROWING;
                        }
                        break;
                    }
                }
                break;
                case TYPE_ANIMATION_RECURSIVE_LOOP:
                {
                    switch (this->statusFx)
                    {
                        case FX_GROWING: // crescente
                        {
                            int totalFinished = 0;
                            int totalExpected = 0;
                            for (uint32_t i = 0; i < this->ptrCurrentShader->getTotalVar(); ++i)
                            {
                                VAR_SHADER *var = this->ptrCurrentShader->getVar(i);
                                totalExpected += var->sizeVar;
                                for (int j = 0; j < var->sizeVar; ++j)
                                {
                                    if (var->control[j])
                                    {
                                        const float incr = delta * var->step[j];
                                        var->current[j] += incr;
                                        if (var->granThen[j])
                                        {
                                            if (var->current[j] >= var->max[j])
                                            {
                                                var->current[j] = var->max[j];
                                                var->control[j] = false;
                                            }
                                        }
                                        else
                                        {
                                            if (var->current[j] <= var->max[j])
                                            {
                                                var->current[j] = var->max[j];
                                                var->control[j] = false;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        totalFinished++;
                                    }
                                }
                            }
                            if (totalFinished == totalExpected)
                            {
                                for (uint32_t i = 0; i < this->ptrCurrentShader->getTotalVar(); ++i)
                                {
                                    VAR_SHADER *var = this->ptrCurrentShader->getVar(i);
                                    memset(var->control, 1, sizeof(var->control));
                                }
                                this->statusFx = FX_DECREASING;
                            }
                        }
                        break;
                        case FX_DECREASING:
                        {
                            int totalFinished = 0;
                            int totalExpected = 0;
                            for (uint32_t i = 0; i < this->ptrCurrentShader->getTotalVar(); ++i)
                            {
                                VAR_SHADER *var = this->ptrCurrentShader->getVar(i);
                                totalExpected += var->sizeVar;
                                for (int j = 0; j < var->sizeVar; ++j)
                                {
                                    if (var->control[j])
                                    {
                                        const float incr = delta * var->step[j];
                                        var->current[j] -= incr;
                                        if (var->granThen[j])
                                        {
                                            if (var->current[j] <= var->min[j])
                                            {
                                                var->current[j] = var->min[j];
                                                var->control[j] = false;
                                            }
                                        }
                                        else
                                        {
                                            if (var->current[j] >= var->min[j])
                                            {
                                                var->current[j] = var->min[j];
                                                var->control[j] = false;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        totalFinished++;
                                    }
                                }
                            }
                            if (totalFinished == totalExpected)
                            {
                                for (uint32_t i = 0; i < this->ptrCurrentShader->getTotalVar(); ++i)
                                {
                                    VAR_SHADER *var = this->ptrCurrentShader->getVar(i);
                                    memset(var->control, 1, sizeof(var->control));
                                }
                                this->statusFx = FX_GROWING;
                            }
                        }
                        break;
                        default: { this->statusFx = FX_GROWING;
                        }
                        break;
                    }
                }
                break;
                default:
                { /*do nothing*/
                }
                break;
            }
        }
    }

    bool EFFECT_SHADER::isEndedFx()const
    {
        if(this->statusFx == FX_END)
            return true;
        if(this->statusFx == FX_END_CALLBACK)
            return true;
        return false;
    }

    void EFFECT_SHADER::forceEndFx()
    {
        if(this->ptrCurrentShader)
        {
            switch (this->typeAnim)
            {
                case TYPE_ANIMATION_PAUSED:
                case TYPE_ANIMATION_GROWING:
                case TYPE_ANIMATION_GROWING_LOOP:
                {
                    for (uint32_t i = 0; i < this->ptrCurrentShader->getTotalVar(); ++i)
                    {
                        VAR_SHADER *thisVar = this->ptrCurrentShader->getVar(i);
                        memcpy(thisVar->current, thisVar->max, sizeof(thisVar->current));
                    }
                }
                break;
                case TYPE_ANIMATION_DECREASING:
                case TYPE_ANIMATION_DECREASING_LOOP:
                case TYPE_ANIMATION_RECURSIVE:
                case TYPE_ANIMATION_RECURSIVE_LOOP:
                {
                    for (uint32_t i = 0; i < this->ptrCurrentShader->getTotalVar(); ++i)
                    {
                        VAR_SHADER *thisVar = this->ptrCurrentShader->getVar(i);
                        memcpy(thisVar->current, thisVar->min, sizeof(thisVar->current));
                    }
                }
                break;
            }
        }
        this->statusFx = FX_END;
    }

    bool EFFECT_SHADER::endEffect()
    {
        if (this->statusFx == FX_END)
        {
            this->statusFx = FX_END_CALLBACK;
            return true;
        }
        return false;
    }

    bool EFFECT_SHADER::setNewTimeAnim(const float newTimeAnim)
    {
        this->timeAnimation = newTimeAnim;
        if (this->ptrCurrentShader == nullptr)
            return false;
        if (this->ptrCurrentShader->getTotalVar() == 0)
        {
            return true;
        }
        for (uint32_t i = 0; i < this->ptrCurrentShader->getTotalVar(); ++i)
        {
            VAR_SHADER *thisVar = this->ptrCurrentShader->getVar(i);
            thisVar->set(thisVar->min, thisVar->max, newTimeAnim);
        }
        return true;
    }

    bool EFFECT_SHADER::adjustMinMax(const uint32_t indexVar, const float min[4], const float max[4], const float timeAnim)
    {
        if (this->ptrCurrentShader == nullptr)
            return false;
        if (this->ptrCurrentShader->getTotalVar() == 0)
            return true;
        if (indexVar >= this->ptrCurrentShader->getTotalVar())
            return false;
        VAR_SHADER *var = this->ptrCurrentShader->getVar(indexVar);
        var->set(min, max, timeAnim);
        return true;
    }

    ANIMATION::ANIMATION()
    {
        blendState                   = BLEND_DISABLE;
        currentTimeToChangeAnimation = 0.0f;
        indexInitialFrame            = 0;
        indexFinalFrame              = 0;
        intervalChangeFrame          = 1;
        indexCurrentFrame            = 0;
        isEndedThisAnimation         = false;
        currentWayGrowingOfAnimation = true;
        memset(nameAnimation, 0, sizeof(nameAnimation));
        type						= TYPE_ANIMATION_PAUSED;
    }

    void ANIMATION::restartAnimation()
    {
        isEndedThisAnimation = false;
        fx.fxPS->restart();
        fx.fxVS->restart();
        
        if (type == TYPE_ANIMATION_DECREASING || type == TYPE_ANIMATION_DECREASING_LOOP)
        {
            this->currentWayGrowingOfAnimation = false;
            indexCurrentFrame                  = this->indexFinalFrame;
        }
        else
        {
            this->currentWayGrowingOfAnimation = true;
            indexCurrentFrame                  = this->indexInitialFrame;
        }
    }

    void ANIMATION::updateAnimation(const float delta, RENDERIZABLE *me,
                                    OnEndAnimation onEndAnimation,
                                    OnEndEffect onEndFX)
    {
        if (delta <= 0.0f)
            return;
        if(fx.fxPS->isEndedFx() == false)
        {
            fx.fxPS->updateEffect(delta);
            if (fx.fxPS->endEffect())
            {
                if (onEndFX && fx.fxPS->ptrCurrentShader)
                    onEndFX(fx.fxPS->ptrCurrentShader->fileName.c_str(),me);
            }
        }
        if (fx.fxVS->isEndedFx() == false)
        {
            fx.fxVS->updateEffect(delta);
            if (fx.fxVS->endEffect())
            {
                if (onEndFX && fx.fxVS->ptrCurrentShader)
                    onEndFX(fx.fxVS->ptrCurrentShader->fileName.c_str(), me);
            }
        }
        if (type != TYPE_ANIMATION_PAUSED)
        {
            switch (type)
            {
                case TYPE_ANIMATION_GROWING:
                case TYPE_ANIMATION_DECREASING:
                case TYPE_ANIMATION_RECURSIVE:
                {
                    if (isEndedThisAnimation)
                        return;
                }
                break;
                default: break;
            }
            currentTimeToChangeAnimation += delta;
            uint32_t incrDiff = 0;
            if (currentTimeToChangeAnimation >= intervalChangeFrame)
            {
                incrDiff = static_cast<uint32_t>(currentTimeToChangeAnimation / intervalChangeFrame);
                currentTimeToChangeAnimation -= (incrDiff * intervalChangeFrame);
            }
            if (incrDiff)
            {
                switch (type)
                {
                    case 1: // 1:Crescente
                    {
                        indexCurrentFrame += incrDiff;
                        if (indexCurrentFrame > indexFinalFrame)
                        {
                            indexCurrentFrame            = indexFinalFrame;
                            isEndedThisAnimation         = true;
                            currentWayGrowingOfAnimation = true;
                            if (onEndAnimation)
                                onEndAnimation(this->nameAnimation, me);
                        }
                    }
                    break;
                    case 2: // 2:Crescente Com Loop
                    {
                        indexCurrentFrame += incrDiff;
                        if (indexCurrentFrame > indexFinalFrame)
                            indexCurrentFrame        = (indexCurrentFrame - indexFinalFrame) - 1 + indexInitialFrame;
                        currentWayGrowingOfAnimation = true;
                    }
                    break;
                    case 3: // 3:Decrescente
                    {
                        indexCurrentFrame -= incrDiff;
                        if (indexCurrentFrame < indexInitialFrame)
                        {
                            isEndedThisAnimation = true;
                            indexCurrentFrame    = indexInitialFrame;
                            if (onEndAnimation)
                                onEndAnimation(this->nameAnimation, me);
                        }
                        currentWayGrowingOfAnimation = false;
                    }
                    break;
                    case 4: // 4:Decrescente Com Loop
                    {
                        indexCurrentFrame -= incrDiff;
                        if (indexCurrentFrame < indexInitialFrame)
                        {
                            incrDiff          = static_cast<uint32_t>(indexInitialFrame - indexCurrentFrame - 1);
                            indexCurrentFrame = static_cast<int>(indexFinalFrame) - static_cast<int>(incrDiff);
                        }
                        currentWayGrowingOfAnimation = false;
                    }
                    break;
                    case 5: // 5:Recursiva
                    {
                        if (currentWayGrowingOfAnimation)
                        {
                            indexCurrentFrame += incrDiff;
                            if (indexCurrentFrame > indexFinalFrame)
                            {
                                indexCurrentFrame            = indexFinalFrame - 1;
                                if (indexCurrentFrame < indexInitialFrame)
                                    indexCurrentFrame = indexInitialFrame;
                                currentWayGrowingOfAnimation = false;
                            }
                        }
                        else
                        {
                            indexCurrentFrame -= incrDiff;
                            if (indexCurrentFrame < indexInitialFrame)
                            {
                                isEndedThisAnimation = true;
                                indexCurrentFrame    = indexInitialFrame;
                                if (onEndAnimation)
                                    onEndAnimation(this->nameAnimation, me);
                            }
                        }
                    }
                    break;
                    case 6: // 6:Recursiva Com Loop
                    {
                        if (currentWayGrowingOfAnimation)
                        {
                            indexCurrentFrame += incrDiff;
                            if (indexCurrentFrame > indexFinalFrame)
                            {
                                indexCurrentFrame = indexFinalFrame - 1;
                                if(indexCurrentFrame < indexInitialFrame)
                                    indexCurrentFrame = indexInitialFrame;
                                currentWayGrowingOfAnimation = false;
                            }
                        }
                        else
                        {
                            indexCurrentFrame -= incrDiff;
                            if (indexCurrentFrame < indexInitialFrame)
                            {
                                currentWayGrowingOfAnimation = true;
                                indexCurrentFrame = indexInitialFrame + 1;
                                if(indexCurrentFrame > indexFinalFrame)
                                    indexCurrentFrame = indexFinalFrame;
                            }
                        }
                    }
                    break;
                    default: break;
                }
            }
            if (indexCurrentFrame < indexInitialFrame)
                indexCurrentFrame = indexInitialFrame;
            else if (indexCurrentFrame > indexFinalFrame)
                indexCurrentFrame = indexFinalFrame;
        }
    }

    ANIMATION_MANAGER::ANIMATION_MANAGER() noexcept : 
    indexCurrentAnimation(0), onEndAnimation(nullptr),onEndFx(nullptr)
    {
    }

    ANIMATION_MANAGER::~ANIMATION_MANAGER()
    {
        this->indexCurrentAnimation = 0;
        this->releaseAnimation();
    }

    void ANIMATION_MANAGER::populateTextureStage2FromMesh(MESH_MBM *mesh)
    {
        TEXTURE_MANAGER *texMan = TEXTURE_MANAGER::getInstance();
        for (std::vector<util::INFO_ANIMATION::INFO_HEADER_ANIM *>::size_type i = 0; i < mesh->infoAnimation.lsHeaderAnim.size(); ++i)
        {
            util::INFO_ANIMATION::INFO_HEADER_ANIM * infoHead = mesh->infoAnimation.lsHeaderAnim[i];
            if(infoHead->effectShader)
            {
                util::INFO_SHADER_DATA *infoPS         = infoHead->effectShader->dataPS;
                ANIMATION *anim                         = i < this->lsAnimation.size() ? this->lsAnimation[i] : nullptr;
                if (infoPS && infoPS->fileNameTextureStage2)
                {
                    TEXTURE *  tex  = texMan->load(infoPS->fileNameTextureStage2, true);
                    if (anim && tex)
                        anim->fx.textureOverrideStage2 = tex;
                }

                util::INFO_SHADER_DATA *infoVS = infoHead->effectShader->dataVS;
                if (infoVS && infoVS->fileNameTextureStage2)
                {
                    TEXTURE *  tex  = texMan->load(infoVS->fileNameTextureStage2, true);
                    if (anim && tex)
                        anim->fx.textureOverrideStage2 = tex;
                }
                if(anim)
                    anim->blendState = static_cast<mbm::BLEND_STATE>(infoHead->headerAnim->blendState);
            }
        }
    }

    bool ANIMATION_MANAGER::populateAnimationFromHeader(MESH_MBM *mesh, util::HEADER_ANIMATION *header, const uint32_t index)
    {
        if (mesh == nullptr || header == nullptr)
        {
            ERROR_LOG( "error on add shader mesh [%p] header [%p]",mesh,header);
            return false;
        }
        mbm::DEVICE *          device = mbm::DEVICE::getInstance();
        mbm::TEXTURE_MANAGER *texMan  = mbm::TEXTURE_MANAGER::getInstance();
        auto                   anim   = new ANIMATION();
        anim->indexFinalFrame         = header->finalFrame;
        anim->indexInitialFrame       = header->initialFrame;
        anim->intervalChangeFrame     = header->timeBetweenFrame;
        anim->type                    = static_cast<TYPE_ANIMATION>(header->typeAnimation);
        if (strlen(header->nameAnimation) >= 1)
            strncpy(anim->nameAnimation, header->nameAnimation, sizeof(anim->nameAnimation));
        else
            strncpy(anim->nameAnimation, "default",sizeof(anim->nameAnimation));
        this->lsAnimation.push_back(anim);
        if (index < mesh->infoAnimation.lsHeaderAnim.size()) // animation total 
        {
            util::INFO_ANIMATION::INFO_HEADER_ANIM *infoHead = mesh->infoAnimation.lsHeaderAnim[index];
            util::INFO_FX *infoShaderStep = infoHead->effectShader;
            if (infoShaderStep && infoShaderStep->dataPS)
            {
                util::INFO_SHADER_DATA *data   = infoShaderStep->dataPS;
                anim->fx.fxPS->timeAnimation  = data->timeAnimation;
                anim->fx.blendOperation          = infoShaderStep->blendOperation;
                if (data->fileNameTextureStage2)
                    anim->fx.textureOverrideStage2 = texMan->load(data->fileNameTextureStage2, true);
                SHADER_CFG *cfgShader              = device->cfg.getShader(data->fileNameShader);
                if (cfgShader)
                {
                    if (!anim->fx.fxPS->loadEffect(data->fileNameShader,
                                                        cfgShader->codeShader.c_str(), // Code
                                                        static_cast<TYPE_ANIMATION>(data->typeAnimation)))
                    {
                        ERROR_LOG( "error to load shader %s at cfg file!",data->fileNameShader);
                        return false;
                    }
                }
                else
                {
                    ERROR_LOG( "Shader %s not found at cfg shader list!",data->fileNameShader);
                    return false;
                }
            }
            if (infoShaderStep && infoShaderStep->dataVS)
            {
                util::INFO_SHADER_DATA *data   = infoShaderStep->dataVS;
                anim->fx.fxVS->timeAnimation  = data->timeAnimation;
                anim->fx.blendOperation          = infoShaderStep->blendOperation;
                if (data->fileNameTextureStage2)
                    anim->fx.textureOverrideStage2 = texMan->load(data->fileNameTextureStage2, true);
                SHADER_CFG *cfgShader              = device->cfg.getShader(data->fileNameShader);
                if (cfgShader)
                {
                    if (!anim->fx.fxVS->loadEffect(data->fileNameShader,
                                                        cfgShader->codeShader.c_str(), // Code
                                                        static_cast<TYPE_ANIMATION>(data->typeAnimation)))
                    {
                        ERROR_LOG( "error to load shader %s at cfg file!",data->fileNameShader);
                        return false;
                    }
                }
                else
                {
                    ERROR_LOG( "Shader %s not found at cfg shader list!",data->fileNameShader);
                    return false;
                }
            }
        }
        // compile shader in pair
        util::INFO_ANIMATION::INFO_HEADER_ANIM *infoHead = mesh->infoAnimation.lsHeaderAnim[index];
        const FVF_PROVIDE_BY_ENGINE fvf = mesh->getBuffer(0)->pBufferGL->fvf;
        if (anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader, fvf))
        {
            if(infoHead->effectShader && infoHead->effectShader->blendOperation != 0)
                anim->fx.blendOperation = infoHead->effectShader->blendOperation;

            if (anim->fx.fxPS->ptrCurrentShader)
            {
                util::INFO_FX *infoShaderStep = infoHead->effectShader;
                if (infoShaderStep && infoShaderStep->dataPS && infoShaderStep->dataPS->fileNameShader)
                {
                    util::INFO_SHADER_DATA *data  = infoShaderStep->dataPS;
                    anim->fx.fxPS->timeAnimation = data->timeAnimation;
                    anim->fx.fxPS->typeAnim      = static_cast<TYPE_ANIMATION>(data->typeAnimation);
                    SHADER_CFG *cfgShader          = device->cfg.getShader(data->fileNameShader);
                    if (cfgShader)
                    {
                        for (auto var : cfgShader->lsVar)
                        {
                            if (!anim->fx.fxPS->ptrCurrentShader->addVar(var->name.c_str(), var->type, var->Default,
                                                                            anim->fx.shader.ptrShaderSpecific, true))
                            {
                                ERROR_LOG( "failed to include variable %s shader %s!",var->name.c_str(), data->fileNameShader);
                                return false;
                            }
                        }
                        if(infoShaderStep->dataPS->lenVars == static_cast<int>(anim->fx.fxPS->ptrCurrentShader->getTotalVar()))
                        {
                            int indexVar = 0;
                            for (uint32_t i = 0; i < anim->fx.fxPS->ptrCurrentShader->getTotalVar(); ++i)
                            {
                                VAR_SHADER *varShader = anim->fx.fxPS->ptrCurrentShader->getVar(i);
                                if (varShader)
                                {
                                    varShader->set(&data->min[indexVar], &data->max[indexVar], data->timeAnimation);
                                    indexVar += 4;
                                }
                            }
                        }
                        else
                        {
                            ERROR_LOG( "Unexpected number of variable for shader [%s]!\nDid the shader change???\nTotal vars [%d] expected [%d]", data->fileNameShader,infoShaderStep->dataPS->lenVars,anim->fx.fxPS->ptrCurrentShader->getTotalVar());
                        }
                        if (data->fileNameTextureStage2)
                        {
                            TEXTURE_MANAGER *man = TEXTURE_MANAGER::getInstance();
                            anim->fx.textureOverrideStage2   = man->load(data->fileNameTextureStage2, true);
                        }
                    }
                    else
                    {
                        ERROR_LOG( "Shader %s not found at cfg shader list!",data->fileNameShader);
                        return false;
                    }
                }
            }
            if (anim->fx.fxVS->ptrCurrentShader)
            {
                util::INFO_FX *infoShaderStep = infoHead->effectShader;
                if (infoShaderStep && infoShaderStep->dataVS && infoShaderStep->dataVS->fileNameShader)
                {
                    util::INFO_SHADER_DATA *data  = infoShaderStep->dataVS;
                    anim->fx.fxVS->timeAnimation = data->timeAnimation;
                    anim->fx.fxVS->typeAnim      = static_cast<TYPE_ANIMATION>(data->typeAnimation);
                    SHADER_CFG *cfgShader          = device->cfg.getShader(data->fileNameShader);
                    if (cfgShader)
                    {
                        for (auto var : cfgShader->lsVar)
                        {
                            if (!anim->fx.fxVS->ptrCurrentShader->addVar(var->name.c_str(), var->type, var->Default,
                                                                            anim->fx.shader.ptrShaderSpecific, false))
                            {
                                ERROR_LOG( "failed to include variable [%s] shader [%s]!",var->name.c_str(), data->fileNameShader);
                                return false;
                            }
                        }
                        if(infoShaderStep->dataVS->lenVars == static_cast<int>(anim->fx.fxVS->ptrCurrentShader->getTotalVar()))
                        {
                            int indexVar = 0;
                            for (uint32_t i = 0; i < anim->fx.fxVS->ptrCurrentShader->getTotalVar(); ++i)
                            {
                                VAR_SHADER *varShader = anim->fx.fxVS->ptrCurrentShader->getVar(i);
                                if (varShader)
                                {
                                    varShader->set(&data->min[indexVar], &data->max[indexVar], data->timeAnimation);
                                    indexVar += 4;
                                }
                            }
                        }
                        else
                        {
                            ERROR_LOG( "Unexpected number of variable for shader [%s]!\nDid the shader change???\nTotal vars [%d] expected [%d]", data->fileNameShader,infoShaderStep->dataVS->lenVars,anim->fx.fxVS->ptrCurrentShader->getTotalVar());
                        }
                        if (data->fileNameTextureStage2)
                        {
                            TEXTURE_MANAGER *man = TEXTURE_MANAGER::getInstance();
                            anim->fx.textureOverrideStage2   = man->load(data->fileNameTextureStage2, true);
                        }
                    }
                    else
                    {
                        ERROR_LOG( "Shader %s not found at cfg shader list!",data->fileNameShader);
                        return false;
                    }
                }
            }
        }
        else
        {
            ERROR_LOG( "Error on compile shader animation:[%s]", anim->nameAnimation);
            return false;
        }
        return true;
    }

    ANIMATION * ANIMATION_MANAGER::getAnimation() const
    {
        if (this->indexCurrentAnimation < this->lsAnimation.size())
            return this->lsAnimation[this->indexCurrentAnimation];
        return nullptr;
    }

    ANIMATION * ANIMATION_MANAGER::getAnimation(const uint32_t index) const
    {
        if (index < this->lsAnimation.size())
            return this->lsAnimation[index];
        return nullptr;
    }

    uint32_t ANIMATION_MANAGER::getTotalAnimation() const
    {
        return static_cast<uint32_t>(this->lsAnimation.size());
    }

    uint32_t ANIMATION_MANAGER::getIndexAnimation() const
    {
        return this->indexCurrentAnimation;
    }

    bool ANIMATION_MANAGER::setAnimationByIndex(const uint32_t newIndex)
    {
        if (newIndex < this->lsAnimation.size())
        {
            this->indexCurrentAnimation = newIndex;
            ANIMATION *anim             = this->lsAnimation[this->indexCurrentAnimation];
            anim->restartAnimation();
            return true;
        }
        return false;
    }

    void ANIMATION_MANAGER::setAnimation(const char *name)
    {
        const std::vector<ANIMATION *>::size_type s =  lsAnimation.size();
        for (std::vector<ANIMATION *>::size_type i = 0; i < s; ++i)
        {
            ANIMATION * anim = lsAnimation[i];
            if (anim && strcmp(anim->nameAnimation, name) == 0)
            {
                this->indexCurrentAnimation = static_cast<uint32_t>(i);
                anim->restartAnimation();
                break;
            }
        }
    }

    void ANIMATION_MANAGER::restartAnimation()
    {
        if (this->indexCurrentAnimation < this->lsAnimation.size())
        {
            ANIMATION *anim = this->lsAnimation[this->indexCurrentAnimation];
            anim->restartAnimation();
        }
    }

    void ANIMATION_MANAGER::removeAnimation(const uint32_t index)
    {
        if (index < this->lsAnimation.size())
        {
            mbm::ANIMATION *anim = this->lsAnimation[index];
            delete anim;
            this->lsAnimation.erase(this->lsAnimation.begin() + index);
            if (this->indexCurrentAnimation > this->lsAnimation.size())
            {
                if (this->lsAnimation.size())
                    this->indexCurrentAnimation = static_cast<uint32_t>(this->lsAnimation.size() - 1);
                else
                    this->indexCurrentAnimation = 0;
            }
            else if (this->lsAnimation.size())
                this->indexCurrentAnimation = static_cast<uint32_t>(this->lsAnimation.size() - 1);
        }
    }

    char * ANIMATION_MANAGER::getNameAnimation(const uint32_t index) const
    {
        if (index < lsAnimation.size())
            return lsAnimation[index]->nameAnimation;
        return nullptr;
    }

    char * ANIMATION_MANAGER::getNameAnimation() const
    {
        if (this->indexCurrentAnimation < lsAnimation.size())
            return lsAnimation[this->indexCurrentAnimation]->nameAnimation;
        return nullptr;
    }

    uint32_t ANIMATION_MANAGER::addAnimation()
    {
        auto anim = new ANIMATION();
        this->lsAnimation.push_back(anim);
        this->indexCurrentAnimation = static_cast<uint32_t>(this->lsAnimation.size() - 1);
        RENDERIZABLE* r = dynamic_cast<RENDERIZABLE*>(this);
        const FVF_PROVIDE_BY_ENGINE fvf = r ? r->getFvfFromBuffer() : FVF_PROVIDE_BY_ENGINE::FVF_NONE;
        if (!anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader, fvf))
        {
            ERROR_AT(__LINE__,__FILE__, "error on add animation");
        }
        return this->indexCurrentAnimation;
    }

    bool ANIMATION_MANAGER::isEndedAnimation() const noexcept
    {
        if (this->indexCurrentAnimation < this->lsAnimation.size())
        {
            mbm::ANIMATION *anim = this->lsAnimation[this->indexCurrentAnimation];
            return anim->isEndedThisAnimation;
        }
        return false;
    }

    void ANIMATION_MANAGER::releaseAnimation()
    {
        for (auto & i : this->lsAnimation)
        {
            ANIMATION *anim = i;
            if (anim)
                delete anim;
            i = nullptr;
        }
        this->lsAnimation.clear();
    }

    void ANIMATION_MANAGER::backupAnimations() noexcept
    {
        animationBackup.backup(this);
    }

    void ANIMATION_MANAGER::restoreBackupAnimations() noexcept
    {
        animationBackup.restore(this);
    }

    bool ANIMATION_MANAGER::setTexture(
        const MESH_MBM *mesh, // fixa textura para o estagio 0 e 1, mesh == nullptr e stage = 1 para textura de estagio 2
        const char *fileNametexture, const uint32_t stage, const bool hasAlpha)
    {
        mbm::ANIMATION *anim = this->getAnimation();
        if (anim)
        {
            TEXTURE *newTex = TEXTURE_MANAGER::getInstance()->load(fileNametexture, hasAlpha);
            if (newTex)
            {
                if (mesh)
                {
                    if (stage == 0)
                    {
                        for (int i = anim->indexInitialFrame; i <= anim->indexFinalFrame; ++i)
                        {
                            mbm::BUFFER_MESH *buff = mesh->getBuffer(static_cast<uint32_t>(i));
                            if (buff)
                            {
                                for (uint32_t j = 0; j < buff->totalSubset; ++j)
                                {
                                    util::SUBSET *subset           = &buff->subset[j];
                                    subset->texture                = newTex;
                                    buff->pBufferGL->setTextureByStage(newTex, stage, j);
                                }
                            }
                        }
                        return true;
                    }
                    else
                    {
                        anim->fx.textureOverrideStage2 = newTex;
                        return true;
                    }
                }
                else if (stage)
                {
                    anim->fx.textureOverrideStage2 = newTex;
                    return true;
                }
            }
        }
        return false;
    }

    ANIMATION_BACKUP::VAR_SHADER_BACKUP::VAR_SHADER_BACKUP(const VAR_SHADER* var) noexcept:
		name(var ? var->name : ""),
		typeVar(var ? var->typeVar : TYPE_VAR_SHADER::VAR_FLOAT),
		isPS(var ? var->isPS : false),
		sizeVar(var ? var->sizeVar : 0)
    {
        if (var)
        {
            memcpy(this->current, var->current, sizeof(this->current));
            memcpy(this->min, var->min, sizeof(this->min));
            memcpy(this->max, var->max, sizeof(this->max));
            memcpy(this->step, var->step, sizeof(this->step));
            memcpy(this->control, var->control, sizeof(this->control));
            memcpy(this->granThen, var->granThen, sizeof(this->granThen));
        }
    }

    ANIMATION_BACKUP::FX_BACKUP::FX_BACKUP(const ANIMATION& anim) noexcept:
        statusFxPs(anim.fx.fxPS ? anim.fx.fxPS->statusFx : STATUS_FX::FX_GROWING),
		statusFxVs(anim.fx.fxVS ? anim.fx.fxVS->statusFx : STATUS_FX::FX_GROWING),
		typeAnimPs(anim.fx.fxPS ? anim.fx.fxPS->typeAnim : TYPE_ANIMATION::TYPE_ANIMATION_PAUSED),
		typeAnimVs(anim.fx.fxVS ? anim.fx.fxVS->typeAnim : TYPE_ANIMATION::TYPE_ANIMATION_PAUSED),
		timeAnimationPs(anim.fx.fxPS ? anim.fx.fxPS->timeAnimation : 0.0f),
		timeAnimationVs(anim.fx.fxVS ? anim.fx.fxVS->timeAnimation : 0.0f)
    {		
        if (anim.fx.fxPS->ptrCurrentShader)
        {
            for (uint32_t i = 0; i < anim.fx.fxPS->ptrCurrentShader->getTotalVar(); ++i)
            {
                VAR_SHADER *var = anim.fx.fxPS->ptrCurrentShader->getVar(i);
                if (var)
                {
                    VAR_SHADER_BACKUP* varCopy = new VAR_SHADER_BACKUP(var);
                    this->varsPS.push_back(varCopy);
                }
            }
        }
        if (anim.fx.fxVS->ptrCurrentShader)
        {
            for (uint32_t i = 0; i < anim.fx.fxVS->ptrCurrentShader->getTotalVar(); ++i)
            {
                VAR_SHADER *var = anim.fx.fxVS->ptrCurrentShader->getVar(i);
                if (var)
                {
                    VAR_SHADER_BACKUP* varCopy = new VAR_SHADER_BACKUP(var);
                    this->varsVS.push_back(varCopy);
                }
            }
        }
    }

    ANIMATION_BACKUP::FX_BACKUP::~FX_BACKUP() noexcept
    {
        for (std::vector<mbm::VAR_SHADER*>::size_type i = 0; i < varsPS.size(); ++i)
        {
            if (varsPS[i])
            {
                delete varsPS[i];
                varsPS[i] = nullptr;
            }
        }
        varsPS.clear();
        for (std::vector<mbm::VAR_SHADER*>::size_type i = 0; i < varsVS.size(); ++i)
        {
            if (varsVS[i])
            {
                delete varsVS[i];
                varsVS[i] = nullptr;
            }
        }
        varsVS.clear();
    }

    void ANIMATION_BACKUP::FX_BACKUP::restoreFX(mbm::ANIMATION& anim) const noexcept
    {
		if (anim.fx.fxPS && anim.fx.fxPS->ptrCurrentShader)
        {
            anim.fx.fxPS->statusFx = this->statusFxPs;
            anim.fx.fxPS->typeAnim = this->typeAnimPs;
            anim.fx.fxPS->timeAnimation = this->timeAnimationPs;
            for (std::vector<VAR_SHADER_BACKUP*>::size_type i = 0; i < this->varsPS.size(); ++i)
            {
                const VAR_SHADER_BACKUP* varBackup = this->varsPS[i];
                if (varBackup)
                {
                    VAR_SHADER *var = anim.fx.fxPS->ptrCurrentShader->getVarByName(varBackup->name.c_str());
                    if (var)
                    {
                        if (var->typeVar != varBackup->typeVar)
                        {
                            ERROR_LOG("Unexpected variable type for shader [%s] variable [%s]!\nDid the shader change???", anim.fx.fxPS->ptrCurrentShader->fileName.c_str(), varBackup->name.c_str());
                        }
                        if (var->isPS != varBackup->isPS)
                        {
                            ERROR_LOG("Unexpected variable shader type for shader [%s] variable [%s]!\nDid the shader change???", anim.fx.fxPS->ptrCurrentShader->fileName.c_str(), varBackup->name.c_str());
                        }
                        if (var->sizeVar != varBackup->sizeVar)
                        {
                            ERROR_LOG("Unexpected variable size for shader [%s] variable [%s]!\nDid the shader change???", anim.fx.fxPS->ptrCurrentShader->fileName.c_str(), varBackup->name.c_str());
                        }
                        memcpy(var->current, varBackup->current, sizeof(var->current));
                        memcpy(var->min, varBackup->min, sizeof(var->min));
                        memcpy(var->max, varBackup->max, sizeof(var->max));
                        memcpy(var->step, varBackup->step, sizeof(var->step));
                        memcpy(var->control, varBackup->control, sizeof(var->control));
                        memcpy(var->granThen, varBackup->granThen, sizeof(var->granThen));
                    }
                }
            }
        }
        if (anim.fx.fxVS && anim.fx.fxVS->ptrCurrentShader)
        {
            anim.fx.fxVS->statusFx = this->statusFxVs;
            anim.fx.fxVS->typeAnim = this->typeAnimVs;
            anim.fx.fxVS->timeAnimation = this->timeAnimationVs;
            for (std::vector<VAR_SHADER_BACKUP*>::size_type i = 0; i < this->varsVS.size(); ++i)
            {
                const VAR_SHADER_BACKUP* varBackup = this->varsVS[i];
                if (varBackup)
                {
                    VAR_SHADER *var = anim.fx.fxVS->ptrCurrentShader->getVarByName(varBackup->name.c_str());
                    if (var)
                    {
                        if (var->typeVar != varBackup->typeVar)
                        {
                            ERROR_LOG("Unexpected variable type for shader [%s] variable [%s]!\nDid the shader change???", anim.fx.fxPS->ptrCurrentShader->fileName.c_str(), varBackup->name.c_str());
                        }
                        if (var->isPS != varBackup->isPS)
                        {
                            ERROR_LOG("Unexpected variable shader type for shader [%s] variable [%s]!\nDid the shader change???", anim.fx.fxPS->ptrCurrentShader->fileName.c_str(), varBackup->name.c_str());
                        }
                        if (var->sizeVar != varBackup->sizeVar)
                        {
                            ERROR_LOG("Unexpected variable size for shader [%s] variable [%s]!\nDid the shader change???", anim.fx.fxPS->ptrCurrentShader->fileName.c_str(), varBackup->name.c_str());
                        }
                        memcpy(var->current, varBackup->current, sizeof(var->current));
                        memcpy(var->min, varBackup->min, sizeof(var->min));
                        memcpy(var->max, varBackup->max, sizeof(var->max));
                        memcpy(var->step, varBackup->step, sizeof(var->step));
                        memcpy(var->control, varBackup->control, sizeof(var->control));
                        memcpy(var->granThen, varBackup->granThen, sizeof(var->granThen));
                    }
                }
            }
        }
    }

    void ANIMATION_BACKUP::clearBackup() noexcept
    {
        this->lsAnimationState.clear();
        for (std::vector<FX_BACKUP>::size_type i = 0; i < this->lsFxBackup.size(); ++i)
        {
            FX_BACKUP* it = this->lsFxBackup[i];
            if (it)
            {
                delete it;
                this->lsFxBackup[i] = nullptr;
            }
        }
		this->lsFxBackup.clear();
    }
    
    void ANIMATION_BACKUP::backup(ANIMATION_MANAGER* animationManager)
    {
        if (animationManager && animationManager->lsAnimation.size())
        {
            this->clearBackup();
            for (std::vector<ANIMATION*>::size_type i = 0; i < animationManager->lsAnimation.size(); ++i)
            {
                ANIMATION* anim = animationManager->lsAnimation[i];
				if (anim)
                {
                    ANIMATION_STATE state               = {};
                    strncpy(state.nameAnimation, anim->nameAnimation, sizeof(state.nameAnimation));
                    state.intervalChangeFrame           = anim->intervalChangeFrame;
                    state.indexInitialFrame             = anim->indexInitialFrame;
                    state.indexFinalFrame               = anim->indexFinalFrame;
                    state.indexCurrentFrame             = anim->indexCurrentFrame;
                    state.blendState                    = anim->blendState;
                    state.isEndedThisAnimation          = anim->isEndedThisAnimation;
                    state.currentWayGrowingOfAnimation  = anim->currentWayGrowingOfAnimation;
                    state.type                          = anim->type;
                    state.currentTimeToChangeAnimation  = anim->currentTimeToChangeAnimation;
                    //fx
                    state.fx_textureOverrideStage2      = anim->fx.textureOverrideStage2 ? anim->fx.textureOverrideStage2->getFileNameTexture() : "";
                    state.fx_textureOverrideStage2Alpha = anim->fx.textureOverrideStage2 ? anim->fx.textureOverrideStage2->useAlphaChannel : false;
                    state.fx_blendOperation             = anim->fx.blendOperation;
                    this->lsAnimationState.push_back(state);

                    FX_BACKUP* fxBackup = new FX_BACKUP(*anim);
                    this->lsFxBackup.push_back(fxBackup);
                }
            }
            
            this->indexCurrentAnimation = animationManager->indexCurrentAnimation;
            animationManager->releaseAnimation();
        }
    }

    void ANIMATION_BACKUP::restore(ANIMATION_MANAGER* animationManager)
    {
        if (animationManager && this->lsAnimationState.size())
        {
            mbm::TEXTURE_MANAGER* texManager = mbm::TEXTURE_MANAGER::getInstance();
            for (std::vector<ANIMATION_STATE>::size_type i = 0; i < this->lsAnimationState.size(); ++i)
            {
                const ANIMATION_STATE& state = this->lsAnimationState[i];
                mbm::ANIMATION* anim = animationManager->getAnimation(i);
                if (anim == nullptr)
                {
                    anim = animationManager->getAnimation(animationManager->addAnimation());
                }
                if (anim)
                {
                    strncpy(anim->nameAnimation, state.nameAnimation, sizeof(anim->nameAnimation));
                    anim->intervalChangeFrame          = state.intervalChangeFrame;
                    anim->indexInitialFrame            = state.indexInitialFrame;
                    anim->indexFinalFrame              = state.indexFinalFrame;
                    anim->indexCurrentFrame            = state.indexCurrentFrame;
                    anim->blendState                   = state.blendState;
                    anim->isEndedThisAnimation         = state.isEndedThisAnimation;
                    anim->currentWayGrowingOfAnimation = state.currentWayGrowingOfAnimation;
                    anim->type                         = state.type;
                    anim->currentTimeToChangeAnimation = state.currentTimeToChangeAnimation;
                    //fx
                    anim->fx.textureOverrideStage2     = state.fx_textureOverrideStage2.size() > 0 ? texManager->load(state.fx_textureOverrideStage2.c_str(), state.fx_textureOverrideStage2Alpha) : nullptr;
                    anim->fx.blendOperation            = state.fx_blendOperation;
                    if (i < this->lsFxBackup.size())
                    {
                        FX_BACKUP* it = this->lsFxBackup[i];
                        if (it)
                        {
                            it->restoreFX(*anim);
                        }
                    }
                }
                
            }
            if (this->indexCurrentAnimation < animationManager->lsAnimation.size())
            {
                animationManager->indexCurrentAnimation = this->indexCurrentAnimation;
            }
            else
            {
                animationManager->indexCurrentAnimation = 0;
            }
        }
        this->clearBackup();
    }

    ANIMATION_BACKUP::~ANIMATION_BACKUP() noexcept
    {
		this->clearBackup();
    }
}   
