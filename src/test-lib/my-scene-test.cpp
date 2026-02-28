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
#include <core_mbm/shader-resource.h>
#include <core_mbm/util-interface.h>
#include <random>

MY_SCENE::MY_SCENE()
{
    randomizeParticleEachLoop = false;
    texBox          = nullptr;
    gif             = nullptr;
    sprite          = nullptr;
    mesh            = nullptr;
    shape           = nullptr;
    line            = nullptr;
    particle        = nullptr;
    render2Texture  = nullptr;
    toTrack         = nullptr;
    steeredParticle = nullptr;
    background      = nullptr;
    fontDraw        = nullptr;
    hmd             = nullptr;
    tile            = nullptr;
    texture         = nullptr;
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
    if (steeredParticle)
        delete steeredParticle;
    if(background)
		delete background;
    if (fontDraw)
        delete fontDraw;
    if(hmd)
		delete hmd;
    if (tile)
        delete tile;
    if (texture)
        delete texture;
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
	this->background = new mbm::BACKGROUND(this, false);
	bool majorScale = true;
    this->background->load("ground.png", true, majorScale);
    //util::addPath("C:\\Users\\miche\\Documents\\mini-mbm\\src\\test-lib\\");
    ////util::addPath("C:\\Users\\miche\\Dropbox\\Games\\3d\\AntigosX");
    //util::addPath("C:\\Users\\miche\\Dropbox\\Games\\3d\\Box-broken");
    

	//this->fontDraw = new mbm::FONT_DRAW(this);
    //if (this->fontDraw->loadFont("font_example.ttf", 32, 0, 0, false))
    //{
	//	this->fontDraw->addText("Hello\tMini-MBM!", mbm::VEC2(10, 10), true, true);
    //    this->fontDraw->addText("Another_text!", mbm::VEC2(10, 50), true, true);
    //}

    //if (meshDebug.loadDebugFromMemory(this->fontDraw->getMesh()))
    //{
    //    meshDebug.getInfo()
    //}
    
	//tile = new mbm::TILE(this, false, false);
    //util::addPath("C:\\Users\\miche\\Documents\\tower-defense\\tile");
    //util::addPath("C:\\Users\\miche\\Documents\\tower-defense\\image");
    //tile->load("tile-stage-1.tile");
    
    //this->texBox->position.z = 0.11;
    //this->texBox->alwaysRenderize = true;

    //bool loaded = this->texBox->getTexture() != nullptr;
    //INFO_LOG("texBox load returned: %d", loaded ? 1 : 0);
    //if (loaded)
    //{
    //    INFO_LOG("texBox texture name: %s", this->texBox->getFileNameTexture().c_str());
    //    INFO_LOG("texBox size: %u x %u  alpha:%d", this->texBox->getTexture()->getWidth(),
    //        this->texBox->getTexture()->getHeight(),
    //        this->texBox->getTexture()->useAlphaChannel ? 1 : 0);
    //}
    //else
    //{
    //    // quick test: try absolute path or solid color
    //    INFO_LOG("Retrying with solid color test...");
    //    this->texBox->load("#FF0000", 200, 200);
    //    this->texBox->getTexture() ? INFO_LOG("Retry succeeded") : INFO_LOG("Retry failed");
    //}

	////TODO: check why gif is resizing wrong when load with width and height on lost device
    //gif = new mbm::GIF_VIEW(this,false,false);
    //gif->load("Lion-King.gif",600,400);

    //this->texBox = new mbm::TEXTURE_VIEW(this, false, false);
    //this->texBox->load("wooden-box.jpg", 200, 200);
    
    //TODO: Needs to be investgated
    //hmd = new mbm::HMD(this);
    //if (hmd->load())
    //{
    //    hmd->addObject2Render(this->gif);
    //}
    
    
    sprite = new mbm::SPRITE(this, false, false);
    //sprite->load("C:\\Users\\miche\\Downloads\\blast.spt");
    util::addPath("C:\\Users\\miche\\Documents\\tower-defense\\image");
    util::addPath("C:\\Users\\miche\\Documents\\tower-defense\\sprite");

    //sprite->load("pie.spt");
    sprite->load("dialog.spt");
    sprite->alwaysRenderize = true;
    sprite->position.z = 1;

    //texture = new mbm::TEXTURE_VIEW(this,false, false);
    //if (texture->load("pie.png"))
    //{
    //    INFO_LOG("Pie texture loaded");
    //    mbm::SHADER_CFG*  pShaderCfgPie = device->cfg.getShader("pie.ps");
    //    if (pShaderCfgPie)
    //    {
    //        INFO_LOG("Pie shader found in the resource ...");
    //        mbm::FX* fx = texture->getFx();
    //        if (fx)
    //        {
    //            INFO_LOG(" Applying shader pie to texture");
    //            if (fx->loadNewShader(pShaderCfgPie, nullptr, mbm::TYPE_ANIMATION_GROWING_LOOP, 5.0, mbm::TYPE_ANIMATION_PAUSED, 0.0f))
    //            {
    //                INFO_LOG("Shader pie applyied sucessfully to texture");
    //                //float dataPercent[4]   = { 0, 0, 0, 0 };
    //                //float dataAngle[4]     = { 0, 0, 0, 0 };
    //                //float dataClockwise[4] = { 1, 0, 0, 0 };
    //                //if (fx->setMinVarPShader("percent", dataPercent) && fx->setMinVarPShader("angle", dataAngle) && fx->setMinVarPShader("clockwise", dataClockwise))
    //                //    INFO_LOG("Min shader values applied to pie");
    //                //{
    //                //    INFO_LOG("Min shader values applied to pie");
    //                //}
    //                //if (fx->setMaxVarPShader("percent", dataPercent) && fx->setMaxVarPShader("angle", dataAngle) && fx->setMaxVarPShader("clockwise", dataClockwise))
    //                //{
    //                //    INFO_LOG("Max shader values applied to pie");
    //                //}
    //                //texture->restartAnimation();
    //            }
    //
    //            /*INFO_LOG("Pin the value to a visible range Applying shader pie to texture");
    //            if (fx->loadNewShader(pShaderCfgPie, nullptr, mbm::TYPE_ANIMATION_GROWING_LOOP, 5.0, mbm::TYPE_ANIMATION_PAUSED, 0.0f))
    //            {
    //                INFO_LOG("Shader pie applyied sucessfully to texture");
    //                float dataPercent[4] = { 0, 0, 0, 0 };
    //                float dataAngle[4] = { 0, 0, 0, 0 };
    //                float dataClockwise[4] = { 1, 0, 0, 0 };
    //                if (fx->setMinVarPShader("percent", dataPercent) && fx->setMinVarPShader("angle", dataAngle) && fx->setMinVarPShader("clockwise", dataClockwise))
    //                    INFO_LOG("Min shader values applied to pie");
    //                if (fx->setMaxVarPShader("percent", dataPercent) && fx->setMaxVarPShader("angle", dataAngle) && fx->setMaxVarPShader("clockwise", dataClockwise))
    //                    INFO_LOG("Max shader values applied to pie");
    //                texture->restartAnimation();
    //            }*/
    //        }
    //    }
    //}
    //
    
    //**************
    //TODO: check C:\Users\miche\Dropbox\Games\3d\Box-broken\crateShattered.mbm save in pixel shader editor fails
    //**************

    //mesh = new mbm::MESH(this, true, false);
    //mesh->load("crateShattered.mbm"); VB noi texture?
    //mesh->load("crateWreck1.mbm");
    //mesh->scale.x = 3;
    //mesh->scale.y = 3;
    //mesh->scale.z = 3;
    //mesh->position.y = 100;
    //

    

    //shape = new mbm::SHAPE_MESH(this, false, false);
    //shape->loadRectangle("quad", 100, 100, true, 2);
    //shape->position.x = 300;

    //line = new mbm::LINE_MESH(this, false, false);
	//for (int i = 0; i < 2; i++)
    //{
    //    std::vector<mbm::VEC3> lines;
    //    lines.push_back(mbm::VEC3(0 + i * 10, 0, 0));
    //    lines.push_back(mbm::VEC3(0 + i * 10, 100, 0));
    //    line->add(std::move(lines));
    //}
    
    
    //render2Texture = new mbm::RENDER_2_TEXTURE(this, false, false);
    //if (render2Texture->load(512, 512, 512, 512, "my-render", true))
    //{
    //    render2Texture->addObject2Render(gif);
    //    render2Texture->addObject2Render(this->texBox);
    //    //render2Texture->addObject2Render(sprite);
    //    //render2Texture->addObject2Render(shape);
    //    //render2Texture->addObject2Render(line);
    //}

    this->toTrack = texBox;
    

    //mbm::INFO_PHYSICS infoPhysiscs;
	//infoPhysiscs.lsCube.push_back(new mbm::CUBE(200,200,200));
    //steeredParticle = new mbm::STEERED_PARTICLE(this, false, false, false, nullptr );
	//mbm::COLOR colorParticle(1.0f, 0.0f, 0.0f, 1.0f);
    //if (steeredParticle->load("C:\\Users\\miche\\Downloads\\fluid_particle.png", &colorParticle, &infoPhysiscs))
    //{
    //    steeredParticle->addParticle(1432, steeredParticle->addGroup(&colorParticle) - 1);
    //    steeredParticle->setRadiusScale(2);
    //    
    //    mbm::FLUID_GROUP* group = steeredParticle->getParticleGroup(0);
    //    group->aSizeParticle = 20.0f;
	//	
    //    randomSteeredParticlePositions();
    //
    //    INFO_LOG("Particle z position %f", steeredParticle->position.z);
    //
    //    INFO_LOG("Steered Particle loaded successfully");
    //    INFO_LOG("Total particles to render: %u", steeredParticle->getTotalParticleToRender());
    //    INFO_LOG("Particle group count: %zu", steeredParticle->getTotalGroup());
    //    if (group)
    //    {
    //        INFO_LOG("Group size: %u, total to render: %u", group->size_particle_array, group->totalParticleToRender);
    //    }
    //
    //    if (steeredParticle->getTexture())
    //    {
    //        INFO_LOG("Texture loaded: %u x %u", steeredParticle->getTexture()->getWidth(), steeredParticle->getTexture()->getHeight());
    //    }
    //    else
    //    {
    //        INFO_LOG("ERROR: Texture not loaded!");
    //    }
    //
    //    INFO_LOG("Enable render: %d, Always renderize: %d", steeredParticle->enableRender ? 1 : 0, steeredParticle->alwaysRenderize ? 1 : 0);
    //}
    //else
    //{
    //    INFO_LOG("ERROR: Failed to load steered particle");
    //}
	

    
    //AARRGGBB
    //const char * fileNameTextureOrMesh = "#FFFF0000";
    //particle = new mbm::PARTICLE(this, false, false);
    //if (particle->load(fileNameTextureOrMesh, nullptr, nullptr, 100, true))
    //{
    //    particle->addParticle(1000,true);
    //    particle->addStage();
    //}
    
}

