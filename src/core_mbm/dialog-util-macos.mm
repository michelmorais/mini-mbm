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
// Advantages over tinyfiledialogs on macOS:
//   1. Custom/unregistered extensions (e.g. ".tile") are correctly shown.
//      tinyfd's osascript "choose file of type" path filters by UTI — extensions
//      without a registered UTI are invisible to the user.
//   2. The dialog appears in front of fullscreen windows.
//      tinyfd spawns a child process (osascript) which lands in a separate macOS
//      Space and is hidden behind the fullscreen window.
//   3. dispatch_sync(main_queue) guarantees the panel runs on the main thread even
//      when called from a secondary thread.

#if defined(__APPLE__) && !defined(ANDROID)

#import <Cocoa/Cocoa.h>
#include <core_mbm/dialog-util.h>
#include <string>

namespace dialog_util
{
    // Build an NSArray of bare extension strings from "*.ext" filter patterns.
    // Returns nil when the list contains a wildcard ("*.*" / "*") — which means "allow all files".
    static NSArray<NSString *> * buildAllowedTypes(const char * const * patterns, int count)
    {
        if (!patterns || count <= 0)
            return nil;

        NSMutableArray<NSString *> *types = [NSMutableArray array];
        for (int i = 0; i < count; ++i)
        {
            const char *p = patterns[i];
            if (!p || !*p)
                continue;
            // A wildcard pattern means "all files" — disable the type filter entirely.
            if (strcmp(p, "*.*") == 0 || strcmp(p, "*") == 0)
                return nil;
            // Strip leading "*." or leading "." to obtain the bare extension.
            if (p[0] == '*' && p[1] == '.')
                p += 2;
            else if (p[0] == '.')
                p += 1;
            if (*p)
                [types addObject:[NSString stringWithUTF8String:p]];
        }
        return types.count > 0 ? types : nil;
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

            // Apply extension filter (nil = allow all files).
            // setAllowedFileTypes: is deprecated in macOS 12+ in favour of contentTypes,
            // but it is the most reliable way to filter by custom/unregistered extensions
            // across all supported macOS versions.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            [panel setAllowedFileTypes:buildAllowedTypes(aFilterPatterns, aNumOfFilterPatterns)];
#pragma clang diagnostic pop

            // Bring the dialog in front of the application (including fullscreen windows).
            [NSApp activateIgnoringOtherApps:YES];
            if ([panel runModal] != NSModalResponseOK)
                return;

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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            [panel setAllowedFileTypes:buildAllowedTypes(aFilterPatterns, aNumOfFilterPatterns)];
#pragma clang diagnostic pop

            [NSApp activateIgnoringOtherApps:YES];
            if ([panel runModal] == NSModalResponseOK)
                captured = panel.URL.path;
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
