/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.                                    |
|-----------------------------------------------------------------------------------------------------------------------*/

// Metal shader resource stubs.
// Provides the free functions that shader-cfg.cpp calls to discover the
// engine's built-in shader catalogue.
//
// For Milestone 1 (solid-colour clear) no shaders are actually rendered, so
// we return an empty resource table and "TODO" placeholders everywhere.
// MSL shader source will be filled in when shader drawing is implemented.

#if defined(USE_METAL)

#include <string>

namespace mbm
{
    // Empty resource table — triple-nullptr sentinel terminates the list.
    static const char* resourceShader[] = { nullptr, nullptr, nullptr };

    const char** getShaderEngineBuiltIn()
    {
        return resourceShader;
    }

    const char* getCodePScolorFor_LINE_MESH()
    {
        // TODO: MSL pixel shader for LINE_MESH.
        static const char* code = "TODO_metal";
        return code;
    }

    const char* getCodeVScolorFor_LINE_MESH()
    {
        // TODO: MSL vertex shader for LINE_MESH.
        static const char* code = "TODO_metal";
        return code;
    }

    const char* getParticlePSCode()
    {
        // TODO: MSL particle pixel shader.
        static const char* code = "TODO_metal";
        return code;
    }

    const char* getParticleVSCode()
    {
        // TODO: MSL particle vertex shader.
        static const char* code = "TODO_metal";
        return code;
    }

    const char* getSteeredParticlePSCode(bool /*hasColor*/)
    {
        // TODO: MSL steered-particle pixel shader.
        static const char* code = "TODO_metal";
        return code;
    }

    const char* getSteeredParticleVSCode()
    {
        // TODO: MSL steered-particle vertex shader.
        static const char* code = "TODO_metal";
        return code;
    }

    static std::string PS_Version("metal_ps");
    static std::string VS_Version("metal_vs");

    const char* getPSVersion() { return PS_Version.c_str(); }
    const char* getVSVersion() { return VS_Version.c_str(); }

    void setPSVersion(const char* version)
    {
        PS_Version = version ? version : "";
    }
    void setVSVersion(const char* version)
    {
        VS_Version = version ? version : "";
    }

} // namespace mbm

#endif // USE_METAL
