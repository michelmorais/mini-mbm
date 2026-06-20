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


#if defined (USE_DUMMY_BACK_END_ENGINE)

#include "dummy-engine.h" // for compiler_message, you can remove it after implement the functions

#include "specific-dummy-buffer.h"

#include <shader.h>
#include <util-interface.h>
#include <shader-var-cfg.h>
#include <device.h>
#include <header-mesh.h>
#include <draw-compatibility.h>
#include <texture-manager.h>
#include <particle-control.h>
#include <shader-resource.h>

namespace mbm
{
    BUFFER_SPECIFIC::BUFFER_SPECIFIC() noexcept 
    {
        REMINDER_TODO
    }

    BUFFER_SPECIFIC::~BUFFER_SPECIFIC()
    {
        this->release();
    }

    void BUFFER_SPECIFIC::release()
    {
        REMINDER_TODO
    }

    BUFFER_GL::BUFFER_GL() : fvf(FVF_PROVIDE_BY_ENGINE::FVF_POS_UV)
    {
        REMINDER_TODO
        //we initialize this at the moment (just once)
        setBackendBuffer(new BUFFER_SPECIFIC());
    }

    BUFFER_GL::~BUFFER_GL()
    {
        BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
        if(backendBuffer)
        {
            // Deleting a void* pointer directly in C++ is undefined behavior and should be avoided. 
            delete static_cast<BUFFER_SPECIFIC*>(backendBuffer);
        }
        setBackendBuffer(nullptr);
        texturesByStage.clear();
        REMINDER_TODO
    }

    void BUFFER_GL::release()
    {
        REMINDER_TODO
        //we do not delete bs
        BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
        backendBuffer->release();
        totalSubset   = 0;
    }

    bool BUFFER_GL::loadBuffer(const VEC3* vertex,
                               const VEC3* normal,
                               const VEC2* uv,
                               const uint32_t sizeOfArrayVertex,
                               const uint32_t totalSubsets,
                               const int* vertexStartSubset,
                               const int* vertexCountSubset,
                               const util::INFO_DRAW_MODE* info_draw_mode,
                               const bool isDynamic)
    {
        this->release();
        if (!vertex || !sizeOfArrayVertex || !totalSubsets || !vertexStartSubset || !vertexCountSubset)
            return false;
        REMINDER_TODO
        return true;
    }

    bool BUFFER_GL::loadBuffer(const VEC3 *vertex, // type index buffer
        const VEC3 *normal,const VEC2 *uv,const uint32_t sizeOfArrayVertex,
        const uint16_t *arrayIndices,const uint32_t totalSubsets,const int *indexStartSubset,
        const int *indexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        release();
        if (!vertex || !sizeOfArrayVertex || !arrayIndices || !totalSubsets || !indexStartSubset || !indexCountSubset)
            return false;
        REMINDER_TODO
        return true;
    }

    bool BUFFER_GL::loadBufferDynamic(  const uint16_t* arrayIndices,
                                        const unsigned int totalSubsets,
                                        const int* indexStartSubset,
                                        const int* indexCountSubset,
                                        const bool hasNormal,
                                        const bool hasUv,
                                        const util::INFO_DRAW_MODE* info_draw_mode)
    {
        release();
        if ( !arrayIndices || !totalSubsets || !indexStartSubset || !indexCountSubset)
            return false;
        REMINDER_TODO
        return true;
    }

    bool BUFFER_GL::loadParticleBuffer()
    {
        constexpr uint32_t totalSubset = 1;
        constexpr uint16_t arrayIndices[6] = { 0, 1, 2, 2, 1, 3 };
        constexpr int indexStartSubset = 0;
        constexpr int indexCountSubset = sizeof(arrayIndices) / sizeof(uint16_t);
        constexpr bool hasNormal       = false;
        constexpr bool hasUv           = true;
        return loadBufferDynamic(arrayIndices, totalSubset, &indexStartSubset, &indexCountSubset, hasNormal, hasUv, nullptr);
    }

    bool BUFFER_GL::updateDynamic(const VEC3* vertex,
                                  const VEC3* normal,
                                  const VEC2* uv,
                                  const int* vertexStartSubset,
                                  const int* vertexCountSubset)// update when dynamic
    {
        REMINDER_TODO
        return true;
    }

    bool BASE_SHADER::addVar(const char *nameVar, const TYPE_VAR_SHADER typeVar, const float *defaultValue,
                       void* ptrShaderSpecific, const bool isPS) // Adiciona uma variavel para o shader indicando o nome da mesma
                                                         // no código e o tipo.
    {
        if (nameVar)
        {
            if (strlen(nameVar) >= 255)
            {
#if defined _DEBUG
                PRINT_IF_DEBUG("max size 255!");
#endif
                return false;
            }
            if (isThereVarIntoLsVars(nameVar))
            {
#if defined _DEBUG
                PRINT_IF_DEBUG("Variable [%s] already exist.", nameVar);
#endif
                return false;
            }
            auto var       = new VAR_SHADER(std::string(nameVar), typeVar, isPS);
            REMINDER_TODO
            lsVar.push_back(var);
            return true;
        }
        return false;
    }

