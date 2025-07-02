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

#ifndef CONTROL_SCENE_H
#define CONTROL_SCENE_H

#include "core-exports.h"

namespace mbm
{

    class DEVICE;
    
    class API_IMPL CONTROL_SCENE
    {
        friend class LOADING;
        friend class SCENE;
      private:
        static int idControl; 
        int        idScene;   
        CONTROL_SCENE()noexcept;
        static int getNextIdScene()noexcept;
      public:
        virtual ~CONTROL_SCENE()noexcept;
        int getIdScene() const noexcept;
    };

    class API_IMPL COMMON_DEVICE
    {
      public:
        COMMON_DEVICE();
        virtual ~COMMON_DEVICE() noexcept = default;
        static DEVICE *device;
    };

    class API_IMPL SCENE : public CONTROL_SCENE, public COMMON_DEVICE
    {
      public:
        bool   endScene;
        bool   wasUnloadedScene;
        SCENE *nextScene;
        bool   goToNextScene;
        void * userData;

        SCENE() noexcept;
        virtual ~SCENE() = default;

        virtual void init() = 0;
        virtual void logic() = 0;
        virtual void startLoading() = 0;
        virtual void * get_lua_state();//if we are using lua we should be able to retrieve the current state
        virtual void endLoading() = 0;
        virtual void onResizeWindow() = 0;

        virtual const char *getSceneName() noexcept;

        virtual void onRestore(const int /*initRestore*/);
        virtual void onTouchDown(int, float, float);
        virtual void onTouchUp(int, float, float);
        virtual void onTouchMove(int, float, float);
        virtual void onTouchZoom(float);
        virtual void onFinalizeScene();
        virtual void onKeyDown(int);
        virtual void onKeyUp(int);
        virtual void onKeyDownJoystick(int, int);
        virtual void onKeyUpJoystick(int, int);
        virtual void onMoveJoystick(int, float, float, float,float);
        virtual void onInfoDeviceJoystick(int, int, const char *,const char *);
        virtual void onDoubleClick(float, float, int);
        virtual void onCallBackCommands(const char *,const char *);

        void setNextScene(SCENE *_nextScene);
    };

}
#endif
