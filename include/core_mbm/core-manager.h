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
#include <memory>
#include <cstdint>
#include "core-exports.h"
#include <core_mbm/joystick-base.h>

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
        ONRESIZEWINDOW,
        ONMOVEWINDOW // custom event for window move (not from engine, but from OS windowing system)
    };

    enum WHICH_FOR : char
    {
        WFOR_INITIAL,
        WFOR_2DS,
        WFOR_2DW,
        WFOR_3D,
        WFOR_DONE
    };

    enum STEP_RETORE : char
    {
        STEP_RES_INIT_GL,
        STEP_RES_DRAW_HOURGLASS,
        STEP_RES_OBJ,
        STEP_RES_END,
    };

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
        union {
            int key;
            int width;
        };
        union {
            int player;
            int height;
        };
        float              rx, ry;
        EVENT_TYPE_ACTIONS eventType;
        constexpr EVENT_KEY() noexcept : x(0), y(0), key(0), player(0), rx(0), ry(0), eventType(UNKNOWN)
        {}

        constexpr EVENT_KEY(const float _x, const float _y, const int _key, const EVENT_TYPE_ACTIONS _eventName) noexcept
            : x(_x),
                y(_y),
                key(_key),
                player(0),
                rx(0.0f),
                ry(0.0f),
                eventType(_eventName)
        {}

        constexpr EVENT_KEY(const float _x, const float _y, const int _width, const int _height, const EVENT_TYPE_ACTIONS _eventName) noexcept
            : x(_x),
                y(_y),
                width(_width),
                height(_height),
                rx(0.0f),
                ry(0.0f),
                eventType(_eventName)
        {}

        constexpr EVENT_KEY(const float _lx, const float _ly, const int _key, const int _player, const float _rx,
                            const float _ry, const EVENT_TYPE_ACTIONS _eventName) noexcept : lx(_lx),
                                                                                                ly(_ly),
                                                                                                key(_key),
                                                                                                player(_player),
                                                                                                rx(_rx),
                                                                                                ry(_ry),
                                                                                                eventType(_eventName)
        {}
        
    };

    struct INFO_JOYSTICK_INIT_PLAYER
    {
        int         player;
        int         maxNumberButton;
        std::string deviceName;
        std::string extraInfo;

        INFO_JOYSTICK_INIT_PLAYER() noexcept : player(0), maxNumberButton(0)
        {
        }

        INFO_JOYSTICK_INIT_PLAYER(const int _player, const int _maxNumberButton, const char* _deviceName,
            const char* _extraInfo) noexcept
            : player(_player), maxNumberButton(_maxNumberButton), deviceName(_deviceName), extraInfo(_extraInfo)
        {
        }

        INFO_JOYSTICK_INIT_PLAYER(int _player, int _maxNumberButton,
            std::string _deviceName, std::string _extraInfo) noexcept
            : player(_player), maxNumberButton(_maxNumberButton),
            deviceName(std::move(_deviceName)), extraInfo(std::move(_extraInfo))
        {
        }

        // Copy constructor
        INFO_JOYSTICK_INIT_PLAYER(const INFO_JOYSTICK_INIT_PLAYER&) = default;

        // Copy assignment
        INFO_JOYSTICK_INIT_PLAYER& operator=(const INFO_JOYSTICK_INIT_PLAYER&) = default;

        // Move constructor
        INFO_JOYSTICK_INIT_PLAYER(INFO_JOYSTICK_INIT_PLAYER&&) noexcept = default;

        // Move assignment
        INFO_JOYSTICK_INIT_PLAYER& operator=(INFO_JOYSTICK_INIT_PLAYER&&) noexcept = default;

        // Destructor
        ~INFO_JOYSTICK_INIT_PLAYER() = default;

    };

    // EVENTS inherits JOYSTICK_BASE so that CORE_MANAGER : public EVENTS has
    // a single linear vtable chain.  Without this, JOYSTICK_BASE would be a
    // secondary base of CORE_MANAGER, forcing GCC to emit non-virtual thunks
    // that it cannot generate in consumer TUs when API_IMPL == dllimport.
    class API_IMPL EVENTS : public JOYSTICK_BASE
    {
      public:
        virtual void onTouchDown(int key, float x, float y) = 0;
        virtual void onTouchUp(int key, float x, float y) = 0;
        virtual void onTouchMove(int key, float x, float y) = 0;
        virtual void onTouchZoom(float zoom) = 0;
        virtual void onKeyDown(int key) = 0;
        virtual void onKeyUp(int key) = 0;
        virtual void onDoubleClick(float x, float y, int key) = 0;
        // joystick methods inherited from JOYSTICK_BASE (no re-declaration needed)
        virtual void onResizeWindow(int width, int height) = 0;
        virtual void onMoveWindow(int x, int y) = 0;
    };

    class CORE_MANAGER : public EVENTS
    {
      public:
        DEVICE *device;
        bool    changeScene;
        API_IMPL CORE_MANAGER();
        API_IMPL virtual ~CORE_MANAGER();
    
        API_IMPL void setScene(SCENE *currentScene);
        API_IMPL virtual bool existScene(const int idScene) = 0;
        API_IMPL void onStopCoreManager();
        API_IMPL unsigned int addPlugin(PLUGIN * plugin);
        API_IMPL void setMinMaxSizeWindow(int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y);
        API_IMPL void setUsageOfDefaultPS_VS_WhenNoShader(const bool _useDeafultPSwhenNoPsShader, const bool _useDeafultVSwhenNoVSShader) noexcept; // This is workaround where  (false, false) the engine does not use default shaders when no shader is set in the objects (so, no shader is used, mostlly in directx)

        API_IMPL void execute_system_cmd_thread(const char* command);//execute system command in other thread
        API_IMPL bool onLostDevice(const bool doSwapBuffers, int width, int height,const int px,const int py);
        API_IMPL bool initGraphics(const char *nameApplication = "Mini-mbm", int width = 800, int height = 600, const int px = 0, const int py = 0, const bool border = true,const bool enable_resize = true);

        API_IMPL int onLoop(const bool singleLoop, const bool doSwapBuffers);
    

        API_IMPL void getScreenSize(int *width,int *height);
    
      private:
        API_IMPL void update();
        API_IMPL bool renderToTargets();
        API_IMPL static void prepareRender2d(std::vector<RENDERIZABLE *> &lsAllObjects2d,std::vector<RENDERIZABLE *> &lsRenderOnFrustum2d);
        API_IMPL static void prepareRender3d(std::vector<RENDERIZABLE *> &lsAllObjects3d,std::vector<RENDERIZABLE *> &lsRenderOnFrustum3d);
        API_IMPL void render();
        API_IMPL void ReleaseGraphics(const bool wasDeviceLost);//this function release the graphics device and all resources
        API_IMPL void forceRestore(const bool doSwapBuffers);
    
      private:
        void handleEventFromWindow();
        void _updateDimFrustum();
        void adjustScaleScreen2d();
        void updateAudio();
        void updatePhysis();
        void initEnableRenders();
        void logic();
        void reinitTimers();
        bool beginRender(); // prepare to render
        void endRender(); // end render
        void swapBuffers(); // Swap buffers
        bool resetDeviceWithNewDimensions(int newWidth, int newHeight);// need to be implemented in each backend engine
        void enableRender(const int idScene);
        void disableRender(const int idScene);
        void pushEvent(EVENT_KEY *event);
        bool popEvent(EVENT_KEY *event);
        void pushEvent(INFO_JOYSTICK_INIT_PLAYER *info);
        bool popEvent(INFO_JOYSTICK_INIT_PLAYER *info);
        void moveWindow(int x, int y);//specific function to handle window move event depending on OS windowing system
        void initializeImpl();
        uint32_t getTotalPlugins() const noexcept;
        PLUGIN *getPlugin(const uint32_t index) const noexcept;
        unsigned int appendPlugin(PLUGIN *plugin);
        void clearPlugins();

      public:
        API_IMPL void onTouchDown(int key, float x, float y) override;
        API_IMPL void onTouchUp(int key, float x, float y) override;
        API_IMPL void onTouchMove(int key, float x, float y) override;
        API_IMPL void onTouchZoom(float zoom) override;
        API_IMPL void onKeyDown(int key) override;
        API_IMPL void onKeyUp(int key) override;
        API_IMPL void onDoubleClick(float x, float y, int key) override;
        API_IMPL void onKeyDownJoystick(int player, int key) override;
        API_IMPL void onKeyUpJoystick(int player, int key) override;
        API_IMPL void onMoveJoystick(int player, float lx, float ly, float rx, float ry) override;
        API_IMPL void onInfoDeviceJoystick(int player, int maxNumberButton, const char* strDeviceName, const char* extraInfo) override;
        API_IMPL void onResizeWindow(int width, int height) override;
        API_IMPL void onMoveWindow(int width, int height) override;

      public:
        bool __sceneWasInit;
        bool keyCapsLockState;
        bool windowBorder;
        bool enableResizeWindow;
        STEP_RETORE getStepRestore() const noexcept { return stepRestore; }
      private:
        bool                                    wasGamePausedBeforeOnStop;
        bool                                    loopVariablesInitialized;
        std::string  nameApplication;
        WHICH_FOR    which_for;
        STEP_RETORE  stepRestore;
        uint32_t     indexOnRestore;
        uint32_t     totalForByLoop;
        float        stepRestoreInfo;
        float        percentRestoreInfo;
        struct Impl;
        struct ImplDeleter
        {
            void operator()(Impl *ptr) const;
        };
        std::unique_ptr<Impl, ImplDeleter> impl;
    };

    
}

#endif
