/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef _SHADERS_GLES_H
#define _SHADERS_GLES_H

#include "core-exports.h"
#include "primitives.h"
#include "particle-control.h"
#include "texture-role.h"
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>


namespace util
{
    struct INFO_DRAW_MODE;
};

namespace mbm
{
    enum class SKELETAL_SHADER_METHOD : uint8_t
    {
        NONE,
        LBS,
        DQS_RIGID,
        AUTO
    };

    enum class SKELETAL_EXECUTION_PATH : uint8_t
    {
        GPU,
        CPU
    };
    class TEXTURE;
    class RENDERIZABLE;
    struct VAR_SHADER;
    struct BUFFER_SPECIFIC;
    struct PARTICLE_CONTROL;
    enum TYPE_VAR_SHADER : char;

    /** Flexible Vertex Format - indicates which vertex attributes (normal, UV) the vertex has.
     *  Passed to compileShader so the shader can add/remove aNormal and vTexCoord.
     *  FVF_NONE = invalid/undefined; compileShader fails when given this (helps detect invalid flow). */
    enum class FVF_PROVIDE_BY_ENGINE
    {
        FVF_NONE,      // invalid/undefined - compileShader must fail
        FVF_POS,       // position only
        FVF_POS_UV,    // position + UV
        FVF_POS_NOR,   // position + normal
        FVF_POS_NOR_UV // position + normal + UV
    };

    enum TYPE_ANIMATION : char
    { 
      TYPE_ANIMATION_PAUSED          = 0, // Pausa A Animação
      TYPE_ANIMATION_GROWING         = 1, // Incrementa Na Ordem Crescente Parando No Limite Maximo
      TYPE_ANIMATION_GROWING_LOOP    = 2, // Incrementa Na Ordem Crescente e Faz Loop Quando Ultrapassar Limite Maximo
      TYPE_ANIMATION_DECREASING      = 3, // Decrementa Na Ordem Decrescente E Para No Limite Mínimo
      TYPE_ANIMATION_DECREASING_LOOP = 4, // Decrementa Na Ordem Decrescente e Faz LoopQuando Ultrapassar O Limite Mínimo
      TYPE_ANIMATION_RECURSIVE       = 5, // icrementa Na Ordem Crescente e Decrescente; Para No Limite Mínimo
      TYPE_ANIMATION_RECURSIVE_LOOP  = 6 // Incrementa Na Ordem Crescente e Decrescente. Faz loop Entre O Limite Mínimo E O Limite Maximo.
    };

    enum STATUS_FX
    {
        FX_GROWING, FX_DECREASING, FX_END, FX_END_CALLBACK
    };

    enum SHADER_TEXTURE_NAMING : uint8_t
    {
        SHADER_TEXTURE_NAMING_NONE = 0,
        SHADER_TEXTURE_NAMING_SEMANTIC_ROLE = 1,
        SHADER_TEXTURE_NAMING_MIXED_INVALID = 2
    };

    API_IMPL const char *getTextureRoleShaderName(TEXTURE_ROLE role, SHADER_TEXTURE_NAMING naming) noexcept;
    API_IMPL int getTextureRoleBackendSlot(TEXTURE_ROLE role) noexcept;
    API_IMPL SHADER_TEXTURE_NAMING parseShaderTextureNaming(const char *value) noexcept;
    API_IMPL const char *getShaderTextureNamingName(SHADER_TEXTURE_NAMING naming) noexcept;
    API_IMPL SHADER_TEXTURE_NAMING detectShaderTextureNamingProfile(const char *shaderCode) noexcept;
    API_IMPL bool shaderCodeDeclaresTextureRole(const char *shaderCode,
                                                TEXTURE_ROLE role,
                                                SHADER_TEXTURE_NAMING naming) noexcept;

    
    class BUFFER_GL // Buffer graphic layer (must be implemented by specific backend, e.g.: Opengl_es, Directx, Vulkan)
    {
      public:
        API_IMPL BUFFER_GL();
        API_IMPL virtual ~BUFFER_GL();
        API_IMPL bool isLoadedBuffer() const;
        API_IMPL void release();
        
