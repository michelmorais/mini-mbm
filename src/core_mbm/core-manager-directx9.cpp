/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2025 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#if defined (USE_DIRECTX9)

#include <specific-directx9.h>
#include <core-manager.h>
#include <device.h>
#include <renderizable.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <util-interface.h>
#include <audio-interface.h>
#include <miniz-wrap/miniz-wrap.h>
#include <plugin-callback.h>
#include <scene.h>


namespace mbm
{
    void CORE_MANAGER::handleEventFromWindow()
    {
        this->device->getSpecificContextDevice()->window.doEvents();
        bool first_menu = true;
        while (mbm::WINDOW::isAnyMenuVisible() && this->device->getSpecificContextDevice()->window.run)
        {
            if (first_menu)
            {
                Sleep(50);
                mbm::WINDOW::refreshMenu();
            }
            this->device->getSpecificContextDevice()->window.doEvents();
            if (first_menu)
            {
                Sleep(50);
                mbm::WINDOW::refreshMenu();
            }
            first_menu = false;
        }
        if (this->device->getSpecificContextDevice()->window.run)
        {
            INFO_JOYSTICK_INIT_PLAYER info;
            while (this->popEvent(&info))
            {
                if (this->device->scene && this->__sceneWasInit)
                    this->device->scene->onInfoDeviceJoystick(info.player, info.maxNumberButton, info.deviceName.c_str(),
                        info.extraInfo.c_str());
            }
        }
        else
        {
            this->device->setRun(false);
        }
    }

    static D3DPRESENT_PARAMETERS getd3dPARAMETERS(const UINT x,const UINT y, HWND hwnd)
    {
        D3DPRESENT_PARAMETERS				d3dParams;
        ZeroMemory(&d3dParams, sizeof(d3dParams));
        d3dParams.BackBufferWidth = x;
        d3dParams.BackBufferHeight = y;
        d3dParams.BackBufferFormat = D3DFMT_A8R8G8B8;
        d3dParams.BackBufferCount = 1;
        d3dParams.MultiSampleType = D3DMULTISAMPLE_NONE;//Use pD3D->CheckDeviceMultiSampleType
        d3dParams.MultiSampleQuality = 0;
        d3dParams.SwapEffect = D3DSWAPEFFECT_COPY;
        d3dParams.hDeviceWindow = hwnd;
        d3dParams.Windowed = true;//Full Screen = false
        d3dParams.EnableAutoDepthStencil = true;//Keep / create the Buffer Depht/Stencil automatically
        d3dParams.AutoDepthStencilFormat = D3DFMT_D24S8;//Bits Reservados Para O Stencil = 8
        d3dParams.Flags = 0;
        d3dParams.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;//Rate render
        d3dParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;//Present imediately
        return d3dParams;
    }

    CORE_MANAGER::CORE_MANAGER()
    {
        this->initializeImpl();
        this->device           = DEVICE::getInstance();
        this->changeScene               = true;
        this->__sceneWasInit            = false;
        this->keyCapsLockState          = false;
#if (defined (__MINGW32__) || defined (__CYGWIN__) || defined(_WIN32))
        this->device->getSpecificContextDevice()->initializeWi32Callbacks(this);
#endif
    }
    
    void CORE_MANAGER::moveWindow(int x, int y)
    {
        // nothing to do here for directx9
    }
    
    void CORE_MANAGER::ReleaseGraphics(const bool wasDeviceLost)
    {
        TEXTURE_MANAGER::getInstance()->release();
        MESH_MANAGER::getInstance()->release();
        this->device->getSpecificContextDevice()->window.setCallEventsManager(nullptr);
        this->device->getSpecificContextDevice()->win32_joystickByPass->releaseJoystick(&this->device->getSpecificContextDevice()->window);
        this->device->getSpecificContextDevice()->release();
    }
    
