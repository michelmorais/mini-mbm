/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                      |
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
#ifndef METAL_SPECIFIC_CONTEXT_H
#define METAL_SPECIFIC_CONTEXT_H

#include <specific-metal.h>

#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include <TargetConditionals.h>
#if TARGET_OS_IOS
    #import <UIKit/UIKit.h>
#else
    #import <Cocoa/Cocoa.h>
#endif

namespace mbm
{
    struct SPECIFIC_AUX_CONTEXT_DEVICE
    {
        SPECIFIC_AUX_CONTEXT_DEVICE() noexcept;
        SPECIFIC_AUX_CONTEXT_DEVICE(const SPECIFIC_AUX_CONTEXT_DEVICE&) = delete;
        SPECIFIC_AUX_CONTEXT_DEVICE& operator=(const SPECIFIC_AUX_CONTEXT_DEVICE&) = delete;
        ~SPECIFIC_AUX_CONTEXT_DEVICE() noexcept;
        void release(const bool wasDeviceLost) noexcept;

        id<MTLDevice>       mtlDevice    = nil;
        id<MTLCommandQueue> commandQueue = nil;
        CAMetalLayer       *metalLayer   = nil;
#if TARGET_OS_IOS
        UIView *metalView = nil;
#else
        NSWindow *window         = nil;
        id        windowDelegate = nil;
#endif

        id<CAMetalDrawable>         currentDrawable       = nil;
        id<MTLCommandBuffer>        currentCommandBuffer  = nil;
        id<MTLRenderCommandEncoder> currentEncoder        = nil;
        MTLRenderPassDescriptor    *currentPassDescriptor = nil;

        int filter_GL_TEXTURE_WRAP_S     = 0;
        int filter_GL_TEXTURE_WRAP_T     = 0;
        int filter_GL_TEXTURE_MIN_FILTER = 0;
        int filter_GL_TEXTURE_MAG_FILTER = 0;

        bool pendingClearDepth = true;

        id<MTLSamplerState> defaultSampler = nil;
        id<MTLSamplerState> nearestSampler = nil;
        bool useNearestSampler             = false;

        bool depthTestEnabled = true;
        int  currentBlendState = 2;

        id<MTLDepthStencilState> defaultDepthStencilState = nil;
        id<MTLDepthStencilState> noDepthStencilState      = nil;
        id<MTLTexture>           depthTexture             = nil;
    };
}

#endif // METAL_SPECIFIC_CONTEXT_H
#endif // USE_METAL
