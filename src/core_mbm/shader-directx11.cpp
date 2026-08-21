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

#if defined(USE_DIRECTX11)

#include "specific-directx11-buffer.h"

#include <shader.h>
#include <util-interface.h>
#include <shader-var-cfg.h>
#include <device.h>
#include <header-mesh.h>
#include <draw-compatibility.h>
#include <texture-manager.h>
#include <particle-control.h>
#include <shader-resource.h>
#include <light.h>
#include "skeletal-directx9-shader-source.h"
#include "skeletal-gpu-lbs.h"

#include "specific-directx11-context.h"
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <algorithm>
#include <unordered_map>
#include <vector>

namespace mbm
{
    namespace
    {
        struct D3D11_VERTEX
        {
            VEC3 position;
            VEC3 normal;
            VEC2 uv;
        };

        struct D3D11_MATRIX_CONSTANTS
        {
            MATRIX mvp;
            MATRIX mv;
        };

        struct D3D11_LIGHT_CONSTANTS
        {
            int modes[4] = {};
            float ambientColor[4] = {};
            float lightDirectionView[4] = {};
            float directionalColor[4] = {};
            float lightColor[DEFAULT_SUPPORTED_MAX_LIGHTS][4] = {};
            float lightPositionRadius[DEFAULT_SUPPORTED_MAX_LIGHTS][4] = {};
            float materialDiffuse[4] = {};
            float materialAmbient[4] = {};
            float materialSpecular[4] = {};
            float materialEmissive[4] = {};
            float materialPower[4] = {};
        };

        struct D3D11_SHADER_DATA
        {
            struct CUSTOM_CONSTANTS
            {
                struct BLOCK
                {
                    ID3D11Buffer *buffer = nullptr;
                    std::vector<uint8_t> values;
                    UINT slot = 0;
                };
                struct LOCATION
                {
                    uint32_t block = 0;
                    uint32_t offset = 0;
                };
                std::vector<BLOCK> blocks;
                std::unordered_map<std::string, LOCATION> locations;
                std::unordered_map<std::string, uint32_t> sizes;

                void release() noexcept
                {
                    for (BLOCK &block : blocks)
                    {
                        if (block.buffer) block.buffer->Release();
                    }
                    blocks.clear();
                    locations.clear();
                    sizes.clear();
                }
            };
            ID3D11VertexShader *vertexShader = nullptr;
            ID3D11PixelShader *pixelShader = nullptr;
            ID3D11InputLayout *inputLayout = nullptr;
            ID3D11Buffer *matrixBuffer = nullptr;
            ID3D11Buffer *skeletalPaletteBuffer = nullptr;
            ID3D11Buffer *colorBuffer = nullptr;
            ID3D11Buffer *lightBuffer = nullptr;
            ID3D11SamplerState *defaultSampler = nullptr;
            ID3D11SamplerState *nearestSampler = nullptr;
            bool usesLineColor = false;
            bool usesParticle = false;
            bool usesSteeredParticle = false;
            bool steeredParticleHasColor = false;
            bool usesReservedLight = false;
            bool usesCustomReservedLight = false;
            uint32_t skeletalPaletteSize = 0;
            SKELETAL_SHADER_METHOD skeletalMethod = SKELETAL_SHADER_METHOD::NONE;
            CUSTOM_CONSTANTS pixelConstants;
            CUSTOM_CONSTANTS vertexConstants;

            void release() noexcept
            {
                if (nearestSampler) nearestSampler->Release();
                if (defaultSampler) defaultSampler->Release();
                if (colorBuffer) colorBuffer->Release();
                if (lightBuffer) lightBuffer->Release();
                if (matrixBuffer) matrixBuffer->Release();
                if (skeletalPaletteBuffer) skeletalPaletteBuffer->Release();
                if (inputLayout) inputLayout->Release();
                if (pixelShader) pixelShader->Release();
                if (vertexShader) vertexShader->Release();
                pixelConstants.release();
                vertexConstants.release();
                nearestSampler = nullptr;
                defaultSampler = nullptr;
                colorBuffer = nullptr;
                lightBuffer = nullptr;
                matrixBuffer = nullptr;
                skeletalPaletteBuffer = nullptr;
                inputLayout = nullptr;
                pixelShader = nullptr;
                vertexShader = nullptr;
                usesLineColor = false;
                usesParticle = false;
                usesSteeredParticle = false;
                steeredParticleHasColor = false;
                usesReservedLight = false;
                usesCustomReservedLight = false;
                skeletalPaletteSize = 0;
                skeletalMethod = SKELETAL_SHADER_METHOD::NONE;
            }
        };

        bool createBuffer(ID3D11Device *device, const void *data, const UINT byteWidth,
                          const UINT bindFlags, const bool dynamic, ID3D11Buffer **buffer);

        bool reflectCustomConstants(ID3DBlob *byteCode, ID3D11Device *device,
                                    D3D11_SHADER_DATA::CUSTOM_CONSTANTS &constants)
        {
            ID3D11ShaderReflection *reflection = nullptr;
            if (FAILED(D3DReflect(byteCode->GetBufferPointer(), byteCode->GetBufferSize(),
                                  __uuidof(ID3D11ShaderReflection), reinterpret_cast<void **>(&reflection))))
                return false;
            D3D11_SHADER_DESC shaderDescription = {};
            reflection->GetDesc(&shaderDescription);
            bool succeeded = true;
            for (UINT index = 0; index < shaderDescription.ConstantBuffers; ++index)
            {
                ID3D11ShaderReflectionConstantBuffer *constantBuffer = reflection->GetConstantBufferByIndex(index);
                D3D11_SHADER_BUFFER_DESC bufferDescription = {};
                if (FAILED(constantBuffer->GetDesc(&bufferDescription)) || bufferDescription.Type != D3D_CT_CBUFFER)
                    continue;
                D3D11_SHADER_INPUT_BIND_DESC binding = {};
                if (FAILED(reflection->GetResourceBindingDescByName(bufferDescription.Name, &binding)))
                    continue;
                D3D11_SHADER_DATA::CUSTOM_CONSTANTS::BLOCK block;
                block.values.assign(bufferDescription.Size, 0);
                block.slot = binding.BindPoint;
                const uint32_t blockIndex = static_cast<uint32_t>(constants.blocks.size());
                for (UINT variableIndex = 0; variableIndex < bufferDescription.Variables; ++variableIndex)
                {
                    D3D11_SHADER_VARIABLE_DESC variableDescription = {};
                    constantBuffer->GetVariableByIndex(variableIndex)->GetDesc(&variableDescription);
                    constants.locations[variableDescription.Name] = { blockIndex, variableDescription.StartOffset };
                    constants.sizes[variableDescription.Name] = variableDescription.Size;
                }
                succeeded = createBuffer(device, nullptr, bufferDescription.Size,
                    D3D11_BIND_CONSTANT_BUFFER, true, &block.buffer);
                if (!succeeded)
                    break;
                constants.blocks.push_back(std::move(block));
            }
            reflection->Release();
            return succeeded;
        }

        bool createBuffer(ID3D11Device *device, const void *data, const UINT byteWidth,
                          const UINT bindFlags, const bool dynamic, ID3D11Buffer **buffer)
        {
            D3D11_BUFFER_DESC description = {};
            description.ByteWidth = byteWidth;
            description.Usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
            description.BindFlags = bindFlags;
            description.CPUAccessFlags = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
            D3D11_SUBRESOURCE_DATA initialData = {};
            initialData.pSysMem = data;
            return SUCCEEDED(device->CreateBuffer(&description, data ? &initialData : nullptr, buffer));
        }

        D3D11_PRIMITIVE_TOPOLOGY getTopology(const uint32_t mode) noexcept
        {
            if (mode == util::MODE_DRAW_LINES)
                return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
            if (mode == util::MODE_DRAW_LINE_STRIP)
                return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
            if (mode == util::MODE_DRAW_TRIANGLE_STRIP)
                return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            if (mode == util::MODE_DRAW_POINTS)
                return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
            return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }

        const MATRIX &getViewMatrixForLightTargetD3D11(const LIGHT_TARGET target)
        {
            const CAMERA &camera = DEVICE::getInstance()->getCamera();
            return target == LIGHT_TARGET_2DW ? camera.matrixView2d : camera.matrixView;
        }

