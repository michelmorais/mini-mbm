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

#ifndef CORE_MANAGER_GLES_H
#define CORE_MANAGER_GLES_H

#include <vector>
#include <string>
#include <map>
#include <list>
#include <mutex>
#include "core-exports.h"

#if defined _WIN32
    #include <joystick-win32/joystick.h>
    #include <EGL/egl.h>
//    #include <../third-party/gles/util/EGLWindow.h>
#elif defined ANDROID
    #include <jni.h>
#elif defined __linux__ && !defined ANDROID
    #include <X11/Xlib.h>
    #include <EGL/egl.h>
#endif
#include <GLES2/gl2.h>


class PLUGIN;

namespace mbm
{
    class DEVICE;
    class SCENE;
    class RENDERIZABLE;

    
    enum EVENT_TYPE_ACTIONS
    {
        UNKNOWN,
        ONTOUCHDOWN,
        ONTOUCHUP,
        ONTOUCHMOVE,
        ONTOUCHZOOM,
        ONKEYDOWN,
        ONKEYUP,
        ONKEYDOWNJOYSTICK,
        ONKEYUPJOYSTICK,
        ONMOVEJOYSTICK,
        ONDOUBLECLICK,
        ONSTREAMSTOPED,
        ONCALLBACKCOMMANDS,
        ONRESIZEWINDOW
    };

    enum WHICH_FOR : char;

    enum STEP_RETORE : char;

    struct API_IMPL EVENT_KEY
    {
      public:
        union {
            float x;
            float lx;
        };
        union {
            float y;
            float ly;
        };
        int                key;
        int                player;
        float              rx, ry;
        EVENT_TYPE_ACTIONS eventType;
        constexpr EVENT_KEY() noexcept;
        constexpr EVENT_KEY(const float _x, const float _y, const int _key, const EVENT_TYPE_ACTIONS _eventName) noexcept;
        constexpr EVENT_KEY(const float _lx, const float _ly, const int _key, const int _player, const float _rx,
                            const float _ry, const EVENT_TYPE_ACTIONS _eventName) noexcept; 
        
    };

    struct INFO_JOYSTICK_INIT_PLAYER
    {
        int         player;
        int         maxNumberButton;
        std::string deviceName;
        std::string extraInfo;
        INFO_JOYSTICK_INIT_PLAYER();
        INFO_JOYSTICK_INIT_PLAYER(const int _player, const int _maxNumberButton, const char *_deviceName,
                                  const char *_extraInfo);
    };

    #if defined(ANDROID) || defined(__linux__)


    class API_IMPL EVENTS
    {
      public:
        EVENTS() noexcept;
        virtual ~EVENTS();
        virtual void onTouchDown(int key, float x, float y) = 0;
        virtual void onTouchUp(int key, float x, float y) = 0;
        virtual void onTouchMove(int key, float x, float y) = 0;
        virtual void onTouchZoom(float zoom) = 0;
        virtual void onKeyDown(int key) = 0;
        virtual void onKeyUp(int key) = 0;
        virtual void onDoubleClick(float x, float y, int key) = 0;
        virtual void onKeyDownJoystick(int, int) = 0; // parameter: int player, int key
        virtual void onKeyUpJoystick(int, int) = 0; // parameter: int player, int key
        virtual void onMoveJoystick(int, float, float, float,float) = 0; // parameter: int player, float lx, float ly, float rx, float ry
        virtual void onInfoDeviceJoystick(int, int, const char *,const char *) = 0; // parameter: int player, int maxNumberButton, const char* strDeviceName, const char* extraInfo
    };
    #endif

