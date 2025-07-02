
/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2020 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef PLUGIN_CALL_BACK_H
#define PLUGIN_CALL_BACK_H
/*
    The methods on Plugin callback are the real order call
*/

class PLUGIN
{   //The methods on Plugin are the real order call from mbm engine
    public:
    PLUGIN                           () noexcept = default;
    virtual ~PLUGIN                  ()          = default;
    virtual void onSubscribe         (int width, int height, void * context) = 0; //width and height of window. context will be the handle in Windows env.
    virtual void onResizeWindow      (int width, int height) = 0; //Resize window width and height of window. context will be the handle in Windows env.
    virtual void onTouchDown         (int key, float x, float y) = 0;// x and y are divided by camera.scale. If the plugin needs the real position it should multiply by the scale of camera2d.
    virtual void onTouchUp           (int key, float x, float y) = 0;// x and y are divided by camera.scale. If the plugin needs the real position it should multiply by the scale of camera2d.
    virtual void onTouchMove         (int key, float x, float y) = 0;// x and y are divided by camera.scale. If the plugin needs the real position it should multiply by the scale of camera2d.
    virtual void onTouchZoom         (float zoom) = 0;
    virtual void onKeyDown           (int key) = 0;
    virtual void onKeyUp             (int key) = 0;
    virtual void onDoubleClick       (float x, float y, int key) = 0;
    virtual void onKeyDownJoystick   (int player, int key) = 0;
    virtual void onKeyUpJoystick     (int player, int key) = 0;
    virtual void onMoveJoystick      (int player, float lx, float ly, float rx, float ry) = 0;
    virtual void onInfoDeviceJoystick(int player, int maxNumberButton, const char * strDeviceName, const char * extraInfo) = 0;
    virtual void onBeginRender       () = 0 ;
    virtual void onLoop              (float delta) = 0;
    virtual void onEndRender         () = 0 ;
    virtual void onDestroy           () = 0 ;
};

#endif