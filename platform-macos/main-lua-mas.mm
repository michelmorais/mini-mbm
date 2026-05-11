/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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
|------------------------------------------------------------------------------------------------------------------------|

    main-lua-mas.mm — macOS Mac App Store (MAS) game entry point.

    Used when -DMAS_DELIVERY=1 is passed to CMake.  Assets are embedded
    into the .app bundle under Contents/Resources/assets/ by Xcode.

    At startup this entry point copies all assets from the read-only bundle
    into a private writable temp directory (identical strategy to
    main-lua-delivery.cpp).  This is required because mesh-manager and other
    engine subsystems write decompressed files to disk alongside the source
    asset — they must have write access to the directory they work in.

    Required compile-time definition (set by src/CMakeLists.txt):
        GAME_APP_TITLE  — display name shown in the window title bar
*/

#ifndef __APPLE__
    #error "This entry point targets macOS (Mac App Store delivery)"
#endif

#import <Foundation/Foundation.h>

#include <lua-wrap/manager-lua.h>
#include <core_mbm/util-interface.h>
#include <mini-mbm-lib.h>

#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#ifndef GAME_APP_TITLE
    #define GAME_APP_TITLE "Game"
#endif

static std::string s_temp_folder;
static std::string title_app(GAME_APP_TITLE);

/* ------------------------------------------------------------------ */
/* Cleanup helpers                                                      */
/* ------------------------------------------------------------------ */

static void cleanup_temp_folder() noexcept
{
    if (!s_temp_folder.empty())
    {
        mbm::remove_folder(s_temp_folder.c_str());
        s_temp_folder.clear();
    }
}

static void on_signal(int /*sig*/) noexcept { exit(1); }

// From Lua: mbm.doCommands('get_tmp_folder') or mbm.doCommands('get_save_dir')
void onDoNativeCommand(const char *command, const char * /*param*/, char *result, const int max_size_result)
{
    if (!command) return;

    if (strcmp(command, "get_tmp_folder") == 0 && !s_temp_folder.empty())
    {
        strncpy(result, s_temp_folder.c_str(), static_cast<size_t>(max_size_result) - 1);
        return;
    }

    if (strcmp(command, "get_save_dir") == 0)
    {
        @autoreleasepool
        {
            // Returns ~/Library/Application Support/<AppName>/
            // Inside the App Sandbox this is remapped to the container automatically.
            // No extra entitlement is needed — the sandbox always permits writes here.
            NSArray<NSString *> *paths = NSSearchPathForDirectoriesInDomains(
                NSApplicationSupportDirectory, NSUserDomainMask, YES);
            if (paths.count > 0)
            {
                NSString *appName   = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleName"];
                if (!appName || appName.length == 0)
                    appName = [NSString stringWithUTF8String:title_app.c_str()];
                NSString *saveDir   = [paths[0] stringByAppendingPathComponent:appName];

                NSFileManager *fm   = [NSFileManager defaultManager];
                NSError       *err  = nil;
                [fm createDirectoryAtPath:saveDir
                  withIntermediateDirectories:YES
                             attributes:nil
                                  error:&err];
                strncpy(result, [saveDir UTF8String], static_cast<size_t>(max_size_result) - 1);
            }
        }
    }
}

