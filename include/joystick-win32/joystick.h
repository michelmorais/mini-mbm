/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <Windows.h>
#include <math.h>
#include <hidsdi.h>
#include <iostream>
#include <vector>
#include <io.h>
#include <plusWindows/plusWindows.h>

#define MAX_BUTTONS     128

#pragma comment(lib,"hid.lib")

using namespace std;

namespace mbm
{

class JOYSTICK
{
public:
    
    enum STATE_KEY
    {
        KEY_NONE,
        KEY_DOWN,
        KEY_UP
    };
    
    JOYSTICK();
    virtual ~JOYSTICK();
    virtual void onKeyDownJoystick(int player, int key)PURE;
    virtual void onKeyUpJoystick(int player, int key)PURE;
    virtual void onMoveJoystick(int player, float lx, float ly, float rx, float ry)PURE;
    virtual void onInfoDeviceJoystick(int player, int maxNumberButton, const char* strDeviceName, const char* extraInfo)PURE;
    bool initJoystick(mbm::WINDOW* win);

private:
    void parseRawInput(PRAWINPUT pRawInput);
    bool needChangeDevices(PRAWINPUTDEVICELIST pRawInputDeviceList, UINT nDevices);
    bool updateDevices();
    static int onParseRawInput(mbm::WINDOW* window, HRAWINPUT phRawInput);
    uint32_t getTotalDevices();

    struct INFO_LAST_MOVE
    {
        DWORD lAxisX;
        DWORD lAxisY;
        DWORD lAxisZ;
        DWORD lAxisRz;
        INFO_LAST_MOVE();
    };
    
    struct INFO_JOYSTICK
    {
        RAWINPUTDEVICELIST hDevice;
        BOOL bButtonStates[MAX_BUTTONS];
        STATE_KEY bStateKey[MAX_BUTTONS];
        LONG lAxisX;
        LONG lAxisY;
        LONG lAxisZ;
        LONG lAxisRz;
        LONG lHat;
        LONG wasMoveZero;
        INT  numberOfButtons;
        std::string name, extraInfo;
        bool info;
        INFO_LAST_MOVE infoLastMove;
        INFO_JOYSTICK();
    };
    std::vector<INFO_JOYSTICK*> lsInfoJoystick;
    bool _b_enableContinousMove;
    static int _indexJoystickInstance;
};

};
#endif
