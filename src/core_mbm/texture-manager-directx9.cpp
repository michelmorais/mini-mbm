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


#if defined (USE_DUMMY_BACK_END_ENGINE)

#include "dummy-engine.h" // for compiler_message, you can remove it after implement the functions

#include <texture-manager.h>
#include <renderizable.h>
#include <uber-image.h>
#include <image-resource.h>
#include <util-interface.h>

namespace mbm
{
    void TEXTURE::release()
    {
        if (idTexture)
        {
            compiler_message("TODO: implement delete texture");
        }
        idTexture       = 0;
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
        const uint8_t *img = uberImg.getImage8bitsPerPixel(data, w, h, depth, channel);
        if (!img)
        {
            PRINT_IF_DEBUG("failed to load texture ");
            return false;
        }
        this->width  = w;
        this->height = h;
        // Upload texture to GPU
        if (idTexture == 0)
        {
            if (uberImg.getImage() == nullptr)
                delete[] img;
            return false;
        }
        compiler_message("TODO: implement generate texture");
        uint8_t *rgba_toDelete = nullptr;
        if (channel == 4)
        {
            compiler_message("TODO: implement upload texture with RGBA data");
        }
        else if (hasAlpha)
        {
            auto     rgba      = new uint8_t[width * height * 4];
            const uint32_t sizeImage = width * height * 3;
            rgba_toDelete                = rgba;
            for (uint32_t i = 0, j = 0; i < sizeImage; i += 3, j += 4)
            {
                const uint8_t r = img[i];
                const uint8_t g = img[i + 1];
                const uint8_t b = img[i + 2];
                rgba[j]               = r;
                rgba[j + 1]           = g;
                rgba[j + 2]           = b;
                rgba[j + 3]           = 255; // 255 - opcao totalmente opaco
            }
            compiler_message("TODO: implement upload texture with RGBA data");
        }
        else
        {
            compiler_message("TODO: implement upload texture with RGB data");
        }
        if (TEXTURE::no_filter)
        { // TILE MAP Mode
            compiler_message("TODO: implement set texture parameters for tile map mode");

        }
        else
        {
            compiler_message("TODO: implement set texture parameters for normal mode");
        }
        if (rgba_toDelete)
            delete[] rgba_toDelete;
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
        compiler_message("TODO: implement pixel store alignment");  
        
        if (idTexture == 0)
            return false;
        //TODO: implement bind texture
        if (TEXTURE::no_filter)
        { // TILE MAP Mode
            compiler_message("TODO: implement set texture parameters for tile map mode");

        }
        else
        {
            compiler_message("TODO: implement set texture parameters for normal mode");
        }
        return true;
    }

    TEXTURE_SHARED::TEXTURE_SHARED()
    {
        this->maxTextureSize = 0;
        compiler_message("TODO: implement get max texture size");
    }

        std::shared_ptr<TEXTURE> TEXTURE_SHARED::createTextureRenderTarget(RENDERIZABLE_TO_TARGET *renderToTarget, const char *nickName,
                                              const bool enableAlpha)
    {
        const char *       fileName = nickName;
        const uint32_t width    = renderToTarget->widthTexture;
        const uint32_t height   = renderToTarget->heightTexture;
        if (fileName == nullptr || fileName[0] == 0)
            return nullptr;
        if (static_cast<int>(width) > this->maxTextureSize || static_cast<int>(height) > this->maxTextureSize)
        {
            PRINT_IF_DEBUG("max size to generate texture is  %d/%d.", width > height ? width : height,this->maxTextureSize);
            return nullptr;
        }
        std::shared_ptr<TEXTURE> pTexture = loadFromCache(fileName);
        if (pTexture->isLoaded())
            return pTexture;
        
        uint32_t idFrameBuffer  = 0;
        uint32_t idTexture2d    = 0;
        uint32_t idRenderBuffer = 0;
        compiler_message("TODO: implement generate framebuffer, renderbuffer and texture");

        // texture
        

        if (TEXTURE::no_filter)
        { // TILE MAP Mode
            compiler_message("TODO: implement set texture parameters for tile map mode");

        }
        else
        {
            compiler_message("TODO: implement set texture parameters for normal mode");
        }

        if (enableAlpha)
        {
            compiler_message("TODO: implement upload texture with RGBA format");
        }
        else
        {
            compiler_message("TODO: implement upload texture with RGB format");
        }
        // depth buffer
        compiler_message("TODO: implement bind renderbuffer and set storage");
        // frame buffer
        
        // attachments
        compiler_message("TODO: implement attach texture and renderbuffer to framebuffer");
        //

        renderToTarget->idFrameBuffer       = idFrameBuffer;
        renderToTarget->idDepthRenderbuffer = idRenderBuffer;
        renderToTarget->idTextureDynamic    = static_cast<int>(idTexture2d);
        pTexture->idTexture                  = idTexture2d;
        pTexture->width                      = width;
        pTexture->height                     = height;
        pTexture->useAlphaChannel            = enableAlpha;
        pTexture->fileName                   = nickName;
        return pTexture;
    }

        TEXTURE * TEXTURE_MANAGER::createTextureRenderTarget(RENDERIZABLE_TO_TARGET *renderToTarget, const char *nickName,
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
        compiler_message("TODO: implement generate framebuffer, renderbuffer and");

        // texture
        compiler_message("TODO: implement generate texture");

        if (TEXTURE::no_filter)
        { // TILE MAP Mode
            compiler_message("TODO: implement set texture parameters for tile map mode");

        }
        else
        {
            compiler_message("TODO: implement set texture parameters for normal mode");
        }

        if (enableAlpha)
        {
            compiler_message("TODO: implement upload texture with RGBA format");
        }
        else
        {
            compiler_message("TODO: implement upload texture with RGB format");
        }
        // depth buffer
        compiler_message("TODO: implement bind renderbuffer and set storage");
        // frame buffer
        compiler_message("TODO: implement bind framebuffer");
        // attachments
        compiler_message("TODO: implement attach texture and renderbuffer to framebuffer");
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

    TEXTURE_MANAGER::TEXTURE_MANAGER()
    {
        this->maxTextureSize = 0;
        compiler_message("TODO: implement get max texture size");
    }
}

#endif // USE_DUMMY_BACK_END_ENGINE