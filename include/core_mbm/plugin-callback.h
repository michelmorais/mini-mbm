
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
    PLUGIN — engine callback interface for shared-library plugins.

    WHEN TO USE THIS INTERFACE
    --------------------------
    Inherit from PLUGIN when your plugin needs any of the following:
      - Per-frame updates (onLoop / onRender)
      - Input events (touch, keyboard, joystick)
      - Access to the graphics backend (renderDevice in onSubscribe)
      - Per-scene init/teardown lifecycle (onSubscribe / onDestroy)

    If the plugin only exposes stateless utility functions to Lua (parsing,
    encoding, math), do NOT inherit from PLUGIN. Instead register a luaL_Reg
    table and return it from luaopen_ via luaL_newlib. The caller then does:
        local mp = require "myplugin"   -- mp is the table
    See modules/obj_importer_lua/ (tiny_obj_loader) for this pattern.

    CALL ORDER
    ----------
    require "myplugin"  ->  luaopen_myplugin()  ->  onSubscribe()
    every frame         ->  onLoop(delta)  ->  onRender()
    scene ends          ->  onDestroy()
    next scene          ->  luaopen_myplugin() again from scratch  ->  onSubscribe()

    DEFERRED DESTRUCTION RACE
    -------------------------
    The engine may enqueue scene destruction rather than executing it
    synchronously. This means onDestroy() of the OLD scene's plugin instance
    may fire AFTER onSubscribe() of the NEW scene's instance. Both instances
    can be alive simultaneously for a short window. Never rely on onDestroy
    being called before the next onSubscribe.

    PROCESS-LIFETIME vs SCENE-LIFETIME RESOURCES
    ---------------------------------------------
    Some external libraries (e.g. Steam, a GPU context, a network layer) must
    be initialized once per process and must NOT be shut down between scenes.
    Guard them with a process-global bool flag:

        bool g_myLibReady = false;

        void onSubscribe(...) override {
            if (!g_myLibReady) {
                if (MyLib_Init() == OK) {
                    g_myLibReady = true;
                    std::atexit([] { MyLib_Shutdown(); });
                }
            }
            m_bReady = g_myLibReady;
        }

        void onDestroy() override {
            m_bReady = false;           // do NOT call MyLib_Shutdown() here
            m_pendingRequests.clear();  // release only per-scene state
        }

    See plugins/steam/steam-impl.h for a real-world example of this pattern.

    TILEMAP EXAMPLE
    ---------------
    plugins/tiled/ implements PLUGIN and subscribes to the engine.
    It exposes a Lua tile-map editor and uses the internal userdata type
    mbm::L_USER_TYPE::L_USER_TYPE_PLUGIN defined in class-identifier.h.
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