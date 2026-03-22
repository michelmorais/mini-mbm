/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2021      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#if defined(__APPLE__)

#include "mini-mbm-lib.h"
#import <Cocoa/Cocoa.h>
#include <vector>
#include <string>

extern std::string my_app_name;

// Default resolution list (matches Linux implementation)
static mbm::SCREEN_RESOLUTION s_default_resolutions[] = {
    {640,  360,  "Low resolution"},
    {800,  600,  "XVGA"},
    {960,  540,  "qHD"},
    {1024, 768,  "XGA"},
    {1280, 720,  "Standard High Density (HD)"},
    {1280, 768,  "WXGA"},
    {1280, 800,  "WXGA"},
    {1600, 900,  "HD+"},
    {1920, 1080, "Standard Full HD Display"},
    {2560, 1440, "Standard Quad HD Display"},
    {3200, 1800, "QHD+"},
    {3840, 2160, "Standard Ultra HD Display"},
};
static const int s_default_resolutions_count = static_cast<int>(sizeof(s_default_resolutions) / sizeof(mbm::SCREEN_RESOLUTION));

// Custom script path set by NSOpenPanel. Lives in C++ space so it outlives the ObjC controller.
static std::string s_customScriptPath;

// ──────────────────────────────────────────────────────────────────────────────
// Objective-C++ dialog controller
// ──────────────────────────────────────────────────────────────────────────────
@interface MBMLauncherController : NSObject <NSWindowDelegate>
@property (nonatomic, strong) NSWindow*      window;
@property (nonatomic, strong) NSPopUpButton* monitorPopup;
@property (nonatomic, strong) NSPopUpButton* resolutionPopup;
@property (nonatomic, strong) NSPopUpButton* appPopup;
@property (nonatomic, strong) NSButton*      fullscreenCheckbox;
@property (nonatomic, assign) BOOL           confirmed;
@property (nonatomic, assign) int            prevAppSelection;
// Resolution source data (pointer into caller's list, valid during the modal loop)
@property (nonatomic, assign) mbm::SCREEN_RESOLUTION* srcResolutionList;
@property (nonatomic, assign) int            srcResolutionCount;
@property (nonatomic, assign) int            requestedWidth;
@property (nonatomic, assign) int            requestedHeight;
@property (nonatomic, assign) BOOL           firstResolutionBuild;
@end

@implementation MBMLauncherController

