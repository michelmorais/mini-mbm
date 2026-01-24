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
#include <scene.h>
#include <render-2-texture.h>
#include <lodepng/lodepng.h>
#include <texture-manager.h>
#include <util-interface.h>

namespace mbm
{

    RENDERIZABLE_TO_TARGET::RENDERIZABLE_TO_TARGET(const SCENE* scene, const TYPE_CLASS newTypeClass, const bool _is3d, const bool _is2ds) noexcept :
        RENDERIZABLE(scene->getIdScene(), newTypeClass, _is3d, _is2ds)
    {
        this->specificConfig = new RENDER2TARGET_DIRECTX9();
        this->colorClearBackGround = COLOR(255, 255, 255); // alpha em 0 significa transparente
        this->colorClearBackGround.a = 1.0f;
        this->widthTexture = 0;
        this->heightTexture = 0;
    }

    RENDERIZABLE_TO_TARGET::~RENDERIZABLE_TO_TARGET()
    {
        // Deleting a void* pointer directly in C++ is undefined behavior and should be avoided. 
        delete static_cast<RENDER2TARGET_DIRECTX9*>(this->specificConfig);
    }

    RENDER2TARGET_DIRECTX9::~RENDER2TARGET_DIRECTX9()
    {
        release();
    }

    void RENDER2TARGET_DIRECTX9::release() noexcept
    {
        if (pRenderSurface)
            pRenderSurface->Release();
        pRenderSurface = nullptr;
    }

