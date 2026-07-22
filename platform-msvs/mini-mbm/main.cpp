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

#ifndef _WIN32
	#error "Target expected Windows"
#endif

#pragma comment(lib, "core_mbm.lib")
#pragma comment(lib, "mini-mbm.lib")
#pragma comment(lib, "lua5.4.lib")

#include "resource.h"
#include <mini-mbm-lib.h>
#include <core_mbm/parse-launcher-args.hpp>
#include <ctime>
#include <string>

// Baked-in game name written to mbm_game_name.cpp at configure time by CMake.
extern const char mbm_baked_game_name[];

std::string title_app(mbm_baked_game_name);
std::string temporary_folder_path;

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
				strncpy_s(result, max_size_result, temporary_folder_path.c_str(), temporary_folder_path.size() - 1);
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
#if defined _DEBUG // testing proposal, you can call it from lua script, e.g. mbm.doCommands('restoreDeviceTest')
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
	bool allowFullScreen        = false;
	bool full_screen_checked    = true;
	bool disable_select_monitor = false;

	mbm::set_callback_do_commands(onDoNativeCommand);

	{
		PARSE_launcher_ARGS parser;

		unsigned int width = 0, height = 0;
		unsigned int requested_width = 0, requested_height = 0;
		if (parser.getWidthHeight(width, height))
		{
			requested_width  = width;
			requested_height = height;
			mbm::set_window_size(static_cast<int>(width), static_cast<int>(height));
		}
		else
			mbm::set_window_size(1920, 1080);

		unsigned int expected_width = 0, expected_height = 0;
		if (parser.getExpectedWidthHeight(expected_width, expected_height))
			mbm::set_expected_window_size(static_cast<int>(expected_width), static_cast<int>(expected_height));
		else
			// Default the "expected" (design) resolution to the actual requested window size so
			// camera.scaleScreen2d (device-common.cpp) comes out to 1.0 unless the caller
			// explicitly opts into design-resolution scaling via -ew/-eh. This used to hardcode
			// 1920x1080 here regardless of -w/-h -- see docs/future_investigation.md.
			mbm::set_expected_window_size(
				static_cast<int>(requested_width  > 0 ? requested_width  : 1920),
				static_cast<int>(requested_height > 0 ? requested_height : 1080));

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
