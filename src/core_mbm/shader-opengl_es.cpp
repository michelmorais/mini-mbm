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


#if defined (USE_OPENGL_ES)

#include <shader.h>
#include <device.h>
#include <light.h>
#include <texture-manager.h>
#include <specific-opengl_es.h>
#include "specific-opengl_es-buffer.h"
#include "specific-opengl_es-shader.h"
#include <util-interface.h>
#include <shader-var-cfg.h>
#include <draw-compatibility.h>
#include <header-mesh.h>
#include <particle-control.h>

namespace mbm
{
    static uint32_t loadShaderProgram(BASE_SHADER* pShader, BASE_SHADER* vShader, void* ptrShaderSpecific, const char* vertShaderSrc, const char* fragShaderSrc);
    static uint32_t compileCodeShader(BASE_SHADER* ptrShader, const unsigned int type, const char* shaderSrc);

    static const MATRIX &getViewMatrixForLightTarget(const LIGHT_TARGET target)
    {
        const CAMERA &camera = DEVICE::getInstance()->getCamera();
        return target == LIGHT_TARGET_2DW ? camera.matrixView2d : camera.matrixView;
    }

    static VEC3 getLightDirectionView(const LIGHT_STATE &lightState, const LIGHT_TARGET target)
    {
        const MATRIX &view = getViewMatrixForLightTarget(target);
        VEC3 directionView(
            (lightState.directionalDirection.x * view._11) +
            (lightState.directionalDirection.y * view._21) +
            (lightState.directionalDirection.z * view._31),
            (lightState.directionalDirection.x * view._12) +
            (lightState.directionalDirection.y * view._22) +
            (lightState.directionalDirection.z * view._32),
            (lightState.directionalDirection.x * view._13) +
            (lightState.directionalDirection.y * view._23) +
            (lightState.directionalDirection.z * view._33));
        Vec3Normalize(&directionView, &directionView);
        return directionView;
    }

    static VEC3 getLightPositionView(const LIGHT_STATE &lightState, const LIGHT_TARGET target)
    {
        const MATRIX &view = getViewMatrixForLightTarget(target);
        return VEC3(
            (lightState.pointPosition.x * view._11) +
            (lightState.pointPosition.y * view._21) +
            (lightState.pointPosition.z * view._31) + view._41,
            (lightState.pointPosition.x * view._12) +
            (lightState.pointPosition.y * view._22) +
            (lightState.pointPosition.z * view._32) + view._42,
            (lightState.pointPosition.x * view._13) +
            (lightState.pointPosition.y * view._23) +
            (lightState.pointPosition.z * view._33) + view._43);
    }

    static VEC3 getPointLightPositionView(const VEC3 &pointPosition, const LIGHT_TARGET target)
    {
        const MATRIX &view = getViewMatrixForLightTarget(target);
        return VEC3(
            (pointPosition.x * view._11) +
            (pointPosition.y * view._21) +
            (pointPosition.z * view._31) + view._41,
            (pointPosition.x * view._12) +
            (pointPosition.y * view._22) +
            (pointPosition.z * view._32) + view._42,
            (pointPosition.x * view._13) +
            (pointPosition.y * view._23) +
            (pointPosition.z * view._33) + view._43);
    }

    static bool bufferHasUv(const BUFFER_GL *pBufferId) noexcept
    {
        if (pBufferId == nullptr)
            return false;
        return pBufferId->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_UV ||
               pBufferId->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV;
    }

    static bool bufferHasNormal(const BUFFER_GL *pBufferId) noexcept
    {
        if (pBufferId == nullptr)
            return false;
        return pBufferId->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR ||
               pBufferId->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV;
    }

    static int getReservedLightMode(const LIGHT_STATE &lightState, const LIGHT_TARGET target,
                                    const BUFFER_GL *pBufferId) noexcept
    {
        if (lightState.enabled == false)
            return 0;
        if (target == LIGHT_TARGET_2DW)
            return bufferHasUv(pBufferId) ? 2 : 0;
        return bufferHasNormal(pBufferId) ? 1 : 0;
    }

    static util::MATERIAL getReservedMaterialForCurrentRender()
    {
        util::MATERIAL material;
        if (DEVICE::getInstance()->getMaterialForCurrentRender(material))
            return material;
        material.Diffuse = COLOR(1.0f, 1.0f, 1.0f, 1.0f);
        material.Ambient = COLOR(1.0f, 1.0f, 1.0f, 1.0f);
        material.Specular = COLOR(0.0f, 0.0f, 0.0f, 1.0f);
        material.Emissive = COLOR(0.0f, 0.0f, 0.0f, 1.0f);
        material.Power = 0.0f;
        return material;
    }

    struct ScopedRenderizableContext
    {
        DEVICE *device;

        ScopedRenderizableContext(const RENDERIZABLE *renderizableOwner) noexcept
            : device(DEVICE::getInstance())
        {
            device->setRenderizableForCurrentRender(renderizableOwner);
        }

        ~ScopedRenderizableContext() noexcept
        {
            device->clearRenderizableForCurrentRender();
        }
    };

    static GLint getOptionalArrayUniformLocation(const uint32_t programObject, const char *uniformName)
    {
        char uniformNameIndex0[128];
        snprintf(uniformNameIndex0, sizeof(uniformNameIndex0), "%s[0]", uniformName);
        return GLGetUniformLocationOptional(programObject, uniformNameIndex0);
    }

    static void uploadReservedLightUniformsOpenGlEs(const uint32_t programObject, const BUFFER_GL *pBufferId,
                                                    const uint32_t subsetIndex)
    {
        if (programObject == 0)
            return;
        LIGHT_STATE lightState;
        LIGHT_TARGET lightTarget = LIGHT_TARGET_3D;
        const bool hasRenderLight = DEVICE::getInstance()->getLightStateForCurrentRender(lightState);
        DEVICE::getInstance()->getLightTargetForCurrentRender(lightTarget);
        if (hasRenderLight == false)
            lightState = LIGHT_STATE();
        const util::MATERIAL material = getReservedMaterialForCurrentRender();
        const int lightMode = getReservedLightMode(lightState, lightTarget, pBufferId);
        const int enabled = lightMode != 0 ? 1 : 0;
        const int hasNormalMap = (lightMode == 2 && pBufferId && pBufferId->getTextureByStage(2, subsetIndex)) ? 1 : 0;
        const VEC3 directionView = getLightDirectionView(lightState, lightTarget);
        VEC3 positionView = getLightPositionView(lightState, lightTarget);
        COLOR lightColor = lightTarget == LIGHT_TARGET_2DW ? lightState.pointColor : lightState.directionalColor;
        float lightRadius = lightState.pointRadius;
        float lightPositionViewArray[DEFAULT_SUPPORTED_MAX_LIGHTS * 3] = {};
        float lightRadiusArray[DEFAULT_SUPPORTED_MAX_LIGHTS] = {};
        float lightColorArray[DEFAULT_SUPPORTED_MAX_LIGHTS * 4] = {};
        uint32_t selectedPointLightCount = 0u;
        if (lightMode == 2)
        {
            LIGHT_POINT_SELECTION pointLightSelections[DEFAULT_SUPPORTED_MAX_LIGHTS];
            selectedPointLightCount = DEVICE::getInstance()->getSelectedPointLightsForCurrentRender(
                pointLightSelections, DEFAULT_SUPPORTED_MAX_LIGHTS);
            for (uint32_t i = 0; i < selectedPointLightCount; ++i)
            {
                const VEC3 selectedPositionView =
                    getPointLightPositionView(pointLightSelections[i].pointLight.position, lightTarget);
                lightPositionViewArray[(i * 3u) + 0u] = selectedPositionView.x;
                lightPositionViewArray[(i * 3u) + 1u] = selectedPositionView.y;
                lightPositionViewArray[(i * 3u) + 2u] = selectedPositionView.z;
                lightRadiusArray[i] = pointLightSelections[i].pointLight.radius;
                lightColorArray[(i * 4u) + 0u] = pointLightSelections[i].pointLight.color.r;
                lightColorArray[(i * 4u) + 1u] = pointLightSelections[i].pointLight.color.g;
                lightColorArray[(i * 4u) + 2u] = pointLightSelections[i].pointLight.color.b;
                lightColorArray[(i * 4u) + 3u] = pointLightSelections[i].pointLight.color.a;
            }
            if (selectedPointLightCount > 0u)
            {
                positionView.x = lightPositionViewArray[0];
                positionView.y = lightPositionViewArray[1];
                positionView.z = lightPositionViewArray[2];
                lightRadius = lightRadiusArray[0];
                lightColor = COLOR(lightColorArray[0], lightColorArray[1], lightColorArray[2], lightColorArray[3]);
            }
        }
        else
        {
            lightPositionViewArray[0] = positionView.x;
            lightPositionViewArray[1] = positionView.y;
            lightPositionViewArray[2] = positionView.z;
            lightRadiusArray[0] = lightRadius;
            lightColorArray[0] = lightColor.r;
            lightColorArray[1] = lightColor.g;
            lightColorArray[2] = lightColor.b;
            lightColorArray[3] = lightColor.a;
        }
        const int lightCount = lightMode == 2 ? static_cast<int>(selectedPointLightCount) : (enabled != 0 ? 1 : 0);

        GLint handle = GLGetUniformLocationOptional(programObject, "LightEnabled");
        if (handle != -1)
        {
            GLUniform1i(handle, enabled);
        }
        handle = GLGetUniformLocationOptional(programObject, "LightCount");
        if (handle != -1)
        {
            GLUniform1i(handle, lightCount);
        }
        handle = GLGetUniformLocationOptional(programObject, "LightMode");
        if (handle != -1)
        {
            GLUniform1i(handle, lightMode);
        }
        handle = GLGetUniformLocationOptional(programObject, "AmbientColor");
        if (handle != -1)
        {
            GLUniform4f(handle, lightState.ambientColor.r, lightState.ambientColor.g,
                        lightState.ambientColor.b, lightState.ambientColor.a);
        }
        handle = GLGetUniformLocationOptional(programObject, "LightDirectionView");
        if (handle != -1)
        {
            GLUniform3f(handle, directionView.x, directionView.y, directionView.z);
        }
        handle = GLGetUniformLocationOptional(programObject, "LightPositionView");
        if (handle != -1)
        {
            GLUniform3f(handle, positionView.x, positionView.y, positionView.z);
        }
        handle = GLGetUniformLocationOptional(programObject, "LightRadius");
        if (handle != -1)
        {
            GLUniform1f(handle, lightRadius);
        }
        handle = GLGetUniformLocationOptional(programObject, "LightColor");
        if (handle != -1)
        {
            GLUniform4f(handle, lightColor.r, lightColor.g, lightColor.b, lightColor.a);
        }
        handle = getOptionalArrayUniformLocation(programObject, "LightPositionView");
        if (handle != -1)
        {
            glUniform3fv(handle, DEFAULT_SUPPORTED_MAX_LIGHTS, lightPositionViewArray);
        }
        handle = getOptionalArrayUniformLocation(programObject, "LightRadius");
        if (handle != -1)
        {
            glUniform1fv(handle, DEFAULT_SUPPORTED_MAX_LIGHTS, lightRadiusArray);
        }
        handle = getOptionalArrayUniformLocation(programObject, "LightColor");
        if (handle != -1)
        {
            glUniform4fv(handle, DEFAULT_SUPPORTED_MAX_LIGHTS, lightColorArray);
        }
        handle = GLGetUniformLocationOptional(programObject, "HasNormalMap");
        if (handle != -1)
        {
            GLUniform1i(handle, hasNormalMap);
        }
        handle = GLGetUniformLocationOptional(programObject, "MaterialDiffuse");
        if (handle != -1)
        {
            GLUniform4f(handle, material.Diffuse.r, material.Diffuse.g, material.Diffuse.b, material.Diffuse.a);
        }
        handle = GLGetUniformLocationOptional(programObject, "MaterialAmbient");
        if (handle != -1)
        {
            GLUniform4f(handle, material.Ambient.r, material.Ambient.g, material.Ambient.b, material.Ambient.a);
        }
        handle = GLGetUniformLocationOptional(programObject, "MaterialSpecular");
        if (handle != -1)
        {
            GLUniform4f(handle, material.Specular.r, material.Specular.g, material.Specular.b, material.Specular.a);
        }
        handle = GLGetUniformLocationOptional(programObject, "MaterialEmissive");
        if (handle != -1)
        {
            GLUniform4f(handle, material.Emissive.r, material.Emissive.g, material.Emissive.b, material.Emissive.a);
        }
        handle = GLGetUniformLocationOptional(programObject, "MaterialPower");
        if (handle != -1)
        {
            GLUniform1f(handle, material.Power);
        }
    }

