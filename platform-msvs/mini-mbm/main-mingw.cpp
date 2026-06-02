/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                            |
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

// MinGW / GCC Windows entry point — mirrors platform-msvs/mini-mbm-launcher/mini-mbm-launcher.cpp
// but avoids MSVS-specific resource.h / #pragma comment.

#ifndef _WIN32
    #error "Target expected Windows"
#endif

#include <windows.h>
#include <mini-mbm-lib.h>
#include <core_mbm/parse-launcher-args.hpp>
#include <core_mbm/util-interface.h>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <string>

// Numeric resource ID for the icon compiled into the exe via windres .rc file.
// The CMake-generated .rc contains: 101 ICON "path/to/icon.ico"
#define IDI_ICON1 101

// Baked-in game name written to mbm_game_name.cpp at configure time by CMake.
extern const char mbm_baked_game_name[];

std::string title_app(mbm_baked_game_name);
std::string temporary_folder_path;

// From LUA Use: doCommands(string command, string parameter)
// e.g.: local tmp_folder = mbm.doCommands('get_tmp_folder')
void onDoNativeCommand(const char* command, const char* param, char* result, const int max_size_result)
{
    if (command)
    {
        if (strcmp(command, "get_tmp_folder") == 0)
        {
            if (temporary_folder_path.empty())
            {
                char temp_folder[1024] = "";
                if (title_app.size() > 0 && mbm::create_temp_folder(title_app.c_str(), temp_folder, sizeof(temp_folder)))
                    temporary_folder_path = temp_folder;
                else if (mbm::create_temp_folder(nullptr, temp_folder, sizeof(temp_folder)))
                    temporary_folder_path = temp_folder;
            }
            if (!temporary_folder_path.empty())
                strncpy(result, temporary_folder_path.c_str(), max_size_result - 1);
        }
        else if (strcmp(command, "get_current_time") == 0)
        {
            time_t now = time(nullptr);
            tm* localTime = localtime(&now);
            if (param)
                strftime(result, max_size_result, param, localTime);
            else
                strftime(result, max_size_result, "%A, %B %d, %Y %H:%M:%S", localTime);
        }
        else if (strcmp(command, "get_random_number") == 0)
        {
            const int random_number = rand() % 100;
            snprintf(result, max_size_result, "%d", random_number);
        }
#if defined _DEBUG
        else if (strcmp(command, "restoreDeviceTest") == 0)
        {
            mbm::restoreDeviceTest();
        }
#endif
        else
        {
            snprintf(result, max_size_result, "Unknown command: %s", command);
        }
    }
}

int main(const int /*argc*/, const char** /*argv*/)
{
    bool allowFullScreen       = true;
    bool full_screen_checked   = true;
    bool disable_select_monitor = false;

    mbm::set_callback_do_commands(onDoNativeCommand);

    {
        PARSE_launcher_ARGS parser;

        util::addPath("assets/");
        unsigned int width = 0, height = 0;
        if (parser.getWidthHeight(width, height))
            mbm::set_window_size(static_cast<int>(width), static_cast<int>(height));
        else
            mbm::set_window_size(1920, 1080);

        unsigned int expected_width = 0, expected_height = 0;
        if (parser.getExpectedWidthHeight(expected_width, expected_height))
            mbm::set_expected_window_size(static_cast<int>(expected_width), static_cast<int>(expected_height));
        else
            mbm::set_expected_window_size(1920, 1080);

        mbm::set_verbose(false);

        if (parser.noSplash)
            mbm::disable_splash();

        const char* nameApp = parser.getNameApplication();
        if (nameApp && strlen(nameApp) > 0)
        {
            title_app = nameApp;
            mbm::set_app_name(title_app.c_str());
        }
        else
        {
            mbm::set_app_name(title_app.c_str());
        }

        mbm::set_icon(IDI_ICON1);
        mbm::set_window_resizable(parser.enableResizeWindow);
        mbm::set_window_theme(parser.window_theme, parser.enableBorder);

        const char* fileNameInitialLua = parser.getFileNameInitialLua();
        if (fileNameInitialLua && strlen(fileNameInitialLua) > 0)
            mbm::set_scene(fileNameInitialLua);

        mbm::set_window_position(parser.positionXWindow, parser.positionYWindow);

        allowFullScreen        = parser.allowFullScreen;
        full_screen_checked    = parser.full_screen_checked;
        disable_select_monitor = parser.disable_select_monitor;
    }

    int ret = 0;
    if (disable_select_monitor)
    {
        ret = mbm::onLoop();
    }
    else if (mbm::select_resolution(nullptr, 0, allowFullScreen, full_screen_checked))
    {
        ret = mbm::onLoop();
    }

    if (!temporary_folder_path.empty())
        mbm::remove_folder(temporary_folder_path.c_str());

    return ret;
}
