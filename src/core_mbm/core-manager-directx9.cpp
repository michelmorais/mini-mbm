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

#include <directx9-specific.h>

#include "dummy-engine.h" // for compiler_message, you can remove it after implement the functions

#include <core-manager.h>
#include <device.h>
#include <scene.h>
#include <renderizable.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <util-interface.h>
#include <audio-interface.h>
#include <version/version.h>
#include <miniz-wrap/miniz-wrap.h>
#include <cassert>
#include <algorithm>
#include <cstring>
#include <log-util.h>
#include <cr-static-local.h>
#include <plugin-callback.h>
#include <dynamic-var.h>


namespace mbm
{
    
    enum WHICH_FOR : char
    {
        WFOR_INITIAL,
        WFOR_2DS,
        WFOR_2DW,
        WFOR_3D,
        WFOR_DONE
    };

    enum STEP_RETORE : char
    {
        STEP_RES_INIT_GL,
        STEP_RES_DRAW_HOURGLASS,
        STEP_RES_OBJ,
        STEP_RES_END,
    };

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
    
    
    bool CORE_MANAGER::onLostDevice(int width, int height,const int px,const int py)
    {
        if (stepRestore == STEP_RES_INIT_GL)
        {
            #if defined _DEBUG
            ERROR_LOG("onLostDevice step %d",stepRestore);
            #endif

            const HRESULT hr = this->device->specificContextDevice->pd3dDevice->TestCooperativeLevel();

            if (FAILED(hr))
            {
                // Se o dispositivo foi perdido, não renderiza até carregar de volta
                if (D3DERR_DEVICELOST == hr || D3DERR_DRIVERINTERNALERROR == hr)
                    return false;
                // Verifica se precisa resetar o dispositivo
                if (D3DERR_DEVICENOTRESET == hr)
                {
                    TEXTURE_MANAGER::getInstance()->release();
                    MESH_MANAGER::getInstance()->release();
                    if (this->device->specificContextDevice->pD3D != NULL)
                        this->device->specificContextDevice->pD3D->Release();
                    this->device->specificContextDevice->pD3D = NULL;

                    if (this->device->specificContextDevice->pd3dDevice != NULL)
                        this->device->specificContextDevice->pd3dDevice->Release();
                    this->device->specificContextDevice->pd3dDevice = NULL;

                    
                    return false;
                }
                return false;
            }
			//TODO: test lost device DirectX9
            #define __nameAplication "Mini-mbm " MBM_VERSION " DUMMY"
            if (initGraphics(__nameAplication, width, height,px,py, false,false))
            {
                #if defined _DEBUG
                    WARN_LOG("onLostDevice step %d function initGraphics sucess!",stepRestore);
                #endif
                
                this->device->__percXcam2dScale = 1.0f / this->device->camera.scale2d.x;
                this->device->__percYcam2dScale = 1.0f / this->device->camera.scale2d.y;
                this->adjustScaleScreen2d();
                stepRestore = STEP_RES_DRAW_HOURGLASS;
                return false;
            }
            else
            {
                #if defined _DEBUG
                    WARN_LOG("onLostDevice step %d function initGraphics failed!",stepRestore);
                #endif
                return false;
            }
        }
        else if (stepRestore == STEP_RES_DRAW_HOURGLASS)
        {
            #if defined _DEBUG
            WARN_LOG("onLostDevice step %d draw Hourglass.",stepRestore);
            #endif
            if (SUCCEEDED(this->device->specificContextDevice->pd3dDevice->BeginScene()))
            {

                device->setProjectionMode(false, device->backBufferWidth, device->backBufferHeight);
                device->setDephtTest(false);
                device->clearDepthColored();
                if (device->scene)
                    device->scene->onRestore(0); //true means: no call restore,  just to prepare the screen.
                stepRestore = STEP_RES_OBJ;
                this->which_for = WFOR_INITIAL;
                //Swap buffers
                this->device->specificContextDevice->pd3dDevice->Present(NULL, NULL, NULL, NULL);
            }
            return false;
        }
        else if (stepRestore == STEP_RES_OBJ)
        {
            device->setProjectionMode(false, device->backBufferWidth, device->backBufferHeight);
            device->setDephtTest(false);
            device->clearDepthColored();
            switch(this->which_for)
            {
                case WFOR_INITIAL:
                {
                    #if defined _DEBUG
                        WARN_LOG("onLostDevice step %d restoring objs.",stepRestore);
                    #endif
                    const auto t = static_cast<float>(this->device->lsObjectRender2DW.size() + this->device->lsObjectRender2DS.size() + this->device->lsObjectRender3D.size());
                    if(t > 0.0f)
                    {
                        this->totalForByLoop = static_cast<uint32_t>(std::ceil(t / 60.0f));//1 seconds should be loaded all objects
                        this->stepRestoreInfo = 98.0f /  t * static_cast<float>(this->totalForByLoop);
                    }
                    else
                    {
                        this->stepRestoreInfo = 0.001f;
                        this->totalForByLoop = 1;
                    }
                    this->percentRestoreInfo = 0.0f;
                    this->which_for = WFOR_2DW;
                    this->indexOnRestore = 0;
                    return false;
                }
                break;
                case WFOR_2DW:
                {
                    if (SUCCEEDED(this->device->specificContextDevice->pd3dDevice->BeginScene()))
                    {
                        for (uint32_t i = this->indexOnRestore, j = 0; i < this->device->lsObjectRender2DW.size(); ++i)
                        {
                            RENDERIZABLE* ptr = this->device->lsObjectRender2DW[i];
                            const bool    alwaysRenderize = ptr->alwaysRenderize;
                            const bool    enableRender = ptr->enableRender;
                            ptr->alwaysRenderize = false;
                            ptr->enableRender = false;
                            if (ptr->onRestoreDevice())
                            {
                                ptr->alwaysRenderize = alwaysRenderize;
                                ptr->enableRender = enableRender;
                            }
                            if (++j >= this->totalForByLoop)
                            {
                                this->indexOnRestore = (i + 1);
                                this->percentRestoreInfo += this->stepRestoreInfo;
                                if (device->scene)
                                    device->scene->onRestore(static_cast<int>(std::ceil(this->percentRestoreInfo > 98.9f ? 98.9f : this->percentRestoreInfo)));
                                break;
                            }
                        }
                        //Swap buffers
                        this->device->specificContextDevice->pd3dDevice->Present(NULL, NULL, NULL, NULL);
                        return false;
                    }
                    this->indexOnRestore = 0;
                    this->which_for = WFOR_2DS;
                    return false;
                }
                break;
                case WFOR_2DS:
                {
                    if (SUCCEEDED(this->device->specificContextDevice->pd3dDevice->BeginScene()))
                    {
                        for (uint32_t i = this->indexOnRestore, j = 0; i < this->device->lsObjectRender2DS.size(); ++i)
                        {
                            RENDERIZABLE* ptr = this->device->lsObjectRender2DS[i];
                            const bool    alwaysRenderize = ptr->alwaysRenderize;
                            const bool    enableRender = ptr->enableRender;
                            ptr->alwaysRenderize = false;
                            ptr->enableRender = false;
                            if (ptr->onRestoreDevice())
                            {
                                ptr->alwaysRenderize = alwaysRenderize;
                                ptr->enableRender = enableRender;
                            }
                            if (++j >= this->totalForByLoop)
                            {
                                this->indexOnRestore = (i + 1);
                                this->percentRestoreInfo += this->stepRestoreInfo;
                                if (device->scene)
                                    device->scene->onRestore(static_cast<int>(std::ceil(this->percentRestoreInfo > 98.9f ? 98.9f : this->percentRestoreInfo)));
                                break;
                            }
                            //Swap buffers
                            this->device->specificContextDevice->pd3dDevice->Present(NULL, NULL, NULL, NULL);
                            return false;
                        }
                    }
                    this->indexOnRestore = 0;
                    this->which_for = WFOR_3D;
                    return false;
                }
                break;
                case WFOR_3D:
                {
                    if (SUCCEEDED(this->device->specificContextDevice->pd3dDevice->BeginScene()))
                    {
                        for (uint32_t i = this->indexOnRestore, j = 0; i < this->device->lsObjectRender3D.size(); ++i)
                        {
                            RENDERIZABLE* ptr = this->device->lsObjectRender3D[i];
                            const bool    alwaysRenderize = ptr->alwaysRenderize;
                            const bool    enableRender = ptr->enableRender;
                            ptr->alwaysRenderize = false;
                            ptr->enableRender = false;
                            if (ptr->onRestoreDevice())
                            {
                                ptr->alwaysRenderize = alwaysRenderize;
                                ptr->enableRender = enableRender;
                            }
                            if (++j >= this->totalForByLoop)
                            {
                                this->indexOnRestore = (i + 1);
                                this->percentRestoreInfo += this->stepRestoreInfo;
                                if (device->scene)
                                    device->scene->onRestore(static_cast<int>(std::ceil(this->percentRestoreInfo > 98.9f ? 98.9f : this->percentRestoreInfo)));
                                break;
                            }
                        }
                        //Swap buffers
                        this->device->specificContextDevice->pd3dDevice->Present(NULL, NULL, NULL, NULL);
                        return false;
                    }
                    this->indexOnRestore = 0;
                    this->which_for = WFOR_DONE;
                }
                break;
                default:{};
            }
            stepRestore = STEP_RES_END;
            return false;
        }
        else if (stepRestore == STEP_RES_END)
        {
            #if defined _DEBUG
                WARN_LOG("onLostDevice step %d resumeGame",stepRestore);
            #endif
            stepRestore             = STEP_RES_INIT_GL;
            device->clearBackGround = true;
            this->device->resumeGame();
            this->device->resumeTimer();
            if (device->scene)
                device->scene->onRestore(100);
            return true;
        }
        return false;
    }

