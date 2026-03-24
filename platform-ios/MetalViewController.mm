/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License — see main-lua.mm for full text.                                                                           |
|-----------------------------------------------------------------------------------------------------------------------*/

#import "MetalViewController.h"

#ifdef USE_LUA
#include <lua-wrap/manager-lua.h>
#else
#include "my-scene.h"
#endif
#include <core_mbm/device.h>
#include <util-interface.h>
#include <map>
#include <string>
#include <vector>

// Declared in core-manager-metal-ios.mm — sets the CAMetalLayer for initGraphics().
extern "C" void mbm_ios_setMetalLayer(CAMetalLayer* _Nullable layer);

// ---------------------------------------------------------------------------
// Multi-touch tracking: map UITouch* → stable integer finger index.
// ---------------------------------------------------------------------------
static std::map<UITouch*, int> s_touchMap;
static int                     s_nextTouchID = 0;

static int touchID(UITouch* touch)
{
    auto it = s_touchMap.find(touch);
    if (it != s_touchMap.end())
        return it->second;
    int id = s_nextTouchID++;
    s_touchMap[touch] = id;
    return id;
}

static void releaseTouch(UITouch* touch)
{
    s_touchMap.erase(touch);
    if (s_touchMap.empty())
        s_nextTouchID = 0;
}

// ---------------------------------------------------------------------------
// Global engine instance (heap-allocated, survives the VC lifecycle).
// ---------------------------------------------------------------------------
#ifdef USE_LUA
static mbm::LUA_MANAGER* s_game = nullptr;
#else
static GAME* s_game = nullptr;
#endif

// ---------------------------------------------------------------------------
@implementation MetalViewController
{
    MBMMetalView*  _metalView;
    CADisplayLink* _displayLink;
}

// ── viewDidLoad — initialise the Metal view and the mini-mbm engine ──────────
- (void)viewDidLoad
{
    [super viewDidLoad];

    // 1. Create the Metal view and use it as the root view.
    _metalView = [[MBMMetalView alloc] initWithFrame:self.view.bounds];
    _metalView.contentScaleFactor = UIScreen.mainScreen.scale;  // set before layout
    _metalView.autoresizingMask = UIViewAutoresizingFlexibleWidth |
                                  UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:_metalView];

    // 2. Pixel dimensions of the view (points × Retina scale factor).
    // UIScreen.mainScreen.scale is reliable even before the view is on-screen;
    // contentScaleFactor would return 1.0 until after the first layout pass.
    const float scale = UIScreen.mainScreen.scale;
    const int w = static_cast<int>(_metalView.bounds.size.width  * scale);
    const int h = static_cast<int>(_metalView.bounds.size.height * scale);

#ifdef USE_LUA
    // 3. Locate the app bundle's resource directory — where Lua scripts live.
    NSString* bundleResourcePath = [[NSBundle mainBundle] resourcePath];
    const std::string resourcePath = bundleResourcePath
        ? [bundleResourcePath UTF8String]
        : "";

    // 4. Build the argument list for LUA_MANAGER (mirrors platform-android setup).
    std::vector<std::string> args;
    args.push_back("mini_mbm");
    args.push_back("--width");
    args.push_back(std::to_string(w));
    args.push_back("--height");
    args.push_back(std::to_string(h));
    args.push_back("--expectedwidth");
    args.push_back(std::to_string(w));
    args.push_back("--expectedheight");
    args.push_back(std::to_string(h));
    if (!resourcePath.empty())
    {
        args.push_back("--addPath");
        args.push_back(resourcePath);
        // Game assets (including main.lua) live under assets/ in the bundle.
        // Adding this sub-path lets the engine find main.lua and any Lua
        // modules that sit at the root of the GAME_ASSETS_DIR folder.
        args.push_back("--addPath");
        args.push_back(resourcePath + "/assets");
        // Documents directory: writable sandbox location for save files.
        // Adding it as a read path lets the engine find files written there.
        const char *home = getenv("HOME");
        if (home)
        {
            args.push_back("--addPath");
            args.push_back(std::string(home) + "/Documents");
        }
    }
    args.push_back("--scene");
    args.push_back("main.lua");

    // 5. Register the CAMetalLayer so initGraphics() can pick it up.
    mbm_ios_setMetalLayer(_metalView.metalLayer);

    // 6. Create the engine manager.
    s_game = new mbm::LUA_MANAGER(args);
    s_game->device->backBufferWidth  = static_cast<float>(w);
    s_game->device->backBufferHeight = static_cast<float>(h);
    // print the resolution to the console for debugging (Lua's print goes to Xcode's console).
    NSLog(@"[mini-mbm] view size: %d x %d scale %f ", w, h, scale);

    // 7. Initialise graphics + scene (calls core-manager-metal-ios.mm::initGraphics).
    constexpr bool border = false;
    if (!s_game->initializeSceneLua(w, h, w, h, border))
    {
        NSLog(@"[mini-mbm] initializeSceneLua failed — engine will not run.");
        delete s_game;
        s_game = nullptr;
        return;
    }

    // 8. Prepare Lua scene (returns immediately on iOS — no blocking loop).
    s_game->run();
