/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2026 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

// macOS native file dialog implementation using NSOpenPanel / NSSavePanel.
//
// Fixes over tinyfiledialogs on macOS:
//   1. Custom/unregistered extensions (e.g. ".tile") are selectable.
//      setAllowedFileTypes: resolves extension strings through the UTI system.
//      Extensions with no registered UTI (like ".tile") are grayed out even when
//      the file is visible. Fix: use NSOpenSavePanelDelegate / shouldEnableURL:
//      to filter purely by extension string, bypassing the UTI resolver.
//   2. The dialog appears in front of fullscreen windows.
//      The engine's borderless fullscreen window sits at NSMainMenuWindowLevel+1
//      (level 25). An NSPanel defaults to NSNormalWindowLevel (0) or
//      NSModalPanelWindowLevel (8) — both below 25, so the dialog is hidden.
//      Fix: read the main window's current level, raise the panel above it, and
//      restore after the modal session ends.
//   3. dispatch_sync(main_queue) guarantees the panel runs on the main thread.

#if defined(__APPLE__) && !defined(ANDROID)

#import <Cocoa/Cocoa.h>
#include <core_mbm/dialog-util.h>
#include <string>

// ---------------------------------------------------------------------------
// Delegate: filters by file extension without going through the UTI system.
// Directories are always enabled so the user can navigate.
// ---------------------------------------------------------------------------
@interface MBMDialogDelegate : NSObject <NSOpenSavePanelDelegate>
@property (nonatomic, strong) NSArray<NSString *> *allowedExtensions; // lowercase, no dot
@end

@implementation MBMDialogDelegate
- (BOOL)panel:(id)sender shouldEnableURL:(NSURL *)url
{
    // No filter — allow everything.
    if (!self.allowedExtensions || self.allowedExtensions.count == 0)
        return YES;

    // Always allow directories so the user can navigate into them.
    BOOL isDir = NO;
    [[NSFileManager defaultManager] fileExistsAtPath:url.path isDirectory:&isDir];
    if (isDir)
        return YES;

    NSString *ext = url.pathExtension.lowercaseString;
    for (NSString *allowed in self.allowedExtensions)
        if ([allowed isEqualToString:ext])
            return YES;

    return NO;
}
@end

namespace dialog_util
{
    // Parse "*.ext" / ".ext" / "ext" patterns into a lowercase bare-extension array.
    // Returns nil when the list is empty or contains a wildcard (show all files).
    static NSArray<NSString *> * parseExtensions(const char * const * patterns, int count)
    {
        if (!patterns || count <= 0)
            return nil;

        NSMutableArray<NSString *> *exts = [NSMutableArray array];
        for (int i = 0; i < count; ++i)
        {
            const char *p = patterns[i];
            if (!p || !*p)
                continue;
            // Wildcard → show all files
            if (strcmp(p, "*.*") == 0 || strcmp(p, "*") == 0)
                return nil;
            if (p[0] == '*' && p[1] == '.')
                p += 2;
            else if (p[0] == '.')
                p += 1;
            if (*p)
                [exts addObject:[[NSString stringWithUTF8String:p] lowercaseString]];
        }
        return exts.count > 0 ? exts : nil;
    }

    // Raise the panel above the app's main window (handles fullscreen level),
    // run the modal session, then restore the previous window level.
    static NSModalResponse runPanelModal(NSSavePanel *panel)
    {
        // The engine sets NSMainMenuWindowLevel+1 for borderless fullscreen windows.
        // Raise the panel above whatever level the main window currently has.
        NSWindow *mainWin    = [NSApp mainWindow] ?: [NSApp keyWindow];
        NSWindowLevel origLevel = NSNormalWindowLevel;
        if (mainWin)
        {
            origLevel = mainWin.level;
            // Temporarily drop the main window to normal level so it doesn't
            // occlude the panel.
            [mainWin setLevel:NSNormalWindowLevel];
        }

        // Set panel level above NSModalPanelWindowLevel as a safety margin.
        [panel setLevel:NSModalPanelWindowLevel + 1];
        [NSApp activateIgnoringOtherApps:YES];
        [panel orderFrontRegardless];

        NSModalResponse result = [NSApp runModalForWindow:panel];

        // Restore the main window to its original level (re-enters fullscreen look).
        if (mainWin)
            [mainWin setLevel:origLevel];

        return result;
    }

