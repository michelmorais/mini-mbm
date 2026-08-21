/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------*/
#if defined(USE_DIRECTX11)

#include "directx11-skeletal-parity-tests.h"
#include "skeletal-parity-tests.h"

#include <skeletal-directx9-shader-source.h>
#include <specific-directx11-context.h>
#include <core_mbm/device.h>
#include <core_mbm/util-interface.h>
#include <d3dcompiler.h>

#include <cstring>
#include <string>
#include <vector>

namespace
{
    using namespace mbm;
    using namespace mbm::skeletal;
    using namespace mbm::skeletal::test;

    struct PARITY_VERTEX
    {
        float position[4];
        float normal[3];
        float boneIndices[4];
        float boneWeights[4];
        float sample;
        float corner[2];
    };

    void releaseUnknown(IUnknown *object)
    {
        if (object)
            object->Release();
    }

    bool compileShader(const std::string &source, const char *entry, const char *profile,
                       ID3DBlob **code, std::string &error)
    {
        ID3DBlob *messages = nullptr;
        const HRESULT result = D3DCompile(source.c_str(), source.size(), "directx11-skeletal-parity",
                                          nullptr, nullptr, entry, profile,
                                          D3DCOMPILE_ENABLE_STRICTNESS, 0, code, &messages);
        if (FAILED(result))
        {
            error = "DirectX 11 skeletal parity shader compile failed";
            if (messages && messages->GetBufferPointer())
                error += std::string(": ") + static_cast<const char *>(messages->GetBufferPointer());
        }
        releaseUnknown(messages);
        return SUCCEEDED(result);
    }

    bool captureRgba8(const SKELETAL_PARITY_CASE &testCase, const SKELETAL_PARITY_ENCODING &encoding,
                      std::vector<uint8_t> &positionPixels, std::vector<uint8_t> &normalPixels,
                      std::string &error)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = DEVICE::getInstance()->getSpecificContextDevice();
        if (!context || !context->device || !context->immediateContext)
        {
            error = "DirectX 11 skeletal parity has no active device";
            return false;
        }
        ID3D11Device *device = context->device;
        ID3D11DeviceContext *immediate = context->immediateContext;
        std::string vertexSource =
            "struct VS_IN{float4 position:POSITION0;float3 normal:NORMAL0;float4 boneIndices:BLENDINDICES0;"
            "float4 boneWeights:BLENDWEIGHT0;float sample:TEXCOORD0;float2 corner:TEXCOORD1;};"
            "struct VS_OUT{float4 position:SV_POSITION;float3 valuePosition:TEXCOORD0;float3 valueNormal:TEXCOORD1;};";
        appendDirectX9SkeletalFunctions(vertexSource, testCase.paletteSize, testCase.method);
        vertexSource += "VS_OUT main(VS_IN input){VS_OUT output;";
        appendDirectX9SkeletalDeformation(vertexSource, testCase.method, true);
        vertexSource += "output.position=float4(input.sample+input.corner.x,input.corner.y,0,1);"
                        "output.valuePosition=skinnedPosition.xyz;output.valueNormal=skinnedNormal;return output;}";
        const std::string pixelSource =
            "cbuffer Encode:register(b0){float4 centerExtent;float4 options;}"
            "float4 main(float4 position:SV_POSITION,float3 valuePosition:TEXCOORD0,float3 valueNormal:TEXCOORD1):SV_TARGET{"
            "float3 value=options.x>0.5?valueNormal*0.5+0.5:(valuePosition-centerExtent.xyz)/(2*centerExtent.w)+0.5;"
            "return float4(saturate(value),1);}";

        ID3DBlob *vertexCode = nullptr, *pixelCode = nullptr;
        ID3D11VertexShader *vertexShader = nullptr;
        ID3D11PixelShader *pixelShader = nullptr;
        ID3D11InputLayout *layout = nullptr;
        ID3D11Buffer *vertexBuffer = nullptr, *paletteBuffer = nullptr, *encodeBuffer = nullptr;
        ID3D11RasterizerState *captureRasterizer = nullptr, *oldRasterizer = nullptr;
        ID3D11Texture2D *targetTexture = nullptr, *stagingTexture = nullptr;
        ID3D11RenderTargetView *targetView = nullptr, *oldTarget = nullptr;
        ID3D11DepthStencilView *oldDepth = nullptr;
        D3D11_VIEWPORT oldViewport = {};
        UINT oldViewportCount = 1;
        bool success = false;
        const UINT width = static_cast<UINT>(testCase.positions.size());
        std::vector<PARITY_VERTEX> vertices(width * 6u);

