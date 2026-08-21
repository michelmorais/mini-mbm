/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include "specific-directx11-context.h"
#include <core-manager.h>
#include <util-interface.h>
#include <draw-compatibility.h>

namespace
{
    template <typename T>
    void releaseCom(T *&value) noexcept
    {
        if (value)
            value->Release();
        value = nullptr;
    }
}

namespace mbm
{
    SPECIFIC_AUX_CONTEXT_DEVICE::SPECIFIC_AUX_CONTEXT_DEVICE() noexcept :
        idIcon(0),
        win32_EventByPass(nullptr),
        win32_joystickByPass(nullptr),
        device(nullptr),
        immediateContext(nullptr),
        swapChain(nullptr),
        backBufferView(nullptr),
        depthTexture(nullptr),
        depthView(nullptr),
        depthEnabledState(nullptr),
        depthDisabledState(nullptr),
        currentBlendState(0),
        currentBlendOperation(1),
        featureLevel(D3D_FEATURE_LEVEL_10_0)
    {
        for (auto &statesByOperation : blendStates)
            for (ID3D11BlendState *&state : statesByOperation)
                state = nullptr;
        for (auto &statesByDirection : rasterizerStates)
            for (ID3D11RasterizerState *&state : statesByDirection)
                state = nullptr;
    }

    SPECIFIC_AUX_CONTEXT_DEVICE::~SPECIFIC_AUX_CONTEXT_DEVICE() noexcept
    {
        release();
        delete win32_EventByPass;
        win32_EventByPass = nullptr;
        delete win32_joystickByPass;
        win32_joystickByPass = nullptr;
    }

    bool SPECIFIC_AUX_CONTEXT_DEVICE::createBackBufferTargets(const UINT width, const UINT height) noexcept
    {
        if (!device || !immediateContext || !swapChain || width == 0 || height == 0)
            return false;

        ID3D11Texture2D *backBuffer = nullptr;
        HRESULT result = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&backBuffer));
        if (SUCCEEDED(result))
            result = device->CreateRenderTargetView(backBuffer, nullptr, &backBufferView);
        releaseCom(backBuffer);
        if (FAILED(result))
            return false;

        D3D11_TEXTURE2D_DESC depthDescription = {};
        depthDescription.Width = width;
        depthDescription.Height = height;
        depthDescription.MipLevels = 1;
        depthDescription.ArraySize = 1;
        depthDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDescription.SampleDesc.Count = 1;
        depthDescription.Usage = D3D11_USAGE_DEFAULT;
        depthDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        result = device->CreateTexture2D(&depthDescription, nullptr, &depthTexture);
        if (SUCCEEDED(result))
            result = device->CreateDepthStencilView(depthTexture, nullptr, &depthView);
        if (FAILED(result))
        {
            releaseBackBufferTargets();
            return false;
        }

        immediateContext->OMSetRenderTargets(1, &backBufferView, depthView);
        D3D11_VIEWPORT viewport = {};
        viewport.Width = static_cast<float>(width);
        viewport.Height = static_cast<float>(height);
        viewport.MaxDepth = 1.0f;
        immediateContext->RSSetViewports(1, &viewport);
        return true;
    }

    void SPECIFIC_AUX_CONTEXT_DEVICE::releaseBackBufferTargets() noexcept
    {
        if (immediateContext)
            immediateContext->OMSetRenderTargets(0, nullptr, nullptr);
        releaseCom(depthView);
        releaseCom(depthTexture);
        releaseCom(backBufferView);
    }

    void SPECIFIC_AUX_CONTEXT_DEVICE::release() noexcept
    {
        releaseBackBufferTargets();
        for (auto &statesByOperation : blendStates)
            for (ID3D11BlendState *&state : statesByOperation)
                releaseCom(state);
        for (auto &statesByDirection : rasterizerStates)
            for (ID3D11RasterizerState *&state : statesByDirection)
                releaseCom(state);
        releaseCom(depthDisabledState);
        releaseCom(depthEnabledState);
        releaseCom(swapChain);
        if (immediateContext)
            immediateContext->ClearState();
        releaseCom(immediateContext);
#if defined(_DEBUG)
        ID3D11Debug *debug = nullptr;
        if (device && SUCCEEDED(device->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void **>(&debug))))
        {
            debug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY);
            debug->Release();
        }
