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

    main-lua-delivery.cpp — Linux game delivery entry point.

    Used instead of main-lua.cpp when GAME_ASSETS_DIR is defined at build time.
    The game assets are packed into a single encrypted .asset file placed in
    <AppDir>/assets/  (AppRun sets CWD=$APPDIR so "assets/" resolves correctly).

    Required compile-time definitions (set by src/CMakeLists.txt):
        GAME_ASSET_FILE      — filename of the .asset archive, e.g. "mygame.asset"
        GAME_ASSET_PASSWORD  — (may be empty string) decryption password
        GAME_APP_TITLE       — display name shown in the window title bar
*/

#ifndef __linux__
    #error "This delivery main targets Linux only"
#endif

#ifndef GAME_ASSET_FILE
    #error "GAME_ASSET_FILE must be defined by the build system"
#endif

#include <distribution.h>
#include <lua-wrap/manager-lua.h>
#include <core_mbm/util-interface.h>
#include <mini-mbm-lib.h>

#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

#ifndef GAME_APP_TITLE
    #define GAME_APP_TITLE "Game"
#endif

#ifndef GAME_ASSET_PASSWORD
    #define GAME_ASSET_PASSWORD ""
#endif

// From LUA: mbm.doCommands('get_tmp_folder')
static std::string temporary_folder_path;
static std::string title_app(GAME_APP_TITLE);

/* ------------------------------------------------------------------ */
/* Cleanup helpers                                                      */
/* ------------------------------------------------------------------ */

// The folder to delete on exit — set once after mkdtemp succeeds.
static std::string s_cleanup_folder;

// Called by std::atexit (and therefore by exit(), end of main, and our
// signal handler below). Safe to call more than once.
static void cleanup_temp_folder() noexcept
{
    if (!s_cleanup_folder.empty())
    {
        mbm::remove_folder(s_cleanup_folder.c_str());
        s_cleanup_folder.clear();
    }
}

// Convert SIGTERM / SIGINT into a normal exit() so atexit handlers fire.
static void on_signal(int /*sig*/) noexcept
{
    exit(1);
}

// Build a unique temp-dir template under the best available runtime base:
//   1. $XDG_RUNTIME_DIR  — user-private tmpfs, auto-wiped on logout (recommended)
//   2. /run/user/<uid>   — same, without the env var
//   3. /tmp              — last resort (world-readable, but POSIX-guaranteed)
static std::string make_delivery_temp_template(const char *app_name)
{
    // Choose base directory
    std::string base;
    const char *xdg = getenv("XDG_RUNTIME_DIR");
    if (xdg && xdg[0] == '/')
    {
        base = xdg;
    }
    else
    {
        char run_user[64] = "";
        snprintf(run_user, sizeof(run_user), "/run/user/%d", static_cast<int>(getuid()));
        struct stat st{};
        if (stat(run_user, &st) == 0 && S_ISDIR(st.st_mode))
            base = run_user;
        else
            base = "/tmp";
    }

    // Sanitise app name for use as a directory prefix
    std::string safe;
    for (const char *p = app_name; p && *p; ++p)
        safe += (isalnum(static_cast<unsigned char>(*p)) ? *p : '_');
    if (safe.empty()) safe = "game";

    return base + "/" + safe + "_XXXXXX";
}

void onDoNativeCommand(const char *command, const char * /*param*/, char *result, const int max_size_result)
{
    if (!command) return;
    if (strcmp(command, "get_tmp_folder") == 0)
    {
        // temporary_folder_path is set before main.lua starts; just return it.
        if (!temporary_folder_path.empty())
            strncpy(result, temporary_folder_path.c_str(), static_cast<size_t>(max_size_result) - 1);
    }
}

int main(int /*argc*/, const char ** /*argv*/)
{
    mbm::set_callback_do_commands(onDoNativeCommand);
    mbm::set_app_name(title_app.c_str());
    mbm::set_window_size(1920, 1080);
    mbm::set_expected_window_size(1920, 1080);
    mbm::set_verbose(false);
    mbm::disable_splash();

    /* ------------------------------------------------------------------ */
    /* Extract the .asset archive into a private temporary folder          */
    /* ------------------------------------------------------------------ */
    const char *asset_file  = GAME_ASSET_FILE;
    const char *password    = GAME_ASSET_PASSWORD;

    std::string asset_path = std::string("assets/") + asset_file;

    // Create a private temp dir under XDG_RUNTIME_DIR (owner-only, tmpfs).
    char temp_folder[1024] = "";
    {
        const std::string tmpl = make_delivery_temp_template(title_app.c_str());
        strncpy(temp_folder, tmpl.c_str(), sizeof(temp_folder) - 1);
        if (!mkdtemp(temp_folder))
        {
            fprintf(stderr, "[delivery] Failed to create temporary folder\n");
            return 1;
        }
        chmod(temp_folder, 0700);  // restrict to owner only
    }

    // Register cleanup for every exit path: normal return, exit(), and signals.
    s_cleanup_folder      = temp_folder;
    temporary_folder_path = temp_folder;  // expose to Lua via get_tmp_folder
    std::atexit(cleanup_temp_folder);
    signal(SIGTERM, on_signal);
    signal(SIGINT,  on_signal);

    {
        DISTRIBUTION_CTX *ctx = distribution_create();
        if (!ctx)
        {
            fprintf(stderr, "[delivery] Failed to create distribution context\n");
            return 1;
        }

        if (password && password[0] != '\0')
            distribution_set_password(ctx, password);

        const int ok = distribution_extract_asset(ctx, asset_path.c_str(), temp_folder);
        if (!ok)
            fprintf(stderr, "[delivery] extraction error: %s\n", distribution_last_error(ctx));

        distribution_destroy(ctx);

        if (!ok)
        {
            mbm::remove_folder(temp_folder);
            return 1;
        }
    }

    /* ------------------------------------------------------------------ */
    /* Add the extracted folder to asset search paths and run main.lua     */
    /* ------------------------------------------------------------------ */
    util::addPath(temp_folder);
    mbm::set_scene("main.lua");

    const int ret = mbm::onLoop();
    // cleanup_temp_folder() will be called automatically via std::atexit.
    return ret;
}
