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


#include <specific-directx9.h>
#include <device.h>
#include <texture-manager.h>
#include <audio-interface.h>
#include <mesh-manager.h>

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
        // Depth+stencil only — colour is intentionally preserved so the 3D scene is not erased.
        // Use clearDepthColored() when you also need to repaint the background colour.
        specificContextDevice->pd3dDevice->Clear(0, NULL, D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, D3DCOLOR_XRGB(0, 0, 0), 1.0f, 0);
    }
    void DEVICE::clearDepthColored()
    {
        D3DCOLOR color = D3DCOLOR_COLORVALUE(this->colorClearBackGround.r, this->colorClearBackGround.g, this->colorClearBackGround.b,0xff);
        specificContextDevice->pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, color, 1.0f, 0);
    }

    const char* DEVICE::getBackendEngineName() const noexcept
    {
        return "Directx9";
    }

    const char* DEVICE::getBackendEngineVersion() const noexcept
    {
        return "9";
    }

    void DEVICE::setProjectionMode(const bool is3D, const float width, const float height)
    {
        IDirect3DDevice9* pd3dDevice = this->specificContextDevice->pd3dDevice;
        if (width > 0 && height > 0)
        {
            //TOD: check this
            const D3DVIEWPORT9 view_port = D3DVIEWPORT9{ 0, 0, static_cast<DWORD>(width), static_cast<DWORD>(height), 0.0f, 1.0f };
			pd3dDevice->SetViewport(&view_port);
        }
        if (width > 0)
            backBufferWidth = width;
        if (height > 0)
            backBufferHeight = height;
        if (width > 0 && height > 0)
            this->camera.updateCam(is3D, static_cast<float>(width), static_cast<float>(height));
        if (is3D)
        {
            const D3DMATRIX* matrixView = reinterpret_cast<const D3DMATRIX*>(&this->camera.matrixView);
            const D3DMATRIX* matrixProj = reinterpret_cast<const D3DMATRIX*>(&this->camera.matrixProj);
            pd3dDevice->SetTransform(D3DTS_VIEW, matrixView);
            pd3dDevice->SetTransform(D3DTS_PROJECTION, matrixProj);
        }
        else
        {
            const D3DMATRIX* matrixView2d = reinterpret_cast<const D3DMATRIX*>(&this->camera.matrixView2d);
            const D3DMATRIX* matrixOrtho  = reinterpret_cast<const D3DMATRIX*>(&this->camera.matrixOrtho);
            pd3dDevice->SetTransform(D3DTS_VIEW, matrixView2d);
            pd3dDevice->SetTransform(D3DTS_PROJECTION, matrixOrtho);
        }
        
    }

    const char* DEVICE::copyFileFromAsset(const char* assetName, const char* mode)// Meant to be used in Android / Iphone (others specific implementations can just return assetName).
    {
        return assetName;
    }

    void DEVICE::disableFilteringForPixelPerfect() noexcept//backend specific way to disable texture filtering for pixel perfect rendering
    {
        _pixelPerfectRenderingActive = true;
        IDirect3DDevice9* pd3dDevice = this->specificContextDevice->pd3dDevice;
		for (int i = 0; i < 2; ++i)
        {
            pd3dDevice->GetSamplerState(i, D3DSAMP_MINFILTER, &this->specificContextDevice->DWORD_D3DSAMP_MINFILTER[i]);
            pd3dDevice->GetSamplerState(i, D3DSAMP_MAGFILTER, &this->specificContextDevice->DWORD_D3DSAMP_MAGFILTER[i]);
            pd3dDevice->GetSamplerState(i, D3DSAMP_MIPFILTER, &this->specificContextDevice->DWORD_D3DSAMP_MIPFILTER[i]);
            // Point filtering (nearest neighbor - no interpolation)
            pd3dDevice->SetSamplerState(i, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            pd3dDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            pd3dDevice->SetSamplerState(i, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            pd3dDevice->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
            pd3dDevice->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
        }
    }

    void DEVICE::enableFilteringAfterPixelPerfect() noexcept
    {
        _pixelPerfectRenderingActive = false;
        IDirect3DDevice9* pd3dDevice = this->specificContextDevice->pd3dDevice;
        for (int i = 0; i < 2; ++i)
        {
            pd3dDevice->SetSamplerState(i, D3DSAMP_MINFILTER, this->specificContextDevice->DWORD_D3DSAMP_MINFILTER[i]);
            pd3dDevice->SetSamplerState(i, D3DSAMP_MAGFILTER, this->specificContextDevice->DWORD_D3DSAMP_MAGFILTER[i]);
            pd3dDevice->SetSamplerState(i, D3DSAMP_MIPFILTER, this->specificContextDevice->DWORD_D3DSAMP_MIPFILTER[i]);
            pd3dDevice->SetSamplerState(i, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
            pd3dDevice->SetSamplerState(i, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
		}
    }


}
#endif // USE_DIRECTX9