    bool CORE_MANAGER::initGraphics(const char *nameAplication, int width, int height, const int px, const int py, const bool border,const bool enable_resize)
    {
        int x = width;
        int y = height;
		DEVICE* device = DEVICE::getInstance();
        device->window.setNameAplication(nameAplication);
        if (!device->window.init(nameAplication, x, y, px, py, enable_resize, enable_resize, enable_resize, false, nullptr, border == false,
                                       this->idIcon,false))
        {
            device->window.messageBox("error on init app ... will be closed ");
            PRINT_IF_DEBUG( "error on init app ... will be closed %s", "error on create window");
            return false;
        }
        device->window.setMinSizeAllowed(800,600);
        HWND mNativeWindow = device->window.getHwnd();
        RECT rect;
        if (!GetClientRect(mNativeWindow, &rect))
        {
            //MessageBoxW(mNativeWindow, L"error on get the window size!", "DEVICE", MB_OK | MB_ICONERROR);
            rect.right  = width;
            rect.bottom = height;
            rect.left   = 0;
            rect.top    = 0;
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

            ERROR_AT(__LINE__, __FILE__,"failed to create hardware device '%s'", "Direct3DCreate9");
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

        D3DPRESENT_PARAMETERS				d3dParams;
        ZeroMemory(&d3dParams, sizeof(d3dParams));
        d3dParams.BackBufferWidth = x;
        d3dParams.BackBufferHeight = y;
        d3dParams.BackBufferFormat = D3DFMT_A8R8G8B8;
        d3dParams.BackBufferCount = 1;
        d3dParams.MultiSampleType = D3DMULTISAMPLE_NONE;//Use pD3D->CheckDeviceMultiSampleType
        d3dParams.MultiSampleQuality = 0;
        d3dParams.SwapEffect = D3DSWAPEFFECT_COPY;
        d3dParams.hDeviceWindow = mNativeWindow;
        d3dParams.Windowed = true;//Full Screen = false
		d3dParams.EnableAutoDepthStencil = true;//Keep / create the Buffer Depht/Stencil automatically
        d3dParams.AutoDepthStencilFormat = D3DFMT_D24S8;//Bits Reservados Para O Stencil = 8
        d3dParams.Flags = 0;
        d3dParams.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;//Rate render
		d3dParams.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;//Present imediately

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
                ERROR_AT(__LINE__,__FILE__, "failed to create software device");
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

        this->device->specificContextDevice->pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);//Turn on the face oclusion
        this->device->specificContextDevice->pd3dDevice->SetRenderState(D3DRS_LIGHTING, false);//Turn off ilumination
        this->device->specificContextDevice->pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);//Turn on Zbuffer
		//TODO: set matrix mode to world
		
