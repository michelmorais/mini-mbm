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
#include "skeletal-render-capability.h"
#include "specific-directx11-render-target.h"
#include <core-manager.h>
#include <device.h>
#include <scene.h>
#include <renderizable.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <util-interface.h>
#include <audio-interface.h>
#include <miniz-wrap/miniz-wrap.h>
#include <plugin-callback.h>
#include <log-util.h>
#include <core_mbm/platform-win32.h>

namespace mbm
{
    void CORE_MANAGER::handleEventFromWindow()
    {
        DEVICE *device = getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        context->window.doEvents();
        bool firstMenu = true;
        while (WINDOW::isAnyMenuVisible() && context->window.run)
        {
            if (firstMenu)
            {
                Sleep(50);
                WINDOW::refreshMenu();
            }
            context->window.doEvents();
            firstMenu = false;
        }
        if (!context->window.run)
        {
            device->setRun(false);
            return;
        }
        INFO_JOYSTICK_INIT_PLAYER info;
        while (popEvent(&info))
        {
            SCENE *scene = device->getScene();
            if (scene && isSceneInitialized())
                scene->onInfoDeviceJoystick(info.player, info.maxNumberButton, info.deviceName.c_str(), info.extraInfo.c_str());
        }
    }

    CORE_MANAGER::CORE_MANAGER()
    {
        initializeImpl();
        setDevice(DEVICE::getInstance());
        setChangeScene(true);
        setSceneInitialized(false);
        setKeyCapsLockState(false);
        getDevice()->getSpecificContextDevice()->initializeWin32Callbacks(this);
    }

    void CORE_MANAGER::moveWindow(int x, int y)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getDevice()->getSpecificContextDevice();
        HWND handle = context->window.getHwnd();
        if (handle)
            SetWindowPos(handle, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void CORE_MANAGER::ReleaseGraphics(const bool)
    {
        TEXTURE_MANAGER::getInstance()->release();
        MESH_MANAGER::getInstance()->release();
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getDevice()->getSpecificContextDevice();
        context->window.setCallEventsManager(nullptr);
        if (context->win32_joystickByPass)
            context->win32_joystickByPass->releaseJoystick(&context->window);
        context->release();
    }

    bool CORE_MANAGER::initGraphics(const char *nameApplication, int width, int height, const int px, const int py,
                                    const bool border, const bool enableResize)
    {
        DEVICE *engineDevice = getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE *context = engineDevice->getSpecificContextDevice();
        engineDevice->setWindowPosition(px, py);
        setNameApplication(nameApplication);
        context->window.setNameAplication(nameApplication);
        if (!context->window.init(nameApplication, width, height, px, py, enableResize, enableResize, enableResize,
                                  false, nullptr, !border, context->idIcon, false))
            return false;
        context->window.setMinSizeAllowed(800, 600);
        const HWND windowHandle = context->window.getHwnd();
        RECT client = {};
        if (GetClientRect(windowHandle, &client))
        {
            width = client.right - client.left;
            height = client.bottom - client.top;
        }
        if (context->win32_EventByPass)
            context->window.setCallEventsManager(context->win32_EventByPass);
        if (context->win32_joystickByPass)
            context->win32_joystickByPass->initJoystick(&context->window);

        DXGI_SWAP_CHAIN_DESC swapDescription = {};
        swapDescription.BufferDesc.Width = static_cast<UINT>(width);
        swapDescription.BufferDesc.Height = static_cast<UINT>(height);
        swapDescription.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapDescription.SampleDesc.Count = 1;
        swapDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapDescription.BufferCount = 2;
        swapDescription.OutputWindow = windowHandle;
        swapDescription.Windowed = TRUE;
        swapDescription.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

        const D3D_FEATURE_LEVEL requestedLevels[] = {
            D3D_FEATURE_LEVEL_11_0,
            D3D_FEATURE_LEVEL_10_1,
            D3D_FEATURE_LEVEL_10_0};
        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#if defined(_DEBUG)
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        HRESULT result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            requestedLevels, static_cast<UINT>(sizeof(requestedLevels) / sizeof(requestedLevels[0])),
            D3D11_SDK_VERSION, &swapDescription, &context->swapChain, &context->device,
            &context->featureLevel, &context->immediateContext);
#if defined(_DEBUG)
        if (result == DXGI_ERROR_SDK_COMPONENT_MISSING)
        {
            flags &= ~D3D11_CREATE_DEVICE_DEBUG;
            INFO_LOG("DirectX11 debug layer unavailable; retrying without it");
            result = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
                requestedLevels, static_cast<UINT>(sizeof(requestedLevels) / sizeof(requestedLevels[0])),
                D3D11_SDK_VERSION, &swapDescription, &context->swapChain, &context->device,
                &context->featureLevel, &context->immediateContext);
        }
#endif
        if (FAILED(result) || !context->createBackBufferTargets(static_cast<UINT>(width), static_cast<UINT>(height)))
        {
            ERROR_AT(__LINE__, __FILE__, "failed to initialize DirectX11 (HRESULT=0x%08lx)", result);
            context->release();
            return false;
        }

