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

#include "dummy-engine.h" // for compiler_message, you can remove it after implement the functions
#include <specific-directx9.h>
#include <texture-manager.h>
#include <renderizable.h>
#include <uber-image.h>
#include <image-resource.h>
#include <util-interface.h>
#include <device.h>

namespace mbm
{
    void TEXTURE::release()
    {
        IDirect3DTexture9* p3DTexture9 = static_cast<IDirect3DTexture9*>(ptrTexture);
        if (p3DTexture9)
        {
            p3DTexture9->Release();
        }
        ptrTexture      = nullptr;
        width           = 0;
        height          = 0;
        useAlphaChannel = false;
    }

    bool TEXTURE::loadFromData(const uint8_t *data, // Bitmap or uber image
                             const uint32_t w, const uint32_t h, const uint16_t depth,
                             const uint16_t channel, const bool hasAlpha)
    {
        if (!data)
            return false;

        mbm::UBER_IMG        uberImg;
        // this uncompress if (compressed)
        const uint8_t *img = uberImg.getImage8bitsPerPixel(data, w, h, depth, channel);
        if (img == nullptr)
        {
            ERROR_AT(__LINE__, __FILE__, "failed to get texture from DATA");
            return false;
        }
        this->width  = w;
        this->height = h;
        uint8_t *rgba_toDelete = nullptr;
        D3DFORMAT requested_format = D3DFMT_A8R8G8B8;
        if (channel == 3 && hasAlpha == false)
        {
            requested_format = D3DFMT_R8G8B8;
        }
        if (channel == 3 && hasAlpha == true) // requested to have alpha, so we added
        {
            uint8_t* rgba            = new uint8_t[width * height * 4];
            const uint32_t sizeImage = width * height * 3;
            rgba_toDelete            = rgba;
            for (uint32_t i = 0, j = 0; i < sizeImage; i += 3, j += 4)
            {
                const uint8_t r = img[i];
                const uint8_t g = img[i + 1];
                const uint8_t b = img[i + 2];
                rgba[j]         = r;
                rgba[j + 1]     = g;
                rgba[j + 2]     = b;
                rgba[j + 3]     = 0xff; // 255 - Opaco
            }
        }
        
        IDirect3DSurface9* surfaceDest = nullptr;
        D3DSURFACE_DESC	descSurfaceDest;
        D3DLOCKED_RECT	lockDestRect;
		mbm::DEVICE* device = mbm::DEVICE::getInstance();
        IDirect3DTexture9** pp3DTexture9 = reinterpret_cast<IDirect3DTexture9**>(&this->ptrTexture);

        const UINT mipMap = TEXTURE::no_filter ? 1 : 4;
        
		if (FAILED(device->specificContextDevice->pd3dDevice->CreateTexture(w,
            h,
            mipMap,
            D3DUSAGE_DYNAMIC,
            requested_format,
            D3DPOOL_DEFAULT,//,
            pp3DTexture9, nullptr)))
        {
            ERROR_AT(__LINE__, __FILE__, "failed to create dynamic texture ");
            if (rgba_toDelete)
                delete[] rgba_toDelete;
            return false;
        }
        IDirect3DTexture9* p3DTexture9 = *pp3DTexture9;
        if (FAILED(p3DTexture9->GetSurfaceLevel(0, &surfaceDest)))
        {
            ERROR_AT(__LINE__, __FILE__, "failed to get surface of texture");
            if (rgba_toDelete)
                delete[] rgba_toDelete;
            return false;
        }
        if (FAILED(surfaceDest->GetDesc(&descSurfaceDest)))
        {
            if (surfaceDest)
                surfaceDest->Release();
            if (rgba_toDelete)
                delete[] rgba_toDelete;
            ERROR_AT(__LINE__, __FILE__, "failed to get description of texture");
            return false;
        }
        if (FAILED(surfaceDest->LockRect(&lockDestRect, 0, D3DLOCK_DISCARD)))
        {
            if (surfaceDest)
                surfaceDest->Release();
            if (rgba_toDelete)
                delete[] rgba_toDelete;
            ERROR_AT(__LINE__, __FILE__, "failed to lock texture");
            return false;
        }

        if (D3DFMT_A8R8G8B8 != descSurfaceDest.Format && descSurfaceDest.Format != D3DFMT_R8G8B8)
        {
            if (surfaceDest)
            {
                surfaceDest->UnlockRect();
                surfaceDest->Release();
            }
            if (rgba_toDelete)
                delete[] rgba_toDelete;
            ERROR_AT(__LINE__, __FILE__, "Format of texture not as expected D3DFMT_A8R8G8B8 or D3DFMT_R8G8B8");
            return false;
        }
        const uint8_t* dataImage = img;
        if (rgba_toDelete)
        {
            dataImage = rgba_toDelete;
        }
        uint8_t* dataDest = static_cast<uint8_t*>(lockDestRect.pBits);
        if (descSurfaceDest.Format == D3DFMT_R8G8B8)
        {
            const uint32_t sizeImage = width * height * 3;
            for (uint32_t i = 0; i < sizeImage; i += 3)
            {
                const uint8_t r = dataImage[i];
                const uint8_t g = dataImage[i + 1];
                const uint8_t b = dataImage[i + 2];
                dataDest[i]     = b;
                dataDest[i + 1] = g;
                dataDest[i + 2] = r;
            }
        }
        else if (descSurfaceDest.Format == D3DFMT_A8R8G8B8)
        {
            const uint32_t sizeImage = width * height * 4;
            for (uint32_t i = 0; i < sizeImage; i += 4)
            {
                const uint8_t r = dataImage[i];
                const uint8_t g = dataImage[i + 1];
                const uint8_t b = dataImage[i + 2];
                const uint8_t a = dataImage[i + 3];
                dataDest[i]     = b; // blue
                dataDest[i + 1] = g; // green
                dataDest[i + 2] = r; // red
                dataDest[i + 3] = a; // alpha
            }
        }
        else
        {
            ERROR_AT(__LINE__, __FILE__, "Format of texture not as expected D3DFMT_A8R8G8B8 or D3DFMT_R8G8B8");
        }

        if (rgba_toDelete)
            delete[] rgba_toDelete;

        if (surfaceDest != nullptr)
        {
            surfaceDest->UnlockRect();
            surfaceDest->Release();
        }
        surfaceDest = nullptr;
        this->useAlphaChannel = hasAlpha ? true : false;
        return true;
    }
    