    bool CORE_MANAGER::initGraphics(const char* nameApplication, int width, int height, const int px, const int py, const bool border, const bool enable_resize)
    {
        int x = width;
        int y = height;
        DEVICE* device = DEVICE::getInstance();
        // Initialize window position
        device->setWindowPosition(px, py);
        this->setNameApplication(nameApplication);
        device->getSpecificContextDevice()->window.setNameAplication(nameApplication);
        if (!device->getSpecificContextDevice()->window.init(nameApplication, x, y, px, py, enable_resize, enable_resize, enable_resize, false, nullptr, border == false,
            this->device->getSpecificContextDevice()->idIcon, false))
        {
            device->getSpecificContextDevice()->window.messageBox("error on init app ... will be closed ");
            PRINT_IF_DEBUG("error on init app ... will be closed %s", "error on create window");
            return false;
        }
        device->getSpecificContextDevice()->window.setMinSizeAllowed(800, 600);
        HWND mNativeWindow = device->getSpecificContextDevice()->window.getHwnd();
        RECT rect;
        if (!GetClientRect(mNativeWindow, &rect))
        {
            //MessageBoxW(mNativeWindow, L"error on get the window size!", "DEVICE", MB_OK | MB_ICONERROR);
            rect.right = width;
            rect.bottom = height;
            rect.left = 0;
            rect.top = 0;
        }
        if ((rect.right - rect.left) != width || (rect.bottom - rect.top) != height)
        {
            x = rect.right - rect.left;
            y = rect.bottom - rect.top;
            INFO_LOG("BackBuffer adjusted because the width and height are different from window\n"
                "expected X: %d Y: %d \n"
                "real     X: %d Y: %d \n",
                width, height, x, y);
        }
        else
        {
            x = width;
            y = height;
        }
        if (this->device->getSpecificContextDevice()->win32_EventByPass)
            device->getSpecificContextDevice()->window.setCallEventsManager(this->device->getSpecificContextDevice()->win32_EventByPass);
        if (this->device->getSpecificContextDevice()->win32_joystickByPass)
            this->device->getSpecificContextDevice()->win32_joystickByPass->initJoystick(&device->getSpecificContextDevice()->window);

        if (D3DXCheckVersion(D3D_SDK_VERSION, D3DX_SDK_VERSION))
        {
            INFO_LOG("DirectX version is not present or if there is a failure during initialization");
        }

        if (NULL == (this->device->getSpecificContextDevice()->pD3D = Direct3DCreate9(D3D_SDK_VERSION)))
        {

            ERROR_AT(__LINE__, __FILE__, "failed to create hardware device '%s'", "Direct3DCreate9");
            return false;
        }

        /*
        DWORD dwVersion;
        DWORD dwRevision;
        if (DirectXSetupGetVersion(&dwVersion, &dwRevision))
        {
            INFO_LOG("DirectX version is %d.%d.%d.%d\n",
                HIWORD(dwVersion), LOWORD(dwVersion),
                HIWORD(dwRevision), LOWORD(dwRevision));
        }*/

        D3DPRESENT_PARAMETERS d3dParams = getd3dPARAMETERS(x, y, mNativeWindow);
        
        D3DCAPS9 cap;

        D3DDEVTYPE _typeDevice = D3DDEVTYPE_HAL;
        this->device->getSpecificContextDevice()->pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT, _typeDevice, &cap);
        int Hardware_Software_Vertex_Process = 0;
        const bool forceSoftwareProcess = false;
        if (forceSoftwareProcess)
        {
            if (FAILED(this->device->getSpecificContextDevice()->pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, mNativeWindow,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
                &d3dParams, &this->device->getSpecificContextDevice()->pd3dDevice)))
            {
                ERROR_AT(__LINE__, __FILE__, "failed to create software device");
                return false;
            }
        }
        else
        {
            if (cap.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
                Hardware_Software_Vertex_Process = D3DCREATE_HARDWARE_VERTEXPROCESSING;
            else
            {
                _typeDevice = D3DDEVTYPE_REF;//Nosso Tipo De Dispositivo Que Queremos Capturar
                this->device->getSpecificContextDevice()->pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT, _typeDevice, &cap);
                Hardware_Software_Vertex_Process = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
            }
            //Passo 4:Criamos O Dispositivo -----------------------------------------------------------------------------------
            if (FAILED(this->device->getSpecificContextDevice()->pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, mNativeWindow,
                Hardware_Software_Vertex_Process | D3DCREATE_MULTITHREADED,
                &d3dParams, &this->device->getSpecificContextDevice()->pd3dDevice)))
            {
                if (FAILED(this->device->getSpecificContextDevice()->pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, mNativeWindow,
                    Hardware_Software_Vertex_Process | D3DCREATE_MULTITHREADED,
                    &d3dParams, &this->device->getSpecificContextDevice()->pd3dDevice)))
                {
                    if (FAILED(this->device->getSpecificContextDevice()->pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_SW, mNativeWindow,
                        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT,
                        &d3dParams, &this->device->getSpecificContextDevice()->pd3dDevice)))
                    {
                        ERROR_AT(__LINE__, __FILE__, "failed to create hardware device");
                        return false;
                    }
                }
            }
        }

        TEXTURE_MANAGER* texture_manager = TEXTURE_MANAGER::getInstance();
        const uint32_t maxTextureSize = cap.MaxTextureWidth * cap.MaxTextureWidth;
        texture_manager->setTextureCapabilities(maxTextureSize, cap.MaxTextureWidth, cap.MaxTextureHeight);

        this->device->getSpecificContextDevice()->pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);//Turn on the face oclusion
        this->device->getSpecificContextDevice()->pd3dDevice->SetRenderState(D3DRS_LIGHTING, false);//Turn off ilumination
        this->device->getSpecificContextDevice()->pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);//Turn on Zbuffer
        //TODO: set matrix mode to world

        //optional states
        this->device->getSpecificContextDevice()->pd3dDevice->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_XRGB(100, 100, 100));
        //enable blender
        this->device->getSpecificContextDevice()->pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, true);

        //This value can range from 1 to 16, with higher values providing better image quality at the cost of performance.
        for (DWORD stage = 0; stage < 2; ++stage)
        {
            const DWORD Anisotropy = static_cast<DWORD>(static_cast<float>(cap.MaxAnisotropy) * 0.5);
            if (FAILED(this->device->getSpecificContextDevice()->pd3dDevice->SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, Anisotropy))) //50%
            {
                ERROR_AT(__LINE__, __FILE__, "failed to SetSamplerState D3DSAMP_MAXANISOTROPY %d", Anisotropy);
            }
            if (FAILED(this->device->getSpecificContextDevice()->pd3dDevice->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR)))
            {
                ERROR_AT(__LINE__, __FILE__, "failed to SetSamplerState D3DSAMP_MINFILTER %d", Anisotropy);
            }
            if (FAILED(this->device->getSpecificContextDevice()->pd3dDevice->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR)))
            {
                ERROR_AT(__LINE__, __FILE__, "failed to SetSamplerState D3DSAMP_MAGFILTER %d", Anisotropy);
            }
            if (FAILED(this->device->getSpecificContextDevice()->pd3dDevice->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_NONE)))
            {
                ERROR_AT(__LINE__, __FILE__, "failed to SetSamplerState D3DSAMP_MIPFILTER %d", Anisotropy);
            }
            this->device->getSpecificContextDevice()->pd3dDevice->SetSamplerState(stage, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            this->device->getSpecificContextDevice()->pd3dDevice->SetSamplerState(stage, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
            this->device->getSpecificContextDevice()->pd3dDevice->GetSamplerState(stage, D3DSAMP_MINFILTER, &this->device->getSpecificContextDevice()->DWORD_D3DSAMP_MINFILTER[stage]);
            this->device->getSpecificContextDevice()->pd3dDevice->GetSamplerState(stage, D3DSAMP_MAGFILTER, &this->device->getSpecificContextDevice()->DWORD_D3DSAMP_MAGFILTER[stage]);
            this->device->getSpecificContextDevice()->pd3dDevice->GetSamplerState(stage, D3DSAMP_MIPFILTER, &this->device->getSpecificContextDevice()->DWORD_D3DSAMP_MIPFILTER[stage]);
        }

        
        
        device->getSpecificContextDevice()->window.disableRender(mNativeWindow);
        //TODO: set real version from DirectX
        INFO_LOG("\nDIRECTX Version: %s\n", "9");
        if (device->isVerbose())
        {
            MINIZ::showVersion();
            INFO_LOG("\nAudio engine: %s\n", AUDIO_ENGINE_version());
        }

        // device->setProjectionMode set viewport and other initial states for ANY
        if (x > 0)
            device->backBufferWidth = static_cast<float>(x);
        if (y > 0)
            device->backBufferHeight = static_cast<float>(y);
        return true;
    }

    bool CORE_MANAGER::resetDeviceWithNewDimensions(int newWidth, int newHeight)// need to be implemented in each backend engine
    {
        // Reset D3D device with new dimensions
        D3DPRESENT_PARAMETERS d3dParams = getd3dPARAMETERS(static_cast<UINT>(newWidth), static_cast<UINT>(newHeight), device->getSpecificContextDevice()->window.getHwnd());

        // Verify window handle is valid
        if (!IsWindow(d3dParams.hDeviceWindow))
        {
            ERROR_AT(__LINE__, __FILE__, "Invalid window handle during resize");
            return false;
        }
        // Attempt reset
        HRESULT hr = this->device->getSpecificContextDevice()->pd3dDevice->Reset(&d3dParams);

        if (FAILED(hr))
        {
#if defined _DEBUG
            ERROR_LOG("ONRESIZEWINDOW: Reset failed with HRESULT: 0x%x", hr);
#endif
            if (D3DERR_DEVICELOST == hr)
            {
                // Trigger full device restore sequence
                ERROR_AT(__LINE__, __FILE__, "Reason device reset fail - D3DERR_DEVICELOST");
            }
            if (D3DERR_DEVICENOTRESET == hr)
            {
                // Trigger full device restore sequence
                ERROR_AT(__LINE__, __FILE__, "Reason device reset fail - D3DERR_DEVICENOTRESET");
            }
            if (D3DERR_DRIVERINTERNALERROR == hr)
            {
                // Trigger full device restore sequence
                ERROR_AT(__LINE__, __FILE__, "Reason device reset fail - D3DERR_DRIVERINTERNALERROR");
            }
            else if (D3DERR_INVALIDCALL == hr)
            {
                // Invalid parameters or device state
                ERROR_AT(__LINE__, __FILE__, "Reason device reset fail - D3DERR_INVALIDCALL");
            }
            else if (D3DERR_OUTOFVIDEOMEMORY == hr)
            {
                ERROR_AT(__LINE__, __FILE__, "Reason device reset fail - D3DERR_OUTOFVIDEOMEMORY");
            }
            return false;
        }
        return true;
    }

    bool CORE_MANAGER::beginRender()
    {
        HRESULT hr = this->device->getSpecificContextDevice()->pd3dDevice->BeginScene();
        if (CHECK_AND_LOG_HRESULT_DX(hr))
        {
            return true;
        }
        return false;
    }

    void CORE_MANAGER::endRender()
    {
        this->device->getSpecificContextDevice()->pd3dDevice->EndScene();
    }

    void CORE_MANAGER::swapBuffers()
    {
        this->device->getSpecificContextDevice()->pd3dDevice->Present(NULL, NULL, NULL, NULL);
    }

    bool CORE_MANAGER::renderToTargets()
    {
        bool oneRender                 = false;
        IDirect3DDevice9* pd3dDevice   = this->device->getSpecificContextDevice()->pd3dDevice;
        IDirect3DSurface9* pBackBuffer = NULL;
        
        pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        if (FAILED(pd3dDevice->GetRenderTarget(0, &pBackBuffer)))
        {
            ERROR_AT(__LINE__, __FILE__, "failed to backup backbuffer before render to target");
            return false;
        }
        const uint32_t totalRenderTargets = this->device->getTotalRenderTargets();
        for (uint32_t i = 0; i < totalRenderTargets; ++i)
        {
            auto renderTarget = this->device->getRenderTarget(i);
            if (!renderTarget)
                continue;
            if (!renderTarget->isObjectOnFrustum)
                continue;
            // DX9 cannot safely render into a texture that is still bound as an input sampler
            // from a previous draw. OpenGL explicitly unbinds after render-to-texture; do the
            // equivalent here before switching the texture into render-target mode.
            for (DWORD stage = 0; stage < 8; ++stage)
                pd3dDevice->SetTexture(stage, nullptr);
            RENDER2TARGET_DIRECTX9* sf = static_cast<RENDER2TARGET_DIRECTX9*>(renderTarget->specificConfig);
            HRESULT hr = pd3dDevice->SetRenderTarget(0, sf->pRenderSurface);

            //begin rendering to texture
            if (FAILED(hr))
            {
                ERROR_AT(__LINE__, __FILE__, "Error renderToTargets HRESULT: 0x%x use DXErr to verify!", hr);
                pd3dDevice->SetRenderTarget(0, pBackBuffer);
                return false;
            }

            // DirectX 9 does NOT automatically resize the viewport when SetRenderTarget is called.
            // The viewport stays at the back-buffer dimensions, so any content that falls below
            // the window height (e.g. a 1024x1024 canvas on a 900px-tall window) is silently
            // clipped. Set the viewport explicitly to the render target dimensions.
            const D3DVIEWPORT9 rtViewport = { 0, 0,
                static_cast<DWORD>(renderTarget->widthTexture),
                static_cast<DWORD>(renderTarget->heightTexture),
                0.0f, 1.0f };
            pd3dDevice->SetViewport(&rtViewport);

            // Swap in the render target's own depth stencil surface.
            // The device auto-depth-stencil is sized to the back buffer; without this swap
            // DX9 clips rendering to the back buffer height even when the viewport is larger.
            IDirect3DSurface9* pOldDepthStencil = nullptr;
            if (sf->pDepthStencilSurface)
            {
                pd3dDevice->GetDepthStencilSurface(&pOldDepthStencil);
                pd3dDevice->SetDepthStencilSurface(sf->pDepthStencilSurface);
            }

            hr = pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, renderTarget->colorClearBackGround, 1.0f, 0);
            //clear color and z-buffer
            if (FAILED(hr))
            {
                ERROR_AT(__LINE__, __FILE__,"Error Clear Z buffer of render 2 texture HRESULT: 0x%h use DXErr to verify!", hr);
                if (pOldDepthStencil) { pd3dDevice->SetDepthStencilSurface(pOldDepthStencil); pOldDepthStencil->Release(); }
                pd3dDevice->SetRenderTarget(0, pBackBuffer);
                return false;
            }

            hr = pd3dDevice->BeginScene();
            if (FAILED(hr))
            {
                ERROR_AT(__LINE__, __FILE__, "Error BeginScene of render 2 texture HRESULT: 0x%h use DXErr to verify!", hr);
                if (pOldDepthStencil) { pd3dDevice->SetDepthStencilSurface(pOldDepthStencil); pOldDepthStencil->Release(); }
                pd3dDevice->SetRenderTarget(0, pBackBuffer);
                return false;
            }
            
            //this->device->camera.updateCam(false, static_cast<float>(renderTarget->widthTexture), static_cast<float>(renderTarget->heightTexture));
            //RENDER_2_TEXTURE::render2Texture() update the matrix of projection, so, later we need to restore, see this->device->camera.updateCam
            if (!renderTarget->render2Texture())
            {
                ERROR_AT(__LINE__, __FILE__, "Error render2Texture!");
                if (pOldDepthStencil) { pd3dDevice->SetDepthStencilSurface(pOldDepthStencil); pOldDepthStencil->Release(); }
                pd3dDevice->SetRenderTarget(0, pBackBuffer);
                this->device->camera.updateCam(true, static_cast<float>(device->backBufferWidth), static_cast<float>(device->backBufferHeight));
                return false;
            }
            hr = pd3dDevice->EndScene();
            if (FAILED(hr))
            {
                ERROR_AT(__LINE__, __FILE__, "Error EndScene of render 2 texture HRESULT: 0x%h use DXErr to verify!", hr);
                if (pOldDepthStencil) { pd3dDevice->SetDepthStencilSurface(pOldDepthStencil); pOldDepthStencil->Release(); }
                pd3dDevice->SetRenderTarget(0, pBackBuffer);
                this->device->camera.updateCam(false, static_cast<float>(device->backBufferWidth), static_cast<float>(device->backBufferHeight));
                this->device->camera.updateCam(true, static_cast<float>(device->backBufferWidth), static_cast<float>(device->backBufferHeight));
                return false;
            }
            if (pOldDepthStencil) { pd3dDevice->SetDepthStencilSurface(pOldDepthStencil); pOldDepthStencil->Release(); }
            oneRender = true;
        }
        if (oneRender)
        {
            pd3dDevice->SetRenderTarget(0, pBackBuffer);
            // Restore the viewport to the back-buffer dimensions after render-to-texture pass.
            const D3DVIEWPORT9 bbViewport = { 0, 0,
                static_cast<DWORD>(device->backBufferWidth),
                static_cast<DWORD>(device->backBufferHeight),
                0.0f, 1.0f };
            pd3dDevice->SetViewport(&bbViewport);
            this->device->camera.updateCam(true, static_cast<float>(device->backBufferWidth), static_cast<float>(device->backBufferHeight));
        }
        return true;
    }
    
    unsigned int CORE_MANAGER::addPlugin(PLUGIN * plugin)
    {
        for(unsigned int i=0; i < this->getTotalPlugins(); ++i)
        {
            const PLUGIN * thatPlugin = this->getPlugin(i);
            if(plugin == thatPlugin)
            {
                return i;
            }
        }
        if(plugin != nullptr)
        {
            const unsigned int indexPlugin = this->appendPlugin(plugin);
            void * handle = this->device->getSpecificContextDevice()->window.getHwnd();
            void * renderDevice = this->device->getSpecificContextDevice()->pd3dDevice;
            plugin->onSubscribe(static_cast<int>(this->device->backBufferWidth),static_cast<int>(this->device->backBufferHeight), handle, renderDevice);
            return indexPlugin;
        }
        return 0xffffffff;
    }

    void CORE_MANAGER::setMinMaxSizeWindow(int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y)
    {
        this->device->getSpecificContextDevice()->window.setMinSizeAllowed(min_x, min_y);
        this->device->getSpecificContextDevice()->window.setMaxSizeAllowed(max_x,max_y);
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
        if(onScriptPrintLine)
            onScriptPrintLine();
    }

}
#endif // USE_DIRECTX9
