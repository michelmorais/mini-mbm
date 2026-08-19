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
#include "directx9-skeletal-parity-tests.h"
#include "skeletal-parity-tests.h"

#include <skeletal-directx9-shader-source.h>
#include <specific-directx9-context.h>
#include <d3dx9-mingw.h>
#include <core_mbm/device.h>
#include <core_mbm/util-interface.h>

#include <cstring>
#include <cstdio>
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

    bool compileShader(const std::string &source, const char *profile, ID3DXBuffer **code,
                       ID3DXConstantTable **constants, std::string &error)
    {
        ID3DXBuffer *messages = nullptr;
        const HRESULT result = D3DXCompileShader(source.c_str(), static_cast<UINT>(source.size()), nullptr, nullptr,
                                                  "main", profile, D3DXSHADER_DEBUG, code, &messages, constants);
        if (FAILED(result))
        {
            error = "DirectX 9 skeletal parity shader compile failed";
            if (messages && messages->GetBufferPointer())
                error += std::string(": ") + static_cast<const char *>(messages->GetBufferPointer());
        }
        if (messages)
            messages->Release();
        return SUCCEEDED(result);
    }

    bool captureRgba8(const SKELETAL_PARITY_CASE &testCase, const SKELETAL_PARITY_ENCODING &encoding,
                      std::vector<uint8_t> &positionPixels, std::vector<uint8_t> &normalPixels,
                      std::string &error)
    {
        IDirect3DDevice9 *d3d = DEVICE::getInstance()->getSpecificContextDevice()->pd3dDevice;
        if (!d3d)
        {
            error = "DirectX 9 skeletal parity has no active device";
            return false;
        }

        std::string vertexSource =
            "struct VS_IN{float4 position:POSITION0;float3 normal:NORMAL0;float4 boneIndices:BLENDINDICES0;"
            "float4 boneWeights:BLENDWEIGHT0;float sample:TEXCOORD0;float2 corner:TEXCOORD1;};"
            "struct VS_OUT{float4 position:POSITION0;float3 valuePosition:TEXCOORD0;float3 valueNormal:TEXCOORD1;};";
        appendDirectX9SkeletalFunctions(vertexSource, testCase.paletteSize, testCase.method);
        vertexSource += "VS_OUT main(VS_IN input){VS_OUT output;";
        appendDirectX9SkeletalDeformation(vertexSource, testCase.method, true);
        vertexSource += "output.position=float4(input.sample+input.corner.x,input.corner.y,0,1);output.valuePosition=skinnedPosition.xyz;"
                        "output.valueNormal=skinnedNormal;return output;}";
        const std::string pixelSource =
            "float outputNormal;float3 encodeCenter;float encodeExtent;"
            "float4 main(float3 valuePosition:TEXCOORD0,float3 valueNormal:TEXCOORD1):COLOR0{"
            "float3 value=outputNormal>0.5?valueNormal*0.5+0.5:(valuePosition-encodeCenter)/(2*encodeExtent)+0.5;"
            "return float4(saturate(value),1);}";

        ID3DXBuffer *vertexCode = nullptr, *pixelCode = nullptr;
        ID3DXConstantTable *vertexConstants = nullptr, *pixelConstants = nullptr;
        IDirect3DVertexShader9 *vertexShader = nullptr;
        IDirect3DPixelShader9 *pixelShader = nullptr;
        IDirect3DVertexDeclaration9 *declaration = nullptr;
        IDirect3DSurface9 *target = nullptr, *oldTarget = nullptr, *oldDepth = nullptr;
        const UINT width = static_cast<UINT>(testCase.positions.size());
        std::vector<PARITY_VERTEX> vertices(width * 6);
        D3DVIEWPORT9 oldViewport = {};
        D3DVIEWPORT9 viewport = {0, 0, width, 1, 0.0f, 1.0f};
        DWORD oldCullMode = D3DCULL_CCW;
        DWORD oldZEnable = D3DZB_TRUE;
        bool success = false;

        if (!compileShader(vertexSource, "vs_3_0", &vertexCode, &vertexConstants, error) ||
            !compileShader(pixelSource, "ps_3_0", &pixelCode, &pixelConstants, error))
            goto cleanup;
        if (FAILED(d3d->CreateVertexShader(static_cast<const DWORD *>(vertexCode->GetBufferPointer()), &vertexShader)) ||
            FAILED(d3d->CreatePixelShader(static_cast<const DWORD *>(pixelCode->GetBufferPointer()), &pixelShader)))
        {
            error = "DirectX 9 skeletal parity could not create shaders";
            goto cleanup;
        }
        {
            const D3DVERTEXELEMENT9 elements[] = {
                {0, static_cast<WORD>(offsetof(PARITY_VERTEX, position)), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
                {0, static_cast<WORD>(offsetof(PARITY_VERTEX, normal)), D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
                {0, static_cast<WORD>(offsetof(PARITY_VERTEX, boneIndices)), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0},
                {0, static_cast<WORD>(offsetof(PARITY_VERTEX, boneWeights)), D3DDECLTYPE_FLOAT4, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT, 0},
                {0, static_cast<WORD>(offsetof(PARITY_VERTEX, sample)), D3DDECLTYPE_FLOAT1, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
                {0, static_cast<WORD>(offsetof(PARITY_VERTEX, corner)), D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 1},
                D3DDECL_END()};
            if (FAILED(d3d->CreateVertexDeclaration(elements, &declaration)))
            {
                error = "DirectX 9 skeletal parity could not create vertex declaration";
                goto cleanup;
            }
        }

        if (FAILED(d3d->CreateRenderTarget(width, 1, D3DFMT_A8R8G8B8, D3DMULTISAMPLE_NONE,
                                           0, TRUE, &target, nullptr)))
        {
            error = "DirectX 9 skeletal parity could not create readback surfaces";
            goto cleanup;
        }

        const float corners[6][2] = {{-1.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f},
                                     {-1.0f, 1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}};
        for (UINT index = 0; index < width; ++index)
        {
            const VEC3 &position = testCase.positions[index];
            const VEC3 &normal = testCase.normals[index];
            for (UINT corner = 0; corner < 6; ++corner)
            {
                PARITY_VERTEX &vertex = vertices[index * 6 + corner];
                vertex.position[0] = position.x; vertex.position[1] = position.y;
                vertex.position[2] = position.z; vertex.position[3] = 1.0f;
                vertex.normal[0] = normal.x; vertex.normal[1] = normal.y; vertex.normal[2] = normal.z;
                for (UINT influence = 0; influence < 4; ++influence)
                {
                    vertex.boneIndices[influence] = testCase.influences[index].boneIndex[influence];
                    vertex.boneWeights[influence] = testCase.influences[index].weight[influence];
                }
                vertex.sample = static_cast<float>(index * 2) / static_cast<float>(width) - 1.0f;
                vertex.corner[0] = corners[corner][0] / static_cast<float>(width);
                vertex.corner[1] = corners[corner][1];
            }
        }

        d3d->GetRenderTarget(0, &oldTarget);
        d3d->GetDepthStencilSurface(&oldDepth);
        d3d->GetViewport(&oldViewport);
        d3d->GetRenderState(D3DRS_CULLMODE, &oldCullMode);
        d3d->GetRenderState(D3DRS_ZENABLE, &oldZEnable);
        if (FAILED(d3d->SetRenderTarget(0, target)) || FAILED(d3d->SetDepthStencilSurface(nullptr)) ||
            FAILED(d3d->SetViewport(&viewport)) || FAILED(d3d->SetVertexDeclaration(declaration)) ||
            FAILED(d3d->SetVertexShader(vertexShader)) || FAILED(d3d->SetPixelShader(pixelShader)) ||
            FAILED(d3d->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE)) ||
            FAILED(d3d->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE)))
        {
            error = "DirectX 9 skeletal parity could not bind capture state";
            goto restore;
        }
        vertexConstants->SetFloatArray(d3d, vertexConstants->GetConstantByName(nullptr, "bonePalette"),
                                       testCase.palette.data(), static_cast<UINT>(testCase.palette.size()));
        pixelConstants->SetFloatArray(d3d, pixelConstants->GetConstantByName(nullptr, "encodeCenter"),
                                      &encoding.positionCenter.x, 3);
        pixelConstants->SetFloat(d3d, pixelConstants->GetConstantByName(nullptr, "encodeExtent"), encoding.positionExtent);
        positionPixels.resize(width * 4);
        normalPixels.resize(width * 4);
        for (UINT normalPass = 0; normalPass < 2; ++normalPass)
        {
            pixelConstants->SetFloat(d3d, pixelConstants->GetConstantByName(nullptr, "outputNormal"), static_cast<float>(normalPass));
            d3d->Clear(0, nullptr, D3DCLEAR_TARGET, 0, 1.0f, 0);
            const HRESULT beginResult = d3d->BeginScene();
            if (FAILED(beginResult))
            {
                error = "DirectX 9 skeletal parity could not begin capture scene";
                goto restore;
            }
            const HRESULT drawResult = d3d->DrawPrimitiveUP(D3DPT_TRIANGLELIST, width * 2,
                                                             vertices.data(), sizeof(PARITY_VERTEX));
            d3d->EndScene();
            if (FAILED(drawResult))
            {
                char resultText[96] = {};
                std::snprintf(resultText, sizeof(resultText),
                              "DirectX 9 skeletal parity draw failed (HRESULT=0x%08lx)",
                              static_cast<unsigned long>(drawResult));
                error = resultText;
                goto restore;
            }
            D3DLOCKED_RECT locked = {};
            if (FAILED(target->LockRect(&locked, nullptr, D3DLOCK_READONLY)))
            {
                error = "DirectX 9 skeletal parity readback lock failed";
                goto restore;
            }
            std::vector<uint8_t> &pixels = normalPass ? normalPixels : positionPixels;
            const uint8_t *bgra = static_cast<const uint8_t *>(locked.pBits);
            for (UINT index = 0; index < width; ++index)
            {
                pixels[index * 4] = bgra[index * 4 + 2]; pixels[index * 4 + 1] = bgra[index * 4 + 1];
                pixels[index * 4 + 2] = bgra[index * 4]; pixels[index * 4 + 3] = bgra[index * 4 + 3];
            }
            target->UnlockRect();
        }
        success = true;

restore:
        if (oldTarget) d3d->SetRenderTarget(0, oldTarget);
        d3d->SetDepthStencilSurface(oldDepth);
        d3d->SetViewport(&oldViewport);
        d3d->SetRenderState(D3DRS_CULLMODE, oldCullMode);
        d3d->SetRenderState(D3DRS_ZENABLE, oldZEnable);
cleanup:
        if (oldDepth) oldDepth->Release(); if (oldTarget) oldTarget->Release();
        if (target) target->Release();
        if (declaration) declaration->Release(); if (pixelShader) pixelShader->Release(); if (vertexShader) vertexShader->Release();
        if (pixelConstants) pixelConstants->Release(); if (vertexConstants) vertexConstants->Release();
        if (pixelCode) pixelCode->Release(); if (vertexCode) vertexCode->Release();
        return success;
    }
}

bool runDirectX9SkeletalParityTests()
{
    std::string error;
    if (mbm::skeletal::test::runSkeletalParitySuite("DirectX 9", captureRgba8, error))
        return true;
    ERROR_LOG("testLib: %s", error.c_str());
    return false;
}
