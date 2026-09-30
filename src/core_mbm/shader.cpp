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
#include <device.h>
#include <renderizable.h>
#include <render-features.h>
#include "private/normal-map-upload.h"
#include "private/normal-map-preparation.h"
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
        if (strcmp(value, "semantic") == 0 || strcmp(value, "role") == 0)
            return SHADER_TEXTURE_NAMING_SEMANTIC_ROLE;
        return SHADER_TEXTURE_NAMING_MIXED_INVALID;
    }

    const char *getShaderTextureNamingName(const SHADER_TEXTURE_NAMING naming) noexcept
    {
        switch (naming)
        {
            case SHADER_TEXTURE_NAMING_NONE: return "none";
            case SHADER_TEXTURE_NAMING_SEMANTIC_ROLE: return "semantic";
            case SHADER_TEXTURE_NAMING_MIXED_INVALID: return "mixed-invalid";
        }
        return "mixed-invalid";
    }

    SHADER_TEXTURE_NAMING detectShaderTextureNamingProfile(const char *shaderCode) noexcept
    {
        if (shaderCode == nullptr || shaderCode[0] == 0)
            return SHADER_TEXTURE_NAMING_NONE;

        for (const TEXTURE_ROLE role : {TEXTURE_ROLE_DIFFUSE,
                                        TEXTURE_ROLE_ANIMATION_EFFECT,
                                        TEXTURE_ROLE_NORMAL,
                                        TEXTURE_ROLE_SPECULAR,
                                        TEXTURE_ROLE_EMISSIVE,
                                        TEXTURE_ROLE_MASK})
        {
            const char *semanticName = getTextureRoleShaderName(role, SHADER_TEXTURE_NAMING_SEMANTIC_ROLE);
            if (containsShaderIdentifier(shaderCode, semanticName))
                return SHADER_TEXTURE_NAMING_SEMANTIC_ROLE;
        }
        return SHADER_TEXTURE_NAMING_NONE;
    }

    bool shaderCodeDeclaresTextureRole(const char *shaderCode,
                                       const TEXTURE_ROLE role,
                                       const SHADER_TEXTURE_NAMING naming) noexcept
    {
        return containsShaderIdentifier(shaderCode, getTextureRoleShaderName(role, naming));
    }

#if USE_NORMAL_MAPPING_3D
    struct NORMAL_MAP_PENDING
    {
        struct SETTINGS { bool basis = false; bool active = false; int greenSign = 1; float strength = 1; };
        std::vector<VEC3> positions, normals;
        std::vector<VEC2> uv;
        std::shared_ptr<const normal_map::PREPARED> prepared;
        std::vector<SETTINGS> subsets;
        bool uploaded = false;
        uint32_t activeSubsets = 0;
    };
#endif
    struct BUFFER_GL::BackendData
    {
        BUFFER_SPECIFIC *buffer;
#if USE_NORMAL_MAPPING_3D
        std::unique_ptr<NORMAL_MAP_PENDING> normalMap;
#endif

        BackendData() noexcept :
            buffer(nullptr)
        {
        }
    };

    void BUFFER_GL::BackendDataDeleter::operator()(BackendData *data) const noexcept
    {
        delete data;
    }