        D3D11_DEPTH_STENCIL_DESC depthDescription = {};
        depthDescription.DepthEnable = TRUE;
        depthDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        depthDescription.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
        depthDescription.StencilEnable = TRUE;
        depthDescription.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
        depthDescription.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
        depthDescription.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
        depthDescription.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
        depthDescription.BackFace = depthDescription.FrontFace;
        if (FAILED(context->device->CreateDepthStencilState(&depthDescription, &context->depthEnabledState)))
            return false;
        depthDescription.DepthEnable = FALSE;
        depthDescription.StencilEnable = FALSE;
        if (FAILED(context->device->CreateDepthStencilState(&depthDescription, &context->depthDisabledState)))
            return false;
        context->immediateContext->OMSetDepthStencilState(context->depthEnabledState, 0);

        const uint32_t maxTextureDimension =
            context->featureLevel >= D3D_FEATURE_LEVEL_11_0 ? D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION :
                                                             D3D10_REQ_TEXTURE2D_U_OR_V_DIMENSION;
        TEXTURE_MANAGER::getInstance()->setTextureCapabilities(maxTextureDimension * maxTextureDimension,
                                                               maxTextureDimension,
                                                               maxTextureDimension);
        constexpr uint32_t maxConstantBufferVectors = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT;
        constexpr uint32_t maxInputAttributes = D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
        skeletal::setMeasuredSkinningCapability(maxConstantBufferVectors, maxInputAttributes);
        const skeletal::SKINNING_CAPABILITY skinning = skeletal::getMeasuredSkinningCapability();
        INFO_LOG("DirectX11 skeletal capability: constantBufferVectors=%u vertexAttributes=%u LBS=%u DQS=%u",
                 skinning.maxVertexShaderVectors, skinning.maxVertexAttributes,
                 skinning.lbsMatrixPaletteBones, skinning.dqsRigidPaletteBones);
        engineDevice->setBackBufferWidth(static_cast<float>(width));
        engineDevice->setBackBufferHeight(static_cast<float>(height));
        context->window.disableRender(windowHandle);
        const char *featureLevelName = "unknown";
        switch (context->featureLevel)
        {
            case D3D_FEATURE_LEVEL_10_0: featureLevelName = "10_0"; break;
            case D3D_FEATURE_LEVEL_10_1: featureLevelName = "10_1"; break;
            case D3D_FEATURE_LEVEL_11_0: featureLevelName = "11_0"; break;
            default: break;
        }
        INFO_LOG("DirectX11 initialized: Feature Level %s (0x%x)", featureLevelName,
                 static_cast<unsigned int>(context->featureLevel));
        if (engineDevice->isVerbose())
        {
            MINIZ::showVersion();
            INFO_LOG("Audio engine: %s", AUDIO_ENGINE_version());
        }
        return true;
    }

    bool CORE_MANAGER::resetDeviceWithNewDimensions(const int newWidth, const int newHeight)
    {
        if (newWidth <= 0 || newHeight <= 0)
            return false;
        DEVICE *engineDevice = getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE *context = engineDevice->getSpecificContextDevice();
        context->releaseBackBufferTargets();
        const HRESULT result = context->swapChain->ResizeBuffers(0, static_cast<UINT>(newWidth),
            static_cast<UINT>(newHeight), DXGI_FORMAT_UNKNOWN, 0);
        if (FAILED(result) || !context->createBackBufferTargets(static_cast<UINT>(newWidth), static_cast<UINT>(newHeight)))
        {
            ERROR_AT(__LINE__, __FILE__, "failed to resize DirectX11 back buffer (HRESULT=0x%08lx)", result);
            return false;
        }
        engineDevice->setBackBufferWidth(static_cast<float>(newWidth));
        engineDevice->setBackBufferHeight(static_cast<float>(newHeight));
        return true;
    }

    bool CORE_MANAGER::beginRender()
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getDevice()->getSpecificContextDevice();
        return context && context->immediateContext && context->backBufferView;
    }

    void CORE_MANAGER::endRender()
    {
        // Direct3D 11 has no BeginScene/EndScene pair. swapBuffers() presents the queued commands.
    }

    void CORE_MANAGER::swapBuffers()
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getDevice()->getSpecificContextDevice();
        if (!context || !context->swapChain)
            return;
        const HRESULT result = context->swapChain->Present(0, 0);
        if (FAILED(result))
            ERROR_AT(__LINE__, __FILE__, "DirectX11 Present failed (HRESULT=0x%08lx)", result);
    }

    bool CORE_MANAGER::renderToTargets()
    {
        DEVICE *device = getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        if (!context || !context->immediateContext)
            return false;
        ID3D11DeviceContext *immediateContext = context->immediateContext;
        ID3D11RenderTargetView *previousRenderTarget = nullptr;
        ID3D11DepthStencilView *previousDepthView = nullptr;
        immediateContext->OMGetRenderTargets(1, &previousRenderTarget, &previousDepthView);
        D3D11_VIEWPORT previousViewport = {};
        UINT viewportCount = 1;
        immediateContext->RSGetViewports(&viewportCount, &previousViewport);
        bool rendered = false;
        bool succeeded = true;
        CAMERA &camera = device->getCamera();
        ID3D11ShaderResourceView *emptyViews[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
        const uint32_t totalRenderTargets = device->getTotalRenderTargets();
        for (uint32_t i = 0; i < totalRenderTargets; ++i)
        {
            RENDERIZABLE_TO_TARGET *renderTarget = device->getRenderTarget(i);
            if (!renderTarget || !renderTarget->isOnFrustum())
                continue;
            RENDER2TARGET_DIRECTX11 *target = static_cast<RENDER2TARGET_DIRECTX11 *>(renderTarget->getRenderTargetSpecificConfig());
            if (!target || !target->renderTargetView || !target->depthView)
            {
                succeeded = false;
                break;
            }
            immediateContext->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, emptyViews);
            immediateContext->OMSetRenderTargets(1, &target->renderTargetView, target->depthView);
            D3D11_VIEWPORT viewport = {};
            viewport.Width = static_cast<float>(renderTarget->getRenderTargetWidth());
            viewport.Height = static_cast<float>(renderTarget->getRenderTargetHeight());
            viewport.MaxDepth = 1.0f;
            immediateContext->RSSetViewports(1, &viewport);
            const COLOR &color = renderTarget->getRenderTargetClearColor();
            const float clearColor[4] = { color.r, color.g, color.b, color.a };
            immediateContext->ClearRenderTargetView(target->renderTargetView, clearColor);
            immediateContext->ClearDepthStencilView(target->depthView,
                D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
            if (!renderTarget->render2Texture())
            {
                succeeded = false;
                break;
            }
            rendered = true;
        }
        immediateContext->OMSetRenderTargets(1, &previousRenderTarget, previousDepthView);
        if (viewportCount)
            immediateContext->RSSetViewports(1, &previousViewport);
        if (rendered)
            camera.updateCam(true, device->getBackBufferWidth(), device->getBackBufferHeight());
        if (previousDepthView)
            previousDepthView->Release();
        if (previousRenderTarget)
            previousRenderTarget->Release();
        if (!succeeded)
            ERROR_AT(__LINE__, __FILE__, "DirectX11 render-to-texture pass failed");
        return succeeded;
    }

    unsigned int CORE_MANAGER::addPlugin(PLUGIN *plugin)
    {
        for (unsigned int i = 0; i < getTotalPlugins(); ++i)
            if (plugin == getPlugin(i))
                return i;
        if (!plugin)
            return 0xffffffff;
        const unsigned int index = appendPlugin(plugin);
        DEVICE *engineDevice = getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE *context = engineDevice->getSpecificContextDevice();
        plugin->onSubscribe(static_cast<int>(engineDevice->getBackBufferWidth()),
            static_cast<int>(engineDevice->getBackBufferHeight()), context->window.getHwnd(), context->device);
        return index;
    }

    void CORE_MANAGER::getScreenSize(int *width, int *height)
    {
        if (!width || !height)
            return;
        *width = 0;
        *height = 0;
        util::getDisplayMetrics(width, height);
    }

    void CORE_MANAGER::setMinMaxSizeWindow(const int32_t minX, const int32_t minY, const int32_t maxX, const int32_t maxY)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getDevice()->getSpecificContextDevice();
        context->window.setMinSizeAllowed(minX, minY);
        context->window.setMaxSizeAllowed(maxX, maxY);
    }
}

namespace log_util
{
    OnScriptPrintLine onScriptPrintLine = nullptr;

    void setScriptPrintLine(OnScriptPrintLine onNewScriptPrintLine) noexcept
    {
        onScriptPrintLine = onNewScriptPrintLine;
    }

    void callScriptPrintLine() noexcept
    {
        if (onScriptPrintLine)
            onScriptPrintLine();
    }
}

#endif
