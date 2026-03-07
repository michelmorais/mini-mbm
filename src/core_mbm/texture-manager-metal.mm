/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.                                    |
|-----------------------------------------------------------------------------------------------------------------------*/

// Metal texture-manager stubs.
// TEXTURE::loadFromData() and TEXTURE::release() are called from the common
// texture-manager.cpp which is always compiled. For Milestone 1 (empty scene,
// no textures loaded) these functions are never called at runtime, but the
// linker requires their symbols to exist.

#if defined(USE_METAL)

#include <specific-metal.h>
#include <texture-manager.h>
#include <util-interface.h>
#include <image-resource.h>
#include <render-2-texture.h>

namespace mbm
{
    void TEXTURE::release()
    {
        idTexture       = 0;
        width           = 0;
        height          = 0;
        useAlphaChannel = false;
    }

    bool TEXTURE::loadFromData(const uint8_t* data,
                               const uint32_t w, const uint32_t h,
                               const uint16_t /*depth*/,
                               const uint16_t channel,
                               const bool     hasAlpha)
    {
        // TODO: create MTLTexture from pixel data using MTLDevice::newTextureWithDescriptor:.
        (void)data; (void)w; (void)h; (void)channel; (void)hasAlpha;
        WARN_LOG("Metal: TEXTURE::loadFromData() is not yet implemented.");
        return false;
    }

    bool TEXTURE::loadFromResourceData(const IMAGE_RESOURCE* image)
    {
        if (image == nullptr)
            return false;
        // TODO: upload embedded resource image to MTLTexture.
        WARN_LOG("Metal: TEXTURE::loadFromResourceData() is not yet implemented.");
        this->width           = image->width;
        this->height          = image->height;
        this->useAlphaChannel = true;
        return false;
    }

    TEXTURE* TEXTURE_MANAGER::loadNativeEngine(const char* fileName, const bool forceAlpha)
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
        lsTextures[fileNameBase] = tex;
        // TODO: load the image file and create an MTLTexture.
        WARN_LOG("Metal: TEXTURE_MANAGER::loadNativeEngine() is not yet implemented.");
        return tex;
    }

    TEXTURE* TEXTURE_MANAGER::createTextureRenderTarget(RENDERIZABLE_TO_TARGET* renderToTarget,
                                                        const char* nickName,
                                                        const bool  enableAlpha)
    {
        if (nickName == nullptr || renderToTarget == nullptr)
            return nullptr;
        std::string fileNameBase = util::getBaseName(nickName);
        if (fileNameBase.empty())
            return nullptr;

        const auto width  = static_cast<int>(renderToTarget->widthTexture);
        const auto height = static_cast<int>(renderToTarget->heightTexture);

        if (width > this->maxTextureSize || height > this->maxTextureSize)
        {
            PRINT_IF_DEBUG("max size to generate texture is %d/%d.",
                           width > height ? width : height, this->maxTextureSize);
            return nullptr;
        }
        TEXTURE* texture = lsTextures[fileNameBase];
        if (texture)
            return texture;
        texture = new TEXTURE();
        // TODO: create MTLTexture as render target and attach to renderToTarget.
        texture->width            = static_cast<uint32_t>(width);
        texture->height           = static_cast<uint32_t>(height);
        texture->useAlphaChannel  = enableAlpha;
        texture->fileName         = std::move(fileNameBase);
        lsTextures[texture->fileName] = texture;
        return texture;
    }

} // namespace mbm

#endif // USE_METAL
