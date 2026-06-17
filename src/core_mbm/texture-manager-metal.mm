/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.                                    |
|-----------------------------------------------------------------------------------------------------------------------*/

#if defined(USE_METAL)

#include "specific-metal-context.h"
#include "specific-metal-render-target.h"
#include <texture-manager.h>
#include <util-interface.h>
#include <image-resource.h>
#include <render-2-texture.h>
#include <uber-image.h>
#include <device.h>

namespace mbm
{
    // Helper: current MTLDevice from the global DEVICE singleton.
    static id<MTLDevice> getMetalDevice()
    {
        DEVICE* dev = DEVICE::getInstance();
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = dev ? dev->getSpecificContextDevice() : nullptr;
        return ctx ? ctx->mtlDevice : nil;
    }
    void TEXTURE::release()
    {
        void *texturePointer = getBackendTexturePointer();
        if (texturePointer)
        {
            CFBridgingRelease(texturePointer);
            setBackendTexturePointer(nullptr);
        }
        width           = 0;
        height          = 0;
        useAlphaChannel = false;
    }

    bool TEXTURE::loadFromData(const uint8_t* data,
                               const uint32_t w, const uint32_t h,
                               const uint16_t depth,
                               const uint16_t channel,
                               const bool     hasAlpha)
    {
        if (!data || !w || !h) return false;
        id<MTLDevice> device = getMetalDevice();
        if (!device) return false;

        // Normalise to 8-bpp (handles 16-bpp HDR inputs, etc.).
        UBER_IMG uberImg;
        const uint8_t* img = uberImg.getImage8bitsPerPixel(data, w, h, depth, channel);
        if (!img) return false;

        // Metal requires RGBA8Unorm; expand RGB/Grey to RGBA.
        const uint8_t* rgbaData = nullptr;
        uint8_t*       tempBuf  = nullptr;
        if (channel == 4)
        {
            rgbaData = img;
        }
        else
        {
            tempBuf = new uint8_t[w * h * 4];
            for (uint32_t i = 0; i < w * h; ++i)
            {
                if (channel == 3)
                {
                    tempBuf[i*4+0] = img[i*3+0];
                    tempBuf[i*4+1] = img[i*3+1];
                    tempBuf[i*4+2] = img[i*3+2];
                    tempBuf[i*4+3] = 255;
                }
                else // channel == 1 (greyscale)
                {
                    const uint8_t v = img[i];
                    tempBuf[i*4+0] = tempBuf[i*4+1] = tempBuf[i*4+2] = v;
                    tempBuf[i*4+3] = 255;
                }
            }
            rgbaData = tempBuf;
        }

        MTLTextureDescriptor* desc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                              width:w
                                                             height:h
                                                          mipmapped:NO];
        desc.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> tex = [device newTextureWithDescriptor:desc];
        if (tex)
        {
            [tex replaceRegion:MTLRegionMake2D(0, 0, w, h)
                   mipmapLevel:0
                     withBytes:rgbaData
                   bytesPerRow:w * 4];
            void *texturePointer = getBackendTexturePointer();
            if (texturePointer) CFBridgingRelease(texturePointer);
            setBackendTexturePointer((__bridge_retained void*)tex);
        }
        delete[] tempBuf;
        if (!tex) return false;
        this->width           = w;
        this->height          = h;
        this->useAlphaChannel = (channel == 4 || hasAlpha);
        return true;
    }

    bool TEXTURE::loadFromResourceData(const IMAGE_RESOURCE* image)
    {
        if (!image) return false;
        id<MTLDevice> device = getMetalDevice();
        if (!device) return false;

        MTLTextureDescriptor* desc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                              width:image->width
                                                             height:image->height
                                                          mipmapped:NO];
        desc.usage = MTLTextureUsageShaderRead;
        id<MTLTexture> tex = [device newTextureWithDescriptor:desc];
        if (!tex) return false;
        [tex replaceRegion:MTLRegionMake2D(0, 0, image->width, image->height)
               mipmapLevel:0
                 withBytes:image->data
               bytesPerRow:image->width * 4];
        void *texturePointer = getBackendTexturePointer();
        if (texturePointer) CFBridgingRelease(texturePointer);
        setBackendTexturePointer((__bridge_retained void*)tex);
        this->width           = image->width;
        this->height          = image->height;
        this->useAlphaChannel = true;
        return true;
    }

    TEXTURE* TEXTURE_MANAGER::loadNativeEngine(const char* fileName, const bool forceAlpha)
    {
        // Return nullptr so the common loader uses lodepng and calls loadFromData().
        // (Matches the OpenGL ES behaviour on Linux/macOS.)
        (void)fileName; (void)forceAlpha;
        return nullptr;
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

        const auto width  = static_cast<int>(renderToTarget->getRenderTargetWidth());
        const auto height = static_cast<int>(renderToTarget->getRenderTargetHeight());

        const uint32_t maxTextureSize = getMaxTextureSize();
        if (static_cast<uint32_t>(width) > maxTextureSize || static_cast<uint32_t>(height) > maxTextureSize)
        {
            PRINT_IF_DEBUG("max size to generate texture is %d/%d.",
                           width > height ? width : height, maxTextureSize);
            return nullptr;
        }
        TEXTURE* texture = getCachedTexture(fileNameBase);
        if (texture)
            return texture;

        id<MTLDevice> mtlDev = getMetalDevice();
        if (!mtlDev) return nullptr;

        TEXTURE* newTexture = nullptr;
        @autoreleasepool
        {
            const NSUInteger tw = static_cast<NSUInteger>(width);
            const NSUInteger th = static_cast<NSUInteger>(height);

            // Color texture — same pixel format as the main CAMetalLayer so that the
            // existing (already compiled) MTLRenderPipelineState is valid for both the
            // main pass and any render-to-texture pass.
            MTLTextureDescriptor* cd =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                   width:tw
                                                                  height:th
                                                               mipmapped:NO];
            cd.usage       = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
            cd.storageMode = MTLStorageModePrivate;
            id<MTLTexture> colorTex = [mtlDev newTextureWithDescriptor:cd];
            if (!colorTex) return nullptr;

            // Depth texture — same format as the main depth buffer.
            MTLTextureDescriptor* dd =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                   width:tw
                                                                  height:th
                                                               mipmapped:NO];
            dd.usage       = MTLTextureUsageRenderTarget;
            dd.storageMode = MTLStorageModePrivate;
            id<MTLTexture> depthTex = [mtlDev newTextureWithDescriptor:dd];

            // Build the render pass descriptor used each time this target is rendered.
            MTLRenderPassDescriptor* passDesc = [MTLRenderPassDescriptor renderPassDescriptor];
            passDesc.colorAttachments[0].texture     = colorTex;
            passDesc.colorAttachments[0].loadAction  = MTLLoadActionClear;
            passDesc.colorAttachments[0].storeAction = MTLStoreActionStore;
            passDesc.colorAttachments[0].clearColor  = MTLClearColorMake(1.0, 1.0, 1.0, 1.0);
            if (depthTex)
            {
                passDesc.depthAttachment.texture     = depthTex;
                passDesc.depthAttachment.loadAction  = MTLLoadActionClear;
                passDesc.depthAttachment.clearDepth  = 1.0;
                passDesc.depthAttachment.storeAction = MTLStoreActionDontCare;
            }

            // Store GPU objects in the render-target's backend config.
            void *renderTargetSpecificConfig = renderToTarget->getRenderTargetSpecificConfig();
            RENDER2TARGET_METAL* rf = static_cast<RENDER2TARGET_METAL*>(renderTargetSpecificConfig);
            rf->renderTexture  = colorTex;
            rf->depthTexture   = depthTex;
            rf->passDescriptor = passDesc;
            rf->width          = static_cast<uint32_t>(tw);
            rf->height         = static_cast<uint32_t>(th);

            // Build the TEXTURE record used for sampling in the main pass.
            newTexture = new TEXTURE();
            newTexture->setBackendTexturePointer((__bridge_retained void*)colorTex);
            newTexture->width           = static_cast<uint32_t>(tw);
            newTexture->height          = static_cast<uint32_t>(th);
            newTexture->useAlphaChannel = enableAlpha;
            newTexture->fileName        = std::move(fileNameBase);
            cacheTexture(newTexture->fileName, newTexture);
        }
        return newTexture;
    }

} // namespace mbm

#endif // USE_METAL
