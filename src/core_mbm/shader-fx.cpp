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
#include <gles-debug.h>
#include <util-interface.h>

#if defined _WIN32
	#include <../third-party/gles/GLES3/gl3.h>
#endif

    namespace mbm
    {

    FX::FX() noexcept
	{
		fxPS = new EFFECT_SHADER();
		fxVS = new EFFECT_SHADER();
		textureOverrideStage2 = nullptr;
		blendOperation = 1;
	}
    
	FX::~FX()
	{
		delete fxPS;
		delete fxVS;
	}
    
	void FX::setBlendDefaultOp()
    {
        GLBlendEquation(GL_FUNC_ADD);
    }

    void FX::setBlendOp()
    {
        switch (blendOperation)
        {
            case 1: // D3DBLENDOP_ADD              = 1,
            {
                GLBlendEquation(GL_FUNC_ADD);
            }
            break;
            case 2: // D3DBLENDOP_SUBTRACT         = 2,
            {
                GLBlendEquation(GL_FUNC_SUBTRACT);
            }
            break;
            case 3: // D3DBLENDOP_REVSUBTRACT      = 3,
            {
                GLBlendEquation(GL_FUNC_REVERSE_SUBTRACT);
            }
            break;
            case 4: // D3DBLENDOP_MIN              = 4,
            {
    #if defined(ANDROID) || defined(__linux__)
                GLBlendEquation(0x8007);
    #else
                GLBlendEquation(GL_MIN);
    #endif
            }
            break;
            case 5: // D3DBLENDOP_MAX              = 5,
            {
    #if defined(ANDROID) || defined(__linux__)
                GLBlendEquation(0x8008);
    #else
                GLBlendEquation(GL_MAX);
    #endif
            }
            break;
        }
    }

   bool FX::loadNewShader(SHADER_CFG *pShaderCfg,
                                    SHADER_CFG *vShaderCfg, const TYPE_ANIMATION typePs, const float timeAnimPs,
                                    const TYPE_ANIMATION typeVs, const float timeAnimVs)
    {
        BASE_SHADER *basePixelShader  = nullptr;
        BASE_SHADER *baseVertexShader = nullptr;
        if (pShaderCfg)
            basePixelShader  = fxPS->loadEffect(pShaderCfg->fileName.c_str(), pShaderCfg->codeShader.c_str(), typePs);
        if (vShaderCfg)
            baseVertexShader = fxVS->loadEffect(vShaderCfg->fileName.c_str(), vShaderCfg->codeShader.c_str(), typeVs);
        if (fxPS->ptrCurrentShader)
            fxPS->ptrCurrentShader->releaseVars();
        if (fxVS->ptrCurrentShader)
            fxVS->ptrCurrentShader->releaseVars();
        shader.releaseShader();
        if(pShaderCfg == nullptr)//want to release it
        {
            fxPS->ptrCurrentShader = nullptr;
        }
        if(vShaderCfg == nullptr)//want to release it
        {
            fxVS->ptrCurrentShader = nullptr;
        }
        const bool ret = shader.compileShader(basePixelShader, baseVertexShader);
        if (!ret)
            return false;
        if (pShaderCfg)
        {
            for (uint32_t i = 0; i < pShaderCfg->lsVar.size(); ++i)
            {
                VAR_CFG *var = pShaderCfg->lsVar[i];
                if (!fxPS->ptrCurrentShader->addVar(var->name.c_str(), var->type, var->Default, //-V522
                                                               shader.programObject))
                {
#if defined _DEBUG
                    PRINT_IF_DEBUG( "failed to included variable %s shader %s!", var->name.c_str(),
                                 pShaderCfg->fileName.c_str());
#endif
                }
            }
            for (uint32_t i = 0; i < fxPS->ptrCurrentShader->getTotalVar(); ++i)
            {
                VAR_CFG *         var       = pShaderCfg->lsVar[i];
                VAR_SHADER *varShader = fxPS->ptrCurrentShader->getVar(i);
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
                if (!fxVS->ptrCurrentShader->addVar(var->name.c_str(), var->type, var->Default, //-V522
                                                               shader.programObject))
                {
#if defined _DEBUG
                    PRINT_IF_DEBUG( "failed to included variable %s shader %s!", var->name.c_str(),
                                 vShaderCfg->fileName.c_str());
#endif
                }
            }
            for (uint32_t i = 0; i < fxVS->ptrCurrentShader->getTotalVar(); ++i)
            {
                VAR_CFG *         var       = vShaderCfg->lsVar[i];
                VAR_SHADER *varShader = fxVS->ptrCurrentShader->getVar(i);
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
        if (fxPS->ptrCurrentShader)
        {
            VAR_SHADER *var = fxPS->ptrCurrentShader->getVarByName(varName);
            if (var)
            {
                memcpy(var->current, data, sizeof(var->current));
                var->set(var->min, var->max, fxPS->timeAnimation);
                return true;
            }
        }
        return false;
    }
    
   bool FX::setMaxVarPShader(const char *varName,const float data[4])
    {
        if (fxPS->ptrCurrentShader)
        {
            VAR_SHADER *var = fxPS->ptrCurrentShader->getVarByName(varName);
            if (var)
            {
                memcpy(var->max, data, sizeof(var->max));
                var->set(var->min, var->max, fxPS->timeAnimation);
                return true;
            }
        }
        return false;
    }
    
   bool FX::setMinVarPShader(const char *varName,const float data[4])
    {
        if (fxPS->ptrCurrentShader)
        {
            VAR_SHADER *var = fxPS->ptrCurrentShader->getVarByName(varName);
            if (var)
            {
                memcpy(var->min, data, sizeof(var->min));
                var->set(var->min, var->max, fxPS->timeAnimation);
                return true;
            }
        }
        return false;
    }

    int FX::getMaxVarPShader(const char *varName, float outData[4])const 
    {
        if (fxPS->ptrCurrentShader)
        {
            const VAR_SHADER *var = fxPS->ptrCurrentShader->getVarByName(varName);
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
        if (fxPS->ptrCurrentShader)
        {
            const VAR_SHADER *var = fxPS->ptrCurrentShader->getVarByName(varName);
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
        if (fxVS->ptrCurrentShader)
        {
            const VAR_SHADER *var = fxVS->ptrCurrentShader->getVarByName(varName);
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
        if (fxVS->ptrCurrentShader)
        {
            const VAR_SHADER *var = fxVS->ptrCurrentShader->getVarByName(varName);
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
        if (fxVS->ptrCurrentShader)
        {
            VAR_SHADER *var = fxVS->ptrCurrentShader->getVarByName(varName);
            if (var)
            {
                memcpy(var->current, data, sizeof(var->current));
                var->set(var->min, var->max, fxVS->timeAnimation);
                return true;
            }
        }
        return false;
    }
    
   bool FX::setMaxVarVShader(const char *varName,const float data[4])
    {
        if (fxVS->ptrCurrentShader)
        {
            VAR_SHADER *var = fxVS->ptrCurrentShader->getVarByName(varName);
            if (var)
            {
                memcpy(var->max, data, sizeof(var->max));
                var->set(var->min, var->max, fxVS->timeAnimation);
                return true;
            }
        }
        return false;
    }
    
   bool FX::setMinVarVShader(const char *varName,const float data[4])
    {
        if (fxVS->ptrCurrentShader)
        {
            VAR_SHADER *var = fxVS->ptrCurrentShader->getVarByName(varName);
            if (var)
            {
                memcpy(var->min, data, sizeof(var->min));
                var->set(var->min, var->max, fxVS->timeAnimation);
                return true;
            }
        }
        return false;
    }
    
   int FX::getVarPShader(const char *varName,float dataOut[4])const  // (0 - fail )
    {
        if (fxPS->ptrCurrentShader)
        {
            VAR_SHADER *var = fxPS->ptrCurrentShader->getVarByName(varName);
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
        if (fxVS->ptrCurrentShader)
        {
            VAR_SHADER *var = fxVS->ptrCurrentShader->getVarByName(varName);
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
        if (fxPS->ptrCurrentShader)
            return fxPS->ptrCurrentShader->getVars();
        return nullptr;
    }
    
   const char * FX::getCodePS() const
    {
        if (fxPS->ptrCurrentShader)
            return fxPS->ptrCurrentShader->getCode();
        return nullptr;
    }
    
   std::vector<VAR_SHADER *> * FX::getVarsVS() const
    {
        if (fxVS->ptrCurrentShader)
            return fxVS->ptrCurrentShader->getVars();
        return nullptr;
    }
    
   const char * FX::getCodeVS() const
    {
        if (fxVS->ptrCurrentShader)
            return fxVS->ptrCurrentShader->getCode();
        return nullptr;
    }
    
   bool FX::setTypePS(const mbm::TYPE_ANIMATION newType)
    {
        if (fxPS->ptrCurrentShader)
        {
            fxPS->typeAnim = newType;
            return true;
        }
        return false;
    }
    
   bool FX::setTypeVS(const mbm::TYPE_ANIMATION newType)
    {
        if (fxVS->ptrCurrentShader)
        {
            fxVS->typeAnim = newType;
            return true;
        }
        return false;
    }
    
   mbm::TYPE_ANIMATION FX::getTypePS()const
    {
        if (fxPS->ptrCurrentShader)
            return fxPS->typeAnim;
        return TYPE_ANIMATION_PAUSED;
    }
    
   mbm::TYPE_ANIMATION FX::getTypeVS()const
    {
        if (fxVS->ptrCurrentShader)
            return fxVS->typeAnim;
        return TYPE_ANIMATION_PAUSED;
    }
    
   bool FX::setTimePS(float time)
    {
        if (fxPS->ptrCurrentShader)
        {
            fxPS->timeAnimation = time;
            const uint32_t s = fxPS->ptrCurrentShader->getTotalVar();
            for (uint32_t i = 0; i < s; ++i)
            {
                VAR_SHADER *var = fxPS->ptrCurrentShader->getVar(i);
                if (var)
                {
                    var->set(var->min, var->max, fxPS->timeAnimation);
                }
            }
            return true;
        }
        return false;
    }
    
   bool FX::setTimeVS(float time)
    {
        if (fxVS->ptrCurrentShader)
        {
            fxVS->timeAnimation = time;
            const uint32_t s = fxVS->ptrCurrentShader->getTotalVar();
            for (uint32_t i = 0; i < s; ++i)
            {
                VAR_SHADER *var = fxVS->ptrCurrentShader->getVar(i);
                if (var)
                {
                    var->set(var->min, var->max, fxVS->timeAnimation);
                }
            }
            return true;
        }
        return false;
    }
    float FX::getTimePS()
    {
        if (fxPS->ptrCurrentShader)
        {
            return fxPS->timeAnimation;
        }
        return 0.0f;
    }
    float FX::getTimeVS()
    {
        if (fxVS->ptrCurrentShader)
        {
            return fxVS->timeAnimation;
        }
        return 0.0f;
    }
}
