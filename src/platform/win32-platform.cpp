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
|-----------------------------------------------------------------------------------------------------------------------*/


#if (defined (__MINGW32__) || defined (__CYGWIN__) || defined(_WIN32))

#include <platform/win32-platform.h>
#include <core_mbm/device.h>

#if defined (USE_DIRECTX9)
    #include <specific-directx9.h>
#elif defined (USE_OPENGL_ES)
    #include <specific-opengl_es.h>
#endif


namespace mbm
{
    void WIN_EVENT_BY_PASS::onTouchDown(HWND, int key, float x, float y)
    {
        if (parent)
            parent->onTouchDown(key, x, y);
    }
    void WIN_EVENT_BY_PASS::onTouchUp(HWND, int key, float x, float y)
    {
        if (parent)
            parent->onTouchUp(key, x, y);
    }
    void WIN_EVENT_BY_PASS::onTouchMove(HWND, float x, float y)
    {
        if (parent)
            parent->onTouchMove(0, x, y);
    }
    void WIN_EVENT_BY_PASS::onTouchZoom(HWND, float zoom)
    {
        if (parent)
            parent->onTouchZoom(zoom);
    }
    void WIN_EVENT_BY_PASS::onKeyDown(HWND, int key)
    {
        if (parent)
            parent->onKeyDown(key);
    }
    void WIN_EVENT_BY_PASS::onKeyUp(HWND, int key)
    {
        if (parent)
            parent->onKeyUp(key);
    }
    void WIN_EVENT_BY_PASS::onDoubleClick(HWND, float x, float y, int key)
    {
        if (parent)
            parent->onDoubleClick(x, y, key);
    }
    void WIN_EVENT_BY_PASS::onResizeWindow(HWND, int width, int height)
    {
        if (parent)
            parent->onResizeWindow(width, height);
    }


    void WIN_JOYSTICK_BY_PASS::onKeyDownJoystick(int player, int key)
    {
        if (parent)
            parent->onKeyDownJoystick(player, key);
    }
    void WIN_JOYSTICK_BY_PASS::onKeyUpJoystick(int player, int key)
    {
        if (parent)
            parent->onKeyUpJoystick(player, key);
    }
    void WIN_JOYSTICK_BY_PASS::onMoveJoystick(int player, float lx, float ly, float rx, float ry)
    {
        if (parent)
            parent->onMoveJoystick(player, lx, ly, rx, ry);
    }
    void WIN_JOYSTICK_BY_PASS::onInfoDeviceJoystick(int player, int maxNumberButton, const char* strDeviceName, const char* extraInfo)
    {
        if (parent)
            parent->onInfoDeviceJoystick(player, maxNumberButton, strDeviceName, extraInfo);
    }

    void setWin32IconToBeUsed(const int ID_ICON)
    {
        DEVICE* device = DEVICE::getInstance();
        device->specificContextDevice->idIcon = ID_ICON;
    }

    const char* selectFolderDialog(char* folderPathOut)
    {
        HWND hwnd = mbm::DEVICE::getInstance()->specificContextDevice->window.getHwnd();
        const char* path = mbm::selectetDirectory(hwnd, folderPathOut);
        return path;
    }
};

#endif