
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

#include "my-scene-test.h"
#include <core_mbm/texture-manager.h>
#include <core_mbm/util-interface.h>


MY_SCENE::MY_SCENE()
{
    texBox         = nullptr;
    gif            = nullptr;
    sprite         = nullptr;
    mesh           = nullptr;
    shape          = nullptr;
    line           = nullptr;
    particle       = nullptr;
    render2Texture = nullptr;
}

MY_SCENE::~MY_SCENE()
{
    if(texBox)
        delete texBox;
    if(gif)
        delete gif;
    if(sprite)
        delete sprite;
    if (mesh)
        delete mesh;
    if (shape)
        delete shape;
    if (line)
        delete line;
    if (particle)
        delete particle;
    if (render2Texture)
        delete render2Texture;
}

void MY_SCENE::startLoading()
{
    INFO_LOG("Starting loading scene...");
}
    
void MY_SCENE::endLoading()
{
    INFO_LOG("End loading scene...");
}

void MY_SCENE::init() 
{
    mbm::DEVICE * device    = mbm::DEVICE::getInstance();
    device->camera.position = mbm::VEC3(0, 280, -900);
    device->camera.focus    = mbm::VEC3(0, 280, 0);
    device->colorClearBackGround.b = 0.5f;
    util::addPath(__FILE__);//little trick to add path of file image when debuging VS
    util::addPath("C:\\Users\\miche\\Downloads");
    //util::addPath("C:\\Users\\miche\\Documents\\mini-mbm\\src\\test-lib\\");
    ////util::addPath("C:\\Users\\miche\\Dropbox\\Games\\3d\\AntigosX");
    //util::addPath("C:\\Users\\miche\\Dropbox\\Games\\3d\\Box-broken");
    this->texBox            = new mbm::TEXTURE_VIEW(this, false, true);
    this->texBox->load("wooden-box.jpg", 200, 200);
    this->texBox->position.z = 1;
    this->texBox->alwaysRenderize = true;

    bool loaded = this->texBox->getTexture() != nullptr;
    INFO_LOG("texBox load returned: %d", loaded ? 1 : 0);
    if (loaded)
    {
        INFO_LOG("texBox texture name: %s", this->texBox->getFileNameTexture().c_str());
        INFO_LOG("texBox size: %u x %u  alpha:%d", this->texBox->getTexture()->getWidth(),
            this->texBox->getTexture()->getHeight(),
            this->texBox->getTexture()->useAlphaChannel ? 1 : 0);
    }
    else
    {
        // quick test: try absolute path or solid color
        INFO_LOG("Retrying with solid color test...");
        this->texBox->load("#FF0000", 200, 200);
        this->texBox->getTexture() ? INFO_LOG("Retry succeeded") : INFO_LOG("Retry failed");
    }

    gif = new mbm::GIF_VIEW(this,false,false);
    gif->load("Lion-King.gif");

    //sprite = new mbm::SPRITE(this, false, true);
    //sprite->load("C:\\Users\\miche\\Downloads\\blast.spt");
    //sprite->alwaysRenderize = true;
    
    
    //**************
    //TODO: check C:\Users\miche\Dropbox\Games\3d\Box-broken\crateShattered.mbm save in pixel shader editor fails
    //**************

    //mesh = new mbm::MESH(this, true, false);
    ////mesh->load("crateShattered.mbm"); VB noi texture?
    //mesh->load("crateWreck1.mbm");
    //mesh->scale.x = 3;
    //mesh->scale.y = 3;
    //mesh->scale.z = 3;
    //

    render2Texture = new mbm::RENDER_2_TEXTURE(this, false, false);
    
    if (render2Texture->load(512, 512, 512, 512, "my-render", true))
    {
        render2Texture->addObject2Render(gif);
    }

    shape = new mbm::SHAPE_MESH(this, false, false);
    shape->loadRectangle("quad", 100, 100, true, 2);
    shape->position.x = 300;

    line = new mbm::LINE_MESH(this, false, false);
    std::vector<mbm::VEC3> lines;
    lines.push_back(mbm::VEC3(0, 0, 0));
    lines.push_back(mbm::VEC3(100,100, 0));
    line->add(std::move(lines));
    //AARRGGBB
    const char * fileNameTextureOrMesh = "#FFFF0000";
    particle = new mbm::PARTICLE(this, false, false);
    if (particle->load(fileNameTextureOrMesh, nullptr, nullptr, 100, true))
    {
        particle->addParticle(1000,true);
    }
    particle->alwaysRenderize = true;
    
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
    if (sprite)
    {
        sprite->position.x = x;
        sprite->position.y = y;
    }
}

void MY_SCENE::onTouchZoom(float)
{
}

void MY_SCENE::onFinalizeScene()
{
}

void MY_SCENE::onKeyDown(int key)
{
    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    if (key == 39)//right
    {
        device->camera.position2d.x += 10;
    }
    else if (key == 37)//left
    {
        device->camera.position2d.x -= 10;
    }
    else if (key == 38)//up
    {
        device->camera.position2d.y += 10;
    }
    else if (key == 40)//down
    {
        device->camera.position2d.y -= 10;
    }
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

void MY_SCENE::onResizeWindow()
{
    INFO_LOG("No resize window implementation for this scene");
}

bool GAME::existScene(const int idScene)
{
    if(idScene == this->myScene.getIdScene())
        return true;
    return false;
}


GAME::GAME()
{
    this->setScene(&myScene);
}
GAME::~GAME()
{
    mbm::DEVICE::quit();
}