#if USE_NORMAL_MAPPING_3D
    struct normal_map::BUFFER_ACCESS
    {
        static std::unique_ptr<NORMAL_MAP_PENDING> &source(BUFFER_GL *buffer)
        { return buffer->backendData->normalMap; }
        static NORMAL_MAP_PENDING *source(const BUFFER_GL *buffer)
        { return buffer && buffer->backendData ? buffer->backendData->normalMap.get() : nullptr; }
    };

    void normal_map::discardSource(BUFFER_GL *buffer)
    {
        if (BUFFER_ACCESS::source(static_cast<const BUFFER_GL *>(buffer)))
            BUFFER_ACCESS::source(buffer).reset();
    }

    bool normal_map::stageStatic(BUFFER_GL *buffer, const VEC3 *positions, const VEC3 *normals,
                                  const VEC2 *uv, std::shared_ptr<const PREPARED> prepared)
    {
        if (!prepared) return false;
        if (prepared->batches.empty()) return true;
        if (!buffer || !buffer->getBackendBuffer() || !positions || !normals || !uv) return false;
        auto pending = std::make_unique<NORMAL_MAP_PENDING>();
        pending->positions.assign(positions, positions + buffer->sizeOfArrayVertex);
        pending->normals.assign(normals, normals + buffer->sizeOfArrayVertex);
        pending->uv.assign(uv, uv + buffer->sizeOfArrayVertex);
        pending->prepared = std::move(prepared);
        pending->subsets.resize(buffer->totalSubset);
        for (const auto &batch : pending->prepared->batches)
        {
            if (batch.subset >= buffer->totalSubset) return false;
            pending->subsets[batch.subset].basis = !batch.indices.empty();
        }
        BUFFER_ACCESS::source(buffer) = std::move(pending);
        textureChanged(buffer, 0);
        return true;
    }

    void normal_map::setRenderSettings(BUFFER_GL *buffer, uint32_t subset, int sign, float strength)
    {
        auto *data = BUFFER_ACCESS::source(static_cast<const BUFFER_GL *>(buffer));
        if (!data || subset >= data->subsets.size()) return;
        data->subsets[subset].greenSign = sign;
        data->subsets[subset].strength = strength;
        if (data->uploaded) setBackendSettings(buffer, subset, sign, strength);
        const bool active = data->subsets[subset].basis && strength != 0 && buffer->getTextureByStage(2, subset);
        if (data->subsets[subset].active != active)
        {
            if (active) ++data->activeSubsets; else --data->activeSubsets;
            data->subsets[subset].active = active;
        }
    }

    bool normal_map::isActive(const BUFFER_GL *buffer, uint32_t subset)
    {
        const auto *data = BUFFER_ACCESS::source(buffer);
        return data && subset < data->subsets.size() && data->subsets[subset].active;
    }

    uint32_t normal_map::activeSubsetCount(const BUFFER_GL *buffer)
    {
        const auto *data = BUFFER_ACCESS::source(buffer);
        return data ? data->activeSubsets : 0;
    }

    void normal_map::textureChanged(BUFFER_GL *buffer, uint32_t subset)
    {
        auto *data = BUFFER_ACCESS::source(static_cast<const BUFFER_GL *>(buffer));
        if (!data || subset >= data->subsets.size()) return;
        // Slot zero can also be the legacy fallback for subsets without an explicit slot.
        const uint32_t first = subset == 0 ? 0 : subset;
        const uint32_t last = subset == 0 ? static_cast<uint32_t>(data->subsets.size()) : subset + 1;
        for (uint32_t i = first; i < last; ++i)
        {
            auto &settings = data->subsets[i];
            const bool active = settings.basis && settings.strength != 0 && buffer->getTextureByStage(2, i);
            if (settings.active == active) continue;
            if (active) ++data->activeSubsets; else --data->activeSubsets;
            settings.active = active;
        }
    }

    bool normal_map::ensureUploaded(const BUFFER_GL *buffer)
    {
        auto *data = BUFFER_ACCESS::source(buffer);
        if (!data) return false;
        if (data->uploaded) return true;
        auto *mutableBuffer = const_cast<BUFFER_GL *>(buffer);
        if (!uploadBackend(mutableBuffer, data->positions.data(), data->normals.data(),
                           data->uv.data(), *data->prepared)) return false;
        for (uint32_t subset = 0; subset < data->subsets.size(); ++subset)
            setBackendSettings(mutableBuffer, subset, data->subsets[subset].greenSign, data->subsets[subset].strength);
        data->uploaded = true;
        std::vector<VEC3>().swap(data->positions);
        std::vector<VEC3>().swap(data->normals);
        std::vector<VEC2>().swap(data->uv);
        data->prepared = {};
        return true;
    }
#endif

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
#if USE_NORMAL_MAPPING_3D
        if (index_stage == 2) normal_map::textureChanged(this, index_subset);
#endif
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
#if USE_NORMAL_MAPPING_3D
        bool normalMappingVariant = false;
        bool canSelectNormalMapping = false;
        FVF_PROVIDE_BY_ENGINE fvf = FVF_PROVIDE_BY_ENGINE::FVF_NONE;
        std::unique_ptr<SHADER> mapped;
