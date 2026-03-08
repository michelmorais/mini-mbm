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
#include <specific-metal.h>
#include <util-interface.h>
#include <cr-static-local.h>
#include <plugin-callback.h>
#include <texture-manager.h>
#include <mesh-manager.h>

namespace mbm
{

    CORE_MANAGER::CORE_MANAGER()
    {
        this->device                    = DEVICE::getInstance();
        this->indexOnRestore            = 0;
        this->totalForByLoop            = 0;
        this->percentRestoreInfo        = 0.0f;
        this->stepRestoreInfo           = 0.1f;
        this->stepRestore               = STEP_RES_INIT_GL;
        this->which_for                 = WFOR_INITIAL;
        this->changeScene               = true;
        this->__sceneWasInit            = false;
        this->keyCapsLockState          = false;
        this->wasGamePausedBeforeOnStop  = false;
    }

    CORE_MANAGER::~CORE_MANAGER()
    {
        DEVICE::quit();
    }

    void CORE_MANAGER::swapBuffers()
    {
        @autoreleasepool
        {
            SPECIFIC_AUX_CONTEXT_DEVICE* ctx = this->device->specificContextDevice;
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
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = this->device->specificContextDevice;
        if (!ctx || !ctx->metalLayer) return true;

        ctx->metalLayer.drawableSize = CGSizeMake(newWidth, newHeight);
        this->device->backBufferWidth  = static_cast<float>(newWidth);
        this->device->backBufferHeight = static_cast<float>(newHeight);
        return true;
    }

    bool CORE_MANAGER::beginRender()
    {
        @autoreleasepool
        {
            SPECIFIC_AUX_CONTEXT_DEVICE* ctx = this->device->specificContextDevice;
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
            desc.colorAttachments[0].clearColor  = MTLClearColorMake(
                static_cast<double>(device->colorClearBackGround.r),
                static_cast<double>(device->colorClearBackGround.g),
                static_cast<double>(device->colorClearBackGround.b),
                static_cast<double>(device->colorClearBackGround.a));
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
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = this->device->specificContextDevice;
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
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = this->device->specificContextDevice;
        if (!ctx || !ctx->mtlDevice || !ctx->commandQueue) return true;

        bool oneRender = false;
        for (auto renderTarget : this->device->lsObjectRenderToTarget)
        {
            if (!renderTarget->isObjectOnFrustum)
                continue;
            RENDER2TARGET_METAL* rf =
                static_cast<RENDER2TARGET_METAL*>(renderTarget->specificConfig);
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
            this->device->camera.updateCam(
                true,
                static_cast<float>(device->backBufferWidth),
                static_cast<float>(device->backBufferHeight));
        }
        return true;
    }

    unsigned int CORE_MANAGER::addPlugin(PLUGIN* plugin)
    {
        for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
        {
            if (this->lsPlugins[i] == plugin)
                return i;
        }
        if (plugin)
        {
            this->lsPlugins.push_back(plugin);
            // Provide the NSWindow as the platform handle (cast to void*).
            void* handle = (__bridge void*)this->device->specificContextDevice->window;
            plugin->onSubscribe(
                static_cast<int>(this->device->backBufferWidth),
                static_cast<int>(this->device->backBufferHeight),
                handle, nullptr);
            return static_cast<unsigned int>(this->lsPlugins.size() - 1);
        }
        return 0xffffffff;
    }

    void CORE_MANAGER::setMinMaxSizeWindow(int32_t min_x, int32_t min_y, int32_t max_x, int32_t max_y)
    {
        NSWindow* win = this->device->specificContextDevice->window;
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
