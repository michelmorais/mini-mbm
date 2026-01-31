/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#if (defined(__MINGW32__) || defined(__CYGWIN__) || defined(_WIN32))
#ifndef WIN32_PLATFORM_H
#define WIN32_PLATFORM_H

#include <joystick-win32/joystick-win32.h>
#include <plusWindows/plusWindows.h>
#include <core-manager.h>

namespace mbm
{

    class WIN_EVENT_BY_PASS : public mbm::EVENTS_WIN32
    {
    public:
        WIN_EVENT_BY_PASS() = delete;
        explicit WIN_EVENT_BY_PASS(mbm::EVENTS* the_parent) :parent(the_parent) {};
        ~WIN_EVENT_BY_PASS() = default;

        void onTouchDown(HWND w, int key, float x, float y) override;
        void onTouchUp(HWND w, int key, float x, float y)  override;
        void onTouchMove(HWND w, float x, float y)  override;
        void onTouchZoom(HWND w, float zoom)  override;
        void onKeyDown(HWND w, int key)  override;
        void onKeyUp(HWND w, int key)  override;
        void onDoubleClick(HWND w, float x, float y, int key)  override;
        void onResizeWindow(HWND w, int width, int height)  override;
        mbm::EVENTS* parent;
    };

    class WIN_JOYSTICK_BY_PASS : public mbm::JOYSTICK
    {
    public:
        WIN_JOYSTICK_BY_PASS() = delete;
        explicit WIN_JOYSTICK_BY_PASS(mbm::JOYSTICK_BASE* the_parent) :parent(the_parent) {};
        ~WIN_JOYSTICK_BY_PASS() = default;

        void onKeyDownJoystick(int player, int key) override;
        void onKeyUpJoystick(int player, int key) override;
        void onMoveJoystick(int player, float lx, float ly, float rx, float ry) override;
        void onInfoDeviceJoystick(int player, int maxNumberButton, const char* strDeviceName, const char* extraInfo) override;
        mbm::JOYSTICK_BASE* parent;
    };

}
#endif
#endif