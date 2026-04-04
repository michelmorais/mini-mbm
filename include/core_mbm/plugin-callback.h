
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
	onSubscribe and onDestroy are called for each scene, onLoop and onRender are called every frame, the others are called when the event happens.
    The engine may enqueue scene destruction rather than executing it synchronously. This means:
    `onDestroy` of the **old** scene's plugin instance may fire **after** `onSubscribe` of the **new** scene's instance.
    Both instances may be alive simultaneously for a short window.

	If the plugin that you need in LUA does not need any of the events, you can just implement empty methods for those events.
	For example, if you only need onLoop, you might not need this interface at all, and just implement as normal library in LUA.
	See example as tiny_obj_loader, which is a pure Lua library that does not need to subscribe to the engine and does not implement PLUGIN interface. 
    It just provides a Lua function to parse OBJ files and returns the data as Lua tables.
	Other example is tilemap, which is a plugin that implements PLUGIN interface and subscribe to the engine. 
	It provides a Lua function to create a tile map editor, it has a internanal mbm::L_USER_TYPE::L_USER_TYPE_PLUGIN defined in class-identifier.h.

    

*/

class PLUGIN
{   //The methods on Plugin are the real order call from mbm engine
    public:
    PLUGIN                           () noexcept = default;
    virtual ~PLUGIN                  ()          = default;
    virtual void onSubscribe         (int width, int height, void * context, void * renderDevice) = 0; // width/height of window; context = platform window handle (e.g. HWND on Windows); renderDevice = graphics API device for active backend (e.g. IDirect3DDevice9* when USE_DIRECTX9, nullptr when USE_OPENGL_ES)
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
	virtual void onPrepare           () = 0;// called once before render loop in the very beggining of engine (after PollEvents of main core loop)
    virtual void onLoop              (float delta) = 0;
    virtual void onRender            () = 0 ;
    virtual void onDestroy           () = 0 ;
};

#endif