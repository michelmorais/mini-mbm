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
#import <Cocoa/Cocoa.h>

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
        NSWindow*            window         = nil;
        id                   windowDelegate = nil; // MBMWindowDelegate (opaque to avoid circular header dep)

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
    };

} // namespace mbm

#endif // METAL_SPECIFIC_H
#endif // USE_METAL