    bool TEXTURE::loadFromResourceData(const IMAGE_RESOURCE *image)
    {
        if (!image)
            return false;
        this->width           = image->width;
        this->height          = image->height;
        this->useAlphaChannel = true;
        #pragma message(REMINDER_TODO "  implement pixel store alignment");  
        
        if (idTexture == 0)
            return false;
        //TODO: implement bind texture
        if (TEXTURE::no_filter)
        { // TILE MAP Mode
            #pragma message(REMINDER_TODO "  implement set texture parameters for tile map mode");

        }
        else
        {
            #pragma message(REMINDER_TODO "  implement set texture parameters for normal mode");
        }
        return true;
    }

    

    TEXTURE * TEXTURE_MANAGER::createTextureRenderTarget(RENDERIZABLE_TO_TARGET *renderToTarget, 
                                                        const char *nickName,
                                                        const bool enableAlpha)
    {
        std::string fileNameBase    = util::getBaseName(nickName);
        const auto width         = static_cast<int>(renderToTarget->widthTexture);
        const auto height        = static_cast<int>(renderToTarget->heightTexture);
        if (fileNameBase.size() == 0)
            return nullptr;
        if (static_cast<int>(width) > this->maxTextureSize || static_cast<int>(height) > this->maxTextureSize)
        {
            PRINT_IF_DEBUG("max size to generate texture is  %d/%d.", width > height ? width : height,this->maxTextureSize);
            return nullptr;
        }
        TEXTURE *texture = lsTextures[fileNameBase];
        if (texture)
            return texture;
        texture = new TEXTURE();

        uint32_t idFrameBuffer  = 0;
        uint32_t idTexture2d    = 0;
        uint32_t idRenderBuffer = 0;
        #pragma message(REMINDER_TODO "  implement generate framebuffer, renderbuffer and");

        // texture
        #pragma message(REMINDER_TODO "  implement generate texture");

        if (TEXTURE::no_filter)
        { // TILE MAP Mode
            #pragma message(REMINDER_TODO "  implement set texture parameters for tile map mode");

        }
        else
        {
            #pragma message(REMINDER_TODO "  implement set texture parameters for normal mode");
        }

        if (enableAlpha)
        {
            #pragma message(REMINDER_TODO "  implement upload texture with RGBA format");
        }
        else
        {
            #pragma message(REMINDER_TODO "  implement upload texture with RGB format");
        }
        // depth buffer
        #pragma message(REMINDER_TODO "  implement bind renderbuffer and set storage");
        // frame buffer
        #pragma message(REMINDER_TODO "  implement bind framebuffer");
        // attachments
        #pragma message(REMINDER_TODO "  implement attach texture and renderbuffer to framebuffer");
        //
        
        renderToTarget->idFrameBuffer       = idFrameBuffer;
        renderToTarget->idDepthRenderbuffer = idRenderBuffer;
        renderToTarget->idTextureDynamic    = static_cast<int>(idTexture2d);
        texture->idTexture                  = idTexture2d;
        texture->width                      = static_cast<uint32_t>(width);
        texture->height                     = static_cast<uint32_t>(height);
        texture->useAlphaChannel            = enableAlpha;
        texture->fileName                   = std::move(fileNameBase);
        lsTextures[texture->fileName]       = texture;
        return texture;
    }
}

#endif // USE_DIRECTX9