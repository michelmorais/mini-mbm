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
#if defined(__APPLE__) && !TARGET_OS_IOS

#include <core-manager.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <device.h>
#include "specific-metal-context.h"
#include <audio-interface.h>
#include <util-interface.h>

// ---------------------------------------------------------------------------
// MBMBorderlessWindow — NSWindow subclass used in borderless/fullscreen mode.
// The default NSWindow returns NO from canBecomeKeyWindow for borderless
// windows, which prevents keyboard input and confuses AppKit during shutdown.
// ---------------------------------------------------------------------------
@interface MBMBorderlessWindow : NSWindow
@end
@implementation MBMBorderlessWindow
- (BOOL)canBecomeKeyWindow  { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
@end

// ---------------------------------------------------------------------------
// MBMQuitHandler — handles the "Quit" menu item action.
// Calls DEVICE::setRun(false) so the engine loop exits cleanly and main()
// returns 0, instead of calling exit() from inside AppKit (which bypasses
// C++ destructors and produces a non-zero exit code in Xcode).
// ---------------------------------------------------------------------------
@interface MBMQuitHandler : NSObject
@property (nonatomic, assign) mbm::DEVICE* device;
- (void)quit:(id)sender;
@end
@implementation MBMQuitHandler
- (void)quit:(id)sender
{
    (void)sender;
    if (_device)
        _device->setRun(false);
}
@end

// One instance lives for the lifetime of the process.
static MBMQuitHandler* s_quitHandler = nil;

// ---------------------------------------------------------------------------
// MBMWindowDelegate — forwards macOS window events into the engine.
// ---------------------------------------------------------------------------
@interface MBMWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) mbm::DEVICE* device;
@end

@implementation MBMWindowDelegate