    bool RENDER_2_TEXTURE::saveAsPNG(const char* newFileOutNamePNG, const int x, const int y, const int _width, const int _height)
    {
        if(newFileOutNamePNG == nullptr)
            return log_util::fail(__LINE__,__FILE__,"file name to save png is null");
        if(!this->isLoaded())
            return log_util::fail(__LINE__,__FILE__,"render to texture is not loaded!");
        const RENDER2TARGET_DIRECTX9* sf = static_cast<const RENDER2TARGET_DIRECTX9*>(this->specificConfig);
        if(sf->pRenderSurface == nullptr)
            return log_util::fail(__LINE__,__FILE__,"Surface is null, texture is not created!");
        if(this->texture == nullptr)
            return log_util::fail(__LINE__,__FILE__,"texture is not created!");
        if(strcasecmp(newFileOutNamePNG,this->fileName.c_str()) == 0)
            return log_util::fail(__LINE__,__FILE__,"file name texture in is the same as render2texture [%s]!",fileName.c_str());
        if(x < 0 || _width <= 0 || (_width + x) > static_cast<int>(this->widthTexture))
            return log_util::fail(__LINE__,__FILE__,"size expected [0-0 %dx%d] got [%d-%d %dx%d]",this->widthTexture,this->heightTexture,x,y,_width,_height);
        if(y < 0 || _height <= 0 || (_height + y) > static_cast<int>(this->heightTexture))
            return log_util::fail(__LINE__,__FILE__,"size expected [0-0 %dx%d] got [%d-%d %dx%d]",this->widthTexture,this->heightTexture,x,y,_width,_height);

        //the render target surface (sf->pRenderSurface) is created with:
        //Pool : D3DPOOL_DEFAULT(GPU memory only)
        //Usage : D3DUSAGE_RENDERTARGET(used as render target)
        //DirectX9 rule : Surfaces in D3DPOOL_DEFAULT with D3DUSAGE_RENDERTARGET cannot be locked — 
        // they are GPU - exclusive.You need to use GetRenderTargetData() to copy from GPU to system memory first.
        const int channel = this->texture->useAlphaChannel ? 4 : 3;
        const int sizeImage = _width * _height * channel;
        IDirect3DDevice9* pd3dDevice = mbm::DEVICE::getInstance()->specificContextDevice->pd3dDevice;
        D3DSURFACE_DESC	descSurfaceDest;
        D3DLOCKED_RECT	lockDestRect;
        std::vector<uint8_t> imageData(sizeImage);

        if (FAILED(sf->pRenderSurface->GetDesc(&descSurfaceDest)))
        {
            PRINT_IF_DEBUG("failed to get description of texture");
            return false;
        }
        if (D3DFMT_A8R8G8B8 != descSurfaceDest.Format && descSurfaceDest.Format != D3DFMT_R8G8B8)
        {
            PRINT_IF_DEBUG("Format of texture not as expected D3DFMT_A8R8G8B8 or D3DFMT_R8G8B8");
            return false;
        }

        // Create a staging surface in SYSTEMMEM to copy the render target to
        IDirect3DSurface9* stagingSurface = nullptr;
        D3DFORMAT requested_format = this->texture->useAlphaChannel ? D3DFMT_A8R8G8B8 : D3DFMT_R8G8B8;
        HRESULT hrCreateStaging = pd3dDevice->CreateOffscreenPlainSurface(
            this->widthTexture, this->heightTexture,
            requested_format,
            D3DPOOL_SYSTEMMEM,
            &stagingSurface,
            nullptr);

        if (FAILED(hrCreateStaging) || stagingSurface == nullptr)
        {
            PRINT_IF_DEBUG("CreateOffscreenPlainSurface failed (hr=0x%08x)", hrCreateStaging);
            return false;
        }

        // Copy from GPU render target to system-memory staging surface
        HRESULT hrGetData = pd3dDevice->GetRenderTargetData(sf->pRenderSurface, stagingSurface);
        if (FAILED(hrGetData))
        {
            PRINT_IF_DEBUG("GetRenderTargetData failed (hr=0x%08x)", hrGetData);
            stagingSurface->Release();
            return false;
        }

        // Now lock the staging surface (which is in SYSTEMMEM and can be locked)
        D3DLOCKED_RECT lockStaging;
        HRESULT hrLock = stagingSurface->LockRect(&lockStaging, nullptr, D3DLOCK_READONLY);
        if (FAILED(hrLock))
        {
            PRINT_IF_DEBUG("LockRect on staging surface failed (hr=0x%08x)", hrLock);
            stagingSurface->Release();
            return false;
        }

        // Copy pixels from locked staging surface to imageData
        uint8_t* pixels = static_cast<uint8_t*>(lockStaging.pBits);
        const int bytesPerPixel = channel;
        // Copy pixels from locked staging surface to imageData
        // Swap R and B channels: D3DFMT_A8R8G8B8 is BGRA, PNG expects RGBA
        uint8_t* srcPixels = static_cast<uint8_t*>(lockStaging.pBits);
        // srcRow = y + row — offset by Y position to start at the crop region
        // srcCol = x + col — offset by X position to start at the crop region
        // dstIdx = row * _width * ... — destination still starts at (0,0) because output image has dimensions _width x _height
        // srcIdx = srcRow * lockStaging.Pitch + srcCol * bytesPerPixel — use full texture pitch from staging surface
        for (int row = 0; row < _height; ++row)
        {
            for (int col = 0; col < _width; ++col)
            {
                // Source position: offset by (x, y) in the full texture
                int srcRow = y + row;
                int srcCol = x + col;
                int srcIdx = srcRow * lockStaging.Pitch + srcCol * bytesPerPixel;

                // Destination position: (0, 0) in the output image (top-left)
                int dstIdx = row * _width * bytesPerPixel + col * bytesPerPixel;

                // BGRA -> RGBA
                imageData[dstIdx + 0] = srcPixels[srcIdx + 2];  // R from B position
                imageData[dstIdx + 1] = srcPixels[srcIdx + 1];  // G stays G
                imageData[dstIdx + 2] = srcPixels[srcIdx + 0];  // B from R position
                imageData[dstIdx + 3] = srcPixels[srcIdx + 3];  // A stays A
            }
        }

        stagingSurface->UnlockRect();
        stagingSurface->Release();

        // Encode and save PNG
        std::vector<unsigned char> png;
        // Not need to flip the image vertically, fixed inverting v in RENDER_2_TEXTURE::fillvertexQuad (when defined Directx)
        // this->flip_vertically(imageData.data(), _width, _height, channel);
        unsigned int errorPNG = lodepng::encode(png, imageData.data(), static_cast<unsigned int>(_width), static_cast<unsigned int>(_height), channel == 4 ? LCT_RGBA : LCT_RGB);
        if (errorPNG)
            return log_util::fail(__LINE__, __FILE__, "PNG encoding error  [%s]", lodepng_error_text(errorPNG));
        errorPNG = lodepng::save_file(png, newFileOutNamePNG);
        if (errorPNG)
            return log_util::fail(__LINE__, __FILE__, "PNG encoding error  [%s]", lodepng_error_text(errorPNG));
        return true;
    }
    
};
#endif // USE_DIRECTX9