// ── Build all UI controls ─────────────────────────────────────────────────────
- (void)buildContentView:(mbm::APP_RUN*)appRun count:(int)count allowFullScreen:(BOOL)allowFS
{
    NSArray<NSScreen*>* screens = [NSScreen screens];
    int numScreens = (int)[screens count];

    // Window height depends on optional sections
    CGFloat winH = 260.0;
    if (count > 0) winH += 80.0;
    if (allowFS)   winH += 36.0;
    const CGFloat winW = 440.0;
    const CGFloat margin = 20.0;
    const CGFloat fieldW = winW - 2.0 * margin;

    // Center on main screen
    NSRect mainFrame = [[NSScreen mainScreen] frame];
    NSRect winRect = NSMakeRect(
        mainFrame.origin.x + (mainFrame.size.width  - winW) * 0.5,
        mainFrame.origin.y + (mainFrame.size.height - winH) * 0.5,
        winW, winH);

    NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable;
    self.window = [[NSWindow alloc] initWithContentRect:winRect
                                              styleMask:style
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    NSString* title = my_app_name.length() > 0
        ? [NSString stringWithUTF8String:my_app_name.c_str()]
        : @"Screen Options";
    [self.window setTitle:title];
    [self.window setDelegate:self];
    [self.window setReleasedWhenClosed:NO];

    NSView* content = self.window.contentView;
    CGFloat y = winH - 50.0; // work top-to-bottom

    // ── Monitor label + popup ─────────────────────────────────────────────────
    NSTextField* monLabel = [NSTextField labelWithString:@"Monitor:"];
    monLabel.frame = NSMakeRect(margin, y, fieldW, 20.0);
    [content addSubview:monLabel];
    y -= 28.0;

    self.monitorPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(margin, y, fieldW, 26.0) pullsDown:NO];
    for (int i = 0; i < numScreens; i++)
    {
        NSRect sf = [screens[i] frame];
        NSString* item = [NSString stringWithFormat:@"Monitor %d: %.0fx%.0f at (%.0f,%.0f)",
            i + 1, sf.size.width, sf.size.height, sf.origin.x, sf.origin.y];
        [self.monitorPopup addItemWithTitle:item];
    }
    if (numScreens == 0)
        [self.monitorPopup addItemWithTitle:@"Primary Monitor"];
    [self.monitorPopup setTarget:self];
    [self.monitorPopup setAction:@selector(monitorChanged:)];
    [content addSubview:self.monitorPopup];
    y -= 40.0;

    // ── Resolution label + popup ──────────────────────────────────────────────
    NSTextField* resLabel = [NSTextField labelWithString:@"Resolution:"];
    resLabel.frame = NSMakeRect(margin, y, fieldW, 20.0);
    [content addSubview:resLabel];
    y -= 28.0;

    self.resolutionPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(margin, y, fieldW, 26.0) pullsDown:NO];
    [self.resolutionPopup setTarget:self];
    [self.resolutionPopup setAction:@selector(resolutionChanged:)];
    [content addSubview:self.resolutionPopup];
    y -= 40.0;

    // ── App label + popup (optional) ─────────────────────────────────────────
    if (count > 0)
    {
        NSTextField* appLabel = [NSTextField labelWithString:@"Application:"];
        appLabel.frame = NSMakeRect(margin, y, fieldW, 20.0);
        [content addSubview:appLabel];
        y -= 28.0;

        self.appPopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(margin, y, fieldW, 26.0) pullsDown:NO];
        for (int i = 0; i < count; i++)
        {
            const char* name = appRun[i].name_eng ? appRun[i].name_eng : appRun[i].script_path;
            [self.appPopup addItemWithTitle:[NSString stringWithUTF8String:name ? name : ""]];
        }
        [self.appPopup setTarget:self];
        [self.appPopup setAction:@selector(appChanged:)];
        [content addSubview:self.appPopup];
        y -= 40.0;
    }

    // ── Fullscreen checkbox (optional) ────────────────────────────────────────
    if (allowFS)
    {
        self.fullscreenCheckbox = [NSButton checkboxWithTitle:@"Full Screen"
                                                       target:self
                                                       action:@selector(fullscreenChanged:)];
        self.fullscreenCheckbox.frame = NSMakeRect(margin, y, fieldW, 22.0);
        [content addSubview:self.fullscreenCheckbox];
        y -= 36.0;
    }

    // ── START button ─────────────────────────────────────────────────────────
    NSButton* startBtn = [[NSButton alloc] initWithFrame:NSMakeRect(winW - margin - 100.0, margin, 100.0, 30.0)];
    [startBtn setTitle:@"START"];
    [startBtn setBezelStyle:NSBezelStyleRounded];
    [startBtn setTarget:self];
    [startBtn setAction:@selector(startClicked:)];
    [content addSubview:startBtn];
    [self.window setDefaultButtonCell:startBtn.cell];

    // Populate resolution list for the initially-selected monitor
    self.firstResolutionBuild = YES;
    [self repopulateResolutions];
}

