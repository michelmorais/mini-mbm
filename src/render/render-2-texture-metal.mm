/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.                                    |
|-----------------------------------------------------------------------------------------------------------------------*/

// Metal backend stubs for render-to-texture.
// Milestone 7: createTextureRenderTarget + renderToTargets are now implemented.
// saveAsPNG reads back pixels via a MTLBlitCommandEncoder staging blit.

#if defined(USE_METAL)

#include <specific-metal.h>
#include "../core_mbm/specific-metal-render-target.h"
#include <scene.h>
#include <render-2-texture.h>
#include <texture-manager.h>
#include <util-interface.h>
#include <device.h>
#include <lodepng/lodepng.h>

namespace mbm
{

    RENDERIZABLE_TO_TARGET::RENDERIZABLE_TO_TARGET(const SCENE* scene,
                                                   const TYPE_CLASS newTypeClass,
                                                   const bool _is3d,
                                                   const bool _is2ds) noexcept :
        RENDERIZABLE(scene->getIdScene(), newTypeClass, _is3d, _is2ds)
    {
        setRenderTargetSpecificConfig(new RENDER2TARGET_METAL());
        this->colorClearBackGround = COLOR(255, 255, 255);
        this->colorClearBackGround.a = 1.0f;
        this->widthTexture  = 0;
        this->heightTexture = 0;
    }

    RENDERIZABLE_TO_TARGET::~RENDERIZABLE_TO_TARGET()
    {
        void *renderTargetSpecificConfig = getRenderTargetSpecificConfig();
        delete static_cast<RENDER2TARGET_METAL*>(renderTargetSpecificConfig);
        setRenderTargetSpecificConfig(nullptr);
    }

    FVF_PROVIDE_BY_ENGINE RENDERIZABLE_TO_TARGET::getFvfFromBuffer() const noexcept
    {
        return FVF_PROVIDE_BY_ENGINE::FVF_NONE;
    }

    bool RENDER_2_TEXTURE::saveAsPNG(const char* newFileOutNamePNG,
                                     const int x, const int y,
                                     const int _width, const int _height)
    {
        if (newFileOutNamePNG == nullptr)
            return log_util::fail(__LINE__, __FILE__, "file name to save png is null");
        if (this->texture == nullptr)
            return log_util::fail(__LINE__, __FILE__, "render-to-texture: texture is not created!");

        void *renderTargetSpecificConfig = getRenderTargetSpecificConfig();
        const RENDER2TARGET_METAL* rf =
            static_cast<const RENDER2TARGET_METAL*>(renderTargetSpecificConfig);
        if (!rf || !rf->renderTexture)
            return log_util::fail(__LINE__, __FILE__, "Metal render texture is not created!");

        if (strcasecmp(newFileOutNamePNG, this->fileName.c_str()) == 0)
            return log_util::fail(__LINE__, __FILE__,
                                  "file name texture in is the same as render2texture [%s]!",
                                  fileName.c_str());
        if (x < 0 || _width <= 0 || (_width + x) > static_cast<int>(this->widthTexture))
            return log_util::fail(__LINE__, __FILE__,
                                  "size expected [0-0 %dx%d] got [%d-%d %dx%d]",
                                  widthTexture, heightTexture, x, y, _width, _height);
        if (y < 0 || _height <= 0 || (_height + y) > static_cast<int>(this->heightTexture))
            return log_util::fail(__LINE__, __FILE__,
                                  "size expected [0-0 %dx%d] got [%d-%d %dx%d]",
                                  widthTexture, heightTexture, x, y, _width, _height);

        mbm::DEVICE* dev = mbm::DEVICE::getInstance();
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = dev ? dev->getSpecificContextDevice() : nullptr;
        if (!ctx)
            return log_util::fail(__LINE__, __FILE__, "Metal device not available");
        id<MTLDevice>       mtlDevice = ctx->mtlDevice;
        id<MTLCommandQueue> cmdQueue  = ctx->commandQueue;
        if (!mtlDevice || !cmdQueue)
            return log_util::fail(__LINE__, __FILE__, "Metal device/queue not ready");

        // --- GPU → CPU blit -----------------------------------------------------
        // BGRa8Unorm: 4 bytes per pixel regardless of enableAlpha.
        const NSUInteger bytesPerRow =
            static_cast<NSUInteger>(_width) * 4u;
        const NSUInteger bufSize = bytesPerRow * static_cast<NSUInteger>(_height);

        id<MTLBuffer> stagingBuf =
            [mtlDevice newBufferWithLength:bufSize options:MTLResourceStorageModeShared];
        if (!stagingBuf)
            return log_util::fail(__LINE__, __FILE__,
                                  "Failed to allocate Metal staging buffer (%u bytes)",
                                  static_cast<unsigned>(bufSize));

        @autoreleasepool
        {
            id<MTLCommandBuffer>      cmdBuf = [cmdQueue commandBuffer];
            id<MTLBlitCommandEncoder> blit   = [cmdBuf blitCommandEncoder];
            [blit copyFromTexture:rf->renderTexture
                      sourceSlice:0
                      sourceLevel:0
                     sourceOrigin:MTLOriginMake(static_cast<NSUInteger>(x),
                                                static_cast<NSUInteger>(y), 0)
                       sourceSize:MTLSizeMake(static_cast<NSUInteger>(_width),
                                              static_cast<NSUInteger>(_height), 1)
                         toBuffer:stagingBuf
                destinationOffset:0
           destinationBytesPerRow:bytesPerRow
         destinationBytesPerImage:bufSize];
            [blit endEncoding];
            [cmdBuf commit];
            [cmdBuf waitUntilCompleted]; // synchronous — needed before CPU read
        }

        // --- Convert BGRA → RGBA (or BGR → RGB) and feed to lodepng -------------
        const int channel     = this->texture->useAlphaChannel ? 4 : 3;
        const int sizeImage   = _width * _height * channel;
        auto* image           = new unsigned char[sizeImage];
        const auto* src       = static_cast<const uint8_t*>([stagingBuf contents]);
        for (int row = 0; row < _height; ++row)
        {
            for (int col = 0; col < _width; ++col)
            {
                const int srcOff = (row * _width + col) * 4;
                const int dstOff = (row * _width + col) * channel;
                image[dstOff + 0] = src[srcOff + 2]; // R  (BGRA → src[2] is R)
                image[dstOff + 1] = src[srcOff + 1]; // G
                image[dstOff + 2] = src[srcOff + 0]; // B  (BGRA → src[0] is B)
                if (channel == 4)
                    image[dstOff + 3] = src[srcOff + 3]; // A
            }
        }
        // Metal texture origin is top-left — no vertical flip needed (unlike OpenGL).

        std::vector<unsigned char> png;
        unsigned int errorPNG = lodepng::encode(
            png, image,
            static_cast<unsigned int>(_width),
            static_cast<unsigned int>(_height),
            channel == 4 ? LCT_RGBA : LCT_RGB);
        delete[] image;
        if (errorPNG)
            return log_util::fail(__LINE__, __FILE__,
                                  "PNG encoding error [%s]", lodepng_error_text(errorPNG));

        errorPNG = lodepng::save_file(png, newFileOutNamePNG);
        if (errorPNG)
            return log_util::fail(__LINE__, __FILE__,
                                  "PNG save error [%s]", lodepng_error_text(errorPNG));
        return true;
    }

} // namespace mbm

#endif // USE_METAL
