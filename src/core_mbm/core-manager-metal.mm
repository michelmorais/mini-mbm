/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

#if defined(USE_METAL)

#include <core-manager.h>
#include <device.h>
#include <renderizable.h>
#include "specific-metal-context.h"
#include "specific-metal-render-target.h"
#include <util-interface.h>
#include <cr-static-local.h>
#include <plugin-callback.h>
#include <texture-manager.h>
#include <mesh-manager.h>

namespace mbm
{

    CORE_MANAGER::CORE_MANAGER()
    {
        this->initializeImpl();
        this->setDevice(DEVICE::getInstance());
        this->setChangeScene(true);
        this->setSceneInitialized(false);
        this->setKeyCapsLockState(false);
    }

    void CORE_MANAGER::swapBuffers()
    {
        @autoreleasepool
        {
            DEVICE *device = this->getDevice();
            SPECIFIC_AUX_CONTEXT_DEVICE* ctx = device->getSpecificContextDevice();
            if (!ctx) return;
            if (ctx->currentCommandBuffer && ctx->currentDrawable)
            {
                [ctx->currentCommandBuffer presentDrawable:ctx->currentDrawable];
                [ctx->currentCommandBuffer commit];
                ctx->currentCommandBuffer = nil;
                ctx->currentDrawable      = nil;
            }
        }
    }

    bool CORE_MANAGER::resetDeviceWithNewDimensions(int newWidth, int newHeight)
    {
        DEVICE *device = this->getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = device->getSpecificContextDevice();
        if (!ctx || !ctx->metalLayer) return true;

        // newWidth/newHeight are in logical points; scale up to physical pixels
        // for the Metal drawable (so Retina displays render at full resolution).
        const CGFloat sc = ctx->metalLayer.contentsScale > 0.0 ? ctx->metalLayer.contentsScale : 1.0;
        ctx->metalLayer.drawableSize = CGSizeMake(newWidth * sc, newHeight * sc);
        device->setBackBufferSize(static_cast<float>(newWidth), static_cast<float>(newHeight));
        return true;
    }

    bool CORE_MANAGER::beginRender()
    {
        @autoreleasepool
        {
            DEVICE *device = this->getDevice();
            SPECIFIC_AUX_CONTEXT_DEVICE* ctx = device->getSpecificContextDevice();
            if (!ctx || !ctx->commandQueue || !ctx->metalLayer) return false;

            ctx->currentDrawable = [ctx->metalLayer nextDrawable];
            if (!ctx->currentDrawable)
            {
                INFO_LOG("Metal: nextDrawable returned nil — skipping frame.");
                return false;
            }

            MTLRenderPassDescriptor* desc = [MTLRenderPassDescriptor renderPassDescriptor];

            // Color attachment: clear to background colour
            desc.colorAttachments[0].texture     = ctx->currentDrawable.texture;
            desc.colorAttachments[0].loadAction  = MTLLoadActionClear;
            const COLOR &background = device->getColorClearBackGround();
            desc.colorAttachments[0].clearColor  = MTLClearColorMake(
                static_cast<double>(background.r),
                static_cast<double>(background.g),
                static_cast<double>(background.b),
                static_cast<double>(background.a));
            desc.colorAttachments[0].storeAction = MTLStoreActionStore;

            // Depth attachment — create or resize to match the current drawable.
            {
                const NSUInteger dw = ctx->currentDrawable.texture.width;
                const NSUInteger dh = ctx->currentDrawable.texture.height;
                if (!ctx->depthTexture ||
                    ctx->depthTexture.width  != dw ||
                    ctx->depthTexture.height != dh)
                {
                    MTLTextureDescriptor* dd =
                        [MTLTextureDescriptor
                            texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                         width:dw
                                                        height:dh
                                                     mipmapped:NO];
                    dd.usage       = MTLTextureUsageRenderTarget;
                    dd.storageMode = MTLStorageModePrivate;
                    ctx->depthTexture = [ctx->mtlDevice newTextureWithDescriptor:dd];
                }
                desc.depthAttachment.texture     = ctx->depthTexture;
                desc.depthAttachment.loadAction  = MTLLoadActionClear;
                desc.depthAttachment.clearDepth  = 1.0;
                desc.depthAttachment.storeAction = MTLStoreActionDontCare;
            }

            ctx->currentCommandBuffer = [ctx->commandQueue commandBuffer];
            ctx->currentCommandBuffer.label = @"MBM Frame";

            ctx->currentEncoder = [ctx->currentCommandBuffer renderCommandEncoderWithDescriptor:desc];
            ctx->currentPassDescriptor = desc;

            if (!ctx->currentEncoder)
            {
                ERROR_LOG("Metal: failed to create render command encoder.");
                ctx->currentCommandBuffer = nil;
                ctx->currentDrawable      = nil;
                return false;
            }
            ctx->currentEncoder.label = @"MBM Encoder";
            ctx->pendingClearDepth    = false;
        }
        return true;
    }

    void CORE_MANAGER::endRender()
    {
        DEVICE *device = this->getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = device->getSpecificContextDevice();
        if (!ctx) return;
        if (ctx->currentEncoder)
        {
            [ctx->currentEncoder endEncoding];
            ctx->currentEncoder       = nil;
            ctx->currentPassDescriptor = nil;
        }
    }

