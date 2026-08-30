/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------*/
#if defined(__APPLE__)

#include "macos-window-tests.h"
#include <core_mbm/device.h>
#include <specific-metal-context.h>
#include <cmath>

bool requestMacOSWindowResize(const int width, const int height)
{
    mbm::DEVICE *device = mbm::DEVICE::getInstance();
    mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context = device ? device->getSpecificContextDevice() : nullptr;
    if (!context || !context->window)
        return false;
    [context->window setContentSize:NSMakeSize(width, height)];
    return true;
}

bool validateMacOSWindowResize(const int width, const int height)
{
    mbm::DEVICE *device = mbm::DEVICE::getInstance();
    mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context = device ? device->getSpecificContextDevice() : nullptr;
    if (!context || !context->window || !context->metalLayer)
        return false;
    const NSRect bounds = context->window.contentView.bounds;
    const CGFloat scale = context->window.backingScaleFactor;
    const CGSize drawableSize = context->metalLayer.drawableSize;
    return static_cast<int>(bounds.size.width) == width &&
           static_cast<int>(bounds.size.height) == height &&
           std::fabs(context->metalLayer.contentsScale - scale) < 0.001 &&
           static_cast<int>(drawableSize.width) == static_cast<int>(width * scale) &&
           static_cast<int>(drawableSize.height) == static_cast<int>(height * scale);
}

#endif
