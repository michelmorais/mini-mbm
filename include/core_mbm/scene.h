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

#include <memory>

#include "core-exports.h"

namespace mbm
{

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

    class SCENE : public CONTROL_SCENE
    {
      public:
        API_IMPL SCENE() noexcept;
        API_IMPL virtual ~SCENE();

        virtual void onInitScene() = 0;
        virtual void onLoop() = 0;
        virtual void startLoading() = 0;
        API_IMPL virtual void * get_lua_state();//if we are using lua we should be able to retrieve the current state
        virtual void endLoading() = 0;
        virtual void onResizeWindow() = 0;

        API_IMPL virtual const char *getSceneName() noexcept;

        API_IMPL virtual void onRestore(const int /*initRestore*/);
        API_IMPL virtual void onTouchDown(int, float, float);
        API_IMPL virtual void onTouchUp(int, float, float);
        API_IMPL virtual void onTouchMove(int, float, float);
        API_IMPL virtual void onTouchZoom(float);
        API_IMPL virtual void onFinalizeScene();
        API_IMPL virtual void onKeyDown(int);
        API_IMPL virtual void onKeyUp(int);
        API_IMPL virtual void onKeyDownJoystick(int, int);
        API_IMPL virtual void onKeyUpJoystick(int, int);
        API_IMPL virtual void onMoveJoystick(int, float, float, float,float);
        API_IMPL virtual void onInfoDeviceJoystick(int, int, const char *,const char *);
        API_IMPL virtual void onDoubleClick(float, float, int);
        API_IMPL virtual void onCallBackCommands(const char *,const char *);

        API_IMPL bool isEndScene() const noexcept;
        API_IMPL void setEndScene(bool value) noexcept;
        API_IMPL bool wasSceneUnloaded() const noexcept;
        API_IMPL void setWasUnloadedScene(bool value) noexcept;
        API_IMPL SCENE *getNextScene() const noexcept;
        API_IMPL void setNextScene(SCENE *_nextScene);
        API_IMPL bool shouldGoToNextScene() const noexcept;
        API_IMPL void setGoToNextScene(bool value) noexcept;
        API_IMPL void *getUserData() const noexcept;
        API_IMPL void setUserData(void *_userData) noexcept;

      private:
        struct Impl;
        struct ImplDeleter
        {
            void operator()(Impl *ptr) const;
        };
        std::unique_ptr<Impl, ImplDeleter> impl;
    };

}
#endif
