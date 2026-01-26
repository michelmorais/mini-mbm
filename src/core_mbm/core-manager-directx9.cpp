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

#include "dummy-engine.h" // for compiler_message, you can remove it after implement the functions

#include <core-manager.h>
#include <device.h>
#include <scene.h>
#include <renderizable.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <util-interface.h>
#include <audio-interface.h>
#include <miniz-wrap/miniz-wrap.h>
#include <cstring>
#include <plugin-callback.h>


namespace mbm
{
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

constexpr EVENT_KEY::EVENT_KEY() noexcept : x(0), y(0), key(0), player(0), rx(0), ry(0), eventType(UNKNOWN)
    {}
        
    constexpr EVENT_KEY::EVENT_KEY(const float _x, const float _y, const int _key, const EVENT_TYPE_ACTIONS _eventName) noexcept
        : x(_x),
            y(_y),
            key(_key),
            player(0),
            rx(0.0f),
            ry(0.0f),
            eventType(_eventName)
    {}
    constexpr EVENT_KEY::EVENT_KEY(const float _lx, const float _ly, const int _key, const int _player, const float _rx,
                        const float _ry, const EVENT_TYPE_ACTIONS _eventName) noexcept : lx(_lx),
                                                                                            ly(_ly),
                                                                                            key(_key),
                                                                                            player(_player),
                                                                                            rx(_rx),
                                                                                            ry(_ry),
                                                                                            eventType(_eventName)
    {}

    INFO_JOYSTICK_INIT_PLAYER::INFO_JOYSTICK_INIT_PLAYER() : player(0), maxNumberButton(0)
    {}

    INFO_JOYSTICK_INIT_PLAYER::INFO_JOYSTICK_INIT_PLAYER(const int _player, const int _maxNumberButton, const char *_deviceName,
                                const char *_extraInfo)
        : player(_player), maxNumberButton(_maxNumberButton), deviceName(_deviceName), extraInfo(_extraInfo)
    {}

    CORE_MANAGER::CORE_MANAGER()
    {
        this->device           = DEVICE::getInstance();
        this->indexOnRestore   = 0;
        this->totalForByLoop   = 0;
        this->percentRestoreInfo = 0.0f;
        this->stepRestoreInfo  = 0.1f;
        this->stepRestore      = STEP_RES_INIT_GL;
        this->which_for        = WFOR_INITIAL;
        this->changeScene      = true;
        this->__sceneWasInit   = false;
        this->keyCapsLockState = false;
    }
    
    CORE_MANAGER::~CORE_MANAGER()
    {
        DEVICE::quit();
    }
    
    void CORE_MANAGER::ReleaseGraphics()
    {
        TEXTURE_MANAGER::getInstance()->release();
        MESH_MANAGER::getInstance()->release();
        this->device->specificContextDevice->realease();
    }
    
