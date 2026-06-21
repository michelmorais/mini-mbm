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

#include <shader-fx.h>
#include <shader-cfg.h>
#include <shader-var-cfg.h>
#include <animation.h>
#include <util-interface.h>
#include <light.h>


namespace mbm
{
    DEFAULT_SHADER_MODE getDefaultShaderModeForRenderizable(const RENDERIZABLE *renderizable) noexcept
    {
        if (renderizable == nullptr || renderizable->is2dScreenObject())
            return DEFAULT_SHADER_MODE_UNLIT;

        const LIGHT_TARGET target = renderizable->is3DObject() ? LIGHT_TARGET_3D : LIGHT_TARGET_2DW;
        LIGHT_STATE lightState;
        if (getLightState(target, lightState) && lightState.enabled)
            return DEFAULT_SHADER_MODE_LIT;
        return DEFAULT_SHADER_MODE_UNLIT;
    }

    FX::FX() noexcept
    {
        fxPS = new EFFECT_SHADER();
        fxVS = new EFFECT_SHADER();
        textureOverrideStage2 = nullptr;
        blendOperation = 1;
        defaultShaderMode = DEFAULT_SHADER_MODE_UNLIT;
    }
    
    FX::~FX()
    {
        delete fxPS;
        delete fxVS;
    }

    bool FX::loadNewShader(SHADER_CFG *pShaderCfg,
                                    SHADER_CFG *vShaderCfg, const TYPE_ANIMATION typePs, const float timeAnimPs,
                                    const TYPE_ANIMATION typeVs, const float timeAnimVs, FVF_PROVIDE_BY_ENGINE fvf)
    {
        BASE_SHADER *basePixelShader  = nullptr;
        BASE_SHADER *baseVertexShader = nullptr;
        if (pShaderCfg)
            basePixelShader  = fxPS->loadEffect(pShaderCfg->fileName.c_str(), pShaderCfg->codeShader.c_str(), typePs);
        if (vShaderCfg)
            baseVertexShader = fxVS->loadEffect(vShaderCfg->fileName.c_str(), vShaderCfg->codeShader.c_str(), typeVs);
        if (fxPS->getCurrentShader())
            fxPS->getCurrentShader()->releaseVars();
        if (fxVS->getCurrentShader())
            fxVS->getCurrentShader()->releaseVars();
        shader.releaseShader();
        if(pShaderCfg == nullptr)//want to release it
        {
            fxPS->setCurrentShader(nullptr);
        }
        if(vShaderCfg == nullptr)//want to release it
        {
            fxVS->setCurrentShader(nullptr);
        }
        shader.setUseReservedLightDefault(defaultShaderMode == DEFAULT_SHADER_MODE_LIT);
        const bool ret = shader.compileShader(basePixelShader, baseVertexShader, fvf);
        if (!ret)
            return false;
        void *backendShaderSpecific = shader.getBackendShaderSpecific();
        if (pShaderCfg)
        {
            for (uint32_t i = 0; i < pShaderCfg->lsVar.size(); ++i)
            {
                VAR_CFG *var = pShaderCfg->lsVar[i];
                if (!fxPS->getCurrentShader()->addVar(var->name.c_str(), var->type, var->Default, //-V522
                                                               backendShaderSpecific, true))
                {
#if defined _DEBUG
                    PRINT_IF_DEBUG( "failed to included variable %s shader %s!", var->name.c_str(),
                                 pShaderCfg->fileName.c_str());
#endif
                }
            }
            for (uint32_t i = 0; i < fxPS->getCurrentShader()->getTotalVar(); ++i)
            {
                VAR_CFG *         var       = pShaderCfg->lsVar[i];
                VAR_SHADER *varShader = fxPS->getCurrentShader()->getVar(i);
                if (varShader)
                {
                    varShader->set(var->Min, var->Max, timeAnimPs);
                }
            }
        }

        if (vShaderCfg)
        {
            for (uint32_t i = 0; i < vShaderCfg->lsVar.size(); ++i)
            {
                VAR_CFG *var = vShaderCfg->lsVar[i];
                if (!fxVS->getCurrentShader()->addVar(var->name.c_str(), var->type, var->Default, //-V522
                                                               backendShaderSpecific, false))
                {
#if defined _DEBUG
                    PRINT_IF_DEBUG( "failed to included variable %s shader %s!", var->name.c_str(),
                                 vShaderCfg->fileName.c_str());
#endif
                }
            }
            for (uint32_t i = 0; i < fxVS->getCurrentShader()->getTotalVar(); ++i)
            {
                VAR_CFG *         var       = vShaderCfg->lsVar[i];
                VAR_SHADER *varShader = fxVS->getCurrentShader()->getVar(i);
                if (varShader)
                {
                    varShader->set(var->Min, var->Max, timeAnimVs);
                }
            }
        }
        return ret;
    }
    
