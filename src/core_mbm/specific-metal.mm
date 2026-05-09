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

#include <specific-metal.h>

namespace mbm
{
    // -------------------------------------------------------------------------
    // RENDER2TARGET_METAL
    // -------------------------------------------------------------------------
    void RENDER2TARGET_METAL::release() noexcept
    {
        renderTexture  = nil;
        depthTexture   = nil;
        passDescriptor = nil;
        width          = 0;
        height         = 0;
    }

    RENDER2TARGET_METAL::~RENDER2TARGET_METAL()
    {
        release();
    }

    // -------------------------------------------------------------------------
    // BUFFER_SPECIFIC
    // constructor is = default (header), destructor and release() defined here.
    // -------------------------------------------------------------------------
    void BUFFER_SPECIFIC::release()
    {
        vertexBuffer = nil;
        indexBuffer  = nil;
        vertexCount  = 0;
        indexCount   = 0;
    }

    BUFFER_SPECIFIC::~BUFFER_SPECIFIC()
    {
        release();
    }

    // -------------------------------------------------------------------------
    // SPECIFIC_AUX_CONTEXT_DEVICE
    // -------------------------------------------------------------------------
    SPECIFIC_AUX_CONTEXT_DEVICE::SPECIFIC_AUX_CONTEXT_DEVICE() noexcept
    {
    }

    void SPECIFIC_AUX_CONTEXT_DEVICE::release(const bool /*wasDeviceLost*/) noexcept
    {
        currentEncoder       = nil;
        currentCommandBuffer = nil;
        currentDrawable      = nil;
        currentPassDescriptor = nil;
        commandQueue         = nil;
        metalLayer           = nil;
        defaultSampler       = nil;
        nearestSampler       = nil;
        useNearestSampler    = false;
        defaultDepthStencilState = nil;
        noDepthStencilState  = nil;
        depthTexture         = nil;
#if TARGET_OS_IOS
        metalView = nil;
#else
        // Nil the delegate BEFORE close so no AppKit callbacks can reach it
        // after it is released.  Matches the pattern used for the launcher
        // dialog.  This also prevents EXC_BAD_ACCESS in fullscreen/borderless
        // mode where AppKit walks window lists during presentation-option reset.
        if (window)
        {
            // Nil the delegate first so no callbacks fire on a released object.
            // Then close (hidden, removed from window lists) and explicitly
            // release — setReleasedWhenClosed:NO was set at creation so the
            // window does NOT self-release inside [close].
            [window setDelegate:nil];
            [window close];
            [window release];   // non-ARC explicit release (paired with alloc)
            window = nil;
        }
        windowDelegate = nil;
#endif
        mtlDevice      = nil;
    }

    SPECIFIC_AUX_CONTEXT_DEVICE::~SPECIFIC_AUX_CONTEXT_DEVICE() noexcept
    {
        release(false);
    }

} // namespace mbm

#endif // USE_METAL