    static const TEXTURE *getBoundTextureForRoleOpenGlEs(const BUFFER_GL *pBufferId,
                                                         const uint32_t subsetIndex,
                                                         const TEXTURE_ROLE role)
    {
        if (pBufferId == nullptr)
            return nullptr;
        const uint32_t stageIndex = static_cast<uint32_t>(getTextureRoleBackendSlot(role));
        // Callers always pass subsetIndex=0 for TEXTURE_ROLE_ANIMATION_EFFECT (it's one shared
        // texture per animation, not per-subset - see the per-draw-call bind sites), so no
        // role-specific override is needed here.
        const TEXTURE *texture = pBufferId->getTextureByStage(stageIndex, subsetIndex);
        if (texture)
            return texture;
        return TEXTURE_MANAGER::getInstance()->getFallbackTexture(role);
    }

    static void bindTextureRoleOpenGlEs(const BUFFER_GL *pBufferId,
                                        const uint32_t subsetIndex,
                                        const TEXTURE_ROLE role,
                                        const GLint samplerHandle)
    {
        const int backendSlot = getTextureRoleBackendSlot(role);
        GLActiveTexture(GL_TEXTURE0 + backendSlot);
        const TEXTURE *texture = getBoundTextureForRoleOpenGlEs(pBufferId, subsetIndex, role);
        GLBindTexture(GL_TEXTURE_2D, texture ? texture->getBackendTextureId() : 0);
        if (samplerHandle != -1)
        {
            GLUniform1i(samplerHandle, backendSlot);
        }
    }

    static GLenum getOpenGlEsModeDraw(const uint32_t mode_draw)
    {
        switch (mode_draw)
        {
            case util::MODE_DRAW_POINTS:
            {
                return GL_POINTS;
            }
            break;
            case util::MODE_DRAW_LINES:
            {
                return GL_LINES;
            }
            break;
            case util::MODE_DRAW_LINE_LOOP:
            {
                return GL_LINE_LOOP;
            }
            break;
            case util::MODE_DRAW_LINE_STRIP:
            {
                return GL_LINE_STRIP;
            }
            break;
            case util::MODE_DRAW_TRIANGLES:
            {
                return GL_TRIANGLES;
            }
            break;
            case util::MODE_DRAW_TRIANGLE_STRIP:
            {
                return GL_TRIANGLE_STRIP;
            }
            break;
            case util::MODE_DRAW_TRIANGLE_FAN:
            {
                return GL_TRIANGLE_FAN;
            }
            break;
            default:
            {
                ERROR_AT(__LINE__, __FILE__, "Mode draw for OpenGlEs incorrect!, returning GL_TRIANGLES");
            }
            break;
        }
        return GL_TRIANGLES;
    }