#endif
        releaseCom(device);
    }

    void SPECIFIC_AUX_CONTEXT_DEVICE::applyBlendState() noexcept
    {
        if (!device || !immediateContext || currentBlendState < 0 || currentBlendState >= 11 ||
            currentBlendOperation < 1 || currentBlendOperation > 5)
            return;

        ID3D11BlendState *&state = blendStates[currentBlendState][currentBlendOperation - 1];
        if (!state)
        {
            static const D3D11_BLEND destinationBlend[] = {
                D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_SRC_COLOR,
                D3D11_BLEND_INV_SRC_COLOR, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA,
                D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_DEST_COLOR,
                D3D11_BLEND_INV_DEST_COLOR
            };
            static const D3D11_BLEND_OP blendOperation[] = {
                D3D11_BLEND_OP_ADD, D3D11_BLEND_OP_SUBTRACT, D3D11_BLEND_OP_REV_SUBTRACT,
                D3D11_BLEND_OP_MIN, D3D11_BLEND_OP_MAX
            };
            D3D11_BLEND_DESC description = {};
            D3D11_RENDER_TARGET_BLEND_DESC &target = description.RenderTarget[0];
            target.BlendEnable = TRUE;
            target.SrcBlend = D3D11_BLEND_SRC_ALPHA;
            target.DestBlend = destinationBlend[currentBlendState];
            target.BlendOp = blendOperation[currentBlendOperation - 1];
            target.SrcBlendAlpha = D3D11_BLEND_ONE;
            target.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            if (FAILED(device->CreateBlendState(&description, &state)))
                return;
        }
        immediateContext->OMSetBlendState(state, nullptr, 0xffffffffu);
    }

    bool SPECIFIC_AUX_CONTEXT_DEVICE::applyRasterizerState(const uint32_t cullMode,
                                                            const uint32_t frontFaceDirection) noexcept
    {
        if (!device || !immediateContext)
            return false;
        uint32_t cullIndex = 2u;
        D3D11_CULL_MODE direct3dCullMode = D3D11_CULL_NONE;
        if (cullMode == util::CULL_FRONT)
        {
            cullIndex = 0u;
            direct3dCullMode = D3D11_CULL_FRONT;
        }
        else if (cullMode == util::CULL_BACK)
        {
            cullIndex = 1u;
            direct3dCullMode = D3D11_CULL_BACK;
        }
        const uint32_t directionIndex = frontFaceDirection == util::CCW ? 1u : 0u;
        ID3D11RasterizerState *&state = rasterizerStates[cullIndex][directionIndex];
        if (!state)
        {
            D3D11_RASTERIZER_DESC description = {};
            description.FillMode = D3D11_FILL_SOLID;
            description.CullMode = direct3dCullMode;
            description.FrontCounterClockwise = directionIndex == 1u ? TRUE : FALSE;
            description.DepthClipEnable = TRUE;
            if (FAILED(device->CreateRasterizerState(&description, &state)))
                return false;
        }
        immediateContext->RSSetState(state);
        return true;
    }

    void SPECIFIC_AUX_CONTEXT_DEVICE::initializeWin32Callbacks(CORE_MANAGER *coreManager)
    {
        delete win32_EventByPass;
        delete win32_joystickByPass;
        win32_EventByPass = new WIN_EVENT_BY_PASS(coreManager ? reinterpret_cast<EVENTS *>(coreManager) : nullptr);
        win32_joystickByPass = new WIN_JOYSTICK_BY_PASS(coreManager ? reinterpret_cast<JOYSTICK_BASE *>(coreManager) : nullptr);
    }
}

#endif
