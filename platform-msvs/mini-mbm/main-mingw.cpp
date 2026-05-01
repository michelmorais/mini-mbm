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

// MinGW / GCC Windows entry point — mirrors platform-msvs/mini-mbm/main.cpp
// but avoids MSVS-specific resource.h / icon IDs / #pragma comment.

#ifndef _WIN32
    #error "Target expected Windows"
#endif

#include <windows.h>
#include <mini-mbm-lib.h>
#include <ctime>
#include <cstdlib>
#include <cstring>
#include <cstdio>

void onDoNativeCommand(const char* command, const char* param, char* result, const int max_size_result)
{
    if (strcmp(command, "get_current_time") == 0)
    {
        time_t now = time(nullptr);
        tm* localTime = localtime(&now);
        if (param)
        {
            strftime(result, max_size_result, param, localTime);
        }
        else
        {
            strftime(result, max_size_result, "%A, %B %d, %Y %H:%M:%S", localTime);
        }
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

int main(const int argc, const char **argv)
{
    mbm::set_expected_window_size(1024, 768);
    mbm::set_window_size(1024, 768);
    mbm::set_callback_do_commands(onDoNativeCommand);
    return mbm::forward_args_and_do_loop(argc, argv, 0);
}
