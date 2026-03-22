/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License — see main-lua.mm for full text.                                                                           |
|-----------------------------------------------------------------------------------------------------------------------*/

#if defined(MBM_PLATFORM_IOS)

#import "MetalViewController.h"

#include <lua-wrap/manager-lua.h>
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
static mbm::LUA_MANAGER* s_game = nullptr;

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
    _metalView.autoresizingMask = UIViewAutoresizingFlexibleWidth |
                                  UIViewAutoresizingFlexibleHeight;
    [self.view addSubview:_metalView];

    // 2. Logical point dimensions of the view.
    const int w = static_cast<int>(_metalView.bounds.size.width);
    const int h = static_cast<int>(_metalView.bounds.size.height);

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
    }
    args.push_back("--scene");
    args.push_back("main.lua");
    args.push_back("--nosplash");

    // 5. Register the CAMetalLayer so initGraphics() can pick it up.
    mbm_ios_setMetalLayer(_metalView.metalLayer);

    // 6. Create the engine manager.
    s_game = new mbm::LUA_MANAGER(args);
    s_game->device->backBufferWidth  = static_cast<float>(w);
    s_game->device->backBufferHeight = static_cast<float>(h);

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

    // 9. Start the per-frame display link.
    _displayLink = [CADisplayLink displayLinkWithTarget:self
                                               selector:@selector(renderFrame:)];
    _displayLink.preferredFrameRateRange =
        CAFrameRateRangeMake(30, 60, 60); // ask for up to 60 fps
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

    const int newW = static_cast<int>(_metalView.bounds.size.width);
    const int newH = static_cast<int>(_metalView.bounds.size.height);

    if (newW > 0 && newH > 0 &&
        (newW != static_cast<int>(s_game->device->backBufferWidth) ||
         newH != static_cast<int>(s_game->device->backBufferHeight)))
    {
        s_game->onResizeWindow(newW, newH);
    }
}

// ── Multi-touch routing ──────────────────────────────────────────────────────

- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    (void)event;
    if (!s_game) return;
    for (UITouch* touch in touches)
    {
        const int   id  = touchID(touch);
        const CGPoint p = [touch locationInView:_metalView];
        s_game->onTouchDown(id,
                            static_cast<float>(p.x),
                            static_cast<float>(p.y));
    }
}

- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    (void)event;
    if (!s_game) return;
    for (UITouch* touch in touches)
    {
        const int   id  = touchID(touch);
        const CGPoint p = [touch locationInView:_metalView];
        s_game->onTouchMove(id,
                            static_cast<float>(p.x),
                            static_cast<float>(p.y));
    }
}

- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    (void)event;
    if (!s_game) return;
    for (UITouch* touch in touches)
    {
        const int   id  = touchID(touch);
        const CGPoint p = [touch locationInView:_metalView];
        s_game->onTouchUp(id,
                          static_cast<float>(p.x),
                          static_cast<float>(p.y));
        releaseTouch(touch);
    }
}

- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event
{
    (void)event;
    if (!s_game) return;
    for (UITouch* touch in touches)
    {
        const int   id  = touchID(touch);
        const CGPoint p = [touch locationInView:_metalView];
        s_game->onTouchUp(id,
                          static_cast<float>(p.x),
                          static_cast<float>(p.y));
        releaseTouch(touch);
    }
}

// ── Preferred status bar style ───────────────────────────────────────────────

- (BOOL)prefersStatusBarHidden { return YES; }

- (UIInterfaceOrientationMask)supportedInterfaceOrientations
{
    return UIInterfaceOrientationMaskLandscape;
}

// ── Cleanup ──────────────────────────────────────────────────────────────────

- (void)dealloc
{
    [_displayLink invalidate];
    delete s_game;
    s_game = nullptr;
}

@end

#endif // MBM_PLATFORM_IOS