        API_IMPL bool loadBuffer(const VEC3 *vertex,
                                 const VEC3 *normal,
                                 const VEC2 *uv,
                                 const uint32_t sizeOfArrayVertex,
                                 const uint32_t totalSubsets,
                                 const int *vertexStartSubset,
                                 const int *vertexCountSubset,
                                 const util::INFO_DRAW_MODE * info_draw_mode,
                                 const bool isDynamic);// type vertex buffer, must be implemented by specific backend engine

        API_IMPL bool loadBuffer(const VEC3 *vertex,
                                 const VEC3 *normal,
                                 const VEC2 *uv,
                                 const uint32_t sizeOfArrayVertex,
                                 const uint16_t *arrayIndices,
                                 const unsigned int totalSubsets,
                                 const int *indexStartSubset,
                                 const int *indexCountSubset,
                                 const util::INFO_DRAW_MODE * info_draw_mode);// type index buffer, must be implemented by specific backend engine

        API_IMPL bool loadBufferDynamic(const uint16_t *arrayIndices, 
                                        const unsigned int totalSubsets, 
                                        const int *indexStartSubset,
                                        const int *indexCountSubset,
                                        const bool hasNormal,
                                        const bool hasUv,
                                        const util::INFO_DRAW_MODE * info_draw_mode);// Dynamic Index buffer, must be implemented by specific backend engine

        API_IMPL bool updateDynamic(const VEC3* vertex,
                                    const VEC3* normal,
                                    const VEC2* uv,
                                    const int* vertexStartSubset,
                                    const int* vertexCountSubset);// update vertex buffer when dynamic

        API_IMPL bool loadParticleBuffer();// type index buffer only, must be implemented by specific backend engine

        API_IMPL TEXTURE* getTextureByStage(const uint32_t index_stage,const uint32_t index_subset) const;// common implemenmtation
        API_IMPL void setTextureByStage(TEXTURE* texture,const uint32_t index_stage,const uint32_t index_subset);// common implemenmtation
        API_IMPL BUFFER_SPECIFIC * getBackendBuffer() const noexcept;
        API_IMPL void setBackendBuffer(BUFFER_SPECIFIC *backendBuffer) noexcept;

        int32_t* indexStartIB;      // index start subset IB
        int32_t* indexCountIB;      // index count subset IB
        int32_t* vertexStartVB;     // Start vertex buffer per each subset VB
        int32_t* vertexCountVB;     // Total vertex buffer per each subset VB
        uint32_t sizeOfArrayVertex; // Size of Vertx array

        FVF_PROVIDE_BY_ENGINE fvf;  // vertex format (set by loadBuffer); used when compiling default shader

        uint32_t mode_draw; //default (GL_TRIANGLES), mode: GL_POINTS, GL_LINES, GL_LINE_LOOP, GL_LINE_STRIP, GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN
        uint32_t mode_cull_face;//GL_FRONT, GL_BACK,GL_FRONT_AND_BACK
        uint32_t mode_front_face_direction; //GL_CW, GL_CCW

        inline bool isIndexBuffer() const noexcept { return initializedIndexBuffer; }
        uint32_t totalSubset;   // Total of subset of this buffer

      private:
        struct BackendData;
        struct BackendDataDeleter
        {
            void operator()(BackendData *data) const noexcept;
        };

        std::unique_ptr<BackendData, BackendDataDeleter> backendData;
        bool     initializedIndexBuffer;
        
        void initializeVertexBufferControl(const uint32_t totalSubsets,
                                           const uint32_t _sizeOfArrayVertex,
                                           const int* vertexStartSubset,
                                           const int* vertexCountSubset,
                                           const util::INFO_DRAW_MODE* info_draw_mode);

        void initializeIndexBufferControl(const uint32_t totalSubsets,
                                          const uint32_t _sizeOfArrayVertex,
                                          const int* indexStartSubset,
                                          const int* indexCountSubset,
                                          const util::INFO_DRAW_MODE* info_draw_mode);

        // Stage 0 keeps the historical "one texture per subset" contract.
        // Stage 1 keeps the legacy shared FX texture semantics (subset 0 fallback).
        // Stage 2+ can be used for per-subset material textures such as normal maps.
        std::unordered_map<uint32_t, std::unordered_map<uint32_t, TEXTURE*>> texturesByStage;
    };