    void BASE_SHADER::update(void * ptrShaderSpecific) const
    {
        REMINDER_TODO
    }

    SHADER::SHADER()
    {
        REMINDER_TODO
    }

    SHADER::~SHADER()
    {
        REMINDER_TODO
        // Deleting a void* pointer directly in C++ is undefined behavior and should be avoided. 
    }

    void SHADER::onRestore()
    {
        REMINDER_TODO
    }

    void SHADER::releaseShader()
    {
        REMINDER_TODO
    }

    bool SHADER::compileShader(mbm::BASE_SHADER *ptrPshader, mbm::BASE_SHADER *ptrVshader, mbm::FVF_PROVIDE_BY_ENGINE fvf)
    {
        if (fvf == FVF_PROVIDE_BY_ENGINE::FVF_NONE)
            return false;
        this->pShader             = ptrPshader;
        this->vShader             = ptrVshader;
        constexpr char *defaultCodePs = "sampler2D sample0 : register(s0);"
                                        ""
                                        "float4 main(float2 texCoord : TEXCOORD0) : COLOR"
                                        "{"
                                        "    return tex2D(sample0, texCoord);"
                                        "}";

        constexpr char *defaultCodeVs = "float4x4 mvpMatrix : register(c0);"
                                        ""
                                        "struct VS_INPUT"
                                        "{"
                                        "    float4 position : POSITION;"
                                        "    float2 texCoord : TEXCOORD0;"
                                        // "    float3 normal : NORMAL;" Per-vertex normal information we will pass in. (not used, removed since cause confusion
                                        "};"
                                        ""
                                        "struct VS_OUTPUT"
                                        "{"
                                        "    float4 position : POSITION;"
                                        "    float2 texCoord : TEXCOORD0;"
                                        "};"
                                        ""
                                        "VS_OUTPUT main(VS_INPUT input)"
                                        "{"
                                        "    VS_OUTPUT output;"
                                        "    output.position = mul(input.position, mvpMatrix);"
                                        "    output.texCoord = input.texCoord;"
                                        "    return output;"
                                        "}";
        


        
        constexpr char* mainFunction = "main";
        const char* versionPS        = getPSVersion();
        const char* versionVS        = getVSVersion();
        
        const char* codePS = ptrPshader ? this->pShader->getCode() : defaultCodePs;
        const char* codeVS = ptrVshader ? this->vShader->getCode() : defaultCodeVs;
        const int sizeOfCodePS = strlen(codePS);
        const int sizeOfCodeVS = strlen(codeVS);
        const std::string combinedShaderCode = std::string(codePS) + codeVS;
        const SHADER_TEXTURE_NAMING textureNaming =
            detectShaderTextureNamingProfile(combinedShaderCode.c_str());
        if (textureNaming == SHADER_TEXTURE_NAMING_MIXED_INVALID)
        {
            ERROR_LOG("Dummy shader mixes legacy texture names with semantic texture roles");
            return false;
        }
        if (textureNaming == SHADER_TEXTURE_NAMING_SEMANTIC_ROLE &&
            (shaderCodeDeclaresTextureRole(combinedShaderCode.c_str(), TEXTURE_ROLE_SPECULAR, textureNaming) ||
             shaderCodeDeclaresTextureRole(combinedShaderCode.c_str(), TEXTURE_ROLE_EMISSIVE, textureNaming) ||
             shaderCodeDeclaresTextureRole(combinedShaderCode.c_str(), TEXTURE_ROLE_MASK, textureNaming)))
        {
            ERROR_LOG("Dummy shader declares a reserved semantic texture role without runtime binding support");
            return false;
        }
        REMINDER_TODO
        return true;
    }


    bool SHADER::render(const BUFFER_GL *pBufferId) const
    {
        REMINDER_TODO
        return true;
    }

    bool SHADER::renderDynamic(const BUFFER_GL *pBufferId,const VEC3 *vertex,const VEC3 *normal,const VEC2 *uv) const
    {
        REMINDER_TODO
        return false;
    }

    bool SHADER::renderParticle(const BUFFER_GL* pBufferId, const PARTICLE_CONTROL* particleControl) const
    {
        REMINDER_TODO
        return true;
    }

    bool SHADER::renderParticle(const BUFFER_GL* pBufferId, const FLUID_GROUP* pGroup) const
    {
        REMINDER_TODO
        return true;
    }
}

#endif // USE_DUMMY_BACK_END_ENGINE