   bool FX::setVarPShader(const char *varName,const float data[4])
    {
        if (fxPS->getCurrentShader())
        {
            VAR_SHADER *var = fxPS->getCurrentShader()->getVarByName(varName);
            if (var)
            {
                memcpy(var->current, data, sizeof(var->current));
                var->set(var->min, var->max, fxPS->getTimeAnimation());
                return true;
            }
        }
        return false;
    }
    
   bool FX::setMaxVarPShader(const char *varName,const float data[4])
    {
        if (fxPS->getCurrentShader())
        {
            VAR_SHADER *var = fxPS->getCurrentShader()->getVarByName(varName);
            if (var)
            {
                memcpy(var->max, data, sizeof(var->max));
                var->set(var->min, var->max, fxPS->getTimeAnimation());
                return true;
            }
        }
        return false;
    }
    
   bool FX::setMinVarPShader(const char *varName,const float data[4])
    {
        if (fxPS->getCurrentShader())
        {
            VAR_SHADER *var = fxPS->getCurrentShader()->getVarByName(varName);
            if (var)
            {
                memcpy(var->min, data, sizeof(var->min));
                var->set(var->min, var->max, fxPS->getTimeAnimation());
                return true;
            }
        }
        return false;
    }

    int FX::getMaxVarPShader(const char *varName, float outData[4])const 
    {
        if (fxPS->getCurrentShader())
        {
            const VAR_SHADER *var = fxPS->getCurrentShader()->getVarByName(varName);
            if (var)
            {
                memcpy(outData,var->max, sizeof(var->max));
                return var->sizeVar;
            }
        }
        return 0;
    }
    int FX::getMinVarPShader(const char *varName, float outData[4])const 
    {
        if (fxPS->getCurrentShader())
        {
            const VAR_SHADER *var = fxPS->getCurrentShader()->getVarByName(varName);
            if (var)
            {
                memcpy(outData,var->min, sizeof(var->min));
                return var->sizeVar;
            }
        }
        return 0;
    }
    int FX::getMaxVarVShader(const char *varName, float outData[4])const 
    {
        if (fxVS->getCurrentShader())
        {
            const VAR_SHADER *var = fxVS->getCurrentShader()->getVarByName(varName);
            if (var)
            {
                memcpy(outData,var->max, sizeof(var->max));
                return var->sizeVar;
            }
        }
        return 0;
    }
    int FX::getMinVarVShader(const char *varName, float outData[4])const 
    {
        if (fxVS->getCurrentShader())
        {
            const VAR_SHADER *var = fxVS->getCurrentShader()->getVarByName(varName);
            if (var)
            {
                memcpy(outData,var->min, sizeof(var->min));
                return var->sizeVar;
            }
        }
        return 0;
    }
    
   bool FX::setVarVShader(const char *varName,const float data[4])
    {
        if (fxVS->getCurrentShader())
        {
            VAR_SHADER *var = fxVS->getCurrentShader()->getVarByName(varName);
            if (var)
            {
                memcpy(var->current, data, sizeof(var->current));
                var->set(var->min, var->max, fxVS->getTimeAnimation());
                return true;
            }
        }
        return false;
    }
    
   bool FX::setMaxVarVShader(const char *varName,const float data[4])
    {
        if (fxVS->getCurrentShader())
        {
            VAR_SHADER *var = fxVS->getCurrentShader()->getVarByName(varName);
            if (var)
            {
                memcpy(var->max, data, sizeof(var->max));
                var->set(var->min, var->max, fxVS->getTimeAnimation());
                return true;
            }
        }
        return false;
    }
    
