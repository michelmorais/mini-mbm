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
#include <header-mesh.h>
#include <texture-manager.h>
#include <draw-compatibility.h>

namespace mbm
{
    struct BUFFER_GL::BackendData
    {
        BUFFER_SPECIFIC *buffer;

        BackendData() noexcept :
            buffer(nullptr)
        {
        }
    };

    void BUFFER_GL::BackendDataDeleter::operator()(BackendData *data) const noexcept
    {
        delete data;
    }

    bool BUFFER_GL::isLoadedBuffer() const
    {
        return this->totalSubset != 0;
    }

    void BUFFER_GL::initializeVertexBufferControl(const uint32_t totalSubsets,
                                                  const uint32_t _sizeOfArrayVertex,
                                                  const int* vertexStartSubset,
                                                  const int* vertexCountSubset,
                                                  const util::INFO_DRAW_MODE* info_draw_mode)
    {
        if (this->vertexStartVB)
            delete[] this->vertexStartVB;
        if (this->vertexCountVB)
            delete[] this->vertexCountVB;
        if (this->indexStartIB)
            delete[] this->indexStartIB;
        if (this->indexCountIB)
            delete[] this->indexCountIB;

        this->vertexStartVB = nullptr;
        this->vertexCountVB = nullptr;
        this->indexStartIB  = nullptr;
        this->indexCountIB  = nullptr;
        this->sizeOfArrayVertex = _sizeOfArrayVertex;

        if (totalSubsets > 0)
        {
            this->vertexStartVB = new int32_t[totalSubsets];
            this->vertexCountVB = new int32_t[totalSubsets];
            for (uint32_t i = 0; i < totalSubsets; ++i)
            {
                this->vertexStartVB[i] = vertexStartSubset[i];
                this->vertexCountVB[i] = vertexCountSubset[i];
            }
        }
        if (info_draw_mode)
        {
            this->mode_draw = info_draw_mode->mode_draw;
            this->mode_cull_face = info_draw_mode->mode_cull_face;
            this->mode_front_face_direction = info_draw_mode->mode_front_face_direction;
        }
        else
        {
            this->mode_draw = util::MODE_DRAW_TRIANGLES;
            this->mode_cull_face = util::CULL_BACK;
            this->mode_front_face_direction = util::CW;
        }
        this->initializedIndexBuffer = false;
        this->totalSubset = totalSubsets;
    }

    void BUFFER_GL::initializeIndexBufferControl(const uint32_t totalSubsets,
                                                 const uint32_t _sizeOfArrayVertex,
                                                 const int* indexStartSubset,
                                                 const int* indexCountSubset,
                                                 const util::INFO_DRAW_MODE* info_draw_mode)
    {
        if (this->vertexStartVB)
            delete[] this->vertexStartVB;
        if (this->vertexCountVB)
            delete[] this->vertexCountVB;
        if (this->indexStartIB)
            delete[] this->indexStartIB;
        if (this->indexCountIB)
            delete[] this->indexCountIB;

        this->vertexStartVB = nullptr;
        this->vertexCountVB = nullptr;
        this->indexStartIB = nullptr;
        this->indexCountIB = nullptr;
        this->sizeOfArrayVertex = _sizeOfArrayVertex;

        if (totalSubsets > 0)
        {
            this->indexStartIB  = new int32_t[totalSubsets];
            this->indexCountIB = new int32_t[totalSubsets];
            for (uint32_t i = 0; i < totalSubsets; ++i)
            {
                this->indexStartIB[i]  = indexStartSubset[i];
                this->indexCountIB[i]  = indexCountSubset[i];
            }
        }
        if (info_draw_mode)
        {
            this->mode_draw = info_draw_mode->mode_draw;
            this->mode_cull_face = info_draw_mode->mode_cull_face;
            this->mode_front_face_direction = info_draw_mode->mode_front_face_direction;
        }
        else
        {
            this->mode_draw = util::MODE_DRAW_TRIANGLES;
            this->mode_cull_face = util::CULL_BACK;
            this->mode_front_face_direction = util::CW;
        }
        initializedIndexBuffer = true;
        totalSubset = totalSubsets;
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

    BUFFER_SPECIFIC * BUFFER_GL::getBackendBuffer() const noexcept
    {
        return backendData ? backendData->buffer : nullptr;
    }

    void BUFFER_GL::setBackendBuffer(BUFFER_SPECIFIC *backendBuffer) noexcept
    {
        if (!backendData)
        {
            backendData.reset(new BackendData());
        }
        backendData->buffer = backendBuffer;
    }

    BASE_SHADER::BASE_SHADER() noexcept {}

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

    void SHADER::update()
    {
        if (this->pShader)
            this->pShader->update(this->ptrShaderSpecific);
        if (this->vShader)
            this->vShader->update(this->ptrShaderSpecific);
    }

    mbm::MATRIX mbm::SHADER::modelView; // Matrix do modelo (ModelView)
    mbm::MATRIX mbm::SHADER::mvpMatrix; // ModelView x projection (perspectiva) (automaticamente setada)
    
	// Effect on Directx where it is possible not use shader code for PS or VS
    static bool useDeafultPSwhenNoPsShader = true;
    static bool useDeafultVSwhenNoVsShader = true;

    void _setUsageOfDefaultPS_VS_WhenNoShader(const bool _useDeafultPSwhenNoPsShader, const bool _useDeafultVSwhenNoVSShader) noexcept
    {
        useDeafultPSwhenNoPsShader = _useDeafultPSwhenNoPsShader;
        useDeafultVSwhenNoVsShader = _useDeafultVSwhenNoVSShader;
    }

    bool useDefaultPSWhenNoShader() noexcept
    {
        return useDeafultPSwhenNoPsShader;
    }
    bool useDefaultVSWhenNoShader() noexcept
    {
        return useDeafultVSwhenNoVsShader;
    }
}
