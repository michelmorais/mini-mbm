/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

// iOS Metal backend — provides CORE_MANAGER methods that depend on UIKit/UIView.
// The shared Metal rendering pipeline (shaders, textures, meshes, etc.) lives
// in the platform-agnostic core-manager-metal.mm / *-metal.mm family of files.
// This file only implements the window/surface management layer that differs from
// macOS.

#if defined(USE_METAL)
#if !defined(ANDROID)
#if defined(MBM_PLATFORM_IOS)

#include <core-manager.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <device.h>
#include "specific-metal-context.h"
#include "private/skeletal-render-capability.h"
#include <audio-interface.h>
#include <util-interface.h>

// ---------------------------------------------------------------------------
// mbm_ios_setMetalLayer / mbm_ios_getMetalLayer
//
// MetalViewController calls mbm_ios_setMetalLayer() with the MBMMetalView's
// CAMetalLayer BEFORE calling initializeSceneLua(). initGraphics() below reads
// it back via mbm_ios_getMetalLayer().  This decouples the UIKit lifecycle from
// the engine init sequence without exposing UIKit types in C++ headers.
// ---------------------------------------------------------------------------
static CAMetalLayer* s_pendingMetalLayer = nil;

extern "C" void mbm_ios_setMetalLayer(CAMetalLayer* _Nullable layer)
{
    s_pendingMetalLayer = layer;
}

namespace mbm
{
    // -------------------------------------------------------------------------
    // initGraphics — called by LUA_MANAGER::initializeSceneLua()
    //
    // On iOS there is no window to create: the view hierarchy is controlled by
    // UIKit.  MetalViewController sets the CAMetalLayer via mbm_ios_setMetalLayer
    // before this function runs.
    // -------------------------------------------------------------------------
    bool CORE_MANAGER::initGraphics(const char* nameApplication,
                                     int width, int height,
                                     const int  /*px*/, const int  /*py*/,
                                     const bool /*border*/, const bool /*enable_resize*/)
    {
        this->setNameApplication(nameApplication);

        DEVICE *device = this->getDevice();
        device->initializeSpecificContext();
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = device->getSpecificContextDevice();

        // -- Attach the pre-registered CAMetalLayer ---------------------------
        ctx->metalLayer = s_pendingMetalLayer;
        if (!ctx->metalLayer)
        {
            ERROR_LOG("iOS initGraphics: CAMetalLayer was not registered. "
                      "Call mbm_ios_setMetalLayer() before initializeSceneLua().");
            return false;
        }

        // -- Metal device + command queue -------------------------------------
        ctx->mtlDevice = ctx->metalLayer.device;
        if (!ctx->mtlDevice)
        {
            ctx->mtlDevice = MTLCreateSystemDefaultDevice();
            if (!ctx->mtlDevice)
            {
                ERROR_LOG("iOS Metal: MTLCreateSystemDefaultDevice() returned nil.");
                return false;
            }
            ctx->metalLayer.device = ctx->mtlDevice;
        }
        ctx->commandQueue = [ctx->mtlDevice newCommandQueue];
        if (!ctx->commandQueue)
        {
            ERROR_LOG("iOS Metal: failed to create MTLCommandQueue.");
            return false;
        }
        const uint64_t paletteVectors = std::min<uint64_t>(
            static_cast<uint64_t>(ctx->mtlDevice.maxBufferLength / (4u * sizeof(float))),
            static_cast<uint64_t>(UINT32_MAX));
        skeletal::setMeasuredSkinningCapability(static_cast<uint32_t>(paletteVectors), 31u);

        // -- Backbuffer dimensions -------------------------------------------
        // MetalViewController sets the backbuffer size from the view's logical
        // point size before calling initializeSceneLua, so we trust those values.
        float backBufferWidth = device->getBackBufferWidth();
        float backBufferHeight = device->getBackBufferHeight();
        if (backBufferWidth <= 0)
            backBufferWidth = static_cast<float>(width > 0 ? width : 375);
        if (backBufferHeight <= 0)
            backBufferHeight = static_cast<float>(height > 0 ? height : 667);
        device->setBackBufferSize(backBufferWidth, backBufferHeight);

        // -- CAMetalLayer pixel format + drawable size -----------------------
        const CGFloat scale = [[UIScreen mainScreen] scale];
        ctx->metalLayer.pixelFormat      = MTLPixelFormatBGRA8Unorm;
        ctx->metalLayer.framebufferOnly  = YES;
        ctx->metalLayer.drawableSize     = CGSizeMake(
            ctx->metalLayer.bounds.size.width  * scale,
            ctx->metalLayer.bounds.size.height * scale);

        // -- Mark device as running ------------------------------------------
        device->setRun(true);

        INFO_LOG("iOS Metal device: %s", [ctx->mtlDevice.name UTF8String]);
        INFO_LOG("Backbuffer: %.1f x %.1f | Retina scale: %.2f | Drawable: %.0f x %.0f",
                 backBufferWidth,
                 backBufferHeight,
                 scale,
                 ctx->metalLayer.drawableSize.width,
                 ctx->metalLayer.drawableSize.height);

        mbm::TEXTURE_MANAGER::getInstance()->setTextureCapabilities(16384, 16384, 16384);

        return true;
    }

    // -------------------------------------------------------------------------
    // handleEventFromWindow — called every frame inside onLoop().
    //
    // On iOS all input events are delivered asynchronously via UIResponder
    // callbacks (touchesBegan/Moved/Ended/Cancelled) which call onTouchDown,
    // onTouchMove, onTouchUp.  No polling is required here.
    // -------------------------------------------------------------------------
    void CORE_MANAGER::handleEventFromWindow()
    {
        // No-op on iOS: all events come through UIKit callbacks.
    }

    void CORE_MANAGER::getScreenSize(int* width, int* height)
    {
        CGRect bounds = [UIScreen mainScreen].bounds;
        *width  = static_cast<int>(bounds.size.width);
        *height = static_cast<int>(bounds.size.height);
    }

    void CORE_MANAGER::moveWindow(int /*x*/, int /*y*/)
    {
        // No-op on iOS — window layout is managed by UIKit.
    }

    void CORE_MANAGER::ReleaseGraphics(bool wasDeviceLost)
    {
        mbm::TEXTURE_MANAGER::getInstance()->release();
        mbm::MESH_MANAGER::getInstance()->release();
        DEVICE *device = this->getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = device->getSpecificContextDevice();
        ctx->release(wasDeviceLost);
    }

} // namespace mbm

#endif // MBM_PLATFORM_IOS
#endif // !ANDROID
#endif // USE_METAL
