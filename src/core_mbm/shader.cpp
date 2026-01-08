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

#include <shader.h>
#include <util-interface.h>
#include <shader-var-cfg.h>
#include <cstdlib>
#include <header-mesh.h>
#include <texture-manager.h>

namespace mbm
{

    bool BUFFER_GL::isLoadedBuffer() const
    {
        return this->totalSubset != 0;
    }

    TEXTURE* BUFFER_GL::getTextureByStage(const uint32_t index_stage,const uint32_t index_subset) const
    {
        if(index_stage == 0)
        {
            auto it = this->texture0.find(index_subset);
            if(it != this->texture0.end())
                return it->second;
        }
        else
        {
            return this->texture1;
        }
        return nullptr;
    }

    void BUFFER_GL::setTextureByStage(TEXTURE* texture,const uint32_t index_stage, const uint32_t index_subset)
    {
        if(index_stage == 0)
        {
            texture0[index_subset] = texture;
        }
        else
        {
            texture1 = texture;
        }
    }

    BASE_SHADER::BASE_SHADER() noexcept = default;

    BASE_SHADER::~BASE_SHADER()
    {
        this->releaseVars();
        this->fileName.clear();
        this->stringCodeShader.clear();
    }

    const char * BASE_SHADER::getCode()
    {
        return this->stringCodeShader.c_str();
    }

    VAR_SHADER * BASE_SHADER::getVarByName(const char *nameVar)
    {
        if (nameVar == nullptr)
            return nullptr;
        std::vector<VAR_SHADER *>::size_type s = lsVar.size();
        for (std::vector<VAR_SHADER *>::size_type i = 0; i < s; ++i)
        {
            VAR_SHADER *var = lsVar[i];
            if (strcmp(var->name.c_str(), nameVar) == 0)
                return var;
        }
        return nullptr;
    }

    VAR_SHADER * BASE_SHADER::getVar(const uint32_t indexVar)
    {
        if (indexVar < static_cast<uint32_t>(lsVar.size()))
            return lsVar[static_cast<std::vector<VAR_SHADER *>::size_type>(indexVar)];
        return nullptr;
    }

    uint32_t BASE_SHADER::getTotalVar() const noexcept
    {
        return static_cast<uint32_t>(lsVar.size());
    }

    void BASE_SHADER::releaseVars()
    {
        const std::vector<VAR_SHADER *>::size_type s = lsVar.size();
        for (std::vector<VAR_SHADER *>::size_type i = 0; i < s; ++i)
        {
            VAR_SHADER *var = lsVar[i];
            if (var)
                delete var;
            var      = nullptr;
            lsVar[i] = nullptr;
        }
        lsVar.clear();
    }

    bool BASE_SHADER::loadShader(const char *fileNameShaderVS_PS, const char *code)
    {
        this->stringCodeShader.clear();
        this->fileName.clear();
        if (fileNameShaderVS_PS && code)
        {
            this->fileName         = fileNameShaderVS_PS;
            this->stringCodeShader = code;
            return true;
        }
        return false;
    }

    std::vector<VAR_SHADER*> * BASE_SHADER::getVars()
    {
        return &this->lsVar;
    }

    bool BASE_SHADER::isThereVarIntoLsVars(const char *nameVar)
    {
        if (nameVar == nullptr)
            return false;
        const std::vector<VAR_SHADER *>::size_type s = lsVar.size();
        for (std::vector<VAR_SHADER *>::size_type i = 0; i < s; ++i)
        {
            VAR_SHADER *var = lsVar[i];
            if (var && strcmp(var->name.c_str(), nameVar) == 0)
                return true;
        }
        return false;
    }

    SHADER::SHADER() noexcept : programObject(0),
                                  mvpMatrixHandle(-1),
                                  mvMatrixHandle(-1),
                                  positionHandle(-1),
                                  texCoordHandle(-1),
                                  samplerHandle0(-1),
                                  samplerHandle1(-1),
                                  normalHandle(-1),
                                  pShader(nullptr),
                                  vShader(nullptr)
    {
    }

    void SHADER::onRestore() // Libera o pShader da memória e pode ser carregado novamente
    {
        this->mvpMatrixHandle = -1;
        this->mvMatrixHandle  = -1;
        this->positionHandle  = -1;
        this->texCoordHandle  = -1;
        this->samplerHandle0  = -1;
        this->samplerHandle1  = -1;
        this->normalHandle    = -1;
        this->programObject   = 0;
        this->pShader         = nullptr;
        this->vShader         = nullptr;
    }

    bool SHADER::isLoad()
    {
        return this->programObject != 0;
    }

    void SHADER::update()
    {
        if (this->pShader)
            this->pShader->update(this->programObject);
#if defined _DEBUG
        else if (this->programObject == 0)
            PRINT_IF_DEBUG("missed shader!");
#endif
        if (this->vShader)
            this->vShader->update(this->programObject);
#if defined _DEBUG
        else if (this->programObject == 0)
            PRINT_IF_DEBUG("missed shader!");
#endif
    }

    mbm::MATRIX mbm::SHADER::modelView; // Matrix do modelo (ModelView)
    mbm::MATRIX mbm::SHADER::mvpMatrix; // ModelView x projection (perspectiva) (automaticamente setada)
}
