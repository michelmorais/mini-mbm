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
#if !defined(ANDROID)
#if defined(__linux__) || defined(__APPLE__)
#include <core-manager.h>
namespace mbm
{
    void CORE_MANAGER::initializeWindowx11()
    {
        XSelectInput(this->device->specificContextDevice->display_x11, this->device->specificContextDevice->window_x11,//ResizeRedirectMask ->resize (does not work properly on Linux)
            ResizeRedirectMask | (KeyPressMask | KeyReleaseMask) | (ButtonPressMask | ButtonReleaseMask) | (PointerMotionMask) /*| ExposureMask | StructureNotifyMask*/);
        XkbSetDetectableAutoRepeat(this->device->specificContextDevice->display_x11, true, nullptr);
        XMapWindow(this->device->specificContextDevice->display_x11, this->device->specificContextDevice->window_x11);
        XFlush(this->device->specificContextDevice->display_x11);

        XSizeHints xsize;
        xsize.flags = PMaxSize | PMinSize | USPosition; // only what we wish (for now not PMaxSize)
        xsize.min_width = static_cast<int>(device->backBufferWidth);
        xsize.min_height = static_cast<int>(device->backBufferHeight);
        xsize.max_width = static_cast<int>(device->backBufferWidth);
        xsize.max_height = static_cast<int>(device->backBufferHeight);
        xsize.base_width = static_cast<int>(device->backBufferWidth);
        xsize.base_height = static_cast<int>(device->backBufferHeight);
        xsize.width = static_cast<int>(device->backBufferWidth);
        xsize.height = static_cast<int>(device->backBufferHeight);
        xsize.width_inc = 0;
        xsize.height_inc = 0;
        xsize.x = 0;
        xsize.y = 0;
        XSetWMNormalHints(this->device->specificContextDevice->display_x11, this->device->specificContextDevice->window_x11, &xsize);
    }

    void CORE_MANAGER::handleEventFromWindow()
    {
        while (XPending(this->device->specificContextDevice->display_x11))
        {
            XEvent xevent;
            XNextEvent(this->device->specificContextDevice->display_x11, &xevent);
            switch (xevent.type)
            {
            case KeyPress:
            {
                auto key = static_cast<int>(XLookupKeysym(&xevent.xkey, 0));
                if (key >= 'a' && key <= 'z')
                    key = toupper(key);
                if (key == XK_Caps_Lock)
                    this->keyCapsLockState = ((xevent.xbutton.state & 2) == 0);// == 0 is on
                this->onKeyDown(key);
            }
            break;
            case KeyRelease:
            {
                auto key = static_cast<int>(XLookupKeysym(&xevent.xkey, 0));
                if (key >= 'a' && key <= 'z')
                    key = toupper(key);
                this->onKeyUp(key);
            }
            break;
            case ButtonPress:
            {
                switch (xevent.xbutton.button)
                {
                case Button1: this->onTouchDown(0, xevent.xbutton.x, xevent.xbutton.y); break;
                case Button2: this->onTouchDown(2, xevent.xbutton.x, xevent.xbutton.y); break;
                case Button3: this->onTouchDown(1, xevent.xbutton.x, xevent.xbutton.y); break;
                case 4: // zomm in
                    this->onTouchZoom(1.0f);
                    break;
                case 5: // zomm out
                    this->onTouchZoom(-1.0f);
                    break;
                }
            }
            break;
            case ButtonRelease:
            {
                switch (xevent.xbutton.button)
                {
                case Button1: this->onTouchUp(0, xevent.xbutton.x, xevent.xbutton.y); break;
                case Button2: this->onTouchUp(2, xevent.xbutton.x, xevent.xbutton.y); break;
                case Button3: this->onTouchUp(1, xevent.xbutton.x, xevent.xbutton.y); break;
                }
            }
            break;
            case MotionNotify:
            {
                this->onTouchMove(0, xevent.xmotion.x, xevent.xmotion.y);
            }
            break;
            case ResizeRequest:
            {
                XResizeRequestEvent xResize = xevent.xresizerequest;
                this->onResizeWindow(xResize.width, xResize.height);
            }
            break;
            default: {}
                   break;
            }
        }
    }
}

#endif // USE_OPENGL_ES
#endif //!ANDROID
#endif //(__linux__) || defined(__APPLE__)

