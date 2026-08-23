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

#if defined(USE_DIRECTX11)

#include <texture-manager.h>
#include <renderizable.h>
#include <uber-image.h>
#include <image-resource.h>
#include <util-interface.h>
#include <device.h>

#include "specific-directx11-context.h"
#include "specific-directx11-render-target.h"
#include <d3d11.h>
#include <vector>

namespace mbm
{
    namespace
    {
        bool createTextureView(const uint8_t *pixels, const uint32_t width, const uint32_t height,
                               const uint16_t channel, ID3D11ShaderResourceView **view)
        {
            if (!pixels || !width || !height || (channel != 3 && channel != 4))
                return false;
            std::vector<uint8_t> rgba;
            const uint8_t *uploadPixels = pixels;
            if (channel == 3)
            {
                rgba.resize(static_cast<size_t>(width) * height * 4u);
                for (size_t source = 0, destination = 0; destination < rgba.size(); source += 3, destination += 4)
                {
                    rgba[destination] = pixels[source];
                    rgba[destination + 1] = pixels[source + 1];
                    rgba[destination + 2] = pixels[source + 2];
                    rgba[destination + 3] = 255;
                }
                uploadPixels = rgba.data();
            }
            D3D11_TEXTURE2D_DESC description = {};
            description.Width = width;
            description.Height = height;
            description.MipLevels = 1;
            description.ArraySize = 1;
            description.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            description.SampleDesc.Count = 1;
            description.Usage = D3D11_USAGE_IMMUTABLE;
            description.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            D3D11_SUBRESOURCE_DATA initialData = {};
            initialData.pSysMem = uploadPixels;
            initialData.SysMemPitch = width * 4u;
            ID3D11Device *device = DEVICE::getInstance()->getSpecificContextDevice()->device;
            ID3D11Texture2D *texture = nullptr;
            HRESULT result = device->CreateTexture2D(&description, &initialData, &texture);
            if (SUCCEEDED(result))
                result = device->CreateShaderResourceView(texture, nullptr, view);
            if (texture)
                texture->Release();
            return SUCCEEDED(result);
        }
    }

    void TEXTURE::release()
    {
        ID3D11ShaderResourceView *view = static_cast<ID3D11ShaderResourceView *>(getBackendTexturePointer());
        if (view)
            view->Release();
        setBackendTexturePointer(nullptr);
        width           = 0;
        height          = 0;
        this->setAlphaChannelEnabled(false);
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
        this->width  = w;
        this->height = h;
        this->setAlphaChannelEnabled(hasAlpha ? true : false);
        ID3D11ShaderResourceView *view = nullptr;
        if (!createTextureView(img, w, h, channel, &view))
            return false;
        setBackendTexturePointer(view);
        return true;
    }

    bool TEXTURE::loadFromResourceData(const IMAGE_RESOURCE *image)
    {
        if (image == nullptr)
            return false;
        this->width           = image->width;
        this->height          = image->height;
        this->setAlphaChannelEnabled(true);
        const int  channel    = 4;
        const bool alpha      = true;

        ID3D11ShaderResourceView *view = nullptr;
        if (!createTextureView(reinterpret_cast<const uint8_t *>(image->data), image->width, image->height,
                               channel, &view))
            return false;
        setBackendTexturePointer(view);
        return true;
    }

    TEXTURE* TEXTURE_MANAGER::loadNativeEngine(const char* fileName, const bool forceAlpha) // load native engine (e.g.: Directx LoadTextureFromFile, Metal). Implemented specific
    {
        (void)fileName;
        (void)forceAlpha;
        // DirectX 11 has no separate native file decoder. The common texture path already
        // attempted decoding before reaching this fallback, so never manufacture/cache a
        // TEXTURE without a shader-resource view here.
        return nullptr;
    }

    TEXTURE * TEXTURE_MANAGER::createTextureRenderTarget(RENDERIZABLE_TO_TARGET *renderToTarget,
                                                        const char *nickName,
                                                        const bool enableAlpha)
    {
        if (!renderToTarget || !nickName)
            return nullptr;
        std::string fileNameBase    = util::getBaseName(nickName);
        const auto width            = static_cast<int>(renderToTarget->getRenderTargetWidth());
        const auto height           = static_cast<int>(renderToTarget->getRenderTargetHeight());

        if (fileNameBase.size() == 0 || width <= 0 || height <= 0)
            return nullptr;
        const uint32_t maxTextureSize = getMaxTextureSize();
        if (static_cast<uint32_t>(width) > maxTextureSize || static_cast<uint32_t>(height) > maxTextureSize)
        {
            PRINT_IF_DEBUG("max size to generate texture is  %d/%d.", width > height ? width : height, maxTextureSize);
            return nullptr;
        }
        TEXTURE *texture = getCachedTexture(fileNameBase);
        if (texture)
            return texture;
        texture = new TEXTURE();

        RENDER2TARGET_DIRECTX11 *target = static_cast<RENDER2TARGET_DIRECTX11 *>(renderToTarget->getRenderTargetSpecificConfig());
        SPECIFIC_AUX_CONTEXT_DEVICE *context = DEVICE::getInstance()->getSpecificContextDevice();
        if (!target || !context || !context->device)
        {
            delete texture;
            return nullptr;
        }
        target->release();
        D3D11_TEXTURE2D_DESC colorDescription = {};
        colorDescription.Width = static_cast<UINT>(width);
        colorDescription.Height = static_cast<UINT>(height);
        colorDescription.MipLevels = 1;
        colorDescription.ArraySize = 1;
        colorDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        colorDescription.SampleDesc.Count = 1;
        colorDescription.Usage = D3D11_USAGE_DEFAULT;
        colorDescription.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        ID3D11Texture2D *colorTexture = nullptr;
        HRESULT result = context->device->CreateTexture2D(&colorDescription, nullptr, &colorTexture);
        if (SUCCEEDED(result))
            result = context->device->CreateRenderTargetView(colorTexture, nullptr, &target->renderTargetView);
        ID3D11ShaderResourceView *textureView = nullptr;
        if (SUCCEEDED(result))
            result = context->device->CreateShaderResourceView(colorTexture, nullptr, &textureView);
        if (colorTexture)
            colorTexture->Release();

        D3D11_TEXTURE2D_DESC depthDescription = colorDescription;
        depthDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        depthDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        if (SUCCEEDED(result))
            result = context->device->CreateTexture2D(&depthDescription, nullptr, &target->depthTexture);
        if (SUCCEEDED(result))
            result = context->device->CreateDepthStencilView(target->depthTexture, nullptr, &target->depthView);
        if (FAILED(result))
        {
            if (textureView)
                textureView->Release();
            target->release();
            delete texture;
            ERROR_AT(__LINE__, __FILE__, "failed to create DirectX11 render target (HRESULT=0x%08lx)", result);
            return nullptr;
        }
        texture->setBackendTexturePointer(textureView);
        texture->width                      = static_cast<uint32_t>(width);
        texture->height                     = static_cast<uint32_t>(height);
        texture->setAlphaChannelEnabled(enableAlpha);
        texture->fileName                   = std::move(fileNameBase);
        cacheTexture(texture->fileName, texture);
        return texture;
    }
}

#endif // USE_DIRECTX11