void MY_SCENE::randomSteeredParticlePositions()
{
    if (steeredParticle)
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        mbm::FLUID_GROUP* group = steeredParticle->getParticleGroup(0);
        if (group)
        {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            std::uniform_real_distribution<float> disX(-static_cast<float>(device->backBufferWidth) * 0.5f, static_cast<float>(device->backBufferWidth) * 0.5f);
            std::uniform_real_distribution<float> disY(-static_cast<float>(device->backBufferHeight) * 0.5f, static_cast<float>(device->backBufferHeight) * 0.5f);

            for (uint32_t i = 0; i < group->size_particle_array; i++)
            {
                float randomX = disX(gen);
                float randomY = disY(gen);
                group->particle_positions[i] = mbm::VEC3(randomX, randomY, 0);
            }
        }
    }
}

void MY_SCENE::logic()
{
    static int count = 0;
    count++;
    if (count == 30)
    {
        //if (render2Texture && render2Texture->saveAsPNG("C:\\Users\\miche\\Downloads\\test.png", 0, 0, render2Texture->widthTexture, render2Texture->heightTexture))
        //{
        //    INFO_LOG("Image saved at download");
        //}
    }
    if (randomizeParticleEachLoop)
    {
        randomSteeredParticlePositions();
    }
    //if (sprite)
    //{
    //    auto fx = sprite->getFx();
    //    if (fx)
    //    {
    //        float data[4] = { 0, 0, 0, 0 };
    //        fx->getVarPShader("percent", data);
    //        INFO_LOG("data : %g", data[0]);
    //        //data[0] += 0.01f;
    //        //if (data[0] > 1.0f)
    //        //    data[0] = 0.0f;
    //        //fx->setVarPShader("percent", data);
	//	}
    //}
    if (texture)
    {
        auto fx = texture->getFx();
        if (fx)
        {
            float data[4] = { 0, 0, 0, 0 };
            fx->getVarPShader("percent", data);
            INFO_LOG("data : %g", data[0]);
            //data[0] += 0.01f;
            //if (data[0] > 1.0f)
            //    data[0] = 0.0f;
            //fx->setVarPShader("percent", data);
        }
    }
}

void MY_SCENE::onTouchDown(int key, float, float)
{
	INFO_LOG("Touch down key: %d", key);
    if (sprite)
    {
        if (key == 0)
        {
            auto fx = sprite->getFx();
            if (fx)
            {
                float data[4] = { 0, 0, 0, 0 };
                fx->setVarPShader("percent", data);
            }
        }
        else
        {
            sprite->restartAnimation();
        }
    }
    
}

void MY_SCENE::onTouchUp(int, float, float)
{
}

void MY_SCENE::onTouchMove(int, float x, float y)
{
    if(this->toTrack)
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
		device->transformeScreen2dToWorld2d_scaled(x, y, this->toTrack->position);
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