    // Disable vertex attrib arrays not used for this draw so ANGLE (and strict drivers) do not see
    // stale enabled state from another program (e.g. ImGui) and raise GL_INVALID_OPERATION.
    static void disableUnusedVertexAttribs(const GLES_PS_VS* gles, bool useNormal, bool useTexCoord)
    {
        GLint maxAttribs = 0;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttribs);
        const int pos = gles->positionHandle;
        const int norm = useNormal && gles->normalHandle >= 0 ? gles->normalHandle : -1;
        const int tex = useTexCoord && gles->texCoordHandle >= 0 ? gles->texCoordHandle : -1;
        for (GLint a = 0; a < maxAttribs; ++a)
        {
            if (a != pos && a != norm && a != tex)
                glDisableVertexAttribArray(static_cast<GLuint>(a));
        }
    }

    BUFFER_GL::BUFFER_GL():
        indexStartIB(nullptr),
        indexCountIB(nullptr),
        vertexStartVB(nullptr),
        vertexCountVB(nullptr),
        sizeOfArrayVertex(0),
        fvf(FVF_PROVIDE_BY_ENGINE::FVF_POS_UV),
        mode_draw(GL_TRIANGLES),
        mode_cull_face(GL_BACK),
        mode_front_face_direction(GL_CW),
        totalSubset(0),
        initializedIndexBuffer(false)
    {
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
    }

    BUFFER_SPECIFIC::BUFFER_SPECIFIC() noexcept : 
        vboIndexSubsetIB(nullptr),
        vboVertexSubsetVB(nullptr),
        vboNormalSubsetVB(nullptr),
        vboTextureSubsetVB(nullptr)
    {
        memset(vboVertNorTexIB, 0, sizeof(vboVertNorTexIB));
    }

    BUFFER_SPECIFIC::~BUFFER_SPECIFIC()
    {
        this->release();
    }

    void BUFFER_SPECIFIC::release()
    {
        if (vboVertNorTexIB[0])
        {
            GLDeleteBuffers(3, vboVertNorTexIB);
        }
        memset(vboVertNorTexIB, 0, sizeof(vboVertNorTexIB));
        
        if (vboIndexSubsetIB)
            delete[] vboIndexSubsetIB;
        vboIndexSubsetIB = nullptr;

        if (vboVertexSubsetVB)
            delete[] vboVertexSubsetVB;
        vboVertexSubsetVB = nullptr;

        if (vboNormalSubsetVB)
            delete[] vboNormalSubsetVB;
        vboNormalSubsetVB = nullptr;

        if (vboTextureSubsetVB)
            delete[] vboTextureSubsetVB;
        vboTextureSubsetVB = nullptr;
    }

    void BUFFER_GL::release()
    {
        BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
        backendBuffer->release();
        totalSubset   = 0;
    }

    bool BUFFER_GL::loadBuffer(const mbm::VEC3 *vertex, // type vertex buffer
        const mbm::VEC3 *normal,const mbm::VEC2 *uv,const uint32_t sizeOfArrayVertex,
        const uint32_t totalSubsets,const int *vertexStartSubset,const int *vertexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode, const bool isDynamic)
    {
        this->release();
        if (!vertex || !sizeOfArrayVertex || !totalSubsets || !vertexStartSubset || !vertexCountSubset)
            return false;
        const GLenum usage           = isDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
        this->totalSubset            = totalSubsets;
        BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
        backendBuffer->vboVertexSubsetVB  = new uint32_t[totalSubset];
        backendBuffer->vboNormalSubsetVB  = new uint32_t[totalSubset];
        backendBuffer->vboTextureSubsetVB = new uint32_t[totalSubset];
        this->initializeVertexBufferControl(totalSubsets, sizeOfArrayVertex, vertexStartSubset, vertexCountSubset, info_draw_mode);
        this->fvf = (normal && uv) ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV : (normal ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR : (uv ? FVF_PROVIDE_BY_ENGINE::FVF_POS_UV : FVF_PROVIDE_BY_ENGINE::FVF_POS));
        memset(backendBuffer->vboVertexSubsetVB, 0, sizeof(uint32_t) *  static_cast<size_t>(totalSubset));
        memset(backendBuffer->vboNormalSubsetVB, 0, sizeof(uint32_t) *  static_cast<size_t>(totalSubset));
        memset(backendBuffer->vboTextureSubsetVB, 0, sizeof(uint32_t) * static_cast<size_t>(totalSubset));
        GLGenBuffers(static_cast<GLsizei>(this->totalSubset), backendBuffer->vboVertexSubsetVB);
        if (!backendBuffer->vboVertexSubsetVB[0])
        {
            this->release();
            return false;
        }

        if (normal)
        {
            GLGenBuffers(static_cast<GLsizei>(this->totalSubset), backendBuffer->vboNormalSubsetVB);
        }

        if (uv)
        {
            GLGenBuffers(static_cast<GLsizei>(this->totalSubset), backendBuffer->vboTextureSubsetVB);
        }
        for (uint32_t i = 0; i < totalSubset; ++i)
        {
            GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboVertexSubsetVB[i]);
            GLBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(mbm::VEC3) *  static_cast<size_t>(this->vertexCountVB[i])), &vertex[this->vertexStartVB[i]], usage);
            if (normal)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboNormalSubsetVB[i]);
                GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeof(mbm::VEC3) * static_cast<size_t>(this->vertexCountVB[i])), &normal[this->vertexStartVB[i]],usage);
            }
            if (uv)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboTextureSubsetVB[i]);
                GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeof(mbm::VEC2) * static_cast<size_t>(this->vertexCountVB[i])), &uv[this->vertexStartVB[i]],usage);
            }
        }
        GLBindBuffer(GL_ARRAY_BUFFER, 0);

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
        BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
        GLGenBuffers(3, backendBuffer->vboVertNorTexIB);
        if (backendBuffer->vboVertNorTexIB[0] == 0)
            return false;
        this->totalSubset      = totalSubsets;
        backendBuffer->vboIndexSubsetIB = new uint32_t[totalSubset];
        this->initializeIndexBufferControl(totalSubsets, sizeOfArrayVertex, indexStartSubset, indexCountSubset, info_draw_mode);
        this->fvf = (normal && uv) ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV : (normal ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR : (uv ? FVF_PROVIDE_BY_ENGINE::FVF_POS_UV : FVF_PROVIDE_BY_ENGINE::FVF_POS));
        memset(backendBuffer->vboIndexSubsetIB, 0, sizeof(uint32_t) * static_cast<size_t>(totalSubset));
        GLGenBuffers(static_cast<GLsizei>(this->totalSubset), backendBuffer->vboIndexSubsetIB);
        if (!backendBuffer->vboIndexSubsetIB[0])
        {
            this->release();
            return false;
        }

        GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboVertNorTexIB[0]);
        GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeOfArrayVertex * sizeof(mbm::VEC3)), vertex, GL_STATIC_DRAW);

        if (normal)
        {
            GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboVertNorTexIB[1]);
            GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeOfArrayVertex * sizeof(mbm::VEC3)), normal, GL_STATIC_DRAW);
        }

        if (uv)
        {
            GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboVertNorTexIB[2]);
            GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeOfArrayVertex * sizeof(mbm::VEC2)), uv, GL_STATIC_DRAW);
        }

        for (uint32_t i = 0; i < this->totalSubset; ++i)
        {
            GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, backendBuffer->vboIndexSubsetIB[i]);
            GLBufferData(GL_ELEMENT_ARRAY_BUFFER,static_cast<GLsizeiptr>(sizeof(unsigned short) * static_cast<size_t>(this->indexCountIB[i])),&arrayIndices[this->indexStartIB[i]], GL_STATIC_DRAW);
        }

        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        return true;
    }

    bool BUFFER_GL::loadBufferDynamic(const uint16_t *arrayIndices, 
                                      const unsigned int totalSubsets, 
                                      const int *indexStartSubset,
                                      const int *indexCountSubset,
                                      const bool hasNormal,
                                      const bool hasUv,
                                      const util::INFO_DRAW_MODE * info_draw_mode)
    {
        release();
        if (!arrayIndices || !totalSubsets || !indexStartSubset || !indexCountSubset)
            return false;
        BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
        this->totalSubset      = totalSubsets;
        backendBuffer->vboIndexSubsetIB = new uint32_t[totalSubset];
        memset(backendBuffer->vboIndexSubsetIB, 0, sizeof(uint32_t) * totalSubset);
        this->initializeIndexBufferControl(totalSubsets, sizeOfArrayVertex, indexStartSubset, indexCountSubset, info_draw_mode);
        this->fvf = (hasNormal && hasUv) ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV : (hasNormal ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR : (hasUv ? FVF_PROVIDE_BY_ENGINE::FVF_POS_UV : FVF_PROVIDE_BY_ENGINE::FVF_POS));
        GLGenBuffers(static_cast<GLsizei>(this->totalSubset), backendBuffer->vboIndexSubsetIB);
        if (!backendBuffer->vboIndexSubsetIB[0])
        {
            this->release();
            return false;
        }

        for (uint32_t i = 0; i < this->totalSubset; ++i)
        {
            GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, backendBuffer->vboIndexSubsetIB[i]);
            GLBufferData(GL_ELEMENT_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeof(uint16_t) * static_cast<size_t>(this->indexCountIB[i])),&arrayIndices[this->indexStartIB[i]], GL_STATIC_DRAW);
        }

        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        return true;
    }

    bool BUFFER_GL::updateDynamic(const VEC3* vertex,
                                  const VEC3* normal,
                                  const VEC2* uv,
                                  const int* vertexStartSubset,
                                  const int* vertexCountSubset)// update when dynamic
    {
        for (uint32_t i = 0; i < this->totalSubset; ++i)
        {
            BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
            const uint32_t vertexStart = vertexStartSubset[i];
            const uint32_t vertexCount = vertexCountSubset[i];
            if (vertexCount > this->sizeOfArrayVertex)
            {
                return false;
            }
            if ((vertexStart + vertexCount) > this->sizeOfArrayVertex)
            {
                return false;
            }
            const mbm::VEC3* pVertexStart = &vertex[vertexStart];
            const mbm::VEC3* pNormalStart = normal ? &normal[vertexStart] : nullptr;
            const mbm::VEC2* pUvStart     = uv ? &uv[vertexStart]         : nullptr;
            if (this->initializedIndexBuffer)
            {
                // TODO: for index buffer
                ERROR_AT(__LINE__, __FILE__, "TODO: Update vertex not implemented for index buffer");
                return false;
            }
            else
            {
                GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboVertexSubsetVB[i]);
                GLBufferData(GL_ARRAY_BUFFER, sizeof(mbm::VEC3) * vertexCount, pVertexStart, GL_DYNAMIC_DRAW);
                if (pNormalStart && backendBuffer->vboNormalSubsetVB[i] != 0)
                {
                    GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboNormalSubsetVB[i]);
                    GLBufferData(GL_ARRAY_BUFFER, sizeof(mbm::VEC3) * vertexCount, pNormalStart, GL_DYNAMIC_DRAW);
                }
                if (pUvStart && backendBuffer->vboTextureSubsetVB[i] != 0)
                {
                    GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboTextureSubsetVB[i]);
                    GLBufferData(GL_ARRAY_BUFFER, sizeof(mbm::VEC2) * vertexCount, pUvStart, GL_DYNAMIC_DRAW);
                }
            }
        }
        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        return true;
    }

    bool BUFFER_GL::loadParticleBuffer()// type index buffer only, must be implemented by specific backend engine
    {
        release();
        const uint16_t arrayIndices[6] = { 0, 1, 2, 2, 1, 3 };
        constexpr GLsizeiptr sizeIndexBuffer = sizeof(arrayIndices);
        constexpr int indexStartSubset = 0;
        constexpr int indexCountSubset = sizeof(arrayIndices) / sizeof(uint16_t);
        constexpr uint32_t sizeOfArrayVertex = 0;

        this->totalSubset = 1;
        BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
        backendBuffer->vboIndexSubsetIB = new uint32_t[this->totalSubset];

        this->initializeIndexBufferControl(this->totalSubset, sizeOfArrayVertex, &indexStartSubset, &indexCountSubset, nullptr);
        memset(backendBuffer->vboIndexSubsetIB, 0, sizeof(uint32_t) * static_cast<size_t>(totalSubset));
        GLGenBuffers(static_cast<GLsizei>(this->totalSubset), backendBuffer->vboIndexSubsetIB);
        if (!backendBuffer->vboIndexSubsetIB[0])
        {
            this->release();
            return false;
        }

        for (uint32_t i = 0; i < this->totalSubset; ++i)
        {
            GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, backendBuffer->vboIndexSubsetIB[i]);
            GLBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeIndexBuffer, arrayIndices, GL_STATIC_DRAW);
        }

        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
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
            const GLES_PS_VS* gles_shaderSpecific = static_cast<const GLES_PS_VS*>(ptrShaderSpecific);
            auto var       = new VAR_SHADER(std::string(nameVar), typeVar, isPS);
            int32_t *handleVar = static_cast<int32_t*>(var->ptrHandleVar);
            *handleVar = GLGetUniformLocation(gles_shaderSpecific->programObject, nameVar);
            if (*handleVar == -1)
            {
#if defined _DEBUG
                PRINT_IF_DEBUG("wasn't found: '%s' into shader GLES! \"", nameVar);
#endif
                delete var;
                return false;
            }
            switch (typeVar)
            {
                case VAR_FLOAT:
                case VAR_INT: { var->current[0] = defaultValue[0];
                }
                break;
                case VAR_VECTOR2:
                {
                    for (uint32_t i = 0; i < 2; ++i)
                    {
                        var->current[i] = defaultValue[i];
                    }
                }
                break;
                case VAR_COLOR_RGB:
                case VAR_VECTOR:
                {
                    for (uint32_t i = 0; i < 3; ++i)
                    {
                        var->current[i] = defaultValue[i];
                    }
                }
                break;
                case VAR_COLOR_RGBA:
                {
                    for (uint32_t i = 0; i < 4; ++i)
                    {
                        var->current[i] = defaultValue[i];
                    }
                }
                break;
                default:
                {
                    delete var;
                    return false;
                }
            }
            lsVar.push_back(var);
            return true;
        }
        return false;
    }

    void BASE_SHADER::update(void* ptrShaderSpecific) const
    {
        const GLES_PS_VS* gles_shaderSpecific = static_cast<const GLES_PS_VS*>(ptrShaderSpecific);
        if (gles_shaderSpecific->programObject == 0)
            return;
        GLUseProgram(gles_shaderSpecific->programObject);
        const std::vector<VAR_SHADER *>::size_type s = lsVar.size();
        for (std::vector<VAR_SHADER *>::size_type i = 0; i < s; ++i)
        {
            VAR_SHADER *var = lsVar[i];
            if (var)
            {
                const int32_t handleVar = *static_cast<int32_t*>(var->ptrHandleVar);
                switch (var->typeVar)
                {
                    // Uniform
                    case VAR_FLOAT: { GLUniform1f(handleVar, var->current[0]);
                    }
                    break;
                    case VAR_INT: { GLUniform1i(handleVar, var->getCurrentInt());
                    }
                    break;
                    case VAR_VECTOR2: { GLUniform2f(handleVar, var->current[0], var->current[1]);
                    }
                    break;
                    case VAR_COLOR_RGB:
                    case VAR_VECTOR: { GLUniform3f(handleVar, var->current[0], var->current[1], var->current[2]);
                    }
                    break;
                    case VAR_COLOR_RGBA:
                    {
                        GLUniform4f(handleVar, var->current[0], var->current[1], var->current[2], var->current[3]);
                    }
                    break;
                    default: {
                    }
                    break;
                }
            }
        }
    }

    GLES_PS_VS::GLES_PS_VS() noexcept
        : positionHandle(-1),
          texCoordHandle(-1),
          normalHandle(-1),
          mvpMatrixHandle(-1),
          mvMatrixHandle(-1),
          samplerHandle0(-1),
          samplerHandle1(-1),
          samplerHandle2(-1),
          programObject(0)
    {
	}

    GLES_PS_VS::~GLES_PS_VS()
    {
        release();
    }

    void GLES_PS_VS::release() noexcept
    {
        positionHandle  = -1;
        texCoordHandle  = -1;
        normalHandle    = -1;
        mvpMatrixHandle = -1;
        mvMatrixHandle  = -1;
        samplerHandle0  = -1;
        samplerHandle1  = -1;
        samplerHandle2  = -1;
        if (programObject)
        {
            GLDeleteProgram(programObject);
        }
        programObject  = 0;
    }

    SHADER::SHADER() :
        pShader(nullptr),
        vShader(nullptr)
    {
        setBackendShaderSpecific(new GLES_PS_VS());
    }

    SHADER::~SHADER()
    {
        void *backendShaderSpecific = getBackendShaderSpecific();
        // Deleting a void* pointer directly in C++ is undefined behavior and should be avoided. 
        delete static_cast<GLES_PS_VS*>(backendShaderSpecific);
        setBackendShaderSpecific(nullptr);
    }

    void SHADER::onRestore() // Libera o pShader da memória e pode ser carregado novamente
    {
        void *backendShaderSpecific = getBackendShaderSpecific();
        static_cast<GLES_PS_VS*>(backendShaderSpecific)->release();//TODO: check this: maybe only attribute 0 is enough
        this->pShader            = nullptr;
        this->vShader            = nullptr;
    }

    void SHADER::releaseShader()
    {
        void *backendShaderSpecific = getBackendShaderSpecific();
        static_cast<GLES_PS_VS*>(backendShaderSpecific)->release();
        this->pShader            = nullptr;
        this->vShader            = nullptr;
    }

    bool SHADER::isLoad() const noexcept
    {
        void *backendShaderSpecific = getBackendShaderSpecific();
        return static_cast<const GLES_PS_VS*>(backendShaderSpecific)->programObject != 0;
    }

    bool SHADER::compileShader(mbm::BASE_SHADER *ptrPshader, mbm::BASE_SHADER *ptrVshader, mbm::FVF_PROVIDE_BY_ENGINE fvf)
    {
        if (fvf == FVF_PROVIDE_BY_ENGINE::FVF_NONE)
            return false;
        this->pShader             = ptrPshader;
        this->vShader            = ptrVshader;
        const bool hasNormal = (fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR || fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
        const bool hasUV = (fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_UV || fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
        const bool useReservedLightScaffolding = this->shouldCompileReservedLightDefault();
        const bool canUsePointLight2D = useReservedLightScaffolding && (this->vShader == nullptr);
        const std::string supportedMaxLights = std::to_string(DEFAULT_SUPPORTED_MAX_LIGHTS);
        const char *textureDiffuseName =
            getTextureRoleShaderName(TEXTURE_ROLE_DIFFUSE, SHADER_TEXTURE_NAMING_SEMANTIC_ROLE);
        const char *textureNormalName =
            getTextureRoleShaderName(TEXTURE_ROLE_NORMAL, SHADER_TEXTURE_NAMING_SEMANTIC_ROLE);

        std::string defaultCodePs;
        if (hasUV)
        {
            defaultCodePs = "precision mediump float;"
                "varying vec2 vTexCoord;"
                "uniform sampler2D ";
            defaultCodePs += textureDiffuseName;
            defaultCodePs += ";";
            if (useReservedLightScaffolding == false)
            {
                defaultCodePs += "void main() { gl_FragColor = texture2D(";
                defaultCodePs += textureDiffuseName;
                defaultCodePs += ", vTexCoord); }";
            }
            else
            {
                defaultCodePs += "uniform int LightEnabled;"
                    "uniform vec4 AmbientColor;"
                    "uniform vec3 LightDirectionView;"
                    "uniform vec4 MaterialSpecular;"
                    "uniform vec4 MaterialDiffuse;"
                    "uniform vec4 MaterialAmbient;"
                    "uniform vec4 MaterialEmissive;"
                    "uniform float MaterialPower;";
                if (canUsePointLight2D == false)
                {
                    defaultCodePs += "uniform vec4 LightColor;";
                }
                if (canUsePointLight2D)
                {
                    defaultCodePs += "varying vec3 vPositionView;"
                        "uniform sampler2D ";
                    defaultCodePs += textureNormalName;
                    defaultCodePs += ";"
                        "uniform int LightMode;"
                        "uniform int HasNormalMap;"
                        "uniform vec3 LightPositionView[";
                    defaultCodePs += supportedMaxLights;
                    defaultCodePs += "];"
                        "uniform float LightRadius[";
                    defaultCodePs += supportedMaxLights;
                    defaultCodePs += "];"
                        "uniform vec4 LightColor[";
                    defaultCodePs += supportedMaxLights;
                    defaultCodePs += "];"
                        "uniform int LightCount;";
                }
                if (hasNormal)
                    defaultCodePs += "varying vec3 vNormalView;";
                defaultCodePs += "void main() {"
                    " vec4 texColor = texture2D(";
                defaultCodePs += textureDiffuseName;
                defaultCodePs += ", vTexCoord);"
                    " if (LightEnabled == 0) { gl_FragColor = texColor; return; }"
                    " vec3 base = texColor.rgb * MaterialDiffuse.rgb;"
                    " vec3 light = AmbientColor.rgb * MaterialAmbient.rgb;"
                    " vec3 specular = vec3(0.0);";
                if (canUsePointLight2D == false)
                {
                    if (hasNormal)
                    {
                        defaultCodePs += " vec3 normalView = normalize(vNormalView);"
                            " vec3 viewDir = normalize(-vPositionView);"
                            " vec3 lightTravel = normalize(LightDirectionView);"
                            " float diffuse = max(dot(normalView, -lightTravel), 0.0);"
                            " light += LightColor[0].rgb * diffuse;"
                            " if (diffuse > 0.0 && MaterialPower > 0.0) {"
                            "  vec3 lightDir = normalize(-lightTravel);"
                            "  vec3 halfDir = normalize(lightDir + viewDir);"
                            "  float spec = pow(max(dot(normalView, halfDir), 0.0), MaterialPower);"
                            "  specular += LightColor[0].rgb * MaterialSpecular.rgb * spec;"
                            " }";
                    }
                    defaultCodePs += " vec3 litColor = clamp((base * clamp(light, 0.0, 1.0)) + MaterialEmissive.rgb + specular, 0.0, 1.0);"
                        " gl_FragColor = vec4(litColor, texColor.a * MaterialDiffuse.a);"
                        "}";
                }
                else if (hasNormal)
                {
                    defaultCodePs += " if (LightMode == 0) { gl_FragColor = texColor; return; }"
                        " if (LightMode == 1) {"
                        "  vec3 normalView = normalize(vNormalView);"
                        "  vec3 viewDir = normalize(-vPositionView);"
                        "  vec3 lightTravel = normalize(LightDirectionView);"
                        "  float diffuse = max(dot(normalView, -lightTravel), 0.0);"
                        "  light += LightColor[0].rgb * diffuse;"
                        "  if (diffuse > 0.0 && MaterialPower > 0.0) {"
                        "   vec3 lightDir = normalize(-lightTravel);"
                        "   vec3 halfDir = normalize(lightDir + viewDir);"
                        "   float spec = pow(max(dot(normalView, halfDir), 0.0), MaterialPower);"
                        "   specular += LightColor[0].rgb * MaterialSpecular.rgb * spec;"
                        "  }"
                        " } else ";
                }
                else
                {
                    defaultCodePs += " if (LightMode == 0) { gl_FragColor = texColor; return; }"
                        " if (LightMode == 2) ";
                }
                if (canUsePointLight2D)
                {
                    defaultCodePs += "{"
                        "  vec3 normalView = vec3(0.0, 0.0, 1.0);"
                        "  if (HasNormalMap != 0) normalView = normalize((texture2D(";
                    defaultCodePs += textureNormalName;
                    defaultCodePs += ", vTexCoord).xyz * 2.0) - 1.0);"
                        "  for (int i = 0; i < ";
                    defaultCodePs += supportedMaxLights;
                    defaultCodePs += "; ++i) {"
                        "   if (i >= LightCount) break;"
                        "   vec3 toLight = LightPositionView[i] - vPositionView;"
                        "   float dist = length(toLight);"
                        "   if (LightRadius[i] > 0.0001) {"
                        "    vec3 lightDir = toLight / max(dist, 0.0001);"
                        "    float diffuse = max(dot(normalView, lightDir), 0.0);"
                        "    float attenuation = 1.0 - clamp(dist / LightRadius[i], 0.0, 1.0);"
                        "    attenuation *= attenuation;"
                        "    light += LightColor[i].rgb * diffuse * attenuation;"
                        "    if (diffuse > 0.0 && MaterialPower > 0.0) {"
                        "     vec3 viewDir = normalize(-vPositionView);"
                        "     vec3 halfDir = normalize(lightDir + viewDir);"
                        "     float spec = pow(max(dot(normalView, halfDir), 0.0), MaterialPower);"
                        "     specular += LightColor[i].rgb * MaterialSpecular.rgb * spec * attenuation;"
                        "    }"
                        "   }"
                        "  }"
                        " }"
                        " vec3 litColor = clamp((base * clamp(light, 0.0, 1.0)) + MaterialEmissive.rgb + specular, 0.0, 1.0);"
                        " gl_FragColor = vec4(litColor, texColor.a * MaterialDiffuse.a);"
                        "}";
                }
            }
        }
        else
        {
            defaultCodePs = "precision mediump float;";
            if (hasNormal && useReservedLightScaffolding)
            {
                defaultCodePs += "varying vec3 vNormalView;"
                    "uniform int LightEnabled;"
                    "uniform int LightMode;"
                    "uniform vec4 AmbientColor;"
                    "uniform vec3 LightDirectionView;"
                    "uniform vec4 LightColor;"
                    "uniform vec4 MaterialSpecular;"
                    "uniform vec4 MaterialDiffuse;"
                    "uniform vec4 MaterialAmbient;"
                    "uniform vec4 MaterialEmissive;"
                    "uniform float MaterialPower;"
                    "varying vec3 vPositionView;"
                    "void main() {"
                    " vec4 baseColor = vec4(1.0, 1.0, 1.0, 1.0);"
                    " if (LightEnabled == 0 || LightMode != 1) { gl_FragColor = baseColor; return; }"
                    " vec3 normalView = normalize(vNormalView);"
                    " vec3 viewDir = normalize(-vPositionView);"
                    " vec3 lightTravel = normalize(LightDirectionView);"
                    " float diffuse = max(dot(normalView, -lightTravel), 0.0);"
                    " vec3 base = MaterialDiffuse.rgb;"
                    " vec3 light = clamp((AmbientColor.rgb * MaterialAmbient.rgb) + (LightColor.rgb * diffuse), 0.0, 1.0);"
                    " vec3 specular = vec3(0.0);"
                    " if (diffuse > 0.0 && MaterialPower > 0.0) {"
                    "  vec3 lightDir = normalize(-lightTravel);"
                    "  vec3 halfDir = normalize(lightDir + viewDir);"
                    "  float spec = pow(max(dot(normalView, halfDir), 0.0), MaterialPower);"
                    "  specular = LightColor.rgb * MaterialSpecular.rgb * spec;"
                    " }"
                    " vec3 litColor = clamp((base * light) + MaterialEmissive.rgb + specular, 0.0, 1.0);"
                    " gl_FragColor = vec4(litColor, MaterialDiffuse.a);"
                    "}";
            }
            else
            {
                defaultCodePs += "void main() { gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0); }";
            }
        }

        std::string defaultCodeVs = "attribute vec4 aPosition;";
        if (hasNormal) defaultCodeVs += " attribute vec3 aNormal;";
        if (hasUV) defaultCodeVs += " attribute vec2 aTextCoord;";
        defaultCodeVs += " uniform mat4 mvpMatrix;";
        if ((hasNormal && useReservedLightScaffolding) || (hasUV && useReservedLightScaffolding)) defaultCodeVs += " uniform mat4 mvMatrix;";
        if (hasNormal && useReservedLightScaffolding) defaultCodeVs += " varying vec3 vNormalView;";
        if (hasUV) defaultCodeVs += " varying vec2 vTexCoord;";
        if (useReservedLightScaffolding && (hasNormal || hasUV)) defaultCodeVs += " varying vec3 vPositionView;";
        defaultCodeVs += " void main() { gl_Position = mvpMatrix * aPosition;";
        if (hasNormal && useReservedLightScaffolding) defaultCodeVs += " vNormalView = (mvMatrix * vec4(aNormal, 0.0)).xyz;";
        if (useReservedLightScaffolding && (hasNormal || hasUV)) defaultCodeVs += " vPositionView = (mvMatrix * aPosition).xyz;";
        if (hasUV) defaultCodeVs += " vTexCoord = aTextCoord;";
        defaultCodeVs += " }";
        void *backendShaderSpecific = getBackendShaderSpecific();
        GLES_PS_VS* gles_shaderSpecific = static_cast<GLES_PS_VS*>(backendShaderSpecific);
        if (gles_shaderSpecific->programObject)
        {
            PRINT_IF_DEBUG("programObject already has a value [%d]", gles_shaderSpecific->programObject);
            return true;
        }
        if (this->pShader == nullptr && this->vShader == nullptr)
        {
            if (!loadShaderProgram(this->pShader, this->vShader, backendShaderSpecific, defaultCodeVs.c_str(), defaultCodePs.c_str()))
                return false;
        }
        else if (this->pShader == nullptr && this->vShader)
        {
            if (!loadShaderProgram(this->pShader, this->vShader, backendShaderSpecific, this->vShader->getCode(), defaultCodePs.c_str()))
                return false;
        }
        else if (this->pShader && this->vShader == nullptr)
        {
            if (!loadShaderProgram(this->pShader, this->vShader, backendShaderSpecific, defaultCodeVs.c_str(), this->pShader->getCode()))
                return false;
        }
        else if (this->pShader && this->vShader)
        {
            if (!loadShaderProgram(this->pShader, this->vShader, backendShaderSpecific, this->vShader->getCode(), this->pShader->getCode()))
                return false;
        }

        // In OpenGL, a uniform location of - 1 means "not found", 
        // but a handle of 0 suggests GLGetUniformLocation() isn't finding the uniform in your shader.

        const std::string vertexShaderCode(this->vShader ? this->vShader->getCode() : defaultCodeVs);
        const std::string pixelShaderCode(this->pShader ? this->pShader->getCode() : defaultCodePs);
        const std::string bothShaderCode(pixelShaderCode + vertexShaderCode);
        const SHADER_TEXTURE_NAMING textureNaming =
            detectShaderTextureNamingProfile(bothShaderCode.c_str());
        if (textureNaming == SHADER_TEXTURE_NAMING_MIXED_INVALID)
        {
            ERROR_LOG("OpenGL ES shader mixes legacy texture names with semantic texture roles");
            return false;
        }
        if (textureNaming == SHADER_TEXTURE_NAMING_SEMANTIC_ROLE &&
            (shaderCodeDeclaresTextureRole(bothShaderCode.c_str(), TEXTURE_ROLE_SPECULAR, textureNaming) ||
             shaderCodeDeclaresTextureRole(bothShaderCode.c_str(), TEXTURE_ROLE_EMISSIVE, textureNaming) ||
             shaderCodeDeclaresTextureRole(bothShaderCode.c_str(), TEXTURE_ROLE_MASK, textureNaming)))
        {
            ERROR_LOG("OpenGL ES shader declares a reserved semantic texture role without runtime binding support");
            return false;
        }

        if (bothShaderCode.find("aPosition") != std::string::npos)
        {
            gles_shaderSpecific->positionHandle = GLGetAttribLocation(gles_shaderSpecific->programObject, "aPosition");
        }
        if (bothShaderCode.find("mvpMatrix") != std::string::npos)
        {
            gles_shaderSpecific->mvpMatrixHandle = GLGetUniformLocationOptional(gles_shaderSpecific->programObject, "mvpMatrix");
        }
        if (bothShaderCode.find("mvMatrix") != std::string::npos)
        {
            gles_shaderSpecific->mvMatrixHandle = GLGetUniformLocationOptional(gles_shaderSpecific->programObject, "mvMatrix");
        }
        if (vertexShaderCode.find("aNormal") != std::string::npos)
        {   // Attributes are vertex-only; use Optional - aNormal can be inactive if linker optimizes out unused varying
            gles_shaderSpecific->normalHandle = GLGetAttribLocationOptional(gles_shaderSpecific->programObject, "aNormal");
        }
        else
        {
            gles_shaderSpecific->normalHandle = -1;
        }
        if (bothShaderCode.find("aTextCoord") != std::string::npos)
        {
            gles_shaderSpecific->texCoordHandle = GLGetAttribLocation(gles_shaderSpecific->programObject, "aTextCoord");
        }
        else
        {
            gles_shaderSpecific->texCoordHandle = -1;
        }
        if (shaderCodeDeclaresTextureRole(bothShaderCode.c_str(), TEXTURE_ROLE_DIFFUSE, textureNaming))
        {
            gles_shaderSpecific->samplerHandle0 = GLGetUniformLocation(
                gles_shaderSpecific->programObject,
                getTextureRoleShaderName(TEXTURE_ROLE_DIFFUSE, textureNaming));
        }
        if (shaderCodeDeclaresTextureRole(bothShaderCode.c_str(), TEXTURE_ROLE_ANIMATION_EFFECT, textureNaming))
        {
            gles_shaderSpecific->samplerHandle1 = GLGetUniformLocation(
                gles_shaderSpecific->programObject,
                getTextureRoleShaderName(TEXTURE_ROLE_ANIMATION_EFFECT, textureNaming));
        }
        if (shaderCodeDeclaresTextureRole(bothShaderCode.c_str(), TEXTURE_ROLE_NORMAL, textureNaming))
        {
            gles_shaderSpecific->samplerHandle2 = GLGetUniformLocation(
                gles_shaderSpecific->programObject,
                getTextureRoleShaderName(TEXTURE_ROLE_NORMAL, textureNaming));
        }
        
        return true;
    }


    bool SHADER::render(const BUFFER_GL *pBufferId, const RENDERIZABLE *renderizableOwner) const
    {
        const ScopedRenderizableContext scopedRenderizableContext(renderizableOwner);
        void *backendShaderSpecific = getBackendShaderSpecific();
        const GLES_PS_VS* gles_shaderSpecific = static_cast<const GLES_PS_VS*>(backendShaderSpecific);
        GLCullFace(pBufferId->mode_cull_face);//GL_FRONT 1028, GL_BACK 1029, GL_FRONT_AND_BACK 1032(CullFaceMode)
        GLFrontFace(pBufferId->mode_front_face_direction);//GL_CCW 2305 , GL_CW 2304(FrontFaceDirection)
        const GLenum modeDrawGl       = getOpenGlEsModeDraw(pBufferId->mode_draw);
        BUFFER_SPECIFIC *backendBuffer = pBufferId->getBackendBuffer();
        if (!backendBuffer)
            return false;
        
        if (pBufferId->isIndexBuffer()) // Index buffer
        {
            if (!backendBuffer->vboVertNorTexIB[0])
                return false;
            GLUseProgram(gles_shaderSpecific->programObject);
            //-----------------------------------------------------------------------------------------------------------
            GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboVertNorTexIB[0]);
            GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
            GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            //-----------------------------------------------------------------------------------------------------------
            if (gles_shaderSpecific->normalHandle != -1) // Normal (nem sempre temos normal nos shaders)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboVertNorTexIB[1]);
                GLEnableVertexAttribArray(gles_shaderSpecific->normalHandle);
                GLVertexAttribPointer(gles_shaderSpecific->normalHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            }
            //-----------------------------------------------------------------------------------------------------------
            if (gles_shaderSpecific->texCoordHandle != -1)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboVertNorTexIB[2]);
                GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
            }
            //-----------------------------------------------------------------------------------------------------------
            if (gles_shaderSpecific->mvpMatrixHandle != -1)
            {
                GLUniformMatrix4fv(gles_shaderSpecific->mvpMatrixHandle, 1, GL_FALSE, mvpMatrix.p);
            }
            if (gles_shaderSpecific->mvMatrixHandle != -1)
            {
                GLUniformMatrix4fv(gles_shaderSpecific->mvMatrixHandle, 1, GL_FALSE, modelView.p);
            }
            //-----------------------------------------------------------------------------------------------------------
            // TextureAnimationEffect is one shared texture per animation, not per-subset (see
            // BUFFER_GL::setTextureByStage/getTextureByStage callers) - bind it once here, outside
            // the per-subset loop, instead of redundantly rebinding the same texture every subset.
            bindTextureRoleOpenGlEs(pBufferId, 0, TEXTURE_ROLE_ANIMATION_EFFECT, gles_shaderSpecific->samplerHandle1);
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                // if(pBufferId->hasColorKeying[i])
                //  glEnable(GL_BLEND);
                bindTextureRoleOpenGlEs(pBufferId, i, TEXTURE_ROLE_DIFFUSE, gles_shaderSpecific->samplerHandle0);
                GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, backendBuffer->vboIndexSubsetIB[i]);
                bindTextureRoleOpenGlEs(pBufferId, i, TEXTURE_ROLE_NORMAL, gles_shaderSpecific->samplerHandle2);
                uploadReservedLightUniformsOpenGlEs(gles_shaderSpecific->programObject, pBufferId, i);
                disableUnusedVertexAttribs(gles_shaderSpecific, gles_shaderSpecific->normalHandle != -1, gles_shaderSpecific->texCoordHandle != -1);
                GLDrawElements(modeDrawGl, pBufferId->indexCountIB[i], GL_UNSIGNED_SHORT, nullptr);
            }
        }
        else // Vertex buffer
        {
            if (!backendBuffer->vboVertexSubsetVB)
                return false;
            GLUseProgram(gles_shaderSpecific->programObject);
            // See the index-buffer branch above - shared, not per-subset.
            bindTextureRoleOpenGlEs(pBufferId, 0, TEXTURE_ROLE_ANIMATION_EFFECT, gles_shaderSpecific->samplerHandle1);
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboVertexSubsetVB[i]);
                GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
                //-----------------------------------------------------------------------------------------------------------
                if (gles_shaderSpecific->normalHandle != -1 && backendBuffer->vboNormalSubsetVB && backendBuffer->vboNormalSubsetVB[i] != 0)
                {
                    GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboNormalSubsetVB[i]);
                    GLEnableVertexAttribArray(gles_shaderSpecific->normalHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->normalHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
                }
                //-----------------------------------------------------------------------------------------------------------
                if (gles_shaderSpecific->texCoordHandle != -1 && backendBuffer->vboTextureSubsetVB && backendBuffer->vboTextureSubsetVB[i] != 0)
                {
                    GLBindBuffer(GL_ARRAY_BUFFER, backendBuffer->vboTextureSubsetVB[i]);
                    GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
                }
                //-----------------------------------------------------------------------------------------------------------
                if (gles_shaderSpecific->mvpMatrixHandle != -1)
                {
                    GLUniformMatrix4fv(gles_shaderSpecific->mvpMatrixHandle, 1, GL_FALSE, mvpMatrix.p);
                }
                if (gles_shaderSpecific->mvMatrixHandle != -1)
                {
                    GLUniformMatrix4fv(gles_shaderSpecific->mvMatrixHandle, 1, GL_FALSE, modelView.p);
                }
                //-----------------------------------------------------------------------------------------------------------
                // if(pBufferId->hasColorKeying[i])
                //  glEnable(GL_BLEND);
                bindTextureRoleOpenGlEs(pBufferId, i, TEXTURE_ROLE_DIFFUSE, gles_shaderSpecific->samplerHandle0);
                bindTextureRoleOpenGlEs(pBufferId, i, TEXTURE_ROLE_NORMAL, gles_shaderSpecific->samplerHandle2);

                const bool useNormal = (gles_shaderSpecific->normalHandle != -1)
                    && backendBuffer->vboNormalSubsetVB && backendBuffer->vboNormalSubsetVB[i] != 0;
                const bool useTexCoord = (gles_shaderSpecific->texCoordHandle != -1)
                    && backendBuffer->vboTextureSubsetVB && backendBuffer->vboTextureSubsetVB[i] != 0;
                uploadReservedLightUniformsOpenGlEs(gles_shaderSpecific->programObject, pBufferId, i);
                disableUnusedVertexAttribs(gles_shaderSpecific, useNormal, useTexCoord);

                GLDrawArrays(modeDrawGl, 0, pBufferId->vertexCountVB[i]);
            }
        }
        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        return true;
    }

    bool SHADER::renderDynamic(const BUFFER_GL *pBufferId,const VEC3 *vertex,const VEC3 *normal,const VEC2 *uv,
                               const RENDERIZABLE *renderizableOwner) const
    {
        const ScopedRenderizableContext scopedRenderizableContext(renderizableOwner);
        void *backendShaderSpecific = getBackendShaderSpecific();
        const GLES_PS_VS* gles_shaderSpecific = static_cast<const GLES_PS_VS*>(backendShaderSpecific);
        GLCullFace(pBufferId->mode_cull_face);//GL_FRONT, GL_BACK, GL_FRONT_AND_BACK (CullFaceMode)
        GLFrontFace(pBufferId->mode_front_face_direction);//GL_CCW, GL_CW (FrontFaceDirection)
        const GLenum modeDrawGl       = getOpenGlEsModeDraw(pBufferId->mode_draw);
        BUFFER_SPECIFIC *backendBuffer = pBufferId->getBackendBuffer();
        if (!backendBuffer)
            return false;

        if (pBufferId->isIndexBuffer()) // Index buffer
        {
            if (!backendBuffer->vboIndexSubsetIB)
                return false;
            GLUseProgram(gles_shaderSpecific->programObject);
            //-----------------------------------------------------------------------------------------------------------
            GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
            GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), vertex);
            //-----------------------------------------------------------------------------------------------------------
            if (gles_shaderSpecific->normalHandle != -1 && normal)
            {
                GLEnableVertexAttribArray(gles_shaderSpecific->normalHandle);
                GLVertexAttribPointer(gles_shaderSpecific->normalHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), normal);
            }
            //-----------------------------------------------------------------------------------------------------------
            GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
            GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2), uv);
            //-----------------------------------------------------------------------------------------------------------
            if (gles_shaderSpecific->mvpMatrixHandle != -1)
            {
                GLUniformMatrix4fv(gles_shaderSpecific->mvpMatrixHandle, 1, GL_FALSE, mvpMatrix.p);
            }
            if (gles_shaderSpecific->mvMatrixHandle != -1)
            {
                GLUniformMatrix4fv(gles_shaderSpecific->mvMatrixHandle, 1, GL_FALSE, modelView.p);
            }
            //-----------------------------------------------------------------------------------------------------------
            // TextureAnimationEffect is shared per-animation, not per-subset - bind once, outside the loop.
            bindTextureRoleOpenGlEs(pBufferId, 0, TEXTURE_ROLE_ANIMATION_EFFECT, gles_shaderSpecific->samplerHandle1);
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                bindTextureRoleOpenGlEs(pBufferId, i, TEXTURE_ROLE_DIFFUSE, gles_shaderSpecific->samplerHandle0);
                GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, backendBuffer->vboIndexSubsetIB[i]);
                bindTextureRoleOpenGlEs(pBufferId, i, TEXTURE_ROLE_NORMAL, gles_shaderSpecific->samplerHandle2);
                uploadReservedLightUniformsOpenGlEs(gles_shaderSpecific->programObject, pBufferId, i);
                disableUnusedVertexAttribs(gles_shaderSpecific, gles_shaderSpecific->normalHandle != -1, gles_shaderSpecific->texCoordHandle != -1);
                GLDrawElements(modeDrawGl, pBufferId->indexCountIB[i], GL_UNSIGNED_SHORT, nullptr);
            }
        }
        else // Vertex buffer
        {
            if (!pBufferId->vertexCountVB)
                return false;
            GLUseProgram(gles_shaderSpecific->programObject);
            // Shared per-animation, not per-subset - bind once, outside the loop.
            bindTextureRoleOpenGlEs(pBufferId, 0, TEXTURE_ROLE_ANIMATION_EFFECT, gles_shaderSpecific->samplerHandle1);
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), vertex);
                //-----------------------------------------------------------------------------------------------------------
                if (gles_shaderSpecific->normalHandle != -1 && normal)
                {
                    GLEnableVertexAttribArray(gles_shaderSpecific->normalHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->normalHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), normal);
                }
                //-----------------------------------------------------------------------------------------------------------
                if (gles_shaderSpecific->texCoordHandle != -1 && uv)
                {
                    GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2), uv);
                }
                //-----------------------------------------------------------------------------------------------------------
                if (gles_shaderSpecific->mvpMatrixHandle != -1)
                {
                    GLUniformMatrix4fv(gles_shaderSpecific->mvpMatrixHandle, 1, GL_FALSE, mvpMatrix.p);
                }
                if (gles_shaderSpecific->mvMatrixHandle != -1)
                {
                    GLUniformMatrix4fv(gles_shaderSpecific->mvMatrixHandle, 1, GL_FALSE, modelView.p);
                }
                //-----------------------------------------------------------------------------------------------------------
                bindTextureRoleOpenGlEs(pBufferId, i, TEXTURE_ROLE_DIFFUSE, gles_shaderSpecific->samplerHandle0);
                bindTextureRoleOpenGlEs(pBufferId, i, TEXTURE_ROLE_NORMAL, gles_shaderSpecific->samplerHandle2);

                const bool useNormal = (gles_shaderSpecific->normalHandle != -1) && (normal != nullptr);
                const bool useTexCoord = (gles_shaderSpecific->texCoordHandle != -1) && (uv != nullptr);
                uploadReservedLightUniformsOpenGlEs(gles_shaderSpecific->programObject, pBufferId, i);
                disableUnusedVertexAttribs(gles_shaderSpecific, useNormal, useTexCoord);

                GLDrawArrays(modeDrawGl, 0, pBufferId->vertexCountVB[i]);
            }
        }
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        return true;
    }

    bool SHADER::renderParticle(const BUFFER_GL* pBufferId, const PARTICLE_CONTROL* particleControl) const
    {
        void *backendShaderSpecific = getBackendShaderSpecific();
        const GLES_PS_VS* gles_shaderSpecific = static_cast<const GLES_PS_VS*>(backendShaderSpecific);
        constexpr uint32_t index_subset = 0;
        BUFFER_SPECIFIC *backendBuffer = pBufferId->getBackendBuffer();
        if (!backendBuffer)
            return false;
        bindTextureRoleOpenGlEs(pBufferId, index_subset, TEXTURE_ROLE_DIFFUSE, gles_shaderSpecific->samplerHandle0);
        const GLenum modeDrawGl      = getOpenGlEsModeDraw(pBufferId->mode_draw);
        if (GL_TRIANGLES != modeDrawGl)
        {
            ERROR_AT(__LINE__, __FILE__, "Mode draw for OpenGlEs renderParticle not supported!");
            return false;
        }
        bindTextureRoleOpenGlEs(
            pBufferId, index_subset, TEXTURE_ROLE_ANIMATION_EFFECT, gles_shaderSpecific->samplerHandle1);

        GLboolean depthTestEnabled = true;
        glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
        GLDisable(GL_DEPTH_TEST);
        // Disable face culling for particle quads – a previous draw (e.g. LINE_MESH)
        // may have set glCullFace(GL_FRONT_AND_BACK) which would cull every triangle.
        const GLboolean cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
        if (cullFaceEnabled)
        {
            GLDisable(GL_CULL_FACE);
        }
        
        if (gles_shaderSpecific->mvpMatrixHandle != -1)
        {
            GLUniformMatrix4fv(gles_shaderSpecific->mvpMatrixHandle, 1, GL_FALSE, SHADER::mvpMatrix.p);
        }
        if (gles_shaderSpecific->mvMatrixHandle != -1)
        {
            GLUniformMatrix4fv(gles_shaderSpecific->mvMatrixHandle, 1, GL_FALSE, SHADER::modelView.p);
        }
        // Unbind VBO so vertex pointers are treated as client-side arrays, and disable
        // stale attrib arrays left by a previous draw (e.g. LINE_MESH position-only shader)
        // to avoid GL_INVALID_OPERATION on strict GLES drivers (ANGLE, Mesa).
        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        disableUnusedVertexAttribs(gles_shaderSpecific, false, gles_shaderSpecific->texCoordHandle >= 0);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, backendBuffer->vboIndexSubsetIB[index_subset]);
        VAR_SHADER* var = this->pShader
            ? this->pShader->getVarByName("color")
            : nullptr;

        const uint32_t totalAlive = particleControl->getTotalAlive();
        const VERTEX_UV* buffer = particleControl->getVertexBuffer();
        if (var)
        {
            const ATT_PARTICLE* particles = particleControl->getAttParticle();
            const int32_t handleVar = *static_cast<int32_t*>(var->ptrHandleVar);
            for (unsigned int i = 0; i < totalAlive; ++i)
            {
                const float* vertex = reinterpret_cast<const float*>(&buffer[i * 4]);
                const ATT_PARTICLE* particle = &particles[i];
                GLUniform4f(handleVar, particle->r, particle->g, particle->b, particle->a);
                GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VERTEX_UV), vertex);
        
                GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VERTEX_UV), &vertex[3]);
        
                GLDrawElements(modeDrawGl, 6, GL_UNSIGNED_SHORT, nullptr);
            }
        }
        else
        {
            
            for (unsigned int i = 0; i < totalAlive; ++i)
            {
                const float* vertex = reinterpret_cast<const float*>(&buffer[i * 4]);
                GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VERTEX_UV), vertex);
        
                GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VERTEX_UV), &vertex[3]);
        
                GLDrawElements(modeDrawGl, 6, GL_UNSIGNED_SHORT, nullptr);
            }
        }
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        if (cullFaceEnabled)
        {
            GLEnable(GL_CULL_FACE);
        }
        if (depthTestEnabled)
        {
            GLEnable(GL_DEPTH_TEST);
        }
        return true;
    }

    bool SHADER::renderParticle(const BUFFER_GL* pBufferId, const FLUID_GROUP* pGroup) const
    {
        void *backendShaderSpecific = getBackendShaderSpecific();
        const GLES_PS_VS* gles_shaderSpecific = static_cast<const GLES_PS_VS*>(backendShaderSpecific);
        constexpr uint32_t index_subset = 0;
        BUFFER_SPECIFIC *backendBuffer = pBufferId->getBackendBuffer();
        if (!backendBuffer)
            return false;
        bindTextureRoleOpenGlEs(pBufferId, index_subset, TEXTURE_ROLE_DIFFUSE, gles_shaderSpecific->samplerHandle0);
        const GLenum modeDrawGl = getOpenGlEsModeDraw(pBufferId->mode_draw);
        if (GL_TRIANGLES != modeDrawGl)
        {
            ERROR_AT(__LINE__, __FILE__, "Mode draw for OpenGlEs renderParticle not supported!");
            return false;
        }
        bindTextureRoleOpenGlEs(
            pBufferId, index_subset, TEXTURE_ROLE_ANIMATION_EFFECT, gles_shaderSpecific->samplerHandle1);

        GLboolean depthTestEnabled = true;
        glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
        GLDisable(GL_DEPTH_TEST);
        // Disable face culling for particle quads – a previous draw (e.g. LINE_MESH)
        // may have set glCullFace(GL_FRONT_AND_BACK) which would cull every triangle.
        const GLboolean cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
        if (cullFaceEnabled)
        {
            GLDisable(GL_CULL_FACE);
        }

        if (gles_shaderSpecific->mvpMatrixHandle != -1)
        {
            GLUniformMatrix4fv(gles_shaderSpecific->mvpMatrixHandle, 1, GL_FALSE, SHADER::mvpMatrix.p);
        }
        if (gles_shaderSpecific->mvMatrixHandle != -1)
        {
            GLUniformMatrix4fv(gles_shaderSpecific->mvMatrixHandle, 1, GL_FALSE, SHADER::modelView.p);
        }

        // Unbind VBO so vertex pointers are treated as client-side arrays, and disable
        // stale attrib arrays left by a previous draw (e.g. LINE_MESH position-only shader)
        // to avoid GL_INVALID_OPERATION on strict GLES drivers (ANGLE, Mesa).
        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        disableUnusedVertexAttribs(gles_shaderSpecific, false, gles_shaderSpecific->texCoordHandle >= 0);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, backendBuffer->vboIndexSubsetIB[index_subset]);
        VAR_SHADER* var = this->pShader
            ? this->pShader->getVarByName("color")
            : nullptr;
        if (pGroup->segmented)
        {
            if (var && pGroup->color)
            {
                const int32_t handleVar = *static_cast<int32_t*>(var->ptrHandleVar);
                GLUniform4f(handleVar, pGroup->color->r, pGroup->color->g, pGroup->color->b, pGroup->color->a);

                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    const float* vertex = reinterpret_cast<float*>(&pGroup->vertex_particle[i * 4]);
					const float* uv = reinterpret_cast<float*>(&pGroup->uv[i * 4]);// note that when segmented we have different uv for each particle
                    GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), vertex);
                    GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2), uv);
                    GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                }
            }
            else
            {
                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    //if(i != 168/2 && i != 168/2+1 && i != 168/2+2 && i != 168/2+4)
                    {
                        const float* vertex = reinterpret_cast<float*>(&pGroup->vertex_particle[i * 4]);// note that when segmented we have different uv for each particle
                        const float* uv = reinterpret_cast<float*>(&pGroup->uv[i * 4]);
                        GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                        GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), vertex);
                        GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                        GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2), uv);
                        GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                    }
                }
            }
        }
        else
        {
            if (var && pGroup->color)
            {
                const int32_t handleVar = *static_cast<int32_t*>(var->ptrHandleVar);
                GLUniform4f(handleVar, pGroup->color->r, pGroup->color->g, pGroup->color->b, pGroup->color->a);
                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    const float* vertex = reinterpret_cast<float*>(&pGroup->vertex_particle[i * 4]);
					const float* uv = reinterpret_cast<float*>(pGroup->uv);//note that when not segmented we have same uv for all particles
                    GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), vertex);
                    GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2), uv);
                    GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                }
            }
            else
            {
                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    const float* vertex = reinterpret_cast<float*>(&pGroup->vertex_particle[i * 4]);
                    const float* uv = reinterpret_cast<float*>(pGroup->uv);//note that when not segmented we have same uv for all particles
                    GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), vertex);
                    GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2), uv);
                    GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                }
            }
        }

        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        if (cullFaceEnabled)
        {
            GLEnable(GL_CULL_FACE);
        }
        if (depthTestEnabled)
        {
            GLEnable(GL_DEPTH_TEST);
        }
        return true;
    }

    static uint32_t compileCodeShader(BASE_SHADER* ptrShader,  const unsigned int type, const char *shaderSrc)
    {
        GLint compiled = 0;
        // Create the shader object
        uint32_t shader = GLCreateShader(type);
        if (shader == 0)
        {
            PRINT_IF_DEBUG("GLCreateShader returned 0");
            return 0;
        }
        // Load the shader source
        GLShaderSource(shader, 1, &shaderSrc, nullptr);
        // Compile the shader
        GLCompileShader(shader);
        // Check the compile status
        GLGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            PRINT_IF_DEBUG("failed on compile shader [%s]",shaderSrc ? shaderSrc : "null");
            GLint infoLen = 0;
            GLGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen > 1)
            {
                auto *infoLog = static_cast<char *>(malloc(sizeof(char) * static_cast<size_t>(infoLen)));
                GLGetShaderInfoLog(shader, infoLen, nullptr, infoLog);
                PRINT_IF_DEBUG("Error compiling shader:%s\n%s\n",
                    ptrShader ? ptrShader->fileName.c_str() : "nullptr", infoLog);
                free(infoLog);
            }
            GLDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    static uint32_t loadShaderProgram(BASE_SHADER* pShader, BASE_SHADER* vShader,  void * ptrShaderSpecific, const char *vertShaderSrc, const char *fragShaderSrc)
    {
        GLES_PS_VS* gles_shaderSpecific = static_cast<GLES_PS_VS*>(ptrShaderSpecific);
        GLint linked = 0;
        if (gles_shaderSpecific->programObject)
        {
            PRINT_IF_DEBUG("programObject already exists");
            return gles_shaderSpecific->programObject;
        }
        // Load the vertex/fragment shaders
        uint32_t vertexShader = compileCodeShader(pShader, GL_VERTEX_SHADER, vertShaderSrc);
        if (vertexShader == 0)
        {
            PRINT_IF_DEBUG("vertexShader == 0");
            return 0;
        }
        uint32_t fragmentShader = compileCodeShader(vShader, GL_FRAGMENT_SHADER, fragShaderSrc);
        if (fragmentShader == 0)
        {
            PRINT_IF_DEBUG("fragmentShader == 0");
            GLDeleteShader(vertexShader);
            return 0;
        }
        // Create the program object
        gles_shaderSpecific->programObject = GLCreateProgram();
        if (gles_shaderSpecific->programObject == 0)
        {
            PRINT_IF_DEBUG("Failed to create programObject");
            return 0;
        }
        GLAttachShader(gles_shaderSpecific->programObject, vertexShader);
        GLAttachShader(gles_shaderSpecific->programObject, fragmentShader);
        // Link the program
        GLLinkProgram(gles_shaderSpecific->programObject);
        // Check the link status
        GLGetProgramiv(gles_shaderSpecific->programObject, GL_LINK_STATUS, &linked);
        if (linked == 0)
        {
            GLDeleteShader(vertexShader);
            GLDeleteShader(fragmentShader);
            PRINT_IF_DEBUG("linked status failed");
            GLint infoLen = 0;
            GLGetProgramiv(gles_shaderSpecific->programObject, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen > 1)
            {
                char *infoLog = static_cast<char *>(malloc(sizeof(char) * static_cast<size_t>(infoLen)));
                GLGetProgramInfoLog(gles_shaderSpecific->programObject, infoLen, nullptr, infoLog);
                PRINT_IF_DEBUG("Error linking program:\n%s\n", infoLog);
                free(infoLog);
            }
            GLDeleteProgram(gles_shaderSpecific->programObject);
            gles_shaderSpecific->programObject = 0;
            return 0;
        }
        // Free up no longer needed shader resources
        GLDeleteShader(vertexShader);
        GLDeleteShader(fragmentShader);
        return gles_shaderSpecific->programObject;
    }
}

#endif // USE_OPENGL_ES
