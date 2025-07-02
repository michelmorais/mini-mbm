
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


#include "scene-1.h"
#include <platform/common-jni.h>
#include <util-interface.h>

MY_SCENE::MY_SCENE()
{
    texBox  = nullptr;
}
MY_SCENE::~MY_SCENE()
{
    delete texBox;
}
void MY_SCENE::init() 
{
    this->device->camera.position = mbm::VEC3(0, 280, -900);
    this->device->camera.focus    = mbm::VEC3(0, 280, 0);
    this->texBox                  = new mbm::TEXTURE_VIEW(this, false, true);
	util::addPath(__FILE__);//little trick to add path of file image when debuging VS
    this->texBox->load("../src/test-lib/wooden-box.jpg",200,200);
}

void MY_SCENE::logic()
{
}

void MY_SCENE::onTouchDown(int, float, float)
{
}

void MY_SCENE::onTouchUp(int, float, float)
{
}

void MY_SCENE::onTouchMove(int, float x, float y)
{
	if(this->texBox)
	{
    	this->texBox->position.x = x;
    	this->texBox->position.y = y;
	}
}

void MY_SCENE::onTouchZoom(float)
{
}

void MY_SCENE::onFinalizeScene()
{
}

void MY_SCENE::onKeyDown(int)
{
}

void MY_SCENE::onKeyUp(int)
{
}

void MY_SCENE::onKeyDownJoystick(int, int)
{
}

void MY_SCENE::onKeyUpJoystick(int, int)
{
}

void MY_SCENE::onMoveJoystick(int, float, float, float, float)
{
}

void MY_SCENE::onInfoDeviceJoystick(int, int, const char *,const char *)
{
}


MY_GAME::MY_GAME(JNIEnv *env, jobject obj)
{
	this->device->jni->jenv = env;
    this->setScene(&myScene);
}
MY_GAME::~MY_GAME()
{
    mbm::DEVICE::quit();
}

bool MY_GAME::existScene(const int idScene)
{
    if(idScene == this->myScene.getIdScene())
        return true;
    return false;
}