    bool CORE_MANAGER::initGraphics(const char* nameAplication, int width, int height, const int px, const int py, const bool border, const bool enable_resize)
    {
        int x = width;
        int y = height;
        DEVICE* device = DEVICE::getInstance();
        this->nameAplication = nameAplication ? nameAplication : "Mini-mbm";
        device->window.setNameAplication(nameAplication);
        if (!device->window.init(nameAplication, x, y, px, py, enable_resize, enable_resize, enable_resize, false, nullptr, border == false,
            this->idIcon, false))
        {
            device->window.messageBox("error on init app ... will be closed ");
            PRINT_IF_DEBUG("error on init app ... will be closed %s", "error on create window");
            return false;
        }
        device->window.setMinSizeAllowed(800, 600);
        HWND mNativeWindow = device->window.getHwnd();
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
        device->window.setCallEventsManager(this);
        this->initJoystick(&device->window);

        if (D3DXCheckVersion(D3D_SDK_VERSION, D3DX_SDK_VERSION))
        {
            INFO_LOG("DirectX version is not present or if there is a failure during initialization");
        }

        if (NULL == (this->device->specificContextDevice->pD3D = Direct3DCreate9(D3D_SDK_VERSION)))
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
        this->device->specificContextDevice->pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT, _typeDevice, &cap);
        int Hardware_Software_Vertex_Process = 0;
        const bool forceSoftwareProcess = false;
        if (forceSoftwareProcess)
        {
            if (FAILED(this->device->specificContextDevice->pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, mNativeWindow,
                D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
                &d3dParams, &this->device->specificContextDevice->pd3dDevice)))
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
                this->device->specificContextDevice->pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT, _typeDevice, &cap);
                Hardware_Software_Vertex_Process = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
            }
            //Passo 4:Criamos O Dispositivo -----------------------------------------------------------------------------------
            if (FAILED(this->device->specificContextDevice->pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, mNativeWindow,
                Hardware_Software_Vertex_Process | D3DCREATE_MULTITHREADED,
                &d3dParams, &this->device->specificContextDevice->pd3dDevice)))
            {
                if (FAILED(this->device->specificContextDevice->pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, mNativeWindow,
                    Hardware_Software_Vertex_Process | D3DCREATE_MULTITHREADED,
                    &d3dParams, &this->device->specificContextDevice->pd3dDevice)))
                {
                    if (FAILED(this->device->specificContextDevice->pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_SW, mNativeWindow,
                        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT,
                        &d3dParams, &this->device->specificContextDevice->pd3dDevice)))
                    {
                        ERROR_AT(__LINE__, __FILE__, "failed to create hardware device");
                        return false;
                    }
                }
            }
        }

        TEXTURE_MANAGER* texture_manager = TEXTURE_MANAGER::getInstance();
        const int32_t maxTextureSize = cap.MaxTextureWidth * cap.MaxTextureWidth;
        texture_manager->setTextureCapabilities(maxTextureSize, cap.MaxTextureWidth, cap.MaxTextureHeight);

        this->device->specificContextDevice->pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);//Turn on the face oclusion
        this->device->specificContextDevice->pd3dDevice->SetRenderState(D3DRS_LIGHTING, false);//Turn off ilumination
        this->device->specificContextDevice->pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);//Turn on Zbuffer
        //TODO: set matrix mode to world

        //optional states
        this->device->specificContextDevice->pd3dDevice->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_XRGB(100, 100, 100));
        //enable blender
        this->device->specificContextDevice->pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, true);

        //This value can range from 1 to 16, with higher values providing better image quality at the cost of performance.
        for (DWORD stage = 0; stage < 2; ++stage)
        {
            const DWORD Anisotropy = static_cast<DWORD>(static_cast<float>(cap.MaxAnisotropy) * 0.5);
            if (FAILED(this->device->specificContextDevice->pd3dDevice->SetSamplerState(stage, D3DSAMP_MAXANISOTROPY, Anisotropy))) //50%
            {
                ERROR_AT(__LINE__, __FILE__, "failed to SetSamplerState D3DSAMP_MAXANISOTROPY %d", Anisotropy);
            }
            if (FAILED(this->device->specificContextDevice->pd3dDevice->SetSamplerState(stage, D3DSAMP_MINFILTER, D3DTEXF_LINEAR)))
            {
                ERROR_AT(__LINE__, __FILE__, "failed to SetSamplerState D3DSAMP_MINFILTER %d", Anisotropy);
            }
            if (FAILED(this->device->specificContextDevice->pd3dDevice->SetSamplerState(stage, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR)))
            {
                ERROR_AT(__LINE__, __FILE__, "failed to SetSamplerState D3DSAMP_MAGFILTER %d", Anisotropy);
            }
            if (FAILED(this->device->specificContextDevice->pd3dDevice->SetSamplerState(stage, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR)))
            {
                ERROR_AT(__LINE__, __FILE__, "failed to SetSamplerState D3DSAMP_MIPFILTER %d", Anisotropy);
            }
        }
        
        device->window.disableRender(mNativeWindow);
        //TODO: set real version from DirectX
        INFO_LOG("\nDIRECTX Version: %s\n", "9");
        if (device->verbose)
        {
            MINIZ::showVersion();
            INFO_LOG("\nAudio engine: %s\n", AUDIO_ENGINE_version());
        }

        #pragma message(REMINDER_TODO "  set viewport and other initial states for ANY")
        if (x > 0)
            device->backBufferWidth = static_cast<float>(x);
        if (y > 0)
            device->backBufferHeight = static_cast<float>(y);
        return true;
    }

    bool CORE_MANAGER::resetDeviceWithNewDimensions(int newWidth, int newHeight)// need to be implemented in each backend engine
    {
        // Reset D3D device with new dimensions
        D3DPRESENT_PARAMETERS d3dParams = getd3dPARAMETERS(static_cast<UINT>(newWidth), static_cast<UINT>(newHeight), this->device->window.getHwnd());

        // Verify window handle is valid
        if (!IsWindow(d3dParams.hDeviceWindow))
        {
            ERROR_AT(__LINE__, __FILE__, "Invalid window handle during resize");
            return false;
        }
        // Attempt reset
        HRESULT hr = this->device->specificContextDevice->pd3dDevice->Reset(&d3dParams);

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
        HRESULT hr = this->device->specificContextDevice->pd3dDevice->BeginScene();
        if (CHECK_AND_LOG_HRESULT_DX(hr))
        {
            return true;
        }
        return false;
    }

    void CORE_MANAGER::endRender()
    {
        this->device->specificContextDevice->pd3dDevice->EndScene();
    }

    void CORE_MANAGER::swapBuffers()
    {
        this->device->specificContextDevice->pd3dDevice->Present(NULL, NULL, NULL, NULL);
    }

    bool CORE_MANAGER::renderToTargets()
    {
        bool oneRender                 = false;
        IDirect3DDevice9* pd3dDevice   = this->device->specificContextDevice->pd3dDevice;
        IDirect3DSurface9* pBackBuffer = NULL;
        
        pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        if (FAILED(pd3dDevice->GetRenderTarget(0, &pBackBuffer)))
        {
            ERROR_AT(__LINE__, __FILE__, "failed to backup backbuffer before render to target");
            return false;
        }
        for (auto renderTarget : this->device->lsObjectRenderToTarget)
        {
            if (!renderTarget->isObjectOnFrustum)
                continue;
            RENDER2TARGET_DIRECTX9* sf = static_cast<RENDER2TARGET_DIRECTX9*>(renderTarget->specificConfig);
            HRESULT hr = pd3dDevice->SetRenderTarget(0, sf->pRenderSurface);

            //begin rendering to texture
            if (FAILED(hr))
            {
                ERROR_AT(__LINE__, __FILE__, "Error renderToTargets HRESULT: 0x%x use DXErr to verify!", hr);
                pd3dDevice->SetRenderTarget(0, pBackBuffer);
                return false;
            }

            hr = pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, renderTarget->colorClearBackGround, 1.0f, 0);
            //clear color and z-buffer
            if (FAILED(hr))
            {
                ERROR_AT(__LINE__, __FILE__,"Error Clear Z buffer of render 2 texture HRESULT: 0x%h use DXErr to verify!", hr);
                pd3dDevice->SetRenderTarget(0, pBackBuffer);
                return false;
            }

            hr = pd3dDevice->BeginScene();
            if (FAILED(hr))
            {
                ERROR_AT(__LINE__, __FILE__, "Error BeginScene of render 2 texture HRESULT: 0x%h use DXErr to verify!", hr);
                pd3dDevice->SetRenderTarget(0, pBackBuffer);
                return false;
            }
            
            //this->device->camera.updateCam(false, static_cast<float>(renderTarget->widthTexture), static_cast<float>(renderTarget->heightTexture));
            //RENDER_2_TEXTURE::render2Texture() update the matrix of projection, so, later we need to restore, see this->device->camera.updateCam
            if (!renderTarget->render2Texture())
            {
                ERROR_AT(__LINE__, __FILE__, "Error render2Texture!");
                pd3dDevice->SetRenderTarget(0, pBackBuffer);
                this->device->camera.updateCam(true, static_cast<float>(device->backBufferWidth), static_cast<float>(device->backBufferHeight));
                return false;
            }
            hr = pd3dDevice->EndScene();
            if (FAILED(hr))
            {
                ERROR_AT(__LINE__, __FILE__, "Error EndScene of render 2 texture HRESULT: 0x%h use DXErr to verify!", hr);
                pd3dDevice->SetRenderTarget(0, pBackBuffer);
                this->device->camera.updateCam(false, static_cast<float>(device->backBufferWidth), static_cast<float>(device->backBufferHeight));
                this->device->camera.updateCam(true, static_cast<float>(device->backBufferWidth), static_cast<float>(device->backBufferHeight));
                return false;
            }
            oneRender = true;
        }
        if (oneRender)
        {
            pd3dDevice->SetRenderTarget(0, pBackBuffer);
            this->device->camera.updateCam(true, static_cast<float>(device->backBufferWidth), static_cast<float>(device->backBufferHeight));
        }
        return true;
    }
    
    void CORE_MANAGER::pushEvent(EVENT_KEY *event)
    {
        if (this->device->scene && this->__sceneWasInit)
        {
#if defined _WIN32
            mutexEvents.lock();
#endif
            if (event->eventType == this->lastEvent.eventType)
            {
                switch (event->eventType)
                {
                    case UNKNOWN: return;
                    case ONRESIZEWINDOW:
                    case ONTOUCHDOWN:
                    case ONTOUCHUP:
                    case ONTOUCHMOVE:
                    {
                        if (event->key == this->lastEvent.key &&  //-V550
                            event->x == this->lastEvent.x &&
                            event->y == this->lastEvent.y) //-V550
                        {
#if defined _WIN32
                            mutexEvents.unlock();
#endif
                            return;
                        }
                    }
                    break;
                    case ONDOUBLECLICK:
                    {
                        if (event->key == this->lastEvent.key &&  //-V550
                            event->x == this->lastEvent.x &&
                            event->y == this->lastEvent.y) //-V550
                        {
#if defined _WIN32
                            mutexEvents.unlock();
#endif
                            return;
                        }
                    }
                    break;
                    case ONKEYDOWN:
                    case ONKEYUP:
                    {
                        if (event->key == this->lastEvent.key)
                        {
#if defined _WIN32
                            mutexEvents.unlock();
#endif
                            return;
                        }
                    }
                    break;
                    case ONTOUCHZOOM: {
                    }
                    break;
                    default: {
                    }
                    break;
                }
            }
            this->lastEvent = *event;

            switch (event->eventType)
            {
                case ONKEYDOWN:
                {
                    if (this->__keyPressed[event->key] == false)
                        this->lsEvents.push_back(*event);
                    this->__keyPressed[event->key] = true;
                }
                break;
                case ONKEYUP:
                {
                    if (this->__keyPressed[event->key])
                        this->lsEvents.push_back(*event);
                    this->__keyPressed[event->key] = false;
                }
                break;
                default: { this->lsEvents.push_back(*event);
                }
                break;
            }
#if defined _WIN32
            mutexEvents.unlock();
#endif
        }
    }
    
    bool CORE_MANAGER::popEvent(EVENT_KEY *event)
    {
#if defined _WIN32
        mutexEvents.lock();
#endif
        if (this->lsEvents.size() > 0 && event)
        {
            *event = this->lsEvents.front();
            this->lsEvents.pop_front();
#if defined _WIN32
            mutexEvents.unlock();
#endif
            return true;
        }
        else
        {
#if defined _WIN32
            mutexEvents.unlock();
#endif
            return false;
        }
    }
    
    void CORE_MANAGER::pushEvent(INFO_JOYSTICK_INIT_PLAYER *info)
    {
        if (this->device->scene && this->__sceneWasInit)
        {
#if defined _WIN32
            mutexEvents.lock();
#endif
            this->lsInfoJoystick.push_back(*info);
#if defined _WIN32
            mutexEvents.unlock();
#endif
        }
    }
    
    bool CORE_MANAGER::popEvent(INFO_JOYSTICK_INIT_PLAYER *info)
    {
#if defined _WIN32
        mutexEvents.lock();
#endif
        if (this->lsInfoJoystick.size() > 0 && info)
        {

            *info = this->lsInfoJoystick.front();
            this->lsInfoJoystick.pop_front();
#if defined _WIN32
            mutexEvents.unlock();
#endif
            return true;
        }
        else
        {
#if defined _WIN32
            mutexEvents.unlock();
#endif
            return false;
        }
    }
    
    void CORE_MANAGER::onTouchDown(HWND, int key, float x, float y)
    {
        x /= this->device->camera.scale2d.x;
        y /= this->device->camera.scale2d.y;
        EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONTOUCHDOWN);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onTouchUp(HWND, int key, float x, float y)
    {
        x /= this->device->camera.scale2d.x;
        y /= this->device->camera.scale2d.y;
        EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONTOUCHUP);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onTouchMove(HWND, float x, float y)
    {
        x /= this->device->camera.scale2d.x;
        y /= this->device->camera.scale2d.y;
        EVENT_KEY ev(x, y, 0, EVENT_TYPE_ACTIONS::ONTOUCHMOVE);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onTouchZoom(HWND, float zoom) // Evento chamado ao solicitar zoom. Zoom estes normalmente com movimentos dos
                                       // dedos. É enviados valores entre -1 e +1. No caso de mouse é o scrool do mesmo.
    {
        EVENT_KEY ev(0, 0, (int)zoom, EVENT_TYPE_ACTIONS::ONTOUCHZOOM);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onKeyDown(HWND, int key) // Evento chamado ao pressionar uma tecla na janela ativa. key é um VK padrão da api do Windows.
    {
        EVENT_KEY ev(0, 0, key, EVENT_TYPE_ACTIONS::ONKEYDOWN);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onKeyUp(HWND,int key) // Evento chamado ao pressionar uma tecla na janela ativa. key é um VK padrão da api do Windows.
    {
        EVENT_KEY ev(0, 0, key, EVENT_TYPE_ACTIONS::ONKEYUP);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onDoubleClick(HWND, float x, float y, int key)
    {
        x /= this->device->camera.scale2d.x;
        y /= this->device->camera.scale2d.y;
        EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONDOUBLECLICK);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onKeyDownJoystick(int player, int key)
    {
        EVENT_KEY ev(0.0f, 0.0f, key, player, 0.0f, 0.0f, EVENT_TYPE_ACTIONS::ONKEYDOWNJOYSTICK);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onKeyUpJoystick(int player, int key)
    {
        EVENT_KEY ev(0.0f, 0.0f, key, player, 0.0f, 0.0f, EVENT_TYPE_ACTIONS::ONKEYUPJOYSTICK);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onMoveJoystick(int player, float lx, float ly, float rx, float ry)
    {
        constexpr float pProp_128 = 1.0f / 128.f;
        constexpr float pProp_127 = 1.0f / 127.f;
        const float        flx       = lx > 0 ? lx * pProp_127 : lx * pProp_128;
        const float        fly       = ly > 0 ? ly * pProp_127 : ly * pProp_128;
        const float        frx       = rx > 0 ? rx * pProp_127 : rx * pProp_128;
        const float        fry       = ry > 0 ? ry * pProp_127 : ry * pProp_128;
        EVENT_KEY          ev(flx, fly, 0, player, frx, fry, EVENT_TYPE_ACTIONS::ONMOVEJOYSTICK);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onInfoDeviceJoystick(int player, int maxNumberButton, const char *strDeviceName, const char *extraInfo)
    {
        INFO_JOYSTICK_INIT_PLAYER ev(player, maxNumberButton, strDeviceName, extraInfo);
        this->pushEvent(&ev);
    }

    void CORE_MANAGER::onResizeWindow(HWND, int width, int height)
    {
        EVENT_KEY ev(static_cast<float>(width),static_cast<float>(height),0,EVENT_TYPE_ACTIONS::ONRESIZEWINDOW);
        this->pushEvent(&ev);
    }

    void CORE_MANAGER::forceRestore()
    {
        while (!this->onLostDevice(static_cast<int>(this->device->backBufferWidth),static_cast<int>(this->device->backBufferHeight),0,0));
    }

    unsigned int CORE_MANAGER::addPlugin(PLUGIN * plugin)
    {
        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
        {
            const PLUGIN * thatPlugin = this->lsPlugins[i];
            if(plugin == thatPlugin)
            {
                return i;
            }
        }
        if(plugin != nullptr)
        {
            this->lsPlugins.push_back(plugin);
            // TODO: check this
            void * handle = this->device->window.getHwnd();
            plugin->onSubscribe(static_cast<int>(this->device->backBufferWidth),static_cast<int>(this->device->backBufferHeight),handle);
            return this->lsPlugins.size() - 1;
        }
        return 0xffffffff;
    }

    void CORE_MANAGER::setMinMaxSizeWindow(int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y)
    {
        this->device->window.setMinSizeAllowed(min_x,min_y);
        this->device->window.setMaxSizeAllowed(max_x,max_y);
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