    #if defined(ANDROID) || defined(__linux__)
    class CORE_MANAGER : public EVENTS
    #else
    class CORE_MANAGER : public EVENTS, public JOYSTICK
    #endif
    {
      public:
        DEVICE *device;
        bool    changeScene;
        API_IMPL CORE_MANAGER();
        API_IMPL virtual ~CORE_MANAGER();
    
        API_IMPL void setScene(SCENE *currentScene);
		API_IMPL virtual bool existScene(const int idScene) = 0;
        API_IMPL void onStop();
        API_IMPL unsigned int addPlugin(PLUGIN * plugin);
    #if defined USE_EDITOR_FEATURES && !defined ANDROID
        API_IMPL void execute_system_cmd_thread(const char* command);//execute system command in other thread
    #endif
    #if defined ANDROID
        API_IMPL bool onLostDevice(JNIEnv *jenv, jobject jobj, int width, int height);
    #else
        API_IMPL bool onLostDevice(int width, int height,const int px,const int py);
    #endif
    #if defined(__linux__) && !defined(ANDROID)
        Window     win;
        EGLSurface egl_surf;
        EGLContext egl_ctx;
        EGLDisplay egl_dpy;
        Display *  display;

        API_IMPL static void make_x_window(Display *display, EGLDisplay egl_dpy, const char *name, int x, int y,uint32_t width,uint32_t height,
                                  Window *winRet, EGLContext *ctxRet, EGLSurface *surfRet,bool border);
    #endif

    #if defined(_WIN32)
        EGLDisplay eglDisplay;
        EGLSurface eglSurface;
        EGLContext eglContext;
        //std::unique_ptr<EGLDisplay> eglDisplay;
        //std::unique_ptr<EGLSurface> eglSurface;
        API_IMPL bool initGl(const char *nameAplication = "Mini-mbm", int width = 800, int height = 600, const int px = 0, const int py = 0, const bool border = true,const bool enable_resize = true);
    #elif defined (ANDROID)
        API_IMPL bool initGl(const int width = 800, const int height = 600);
    #elif defined __linux__
        API_IMPL bool initGl(const char *nameAplication = "Mini-mbm", int width = 800, int height = 600, const bool border = true);
    #else
        #error "undefined platform"
        API_IMPL bool initGl();
    #endif

    #ifdef ANDROID
        API_IMPL int loop(JNIEnv *, jobject);
    #elif (defined(_WIN32) || defined(__MINGW32__) || defined(__linux__)) && !defined(ANDROID)
        API_IMPL int loop();
    #else
    #error "platform not suported!"
    #endif

    #if defined(__linux__) && !defined(ANDROID)
        API_IMPL void getScreenSize(int *width,int *height);
    #endif
    
      private:
        API_IMPL void update();
        API_IMPL bool renderToTargets();
        API_IMPL static void prepareRender2d(std::vector<RENDERIZABLE *> &lsAllObjects2d,std::vector<RENDERIZABLE *> &lsRenderOnFrustum2d);
        API_IMPL static void prepareRender3d(std::vector<RENDERIZABLE *> &lsAllObjects3d,std::vector<RENDERIZABLE *> &lsRenderOnFrustum3d);
        API_IMPL void render();
    
      private:
        void _updateDimFrustum();
        void adjustScaleScreen2d();
        void updateAudio();
        void updatePhysis();
        void initEnableRenders();
        void logic();
        void reinitTimers();
        void enableRender(const int idScene);
        void disableRender(const int idScene);
        void pushEvent(EVENT_KEY *event);
        bool popEvent(EVENT_KEY *event);
        void pushEvent(INFO_JOYSTICK_INIT_PLAYER *info);
        bool popEvent(INFO_JOYSTICK_INIT_PLAYER *info);
    #if defined(ANDROID)
      public:
        API_IMPL void onTouchDown(int key, float x, float y);
        API_IMPL void onTouchUp(int key, float x, float y);
        API_IMPL void onTouchMove(int key, float x, float y);
        API_IMPL void onTouchZoom(float zoom);
        API_IMPL void onKeyDown(int key);
        API_IMPL void onKeyUp(int key);
        API_IMPL void onDoubleClick(float x, float y, int key);
        API_IMPL void onKeyDownJoystick(int player, int key);
        API_IMPL void onKeyUpJoystick(int player, int key);
        API_IMPL void onMoveJoystick(int player, float lx, float ly, float rx, float ry);
        API_IMPL void onInfoDeviceJoystick(int player, int maxNumberButton, const char *strDeviceName, const char *extraInfo);
        API_IMPL void onResizeWindow(int width, int height);
    
    #elif defined __linux__
      public:
        API_IMPL void onTouchDown(int key, float x, float y);
        API_IMPL void onTouchUp(int key, float x, float y);
        API_IMPL void onTouchMove(int key, float x, float y);
        API_IMPL void onTouchZoom(float zoom);
        API_IMPL void onKeyDown(int key);
        API_IMPL void onKeyUp(int key);
        API_IMPL void onDoubleClick(float x, float y, int key);
        API_IMPL void onKeyDownJoystick(int player, int key);
        API_IMPL void onKeyUpJoystick(int player, int key);
        API_IMPL void onMoveJoystick(int player, float lx, float ly, float rx, float ry);
        API_IMPL void onInfoDeviceJoystick(int player, int maxNumberButton, const char *strDeviceName, const char *extraInfo);
        API_IMPL void onResizeWindow(int width, int height);
    #elif defined _WIN32
        API_IMPL void onTouchDown(HWND, int key, float x, float y);
        API_IMPL void onTouchUp(HWND, int key, float x, float y);
        API_IMPL void onTouchMove(HWND, float x, float y);
        API_IMPL void onTouchZoom(HWND, float zoom);
        API_IMPL void onKeyDown(HWND, int key);
        API_IMPL void onKeyUp(HWND,int key);
        API_IMPL void onDoubleClick(HWND, float x, float y, int key);
        API_IMPL void onKeyDownJoystick(int player, int key);
        API_IMPL void onKeyUpJoystick(int player, int key);
        API_IMPL void onMoveJoystick(int player, float lx, float ly, float rx, float ry);
        API_IMPL void onInfoDeviceJoystick(int player, int maxNumberButton, const char *strDeviceName, const char *extraInfo);
        API_IMPL void onResizeWindow(HWND w, int width, int height);
        
    #endif
      public:
        bool __sceneWasInit;
        API_IMPL void forceRestore();
        bool keyCapsLockState;
    #if defined _WIN32
        DWORD idIcon;
    #endif
      private:
        std::map<int, bool>                     __keyPressed;
        std::list<EVENT_KEY>                    lsEvents;
        std::list<INFO_JOYSTICK_INIT_PLAYER>    lsInfoJoystick;
        std::vector<PLUGIN*>                    lsPlugins;
    #if defined _WIN32
        std::mutex mutexEvents;
    #endif
        EVENT_KEY lastEvent;
        WHICH_FOR which_for;
        STEP_RETORE  stepRestore;
        uint32_t indexOnRestore;
        uint32_t totalForByLoop;
        float stepRestoreInfo;
        float percentRestoreInfo;
    };
}

#endif
