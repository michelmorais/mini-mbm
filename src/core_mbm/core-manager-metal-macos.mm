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
#if !defined(ANDROID)
#if defined(__APPLE__)

#include <core-manager.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <device.h>
#include <specific-metal.h>
#include <audio-interface.h>
#include <util-interface.h>

// ---------------------------------------------------------------------------
// MBMWindowDelegate — forwards macOS window events into the engine.
// ---------------------------------------------------------------------------
@interface MBMWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) bool* runFlag;
@end

@implementation MBMWindowDelegate

- (BOOL)windowShouldClose:(NSWindow*)__unused sender
{
    if (_runFlag)
        *_runFlag = false;
    return YES;
}

- (void)windowDidResize:(NSNotification*)notification
{
    // The engine's handleEventFromWindow loop picks up NSEventTypeApplicationDefined
    // on the next tick. We post a synthetic resize notification via the CORE_MANAGER
    // event queue; but since we don't have the pointer here we rely on poll in
    // handleEventFromWindow() examining the window's current content-view size.
    (void)notification;
}

@end

// ---------------------------------------------------------------------------
// Key-code translation helper (macOS virtual key codes → engine key codes)
// Engine uses uppercase ASCII for letter keys and raw unichar for others.
// ---------------------------------------------------------------------------
static int translateMacKeyCode(NSEvent* event)
{
    // Try to get the printable character first.
    NSString* chars = [event charactersIgnoringModifiers];
    if (chars && chars.length > 0)
    {
        unichar c = [chars characterAtIndex:0];
        if (c >= 'a' && c <= 'z')
            return static_cast<int>(toupper(c));
        if (c >= 'A' && c <= 'Z')
            return static_cast<int>(c);
        if (c >= '0' && c <= '9')
            return static_cast<int>(c);

        // Map common special keys to XK-compatible values used by the engine.
        // NSF* key codes are in the Unicode private-use range 0xF700–0xF7FF.
        switch (c)
        {
            case NSUpArrowFunctionKey:    return 0xFF52; // XK_Up
            case NSDownArrowFunctionKey:  return 0xFF54; // XK_Down
            case NSLeftArrowFunctionKey:  return 0xFF51; // XK_Left
            case NSRightArrowFunctionKey: return 0xFF53; // XK_Right
            case NSF1FunctionKey:         return 0xFFBE; // XK_F1
            case NSF2FunctionKey:         return 0xFFBF;
            case NSF3FunctionKey:         return 0xFFC0;
            case NSF4FunctionKey:         return 0xFFC1;
            case NSF5FunctionKey:         return 0xFFC2;
            case NSF6FunctionKey:         return 0xFFC3;
            case NSF7FunctionKey:         return 0xFFC4;
            case NSF8FunctionKey:         return 0xFFC5;
            case NSF9FunctionKey:         return 0xFFC6;
            case NSF10FunctionKey:        return 0xFFC7;
            case NSF11FunctionKey:        return 0xFFC8;
            case NSF12FunctionKey:        return 0xFFC9;
            case NSDeleteCharacter:       return 0xFF08; // XK_BackSpace
            case NSDeleteFunctionKey:     return 0xFFFF; // XK_Delete
            case '\r':                    return 0xFF0D; // XK_Return (carriage return)
            case NSEnterCharacter:        return 0xFF0D; // XK_Return (numpad Enter)
            case NSTabCharacter:          return 0xFF09; // XK_Tab
            case 0x1B:                    return 0xFF1B; // XK_Escape
            case ' ':                     return 0x0020; // XK_space
            default:
                return static_cast<int>(c);
        }
    }
    return 0;
}