- (BOOL)windowShouldClose:(NSWindow*)__unused sender
{
    if (_device)
        _device->setRun(false);
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
        this->setNameApplication(nameApplication);
        this->setWindowOptions(border, enable_resize);

        // Initialise NSApplication (safe to call multiple times).
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        // Always (re-)build the app menu so the app-name entry in the macOS
        // menu bar has a working submenu with Quit (⌘Q).  We cannot guard this
        // with `if (![NSApp mainMenu])` because macOS automatically creates a
        // skeletal menu bar when NSApplicationActivationPolicyRegular is set,
        // leaving the app-name item present but with an empty submenu.
        {
            NSString   *appName  = [NSString stringWithUTF8String:this->getNameApplication()];
            NSMenu     *menuBar  = [[NSMenu alloc] initWithTitle:@"MainMenu"];
            // The first item's title is shown as the app name in the menu bar.
            NSMenuItem *appItem  = [[NSMenuItem alloc] initWithTitle:appName action:nil keyEquivalent:@""];
            [menuBar addItem:appItem];

            // Route Quit through MBMQuitHandler so the engine loop exits
            // cleanly (DEVICE::setRun(false)) instead of calling exit() from
            // inside AppKit, which produces a non-zero exit code in Xcode.
            if (!s_quitHandler)
                s_quitHandler = [[MBMQuitHandler alloc] init];
            s_quitHandler.device = this->device;

            NSMenu     *appMenu   = [[NSMenu alloc] initWithTitle:appName];
            NSString   *quitTitle = [@"Quit " stringByAppendingString:appName];
            NSMenuItem *quitItem  = [[NSMenuItem alloc] initWithTitle:quitTitle
                                                               action:@selector(quit:)
                                                        keyEquivalent:@"q"];
            [quitItem setTarget:s_quitHandler];
            [appMenu addItem:quitItem];
            [appItem setSubmenu:appMenu];
            [NSApp setMainMenu:menuBar];
        }

        this->device->initializeSpecificContext();
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = this->device->getSpecificContextDevice();

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

        // For windowed mode, use the screen's visibleFrame (excludes the menu bar
        // at the top and the Dock at the bottom / side) so the window never clips
        // behind those system UI elements.
        // For borderless / fullscreen, use the full screen frame (menu bar and
        // Dock are auto-hidden via NSApplicationPresentationOptions anyway).
        NSRect contentRect;
        if (border)
        {
            // Find the screen whose frame contains the requested x position so
            // multi-monitor setups are handled correctly.
            NSScreen *targetScreen = [NSScreen mainScreen];
            for (NSScreen *s in [NSScreen screens])
            {
                NSRect f = [s frame];
                if ((CGFloat)px >= f.origin.x && (CGFloat)px < f.origin.x + f.size.width)
                {
                    targetScreen = s;
                    break;
                }
            }
            NSRect vis = [targetScreen visibleFrame];
            // `initWithContentRect:` sizes the CONTENT area (below the title
            // bar).  The window FRAME = content + title bar.  If we fill the
            // full visibleFrame height with content, the title bar would sit
            // above visibleFrame.top (in the menu bar zone), and macOS pushes
            // the window down by titleBarHeight — causing it to overlap the
            // Dock by exactly that amount.
            // Fix: measure the title bar height for this style mask and subtract
            // it from the available height; anchor the content bottom at
            // vis.origin.y (top of Dock) so the window frame sits flush within
            // the visible area.
            NSRect  probeFrame = [NSWindow frameRectForContentRect:NSMakeRect(0,0,100,100)
                                                         styleMask:styleMask];
            CGFloat titleBarH  = probeFrame.size.height - 100.0;
            CGFloat maxContentH = vis.size.height - titleBarH;
            if ((CGFloat)width  > vis.size.width) width  = (int)vis.size.width;
            if ((CGFloat)height > maxContentH)    height = (int)maxContentH;
            // Content bottom at Dock top → title bar top aligns with the
            // bottom of the macOS menu bar.
            CGFloat macY = vis.origin.y;
            contentRect  = NSMakeRect((CGFloat)px, macY, (CGFloat)width, (CGFloat)height);
        }
        else
        {
            NSRect screenFrame = [[NSScreen mainScreen] frame];
            CGFloat macY       = screenFrame.size.height - (CGFloat)py - (CGFloat)height;
            contentRect        = NSMakeRect((CGFloat)px, macY, (CGFloat)width, (CGFloat)height);
        }

        // Use MBMBorderlessWindow for borderless/fullscreen so canBecomeKeyWindow
        // returns YES.  A standard NSWindow with NSWindowStyleMaskBorderless
        // cannot become key, which blocks keyboard input and causes AppKit to
        // walk window lists erratically during shutdown → EXC_BAD_ACCESS.
        if (!border)
            ctx->window = [[MBMBorderlessWindow alloc] initWithContentRect:contentRect
                                                                 styleMask:styleMask
                                                                   backing:NSBackingStoreBuffered
                                                                     defer:NO];
        else
            ctx->window = [[NSWindow alloc] initWithContentRect:contentRect
                                                      styleMask:styleMask
                                                        backing:NSBackingStoreBuffered
                                                          defer:NO];
        if (!ctx->window)
        {
            ERROR_LOG("Metal: failed to create NSWindow.");
            return false;
        }
        [ctx->window setTitle:[NSString stringWithUTF8String:this->getNameApplication()]];
        // Disable AppKit's automatic release-on-close so non-ARC code controls the
        // window lifetime explicitly via [window release] in release().
        // Without this, [window close] calls [self release] while autoreleased
        // notification objects still reference the window, causing a double-free
        // when the outer @autoreleasepool in main() drains.
        [ctx->window setReleasedWhenClosed:NO];

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
        delegate.device = this->device;
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

        this->device->setWindowPosition(px, py);

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
        this->device->setBackBufferSize(static_cast<float>(actualBounds.size.width),
                                        static_cast<float>(actualBounds.size.height));
        ctx->metalLayer.drawableSize   = CGSizeMake(actualBounds.size.width  * scale,
                                                     actualBounds.size.height * scale);

        // Mark device as running.
        this->device->setRun(true);

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
            SPECIFIC_AUX_CONTEXT_DEVICE* ctx = this->device->getSpecificContextDevice();
            if (!ctx || !ctx->window) return;

            // The backbuffer size is in logical points (not scaled).
            // NSEvent coordinates arrive in logical points — no scale needed.

            // Converts a logical-point NSPoint (origin bottom-left) to engine coords
            // (origin top-left, physical pixels).
            // NSEvent coordinates are already in logical points (same space as
            // the backbuffer size).  No scale multiplication needed.
            auto toEngineXY = [&](NSPoint p, float& ex, float& ey) {
                ex = static_cast<float>(p.x);
                ey = static_cast<float>(this->device->getBackBufferHeight()) -
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
                        // with the logical backbuffer size and the end-of-frame poll below.
                        if (ctx->window)
                        {
                            NSRect bounds = [ctx->window contentView].bounds;
                            int newW = static_cast<int>(bounds.size.width);
                            int newH = static_cast<int>(bounds.size.height);
                            if (newW > 0 && newH > 0 &&
                                (newW != static_cast<int>(device->getBackBufferWidth()) ||
                                 newH != static_cast<int>(device->getBackBufferHeight())))
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
                this->device->setRun(false);

            // Poll for window resize every frame (catches programmatic resizes too).
            if (ctx->window && ctx->metalLayer)
            {
                NSRect bounds = [ctx->window contentView].bounds;
                // Logical point size, consistent with the backbuffer size.
                int newW = static_cast<int>(bounds.size.width);
                int newH = static_cast<int>(bounds.size.height);
                if (newW > 0 && newH > 0 &&
                    (newW != static_cast<int>(device->getBackBufferWidth()) ||
                     newH != static_cast<int>(device->getBackBufferHeight())))
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
        DEVICE *device = this->getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = device->getSpecificContextDevice();
        NSWindow* win = ctx->window;
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
        DEVICE *device = this->getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE* ctx = device->getSpecificContextDevice();
        ctx->release(wasDeviceLost);
    }

} // namespace mbm

#endif // __APPLE__ && !TARGET_OS_IOS
#endif // !ANDROID
#endif // USE_METAL
