/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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


#if defined(USE_OPENGL_ES)
#if defined(_WIN32) || defined(__MINGW32__)

#include <core-manager.h>
#include <device.h>
#include <scene.h>

namespace mbm
{
    void CORE_MANAGER::handleEventFromWindow()
    {
        this->device->window.doEvents();
        bool first_menu = true;
        while (mbm::WINDOW::isAnyMenuVisible() && device->window.run)
        {
            if (first_menu)
            {
                Sleep(50);
                mbm::WINDOW::refreshMenu();
            }
            this->device->window.doEvents();
            if (first_menu)
            {
                Sleep(50);
                mbm::WINDOW::refreshMenu();
            }
            first_menu = false;
        }
        if (this->device->window.run)
        {
            INFO_JOYSTICK_INIT_PLAYER info;
            while (this->popEvent(&info))
            {
                if (this->device->scene && this->__sceneWasInit)
                    this->device->scene->onInfoDeviceJoystick(info.player, info.maxNumberButton, info.deviceName.c_str(),
                        info.extraInfo.c_str());
            }
        }
    }

}
#endif // USE_OPENGL_ES
#endif //defined(_WIN32) || defined(__MINGW32__)