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
#include <device.h>

namespace mbm
{
    void TEXTURE::release()
    {
        REMINDER_TODO
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
            PRINT_IF_DEBUG("failed to get texture from DATA");
            return false;
        }
        this->width  = w;
        this->height = h;
        this->useAlphaChannel = hasAlpha ? true : false;
        REMINDER_TODO
        return true;
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

        REMINDER_TODO
        return true;
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
        
        tex->useAlphaChannel = forceAlpha;
        tex->fileName = fileName;
        lsTextures[fileNameBase] = (tex);
        REMINDER_TODO
        return tex;
    }

    TEXTURE * TEXTURE_MANAGER::createTextureRenderTarget(RENDERIZABLE_TO_TARGET *renderToTarget, 
                                                        const char *nickName,
                                                        const bool enableAlpha)
    {
        std::string fileNameBase    = util::getBaseName(nickName);
        const auto width            = static_cast<int>(renderToTarget->widthTexture);
        const auto height           = static_cast<int>(renderToTarget->heightTexture);
        
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

        REMINDER_TODO
        texture->width                      = static_cast<uint32_t>(width);
        texture->height                     = static_cast<uint32_t>(height);
        texture->useAlphaChannel            = enableAlpha;
        texture->fileName                   = std::move(fileNameBase);
        lsTextures[texture->fileName]       = texture;
        return texture;
    }
}

#endif // USE_DUMMY_BACK_END_ENGINE