   bool FX::setMinVarVShader(const char *varName,const float data[4])
    {
        if (fxVS->getCurrentShader())
        {
            VAR_SHADER *var = fxVS->getCurrentShader()->getVarByName(varName);
            if (var)
            {
                memcpy(var->min, data, sizeof(var->min));
                var->set(var->min, var->max, fxVS->getTimeAnimation());
                return true;
            }
        }
        return false;
    }
    
   int FX::getVarPShader(const char *varName,float dataOut[4])const  // (0 - fail )
    {
        if (fxPS->getCurrentShader())
        {
            VAR_SHADER *var = fxPS->getCurrentShader()->getVarByName(varName);
            if (var)
            {
                memcpy(dataOut, var->current, sizeof(var->current));
                return var->sizeVar;
            }
        }
        return 0;
    }
    
   int FX::getVarVShader(const char *varName,float dataOut[4])const  // (0 - fail )
    {
        if (fxVS->getCurrentShader())
        {
            VAR_SHADER *var = fxVS->getCurrentShader()->getVarByName(varName);
            if (var)
            {
                memcpy(dataOut, var->current, sizeof(var->current));
                return var->sizeVar;
            }
        }
        return 0;
    }
    
   std::vector<VAR_SHADER *> * FX::getVarsPS() const
    {
        if (fxPS->getCurrentShader())
            return fxPS->getCurrentShader()->getVars();
        return nullptr;
    }
    
   const char * FX::getCodePS() const
    {
        if (fxPS->getCurrentShader())
            return fxPS->getCurrentShader()->getCode();
        return nullptr;
    }
    
   std::vector<VAR_SHADER *> * FX::getVarsVS() const
    {
        if (fxVS->getCurrentShader())
            return fxVS->getCurrentShader()->getVars();
        return nullptr;
    }
    
   const char * FX::getCodeVS() const
    {
        if (fxVS->getCurrentShader())
            return fxVS->getCurrentShader()->getCode();
        return nullptr;
    }
    
   bool FX::setTypePS(const mbm::TYPE_ANIMATION newType)
    {
        if (fxPS->getCurrentShader())
        {
            fxPS->setTypeAnim(newType);
            return true;
        }
        return false;
    }
    
   bool FX::setTypeVS(const mbm::TYPE_ANIMATION newType)
    {
        if (fxVS->getCurrentShader())
        {
            fxVS->setTypeAnim(newType);
            return true;
        }
        return false;
    }
    
   mbm::TYPE_ANIMATION FX::getTypePS()const
    {
        if (fxPS->getCurrentShader())
            return fxPS->getTypeAnim();
        return TYPE_ANIMATION_PAUSED;
    }
    
   mbm::TYPE_ANIMATION FX::getTypeVS()const
    {
        if (fxVS->getCurrentShader())
            return fxVS->getTypeAnim();
        return TYPE_ANIMATION_PAUSED;
    }
    
   bool FX::setTimePS(float time)
    {
        if (fxPS->getCurrentShader())
        {
            fxPS->setTimeAnimation(time);
            const uint32_t s = fxPS->getCurrentShader()->getTotalVar();
            for (uint32_t i = 0; i < s; ++i)
            {
                VAR_SHADER *var = fxPS->getCurrentShader()->getVar(i);
                if (var)
                {
                    var->set(var->min, var->max, fxPS->getTimeAnimation());
                }
            }
            return true;
        }
        return false;
    }
    
   bool FX::setTimeVS(float time)
    {
        if (fxVS->getCurrentShader())
        {
            fxVS->setTimeAnimation(time);
            const uint32_t s = fxVS->getCurrentShader()->getTotalVar();
            for (uint32_t i = 0; i < s; ++i)
            {
                VAR_SHADER *var = fxVS->getCurrentShader()->getVar(i);
                if (var)
                {
                    var->set(var->min, var->max, fxVS->getTimeAnimation());
                }
            }
            return true;
        }
        return false;
    }
    float FX::getTimePS()
    {
        if (fxPS->getCurrentShader())
        {
            return fxPS->getTimeAnimation();
        }
        return 0.0f;
    }
    float FX::getTimeVS()
    {
        if (fxVS->getCurrentShader())
        {
            return fxVS->getTimeAnimation();
        }
        return 0.0f;
    }
}
