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

#include <texture-manager.h>

#if defined(USE_OPENGL_ES)

#include <specific-opengl_es.h>
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
            GLDeleteTextures(1, &idTexture);
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
        GLPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        GLGenTextures(1, &idTexture);
        if (idTexture == 0)
        {
            return false;
        }
        GLBindTexture(GL_TEXTURE_2D, idTexture);
        uint8_t *rgba_toDelete = nullptr;
        if (channel == 4)
        {
            GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, img);
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
            GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        }
        else
        {
            GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGB, GL_UNSIGNED_BYTE, img);
        }
        if (TEXTURE::no_filter)
        { // TILE MAP Mode
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        
        }
        else
        {
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
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
        GLPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        GLGenTextures(1, &idTexture);
        if (idTexture == 0)
            return false;
        GLBindTexture(GL_TEXTURE_2D, idTexture);
        GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, image->data);
        if (TEXTURE::no_filter)
        { // TILE MAP Mode
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        
        }
        else
        {
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
        return true;
    }

    TEXTURE* TEXTURE_MANAGER::loadNativeEngine(const char* fileName, const bool forceAlpha) // No native implementation for OpenGl
    {
        return nullptr;
    }

    TEXTURE * TEXTURE_MANAGER::createTextureRenderTarget(RENDERIZABLE_TO_TARGET *renderToTarget, const char *nickName,
                                              const bool enableAlpha)
    {
        std::string fileNameBase    = util::getBaseName(nickName);
        const auto width         = static_cast<GLsizei>(renderToTarget->widthTexture);
        const auto height        = static_cast<GLsizei>(renderToTarget->heightTexture);
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
        GLGenFramebuffers(1, &idFrameBuffer);
        GLGenRenderbuffers(1, &idRenderBuffer);
        GLGenTextures(1, &idTexture2d);

        // texture
        GLBindTexture(GL_TEXTURE_2D, idTexture2d);

        if (TEXTURE::no_filter)
        { // TILE MAP Mode
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        
        }
        else
        {
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        if (enableAlpha)
        {
            GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        }
        else
        {
            GLTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
        }
        // depth buffer
        GLBindRenderbuffer(GL_RENDERBUFFER, idRenderBuffer);
        GLRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
        // frame buffer
        GLBindFramebuffer(GL_FRAMEBUFFER, idFrameBuffer);
        // attachments
        GLFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, idTexture2d, 0);
        GLFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, idRenderBuffer);
        //
        const GLenum status = GLCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            delete texture;
            return nullptr;
        }
        GLBindTexture(GL_TEXTURE_2D, 0);
        GLBindFramebuffer(GL_FRAMEBUFFER, 0);
        GLBindRenderbuffer(GL_RENDERBUFFER, 0);

        RENDER2TARGET_GLES* sf = static_cast<RENDER2TARGET_GLES*>(renderToTarget->specificConfig);

        sf->idFrameBuffer                   = idFrameBuffer;
        sf->idDepthRenderbuffer             = idRenderBuffer;
        sf->idTextureDynamic                = idTexture2d;
        texture->idTexture                  = idTexture2d;
        texture->width                      = static_cast<uint32_t>(width);
        texture->height                     = static_cast<uint32_t>(height);
        texture->useAlphaChannel            = enableAlpha;
        texture->fileName                   = std::move(fileNameBase);
        lsTextures[texture->fileName]       = texture;
        return texture;
    }
}

#endif // USE_OPENGL_ES