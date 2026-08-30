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

namespace
{
    NSEvent *makeMouseEvent(const NSEventType type, NSWindow *window,
                            const NSPoint location, const NSInteger clickCount)
    {
        return [NSEvent mouseEventWithType:type location:location modifierFlags:0
            timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:window.windowNumber
            context:nil eventNumber:0 clickCount:clickCount pressure:1.0];
    }

    void postModifierEvent(NSWindow *window, const NSEventModifierFlags flags,
                           const unsigned short keyCode)
    {
        NSEvent *event = [NSEvent keyEventWithType:NSEventTypeFlagsChanged
            location:NSZeroPoint modifierFlags:flags
            timestamp:NSProcessInfo.processInfo.systemUptime windowNumber:window.windowNumber
            context:nil characters:@"" charactersIgnoringModifiers:@""
            isARepeat:NO keyCode:keyCode];
        [NSApp postEvent:event atStart:NO];
    }
}

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

bool postMacOSInputTestEvents()
{
    mbm::DEVICE *device = mbm::DEVICE::getInstance();
    mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context = device ? device->getSpecificContextDevice() : nullptr;
    NSWindow *window = context ? context->window : nil;
    if (!window)
        return false;
    const NSTimeInterval timestamp = NSProcessInfo.processInfo.systemUptime;
    NSEvent *keyDown = [NSEvent keyEventWithType:NSEventTypeKeyDown location:NSZeroPoint
        modifierFlags:0 timestamp:timestamp windowNumber:window.windowNumber context:nil
        characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:0];
    NSEvent *keyUp = [NSEvent keyEventWithType:NSEventTypeKeyUp location:NSZeroPoint
        modifierFlags:0 timestamp:timestamp windowNumber:window.windowNumber context:nil
        characters:@"a" charactersIgnoringModifiers:@"a" isARepeat:NO keyCode:0];
    [NSApp postEvent:keyDown atStart:NO];
    [NSApp postEvent:keyUp atStart:NO];

    const struct { NSEventModifierFlags flag; unsigned short keyCode; } modifiers[] = {
        {NSEventModifierFlagShift, 56}, {NSEventModifierFlagControl, 59},
        {NSEventModifierFlagOption, 58}, {NSEventModifierFlagCommand, 55}};
    for (const auto &modifier : modifiers)
    {
        postModifierEvent(window, modifier.flag, modifier.keyCode);
        postModifierEvent(window, 0, modifier.keyCode);
    }
    postModifierEvent(window, NSEventModifierFlagCapsLock, 57);

    const NSPoint location = NSMakePoint(120.0, window.contentView.bounds.size.height - 80.0);
    const NSEventType downTypes[] = {NSEventTypeLeftMouseDown, NSEventTypeRightMouseDown,
                                     NSEventTypeOtherMouseDown};
    const NSEventType upTypes[] = {NSEventTypeLeftMouseUp, NSEventTypeRightMouseUp,
                                   NSEventTypeOtherMouseUp};
    for (size_t index = 0; index < 3; ++index)
    {
        NSEvent *downEvent = makeMouseEvent(downTypes[index], window, location, 1);
        NSEvent *upEvent = makeMouseEvent(upTypes[index], window, location, 1);
        [NSApp postEvent:downEvent atStart:NO];
        [NSApp postEvent:upEvent atStart:NO];
    }
    [NSApp postEvent:makeMouseEvent(NSEventTypeLeftMouseDown, window, location, 2) atStart:NO];
    [NSApp postEvent:makeMouseEvent(NSEventTypeLeftMouseUp, window, location, 2) atStart:NO];
    [NSApp postEvent:makeMouseEvent(NSEventTypeMouseMoved, window, location, 0) atStart:NO];

    CGEventRef scrollUpEvent = CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitLine, 1, 1);
    CGEventRef scrollDownEvent = CGEventCreateScrollWheelEvent(nullptr, kCGScrollEventUnitLine, 1, -1);
    NSEvent *scrollUp = scrollUpEvent ? [NSEvent eventWithCGEvent:scrollUpEvent] : nil;
    NSEvent *scrollDown = scrollDownEvent ? [NSEvent eventWithCGEvent:scrollDownEvent] : nil;
    if (scrollUp)
        [NSApp postEvent:scrollUp atStart:NO];
    if (scrollDown)
        [NSApp postEvent:scrollDown atStart:NO];
    if (scrollUpEvent)
        CFRelease(scrollUpEvent);
    if (scrollDownEvent)
        CFRelease(scrollDownEvent);
    if (!scrollUp || !scrollDown)
        return false;
    return true;
}

bool postMacOSCapsLockRelease()
{
    mbm::DEVICE *device = mbm::DEVICE::getInstance();
    mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context = device ? device->getSpecificContextDevice() : nullptr;
    NSWindow *window = context ? context->window : nil;
    if (!window)
        return false;
    postModifierEvent(window, 0, 57);
    return true;
}

bool requestMacOSWindowClose()
{
    mbm::DEVICE *device = mbm::DEVICE::getInstance();
    mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context = device ? device->getSpecificContextDevice() : nullptr;
    if (!context || !context->window)
        return false;
    [context->window performClose:nil];
    return true;
}

#endif