// ── Rebuild the resolution popup for the currently-selected monitor ───────────
- (void)repopulateResolutions
{
    NSArray<NSScreen*>* screens = [NSScreen screens];
    int monIdx = (int)[self.monitorPopup indexOfSelectedItem];

    int monW, monH;
    if (monIdx >= 0 && monIdx < (int)[screens count])
    {
        NSRect sf = [screens[monIdx] frame];
        monW = (int)sf.size.width;
        monH = (int)sf.size.height;
    }
    else
    {
        NSRect sf = [[NSScreen mainScreen] frame];
        monW = (int)sf.size.width;
        monH = (int)sf.size.height;
    }

    // Try to keep the previously-selected resolution title
    NSString* prevTitle = [self.resolutionPopup titleOfSelectedItem];
    [self.resolutionPopup removeAllItems];

    // Filter source list to entries that fit this monitor
    for (int i = 0; i < self.srcResolutionCount; i++)
    {
        mbm::SCREEN_RESOLUTION* r = &self.srcResolutionList[i];
        if (r->width <= monW && r->height <= monH)
        {
            NSString* t = [NSString stringWithFormat:@"%d x %d %s",
                r->width, r->height, r->description ? r->description : ""];
            [self.resolutionPopup addItemWithTitle:t];
        }
    }

    // Insert requested resolution if it fits and is not already listed
    if (self.requestedWidth > 0 && self.requestedHeight > 0 &&
        self.requestedWidth <= monW && self.requestedHeight <= monH)
    {
        NSString* reqPrefix = [NSString stringWithFormat:@"%d x %d", self.requestedWidth, self.requestedHeight];
        BOOL found = NO;
        for (NSMenuItem* item in self.resolutionPopup.itemArray)
        {
            if ([item.title hasPrefix:reqPrefix]) { found = YES; break; }
        }
        if (!found)
        {
            NSString* t = [NSString stringWithFormat:@"%d x %d Requested", self.requestedWidth, self.requestedHeight];
            [self.resolutionPopup addItemWithTitle:t];
        }
    }

    // Always have a "Native" entry
    NSString* nativePrefix = [NSString stringWithFormat:@"%d x %d", monW, monH];
    BOOL hasNative = NO;
    for (NSMenuItem* item in self.resolutionPopup.itemArray)
    {
        if ([item.title hasPrefix:nativePrefix]) { hasNative = YES; break; }
    }
    if (!hasNative)
        [self.resolutionPopup addItemWithTitle:[NSString stringWithFormat:@"%d x %d Native", monW, monH]];

    if (self.resolutionPopup.numberOfItems == 0)
        [self.resolutionPopup addItemWithTitle:[NSString stringWithFormat:@"%d x %d Native", monW, monH]];

    // Selection priority:
    //   First build: use requestedWidth/Height if given, else select last item (Native).
    //   Subsequent builds: try to restore the previous title, else last item.
    if (self.firstResolutionBuild)
    {
        self.firstResolutionBuild = NO;
        if (self.requestedWidth > 0 && self.requestedHeight > 0)
        {
            NSString* prefix = [NSString stringWithFormat:@"%d x %d", self.requestedWidth, self.requestedHeight];
            for (int i = 0; i < (int)self.resolutionPopup.numberOfItems; i++)
            {
                if ([[self.resolutionPopup itemTitleAtIndex:i] hasPrefix:prefix])
                {
                    [self.resolutionPopup selectItemAtIndex:i];
                    return;
                }
            }
        }
        [self.resolutionPopup selectItemAtIndex:self.resolutionPopup.numberOfItems - 1];
    }
    else
    {
        if (prevTitle)
            [self.resolutionPopup selectItemWithTitle:prevTitle];
        if ([self.resolutionPopup indexOfSelectedItem] < 0)
            [self.resolutionPopup selectItemAtIndex:self.resolutionPopup.numberOfItems - 1];
    }
}

// ── Action handlers ───────────────────────────────────────────────────────────
- (void)monitorChanged:(id)sender
{
    [self repopulateResolutions];
}

- (void)resolutionChanged:(id)sender
{
    // Selection read at confirm time — nothing to do here.
}

- (void)appChanged:(id)sender
{
    int idx   = (int)[self.appPopup indexOfSelectedItem];
    int total = (int)[self.appPopup numberOfItems];

    if (idx == total - 1) // "User specified" — last entry
    {
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setTitle:@"Select Lua Script"];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setAllowsMultipleSelection:NO];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [panel setAllowedFileTypes:@[@"lua", @"LUA"]];
#pragma clang diagnostic pop

        NSModalResponse resp = [panel runModal];
        if (resp == NSModalResponseOK && panel.URL)
        {
            s_customScriptPath = [panel.URL.path UTF8String];
            // Update the popup item label to show the chosen filename
            NSString* basename = [panel.URL.lastPathComponent stringByDeletingPathExtension];
            [[self.appPopup itemAtIndex:idx] setTitle:basename];
        }
        else
        {
            // User cancelled — revert to previous selection
            [self.appPopup selectItemAtIndex:self.prevAppSelection];
        }
    }
    else
    {
        self.prevAppSelection = idx;
        s_customScriptPath.clear();
    }
}