namespace mbm
{
    bool CORE_MANAGER::initGraphics(const char* nameApplication,
                                     int width, int height,
                                     const int px, const int py,
                                     const bool border, const bool enable_resize)
    {
        this->nameAplication       = nameApplication ? nameApplication : "Mini-mbm";
        this->windowBorder         = border;
        this->enableResizeWindow   = enable_resize;

        // Initialise NSApplication (safe to call multiple times).
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        // Initialise device context.
        this->device->initializeSpecificContext();
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = this->device->specificContextDevice;

        // ---- Metal device + command queue ----
        ctx->mtlDevice = MTLCreateSystemDefaultDevice();
        if (!ctx->mtlDevice)
        {
            ERROR_LOG("Metal: MTLCreateSystemDefaultDevice() returned nil. "
                      "Ensure you are running on macOS 10.11+ with Metal support.");
            return false;
        }
        ctx->commandQueue = [ctx->mtlDevice newCommandQueue];
        if (!ctx->commandQueue)
        {
            ERROR_LOG("Metal: failed to create MTLCommandQueue.");
            return false;
        }

        // ---- NSWindow ----
        if (width  <= 0) width  = 800;
        if (height <= 0) height = 600;

        NSUInteger styleMask = NSWindowStyleMaskTitled    |
                               NSWindowStyleMaskClosable  |
                               NSWindowStyleMaskMiniaturizable;
        if (enable_resize)
            styleMask |= NSWindowStyleMaskResizable;
        if (!border)
            styleMask = NSWindowStyleMaskBorderless;

        // macOS coordinate system: y=0 is the bottom of the screen.
        NSRect screenFrame  = [[NSScreen mainScreen] frame];
        CGFloat macY = screenFrame.size.height - py - height;
        NSRect contentRect  = NSMakeRect(px, macY, width, height);

        ctx->window = [[NSWindow alloc] initWithContentRect:contentRect
                                                  styleMask:styleMask
                                                    backing:NSBackingStoreBuffered
                                                      defer:NO];
        if (!ctx->window)
        {
            ERROR_LOG("Metal: failed to create NSWindow.");
            return false;
        }
        [ctx->window setTitle:[NSString stringWithUTF8String:nameAplication.c_str()]];

        // Borderless (fullscreen) window: raise above the menu bar and
        // auto-hide the menu bar + dock so they don't overlap the content.
        if (!border)
        {
            [ctx->window setLevel:NSMainMenuWindowLevel + 1];
            [NSApp setPresentationOptions:
                NSApplicationPresentationAutoHideMenuBar |
                NSApplicationPresentationAutoHideDock];
        }

        // Window delegate — handles close / resize notifications.
        MBMWindowDelegate* delegate = [[MBMWindowDelegate alloc] init];
        delegate.runFlag = &this->device->run;
        [ctx->window setDelegate:delegate];
        ctx->windowDelegate = delegate;

        // ---- CAMetalLayer ----
        ctx->metalLayer              = [CAMetalLayer layer];
        ctx->metalLayer.device       = ctx->mtlDevice;
        ctx->metalLayer.pixelFormat  = MTLPixelFormatBGRA8Unorm;
        ctx->metalLayer.framebufferOnly = YES;

        NSView* contentView = [ctx->window contentView];
        [contentView setLayer:ctx->metalLayer];
        [contentView setWantsLayer:YES];
        ctx->metalLayer.frame = contentView.bounds;

        CGFloat scale = [ctx->window backingScaleFactor];
        ctx->metalLayer.contentsScale = scale;
        // Preliminary drawable size based on requested dimensions — Metal needs a
        // valid size before the window appears.  Will be corrected below after the
        // OS has had a chance to constrain the window to the screen.
        ctx->metalLayer.drawableSize = CGSizeMake(width * scale, height * scale);

        this->device->windowPositionX  = px;
        this->device->windowPositionY  = py;

        // ---- Show window ----
        [ctx->window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        // Pump the run-loop so the window is actually mapped.  After this point
        // macOS has constrained the window to the available screen area, so
        // contentView.bounds reflects the real logical dimensions.
        [NSApp nextEventMatchingMask:NSEventMaskAny
                           untilDate:[NSDate dateWithTimeIntervalSinceNow:0.05]
                              inMode:NSDefaultRunLoopMode
                             dequeue:NO];

        // Read the actual content-view size NOW (after OS constraining).
        // On a screen that is smaller than the requested window size, macOS will
        // cap the window; we must use those capped dimensions as the backbuffer
        // size so that expectedScreen == backBufferSize and scaleScreen2d == 1.0.
        NSRect actualBounds = [ctx->window contentView].bounds;
        this->device->backBufferWidth  = static_cast<float>(actualBounds.size.width);
        this->device->backBufferHeight = static_cast<float>(actualBounds.size.height);
        ctx->metalLayer.drawableSize   = CGSizeMake(actualBounds.size.width  * scale,
                                                     actualBounds.size.height * scale);

        // Mark device as running.
        this->device->run = true;

        // Log so any OS-imposed size difference is immediately visible.
        INFO_LOG("Metal device: %s", [ctx->mtlDevice.name UTF8String]);
        INFO_LOG("Requested logical size: %dx%d | Actual contentView: %.1f x %.1f | Retina scale: %.2f | Drawable: %.0f x %.0f",
                 width, height,
                 actualBounds.size.width, actualBounds.size.height,
                 scale,
                 ctx->metalLayer.drawableSize.width, ctx->metalLayer.drawableSize.height);

        // All Metal-capable Macs support 16384 × 16384 px textures (macOS GPU Family 1+).
        mbm::TEXTURE_MANAGER* texture_manager = mbm::TEXTURE_MANAGER::getInstance();
        texture_manager->setTextureCapabilities(16384, 16384, 16384);

        return true;
    }

    // -------------------------------------------------------------------------
    // handleEventFromWindow — called every frame before update/render.
    // Drains the NSApplication event queue and translates events into the
    // engine's EVENTS interface.
    // -------------------------------------------------------------------------
    void CORE_MANAGER::handleEventFromWindow()
    {
        // Tracks the previous modifier-key state so we can fire synthetic
        // key-down / key-up events when individual modifier bits change.
        static NSEventModifierFlags previousModifierFlags = 0;

        @autoreleasepool
        {
            SPECIFIC_AUX_CONTEXT_DEVICE* ctx = this->device->specificContextDevice;
            if (!ctx || !ctx->window) return;

            // backBufferWidth/Height are in logical points (not scaled).
            // NSEvent coordinates arrive in logical points — no scale needed.

            // Converts a logical-point NSPoint (origin bottom-left) to engine coords
            // (origin top-left, physical pixels).
            // NSEvent coordinates are already in logical points (same space as
            // backBufferWidth/Height).  No scale multiplication needed.
            auto toEngineXY = [&](NSPoint p, float& ex, float& ey) {
                ex = static_cast<float>(p.x);
                ey = static_cast<float>(this->device->backBufferHeight) -
                     static_cast<float>(p.y);
            };

            NSEvent* event;
            while ((event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                               untilDate:[NSDate distantPast]
                                                  inMode:NSDefaultRunLoopMode
                                                 dequeue:YES]) != nil)
            {
                switch (event.type)
                {
                    // ---- Keyboard ----
                    case NSEventTypeKeyDown:
                    {
                        if (!event.isARepeat)
                        {
                            int key = translateMacKeyCode(event);
                            if (key != 0)
                                this->onKeyDown(key);
                        }
                    }
                    break;

                    case NSEventTypeKeyUp:
                    {
                        int key = translateMacKeyCode(event);
                        if (key != 0)
                            this->onKeyUp(key);
                    }
                    break;

                    // ---- Modifier keys (Shift, Ctrl, Option/Alt, Command, CapsLock) ----
                    // NSEventTypeFlagsChanged fires whenever any modifier key is pressed
                    // or released.  We diff against the previous state to produce individual
                    // synthetic key-down / key-up events with XK-compatible codes.
                    case NSEventTypeFlagsChanged:
                    {
                        NSEventModifierFlags cur =
                            event.modifierFlags & NSEventModifierFlagDeviceIndependentFlagsMask;
                        NSEventModifierFlags prev = previousModifierFlags;
                        previousModifierFlags = cur;

                        auto dispatchMod = [&](NSEventModifierFlags flag, int keyCode)
                        {
                            bool wasDown = (prev & flag) != 0;
                            bool isDown  = (cur  & flag) != 0;
                            if      (!wasDown && isDown)  this->onKeyDown(keyCode);
                            else if ( wasDown && !isDown) this->onKeyUp(keyCode);
                        };

                        dispatchMod(NSEventModifierFlagShift,    0xFFE1); // XK_Shift_L
                        dispatchMod(NSEventModifierFlagControl,  0xFFE3); // XK_Control_L
                        dispatchMod(NSEventModifierFlagOption,   0xFFE9); // XK_Alt_L   (Option)
                        dispatchMod(NSEventModifierFlagCommand,  0xFFEB); // XK_Super_L (Command ⌘)
                        dispatchMod(NSEventModifierFlagCapsLock, 0xFFE5); // XK_Caps_Lock
                    }
                    break;

                    // ---- Mouse buttons ----
                    case NSEventTypeLeftMouseDown:
                    {
                        float ex, ey;
                        toEngineXY([event locationInWindow], ex, ey);
                        this->onTouchDown(0, ex, ey);
                    }
                    break;

                    case NSEventTypeLeftMouseUp:
                    {
                        float ex, ey;
                        toEngineXY([event locationInWindow], ex, ey);
                        this->onTouchUp(0, ex, ey);
                    }
                    break;

                    case NSEventTypeRightMouseDown:
                    {
                        float ex, ey;
                        toEngineXY([event locationInWindow], ex, ey);
                        this->onTouchDown(1, ex, ey);
                    }
                    break;

                    case NSEventTypeRightMouseUp:
                    {
                        float ex, ey;
                        toEngineXY([event locationInWindow], ex, ey);
                        this->onTouchUp(1, ex, ey);
                    }
                    break;

                    case NSEventTypeOtherMouseDown:
                    {
                        float ex, ey;
                        toEngineXY([event locationInWindow], ex, ey);
                        this->onTouchDown(2, ex, ey);
                    }
                    break;

                    case NSEventTypeOtherMouseUp:
                    {
                        float ex, ey;
                        toEngineXY([event locationInWindow], ex, ey);
                        this->onTouchUp(2, ex, ey);
                    }
                    break;

                    // ---- Mouse motion ----
                    case NSEventTypeMouseMoved:
                    case NSEventTypeLeftMouseDragged:
                    case NSEventTypeRightMouseDragged:
                    case NSEventTypeOtherMouseDragged:
                    {
                        float ex, ey;
                        toEngineXY([event locationInWindow], ex, ey);
                        this->onTouchMove(0, ex, ey);
                    }
                    break;

                    // ---- Scroll wheel (zoom) ----
                    case NSEventTypeScrollWheel:
                    {
                        float delta = static_cast<float>(event.scrollingDeltaY);
                        if (delta != 0.0f)
                            this->onTouchZoom(delta > 0 ? 1.0f : -1.0f);
                    }
                    break;

                    // ---- Window resize ----
                    case NSEventTypeAppKitDefined:
                    {
                        // Window geometry may have changed; check actual content size.
                        // Use logical points (not physical pixels) to stay consistent
                        // with backBufferWidth/Height and the end-of-frame poll below.
                        if (ctx->window)
                        {
                            NSRect bounds = [ctx->window contentView].bounds;
                            int newW = static_cast<int>(bounds.size.width);
                            int newH = static_cast<int>(bounds.size.height);
                            if (newW > 0 && newH > 0 &&
                                (newW != static_cast<int>(device->backBufferWidth) ||
                                 newH != static_cast<int>(device->backBufferHeight)))
                            {
                                this->onResizeWindow(newW, newH);
                            }
                        }
                    }
                    break;

                    default:
                        break;
                }
                // Let NSApplication / NSWindow handle system-level events (menus, etc.).
                [NSApp sendEvent:event];
            }

            // If the window was closed by the delegate, stop the engine.
            if (ctx->window && ![ctx->window isVisible])
                this->device->run = false;

            // Poll for window resize every frame (catches programmatic resizes too).
            if (ctx->window && ctx->metalLayer)
            {
                NSRect bounds = [ctx->window contentView].bounds;
                // Logical point size — consistent with backBufferWidth/Height.
                int newW = static_cast<int>(bounds.size.width);
                int newH = static_cast<int>(bounds.size.height);
                if (newW > 0 && newH > 0 &&
                    (newW != static_cast<int>(device->backBufferWidth) ||
                     newH != static_cast<int>(device->backBufferHeight)))
                {
                    this->onResizeWindow(newW, newH);
                }
            }
        } // @autoreleasepool
    }

    void CORE_MANAGER::getScreenSize(int* width, int* height)
    {
        NSRect frame = [[NSScreen mainScreen] frame];
        *width  = static_cast<int>(frame.size.width);
        *height = static_cast<int>(frame.size.height);
    }

    void CORE_MANAGER::moveWindow(int x, int y)
    {
        NSWindow* win = this->device->specificContextDevice->window;
        if (!win) return;
        NSRect screenFrame = [[NSScreen mainScreen] frame];
        CGFloat macY = screenFrame.size.height - y - win.frame.size.height;
        [win setFrameOrigin:NSMakePoint(x, macY)];
    }

    void CORE_MANAGER::ReleaseGraphics(bool wasDeviceLost)
    {
        // Restore default presentation (menu bar + dock visible again).
        [NSApp setPresentationOptions:NSApplicationPresentationDefault];
        TEXTURE_MANAGER::getInstance()->release();
        MESH_MANAGER::getInstance()->release();
        this->device->specificContextDevice->release(wasDeviceLost);
    }

} // namespace mbm

#endif // __APPLE__
#endif // !ANDROID
#endif // USE_METAL