#endif

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

    bool SHADER::usesNormalMappingVariant() const noexcept
    {
#if USE_NORMAL_MAPPING_3D
        return backendData && backendData->normalMappingVariant;
#else
        return false;
#endif
    }

    void SHADER::resetNormalMappingVariant() noexcept
    {
#if USE_NORMAL_MAPPING_3D
        if (backendData)
        {
            backendData->mapped.reset();
            backendData->canSelectNormalMapping = false;
        }
#endif
    }

    bool SHADER::compileShader(BASE_SHADER *pixel, BASE_SHADER *vertex, FVF_PROVIDE_BY_ENGINE fvf,
                               uint32_t paletteSize, SKELETAL_SHADER_METHOD method)
    {
        resetNormalMappingVariant();
        const bool compiled = compileBackend(pixel, vertex, fvf, paletteSize, method);
#if USE_NORMAL_MAPPING_3D
        if (compiled && backendData)
        {
            backendData->fvf = fvf;
            backendData->canSelectNormalMapping = !vertex && paletteSize == 0 &&
                fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV &&
                (shouldCompileReservedLightDefault() || (pixel && pixel->fileName == "lit textured.ps"));
        }
#endif
        return compiled;
    }

    bool SHADER::render(const BUFFER_GL *buffer, const RENDERIZABLE *owner, int32_t subset,
                        const float *palette, uint32_t paletteSize) const
    {
#if USE_NORMAL_MAPPING_3D
        if (backendData && backendData->canSelectNormalMapping && (!owner || owner->is3DObject()) &&
            normal_map::activeSubsetCount(buffer) != 0)
        {
            LIGHT_TARGET target = LIGHT_TARGET_3D;
            DEVICE::getInstance()->getLightTargetForCurrentRender(target);
            if (target != LIGHT_TARGET_3D)
                return renderBackend(buffer, owner, subset, palette, paletteSize);
            const uint32_t first = subset < 0 ? 0u : static_cast<uint32_t>(subset);
            const uint32_t last = subset < 0 ? buffer->totalSubset : first + 1u;
            if (last > buffer->totalSubset) return false;
            const uint32_t mappedCount = subset < 0 ? normal_map::activeSubsetCount(buffer) :
                static_cast<uint32_t>(normal_map::isActive(buffer, first));
            if (mappedCount != 0)
            {
                if (!backendData->mapped)
                {
                    auto mapped = std::make_unique<SHADER>();
                    mapped->setUseReservedLightDefault(backendData->useReservedLightDefault);
                    mapped->backendData->normalMappingVariant = true;
                    if (!mapped->compileBackend(pShader, vShader, backendData->fvf, 0, SKELETAL_SHADER_METHOD::NONE))
                        return false;
                    backendData->mapped = std::move(mapped);
                }
                const SHADER *mapped = backendData->mapped.get();
                if (!mapped->hasNormalMappingInterface())
                    return renderBackend(buffer, owner, subset, palette, paletteSize);
                if (!normal_map::ensureUploaded(buffer)) return false;
                if (mappedCount == last - first)
                    return mapped->renderBackend(buffer, owner, subset, palette, paletteSize);
                for (uint32_t i = first; i < last; ++i)
                {
                    const SHADER *selected = normal_map::isActive(buffer, i) ? mapped : this;
                    if (!selected->renderBackend(buffer, owner, static_cast<int32_t>(i), palette, paletteSize)) return false;
                }
                return true;
            }
        }
#endif
        return renderBackend(buffer, owner, subset, palette, paletteSize);
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
    mbm::MATRIX mbm::SHADER::mvMatrixLightSpace; // modelView x camera view (see shader.h)
    mbm::MATRIX mbm::SHADER::mvpMatrix; // ModelView x projection (perspectiva) (automaticamente setada)

    void SHADER::updateMvpAndLightMatrices(const MATRIX &viewMatrix, const MATRIX &perspectiveMatrix) noexcept
    {
        MatrixMultiply(&SHADER::mvMatrixLightSpace, &SHADER::modelView, &viewMatrix);
        MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &perspectiveMatrix);
    }

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