        //optional states
        this->device->specificContextDevice->pd3dDevice->SetRenderState(D3DRS_AMBIENT, D3DCOLOR_XRGB(100, 100, 100));
        //enable blender
        this->device->specificContextDevice->pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, true);
        
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

    int CORE_MANAGER::loop()
    {
        static bool variablesInitialized = false;
        if (!device)
            return -1;
        if (!variablesInitialized)
        {
            // Cfg shader from memory----
            if (!this->device->cfg.parserCFGFromResource())
            {
                PRINT_IF_DEBUG( "\nerror on Parse CFG from memory.");
                return -1;
            }
            this->device->cfg.sortShader();
            device->setProjectionMode(true, device->backBufferWidth, device->backBufferHeight);
            this->device->updateFps();
            initEnableRenders();
            this->_updateDimFrustum();
            variablesInitialized                  = true;
            this->device->camera.expectedScreen.x = this->device->backBufferWidth;
            this->device->camera.expectedScreen.y = this->device->backBufferHeight;
        }
        MSG messageMain;
        memset(&messageMain, 0, sizeof(messageMain));
        while (messageMain.message != WM_QUIT && device->run && this->device->window.run)
        {
            this->device->window.doEvents();
            bool first_menu = true;
            while (mbm::WINDOW::isAnyMenuVisible() && device->window.run)
            {
                if (first_menu)
                {
                    Sleep(50);
                    mbm::WINDOW::refreshMenu();
                }
                this->device->window.doEvents();
                if (first_menu)
                {
                    Sleep(50);
                    mbm::WINDOW::refreshMenu();
                }
                first_menu = false;
            }
            if (!this->device->window.run)
                break;

            INFO_JOYSTICK_INIT_PLAYER info;
            while (this->popEvent(&info))
            {
                if (this->device->scene && this->__sceneWasInit)
                    this->device->scene->onInfoDeviceJoystick(info.player, info.maxNumberButton, info.deviceName.c_str(),
                                                              info.extraInfo.c_str());
            }
            //if (FAILED(this->device->specificContextDevice->pd3dDevice->BeginScene()))
            //{
			//	ERROR_AT(__LINE__, __FILE__, "failed to begin the scene");
            //    return 1;
            //}
            for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
            {
                PLUGIN * plugin = this->lsPlugins[i];
                plugin->onBeginRender();
            }
            EVENT_KEY event;
            while (this->popEvent(&event))
            {
                switch (event.eventType)
                {
                    case UNKNOWN: {
                    }
                    break;
                    case ONRESIZEWINDOW:
                    {
                        #pragma message(REMINDER_TODO "  Implement on resize windows here")
                        this->device->backBufferWidth  = event.x;
                        this->device->backBufferHeight = event.y;
                        if(this->device->scene)
                            this->device->scene->onResizeWindow();
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onResizeWindow(static_cast<int>(event.x),static_cast<int>(event.y));
                        }
                    }
                    break;
                    case ONTOUCHDOWN:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchDown(event.key, event.x, event.y);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onTouchDown(event.key, event.x, event.y);
                        }
                    }
                    break;
                    case ONTOUCHUP:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchUp(event.key, event.x, event.y);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onTouchUp(event.key, event.x, event.y);
                        }
                    }
                    break;
                    case ONTOUCHMOVE:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchMove(event.key, event.x, event.y);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onTouchMove(event.key, event.x, event.y);
                        }
                    }
                    break;
                    case ONTOUCHZOOM:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchZoom((float)event.key);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onTouchZoom((float)event.key);
                        }
                    }
                    break;
                    case ONKEYDOWN:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyDown(event.key);
                        if(event.key == VK_CAPITAL)
                        {
                            if ((GetKeyState(VK_CAPITAL) & 0x0001)!=0)
                                this->keyCapsLockState = true;
                            else
                                this->keyCapsLockState = false;
                        }
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onKeyDown(event.key);
                        }
                    }
                    break;
                    case ONKEYUP:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyUp(event.key);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onKeyUp(event.key);
                        }
                    }
                    break;
                    case ONDOUBLECLICK:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onDoubleClick(event.x, event.y, event.key);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onDoubleClick(event.x, event.y, event.key);
                        }
                    }
                    break;
                    case ONSTREAMSTOPED: {
                    }
                    break;
                    case ONCALLBACKCOMMANDS: {
                    }
                    break;
                    case ONKEYDOWNJOYSTICK:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyDownJoystick(event.player, event.key);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onKeyDownJoystick(event.player, event.key);
                        }
                    }
                    break;
                    case ONKEYUPJOYSTICK:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyUpJoystick(event.player, event.key);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onKeyUpJoystick(event.player, event.key);
                        }
                    }
                    break;
                    case ONMOVEJOYSTICK:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onMoveJoystick(event.player, event.lx, event.ly, event.rx, event.ry);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onMoveJoystick(event.player, event.lx, event.ly, event.rx, event.ry);
                        }
                    }
                    break;
                }
                if (!this->device->run)
                {
                    break;
                }
            }
            
            this->update();
            for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
            {
                PLUGIN * plugin = this->lsPlugins[i];
                plugin->onLoop(this->device->delta);
            }
            this->render();
            for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
            {
                PLUGIN * plugin = this->lsPlugins[i];
                plugin->onEndRender();
            }
            //this->device->specificContextDevice->pd3dDevice->EndScene();
			//Swap buffers
            this->device->specificContextDevice->pd3dDevice->Present(NULL, NULL, NULL, NULL);
        }
        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
        {
            PLUGIN * plugin = this->lsPlugins[i];
            plugin->onDestroy();
        }
        if(this->device->audioInterface)
            this->device->audioInterface->stopAll();
        return 0;
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

    bool CORE_MANAGER::renderToTargets()
    {
        bool oneRender = false;
        for (auto renderTarget : this->device->lsObjectRenderToTarget)
        {
            if (!renderTarget->isObjectOnFrustum)
                continue;
            #pragma message(REMINDER_TODO "  set viewport to render target")
            
            if (!renderTarget->render2Texture())
            {
                #pragma message(REMINDER_TODO "  Set viewport to back buffer")
                this->device->camera.updateCam(true, static_cast<float>(device->backBufferWidth), static_cast<float>(device->backBufferHeight));
                return false;
            }
            oneRender = true;
        }
        if (oneRender)
        {
            #pragma message(REMINDER_TODO "  Set viewport to back buffer")
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
        this->onStop();
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