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

    main-lua-delivery.cpp — macOS game delivery entry point.

    Used instead of main-lua.cpp when GAME_ASSETS_DIR is defined at build time.
    The .asset archive is expected at:
        <AppBundle>/Contents/Resources/assets/<GAME_ASSET_FILE>

    Required compile-time definitions (set by src/CMakeLists.txt):
        GAME_ASSET_FILE      — filename of the .asset archive, e.g. "mygame.asset"
        GAME_ASSET_PASSWORD  — (may be empty string) decryption password
        GAME_APP_TITLE       — display name shown in the window title bar
*/

#ifndef __APPLE__
    #error "This delivery main targets macOS/Apple only"
#endif

#ifndef GAME_ASSET_FILE
    #error "GAME_ASSET_FILE must be defined by the build system"
#endif

#include <distribution.h>
#include <lua-wrap/manager-lua.h>
#include <core_mbm/util-interface.h>
#include <mini-mbm-lib.h>

#include <mach-o/dyld.h>  /* _NSGetExecutablePath */
#include <libgen.h>        /* dirname */
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

static std::string temporary_folder_path;
static std::string title_app(GAME_APP_TITLE);

/* ------------------------------------------------------------------ */
/* Cleanup helpers                                                      */
/* ------------------------------------------------------------------ */

static std::string s_cleanup_folder;

static void cleanup_temp_folder() noexcept
{
    if (!s_cleanup_folder.empty())
    {
        mbm::remove_folder(s_cleanup_folder.c_str());
        s_cleanup_folder.clear();
    }
}

static void on_signal(int /*sig*/) noexcept
{
    exit(1);
}

// On macOS the recommended private temp location is $TMPDIR (set by launchd
// to a per-user, per-session dir like /var/folders/…).  Falls back to /tmp.
static std::string make_delivery_temp_template(const char *app_name)
{
    std::string base;
    const char *tmpdir = getenv("TMPDIR");
    if (tmpdir && tmpdir[0] == '/')
        base = tmpdir;
    else
        base = "/tmp";
    // Strip trailing slash
    while (base.size() > 1 && base.back() == '/') base.pop_back();

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
        if (!temporary_folder_path.empty())
            strncpy(result, temporary_folder_path.c_str(), static_cast<size_t>(max_size_result) - 1);
    }
}

/* Returns the directory containing the running executable. */
static std::string get_binary_dir()
{
    char path[4096] = "";
    uint32_t size = sizeof(path);
    if (_NSGetExecutablePath(path, &size) != 0)
        return ".";
    char resolved[4096] = "";
    if (!realpath(path, resolved))
        return dirname(path);
    return dirname(resolved);
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
    /* Resolve .asset path: <binary>/../Resources/assets/<file>           */
    /* ------------------------------------------------------------------ */
    const char *asset_file = GAME_ASSET_FILE;
    const char *password   = GAME_ASSET_PASSWORD;

    const std::string bin_dir  = get_binary_dir();
    const std::string asset_path = bin_dir + "/../Resources/assets/" + asset_file;

    /* ------------------------------------------------------------------ */
    /* Extract into a private temporary folder                             */
    /* ------------------------------------------------------------------ */
    // Create a private temp dir under $TMPDIR (per-user, per-session on macOS).
    char temp_folder[1024] = "";
    {
        const std::string tmpl = make_delivery_temp_template(title_app.c_str());
        strncpy(temp_folder, tmpl.c_str(), sizeof(temp_folder) - 1);
        if (!mkdtemp(temp_folder))
        {
            fprintf(stderr, "[delivery] Failed to create temporary folder\n");
            return 1;
        }
        chmod(temp_folder, 0700);
    }

    s_cleanup_folder      = temp_folder;
    temporary_folder_path = temp_folder;
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
    /* Add extracted folder to search paths and run main.lua               */
    /* ------------------------------------------------------------------ */
    util::addPath(temp_folder);
    mbm::set_scene("main.lua");

    const int ret = mbm::onLoop();
    // cleanup_temp_folder() called automatically via std::atexit.
    return ret;
}
