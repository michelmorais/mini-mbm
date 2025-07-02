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

#ifndef MY_SCENE_TEST_H
#define MY_SCENE_TEST_H

#include <core_mbm/scene.h>
#include <render/texture-view.h>
#include <core_mbm/core-manager.h>
#include <render/gif-view.h>


class MY_SCENE : public mbm::SCENE
{
  public:
    mbm::TEXTURE_VIEW * texBox;
    mbm::GIF_VIEW* gif;
    MY_SCENE();
    virtual ~MY_SCENE();
	void startLoading();
	void endLoading();
    void init() ;
    void logic();
    void onTouchDown(int key, float x, float y);
    void onTouchUp(int key, float x, float y);
    void onTouchMove(int, float x, float y);
    void onTouchZoom(float zoom);
    void onFinalizeScene();
    void onKeyDown(int key);
    void onKeyUp(int key);
    void onKeyDownJoystick(int player, int key);
    void onKeyUpJoystick(int player, int key);
    void onMoveJoystick(int player, float lx, float ly, float rx, float ry);
    void onInfoDeviceJoystick(int player, int maxNumberButton, const char * strDeviceName,const char * extraInfo);
    void onResizeWindow();
};

class GAME : public mbm::CORE_MANAGER
{
public:
    MY_SCENE myScene;
	bool existScene(const int idScene)override;
    GAME();
    virtual ~GAME();
};

#endif