    bool CORE_MANAGER::renderToTargets()
    {
        DEVICE *device = this->getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = device->getSpecificContextDevice();
        if (!ctx || !ctx->mtlDevice || !ctx->commandQueue) return true;

        bool oneRender = false;
        const uint32_t totalRenderTargets = device->getTotalRenderTargets();
        for (uint32_t i = 0; i < totalRenderTargets; ++i)
        {
            auto renderTarget = device->getRenderTarget(i);
            if (!renderTarget)
                continue;
            if (!renderTarget->isObjectOnFrustum)
                continue;
            void *renderTargetSpecificConfig = renderTarget->getRenderTargetSpecificConfig();
            RENDER2TARGET_METAL* rf =
                static_cast<RENDER2TARGET_METAL*>(renderTargetSpecificConfig);
            if (!rf || !rf->renderTexture || !rf->passDescriptor)
                continue;

            // Update clear colour from the render target definition.
            const COLOR& bg = renderTarget->colorClearBackGround;
            rf->passDescriptor.colorAttachments[0].clearColor =
                MTLClearColorMake(
                    static_cast<double>(bg.r),
                    static_cast<double>(bg.g),
                    static_cast<double>(bg.b),
                    static_cast<double>(bg.a));

            id<MTLCommandBuffer> cmdBuf = [ctx->commandQueue commandBuffer];
            if (!cmdBuf) continue;
            cmdBuf.label = @"MBM RenderToTarget";

            id<MTLRenderCommandEncoder> encoder =
                [cmdBuf renderCommandEncoderWithDescriptor:rf->passDescriptor];
            if (!encoder) continue;
            encoder.label = @"MBM RTT Encoder";

            // Route all SHADER::render() calls to the off-screen encoder.
            ctx->currentEncoder = encoder;
            renderTarget->render2Texture();
            [encoder endEncoding];

            // Commit without stalling CPU.  Both this command buffer and the main
            // frame command buffer (created later in beginRender()) are enqueued on
            // the same MTLCommandQueue, so the GPU executes them in submission order.
            [cmdBuf commit];
            ctx->currentEncoder = nil;
            oneRender = true;
        }

        if (oneRender)
        {
            // Restore the camera to main backbuffer dimensions in case render2Texture()
            // updated it for the off-screen target.
            CAMERA &camera = device->getCamera();
            camera.updateCam(
                true,
                static_cast<float>(device->getBackBufferWidth()),
                static_cast<float>(device->getBackBufferHeight()));
        }
        return true;
    }

    unsigned int CORE_MANAGER::addPlugin(PLUGIN* plugin)
    {
        DEVICE *device = this->getDevice();
        for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
        {
            if (this->getPlugin(i) == plugin)
                return i;
        }
        if (plugin)
        {
            const unsigned int indexPlugin = this->appendPlugin(plugin);
            SPECIFIC_AUX_CONTEXT_DEVICE* ctx = device->getSpecificContextDevice();
            // Provide the window/view as the platform handle (cast to void*).
#if TARGET_OS_IOS
            void* handle = (__bridge void*)ctx->metalView;
#else
            void* handle = (__bridge void*)ctx->window;
#endif
            plugin->onSubscribe(
                static_cast<int>(device->getBackBufferWidth()),
                static_cast<int>(device->getBackBufferHeight()),
                handle,
                (__bridge void*)ctx->mtlDevice);
            return indexPlugin;
        }
        return 0xffffffff;
    }

    void CORE_MANAGER::setMinMaxSizeWindow(int32_t min_x, int32_t min_y, int32_t max_x, int32_t max_y)
    {
#if TARGET_OS_IOS
        (void)min_x; (void)min_y; (void)max_x; (void)max_y;
        // iOS windows are always full-screen; min/max size is not applicable.
#else
        DEVICE *device = this->getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = device->getSpecificContextDevice();
        NSWindow* win = ctx->window;
        if (!win) return;

        NSSize minSize = NSMakeSize(min_x > 0 ? min_x : 100, min_y > 0 ? min_y : 100);
        [win setMinSize:minSize];

        if (max_x > 0 && max_y > 0)
        {
            NSSize maxSize = NSMakeSize(max_x, max_y);
            [win setMaxSize:maxSize];
        }
        else
        {
            [win setMaxSize:NSMakeSize(FLT_MAX, FLT_MAX)];
        }
#endif
    }

} // namespace mbm


// ---------------------------------------------------------------------------
// log_util — Metal build (mirrors the implementations in core-manager-opengl_es.cpp)
// ---------------------------------------------------------------------------
namespace log_util
{
    OnScriptPrintLine onScriptPrintLine = nullptr;

    void setScriptPrintLine(OnScriptPrintLine onNewScriptPrintLine) noexcept
    {
        onScriptPrintLine = onNewScriptPrintLine;
    }

    void callScriptPrintLine() noexcept
    {
        if (onScriptPrintLine)
            onScriptPrintLine();
    }

    const char* getDescriptionError(const unsigned int /*error*/)
    {
        // No GL error codes in Metal.
        return "Metal backend: no GL error codes";
    }
}

#endif // USE_METAL