int main(int /*argc*/, const char ** /*argv*/)
{
    @autoreleasepool
    {
        mbm::set_callback_do_commands(onDoNativeCommand);
        mbm::set_app_name(title_app.c_str());
        mbm::set_expected_window_size(1920, 1080);
        mbm::set_verbose(false);
        mbm::disable_splash();

        /* ------------------------------------------------------------------ */
        /* Monitor / resolution / fullscreen picker                           */
        /* (No app list — MAS delivers a single game.)                        */
        /* Uses NSUserDefaults to remember the player's choices between       */
        /* launches. Defaults to fullscreen on first run.                     */
        /* ------------------------------------------------------------------ */
        // Wrap the launcher dialog in its own pool so that AppKit objects
        // autoreleased during the modal run-loop (NSEvent, NSNotification, etc.)
        // are drained here — while the launcher window is still alive — instead
        // of accumulating in the outer pool until main() exits.
        bool launcherOk = false;
        @autoreleasepool
        {
            launcherOk = mbm::select_app_and_resolution(
                nullptr, 0, nullptr,    // no app list — single game
                nullptr, 0,             // use built-in resolution list
                true,  true,            // allow fullscreen; default to checked
                1920,  1080);           // suggest 1920 × 1080 for windowed
        }
        if (!launcherOk)
            return 0;   // user dismissed the launcher

        /* ------------------------------------------------------------------ */
        /* Create a writable private temp directory                           */
        /* ------------------------------------------------------------------ */
        // mesh-manager and other engine subsystems write decompressed files
        // (.spt, .tile, .fnt …) to disk.  The bundle's Resources/ is read-only,
        // so we copy all assets into a writable per-session temp folder first.
        {
            const char *tmpdir = getenv("TMPDIR");
            std::string base = (tmpdir && tmpdir[0] == '/') ? tmpdir : "/tmp";
            while (base.size() > 1 && base.back() == '/') base.pop_back();

            std::string safe;
            for (const char *p = title_app.c_str(); *p; ++p)
                safe += (isalnum(static_cast<unsigned char>(*p)) ? *p : '_');
            if (safe.empty()) safe = "game";

            std::string tmpl = base + "/" + safe + "_XXXXXX";
            char temp[1024] = "";
            strncpy(temp, tmpl.c_str(), sizeof(temp) - 1);
            if (!mkdtemp(temp))
            {
                fprintf(stderr, "[mas] Failed to create temporary folder\n");
                return 1;
            }
            chmod(temp, 0700);
            s_temp_folder = temp;
        }

        std::atexit(cleanup_temp_folder);
        signal(SIGTERM, on_signal);
        signal(SIGINT,  on_signal);

        /* ------------------------------------------------------------------ */
        /* Copy assets from read-only bundle into writable temp directory     */
        /* ------------------------------------------------------------------ */
        NSString *resourcePath = [[NSBundle mainBundle] resourcePath];
        if (!resourcePath)
        {
            fprintf(stderr, "[mas] NSBundle has no resourcePath — aborting\n");
            return 1;
        }

        NSString *srcAssets = [resourcePath stringByAppendingPathComponent:@"assets"];
        NSString *dstAssets = [[NSString stringWithUTF8String:s_temp_folder.c_str()]
                                stringByAppendingPathComponent:@"assets"];

        NSFileManager *fm = [NSFileManager defaultManager];
        if ([fm fileExistsAtPath:srcAssets])
        {
            NSError *copyErr = nil;
            if (![fm copyItemAtPath:srcAssets toPath:dstAssets error:&copyErr])
            {
                fprintf(stderr, "[mas] Failed to copy assets to temp dir: %s\n",
                        copyErr ? [[copyErr localizedDescription] UTF8String] : "unknown error");
                return 1;
            }
        }
        else
        {
            fprintf(stderr, "[mas] Bundle contains no assets/ directory — aborting\n");
            return 1;
        }

        /* ------------------------------------------------------------------ */
        /* Add writable temp paths to engine search paths and run main.lua    */
        /* ------------------------------------------------------------------ */
        util::addPath(s_temp_folder.c_str());
        util::addPath([dstAssets UTF8String]);

        mbm::set_scene("main.lua");

        // Wrap the game loop in its own pool.  Non-ARC Metal/AppKit code inside
        // onLoop() autoreleases command buffers, encoders, notifications, etc.
        // into whichever pool is current.  Draining a dedicated inner pool here
        // (while the MTL device and NSWindow are still valid) prevents those
        // objects from reaching the outer pool and crashing on drain after the
        // Metal device has already been released.
        int ret = 0;
        @autoreleasepool
        {
            ret = mbm::onLoop();
        }
        // cleanup_temp_folder() is called automatically via std::atexit.
        return ret;
    }
}