#else
    // 3–4. Pure C++ path — no Lua arguments needed.

    // 5. Register the CAMetalLayer so initGraphics() can pick it up.
    mbm_ios_setMetalLayer(_metalView.metalLayer);

    // 6. Create the engine manager (setScene is called in GAME constructor).
    s_game = new GAME();
    s_game->device->backBufferWidth  = static_cast<float>(w);
    s_game->device->backBufferHeight = static_cast<float>(h);

    // 7. Initialise graphics + scene.
    constexpr bool border = false;
    if (!s_game->initGraphics("mini_mbm", w, h, 0, 0, border, false))
    {
        NSLog(@"[mini-mbm] initGraphics failed — engine will not run.");
        delete s_game;
        s_game = nullptr;
        return;
    }
#endif

    // 9. Start the per-frame display link.
    _displayLink = [CADisplayLink displayLinkWithTarget:self
                                               selector:@selector(renderFrame:)];
    if (@available(iOS 15.0, *)) {
        _displayLink.preferredFrameRateRange =
            CAFrameRateRangeMake(30, 60, 60); // ask for up to 60 fps
    } else {
        _displayLink.preferredFramesPerSecond = 60;
    }
    [_displayLink addToRunLoop:[NSRunLoop mainRunLoop]
                       forMode:NSRunLoopCommonModes];
}

// ── Render one engine frame per display refresh ──────────────────────────────
- (void)renderFrame:(CADisplayLink*)link
{
    (void)link;
    if (!s_game) return;

    if (!s_game->device->run)
    {
        [_displayLink invalidate];
        _displayLink = nil;
        // mbm.quit() was called — terminate the process cleanly.
        exit(0);
        return;
    }

    constexpr bool singleLoop    = true;
    constexpr bool doSwapBuffers = true;
    s_game->loop(singleLoop, doSwapBuffers);
}

// ── Handle view layout changes (rotation, split-screen) ─────────────────────
- (void)viewDidLayoutSubviews
{
    [super viewDidLayoutSubviews];

    if (!s_game) return;

    const float scale = UIScreen.mainScreen.scale;
    const int newW = static_cast<int>(_metalView.bounds.size.width  * scale);
    const int newH = static_cast<int>(_metalView.bounds.size.height * scale);

    if (newW > 0 && newH > 0 &&
        (newW != static_cast<int>(s_game->device->backBufferWidth) ||
         newH != static_cast<int>(s_game->device->backBufferHeight)))
    {
        s_game->onResizeWindow(newW, newH);
    }
}

// ── Multi-touch routing ──────────────────────────────────────────────────────
// locationInView: returns logical points; multiply by screen scale to get the
// pixel coordinates that match the engine's pixel-based coordinate system.

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    (void)event;
    if (!s_game) return;
    const float scale = UIScreen.mainScreen.scale;
    for (UITouch* touch in touches)
    {
        const int   id  = touchID(touch);
        const CGPoint p = [touch locationInView:_metalView];
        s_game->onTouchDown(id,
                            static_cast<float>(p.x * scale),
                            static_cast<float>(p.y * scale));
    }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    (void)event;
    if (!s_game) return;
    const float scale = UIScreen.mainScreen.scale;
    for (UITouch* touch in touches)
    {
        const int   id  = touchID(touch);
        const CGPoint p = [touch locationInView:_metalView];
        s_game->onTouchMove(id,
                            static_cast<float>(p.x * scale),
                            static_cast<float>(p.y * scale));
    }
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    (void)event;
    if (!s_game) return;
    const float scale = UIScreen.mainScreen.scale;
    for (UITouch* touch in touches)
    {
        const int   id  = touchID(touch);
        const CGPoint p = [touch locationInView:_metalView];
        s_game->onTouchUp(id,
                          static_cast<float>(p.x * scale),
                          static_cast<float>(p.y * scale));
        releaseTouch(touch);
    }
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    (void)event;
    if (!s_game) return;
    const float scale = UIScreen.mainScreen.scale;
    for (UITouch* touch in touches)
    {
        const int   id  = touchID(touch);
        const CGPoint p = [touch locationInView:_metalView];
        s_game->onTouchUp(id,
                          static_cast<float>(p.x * scale),
                          static_cast<float>(p.y * scale));
        releaseTouch(touch);
    }
}

// ── Preferred status bar style ───────────────────────────────────────────────

- (BOOL)prefersStatusBarHidden { return YES; }

- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
    return UIInterfaceOrientationMaskLandscape;
}

// ── Background / foreground pause ────────────────────────────────────────────

- (void)pauseRendering
{
    _displayLink.paused = YES;
}

- (void)resumeRendering
{
    if (s_game && s_game->device->run)
        _displayLink.paused = NO;
}

// ── Cleanup ──────────────────────────────────────────────────────────────────

- (void)dealloc
{
    [_displayLink invalidate];
    delete s_game;
    s_game = nullptr;
    // ARC inserts [super dealloc] automatically
}

@end
