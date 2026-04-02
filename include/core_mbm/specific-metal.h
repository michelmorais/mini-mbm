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
#if defined(USE_METAL)
#ifndef METAL_SPECIFIC_H
#define METAL_SPECIFIC_H

// This header is only included by Objective-C++ (.mm) files.
// Metal and Cocoa headers require the Objective-C runtime.
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <TargetConditionals.h>
#if TARGET_OS_IOS
    #import <UIKit/UIKit.h>
#else
    #import <Cocoa/Cocoa.h>
#endif

#include "core-exports.h"
#include <primitives.h>

namespace mbm
{
    // Per-texture GPU resource used by the Metal texture manager.
    // 'idTexture' in TEXTURE stores a raw pointer to this (cast to uint32_t is NOT used for Metal).
    // We keep a separate map in the Metal texture manager indexed by TEXTURE pointer.
    // This struct is referenced via specificConfig (void*) in RENDERIZABLE_TO_TARGET.
    struct RENDER2TARGET_METAL
    {
        id<MTLTexture>            renderTexture    = nil;
        id<MTLTexture>            depthTexture     = nil;
        MTLRenderPassDescriptor*  passDescriptor   = nil;
        uint32_t                  width            = 0;
        uint32_t                  height           = 0;

        RENDER2TARGET_METAL() noexcept = default;
        ~RENDER2TARGET_METAL();
        void release() noexcept;
    };

    // Per-mesh GPU vertex/index buffers.
    struct BUFFER_SPECIFIC
    {
        id<MTLBuffer> vertexBuffer = nil;
        id<MTLBuffer> indexBuffer  = nil;
        NSUInteger    vertexCount  = 0;
        NSUInteger    indexCount   = 0;

        BUFFER_SPECIFIC() noexcept = default;
        ~BUFFER_SPECIFIC();
        void release();
    };

    // Window + Metal device context — stored as DEVICE::specificContextDevice.
    struct SPECIFIC_AUX_CONTEXT_DEVICE
    {
        SPECIFIC_AUX_CONTEXT_DEVICE() noexcept;
        SPECIFIC_AUX_CONTEXT_DEVICE(const SPECIFIC_AUX_CONTEXT_DEVICE&) = delete;
        SPECIFIC_AUX_CONTEXT_DEVICE& operator=(const SPECIFIC_AUX_CONTEXT_DEVICE&) = delete;
        ~SPECIFIC_AUX_CONTEXT_DEVICE() noexcept;
        void release(const bool wasDeviceLost) noexcept;

        // Metal core objects
        id<MTLDevice>        mtlDevice      = nil;
        id<MTLCommandQueue>  commandQueue   = nil;
        CAMetalLayer*        metalLayer     = nil;
#if TARGET_OS_IOS
        UIView*              metalView      = nil; // MBMMetalView — set by MetalViewController before initGraphics()
#else
        NSWindow*            window         = nil;
        id                   windowDelegate = nil; // MBMWindowDelegate (opaque to avoid circular header dep)
#endif

        // Per-frame objects (valid between beginRender / swapBuffers)
        id<CAMetalDrawable>           currentDrawable      = nil;
        id<MTLCommandBuffer>          currentCommandBuffer = nil;
        id<MTLRenderCommandEncoder>   currentEncoder       = nil;
        MTLRenderPassDescriptor*      currentPassDescriptor = nil;

        // Stub filter-state fields kept for interface compatibility with DEVICE methods
        int filter_GL_TEXTURE_WRAP_S    = 0;
        int filter_GL_TEXTURE_WRAP_T    = 0;
        int filter_GL_TEXTURE_MIN_FILTER = 0;
        int filter_GL_TEXTURE_MAG_FILTER = 0;

        // Pending clear-depth flag (set by DEVICE::clearDepth / clearDepthColored,
        // consumed by CORE_MANAGER::beginRender).
        bool pendingClearDepth = true;

        // Default sampler (bilinear + clamp-to-edge) — used for all normal rendering.
        id<MTLSamplerState> defaultSampler = nil;

        // Nearest-neighbor sampler (point + repeat) — activated during tile-map rendering
        // by DEVICE::disableFilteringForPixelPerfect(); restored by enableFilteringAfterPixelPerfect().
        id<MTLSamplerState> nearestSampler  = nil;
        bool useNearestSampler              = false;

        // Tracks whether depth testing is currently enabled (toggled by DEVICE::setDepthTest).
        // SHADER::render() reads this to choose between defaultDepthStencilState and noDepthStencilState.
        bool depthTestEnabled               = true;

        // Tracks the destination blend factor requested via RENDER_STATE::set().
        // Stored as the raw BLEND_STATE enum integer to avoid including blend.h here.
        // 2 = BLEND_ONE (additive) — matches the OpenGL default: glBlendFunc(GL_SRC_ALPHA, GL_ONE).
        // SHADER::render() and renderDynamic() read it to select standardPSO vs additivePSO.
        int currentBlendState               = 2; // BLEND_ONE

        // Depth-stencil state: less comparison + depth write enabled (created on first use).
        id<MTLDepthStencilState> defaultDepthStencilState = nil;

        // Depth-stencil state: always pass + no depth write (for particles / 2ds objects).
        id<MTLDepthStencilState> noDepthStencilState = nil;

        // Persistent full-frame depth texture.  Created / resized in beginRender().
        id<MTLTexture> depthTexture = nil;
    };

} // namespace mbm

#endif // METAL_SPECIFIC_H
#endif // USE_METAL
