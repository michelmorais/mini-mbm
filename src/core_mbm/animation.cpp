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
#include <gles-debug.h>
#include <util-interface.h>

#if defined _WIN32
#include <../third-party/gles/GLES3/gl3.h>
#endif

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
			if(infoHead->effetcShader)
			{
				util::INFO_SHADER_DATA *infoPS         = infoHead->effetcShader->dataPS;
				ANIMATION *anim                         = i < this->lsAnimation.size() ? this->lsAnimation[i] : nullptr;
				if (infoPS && infoPS->fileNameTextureStage2)
				{
					TEXTURE *  tex  = texMan->load(infoPS->fileNameTextureStage2, true);
					if (anim && tex)
						anim->fx.textureOverrideStage2 = tex;
				}

				util::INFO_SHADER_DATA *infoVS = infoHead->effetcShader->dataVS;
				if (infoVS && infoVS->fileNameTextureStage2)
				{
					TEXTURE *  tex  = texMan->load(infoVS->fileNameTextureStage2, true);
					if (anim && tex)
						anim->fx.textureOverrideStage2 = tex;
				}
				if(anim)
					anim->blendState = static_cast<mbm::BLEND_OPENGLES>(infoHead->headerAnim->blendState);
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
            util::INFO_FX *infoShaderStep = infoHead->effetcShader;
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
        if (anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader))
        {
            if(infoHead->effetcShader && infoHead->effetcShader->blendOperation != 0)
                anim->fx.blendOperation = infoHead->effetcShader->blendOperation;

            if (anim->fx.fxPS->ptrCurrentShader)
            {
                util::INFO_FX *infoShaderStep = infoHead->effetcShader;
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
                                                                            anim->fx.shader.programObject))
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
                util::INFO_FX *infoShaderStep = infoHead->effetcShader;
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
                                                                            anim->fx.shader.programObject))
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
        if (!anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader))
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
                                    buff->pBufferGL->idTexture0[j] = newTex->idTexture;
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
}
