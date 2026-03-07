/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.                                    |
|-----------------------------------------------------------------------------------------------------------------------*/

// Metal shader stubs.
// Provides the backend-specific shader, buffer, and shader-variable functions
// that are defined in shader-opengl_es.cpp and guarded by USE_OPENGL_ES.
// For Milestone 1 (empty scene) none of these are called at runtime, but the
// linker requires their symbols.

#if defined(USE_METAL)

#include <shader.h>
#include <shader-var-cfg.h>
#include <texture-manager.h>
#include <specific-metal.h>
#include <util-interface.h>
#include <particle-control.h>
#include <header-mesh.h>

namespace mbm
{
    // ---- BUFFER_GL constructor / destructor ----

    BUFFER_GL::BUFFER_GL() :
        indexStartIB(nullptr),
        indexCountIB(nullptr),
        vertexStartVB(nullptr),
        vertexCountVB(nullptr),
        sizeOfArrayVertex(0),
        fvf(FVF_PROVIDE_BY_ENGINE::FVF_POS_UV),
        mode_draw(0),              // MTLPrimitiveTypeTriangle will be set per-draw
        mode_cull_face(0),
        mode_front_face_direction(0),
        totalSubset(0),
        initializedIndexBuffer(false),
        texture1(nullptr)
    {
        bs = new BUFFER_SPECIFIC();
    }

    BUFFER_GL::~BUFFER_GL()
    {
        if (bs)
        {
            delete static_cast<BUFFER_SPECIFIC*>(bs);
        }
        bs       = nullptr;
        texture1 = nullptr;
        texture0.clear();
    }

    // ---- BUFFER_GL backend methods ---- (Must be provided by each backend)

    void BUFFER_GL::release()
    {
        if (bs) bs->release();
        totalSubset = 0;
    }

    bool BUFFER_GL::loadBuffer(const VEC3* /*vertex*/, const VEC3* /*normal*/, const VEC2* /*uv*/,
                               const uint32_t /*sizeOfArrayVertex*/, const uint32_t /*totalSubsets*/,
                               const int* /*vertexStartSubset*/, const int* /*vertexCountSubset*/,
                               const util::INFO_DRAW_MODE* /*info_draw_mode*/, const bool /*isDynamic*/)
    {
        // TODO: create MTLBuffer from vertex data.
        return false;
    }

    bool BUFFER_GL::loadBuffer(const VEC3* /*vertex*/, const VEC3* /*normal*/, const VEC2* /*uv*/,
                               const uint32_t /*sizeOfArrayVertex*/, const uint16_t* /*arrayIndices*/,
                               const unsigned int /*totalSubsets*/, const int* /*indexStartSubset*/,
                               const int* /*indexCountSubset*/, const util::INFO_DRAW_MODE* /*info_draw_mode*/)
    {
        // TODO: create indexed MTLBuffer.
        return false;
    }

    bool BUFFER_GL::loadBufferDynamic(const uint16_t* /*arrayIndices*/, const unsigned int /*totalSubsets*/,
                                      const int* /*indexStartSubset*/, const int* /*indexCountSubset*/,
                                      const bool /*hasNormal*/, const bool /*hasUv*/,
                                      const util::INFO_DRAW_MODE* /*info_draw_mode*/)
    {
        // TODO: create dynamic MTLBuffer.
        return false;
    }

    bool BUFFER_GL::updateDynamic(const VEC3* /*vertex*/, const VEC3* /*normal*/, const VEC2* /*uv*/,
                                  const int* /*vertexStartSubset*/, const int* /*vertexCountSubset*/)
    {
        // TODO: update MTLBuffer contents via replaceBytes:range:.
        return false;
    }

    bool BUFFER_GL::loadParticleBuffer()
    {
        // TODO: create particle index MTLBuffer.
        return false;
    }

    // ---- BASE_SHADER ----

    bool BASE_SHADER::addVar(const char* nameVar, const TYPE_VAR_SHADER typeVar,
                             const float* defaultValue, void* /*ptrShaderSpecific*/, const bool isPS)
    {
        if (!nameVar) return false;
        if (isThereVarIntoLsVars(nameVar)) return false;
        auto* var = new VAR_SHADER(std::string(nameVar), typeVar, isPS);
        if (defaultValue)
            memcpy(var->current, defaultValue, var->sizeVar * sizeof(float));
        lsVar.push_back(var);
        return true;
    }

    void BASE_SHADER::update(void* /*ptrShaderSpecific*/) const
    {
        // TODO: upload uniform buffer to Metal shader via setVertexBytes / setFragmentBytes.
    }

    // ---- GLES_PS_VS — not used for Metal ----
    // (GLES_PS_VS is declared in specific-opengl_es.h and only needed by
    //  the OpenGL backend.  Metal does not include that header.)

    // ---- SHADER ----

    SHADER::SHADER() : ptrShaderSpecific(nullptr),
        pShader(nullptr),
        vShader(nullptr)
    {
    }

    SHADER::~SHADER()
    {
        if (ptrShaderSpecific)
        {
            // Release the retained MTLRenderPipelineState CFBridging object.
            CFRelease(ptrShaderSpecific);
            ptrShaderSpecific = nullptr;
        }
    }

    void SHADER::onRestore()
    {
        releaseShader();
    }

    void SHADER::releaseShader()
    {
        if (ptrShaderSpecific)
        {
            CFRelease(ptrShaderSpecific);
            ptrShaderSpecific = nullptr;
        }
        pShader = nullptr;
        vShader = nullptr;
    }

    bool SHADER::isLoad() const noexcept
    {
        return ptrShaderSpecific != nullptr;
    }

    bool SHADER::compileShader(BASE_SHADER* /*ptrPshader*/, BASE_SHADER* /*ptrVshader*/,
                               FVF_PROVIDE_BY_ENGINE /*fvf*/)
    {
        // TODO: compile MSL shader and create MTLRenderPipelineState.
        WARN_LOG("Metal: SHADER::compileShader() is not yet implemented. "
                 "Shader compilation will be added in a later milestone.");
        return false;
    }

    bool SHADER::render(const BUFFER_GL* /*pBufferId*/) const
    {
        // TODO: encode draw call into current MTLRenderCommandEncoder.
        return false;
    }

    bool SHADER::renderDynamic(const BUFFER_GL* /*pBufferId*/, const VEC3* /*vertex*/,
                               const VEC3* /*normal*/, const VEC2* /*uv*/) const
    {
        // TODO: upload dynamic vertex data and encode draw call.
        return false;
    }

    bool SHADER::renderParticle(const BUFFER_GL* /*pBufferId*/,
                                const PARTICLE_CONTROL* /*particleControl*/) const
    {
        return false;
    }

    bool SHADER::renderParticle(const BUFFER_GL* /*pBufferId*/,
                                const FLUID_GROUP* /*pGroup*/) const
    {
        return false;
    }

} // namespace mbm

#endif // USE_METAL
