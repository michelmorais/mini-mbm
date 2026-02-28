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

    struct PIXELS_FROM_3_DEPTH_TO_4
    {
        PIXELS_FROM_3_DEPTH_TO_4():rgba_toDelete(nullptr)
        {

        }
        ~PIXELS_FROM_3_DEPTH_TO_4()
        {
            if (rgba_toDelete)
                delete[] rgba_toDelete;
            rgba_toDelete = nullptr;
        }

        const uint8_t* get(const uint32_t width, const uint32_t height, const uint8_t* imgSource)
        {
            if (rgba_toDelete)
                delete[] rgba_toDelete;
            rgba_toDelete = nullptr;
            rgba_toDelete = new uint8_t[width * height * 4];
            const uint32_t sizeImage = width * height * 3;
            for (uint32_t i = 0, j = 0; i < sizeImage; i += 3, j += 4)
            {
                const uint8_t r = imgSource[i];
                const uint8_t g = imgSource[i + 1];
                const uint8_t b = imgSource[i + 2];
                rgba_toDelete[j] = r;
                rgba_toDelete[j + 1] = g;
                rgba_toDelete[j + 2] = b;
                rgba_toDelete[j + 3] = 0xff; // 255 - Opaco
            }
            return rgba_toDelete;
        }
    private:
        uint8_t* rgba_toDelete;
    };

    

    void copy_pixels_per_row_Pitch( D3DSURFACE_DESC	&descSurfaceDest, 
                                        const uint32_t width, 
                                        const uint32_t height, 
                                        D3DLOCKED_RECT & lockDestRect, 
                                        const uint8_t* dataImage) noexcept
    {   // compute bytes per pixel in source and destination
        const bool destIsA8R8G8B8 = (descSurfaceDest.Format == D3DFMT_A8R8G8B8);
        const uint32_t dstBpp = destIsA8R8G8B8 ? 4u : 3u;
        const uint32_t srcBpp = dstBpp; // dataImage already converted to match dst format (rgba_toDelete or image->data)
        const uint32_t rowSrcBytes = width * srcBpp;
        uint8_t* dataDest = static_cast<uint8_t*>(lockDestRect.pBits);
        uint8_t* destRowBase = dataDest;
        const uint8_t* srcRowBase = dataImage;

        if (descSurfaceDest.Format != D3DFMT_R8G8B8 && descSurfaceDest.Format != D3DFMT_A8R8G8B8)
        {
            ERROR_AT(__LINE__, __FILE__, "Format of texture not as expected D3DFMT_A8R8G8B8 or D3DFMT_R8G8B8");
        }

        // copy per row respecting Pitch. Also swap RGB->BGR when required by DirectX
        for (uint32_t y = 0; y < height; ++y)
        {
            uint8_t* destRow = destRowBase + static_cast<size_t>(y) * lockDestRect.Pitch;
            const uint8_t* srcRow = srcRowBase + static_cast<size_t>(y) * rowSrcBytes;

            if (destIsA8R8G8B8)
            {
                // dest layout: B G R A
                for (uint32_t x = 0, s = 0, d = 0; x < width; ++x, s += 4, d += 4)
                {
                    const uint8_t r = srcRow[s + 0];
                    const uint8_t g = srcRow[s + 1];
                    const uint8_t b = srcRow[s + 2];
                    const uint8_t a = srcRow[s + 3];
                    destRow[d + 0] = b;
                    destRow[d + 1] = g;
                    destRow[d + 2] = r;
                    destRow[d + 3] = a;
                }
            }
            else // D3DFMT_R8G8B8
            {
                // dest layout: B G R (3 bytes). Note: pitch may include padding.
                for (uint32_t x = 0, s = 0, d = 0; x < width; ++x, s += 3, d += 3)
                {
                    const uint8_t r = srcRow[s + 0];
                    const uint8_t g = srcRow[s + 1];
                    const uint8_t b = srcRow[s + 2];
                    destRow[d + 0] = b;
                    destRow[d + 1] = g;
                    destRow[d + 2] = r;
                }
            }
        }
    }

    static bool created3dTexture(IDirect3DTexture9** pp3DTexture9, 
                                const bool no_filter, 
                                const uint8_t* data,
                                const uint32_t width, const uint32_t height,
                                const uint16_t channel, const bool hasAlpha)
    {
        auto pd3dDevice = mbm::DEVICE::getInstance()->specificContextDevice->pd3dDevice;
        D3DFORMAT requested_format = D3DFMT_A8R8G8B8;
        PIXELS_FROM_3_DEPTH_TO_4 pixels_from_3_depth_to_4;
        IDirect3DSurface9* surfaceDest = nullptr;
        D3DSURFACE_DESC	descSurfaceDest;
        D3DLOCKED_RECT	lockDestRect;

        if (channel == 3 && hasAlpha == false)
        {
            requested_format = D3DFMT_R8G8B8;
        }
        if (channel == 3 && hasAlpha == true) // requested to have alpha, so we added
        {
            data = pixels_from_3_depth_to_4.get(width, height, data);
        }
        //Mipmap levels (mipMap) ? affects rendering quality but not the sampling method
        //Sampler states ? controls how texels are interpolated during rendering (controled by DEVICE::disableFilteringForPixelPerfect)
        // Match OpenGL ES: single mip level to avoid edge bleeding (OpenGL uses no mip chain for 2D).
        const UINT mipMap = 1U;
        const DWORD usage = D3DUSAGE_DYNAMIC;

        if (FAILED(pd3dDevice->CreateTexture(width,
            height,
            mipMap,
            usage,
            requested_format,
            D3DPOOL_DEFAULT,
            pp3DTexture9, nullptr)))
        {
            if (FAILED(pd3dDevice->CreateTexture(width,
                height,
                mipMap,
                usage,
                requested_format,
                D3DPOOL_DEFAULT,
                pp3DTexture9, nullptr)))
            {
                // Always upload as A8R8G8B8 and set alpha to 0xFF when source has no alpha ? avoids unsupported 24 - bit format.
                if (channel == 3 && hasAlpha == false && requested_format == D3DFMT_R8G8B8) // we will force 24 bit format
                {
                    data = pixels_from_3_depth_to_4.get(width, height, data);
                    requested_format = D3DFMT_A8R8G8B8;

                    if (FAILED(pd3dDevice->CreateTexture(width,
                        height,
                        mipMap,
                        usage,
                        requested_format,
                        D3DPOOL_DEFAULT,//,
                        pp3DTexture9, nullptr)))
                    {
                        PRINT_IF_DEBUG("failed to create dynamic texture ");
                        return false;
                    }
                }
                else
                {
                    PRINT_IF_DEBUG("failed to create dynamic texture ");
                    return false;
                }
            }
        }

        IDirect3DTexture9* p3DTexture9 = *pp3DTexture9;
        if (FAILED(p3DTexture9->GetSurfaceLevel(0, &surfaceDest)))
        {
            PRINT_IF_DEBUG("failed to get surface of texture");
            return false;
        }
        if (FAILED(surfaceDest->GetDesc(&descSurfaceDest)))
        {
            if (surfaceDest)
                surfaceDest->Release();
            PRINT_IF_DEBUG("failed to get description of texture");
            return false;
        }
        if (D3DFMT_A8R8G8B8 != descSurfaceDest.Format && descSurfaceDest.Format != D3DFMT_R8G8B8)
        {
            if (surfaceDest)
            {
                surfaceDest->Release();
            }
            PRINT_IF_DEBUG("Format of texture not as expected D3DFMT_A8R8G8B8 or D3DFMT_R8G8B8");
            return false;
        }

        DWORD lockFlags = (usage & D3DUSAGE_DYNAMIC) ? D3DLOCK_DISCARD : 0;
        HRESULT hrLock = surfaceDest->LockRect(&lockDestRect, 0, lockFlags);
        if (FAILED(hrLock))
        {
            surfaceDest->Release();
            surfaceDest = nullptr;
            // LockRect failed on GPU surface ? try system-memory fallback:
            IDirect3DTexture9* texSys = nullptr;
            HRESULT hrCreateSys = pd3dDevice->CreateTexture(
                width, height,                          // width, height
                1,                                      // single mip level for system copy
                0,                                      // usage for system mem copy
                requested_format,
                D3DPOOL_SYSTEMMEM,
                &texSys,
                nullptr);

            if (FAILED(hrCreateSys) || texSys == nullptr)
            {
                PRINT_IF_DEBUG("CreateTexture (SYSTEMMEM) failed (hr=0x%08x)", hrCreateSys);
                return false;
            }

            IDirect3DSurface9* surfSys = nullptr;
            HRESULT hrGetSurf = texSys->GetSurfaceLevel(0, &surfSys);
            if (FAILED(hrGetSurf) || surfSys == nullptr)
            {
                PRINT_IF_DEBUG("GetSurfaceLevel (SYSTEMMEM) failed (hr=0x%08x)", hrGetSurf);
                texSys->Release();
                return false;
            }

            D3DSURFACE_DESC descSys;
            if (FAILED(surfSys->GetDesc(&descSys)))
            {
                PRINT_IF_DEBUG("GetDesc (SYSTEMMEM) failed");
                surfSys->Release();
                texSys->Release();
                return false;
            }

            D3DLOCKED_RECT lrSys;
            HRESULT hrLockSys = surfSys->LockRect(&lrSys, nullptr, 0);
            if (FAILED(hrLockSys))
            {
                PRINT_IF_DEBUG("LockRect (SYSTEMMEM) failed (hr=0x%08x)", hrLockSys);
                surfSys->Release();
                texSys->Release();
                return false;
            }

            // copy pixels into system-memory surface
            copy_pixels_per_row_Pitch(descSys, width, height, lrSys, data);

            surfSys->UnlockRect();
            // Now copy systemmem texture -> GPU texture
            HRESULT hrUpdate = pd3dDevice->UpdateTexture(texSys, p3DTexture9);
            if (FAILED(hrUpdate))
            {
                PRINT_IF_DEBUG("UpdateTexture failed (hr=0x%08x)", hrUpdate);
                surfSys->Release();
                texSys->Release();
                return false;
            }

            // cleanup system mem resources
            surfSys->Release();
            texSys->Release();

            // GPU surface not locked but top-level has been updated ? continue without LockRect.
        }
        else
        {
            copy_pixels_per_row_Pitch(descSurfaceDest, width, height, lockDestRect, data);
            surfaceDest->UnlockRect();
            surfaceDest->Release();
        }
        surfaceDest = nullptr;
        return true;
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
            PRINT_IF_DEBUG("failed to get texture from DATA");
            return false;
        }
        IDirect3DTexture9** pp3DTexture9 = reinterpret_cast<IDirect3DTexture9**>(&this->ptrTexture);
        this->width  = w;
        this->height = h;
        this->useAlphaChannel = hasAlpha ? true : false;
        return created3dTexture(pp3DTexture9,
            TEXTURE::no_filter,
            data,
            width, height,
            channel, hasAlpha);
    }
    
    bool TEXTURE::loadFromResourceData(const IMAGE_RESOURCE *image)
    {
        if (image == nullptr)
            return false;
        this->width           = image->width;
        this->height          = image->height;
        this->useAlphaChannel = true;
        const int  channel    = 4;
        const bool alpha      = true;

        IDirect3DTexture9** pp3DTexture9 = reinterpret_cast<IDirect3DTexture9**>(&this->ptrTexture);
        return created3dTexture(pp3DTexture9,
            TEXTURE::no_filter,
            reinterpret_cast<const uint8_t*>(image->data),
            image->width, image->height,
            channel, alpha);
    }

    TEXTURE* TEXTURE_MANAGER::loadNativeEngine(const char* fileName, const bool forceAlpha) // load native engine (e.g.: Directx LoadTextureFromFile, Metal). Implemented specific
    {
        if (fileName == nullptr)
            return nullptr;
        std::string fileNameBase = util::getBaseName(fileName);
        TEXTURE* tex = lsTextures[fileNameBase];
        if (tex)
            return tex;
        fileName = getFilePathTexture(fileName, nullptr);
        if (fileName == nullptr)
            return nullptr;
        tex = new TEXTURE();
        D3DXIMAGE_INFO infoTexture;
        infoTexture.Width = 0;
        infoTexture.Height = 0;
        auto pd3dDevice = mbm::DEVICE::getInstance()->specificContextDevice->pd3dDevice;
        if (SUCCEEDED(D3DXGetImageInfoFromFileA(fileName, &infoTexture)))
        {
            tex->width = infoTexture.Width;
            tex->height = infoTexture.Height;
            UINT MipLevels = 1U; // Match OpenGL ES: no mip chain for 2D textures (avoids edge bleeding)
            D3DFORMAT tFormat = forceAlpha ? D3DFMT_A8R8G8B8 : D3DFMT_UNKNOWN;
            IDirect3DTexture9** pp3DTexture9 = reinterpret_cast<IDirect3DTexture9**>(&tex->ptrTexture);
            
            constexpr DWORD Usage = 0; //D3DUSAGE_RENDERTARGET D3DUSAGE_DYNAMIC 
            DWORD Filter = D3DX_FILTER_BOX;
            DWORD MipFilter = 0;
            if (MipLevels == 0)//queremos uma imagem sem mipmap e filtro
            {
                Filter = D3DX_DEFAULT;//Mip Filter
                MipFilter = D3DX_FILTER_BOX;
            }
            else
            {
                Filter = D3DX_DEFAULT;//Mip Filter
                MipFilter = D3DX_FILTER_NONE;
            }
            if (FAILED(D3DXCreateTextureFromFileExA(pd3dDevice,
                fileName,
                tex->width,
                tex->height,
                MipLevels,//N?mero De miplevels Que desejamos Que a fun?ao crie>>>0 para Uma completa
                Usage,//D3DUSAGE_RENDERTARGET D3DUSAGE_DYNAMIC 
                tFormat,//D3DFORMAT Formato Do Pixel
                D3DPOOL_MANAGED,//Maneira qu a Textura sera armazenada Na mem?ria
                Filter,//filtro No carregamento Da Textura pode inverter U e V
                MipFilter,//Mip Filter
                0,//Color Key recorte De pixels
                &infoTexture,//Informa??o Da imagen 
                nullptr,//Paleta_Bitmap_True_Color_24_Bits Da imagem
                pp3DTexture9)))
            {
                delete tex;
                PRINT_IF_DEBUG("failed to load texture from file [%s] ", fileName);
                return nullptr;
            }
            tex->useAlphaChannel = forceAlpha;
            tex->fileName = fileName;
            lsTextures[fileNameBase] = (tex);
            return tex;
        }
        else
        {
            PRINT_IF_DEBUG("failed to load texture from file [%s] ", fileName);
            return nullptr;
        }
    }

    TEXTURE * TEXTURE_MANAGER::createTextureRenderTarget(RENDERIZABLE_TO_TARGET *renderToTarget, 
                                                        const char *nickName,
                                                        const bool enableAlpha)
    {
        std::string fileNameBase    = util::getBaseName(nickName);
        const auto width            = static_cast<int>(renderToTarget->widthTexture);
        const auto height           = static_cast<int>(renderToTarget->heightTexture);
        auto pd3dDevice             = mbm::DEVICE::getInstance()->specificContextDevice->pd3dDevice;

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

        IDirect3DTexture9** pp3DTexture9 = reinterpret_cast<IDirect3DTexture9**>(&texture->ptrTexture);
        const D3DFORMAT Format = enableAlpha ? D3DFMT_A8R8G8B8 : D3DFMT_X8R8G8B8;
        if (FAILED(pd3dDevice->CreateTexture(width, height, 1,
            D3DUSAGE_RENDERTARGET,
            Format, D3DPOOL_DEFAULT,
            pp3DTexture9,
            nullptr)))
        {
            ERROR_AT(__LINE__, __FILE__, "failed to create render to target texture");
            delete texture;
            return nullptr;
        }
        IDirect3DTexture9* p3DTexture9 = *pp3DTexture9;

        RENDER2TARGET_DIRECTX9* sf = static_cast<RENDER2TARGET_DIRECTX9*>(renderToTarget->specificConfig);

        if (FAILED(p3DTexture9->GetSurfaceLevel(0, &sf->pRenderSurface)))
        {
            delete texture;
            ERROR_AT(__LINE__, __FILE__, "failed to get surface description level");
            return nullptr;
        }
        texture->width                      = static_cast<uint32_t>(width);
        texture->height                     = static_cast<uint32_t>(height);
        texture->useAlphaChannel            = enableAlpha;
        texture->fileName                   = std::move(fileNameBase);
        lsTextures[texture->fileName]       = texture;
        return texture;
    }
}

#endif // USE_DIRECTX9