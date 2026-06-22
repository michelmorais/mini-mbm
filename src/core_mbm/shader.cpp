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
#include <cctype>
#include <cstring>
#include <initializer_list>

namespace mbm
{
    static bool isShaderIdentifierChar(const char c) noexcept
    {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }

    static bool containsShaderIdentifier(const char *code, const char *name) noexcept
    {
        if (code == nullptr || name == nullptr || name[0] == 0)
            return false;
        const std::size_t nameLen = strlen(name);
        const char *found = strstr(code, name);
        while (found)
        {
            const bool validBefore = found == code || !isShaderIdentifierChar(*(found - 1));
            const char after = found[nameLen];
            const bool validAfter = after == 0 || !isShaderIdentifierChar(after);
            if (validBefore && validAfter)
                return true;
            found = strstr(found + nameLen, name);
        }
        return false;
    }

    const char *getTextureRoleShaderName(const TEXTURE_ROLE role, const SHADER_TEXTURE_NAMING naming) noexcept
    {
        if (naming == SHADER_TEXTURE_NAMING_LEGACY_SAMPLE)
        {
            switch (role)
            {
                case TEXTURE_ROLE_DIFFUSE: return "sample0";
                case TEXTURE_ROLE_ANIMATION_EFFECT: return "sample1";
                case TEXTURE_ROLE_NORMAL: return "sample2";
                default: return nullptr;
            }
        }
        if (naming == SHADER_TEXTURE_NAMING_SEMANTIC_ROLE)
        {
            switch (role)
            {
                case TEXTURE_ROLE_DIFFUSE: return "TextureDiffuse";
                case TEXTURE_ROLE_ANIMATION_EFFECT: return "TextureAnimationEffect";
                case TEXTURE_ROLE_NORMAL: return "TextureNormal";
                case TEXTURE_ROLE_SPECULAR: return "TextureSpecular";
                case TEXTURE_ROLE_EMISSIVE: return "TextureEmissive";
                case TEXTURE_ROLE_MASK: return "TextureMask";
            }
        }
        return nullptr;
    }

    int getTextureRoleBackendSlot(const TEXTURE_ROLE role) noexcept
    {
        switch (role)
        {
            case TEXTURE_ROLE_DIFFUSE: return 0;
            case TEXTURE_ROLE_ANIMATION_EFFECT: return 1;
            case TEXTURE_ROLE_NORMAL: return 2;
            case TEXTURE_ROLE_SPECULAR: return 3;
            case TEXTURE_ROLE_EMISSIVE: return 4;
            case TEXTURE_ROLE_MASK: return 5;
        }
        return -1;
    }

    SHADER_TEXTURE_NAMING parseShaderTextureNaming(const char *value) noexcept
    {
        if (value == nullptr)
            return SHADER_TEXTURE_NAMING_NONE;
        if (strcmp(value, "none") == 0)
            return SHADER_TEXTURE_NAMING_NONE;
        if (strcmp(value, "legacy") == 0 || strcmp(value, "sample") == 0)
            return SHADER_TEXTURE_NAMING_LEGACY_SAMPLE;
        if (strcmp(value, "semantic") == 0 || strcmp(value, "role") == 0)
            return SHADER_TEXTURE_NAMING_SEMANTIC_ROLE;
        return SHADER_TEXTURE_NAMING_MIXED_INVALID;
    }

    const char *getShaderTextureNamingName(const SHADER_TEXTURE_NAMING naming) noexcept
    {
        switch (naming)
        {
            case SHADER_TEXTURE_NAMING_NONE: return "none";
            case SHADER_TEXTURE_NAMING_LEGACY_SAMPLE: return "legacy";
            case SHADER_TEXTURE_NAMING_SEMANTIC_ROLE: return "semantic";
            case SHADER_TEXTURE_NAMING_MIXED_INVALID: return "mixed-invalid";
        }
        return "mixed-invalid";
    }

    SHADER_TEXTURE_NAMING detectShaderTextureNamingProfile(const char *shaderCode) noexcept
    {
        if (shaderCode == nullptr || shaderCode[0] == 0)
            return SHADER_TEXTURE_NAMING_NONE;

        bool hasLegacy = false;
        bool hasSemantic = false;

        for (const TEXTURE_ROLE role : {TEXTURE_ROLE_DIFFUSE, TEXTURE_ROLE_ANIMATION_EFFECT, TEXTURE_ROLE_NORMAL})
        {
            const char *legacyName = getTextureRoleShaderName(role, SHADER_TEXTURE_NAMING_LEGACY_SAMPLE);
            if (containsShaderIdentifier(shaderCode, legacyName))
                hasLegacy = true;
        }
        for (const TEXTURE_ROLE role : {TEXTURE_ROLE_DIFFUSE,
                                        TEXTURE_ROLE_ANIMATION_EFFECT,
                                        TEXTURE_ROLE_NORMAL,
                                        TEXTURE_ROLE_SPECULAR,
                                        TEXTURE_ROLE_EMISSIVE,
                                        TEXTURE_ROLE_MASK})
        {
            const char *semanticName = getTextureRoleShaderName(role, SHADER_TEXTURE_NAMING_SEMANTIC_ROLE);
            if (containsShaderIdentifier(shaderCode, semanticName))
                hasSemantic = true;
        }

        if (hasLegacy && hasSemantic)
            return SHADER_TEXTURE_NAMING_MIXED_INVALID;
        if (hasSemantic)
            return SHADER_TEXTURE_NAMING_SEMANTIC_ROLE;
        if (hasLegacy)
            return SHADER_TEXTURE_NAMING_LEGACY_SAMPLE;
        return SHADER_TEXTURE_NAMING_NONE;
    }

