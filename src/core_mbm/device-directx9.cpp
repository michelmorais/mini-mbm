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


#if defined (USE_DIRECTX9)


#include "dummy-engine.h" // for compiler_message, you can remove it after implement the functions
#include <specific-directx9.h>

#include <device.h>
#include <scene.h>
#include <texture-manager.h>
#include <audio-interface.h>
#include <shapes.h>
#include <physics.h>
#include <renderizable.h>
#include <mesh-manager.h>
#include <util-interface.h>
#include <dynamic-var.h>

#include <plusWindows/defaultThemePlusWindows.h>

namespace mbm
{
    
    
    void DEVICE::initializeSpecificContext()
    {
        this->destroySpecificContext();
        this->specificContextDevice = new SPECIFIC_AUX_CONTEXT_DEVICE();
    }
    void DEVICE::destroySpecificContext()
    {
        if(this->specificContextDevice)
        {
            delete this->specificContextDevice;
            this->specificContextDevice = nullptr;
        }
    }

    void DEVICE::quit()
    {
        TEXTURE_MANAGER::release();
        MESH_MANAGER::release();
		releaseAudioManager();
		if (instanceDevice)
        {
            delete instanceDevice;
        }
        instanceDevice = nullptr;
    }

    void DEVICE::setDephtTest(const bool enable)
    {
        if (enable)
        {
            specificContextDevice->pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        }
        else
        {
            specificContextDevice->pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
        }
    }

    void DEVICE::clearDepth()
    {
        specificContextDevice->pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    }
    void DEVICE::clearDepthColored()
    {
        D3DCOLOR color = D3DCOLOR_COLORVALUE(this->colorClearBackGround.r, this->colorClearBackGround.g, this->colorClearBackGround.b,0xff);
        specificContextDevice->pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, color, 1.0f, 0);
    }

    const char* DEVICE::getBackendEngineName() const noexcept
    {
        return "Dummy engine";
    }

    const char* DEVICE::getBackendEngineVersion() const noexcept
    {
        return "Dummy engine version 1.0";
    }

    void DEVICE::setProjectionMode(const bool is3D, const float width, const float height)
    {
        if (width > 0 && height > 0)
        {
            //TOD: check this
            D3DVIEWPORT9 view_port = D3DVIEWPORT9{ 0, 0, static_cast<DWORD>(width), static_cast<DWORD>(height), 0.0f, 1.0f };
			this->specificContextDevice->pd3dDevice->SetViewport(&view_port);
        }
        if (width > 0)
            backBufferWidth = width;
        if (height > 0)
            backBufferHeight = height;
        if (width > 0 && height > 0)
            this->camera.updateCam(is3D, static_cast<float>(width), static_cast<float>(height));
    }

}
#endif // USE_DIRECTX9