        VEC3 transformLightVectorD3D11(const VEC3 &value, const MATRIX &view, const bool position)
        {
            VEC3 transformed(
                (value.x * view._11) + (value.y * view._21) + (value.z * view._31),
                (value.x * view._12) + (value.y * view._22) + (value.z * view._32),
                (value.x * view._13) + (value.y * view._23) + (value.z * view._33));
            if (position)
            {
                transformed.x += view._41;
                transformed.y += view._42;
                transformed.z += view._43;
            }
            else
                Vec3Normalize(&transformed, &transformed);
            return transformed;
        }

        bool bufferHasNormalD3D11(const BUFFER_GL *buffer) noexcept
        {
            return buffer && (buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR ||
                              buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
        }

        bool bufferHasUvD3D11(const BUFFER_GL *buffer) noexcept
        {
            return buffer && (buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_UV ||
                              buffer->fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
        }

        struct ScopedRenderizableContextD3D11
        {
            DEVICE *device;
            explicit ScopedRenderizableContextD3D11(const RENDERIZABLE *owner) noexcept
                : device(DEVICE::getInstance()) { device->setRenderizableForCurrentRender(owner); }
            ~ScopedRenderizableContextD3D11() noexcept { device->clearRenderizableForCurrentRender(); }
        };

        bool hasReservedLightConstantsD3D11(const D3D11_SHADER_DATA::CUSTOM_CONSTANTS &constants)
        {
            static const char *names[] = {
                "LightEnabled", "LightCount", "LightMode", "AmbientColor", "LightDirectionView",
                "DirectionalColor", "LightPositionView", "LightRadius", "LightColor", "HasNormalMap",
                "MaterialDiffuse", "MaterialAmbient", "MaterialSpecular", "MaterialEmissive", "MaterialPower"
            };
            for (const char *name : names)
            {
                if (constants.locations.find(name) != constants.locations.end())
                    return true;
            }
            return false;
        }

        bool shaderSourceHasReservedLightD3D11(const char *source)
        {
            if (!source)
                return false;
            static const char *names[] = {
                "LightEnabled", "LightCount", "LightMode", "AmbientColor", "LightDirectionView",
                "DirectionalColor", "LightPositionView", "LightRadius", "LightColor", "HasNormalMap",
                "MaterialDiffuse", "MaterialAmbient", "MaterialSpecular", "MaterialEmissive", "MaterialPower"
            };
            for (const char *name : names)
            {
                if (strstr(source, name))
                    return true;
            }
            return false;
        }

        bool buildReservedLightConstantsD3D11(D3D11_LIGHT_CONSTANTS &constants,
                                               const BUFFER_GL *buffer,
                                               const uint32_t subsetIndex)
        {
            DEVICE *device = DEVICE::getInstance();
            LIGHT_STATE lightState;
            LIGHT_TARGET target = LIGHT_TARGET_3D;
            if (!device->getLightStateForCurrentRender(lightState))
                lightState = LIGHT_STATE();
            device->getLightTargetForCurrentRender(target);
            int lightMode = 0;
            if (lightState.enabled)
            {
                if (target == LIGHT_TARGET_2DW && bufferHasUvD3D11(buffer))
                    lightMode = 2;
                else if (target == LIGHT_TARGET_3D && bufferHasNormalD3D11(buffer))
                    lightMode = 1;
            }
            constants = D3D11_LIGHT_CONSTANTS();
            constants.modes[0] = lightMode != 0 ? 1 : 0;
            constants.modes[2] = lightMode;
            constants.modes[3] = lightMode == 2 && buffer && buffer->getTextureByStage(2, subsetIndex) ? 1 : 0;
            memcpy(constants.ambientColor, &lightState.ambientColor.r, sizeof(constants.ambientColor));
            memcpy(constants.directionalColor, &lightState.directionalColor.r, sizeof(constants.directionalColor));
            const MATRIX &view = getViewMatrixForLightTargetD3D11(target);
            const VEC3 directionView = transformLightVectorD3D11(lightState.directionalDirection, view, false);
            constants.lightDirectionView[0] = directionView.x;
            constants.lightDirectionView[1] = directionView.y;
            constants.lightDirectionView[2] = directionView.z;
            if (lightMode != 0)
            {
                LIGHT_POINT_SELECTION selections[DEFAULT_SUPPORTED_MAX_LIGHTS];
                const uint32_t count = device->getSelectedPointLightsForCurrentRender(
                    selections, DEFAULT_SUPPORTED_MAX_LIGHTS);
                constants.modes[1] = static_cast<int>(count);
                for (uint32_t index = 0; index < count; ++index)
                {
                    const LIGHT_POINT &point = selections[index].pointLight;
                    const VEC3 positionView = transformLightVectorD3D11(point.position, view, true);
                    memcpy(constants.lightColor[index], &point.color.r, sizeof(constants.lightColor[index]));
                    constants.lightPositionRadius[index][0] = positionView.x;
                    constants.lightPositionRadius[index][1] = positionView.y;
                    constants.lightPositionRadius[index][2] = positionView.z;
                    constants.lightPositionRadius[index][3] = point.radius;
                }
            }
            util::MATERIAL material;
            if (!device->getMaterialForCurrentRender(material))
            {
                material.Diffuse = COLOR(1.0f, 1.0f, 1.0f, 1.0f);
                material.Ambient = COLOR(1.0f, 1.0f, 1.0f, 1.0f);
                material.Specular = COLOR(0.0f, 0.0f, 0.0f, 1.0f);
                material.Emissive = COLOR(0.0f, 0.0f, 0.0f, 1.0f);
                material.Power = 0.0f;
            }
            memcpy(constants.materialDiffuse, &material.Diffuse.r, sizeof(constants.materialDiffuse));
            memcpy(constants.materialAmbient, &material.Ambient.r, sizeof(constants.materialAmbient));
            memcpy(constants.materialSpecular, &material.Specular.r, sizeof(constants.materialSpecular));
            memcpy(constants.materialEmissive, &material.Emissive.r, sizeof(constants.materialEmissive));
            constants.materialPower[0] = material.Power;
            return true;
        }

        void writeCustomConstantD3D11(D3D11_SHADER_DATA::CUSTOM_CONSTANTS &target,
                                      const char *name, const void *value, const size_t byteCount)
        {
            const auto location = target.locations.find(name);
            const auto size = target.sizes.find(name);
            if (location == target.locations.end() || size == target.sizes.end() ||
                location->second.block >= target.blocks.size())
                return;
            D3D11_SHADER_DATA::CUSTOM_CONSTANTS::BLOCK &block = target.blocks[location->second.block];
            const size_t copySize = std::min(byteCount, static_cast<size_t>(size->second));
            if (location->second.offset + copySize <= block.values.size())
                memcpy(block.values.data() + location->second.offset, value, copySize);
        }

        void writeCustomLightArrayD3D11(D3D11_SHADER_DATA::CUSTOM_CONSTANTS &target,
                                        const char *name, const float *values,
                                        const uint32_t componentCount, const uint32_t elementCount)
        {
            const auto location = target.locations.find(name);
            const auto size = target.sizes.find(name);
            if (location == target.locations.end() || size == target.sizes.end() ||
                location->second.block >= target.blocks.size())
                return;
            D3D11_SHADER_DATA::CUSTOM_CONSTANTS::BLOCK &block = target.blocks[location->second.block];
            if (size->second <= 16u)
            {
                const size_t copySize = std::min(static_cast<size_t>(size->second),
                                                 static_cast<size_t>(componentCount * sizeof(float)));
                if (location->second.offset + copySize <= block.values.size())
                    memcpy(block.values.data() + location->second.offset, values, copySize);
                return;
            }
            const uint32_t capacity = std::min(elementCount, size->second / 16u);
            for (uint32_t index = 0; index < capacity; ++index)
            {
                const size_t destination = location->second.offset + (index * 16u);
                if (destination + (componentCount * sizeof(float)) > block.values.size())
                    break;
                memcpy(block.values.data() + destination, values + (index * 4u),
                       componentCount * sizeof(float));
            }
        }

        bool uploadCustomConstantsD3D11(ID3D11DeviceContext *context,
                                        D3D11_SHADER_DATA::CUSTOM_CONSTANTS &target,
                                        const bool pixel)
        {
            for (D3D11_SHADER_DATA::CUSTOM_CONSTANTS::BLOCK &block : target.blocks)
            {
                if (!block.buffer || block.values.empty())
                    continue;
                D3D11_MAPPED_SUBRESOURCE mapped = {};
                if (FAILED(context->Map(block.buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                    return false;
                memcpy(mapped.pData, block.values.data(), block.values.size());
                context->Unmap(block.buffer, 0);
                if (pixel)
                    context->PSSetConstantBuffers(block.slot, 1, &block.buffer);
                else
                    context->VSSetConstantBuffers(block.slot, 1, &block.buffer);
            }
            return true;
        }

        bool uploadCustomReservedLightConstantsD3D11(ID3D11DeviceContext *context,
                                                      D3D11_SHADER_DATA::CUSTOM_CONSTANTS &target,
                                                      const D3D11_LIGHT_CONSTANTS &constants,
                                                      const bool pixel)
        {
            if (target.blocks.empty() || !hasReservedLightConstantsD3D11(target))
                return true;
            writeCustomConstantD3D11(target, "LightEnabled", &constants.modes[0], sizeof(int));
            writeCustomConstantD3D11(target, "LightCount", &constants.modes[1], sizeof(int));
            writeCustomConstantD3D11(target, "LightMode", &constants.modes[2], sizeof(int));
            writeCustomConstantD3D11(target, "HasNormalMap", &constants.modes[3], sizeof(int));
            writeCustomConstantD3D11(target, "AmbientColor", constants.ambientColor, sizeof(constants.ambientColor));
            writeCustomConstantD3D11(target, "LightDirectionView", constants.lightDirectionView, sizeof(float) * 3u);
            writeCustomConstantD3D11(target, "DirectionalColor", constants.directionalColor, sizeof(constants.directionalColor));
            writeCustomLightArrayD3D11(target, "LightPositionView", &constants.lightPositionRadius[0][0], 3u,
                                       DEFAULT_SUPPORTED_MAX_LIGHTS);
            float radii[DEFAULT_SUPPORTED_MAX_LIGHTS][4] = {};
            for (uint32_t index = 0; index < DEFAULT_SUPPORTED_MAX_LIGHTS; ++index)
                radii[index][0] = constants.lightPositionRadius[index][3];
            writeCustomLightArrayD3D11(target, "LightRadius", &radii[0][0], 1u, DEFAULT_SUPPORTED_MAX_LIGHTS);
            const auto lightColorSize = target.sizes.find("LightColor");
            const float *lightColors = lightColorSize != target.sizes.end() && lightColorSize->second <= 16u &&
                constants.modes[2] == 1 ? constants.directionalColor : &constants.lightColor[0][0];
            writeCustomLightArrayD3D11(target, "LightColor", lightColors, 4u, DEFAULT_SUPPORTED_MAX_LIGHTS);
            writeCustomConstantD3D11(target, "MaterialDiffuse", constants.materialDiffuse, sizeof(constants.materialDiffuse));
            writeCustomConstantD3D11(target, "MaterialAmbient", constants.materialAmbient, sizeof(constants.materialAmbient));
            writeCustomConstantD3D11(target, "MaterialSpecular", constants.materialSpecular, sizeof(constants.materialSpecular));
            writeCustomConstantD3D11(target, "MaterialEmissive", constants.materialEmissive, sizeof(constants.materialEmissive));
            writeCustomConstantD3D11(target, "MaterialPower", constants.materialPower, sizeof(float));
            return uploadCustomConstantsD3D11(context, target, pixel);
        }

        bool uploadReservedLightConstantsD3D11(ID3D11DeviceContext *context,
                                                D3D11_SHADER_DATA *shaderData,
                                                const BUFFER_GL *buffer,
                                                const uint32_t subsetIndex)
        {
            if (!context || !shaderData || !shaderData->lightBuffer)
                return false;
            D3D11_LIGHT_CONSTANTS constants;
            buildReservedLightConstantsD3D11(constants, buffer, subsetIndex);
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (FAILED(context->Map(shaderData->lightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                return false;
            memcpy(mapped.pData, &constants, sizeof(constants));
            context->Unmap(shaderData->lightBuffer, 0);
            context->PSSetConstantBuffers(2, 1, &shaderData->lightBuffer);
            return true;
        }
    }

    BUFFER_SPECIFIC::BUFFER_SPECIFIC() noexcept 
        : vertexBuffer(nullptr), skinVertexBuffer(nullptr), indexBuffer(nullptr), vertexStride(sizeof(D3D11_VERTEX)), dynamicVertexBuffer(false)
    {
    }

    BUFFER_SPECIFIC::~BUFFER_SPECIFIC()
    {
        this->release();
    }

    void BUFFER_SPECIFIC::release()
    {
        if (indexBuffer) indexBuffer->Release();
        if (skinVertexBuffer) skinVertexBuffer->Release();
        if (vertexBuffer) vertexBuffer->Release();
        indexBuffer = nullptr;
        skinVertexBuffer = nullptr;
        vertexBuffer = nullptr;
        vertexStride = sizeof(D3D11_VERTEX);
        dynamicVertexBuffer = false;
    }

    BUFFER_GL::BUFFER_GL() :
        indexStartIB(nullptr),
        indexCountIB(nullptr),
        vertexStartVB(nullptr),
        vertexCountVB(nullptr),
        sizeOfArrayVertex(0),
        fvf(FVF_PROVIDE_BY_ENGINE::FVF_POS_UV),
        mode_draw(util::MODE_DRAW_TRIANGLES),
        mode_cull_face(util::CULL_BACK),
        mode_front_face_direction(util::CCW),
        totalSubset(0),
        initializedIndexBuffer(false)
    {
        //we initialize this at the moment (just once)
        setBackendBuffer(new BUFFER_SPECIFIC());
    }

    BUFFER_GL::~BUFFER_GL()
    {
        release();
        BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
        if(backendBuffer)
        {
            // Deleting a void* pointer directly in C++ is undefined behavior and should be avoided. 
            delete static_cast<BUFFER_SPECIFIC*>(backendBuffer);
        }
        setBackendBuffer(nullptr);
        texturesByStage.clear();
    }

    void BUFFER_GL::release()
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
        this->sizeOfArrayVertex = 0;
        this->initializedIndexBuffer = false;
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
        this->initializeVertexBufferControl(totalSubsets, sizeOfArrayVertex,
                                            vertexStartSubset, vertexCountSubset, info_draw_mode);
        this->fvf = (normal && uv) ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV :
            (normal ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR :
            (uv ? FVF_PROVIDE_BY_ENGINE::FVF_POS_UV : FVF_PROVIDE_BY_ENGINE::FVF_POS));
        std::vector<D3D11_VERTEX> vertices(sizeOfArrayVertex);
        for (uint32_t i = 0; i < sizeOfArrayVertex; ++i)
        {
            vertices[i].position = vertex[i];
            vertices[i].normal = normal ? normal[i] : VEC3(0.0f, 0.0f, 1.0f);
            vertices[i].uv = uv ? uv[i] : VEC2(0.0f, 0.0f);
        }
        BUFFER_SPECIFIC *backend = getBackendBuffer();
        backend->dynamicVertexBuffer = isDynamic;
        ID3D11Device *d3dDevice = DEVICE::getInstance()->getSpecificContextDevice()->device;
        if (!createBuffer(d3dDevice, vertices.data(), static_cast<UINT>(vertices.size() * sizeof(D3D11_VERTEX)),
                          D3D11_BIND_VERTEX_BUFFER, isDynamic, &backend->vertexBuffer))
        {
            release();
            return false;
        }
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
        this->initializeIndexBufferControl(totalSubsets, sizeOfArrayVertex,
                                           indexStartSubset, indexCountSubset, info_draw_mode);
        this->fvf = (normal && uv) ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV :
            (normal ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR :
            (uv ? FVF_PROVIDE_BY_ENGINE::FVF_POS_UV : FVF_PROVIDE_BY_ENGINE::FVF_POS));
        std::vector<D3D11_VERTEX> vertices(sizeOfArrayVertex);
        for (uint32_t i = 0; i < sizeOfArrayVertex; ++i)
        {
            vertices[i].position = vertex[i];
            vertices[i].normal = normal ? normal[i] : VEC3(0.0f, 0.0f, 1.0f);
            vertices[i].uv = uv ? uv[i] : VEC2(0.0f, 0.0f);
        }
        BUFFER_SPECIFIC *backend = getBackendBuffer();
        ID3D11Device *d3dDevice = DEVICE::getInstance()->getSpecificContextDevice()->device;
        uint32_t indexCount = 0;
        for (uint32_t subset = 0; subset < totalSubsets; ++subset)
            indexCount = (std::max)(indexCount, static_cast<uint32_t>(indexStartSubset[subset] + indexCountSubset[subset]));
        if (!createBuffer(d3dDevice, vertices.data(), static_cast<UINT>(vertices.size() * sizeof(D3D11_VERTEX)),
                          D3D11_BIND_VERTEX_BUFFER, false, &backend->vertexBuffer) ||
            !createBuffer(d3dDevice, arrayIndices, indexCount * sizeof(uint16_t),
                          D3D11_BIND_INDEX_BUFFER, false, &backend->indexBuffer))
        {
            release();
            return false;
        }
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
        uint32_t vertexCount = 0;
        for (uint32_t subset = 0; subset < totalSubsets; ++subset)
        {
            if (indexStartSubset[subset] < 0 || indexCountSubset[subset] <= 0)
                return false;
            for (int i = 0; i < indexCountSubset[subset]; ++i)
            {
                const uint32_t candidate =
                    static_cast<uint32_t>(arrayIndices[indexStartSubset[subset] + i]) + 1u;
                if (candidate > vertexCount)
                    vertexCount = candidate;
            }
        }
        if (vertexCount == 0)
            return false;
        this->initializeIndexBufferControl(totalSubsets, vertexCount,
                                           indexStartSubset, indexCountSubset, info_draw_mode);
        this->fvf = (hasNormal && hasUv) ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV :
            (hasNormal ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR :
            (hasUv ? FVF_PROVIDE_BY_ENGINE::FVF_POS_UV : FVF_PROVIDE_BY_ENGINE::FVF_POS));
        uint32_t indexCount = 0;
        for (uint32_t subset = 0; subset < totalSubsets; ++subset)
            indexCount = (std::max)(indexCount, static_cast<uint32_t>(indexStartSubset[subset] + indexCountSubset[subset]));
        BUFFER_SPECIFIC *backend = getBackendBuffer();
        backend->dynamicVertexBuffer = true;
        ID3D11Device *d3dDevice = DEVICE::getInstance()->getSpecificContextDevice()->device;
        if (!createBuffer(d3dDevice, arrayIndices, indexCount * sizeof(uint16_t),
                          D3D11_BIND_INDEX_BUFFER, false, &backend->indexBuffer) ||
            !createBuffer(d3dDevice, nullptr, vertexCount * sizeof(D3D11_VERTEX),
                          D3D11_BIND_VERTEX_BUFFER, true, &backend->vertexBuffer))
        {
            release();
            return false;
        }
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
        if (!vertex || !vertexStartSubset || !vertexCountSubset)
            return false;
        if (this->initializedIndexBuffer)
        {
            BUFFER_SPECIFIC *backend = getBackendBuffer();
            if (!backend || !backend->vertexBuffer || !backend->dynamicVertexBuffer)
                return false;
            std::vector<D3D11_VERTEX> vertices(sizeOfArrayVertex);
            for (uint32_t i = 0; i < sizeOfArrayVertex; ++i)
            {
                vertices[i].position = vertex[i];
                vertices[i].normal = normal ? normal[i] : VEC3(0.0f, 0.0f, 1.0f);
                vertices[i].uv = uv ? uv[i] : VEC2(0.0f, 0.0f);
            }
            ID3D11DeviceContext *d3dContext = DEVICE::getInstance()->getSpecificContextDevice()->immediateContext;
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (FAILED(d3dContext->Map(backend->vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                return false;
            memcpy(mapped.pData, vertices.data(), vertices.size() * sizeof(D3D11_VERTEX));
            d3dContext->Unmap(backend->vertexBuffer, 0);
            return this->sizeOfArrayVertex > 0;
        }
        for (uint32_t subset = 0; subset < this->totalSubset; ++subset)
        {
            if (vertexStartSubset[subset] < 0 || vertexCountSubset[subset] <= 0)
                return false;
            const uint32_t vertexStart = static_cast<uint32_t>(vertexStartSubset[subset]);
            const uint32_t vertexCount = static_cast<uint32_t>(vertexCountSubset[subset]);
            if (vertexCount > this->sizeOfArrayVertex ||
                vertexStart + vertexCount > this->sizeOfArrayVertex)
                return false;
        }
        BUFFER_SPECIFIC *backend = getBackendBuffer();
        if (backend && backend->dynamicVertexBuffer)
        {
            std::vector<D3D11_VERTEX> vertices(sizeOfArrayVertex);
            for (uint32_t i = 0; i < sizeOfArrayVertex; ++i)
            {
                vertices[i].position = vertex[i];
                vertices[i].normal = normal ? normal[i] : VEC3(0.0f, 0.0f, 1.0f);
                vertices[i].uv = uv ? uv[i] : VEC2(0.0f, 0.0f);
            }
            ID3D11DeviceContext *d3dContext = DEVICE::getInstance()->getSpecificContextDevice()->immediateContext;
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (FAILED(d3dContext->Map(backend->vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                return false;
            memcpy(mapped.pData, vertices.data(), vertices.size() * sizeof(D3D11_VERTEX));
            d3dContext->Unmap(backend->vertexBuffer, 0);
        }
        return true;
    }

    bool BASE_SHADER::addVar(const char *nameVar, const TYPE_VAR_SHADER typeVar, const float *defaultValue,
                       void* ptrShaderSpecific, const bool isPS) // Adiciona uma variavel para o shader indicando o nome da mesma
                                                         // no cÃƒÆ’Ã‚Â³digo e o tipo.
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
            D3D11_SHADER_DATA *shaderData = static_cast<D3D11_SHADER_DATA *>(ptrShaderSpecific);
            D3D11_SHADER_DATA::CUSTOM_CONSTANTS &constants = isPS ?
                shaderData->pixelConstants : shaderData->vertexConstants;
            const auto found = constants.locations.find(nameVar);
            if (found == constants.locations.end())
            {
                const bool internalVariable = shaderData->usesLineColor || shaderData->usesParticle ||
                    shaderData->usesSteeredParticle;
                if (!internalVariable)
                {
                    delete var;
                    return false;
                }
            }
            else
            {
                *static_cast<int32_t *>(var->ptrHandleVar) = static_cast<int32_t>(
                    (found->second.block << 24u) | (found->second.offset & 0x00ffffffu));
            }
            if (defaultValue && var->sizeVar > 0)
                memcpy(var->current, defaultValue, static_cast<size_t>(var->sizeVar) * sizeof(float));
            lsVar.push_back(var);
            return true;
        }
        return false;
    }

    void BASE_SHADER::update(void * ptrShaderSpecific) const
    {
        D3D11_SHADER_DATA *shaderData = static_cast<D3D11_SHADER_DATA *>(ptrShaderSpecific);
        if (!shaderData)
            return;
        SPECIFIC_AUX_CONTEXT_DEVICE *context = DEVICE::getInstance()->getSpecificContextDevice();
        for (VAR_SHADER *var : lsVar)
        {
            if (!var || !var->ptrHandleVar)
                continue;
            D3D11_SHADER_DATA::CUSTOM_CONSTANTS &constants = var->isPS ?
                shaderData->pixelConstants : shaderData->vertexConstants;
            const uint32_t location = static_cast<uint32_t>(*static_cast<int32_t *>(var->ptrHandleVar));
            const uint32_t blockIndex = location >> 24u;
            const uint32_t offset = location & 0x00ffffffu;
            const size_t byteCount = static_cast<size_t>(var->sizeVar) * sizeof(float);
            if (blockIndex >= constants.blocks.size())
                continue;
            D3D11_SHADER_DATA::CUSTOM_CONSTANTS::BLOCK &block = constants.blocks[blockIndex];
            if (static_cast<size_t>(offset) + byteCount > block.values.size())
                continue;
            if (var->typeVar == VAR_INT)
            {
                const int32_t value = var->getCurrentInt();
                memcpy(block.values.data() + offset, &value, sizeof(value));
            }
            else
            {
                memcpy(block.values.data() + offset, var->current, byteCount);
            }
        }
        uploadCustomConstantsD3D11(context->immediateContext, shaderData->pixelConstants, true);
        uploadCustomConstantsD3D11(context->immediateContext, shaderData->vertexConstants, false);
    }

    SHADER::SHADER() : pShader(nullptr), vShader(nullptr)
    {
        setBackendShaderSpecific(new D3D11_SHADER_DATA());
    }

    SHADER::~SHADER()
    {
        D3D11_SHADER_DATA *shaderData = static_cast<D3D11_SHADER_DATA *>(getBackendShaderSpecific());
        if (shaderData)
        {
            shaderData->release();
            delete shaderData;
            setBackendShaderSpecific(nullptr);
        }
        // Deleting a void* pointer directly in C++ is undefined behavior and should be avoided. 
    }

    void SHADER::onRestore()
    {
    }

    void SHADER::clearDefaultProgramCache() noexcept
    {
    }

    bool SHADER::isLoad() const noexcept
    {
        const D3D11_SHADER_DATA *shaderData = static_cast<const D3D11_SHADER_DATA *>(getBackendShaderSpecific());
        return shaderData && shaderData->vertexShader && shaderData->pixelShader && shaderData->inputLayout;
    }

    void SHADER::releaseShader()
    {
        D3D11_SHADER_DATA *shaderData = static_cast<D3D11_SHADER_DATA *>(getBackendShaderSpecific());
        if (shaderData)
            shaderData->release();
    }

    bool SHADER::compileShader(mbm::BASE_SHADER *ptrPshader, mbm::BASE_SHADER *ptrVshader,
                               mbm::FVF_PROVIDE_BY_ENGINE fvf, const uint32_t skeletalPaletteSize,
                               const SKELETAL_SHADER_METHOD skeletalMethod)
    {
        if (fvf == FVF_PROVIDE_BY_ENGINE::FVF_NONE)
            return false;
        this->pShader             = ptrPshader;
        this->vShader             = ptrVshader;
        D3D11_SHADER_DATA *shaderData = static_cast<D3D11_SHADER_DATA *>(getBackendShaderSpecific());
        if (!shaderData)
            return false;
        shaderData->release();
        const bool usesLineColor = ptrPshader && ptrVshader &&
            ptrPshader->fileName == "__line_color.ps" && ptrVshader->fileName == "__line_color.vs";
        const bool usesParticle = ptrPshader && ptrVshader &&
            ptrPshader->fileName == "__particle.ps" && ptrVshader->fileName == "__particle.vs";
        const bool usesSteeredParticle = ptrPshader && ptrVshader &&
            (ptrPshader->fileName == "__steered_particle.ps" ||
             ptrPshader->fileName == "__steered_particle_no_color.ps") &&
            ptrVshader->fileName == "__steered_particle.vs";
        const bool steeredParticleHasColor = usesSteeredParticle &&
            ptrPshader->fileName == "__steered_particle.ps";
        const bool usesSkeletal = skeletalPaletteSize > 0;
        const bool hasNormal = fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR ||
                               fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV;
        const bool hasUv = fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_UV ||
                           fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV;
        const bool usesGeneratedReservedLight = this->shouldCompileReservedLightDefault();
        const bool usesCustomReservedLight = (ptrPshader && shaderSourceHasReservedLightD3D11(ptrPshader->getCode())) ||
                                             (ptrVshader && shaderSourceHasReservedLightD3D11(ptrVshader->getCode()));
        const bool usesLightingScaffolding = usesGeneratedReservedLight || usesCustomReservedLight;
        if (usesSkeletal && ptrVshader)
        {
            ERROR_AT(__LINE__, __FILE__, "canonical DirectX11 skinning does not support a custom vertex shader");
            return false;
        }
        std::string defaultShaderSource =
            "cbuffer Matrices:register(b0){row_major float4x4 mvp;row_major float4x4 mv;}";
        if (usesSkeletal)
            skeletal::appendDirectX9SkeletalFunctions(defaultShaderSource, skeletalPaletteSize, skeletalMethod);
        defaultShaderSource +=
            "struct VSInput { float4 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0;";
        if (usesSkeletal)
            defaultShaderSource += " float4 boneIndices : BLENDINDICES0; float4 boneWeights : BLENDWEIGHT0;";
        defaultShaderSource +=
            " };struct VSOutput {float4 position:SV_POSITION;float2 uv:TEXCOORD0;";
        if (usesLightingScaffolding)
            defaultShaderSource += "float3 normalView:TEXCOORD1;float3 positionView:TEXCOORD2;";
        defaultShaderSource += "};"
            "VSOutput VSMain(VSInput input) { VSOutput output;";
        if (usesSkeletal)
            skeletal::appendDirectX9SkeletalDeformation(defaultShaderSource, skeletalMethod,
                                                         usesLightingScaffolding && hasNormal);
        else
            defaultShaderSource += "float4 skinnedPosition=input.position;";
        defaultShaderSource +=
            "output.position=mul(skinnedPosition,mvp);output.uv=input.uv;";
        if (usesLightingScaffolding)
        {
            defaultShaderSource += usesSkeletal && hasNormal ?
                "output.normalView=mul(float4(skinnedNormal,0),mv).xyz;" :
                "output.normalView=mul(float4(input.normal,0),mv).xyz;";
            defaultShaderSource += "output.positionView=mul(skinnedPosition,mv).xyz;";
        }
        defaultShaderSource += "return output;}"
            "Texture2D DiffuseTexture:register(t0);Texture2D NormalTexture:register(t2);"
            "SamplerState DiffuseSampler:register(s0);";
        if (usesGeneratedReservedLight)
        {
            defaultShaderSource +=
                "cbuffer ReservedLight:register(b2){int4 LightModes;float4 AmbientColor;"
                "float4 LightDirectionView;float4 DirectionalColor;float4 LightColor[" +
                std::to_string(DEFAULT_SUPPORTED_MAX_LIGHTS) + "];float4 LightPositionRadius[" +
                std::to_string(DEFAULT_SUPPORTED_MAX_LIGHTS) + "];float4 MaterialDiffuse;"
                "float4 MaterialAmbient;float4 MaterialSpecular;float4 MaterialEmissive;float4 MaterialPower;}"
                "float4 PSMain(VSOutput input):SV_TARGET{float4 texColor=";
            defaultShaderSource += hasUv ? "DiffuseTexture.Sample(DiffuseSampler,input.uv);" : "float4(1,1,1,1);";
            defaultShaderSource +=
                "if(LightModes.x==0||LightModes.z==0)return texColor;"
                "float3 normalView=normalize(input.normalView);"
                "if(LightModes.z==2){normalView=float3(0,0,1);"
                "if(LightModes.w!=0)normalView=normalize(NormalTexture.Sample(DiffuseSampler,input.uv).xyz*2-1);}"
                "float3 viewDir=normalize(-input.positionView);"
                "float3 base=texColor.rgb*MaterialDiffuse.rgb;"
                "float3 light=AmbientColor.rgb*MaterialAmbient.rgb;float3 specular=0;"
                "if(LightModes.z==1){float3 travel=normalize(LightDirectionView.xyz);"
                "float diffuse=max(dot(normalView,-travel),0);light+=DirectionalColor.rgb*diffuse;"
                "if(diffuse>0&&MaterialPower.x>0){float3 halfDir=normalize(-travel+viewDir);"
                "float spec=pow(max(dot(normalView,halfDir),0),MaterialPower.x);"
                "specular+=DirectionalColor.rgb*MaterialSpecular.rgb*spec;}}"
                "[unroll]for(int i=0;i<" + std::to_string(DEFAULT_SUPPORTED_MAX_LIGHTS) + ";++i){"
                "if(i>=LightModes.y)break;float3 toLight=LightPositionRadius[i].xyz-input.positionView;"
                "float dist=length(toLight);float radius=LightPositionRadius[i].w;if(radius>0.0001){"
                "float3 lightDir=toLight/max(dist,0.0001);float diffuse=max(dot(normalView,lightDir),0);"
                "float attenuation=1-saturate(dist/radius);attenuation*=attenuation;"
                "light+=LightColor[i].rgb*diffuse*attenuation;if(diffuse>0&&MaterialPower.x>0){"
                "float3 halfDir=normalize(lightDir+viewDir);float spec=pow(max(dot(normalView,halfDir),0),MaterialPower.x);"
                "specular+=LightColor[i].rgb*MaterialSpecular.rgb*spec*attenuation;}}}"
                "float3 lit=saturate(base*saturate(light)+MaterialEmissive.rgb+specular);"
                "return float4(lit,texColor.a*MaterialDiffuse.a);}";
        }
        else
            defaultShaderSource += "float4 PSMain(VSOutput input):SV_TARGET{return DiffuseTexture.Sample(DiffuseSampler,input.uv);}";
        static const char lineShaderSource[] =
            "cbuffer Matrices : register(b0) { row_major float4x4 mvp; };"
            "cbuffer LineColor : register(b0) { float4 color; };"
            "struct VSInput { float3 position : POSITION; float3 normal : NORMAL; float2 uv : TEXCOORD0; };"
            "float4 VSMain(VSInput input) : SV_POSITION { return mul(float4(input.position, 1.0), mvp); }"
            "float4 PSMain() : SV_TARGET { return color; }";
        const bool usesCustomVertexShader = ptrVshader && !usesLineColor && !usesParticle && !usesSteeredParticle;
        const bool usesCustomPixelShader = ptrPshader && !usesLineColor && !usesParticle && !usesSteeredParticle;
        const char *vertexShaderSource = usesLineColor ? lineShaderSource :
            (usesCustomVertexShader ? ptrVshader->getCode() : defaultShaderSource.c_str());
        const char *pixelShaderSource = usesLineColor ? lineShaderSource :
            (usesCustomPixelShader ? ptrPshader->getCode() : defaultShaderSource.c_str());
        const char *vertexEntryPoint = usesCustomVertexShader ? "main" : "VSMain";
        const char *pixelEntryPoint = usesCustomPixelShader ? "main" : "PSMain";
        if (usesParticle || usesSteeredParticle)
        {
            vertexShaderSource = ptrVshader->getCode();
            pixelShaderSource = ptrPshader->getCode();
            vertexEntryPoint = "main";
            pixelEntryPoint = "main";
        }
        UINT compileFlags = D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;
#if defined(_DEBUG)
        compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
        ID3DBlob *vertexByteCode = nullptr;
        ID3DBlob *pixelByteCode = nullptr;
        ID3DBlob *errors = nullptr;
        const char *vertexProfile = getVSVersion();
        const char *pixelProfile = getPSVersion();
        HRESULT result = D3DCompile(vertexShaderSource, strlen(vertexShaderSource), "mini-mbm-directx11-default",
                                    nullptr, nullptr, vertexEntryPoint, vertexProfile, compileFlags, 0,
                                    &vertexByteCode, &errors);
        if (FAILED(result))
        {
            ERROR_LOG("DirectX11 vertex shader compilation failed: %s",
                      errors ? static_cast<const char *>(errors->GetBufferPointer()) : "unknown error");
            if (errors) errors->Release();
            return false;
        }
        if (errors) errors->Release();
        errors = nullptr;
        result = D3DCompile(pixelShaderSource, strlen(pixelShaderSource), "mini-mbm-directx11-default",
                            nullptr, nullptr, pixelEntryPoint, pixelProfile, compileFlags, 0,
                            &pixelByteCode, &errors);
        if (FAILED(result))
        {
            ERROR_LOG("DirectX11 pixel shader compilation failed: %s",
                      errors ? static_cast<const char *>(errors->GetBufferPointer()) : "unknown error");
            if (errors) errors->Release();
            vertexByteCode->Release();
            return false;
        }
        if (errors) errors->Release();
        ID3D11Device *d3dDevice = DEVICE::getInstance()->getSpecificContextDevice()->device;
        D3D11_INPUT_ELEMENT_DESC elements[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(D3D11_VERTEX, position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, static_cast<UINT>(offsetof(D3D11_VERTEX, normal)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(D3D11_VERTEX, uv)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, static_cast<UINT>(offsetof(skeletal::GPU_LBS_VERTEX, boneIndex)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, static_cast<UINT>(offsetof(skeletal::GPU_LBS_VERTEX, weight)), D3D11_INPUT_PER_VERTEX_DATA, 0 }
        };
        result = d3dDevice->CreateVertexShader(vertexByteCode->GetBufferPointer(), vertexByteCode->GetBufferSize(),
                                               nullptr, &shaderData->vertexShader);
        if (SUCCEEDED(result))
            result = d3dDevice->CreatePixelShader(pixelByteCode->GetBufferPointer(), pixelByteCode->GetBufferSize(),
                                                  nullptr, &shaderData->pixelShader);
        if (SUCCEEDED(result))
            result = d3dDevice->CreateInputLayout(elements, usesSkeletal ? 5u : 3u,
                                                  vertexByteCode->GetBufferPointer(), vertexByteCode->GetBufferSize(),
                                                  &shaderData->inputLayout);
        if (SUCCEEDED(result) && usesCustomVertexShader)
            result = reflectCustomConstants(vertexByteCode, d3dDevice, shaderData->vertexConstants) ? S_OK : E_FAIL;
        if (SUCCEEDED(result) && usesCustomPixelShader)
            result = reflectCustomConstants(pixelByteCode, d3dDevice, shaderData->pixelConstants) ? S_OK : E_FAIL;
        vertexByteCode->Release();
        pixelByteCode->Release();
        D3D11_SAMPLER_DESC samplerDescription = {};
        samplerDescription.Filter = D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
        samplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        samplerDescription.AddressV = samplerDescription.AddressU;
        samplerDescription.AddressW = samplerDescription.AddressU;
        samplerDescription.MaxLOD = D3D11_FLOAT32_MAX;
        const bool createdMatrixBuffer = createBuffer(d3dDevice, nullptr, sizeof(D3D11_MATRIX_CONSTANTS),
                                                       D3D11_BIND_CONSTANT_BUFFER, true,
                                                       &shaderData->matrixBuffer);
        const bool createdLightBuffer = !usesGeneratedReservedLight ||
            createBuffer(d3dDevice, nullptr, sizeof(D3D11_LIGHT_CONSTANTS), D3D11_BIND_CONSTANT_BUFFER,
                         true, &shaderData->lightBuffer);
        const uint32_t floatsPerBone = skeletalMethod == SKELETAL_SHADER_METHOD::DQS_RIGID ? 8u : 12u;
        const bool createdPaletteBuffer = !usesSkeletal ||
            createBuffer(d3dDevice, nullptr, skeletalPaletteSize * floatsPerBone * sizeof(float),
                         D3D11_BIND_CONSTANT_BUFFER, true, &shaderData->skeletalPaletteBuffer);
        const bool needsColorBuffer = usesLineColor || usesParticle || steeredParticleHasColor;
        const bool needsSampler = !usesLineColor;
        const bool createdColorBuffer = !needsColorBuffer ||
            createBuffer(d3dDevice, nullptr, sizeof(float) * 8u, D3D11_BIND_CONSTANT_BUFFER,
                         true, &shaderData->colorBuffer);
        const bool createdDefaultSampler = !needsSampler ||
            SUCCEEDED(d3dDevice->CreateSamplerState(&samplerDescription, &shaderData->defaultSampler));
        samplerDescription.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        samplerDescription.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
        samplerDescription.AddressV = samplerDescription.AddressU;
        samplerDescription.AddressW = samplerDescription.AddressU;
        const bool createdNearestSampler = !needsSampler ||
            SUCCEEDED(d3dDevice->CreateSamplerState(&samplerDescription, &shaderData->nearestSampler));
        const bool createdSampler = createdDefaultSampler && createdNearestSampler;
        const bool createdPixelResource = createdColorBuffer && createdSampler;
        if (FAILED(result) || !createdMatrixBuffer || !createdLightBuffer || !createdPaletteBuffer || !createdPixelResource)
        {
            shaderData->release();
            ERROR_LOG("DirectX11 failed to create the basic shader pipeline (HRESULT=0x%08lx)", result);
            return false;
        }
        shaderData->usesLineColor = usesLineColor;
        shaderData->usesParticle = usesParticle;
        shaderData->usesSteeredParticle = usesSteeredParticle;
        shaderData->steeredParticleHasColor = steeredParticleHasColor;
        shaderData->usesReservedLight = usesGeneratedReservedLight;
        shaderData->usesCustomReservedLight = usesCustomReservedLight;
        shaderData->skeletalPaletteSize = skeletalPaletteSize;
        shaderData->skeletalMethod = usesSkeletal ? skeletalMethod : SKELETAL_SHADER_METHOD::NONE;
        return true;
    }

    bool SHADER::render(const BUFFER_GL *pBufferId, const RENDERIZABLE *renderizableOwner,
                        const int32_t subsetIndex, const float *skeletalPaletteRows,
                        const uint32_t skeletalPaletteFloatCount) const
    {
        const ScopedRenderizableContextD3D11 scopedRenderizableContext(renderizableOwner);
        if (!pBufferId)
            return false;
        BUFFER_SPECIFIC *buffer = pBufferId->getBackendBuffer();
        D3D11_SHADER_DATA *shaderData = static_cast<D3D11_SHADER_DATA *>(getBackendShaderSpecific());
        if (!buffer || !buffer->vertexBuffer || !shaderData || !shaderData->vertexShader || !shaderData->pixelShader)
            return false;
        SPECIFIC_AUX_CONTEXT_DEVICE *context = DEVICE::getInstance()->getSpecificContextDevice();
        if (!context->applyRasterizerState(pBufferId->mode_cull_face,
                                           pBufferId->mode_front_face_direction))
            return false;
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (!shaderData->vertexConstants.blocks.empty())
        {
            writeCustomConstantD3D11(shaderData->vertexConstants, "mvpMatrix",
                                     &SHADER::mvpMatrix, sizeof(MATRIX));
            writeCustomConstantD3D11(shaderData->vertexConstants, "mvMatrix",
                                     &SHADER::mvMatrixLightSpace, sizeof(MATRIX));
            if (!uploadCustomConstantsD3D11(context->immediateContext,
                                            shaderData->vertexConstants, false))
                return false;
        }
        else
        {
            if (FAILED(context->immediateContext->Map(shaderData->matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                return false;
            const D3D11_MATRIX_CONSTANTS matrices = { SHADER::mvpMatrix, SHADER::mvMatrixLightSpace };
            memcpy(mapped.pData, &matrices, sizeof(matrices));
            context->immediateContext->Unmap(shaderData->matrixBuffer, 0);
        }
        const bool usesSkeletal = shaderData->skeletalPaletteSize > 0;
        if (usesSkeletal)
        {
            if (!buffer->skinVertexBuffer || !shaderData->skeletalPaletteBuffer)
                return false;
            const uint32_t floatsPerBone = shaderData->skeletalMethod == SKELETAL_SHADER_METHOD::DQS_RIGID ? 8u : 12u;
            const uint32_t expectedFloatCount = shaderData->skeletalPaletteSize * floatsPerBone;
            std::vector<float> identityPalette;
            const float *palette = skeletalPaletteRows;
            if (!palette)
            {
                identityPalette.assign(expectedFloatCount, 0.0f);
                for (uint32_t bone = 0; bone < shaderData->skeletalPaletteSize; ++bone)
                {
                    if (shaderData->skeletalMethod == SKELETAL_SHADER_METHOD::DQS_RIGID)
                        identityPalette[(bone * 8u) + 3u] = 1.0f;
                    else
                    {
                        identityPalette[(bone * 12u) + 0u] = 1.0f;
                        identityPalette[(bone * 12u) + 5u] = 1.0f;
                        identityPalette[(bone * 12u) + 10u] = 1.0f;
                    }
                }
                palette = identityPalette.data();
            }
            else if (skeletalPaletteFloatCount != expectedFloatCount)
                return false;
            if (FAILED(context->immediateContext->Map(shaderData->skeletalPaletteBuffer, 0,
                                                      D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                return false;
            memcpy(mapped.pData, palette, expectedFloatCount * sizeof(float));
            context->immediateContext->Unmap(shaderData->skeletalPaletteBuffer, 0);
        }
        const UINT offset = 0;
        if (usesSkeletal)
        {
            ID3D11Buffer *vertexBuffers[2] = { buffer->vertexBuffer, buffer->skinVertexBuffer };
            const UINT strides[2] = { buffer->vertexStride, sizeof(skeletal::GPU_LBS_VERTEX) };
            const UINT offsets[2] = { 0, 0 };
            context->immediateContext->IASetVertexBuffers(0, 2, vertexBuffers, strides, offsets);
        }
        else
            context->immediateContext->IASetVertexBuffers(0, 1, &buffer->vertexBuffer, &buffer->vertexStride, &offset);
        context->immediateContext->IASetInputLayout(shaderData->inputLayout);
        context->immediateContext->IASetPrimitiveTopology(getTopology(pBufferId->mode_draw));
        context->immediateContext->VSSetShader(shaderData->vertexShader, nullptr, 0);
        if (shaderData->vertexConstants.blocks.empty())
            context->immediateContext->VSSetConstantBuffers(0, 1, &shaderData->matrixBuffer);
        if (usesSkeletal)
            context->immediateContext->VSSetConstantBuffers(1, 1, &shaderData->skeletalPaletteBuffer);
        context->immediateContext->PSSetShader(shaderData->pixelShader, nullptr, 0);
        if (shaderData->usesLineColor)
        {
            VAR_SHADER *color = pShader ? pShader->getVarByName("color") : nullptr;
            if (!color || FAILED(context->immediateContext->Map(shaderData->colorBuffer, 0,
                                                                D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                return false;
            memcpy(mapped.pData, color->current, sizeof(float) * 4u);
            context->immediateContext->Unmap(shaderData->colorBuffer, 0);
            context->immediateContext->PSSetConstantBuffers(0, 1, &shaderData->colorBuffer);
        }
        else
        {
            ID3D11SamplerState *selectedSampler = DEVICE::getInstance()->isPixelPerfectRendering() ?
                shaderData->nearestSampler : shaderData->defaultSampler;
            ID3D11SamplerState *samplers[6] = {
                selectedSampler, selectedSampler, selectedSampler,
                selectedSampler, selectedSampler, selectedSampler
            };
            context->immediateContext->PSSetSamplers(0, 6, samplers);
        }
        const uint32_t firstSubset = subsetIndex >= 0 ? static_cast<uint32_t>(subsetIndex) : 0u;
        const uint32_t lastSubset = subsetIndex >= 0 ? firstSubset + 1u : pBufferId->totalSubset;
        if (lastSubset > pBufferId->totalSubset)
            return false;
        for (uint32_t subset = firstSubset; subset < lastSubset; ++subset)
        {
            if (shaderData->usesReservedLight &&
                !uploadReservedLightConstantsD3D11(context->immediateContext, shaderData, pBufferId, subset))
                return false;
            if (shaderData->usesCustomReservedLight)
            {
                D3D11_LIGHT_CONSTANTS constants;
                buildReservedLightConstantsD3D11(constants, pBufferId, subset);
                if (!uploadCustomReservedLightConstantsD3D11(context->immediateContext,
                                                              shaderData->pixelConstants, constants, true) ||
                    !uploadCustomReservedLightConstantsD3D11(context->immediateContext,
                                                              shaderData->vertexConstants, constants, false))
                    return false;
            }
            if (!shaderData->usesLineColor)
            {
                ID3D11ShaderResourceView *textureViews[6] = {};
                for (uint32_t stage = 0; stage < 6; ++stage)
                {
                    TEXTURE *texture = pBufferId->getTextureByStage(stage, subset);
                    textureViews[stage] = texture ?
                        static_cast<ID3D11ShaderResourceView *>(texture->getBackendTexturePointer()) : nullptr;
                }
                context->immediateContext->PSSetShaderResources(0, 6, textureViews);
            }
            if (pBufferId->isIndexBuffer())
            {
                context->immediateContext->IASetIndexBuffer(buffer->indexBuffer, DXGI_FORMAT_R16_UINT, 0);
                context->immediateContext->DrawIndexed(static_cast<UINT>(pBufferId->indexCountIB[subset]),
                                                       static_cast<UINT>(pBufferId->indexStartIB[subset]), 0);
            }
            else
            {
                context->immediateContext->Draw(static_cast<UINT>(pBufferId->vertexCountVB[subset]),
                                                static_cast<UINT>(pBufferId->vertexStartVB[subset]));
            }
        }
        return true;
    }

    bool SHADER::renderDynamic(const BUFFER_GL *pBufferId,const VEC3 *vertex,const VEC3 *normal,const VEC2 *uv,
                               const RENDERIZABLE *renderizableOwner) const
    {
        if (!pBufferId)
            return false;
        BUFFER_GL *dynamicBuffer = const_cast<BUFFER_GL *>(pBufferId);
        const int vertexStart = 0;
        const int vertexCount = static_cast<int>(pBufferId->sizeOfArrayVertex);
        const int *vertexStarts = pBufferId->isIndexBuffer() ? &vertexStart : pBufferId->vertexStartVB;
        const int *vertexCounts = pBufferId->isIndexBuffer() ? &vertexCount : pBufferId->vertexCountVB;
        if (!dynamicBuffer->updateDynamic(vertex, normal, uv, vertexStarts, vertexCounts))
            return false;
        return render(pBufferId, renderizableOwner);
    }

    bool SHADER::renderParticle(const BUFFER_GL* pBufferId, const PARTICLE_CONTROL* particleControl) const
    {
        if (!pBufferId || !particleControl || pBufferId->mode_draw != util::MODE_DRAW_TRIANGLES)
            return false;
        BUFFER_SPECIFIC *buffer = pBufferId->getBackendBuffer();
        D3D11_SHADER_DATA *shaderData = static_cast<D3D11_SHADER_DATA *>(getBackendShaderSpecific());
        if (!buffer || !buffer->dynamicVertexBuffer || !buffer->vertexBuffer || !buffer->indexBuffer ||
            !shaderData || !shaderData->usesParticle || !shaderData->colorBuffer || !shaderData->defaultSampler)
            return false;

        SPECIFIC_AUX_CONTEXT_DEVICE *context = DEVICE::getInstance()->getSpecificContextDevice();
        if (!context->applyRasterizerState(pBufferId->mode_cull_face,
                                           pBufferId->mode_front_face_direction))
            return false;
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(context->immediateContext->Map(shaderData->matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            return false;
        memcpy(mapped.pData, &SHADER::mvpMatrix, sizeof(MATRIX));
        context->immediateContext->Unmap(shaderData->matrixBuffer, 0);

        const UINT offset = 0;
        context->immediateContext->IASetVertexBuffers(0, 1, &buffer->vertexBuffer, &buffer->vertexStride, &offset);
        context->immediateContext->IASetIndexBuffer(buffer->indexBuffer, DXGI_FORMAT_R16_UINT, 0);
        context->immediateContext->IASetInputLayout(shaderData->inputLayout);
        context->immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->immediateContext->VSSetShader(shaderData->vertexShader, nullptr, 0);
        context->immediateContext->VSSetConstantBuffers(0, 1, &shaderData->matrixBuffer);
        context->immediateContext->PSSetShader(shaderData->pixelShader, nullptr, 0);
        ID3D11SamplerState *particleSampler = DEVICE::getInstance()->isPixelPerfectRendering() ?
            shaderData->nearestSampler : shaderData->defaultSampler;
        context->immediateContext->PSSetSamplers(0, 1, &particleSampler);
        context->immediateContext->PSSetConstantBuffers(0, 1, &shaderData->colorBuffer);

        TEXTURE *texture = pBufferId->getTextureByStage(0, 0);
        ID3D11ShaderResourceView *textureView = texture ?
            static_cast<ID3D11ShaderResourceView *>(texture->getBackendTexturePointer()) : nullptr;
        context->immediateContext->PSSetShaderResources(0, 1, &textureView);

        ID3D11DepthStencilState *previousDepthState = nullptr;
        UINT previousStencilReference = 0;
        context->immediateContext->OMGetDepthStencilState(&previousDepthState, &previousStencilReference);
        context->immediateContext->OMSetDepthStencilState(context->depthDisabledState, 0);

        const VERTEX_UV *vertices = particleControl->getVertexBuffer();
        const ATT_PARTICLE *particles = particleControl->getAttParticle();
        const uint32_t totalAlive = particleControl->getTotalAlive();
        VAR_SHADER *alphaVar = pShader ? pShader->getVarByName("enableAlphaFromColor") : nullptr;
        const float enableAlphaFromColor = alphaVar ? alphaVar->current[0] : 1.0f;
        bool succeeded = vertices && particles;
        for (uint32_t i = 0; succeeded && i < totalAlive; ++i)
        {
            D3D11_VERTEX quad[4] = {};
            for (uint32_t vertex = 0; vertex < 4; ++vertex)
            {
                const VERTEX_UV &source = vertices[(i * 4u) + vertex];
                quad[vertex].position = VEC3(source.x, source.y, source.z);
                quad[vertex].normal = VEC3(0.0f, 0.0f, 1.0f);
                quad[vertex].uv = VEC2(source.u, source.v);
            }
            if (FAILED(context->immediateContext->Map(buffer->vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                succeeded = false;
                break;
            }
            memcpy(mapped.pData, quad, sizeof(quad));
            context->immediateContext->Unmap(buffer->vertexBuffer, 0);

            const ATT_PARTICLE &particle = particles[i];
            const float particleValues[8] = {
                particle.r, particle.g, particle.b, particle.a,
                enableAlphaFromColor, 0.0f, 0.0f, 0.0f
            };
            if (FAILED(context->immediateContext->Map(shaderData->colorBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                succeeded = false;
                break;
            }
            memcpy(mapped.pData, particleValues, sizeof(particleValues));
            context->immediateContext->Unmap(shaderData->colorBuffer, 0);
            context->immediateContext->DrawIndexed(6, 0, 0);
        }

        context->immediateContext->OMSetDepthStencilState(previousDepthState, previousStencilReference);
        if (previousDepthState)
            previousDepthState->Release();
        return succeeded;
    }

    bool SHADER::renderParticle(const BUFFER_GL* pBufferId, const FLUID_GROUP* pGroup) const
    {
        if (!pBufferId || !pGroup || pBufferId->mode_draw != util::MODE_DRAW_TRIANGLES)
            return false;
        BUFFER_SPECIFIC *buffer = pBufferId->getBackendBuffer();
        D3D11_SHADER_DATA *shaderData = static_cast<D3D11_SHADER_DATA *>(getBackendShaderSpecific());
        if (!buffer || !buffer->dynamicVertexBuffer || !buffer->vertexBuffer || !buffer->indexBuffer ||
            !shaderData || !shaderData->usesSteeredParticle || !shaderData->defaultSampler)
            return false;
        if (shaderData->steeredParticleHasColor && (!shaderData->colorBuffer || !pGroup->color))
            return false;

        SPECIFIC_AUX_CONTEXT_DEVICE *context = DEVICE::getInstance()->getSpecificContextDevice();
        if (!context->applyRasterizerState(pBufferId->mode_cull_face,
                                           pBufferId->mode_front_face_direction))
            return false;
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(context->immediateContext->Map(shaderData->matrixBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            return false;
        memcpy(mapped.pData, &SHADER::mvpMatrix, sizeof(MATRIX));
        context->immediateContext->Unmap(shaderData->matrixBuffer, 0);

        const UINT offset = 0;
        context->immediateContext->IASetVertexBuffers(0, 1, &buffer->vertexBuffer, &buffer->vertexStride, &offset);
        context->immediateContext->IASetIndexBuffer(buffer->indexBuffer, DXGI_FORMAT_R16_UINT, 0);
        context->immediateContext->IASetInputLayout(shaderData->inputLayout);
        context->immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->immediateContext->VSSetShader(shaderData->vertexShader, nullptr, 0);
        context->immediateContext->VSSetConstantBuffers(0, 1, &shaderData->matrixBuffer);
        context->immediateContext->PSSetShader(shaderData->pixelShader, nullptr, 0);
        ID3D11SamplerState *particleSampler = DEVICE::getInstance()->isPixelPerfectRendering() ?
            shaderData->nearestSampler : shaderData->defaultSampler;
        context->immediateContext->PSSetSamplers(0, 1, &particleSampler);

        TEXTURE *texture = pBufferId->getTextureByStage(0, 0);
        ID3D11ShaderResourceView *textureView = texture ?
            static_cast<ID3D11ShaderResourceView *>(texture->getBackendTexturePointer()) : nullptr;
        context->immediateContext->PSSetShaderResources(0, 1, &textureView);
        if (shaderData->steeredParticleHasColor)
        {
            const float colorValues[8] = {
                pGroup->color->r, pGroup->color->g, pGroup->color->b, pGroup->color->a,
                0.0f, 0.0f, 0.0f, 0.0f
            };
            if (FAILED(context->immediateContext->Map(shaderData->colorBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                return false;
            memcpy(mapped.pData, colorValues, sizeof(colorValues));
            context->immediateContext->Unmap(shaderData->colorBuffer, 0);
            context->immediateContext->PSSetConstantBuffers(0, 1, &shaderData->colorBuffer);
        }

        ID3D11DepthStencilState *previousDepthState = nullptr;
        UINT previousStencilReference = 0;
        context->immediateContext->OMGetDepthStencilState(&previousDepthState, &previousStencilReference);
        context->immediateContext->OMSetDepthStencilState(context->depthDisabledState, 0);
        bool succeeded = pGroup->vertex_particle && pGroup->uv;
        for (uint32_t i = 0; succeeded && i < pGroup->totalParticleToRender; ++i)
        {
            D3D11_VERTEX quad[4] = {};
            const VEC2 *uv = pGroup->segmented ? &pGroup->uv[i * 4u] : pGroup->uv;
            for (uint32_t vertex = 0; vertex < 4; ++vertex)
            {
                quad[vertex].position = pGroup->vertex_particle[(i * 4u) + vertex];
                quad[vertex].normal = VEC3(0.0f, 0.0f, 1.0f);
                quad[vertex].uv = uv[vertex];
            }
            if (FAILED(context->immediateContext->Map(buffer->vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
            {
                succeeded = false;
                break;
            }
            memcpy(mapped.pData, quad, sizeof(quad));
            context->immediateContext->Unmap(buffer->vertexBuffer, 0);
            context->immediateContext->DrawIndexed(6, 0, 0);
        }
        context->immediateContext->OMSetDepthStencilState(previousDepthState, previousStencilReference);
        if (previousDepthState)
            previousDepthState->Release();
        return succeeded;
    }
}

#endif // USE_DIRECTX11