    SHADER_TEXTURE_NAMING mergeShaderTextureNamingProfiles(const SHADER_TEXTURE_NAMING a,
                                                           const SHADER_TEXTURE_NAMING b) noexcept
    {
        if (a == SHADER_TEXTURE_NAMING_MIXED_INVALID || b == SHADER_TEXTURE_NAMING_MIXED_INVALID)
            return SHADER_TEXTURE_NAMING_MIXED_INVALID;
        if (a == SHADER_TEXTURE_NAMING_NONE)
            return b;
        if (b == SHADER_TEXTURE_NAMING_NONE || a == b)
            return a;
        return SHADER_TEXTURE_NAMING_MIXED_INVALID;
    }

    bool shaderCodeDeclaresTextureRole(const char *shaderCode,
                                       const TEXTURE_ROLE role,
                                       const SHADER_TEXTURE_NAMING naming) noexcept
    {
        return containsShaderIdentifier(shaderCode, getTextureRoleShaderName(role, naming));
    }

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
        const auto stageIt = this->texturesByStage.find(index_stage);
        if(stageIt == this->texturesByStage.end())
            return nullptr;
        const auto & subsetTextures = stageIt->second;
        auto subsetIt = subsetTextures.find(index_subset);
        if(subsetIt != subsetTextures.end())
            return subsetIt->second;
        if(index_stage > 0)
        {
            // Keep legacy behaviour for stage 1 FX textures, which were historically
            // stored once and reused for every subset.
            subsetIt = subsetTextures.find(0);
            if(subsetIt != subsetTextures.end())
                return subsetIt->second;
        }
        return nullptr;
    }

    void BUFFER_GL::setTextureByStage(TEXTURE* texture,const uint32_t index_stage, const uint32_t index_subset)
    {
        this->texturesByStage[index_stage][index_subset] = texture;
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

    struct SHADER::BackendData
    {
        void *shaderSpecific;
        bool useReservedLightDefault;

        BackendData() noexcept :
            shaderSpecific(nullptr),
            useReservedLightDefault(false)
        {
        }
    };

    void SHADER::BackendDataDeleter::operator()(BackendData *data) const noexcept
    {
        delete data;
    }

    BASE_SHADER::BASE_SHADER() noexcept :
        textureNamingProfile(SHADER_TEXTURE_NAMING_NONE)
    {
    }

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
        this->textureNamingProfile = SHADER_TEXTURE_NAMING_NONE;
        if (fileNameShaderVS_PS && code)
        {
            this->fileName         = fileNameShaderVS_PS;
            this->stringCodeShader = code;
            this->textureNamingProfile = detectShaderTextureNamingProfile(code);
            if (this->textureNamingProfile == SHADER_TEXTURE_NAMING_MIXED_INVALID)
            {
                ERROR_LOG("Shader [%s] mixes legacy texture names with semantic texture roles", fileNameShaderVS_PS);
                return false;
            }
            return true;
        }
        return false;
    }

    SHADER_TEXTURE_NAMING BASE_SHADER::getTextureNamingProfile() const noexcept
    {
        return this->textureNamingProfile;
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
        void *backendShaderSpecific = this->getBackendShaderSpecific();
        if (this->pShader)
            this->pShader->update(backendShaderSpecific);
        if (this->vShader)
            this->vShader->update(backendShaderSpecific);
    }

    bool SHADER::usesPureDefaultShaderPair() const noexcept
    {
        return this->pShader == nullptr && this->vShader == nullptr;
    }

    bool SHADER::shouldCompileReservedLightDefault() const noexcept
    {
        return this->usesPureDefaultShaderPair() && backendData && backendData->useReservedLightDefault;
    }

    void SHADER::setUseReservedLightDefault(const bool enabled) noexcept
    {
        if (!backendData)
        {
            backendData.reset(new BackendData());
        }
        backendData->useReservedLightDefault = enabled;
    }

    void * SHADER::getBackendShaderSpecific() const noexcept
    {
        return backendData ? backendData->shaderSpecific : nullptr;
    }

    void SHADER::setBackendShaderSpecific(void *backendShaderSpecific) noexcept
    {
        if (!backendData)
        {
            backendData.reset(new BackendData());
        }
        backendData->shaderSpecific = backendShaderSpecific;
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
