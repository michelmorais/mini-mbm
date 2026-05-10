/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                            |
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

    main-mingw-delivery.cpp — Windows / MinGW game delivery entry point.

    Used instead of main-mingw.cpp when GAME_ASSETS_DIR is defined at build time.
    The .asset archive is expected at:
        <GameDir>\assets\<GAME_ASSET_FILE>

    Required compile-time definitions (set by src/CMakeLists.txt):
        GAME_ASSET_FILE      — filename of the .asset archive, e.g. "mygame.asset"
        GAME_ASSET_PASSWORD  — (may be empty string) decryption password
        GAME_APP_TITLE       — display name shown in the window title bar
*/

#ifndef _WIN32
    #error "This delivery main targets Windows only"
#endif

#ifndef GAME_ASSET_FILE
    #error "GAME_ASSET_FILE must be defined by the build system"
#endif

#include <distribution.h>
#include <mini-mbm-lib.h>
#include <core_mbm/util-interface.h>

#include <windows.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Numeric resource ID for the icon compiled into the exe via windres .rc file.
#define IDI_ICON1 101

// Baked-in game name written to mbm_game_name.cpp at configure time by CMake.
extern const char mbm_baked_game_name[];

#ifndef GAME_APP_TITLE
    #define GAME_APP_TITLE mbm_baked_game_name
#endif

#ifndef GAME_ASSET_PASSWORD
    #define GAME_ASSET_PASSWORD ""
#endif

static std::string temporary_folder_path;
static std::string s_save_dir;
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

// On Windows, create a unique subdirectory under %LOCALAPPDATA%\Temp.
// That location is user-private (not world-readable like C:\Windows\Temp).
// Falls back to the system GetTempPath if LOCALAPPDATA is not set.
static std::string make_delivery_temp_dir(const char *app_name)
{
    // Build a safe name prefix from the app title
    std::string safe;
    for (const char *p = app_name; p && *p; ++p)
        safe += (isalnum(static_cast<unsigned char>(*p)) ? *p : '_');
    if (safe.empty()) safe = "game";

    // Choose base: %LOCALAPPDATA%\Temp  (user-private) or system temp
    char base[MAX_PATH] = "";
    const char *local_app = getenv("LOCALAPPDATA");
    if (local_app && local_app[0])
        snprintf(base, sizeof(base), "%s\\Temp", local_app);
    else
        GetTempPathA(sizeof(base), base);

    // Ensure base exists
    CreateDirectoryA(base, nullptr);

    // Generate a unique subdirectory name from tick count + PID (no extra lib needed)
    char unique_str[64] = "";
    snprintf(unique_str, sizeof(unique_str), "%I64X_%lX",
        static_cast<unsigned long long>(GetTickCount64()),
        static_cast<unsigned long>(GetCurrentProcessId()));

    char result[MAX_PATH] = "";
    snprintf(result, sizeof(result), "%s\\%s_%s", base, safe.c_str(), unique_str);
    return result;
}

// Returns (and creates) %APPDATA%\<AppName> as the persistent save-data location.
static std::string compute_save_dir(const std::string &app_name)
{
    const char *appdata = getenv("APPDATA");
    std::string dir = std::string(appdata ? appdata : ".") + "\\" + app_name;
    CreateDirectoryA(dir.c_str(), nullptr);  // create if absent (EEXIST is fine)
    return dir;
}

void onDoNativeCommand(const char *command, const char * /*param*/, char *result, const int max_size_result)
{
    if (!command) return;
    if (strcmp(command, "get_tmp_folder") == 0)
    {
        if (!temporary_folder_path.empty())
            strncpy(result, temporary_folder_path.c_str(), static_cast<size_t>(max_size_result) - 1);
    }
    else if (strcmp(command, "get_save_dir") == 0)
    {
        if (!s_save_dir.empty())
            strncpy(result, s_save_dir.c_str(), static_cast<size_t>(max_size_result) - 1);
    }
}

/* Returns directory of the running executable (no trailing backslash). */
static std::string get_binary_dir()
{
    char path[MAX_PATH] = "";
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    char *sep = strrchr(path, '\\');
    if (sep) *sep = '\0';
    return path;
}

int main(int /*argc*/, const char ** /*argv*/)
{
    mbm::set_callback_do_commands(onDoNativeCommand);
    mbm::set_app_name(title_app.c_str());
    mbm::set_window_size(1920, 1080);
    mbm::set_expected_window_size(1920, 1080);
    mbm::set_verbose(false);
    mbm::disable_splash();
    mbm::set_icon(IDI_ICON1);

    // Compute save dir once; expose to Lua via get_save_dir command.
    s_save_dir = compute_save_dir(title_app);

    /* ------------------------------------------------------------------ */
    /* Resolve .asset path: <GameDir>\assets\<file>                        */
    /* ------------------------------------------------------------------ */
    const char *asset_file = GAME_ASSET_FILE;
    const char *password   = GAME_ASSET_PASSWORD;

    const std::string bin_dir   = get_binary_dir();
    const std::string asset_path = bin_dir + "\\assets\\" + asset_file;

    /* ------------------------------------------------------------------ */
    /* Extract into a private temporary folder                             */
    /* ------------------------------------------------------------------ */
    // Create a user-private temporary directory under %LOCALAPPDATA%\Temp.
    const std::string temp_dir_path = make_delivery_temp_dir(title_app.c_str());
    char temp_folder[MAX_PATH] = "";
    strncpy(temp_folder, temp_dir_path.c_str(), sizeof(temp_folder) - 1);
    if (!CreateDirectoryA(temp_folder, nullptr))
    {
        fprintf(stderr, "[delivery] Failed to create temporary folder\n");
        return 1;
    }

    s_cleanup_folder      = temp_folder;
    temporary_folder_path = temp_folder;
    std::atexit(cleanup_temp_folder);

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

    /* ------------------------------------------------------------------ */
    /* Show the monitor / resolution / fullscreen picker                   */
    /* ------------------------------------------------------------------ */
    if (!mbm::select_app_and_resolution(
            nullptr, 0, nullptr,   // no app list — single game
            nullptr, 0,            // use built-in resolution list
            true,  true,           // allow fullscreen; default to checked
            1920,  1080))          // suggest 1920 × 1080 for windowed
        return 0;  // user dismissed the launcher

    const int ret = mbm::onLoop();
    // cleanup_temp_folder() called automatically via std::atexit.
    return ret;
}
