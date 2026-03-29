/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2026 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include "my-scene.h"
#include <core_mbm/util-interface.h>
#include <core_mbm/specific-opengl_es.h>

MY_SCENE::MY_SCENE()
    : texBox(nullptr)
{
}

MY_SCENE::~MY_SCENE()
{
    delete texBox;
}

void MY_SCENE::init()
{
    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    device->camera.position = mbm::VEC3(0, 280, -900);
    device->camera.focus    = mbm::VEC3(0, 280,    0);
    texBox = new mbm::TEXTURE_VIEW(this, false, true);
    texBox->load("wooden-box.jpg", 200, 200);
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
    if (texBox)
    {
        texBox->position.x = x;
        texBox->position.y = y;
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

void MY_SCENE::onInfoDeviceJoystick(int, int, const char*, const char*)
{
}

void MY_SCENE::onResizeWindow()
{
}

// ---------------------------------------------------------------------------

MY_GAME::MY_GAME()
{
    this->setScene(&myScene);
}

MY_GAME::~MY_GAME()
{
}

bool MY_GAME::existScene(const int /*idScene*/)
{
    return false;
}
