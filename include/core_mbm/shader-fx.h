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

#ifndef SHADER_FX_GLES_H
#define SHADER_FX_GLES_H

#include "shader.h"
#include "core-exports.h"
#include <vector>

namespace mbm
{
    class SHADER_CFG;
	class TEXTURE;
	class EFFECT_SHADER;
    struct VAR_SHADER;
    
    class FX
    {
      public:
        API_IMPL FX() noexcept;
        API_IMPL virtual ~FX();
        
        API_IMPL bool loadNewShader(SHADER_CFG *pShaderCfg,SHADER_CFG *vShaderCfg, const TYPE_ANIMATION typePs, const float timeAnimPs,const TYPE_ANIMATION typeVs, const float timeAnimVs);
        API_IMPL bool setVarPShader(const char *varName,const float data[4]);
        API_IMPL bool setMaxVarPShader(const char *varName,const  float data[4]);
        API_IMPL bool setMinVarPShader(const char *varName,const  float data[4]);
        API_IMPL int  getMaxVarPShader(const char *varName, float outData[4])const;
        API_IMPL int  getMinVarPShader(const char *varName, float outData[4])const;
        API_IMPL int  getMaxVarVShader(const char *varName, float outData[4])const;
        API_IMPL int  getMinVarVShader(const char *varName, float outData[4])const;
        API_IMPL bool setVarVShader(const char *varName,const float data[4]);
        API_IMPL bool setMaxVarVShader(const char *varName,const float data[4]);
        API_IMPL bool setMinVarVShader(const char *varName,const float data[4]);
        API_IMPL int getVarPShader(const char *varName,float dataOut[4])const;
        API_IMPL int getVarVShader(const char *varName,float dataOut[4])const;
        API_IMPL std::vector<VAR_SHADER *> *getVarsPS() const;
        API_IMPL std::vector<VAR_SHADER *> *getVarsVS() const;
        API_IMPL const char *getCodePS() const;
        API_IMPL const char *getCodeVS() const;
        API_IMPL bool setTypePS(const TYPE_ANIMATION newType);
        API_IMPL bool setTypeVS(const TYPE_ANIMATION newType);
        API_IMPL TYPE_ANIMATION getTypePS()const;
        API_IMPL TYPE_ANIMATION getTypeVS()const;
        API_IMPL bool setTimePS(float time);
        API_IMPL bool setTimeVS(float time);
        API_IMPL float getTimePS();
        API_IMPL float getTimeVS();

        EFFECT_SHADER* fxPS;//pixel shader
        EFFECT_SHADER* fxVS;//vertex shader
        SHADER        shader;
        TEXTURE *     textureOverrideStage2;
        int           blendOperation;
        API_IMPL void setBlendDefaultOp();
        API_IMPL void setBlendOp();
    };
}

#endif
