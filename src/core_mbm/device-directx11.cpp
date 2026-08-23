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
#include <device.h>
#include <texture-manager.h>
#include <audio-interface.h>
#include <mesh-manager.h>
#include <cstdio>

namespace mbm
{
    void DEVICE::initializeSpecificContext()
    {
        destroySpecificContext();
        setSpecificContextDevice(new SPECIFIC_AUX_CONTEXT_DEVICE());
    }

    void DEVICE::destroySpecificContext()
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getSpecificContextDevice();
        delete context;
        setSpecificContextDevice(nullptr);
    }

    void DEVICE::quit()
    {
        TEXTURE_MANAGER::release();
        MESH_MANAGER::release();
        releaseAudioManager();
        delete instanceDevice;
        instanceDevice = nullptr;
    }

    void DEVICE::setDepthTest(const bool enable)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getSpecificContextDevice();
        if (!context || !context->immediateContext)
            return;
        context->immediateContext->OMSetDepthStencilState(enable ? context->depthEnabledState : context->depthDisabledState, 0);
    }

    void DEVICE::clearDepth()
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getSpecificContextDevice();
        if (context && context->immediateContext && context->depthView)
            context->immediateContext->ClearDepthStencilView(context->depthView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    }

    void DEVICE::clearDepthColored()
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getSpecificContextDevice();
        if (!context || !context->immediateContext)
            return;
        const COLOR &background = getColorClearBackGround();
        const float color[4] = {background.r, background.g, background.b, background.a};
        if (context->backBufferView)
            context->immediateContext->ClearRenderTargetView(context->backBufferView, color);
        clearDepth();
    }

    const char *DEVICE::getBackendEngineName() const noexcept
    {
        return "DirectX11";
    }

    const char *DEVICE::getBackendEngineVersion() const noexcept
    {
        static char description[320] = "Direct3D 11";
        static bool initialized = false;
        if (initialized)
            return description;

        SPECIFIC_AUX_CONTEXT_DEVICE *context = getSpecificContextDevice();
        if (!context || !context->device)
            return description;
        initialized = true;

        const char *featureLevel = "unknown feature level";
        switch (context->device->GetFeatureLevel())
        {
            case D3D_FEATURE_LEVEL_9_1: featureLevel = "9_1"; break;
            case D3D_FEATURE_LEVEL_9_2: featureLevel = "9_2"; break;
            case D3D_FEATURE_LEVEL_9_3: featureLevel = "9_3"; break;
            case D3D_FEATURE_LEVEL_10_0: featureLevel = "10_0"; break;
            case D3D_FEATURE_LEVEL_10_1: featureLevel = "10_1"; break;
            case D3D_FEATURE_LEVEL_11_0: featureLevel = "11_0"; break;
            case D3D_FEATURE_LEVEL_11_1: featureLevel = "11_1"; break;
            default: break;
        }

        char adapterName[192] = "unknown adapter";
        IDXGIDevice *dxgiDevice = nullptr;
        IDXGIAdapter *adapter = nullptr;
        if (SUCCEEDED(context->device->QueryInterface(__uuidof(IDXGIDevice),
                                                      reinterpret_cast<void **>(&dxgiDevice))) &&
            dxgiDevice && SUCCEEDED(dxgiDevice->GetAdapter(&adapter)) && adapter)
        {
            DXGI_ADAPTER_DESC adapterDescription = {};
            if (SUCCEEDED(adapter->GetDesc(&adapterDescription)))
                WideCharToMultiByte(CP_UTF8, 0, adapterDescription.Description, -1,
                                    adapterName, static_cast<int>(sizeof(adapterName)), nullptr, nullptr);
        }
        if (adapter)
            adapter->Release();
        if (dxgiDevice)
            dxgiDevice->Release();

        std::snprintf(description, sizeof(description), "\nDirect3D 11 - Feature Level %s - %s",
                      featureLevel, adapterName);
        return description;
    }

    void DEVICE::setProjectionMode(const bool is3D, const float width, const float height)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getSpecificContextDevice();
        if (context && context->immediateContext && width > 0 && height > 0)
        {
            D3D11_VIEWPORT viewport = {};
            viewport.Width = width;
            viewport.Height = height;
            viewport.MaxDepth = 1.0f;
            context->immediateContext->RSSetViewports(1, &viewport);
        }
        if (width > 0)
            setBackBufferWidth(width);
        if (height > 0)
            setBackBufferHeight(height);
        if (width > 0 && height > 0)
            getCamera().updateCam(is3D, width, height);
    }

    const char *DEVICE::copyFileFromAsset(const char *assetName, const char *)
    {
        return assetName;
    }

    void DEVICE::disableFilteringForPixelPerfect() noexcept
    {
        setPixelPerfectRenderingActive(true);
    }

    void DEVICE::enableFilteringAfterPixelPerfect() noexcept
    {
        setPixelPerfectRenderingActive(false);
    }
}

#endif