        if (!compileShader(vertexSource, "main", "vs_4_0", &vertexCode, error) ||
            !compileShader(pixelSource, "main", "ps_4_0", &pixelCode, error))
            goto cleanup;
        if (FAILED(device->CreateVertexShader(vertexCode->GetBufferPointer(), vertexCode->GetBufferSize(), nullptr, &vertexShader)) ||
            FAILED(device->CreatePixelShader(pixelCode->GetBufferPointer(), pixelCode->GetBufferSize(), nullptr, &pixelShader)))
        {
            error = "DirectX 11 skeletal parity could not create shaders";
            goto cleanup;
        }
        {
            const D3D11_INPUT_ELEMENT_DESC elements[] = {
                {"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(PARITY_VERTEX,position),D3D11_INPUT_PER_VERTEX_DATA,0},
                {"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,offsetof(PARITY_VERTEX,normal),D3D11_INPUT_PER_VERTEX_DATA,0},
                {"BLENDINDICES",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(PARITY_VERTEX,boneIndices),D3D11_INPUT_PER_VERTEX_DATA,0},
                {"BLENDWEIGHT",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,offsetof(PARITY_VERTEX,boneWeights),D3D11_INPUT_PER_VERTEX_DATA,0},
                {"TEXCOORD",0,DXGI_FORMAT_R32_FLOAT,0,offsetof(PARITY_VERTEX,sample),D3D11_INPUT_PER_VERTEX_DATA,0},
                {"TEXCOORD",1,DXGI_FORMAT_R32G32_FLOAT,0,offsetof(PARITY_VERTEX,corner),D3D11_INPUT_PER_VERTEX_DATA,0}};
            if (FAILED(device->CreateInputLayout(elements, 6, vertexCode->GetBufferPointer(),
                                                  vertexCode->GetBufferSize(), &layout)))
            {
                error = "DirectX 11 skeletal parity could not create input layout";
                goto cleanup;
            }
        }
        {
            const float corners[6][2] = {{-1,-1},{1,-1},{-1,1},{-1,1},{1,-1},{1,1}};
            for (UINT index = 0; index < width; ++index)
                for (UINT corner = 0; corner < 6; ++corner)
                {
                    PARITY_VERTEX &vertex = vertices[index * 6u + corner];
                    const VEC3 &position = testCase.positions[index];
                    const VEC3 &normal = testCase.normals[index];
                    vertex.position[0]=position.x; vertex.position[1]=position.y; vertex.position[2]=position.z; vertex.position[3]=1;
                    vertex.normal[0]=normal.x; vertex.normal[1]=normal.y; vertex.normal[2]=normal.z;
                    memcpy(vertex.boneIndices, testCase.influences[index].boneIndex, sizeof(vertex.boneIndices));
                    memcpy(vertex.boneWeights, testCase.influences[index].weight, sizeof(vertex.boneWeights));
                    vertex.sample=static_cast<float>((index*2u)+1u)/static_cast<float>(width)-1.0f;
                    vertex.corner[0]=corners[corner][0]/static_cast<float>(width); vertex.corner[1]=corners[corner][1];
                }
        }
        {
            D3D11_BUFFER_DESC bufferDescription = {};
            D3D11_SUBRESOURCE_DATA initialData = {};
            bufferDescription.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(PARITY_VERTEX));
            bufferDescription.Usage = D3D11_USAGE_IMMUTABLE; bufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            initialData.pSysMem = vertices.data();
            if (FAILED(device->CreateBuffer(&bufferDescription, &initialData, &vertexBuffer))) goto resource_failure;
            bufferDescription.ByteWidth = static_cast<UINT>(testCase.palette.size() * sizeof(float));
            bufferDescription.BindFlags = D3D11_BIND_CONSTANT_BUFFER; initialData.pSysMem = testCase.palette.data();
            if (FAILED(device->CreateBuffer(&bufferDescription, &initialData, &paletteBuffer))) goto resource_failure;
            const float encodeValues[8] = {encoding.positionCenter.x,encoding.positionCenter.y,encoding.positionCenter.z,
                                            encoding.positionExtent,0,0,0,0};
            bufferDescription.ByteWidth = sizeof(encodeValues); bufferDescription.Usage = D3D11_USAGE_DEFAULT;
            initialData.pSysMem = encodeValues;
            if (FAILED(device->CreateBuffer(&bufferDescription, &initialData, &encodeBuffer))) goto resource_failure;
        }
        {
            D3D11_TEXTURE2D_DESC textureDescription = {};
            textureDescription.Width=width; textureDescription.Height=1; textureDescription.MipLevels=1;
            textureDescription.ArraySize=1; textureDescription.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
            textureDescription.SampleDesc.Count=1; textureDescription.Usage=D3D11_USAGE_DEFAULT;
            textureDescription.BindFlags=D3D11_BIND_RENDER_TARGET;
            if (FAILED(device->CreateTexture2D(&textureDescription,nullptr,&targetTexture)) ||
                FAILED(device->CreateRenderTargetView(targetTexture,nullptr,&targetView))) goto resource_failure;
            textureDescription.Usage=D3D11_USAGE_STAGING; textureDescription.BindFlags=0;
            textureDescription.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            if (FAILED(device->CreateTexture2D(&textureDescription,nullptr,&stagingTexture))) goto resource_failure;
        }
        {
            D3D11_RASTERIZER_DESC rasterizerDescription = {};
            rasterizerDescription.FillMode = D3D11_FILL_SOLID;
            rasterizerDescription.CullMode = D3D11_CULL_NONE;
            rasterizerDescription.DepthClipEnable = TRUE;
            if (FAILED(device->CreateRasterizerState(&rasterizerDescription, &captureRasterizer))) goto resource_failure;
        }
        immediate->OMGetRenderTargets(1,&oldTarget,&oldDepth);
        immediate->RSGetState(&oldRasterizer);
        immediate->RSGetViewports(&oldViewportCount,&oldViewport);
        {
            D3D11_VIEWPORT viewport = {0,0,static_cast<float>(width),1.0f,0,1};
            const UINT stride=sizeof(PARITY_VERTEX), offset=0;
            immediate->OMSetRenderTargets(1,&targetView,nullptr); immediate->RSSetViewports(1,&viewport);
            immediate->RSSetState(captureRasterizer);
            immediate->IASetInputLayout(layout); immediate->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            immediate->IASetVertexBuffers(0,1,&vertexBuffer,&stride,&offset);
            immediate->VSSetShader(vertexShader,nullptr,0); immediate->VSSetConstantBuffers(0,1,&paletteBuffer);
            immediate->PSSetShader(pixelShader,nullptr,0); immediate->PSSetConstantBuffers(0,1,&encodeBuffer);
            positionPixels.resize(width*4u); normalPixels.resize(width*4u);
            for (UINT normalPass=0; normalPass<2; ++normalPass)
            {
                const float clear[4]={0,0,0,0}; immediate->ClearRenderTargetView(targetView,clear);
                const float encodeValues[8]={encoding.positionCenter.x,encoding.positionCenter.y,encoding.positionCenter.z,
                                              encoding.positionExtent,static_cast<float>(normalPass),0,0,0};
                immediate->UpdateSubresource(encodeBuffer,0,nullptr,encodeValues,0,0);
                immediate->Draw(static_cast<UINT>(vertices.size()),0);
                immediate->CopyResource(stagingTexture,targetTexture);
                D3D11_MAPPED_SUBRESOURCE mapped = {};
                if (FAILED(immediate->Map(stagingTexture,0,D3D11_MAP_READ,0,&mapped))) goto restore;
                std::vector<uint8_t> &pixels=normalPass?normalPixels:positionPixels;
                memcpy(pixels.data(),mapped.pData,width*4u); immediate->Unmap(stagingTexture,0);
            }
        }
        success=true;
        goto restore;
resource_failure:
        error = "DirectX 11 skeletal parity could not create GPU resources";
restore:
        immediate->OMSetRenderTargets(1,&oldTarget,oldDepth);
        immediate->RSSetState(oldRasterizer);
        if (oldViewportCount) immediate->RSSetViewports(1,&oldViewport);
cleanup:
        releaseUnknown(oldRasterizer); releaseUnknown(captureRasterizer); releaseUnknown(oldDepth); releaseUnknown(oldTarget);
        releaseUnknown(stagingTexture); releaseUnknown(targetView);
        releaseUnknown(targetTexture); releaseUnknown(encodeBuffer); releaseUnknown(paletteBuffer); releaseUnknown(vertexBuffer);
        releaseUnknown(layout); releaseUnknown(pixelShader); releaseUnknown(vertexShader); releaseUnknown(pixelCode); releaseUnknown(vertexCode);
        return success;
    }
}

bool runDirectX11SkeletalParityTests()
{
    std::string error;
    if (mbm::skeletal::test::runSkeletalParitySuite("DirectX 11", captureRgba8, error))
        return true;
    ERROR_LOG("testLib: %s", error.c_str());
    return false;
}
#endif