    class BASE_SHADER
    {
        friend class SHADER;
      public:
        std::string fileName; // shader
        API_IMPL BASE_SHADER() noexcept;
        API_IMPL virtual ~BASE_SHADER();
        API_IMPL const char *getCode();
        API_IMPL VAR_SHADER *getVarByName(const char *nameVar);
        API_IMPL VAR_SHADER *getVar(const unsigned int indexVar);
        API_IMPL bool addVar(const char *nameVar, const TYPE_VAR_SHADER typeVar, const float *defaultValue,void* ptrShaderSpecific, const bool isPS);
        API_IMPL unsigned int getTotalVar() const noexcept;
        API_IMPL void releaseVars();
        API_IMPL bool loadShader(const char *fileNameShaderVS_PS, const char *code);
        API_IMPL SHADER_TEXTURE_NAMING getTextureNamingProfile() const noexcept;
        std::vector<VAR_SHADER *> *getVars();
      private:
        bool isThereVarIntoLsVars(const char *nameVar);
        void update(void* ptrShaderSpecific) const;
      protected:
        std::vector<VAR_SHADER *> lsVar;
        std::string        stringCodeShader;
        SHADER_TEXTURE_NAMING textureNamingProfile;
    };

    class SHADER
    {
      public:
        API_IMPL static MATRIX modelView;
        API_IMPL static MATRIX mvMatrixLightSpace; // modelView x camera view - true view-space matrix uploaded as the "mvMatrix" shader uniform (drives vNormalView/vPositionView)
        API_IMPL static MATRIX mvpMatrix; // ModelView x projection
        API_IMPL static void updateMvpAndLightMatrices(const MATRIX &viewMatrix, const MATRIX &perspectiveMatrix) noexcept;
        // Clears the backend's process-lifetime cache of compiled default shader programs (see
        // compileShader's default-shader-pair fast path). Must be called after a GL/device context
        // is destroyed and recreated (CORE_MANAGER::onLostDevice) -- every previously compiled
        // program id is invalid (and may be silently reused for a *different* program) once that
        // happens. No-op on backends that don't cache (DirectX9, Metal, dummy).
        API_IMPL static void clearDefaultProgramCache() noexcept;
        API_IMPL SHADER();
        API_IMPL virtual ~SHADER();
        API_IMPL void * getBackendShaderSpecific() const noexcept;
        API_IMPL void setBackendShaderSpecific(void *backendShaderSpecific) noexcept;
        API_IMPL void setUseReservedLightDefault(bool enabled) noexcept;
        API_IMPL void releaseShader();
        API_IMPL void onRestore();
        API_IMPL bool compileShader(BASE_SHADER *ptrPshader, BASE_SHADER *ptrVshader,
                                    FVF_PROVIDE_BY_ENGINE fvf,
                                    uint32_t skeletalPaletteSize = 0,
                                    SKELETAL_SHADER_METHOD skeletalMethod = SKELETAL_SHADER_METHOD::LBS);
        API_IMPL bool isLoad() const noexcept;
        API_IMPL bool render(const BUFFER_GL *pBufferId, const RENDERIZABLE *renderizableOwner = nullptr,
                             const int32_t subsetIndex = -1,
                             const float *skeletalPaletteRows = nullptr,
                             uint32_t skeletalPaletteFloatCount = 0) const;
        API_IMPL bool renderParticle(const BUFFER_GL* pBufferId, const PARTICLE_CONTROL* particleControl) const;
        API_IMPL bool renderParticle(const BUFFER_GL* pBufferId, const FLUID_GROUP* pGroup) const;
        API_IMPL bool renderDynamic(const BUFFER_GL *pBufferId,const VEC3 *vertex,const VEC3 *normal,const VEC2 *uv,
                                    const RENDERIZABLE *renderizableOwner = nullptr) const;
        API_IMPL void update();
      private:
        bool usesPureDefaultShaderPair() const noexcept;
        bool shouldCompileReservedLightDefault() const noexcept;
        struct BackendData;
        struct BackendDataDeleter
        {
            void operator()(BackendData *data) const noexcept;
        };

        std::unique_ptr<BackendData, BackendDataDeleter> backendData;
        BASE_SHADER *pShader;
        BASE_SHADER *vShader;
    };

}

#endif