- (void)fullscreenChanged:(id)sender
{
    BOOL fs = (self.fullscreenCheckbox.state == NSControlStateValueOn);
    [self.resolutionPopup setEnabled:!fs];
}

- (void)startClicked:(id)sender
{
    self.confirmed = YES;
    [NSApp stopModal];
}

- (BOOL)windowShouldClose:(NSWindow*)sender
{
    [NSApp stopModal];
    return YES;
}

@end // MBMLauncherController

// ──────────────────────────────────────────────────────────────────────────────
// C++ bridge — implements the mbm:: public API
// ──────────────────────────────────────────────────────────────────────────────
namespace mbm
{

    bool select_app_and_resolution(APP_RUN* app_run, int size_app_run, int* index_app_selected,
                                   SCREEN_RESOLUTION* screen_resolution_list, int size_screen_resolution_list,
                                   bool allow_full_screen, const bool full_screen_checked,
                                   int requested_width, int requested_height)
    {
        // ── Ensure NSApplication is initialised ───────────────────────────────
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        if (![NSApp isRunning])
            [NSApp finishLaunching];

        // ── Use default list if none provided ─────────────────────────────────
        if (screen_resolution_list == nullptr)
        {
            screen_resolution_list  = s_default_resolutions;
            size_screen_resolution_list = s_default_resolutions_count;
        }

        s_customScriptPath.clear();

        // ── Build the controller and its window ───────────────────────────────
        MBMLauncherController* ctrl = [[MBMLauncherController alloc] init];
        ctrl.confirmed          = NO;
        ctrl.prevAppSelection   = 0;
        ctrl.srcResolutionList  = screen_resolution_list;
        ctrl.srcResolutionCount = size_screen_resolution_list;
        ctrl.requestedWidth     = requested_width;
        ctrl.requestedHeight    = requested_height;

        [ctrl buildContentView:app_run count:size_app_run allowFullScreen:(allow_full_screen ? YES : NO)];

        // ── Restore persisted selections from NSUserDefaults ──────────────────
        NSUserDefaults* ud        = [NSUserDefaults standardUserDefaults];
        NSInteger savedMonitor    = [ud integerForKey:@"mbm_last_monitor"];
        NSInteger savedResolution = [ud integerForKey:@"mbm_last_resolution"];
        NSInteger savedApp        = [ud integerForKey:@"mbm_last_app"];
        BOOL      savedFullscreen = [ud boolForKey:@"mbm_last_fullscreen"];

        // Pre-select monitor (triggers resolution repopulation implicitly)
        if (savedMonitor < (NSInteger)ctrl.monitorPopup.numberOfItems)
            [ctrl.monitorPopup selectItemAtIndex:savedMonitor];
        // Repopulate resolutions for the restored monitor
        [ctrl repopulateResolutions];

        // Pre-select resolution: explicit CLI request takes priority over saved preference
        if (requested_width == 0 && savedResolution < (NSInteger)ctrl.resolutionPopup.numberOfItems)
            [ctrl.resolutionPopup selectItemAtIndex:savedResolution];

        // Pre-select app: explicit CLI index takes priority over saved preference
        if (ctrl.appPopup)
        {
            NSInteger appIdx = (index_app_selected && *index_app_selected >= 0
                                && *index_app_selected < (int)ctrl.appPopup.numberOfItems)
                ? (NSInteger)*index_app_selected
                : savedApp;
            if (appIdx < (NSInteger)ctrl.appPopup.numberOfItems)
            {
                [ctrl.appPopup selectItemAtIndex:appIdx];
                ctrl.prevAppSelection = (int)appIdx;
            }
        }

        // Pre-set fullscreen checkbox and sync resolution popup enabled state
        if (ctrl.fullscreenCheckbox)
        {
            BOOL wantFS = allow_full_screen && (savedFullscreen || full_screen_checked);
            ctrl.fullscreenCheckbox.state = wantFS ? NSControlStateValueOn : NSControlStateValueOff;
            [ctrl.resolutionPopup setEnabled:!wantFS];
        }

        // ── Run the modal dialog ──────────────────────────────────────────────
        [NSApp activateIgnoringOtherApps:YES];
        [NSApp runModalForWindow:ctrl.window];
        [ctrl.window orderOut:nil];

        if (!ctrl.confirmed)
            return false;

        // ── Apply selections ──────────────────────────────────────────────────
        NSArray<NSScreen*>* screens = [NSScreen screens];
        int monIdx = (int)[ctrl.monitorPopup indexOfSelectedItem];

        NSRect screenFrame = [[NSScreen mainScreen] frame];
        if (monIdx >= 0 && monIdx < (int)[screens count])
            screenFrame = [screens[monIdx] frame];

        int pos_x = (int)screenFrame.origin.x;
        // The engine converts py → NSScreen-y via: macY = mainScreenH - py - windowH
        // We want the window at the TOP of the target screen, so solve for pos_y:
        //   mainScreenH - pos_y - windowH  =  screenFrame.origin.y + screenFrame.size.height - windowH
        //   → pos_y = mainScreenH - screenFrame.origin.y - screenFrame.size.height
        int mainScreenH = (int)[[NSScreen mainScreen] frame].size.height;
        int pos_y = mainScreenH - (int)(screenFrame.origin.y + screenFrame.size.height);
        int sel_w = (int)screenFrame.size.width;
        int sel_h = (int)screenFrame.size.height;

        BOOL fullscreen = ctrl.fullscreenCheckbox
            ? (ctrl.fullscreenCheckbox.state == NSControlStateValueOn)
            : NO;

        if (!fullscreen)
        {
            // Parse "W x H description" from the selected resolution title
            NSString* resTitle = [ctrl.resolutionPopup titleOfSelectedItem];
            if (resTitle)
            {
                int rw = 0, rh = 0;
                if (sscanf([resTitle UTF8String], "%d x %d", &rw, &rh) == 2 && rw > 0 && rh > 0)
                {
                    sel_w = rw;
                    sel_h = rh;
                }
            }
        }
        else
        {
            mbm::disable_window_border();
            mbm::set_expected_window_size(sel_w, sel_h);
        }

        mbm::set_window_position(pos_x, pos_y);
        mbm::set_window_size(sel_w, sel_h);

        // ── Persist selections ────────────────────────────────────────────────
        [ud setInteger:[ctrl.monitorPopup indexOfSelectedItem]    forKey:@"mbm_last_monitor"];
        [ud setInteger:[ctrl.resolutionPopup indexOfSelectedItem] forKey:@"mbm_last_resolution"];
        if (ctrl.appPopup)
            [ud setInteger:[ctrl.appPopup indexOfSelectedItem]    forKey:@"mbm_last_app"];
        [ud setBool:fullscreen forKey:@"mbm_last_fullscreen"];
        [ud synchronize];

        // ── Report chosen app index back to the caller ────────────────────────
        if (index_app_selected)
        {
            if (ctrl.appPopup)
                *index_app_selected = (int)[ctrl.appPopup indexOfSelectedItem];
            else
                *index_app_selected = 0;
        }

        // If the user browsed for a custom script, update the last APP_RUN entry
        if (!s_customScriptPath.empty() && app_run && size_app_run > 0)
            app_run[size_app_run - 1].script_path = s_customScriptPath.c_str();

        return true;
    }

    bool select_resolution(SCREEN_RESOLUTION* screen_resolution_list, int size_screen_resolution_list,
                           bool allow_full_screen, const bool full_screen_checked)
    {
        return select_app_and_resolution(nullptr, 0, nullptr,
                                         screen_resolution_list, size_screen_resolution_list,
                                         allow_full_screen, full_screen_checked, 0, 0);
    }

} // namespace mbm

#endif // defined(__APPLE__)
