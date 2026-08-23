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
#ifndef DIRECTX11_SPECIFIC_CONTEXT_H
#define DIRECTX11_SPECIFIC_CONTEXT_H

#include <platform/win32-platform.h>
#include <core-exports.h>
#include <d3d11.h>
#include <d3d11sdklayers.h>
#include <dxgi.h>
#include <cstdint>

#if !defined(__MINGW32__)
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#endif

namespace mbm
{
    class CORE_MANAGER;

    struct SPECIFIC_AUX_CONTEXT_DEVICE
    {
        WINDOW window;
        DWORD idIcon;
        WIN_EVENT_BY_PASS *win32_EventByPass;
        WIN_JOYSTICK_BY_PASS *win32_joystickByPass;
        ID3D11Device *device;
        ID3D11DeviceContext *immediateContext;
        IDXGISwapChain *swapChain;
        ID3D11RenderTargetView *backBufferView;
        ID3D11Texture2D *depthTexture;
        ID3D11DepthStencilView *depthView;
        ID3D11DepthStencilState *depthEnabledState;
        ID3D11DepthStencilState *depthDisabledState;
        ID3D11BlendState *blendStates[11][5];
        ID3D11RasterizerState *rasterizerStates[3][2];
        int currentBlendState;
        int currentBlendOperation;
        D3D_FEATURE_LEVEL featureLevel;

        SPECIFIC_AUX_CONTEXT_DEVICE() noexcept;
        SPECIFIC_AUX_CONTEXT_DEVICE(const SPECIFIC_AUX_CONTEXT_DEVICE &) = delete;
        SPECIFIC_AUX_CONTEXT_DEVICE &operator=(const SPECIFIC_AUX_CONTEXT_DEVICE &) = delete;
        ~SPECIFIC_AUX_CONTEXT_DEVICE() noexcept;

        bool createBackBufferTargets(UINT width, UINT height) noexcept;
        void releaseBackBufferTargets() noexcept;
        void release() noexcept;
        void applyBlendState() noexcept;
        API_IMPL bool applyRasterizerState(uint32_t cullMode, uint32_t frontFaceDirection) noexcept;
        void initializeWin32Callbacks(CORE_MANAGER *coreManager);
    };
}

#endif
#endif
