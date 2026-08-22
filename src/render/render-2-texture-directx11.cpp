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

#include "specific-directx11-render-target.h"

#include <scene.h>
#include <render-2-texture.h>
#include <lodepng/lodepng.h>
#include <texture-manager.h>
#include <util-interface.h>
#include "specific-directx11-context.h"
#include <device.h>

namespace mbm
{

    RENDERIZABLE_TO_TARGET::RENDERIZABLE_TO_TARGET(const SCENE* scene, const TYPE_CLASS newTypeClass, const bool _is3d, const bool _is2ds) noexcept :
        RENDERIZABLE(scene->getIdScene(), newTypeClass, _is3d, _is2ds)
    {
        this->setRenderTargetSpecificConfig(new RENDER2TARGET_DIRECTX11());
        this->setRenderTargetClearColor(COLOR(255, 255, 255)); // alpha em 0 significa transparente
        this->setRenderTargetSize(0, 0);
    }

    RENDERIZABLE_TO_TARGET::~RENDERIZABLE_TO_TARGET()
    {
        delete static_cast<RENDER2TARGET_DIRECTX11 *>(this->getRenderTargetSpecificConfig());
        this->setRenderTargetSpecificConfig(nullptr);
    }

    FVF_PROVIDE_BY_ENGINE RENDERIZABLE_TO_TARGET::getFvfFromBuffer() const noexcept
    {
        return FVF_PROVIDE_BY_ENGINE::FVF_NONE;
    }

    RENDER2TARGET_DIRECTX11::~RENDER2TARGET_DIRECTX11()
    {
        release();
    }

    void RENDER2TARGET_DIRECTX11::release() noexcept
    {
        if (depthView)
            depthView->Release();
        if (depthTexture)
            depthTexture->Release();
        if (renderTargetView)
            renderTargetView->Release();
        depthView = nullptr;
        depthTexture = nullptr;
        renderTargetView = nullptr;
    }

    bool RENDER_2_TEXTURE::saveAsPNG(const char* newFileOutNamePNG, const int x, const int y, const int _width, const int _height)
    {
        if(newFileOutNamePNG == nullptr)
            return log_util::fail(__LINE__,__FILE__,"file name to save png is null");
        if(!this->isLoaded())
            return log_util::fail(__LINE__,__FILE__,"render to texture is not loaded!");
        const TEXTURE *renderTargetTexture = this->getRenderTargetTexture();
        if(renderTargetTexture == nullptr)
            return log_util::fail(__LINE__,__FILE__,"texture is not created!");
        if(strcasecmp(newFileOutNamePNG,this->getInternalFileName()) == 0)
            return log_util::fail(__LINE__,__FILE__,"file name texture in is the same as render2texture [%s]!",this->getInternalFileName());
        const uint32_t renderTargetWidth = this->getRenderTargetWidth();
        const uint32_t renderTargetHeight = this->getRenderTargetHeight();
        if(x < 0 || _width <= 0 || (_width + x) > static_cast<int>(renderTargetWidth))
            return log_util::fail(__LINE__,__FILE__,"size expected [0-0 %dx%d] got [%d-%d %dx%d]",renderTargetWidth,renderTargetHeight,x,y,_width,_height);
        if(y < 0 || _height <= 0 || (_height + y) > static_cast<int>(renderTargetHeight))
            return log_util::fail(__LINE__,__FILE__,"size expected [0-0 %dx%d] got [%d-%d %dx%d]",renderTargetWidth,renderTargetHeight,x,y,_width,_height);

        const int channel = renderTargetTexture->hasAlphaChannel() ? 4 : 3;
        const int sizeImage = _width * _height * channel;
        std::vector<uint8_t> imageData(sizeImage);
        RENDER2TARGET_DIRECTX11 *target = static_cast<RENDER2TARGET_DIRECTX11 *>(getRenderTargetSpecificConfig());
        if (!target || !target->renderTargetView)
            return log_util::fail(__LINE__, __FILE__, "DirectX11 render target view is null");
        ID3D11Resource *renderTargetResource = nullptr;
        target->renderTargetView->GetResource(&renderTargetResource);
        ID3D11Texture2D *renderTargetTexture2d = nullptr;
        HRESULT result = renderTargetResource ?
            renderTargetResource->QueryInterface(__uuidof(ID3D11Texture2D),
                reinterpret_cast<void **>(&renderTargetTexture2d)) : E_FAIL;
        if (renderTargetResource)
            renderTargetResource->Release();
        if (FAILED(result) || !renderTargetTexture2d)
            return log_util::fail(__LINE__, __FILE__, "failed to access DirectX11 render target texture");

        D3D11_TEXTURE2D_DESC description = {};
        renderTargetTexture2d->GetDesc(&description);
        description.Usage = D3D11_USAGE_STAGING;
        description.BindFlags = 0;
        description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        description.MiscFlags = 0;
        SPECIFIC_AUX_CONTEXT_DEVICE *context = DEVICE::getInstance()->getSpecificContextDevice();
        ID3D11Texture2D *stagingTexture = nullptr;
        result = context->device->CreateTexture2D(&description, nullptr, &stagingTexture);
        if (SUCCEEDED(result))
        {
            context->immediateContext->CopyResource(stagingTexture, renderTargetTexture2d);
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            result = context->immediateContext->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
            if (SUCCEEDED(result))
            {
                const uint8_t *source = static_cast<const uint8_t *>(mapped.pData);
                for (int row = 0; row < _height; ++row)
                {
                    const uint8_t *sourceRow = source + static_cast<size_t>(y + row) * mapped.RowPitch +
                        static_cast<size_t>(x) * 4u;
                    uint8_t *destinationRow = imageData.data() + static_cast<size_t>(row) * _width * channel;
                    for (int column = 0; column < _width; ++column)
                    {
                        memcpy(destinationRow + static_cast<size_t>(column) * channel,
                               sourceRow + static_cast<size_t>(column) * 4u, channel);
                    }
                }
                context->immediateContext->Unmap(stagingTexture, 0);
            }
        }
        if (stagingTexture)
            stagingTexture->Release();
        renderTargetTexture2d->Release();
        if (FAILED(result))
            return log_util::fail(__LINE__, __FILE__, "DirectX11 render-target readback failed (HRESULT=0x%08lx)", result);

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
#endif // USE_DIRECTX11