    const char * openFileDialog(
        const char *             aTitle,
        const char *             aDefaultPathAndFile,
        const char * const *     aFilterPatterns,
        int                      aNumOfFilterPatterns,
        int                      aAllowMultipleSelects)
    {
        static std::string sResult;
        __block NSString *captured = nil;

        dispatch_block_t block = ^{
            NSOpenPanel *panel = [NSOpenPanel openPanel];

            if (aTitle && *aTitle)
                [panel setMessage:[NSString stringWithUTF8String:aTitle]];

            [panel setCanChooseFiles:YES];
            [panel setCanChooseDirectories:NO];
            [panel setAllowsMultipleSelection:(aAllowMultipleSelects ? YES : NO)];

            // Set starting directory from the directory component of the default path.
            if (aDefaultPathAndFile && *aDefaultPathAndFile)
            {
                NSString *s   = [NSString stringWithUTF8String:aDefaultPathAndFile];
                NSString *dir = [s stringByDeletingLastPathComponent];
                if (dir.length > 0 && ![dir isEqualToString:@"."])
                    [panel setDirectoryURL:[NSURL fileURLWithPath:dir isDirectory:YES]];
            }

            // Use delegate-based filtering to bypass the UTI resolver.
            // setAllowedFileTypes: grays out files whose extension has no registered UTI
            // (e.g. ".tile"). The delegate checks the raw extension string instead.
            NSArray<NSString *> *exts = parseExtensions(aFilterPatterns, aNumOfFilterPatterns);
            MBMDialogDelegate *delegate = nil;
            if (exts)
            {
                delegate = [[MBMDialogDelegate alloc] init];
                delegate.allowedExtensions = exts;
                [panel setDelegate:delegate];
            }

            if (runPanelModal(panel) != NSModalResponseOK)
            {
                [panel setDelegate:nil];
                return;
            }
            [panel setDelegate:nil];

            if (aAllowMultipleSelects)
            {
                NSMutableString *joined = [NSMutableString string];
                for (NSUInteger i = 0; i < panel.URLs.count; ++i)
                {
                    if (i > 0)
                        [joined appendString:@"|"];
                    [joined appendString:panel.URLs[i].path];
                }
                captured = [joined copy];
            }
            else
            {
                captured = panel.URL.path;
            }
        };

        if ([NSThread isMainThread])
            block();
        else
            dispatch_sync(dispatch_get_main_queue(), block);

        if (!captured)
            return nullptr;

        sResult = [captured UTF8String];
        return sResult.c_str();
    }

    const char * saveFileDialog(
        const char *             aTitle,
        const char *             aDefaultPathAndFile,
        const char * const *     aFilterPatterns,
        int                      aNumOfFilterPatterns)
    {
        static std::string sResult;
        __block NSString *captured = nil;

        dispatch_block_t block = ^{
            NSSavePanel *panel = [NSSavePanel savePanel];

            if (aTitle && *aTitle)
                [panel setMessage:[NSString stringWithUTF8String:aTitle]];

            if (aDefaultPathAndFile && *aDefaultPathAndFile)
            {
                NSString *s    = [NSString stringWithUTF8String:aDefaultPathAndFile];
                NSString *dir  = [s stringByDeletingLastPathComponent];
                NSString *name = [s lastPathComponent];
                if (dir.length > 0 && ![dir isEqualToString:@"."])
                    [panel setDirectoryURL:[NSURL fileURLWithPath:dir isDirectory:YES]];
                if (name.length > 0)
                    [panel setNameFieldStringValue:name];
            }

            // Save panel: use delegate filtering as well so the allowed-extensions
            // popup works correctly for unregistered extensions.
            NSArray<NSString *> *exts = parseExtensions(aFilterPatterns, aNumOfFilterPatterns);
            MBMDialogDelegate *delegate = nil;
            if (exts)
            {
                delegate = [[MBMDialogDelegate alloc] init];
                delegate.allowedExtensions = exts;
                [panel setDelegate:delegate];
            }

            if (runPanelModal(panel) == NSModalResponseOK)
                captured = panel.URL.path;

            [panel setDelegate:nil];
        };

        if ([NSThread isMainThread])
            block();
        else
            dispatch_sync(dispatch_get_main_queue(), block);

        if (!captured)
            return nullptr;

        sResult = [captured UTF8String];
        return sResult.c_str();
    }
}

#endif // defined(__APPLE__) && !defined(ANDROID)
