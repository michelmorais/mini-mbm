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
    texBox                    = nullptr;
    gif                       = nullptr;
    sprite                    = nullptr;
    mesh                      = nullptr;
    shape                     = nullptr;
    line                      = nullptr;
    particle                  = nullptr;
    particle_ptl              = nullptr;
    render2Texture            = nullptr;
    toTrack                   = nullptr;
    steeredParticle           = nullptr;
    background                = nullptr;
    fontDraw                  = nullptr;
    hmd                       = nullptr;
    tile                      = nullptr;
    texture                   = nullptr;
    mousePositionText         = nullptr;
    backGroundTimeToHide      = 1.0f;
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
    if(particle_ptl)
        delete particle_ptl;
    //we do not to delete mousePositionText because it is managed by fontDraw, so when fontDraw is deleted, it will take care of deleting the text objects that it manages (including mousePositionText)
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
    constexpr bool _2dWorldIsTrue        = true;
    constexpr bool _2dScreenWorldIsTrue  = true;
    constexpr bool _2dScreenWorldIsFalse = false;
    constexpr bool _3dWorldIsFalse       = false;
    constexpr bool _3dWorldIsTrue        = true;

    constexpr bool create_texBox              = true;
    constexpr bool create_gif                 = true;
    constexpr bool create_sprite              = true;
    constexpr bool create_background          = false;
    constexpr bool create_mesh                = true;
    constexpr bool create_shape               = true;
    constexpr bool create_line                = true;
    constexpr bool create_particle            = true;
    constexpr bool create_particle_ptl        = false;
    constexpr bool create_render2Texture      = true;
    constexpr bool create_steeredParticle     = false;
    constexpr bool create_fontDraw            = false;//segmentation fault when load font on Mac
    constexpr bool create_hmd                 = false;
    //constexpr bool create_tile                = true;
    constexpr bool create_texture             = true;
    constexpr bool create_mousePositionText   = false;
    
    util::addPath(__FILE__);//little trick to add path of file image when debuging VS
    util::addPath("C:\\Users\\miche\\Downloads");
    if (create_background)
    {
        this->background = new mbm::BACKGROUND(this, _3dWorldIsFalse);
        bool majorScale = true;
        if(this->background->load("ground.png", true, majorScale))
        {
            INFO_LOG("Background loaded successfully");
            this->background->position.y = 200;
            this->background->position.z = 100;
        }
        else
        {
            ERROR_LOG("Failed to load background");
        }
    }
    
    if (create_fontDraw)
    {
        this->fontDraw = new mbm::FONT_DRAW(this);
        float heightLetter = 0; // since this font is binary, this should not affect the result, but for ttf fonts this is the height of the letters in pixels, so it is important to set it right (50 in our test font)
        short spaceWidth = 0;
        short spaceHeight = 0;
        bool saveTextureAsPng = false;
        if (this->fontDraw && this->fontDraw->loadFont("VCR_OSD_MONO_1-50.fnt", heightLetter, spaceWidth, spaceHeight, saveTextureAsPng))
        {
            this->fontDraw->addText("Hello\tMini-MBM!", _2dWorldIsTrue, _2dScreenWorldIsTrue);
            if (create_mousePositionText)
            {
                mousePositionText = this->fontDraw->addText("Another line!",_2dWorldIsTrue, _2dScreenWorldIsTrue);
                mousePositionText->scale = mbm::VEC3(0.5f, 0.5f, 0.5f);
            }
            INFO_LOG("Font loaded successfully");
        }
    }
    

    //if (meshDebug.loadDebugFromMemory(this->fontDraw->getMesh()))
    //{
    //    meshDebug.getInfo()
    //}
    
    //tile = new mbm::TILE(this, false, false);
    //util::addPath("C:\\Users\\miche\\Documents\\tower-defense\\tile");
    //util::addPath("C:\\Users\\miche\\Documents\\tower-defense\\image");
    //tile->load("tile-stage-1.tile");
    
    
    //TODO: check why gif is resizing wrong when load with width and height on lost device
    if (create_gif)
    {
        gif = new mbm::GIF_VIEW(this, _3dWorldIsFalse, _2dScreenWorldIsFalse);
        gif->load("Lion-King.gif",600,400);
    }
    
    if (create_texBox)
    {
        this->texBox = new mbm::TEXTURE_VIEW(this, _3dWorldIsFalse, _2dScreenWorldIsFalse);
        if(this->texBox->load("wooden-box.jpg", 200, 200))
        {
            INFO_LOG("TextureView loaded successfully");
            this->texBox->position.x = 300;
            this->texBox->position.y = 100;
        }
        else
        {
            ERROR_LOG("Failed to load TextureView");
        }
    }
    
    //TODO: Needs to be investgated
    if(create_hmd)
    {
        hmd = new mbm::HMD(this);
        if (hmd->load())
        {
            hmd->addObject2Render(this->gif);
            INFO_LOG("HMD loaded successfully");
        }
        else
        {
            ERROR_LOG("Failed to load HMD");
        }
    }
    
    
    if (create_sprite)
    {
        sprite = new mbm::SPRITE(this, false, false);
        if(sprite && sprite->load("box.spt"))
        {
            INFO_LOG("Sprite loaded successfully");
            sprite->alwaysRenderize = true;
            sprite->scale = mbm::VEC3(0.5f, 0.5f, 0.5f);
            sprite->position.x = -200;
        }
        else
        {
            ERROR_LOG("Failed to load sprite");
        }
    }
    //sprite->load("C:\\Users\\miche\\Downloads\\blast.spt");
    util::addPath("C:\\Users\\miche\\Documents\\tower-defense\\image");
    util::addPath("C:\\Users\\miche\\Documents\\tower-defense\\sprite");


    if (create_texture)
    {
        texture = new mbm::TEXTURE_VIEW(this, _3dWorldIsFalse, _2dScreenWorldIsTrue);
        if (texture->load("pie.png"))
        {
            float w, h;
            texture->getAABB(&w, &h);
            texture->position.y = device->backBufferHeight - (h / 2.0f);
            texture->position.x = w / 2.0f;
            INFO_LOG("Pie texture loaded");
            mbm::SHADER_CFG*  pShaderCfgPie = device->cfg.getShader("pie.ps");
            if (pShaderCfgPie)
            {
                INFO_LOG("Pie shader found in the resource ...");
                mbm::FX* fx = texture->getFx();
                if (fx)
                {
                    INFO_LOG(" Applying shader pie to texture");
                    if (fx->loadNewShader(pShaderCfgPie, nullptr, mbm::TYPE_ANIMATION_GROWING_LOOP, 5.0, mbm::TYPE_ANIMATION_PAUSED, 0.0f))
                    {
                        INFO_LOG("Shader pie applyied sucessfully to texture");
                        //float dataPercent[4]   = { 0, 0, 0, 0 };
                        //float dataAngle[4]     = { 0, 0, 0, 0 };
                        //float dataClockwise[4] = { 1, 0, 0, 0 };
                        //if (fx->setMinVarPShader("percent", dataPercent) && fx->setMinVarPShader("angle", dataAngle) && fx->setMinVarPShader("clockwise", dataClockwise))
                        //    INFO_LOG("Min shader values applied to pie");
                        //{
                        //    INFO_LOG("Min shader values applied to pie");
                        //}
                        //if (fx->setMaxVarPShader("percent", dataPercent) && fx->setMaxVarPShader("angle", dataAngle) && fx->setMaxVarPShader("clockwise", dataClockwise))
                        //{
                        //    INFO_LOG("Max shader values applied to pie");
                        //}
                        //texture->restartAnimation();
                    }
                    else
                    {
                        ERROR_LOG("Failed to apply shader pie to texture");
                    }
                }
                else
                {
                    ERROR_LOG("Failed to get FX from texture");
                }
            }
        }
    }
    
    if (create_mesh)
    {
        mesh = new mbm::MESH(this, _3dWorldIsTrue, _2dScreenWorldIsFalse);
        if(mesh->load("Barrel_NoTop.msh"))
        {
            INFO_LOG("Mesh loaded successfully");
            mesh->scale = mbm::VEC3(10.0f, 10.0f, 10.0f);
            mesh->position.z = -100;
            mesh->position.y = 100;
        }
        else
        {
            ERROR_LOG("Failed to load mesh");
        }
    }
    

    if (create_shape)
    {
        shape = new mbm::SHAPE_MESH(this, _3dWorldIsFalse, _2dScreenWorldIsFalse);
        if(shape->loadRectangle("quad", 100, 100, true, 2))
        {
            INFO_LOG("Shape loaded successfully");
            shape->position.x = 100;
            shape->position.y = 100;
        }
        else
        {
            ERROR_LOG("Failed to load shape");
        }
    }
    
    if (create_line)
    {
        line = new mbm::LINE_MESH(this, _3dWorldIsFalse, _2dScreenWorldIsFalse);
        for (int i = 0; i < 2; i++)
        {
            std::vector<mbm::VEC3> lines;
            lines.push_back(mbm::VEC3(0 + i * 10, 0, 0));
            lines.push_back(mbm::VEC3(0 + i * 10, 100, 0));
            line->add(std::move(lines));
        }
        line->enableRender = false;
    }
    
    
    if (create_render2Texture)
    {
        render2Texture = new mbm::RENDER_2_TEXTURE(this, _3dWorldIsFalse, _2dScreenWorldIsTrue);
        if (render2Texture->load(350, 204, 350, 204, "my-render", true))
        {
            INFO_LOG("Render2Texture loaded successfully");
            if(render2Texture->addObject2Render(gif))
            {
                INFO_LOG("Gif added to render2Texture successfully");
            }
            else
            {
                ERROR_LOG("Failed to add gif to render2Texture");
            }
            render2Texture->position.x = static_cast<float>(device->backBufferWidth) - (350 / 2.0f);
            render2Texture->position.y = 204 / 2.0f;
        }
        else
        {
            ERROR_LOG("Failed to load render2Texture");
        }
    }

    if (texBox)
    {
        this->toTrack = texBox;
    }

    if (create_steeredParticle)
    {
        bool segmented = false;
        mbm::INFO_PHYSICS infoPhysiscs;
        infoPhysiscs.lsCube.push_back(new mbm::CUBE(200,200,200));
        steeredParticle = new mbm::STEERED_PARTICLE(this, _3dWorldIsFalse, _2dScreenWorldIsFalse, segmented, nullptr );
        mbm::COLOR colorParticle(1.0f, 0.0f, 0.0f, 1.0f);
        if (steeredParticle->load("particle.png", &colorParticle, &infoPhysiscs))
        {
            steeredParticle->addParticle(1432, steeredParticle->addGroup(&colorParticle) - 1);
            steeredParticle->setRadiusScale(2);
            
            mbm::FLUID_GROUP* group = steeredParticle->getParticleGroup(0);
            group->aSizeParticle = 20.0f;
            
            randomSteeredParticlePositions();
        
            INFO_LOG("Particle z position %f", steeredParticle->position.z);
        
            INFO_LOG("Steered Particle loaded successfully");
            INFO_LOG("Total particles to render: %u", steeredParticle->getTotalParticleToRender());
            INFO_LOG("Particle group count: %zu", steeredParticle->getTotalGroup());
            if (group)
            {
                INFO_LOG("Group size: %u, total to render: %u", group->size_particle_array, group->totalParticleToRender);
            }
        
            if (steeredParticle->getTexture())
            {
                INFO_LOG("Texture loaded: %u x %u", steeredParticle->getTexture()->getWidth(), steeredParticle->getTexture()->getHeight());
            }
            else
            {
                ERROR_LOG("ERROR: Texture not loaded!");
            }
        
            INFO_LOG("Enable render: %d, Always renderize: %d", steeredParticle->enableRender ? 1 : 0, steeredParticle->alwaysRenderize ? 1 : 0);
            steeredParticle->restartAnimationParticle();
            steeredParticle->restartAnimation();
        }
        else
        {
            ERROR_LOG("Failed to load steered particle");
        }
    }

    if (create_particle)
    {
        particle = new mbm::PARTICLE(this, _3dWorldIsFalse, _2dScreenWorldIsFalse);
        if (particle->load("particle.png", nullptr, nullptr, 100, true))
        {
            particle->addParticle(1000,true);
            particle->addStage();
            particle->restartAnimationParticle();
            INFO_LOG("Particle loaded successfully");
        }
        else
        {
            ERROR_LOG("Failed to load particle from file...");
        }
    }

    if (create_particle_ptl)
    {
        particle_ptl = new mbm::PARTICLE(this, _3dWorldIsFalse, _2dScreenWorldIsFalse);
        if (particle_ptl->load("particle_red.ptl", nullptr, nullptr, 100, true))
        {
            particle_ptl->addParticle(100, true);
            particle_ptl->restartAnimationParticle();
            INFO_LOG("Particle_ptl loaded successfully");
        }
        else
        {
            ERROR_LOG("Failed to load particle_ptl");
        }
    }
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
    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    backGroundTimeToHide -= device->delta;
    if(backGroundTimeToHide <= 0 && background)
    {
        background->enableRender = false;
    }
    if (count == 30)
    {
        //if (render2Texture && render2Texture->saveAsPNG("C:\\Users\\miche\\Downloads\\test.png", 0, 0, render2Texture->widthTexture, render2Texture->heightTexture))
        //{
        //    INFO_LOG("Image saved at download");
        //}
    }
    if(mesh)
    {
        mesh->angle.y += device->delta * 0.5f;
        if (mesh->angle.y > 360.0f)
            mesh->angle.y -= 360.0f;
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
            if(fx->getVarPShader("percent", data) == 0)
            {
                static bool loggedError = false;
                if (!loggedError)
                ERROR_LOG("Failed to get percent variable from shader");
                loggedError = true;
            }
            //INFO_LOG("data : %g", data[0]);
            //data[0] += 0.01f;
            //if (data[0] > 1.0f)
            //    data[0] = 0.0f;
            //fx->setVarPShader("percent", data);
        }
    }
}

void MY_SCENE::onTouchDown(int key, float x, float y)
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
    if(line)
    {
        if (key == 0)
        {
            line->enableRender = !line->enableRender;
        }
    }
    if(particle_ptl)
    {
        if (key == 0)
        {
            if(particle_ptl->is2dS)
            {
                particle_ptl->position.x = x;
                particle_ptl->position.y = y;
            }
            else
            {
                mbm::DEVICE* device = mbm::DEVICE::getInstance();
                device->transformeScreen2dToWorld2d_scaled(x, y, particle_ptl->position);
            }

            if(particle_ptl->addParticle(100, true))
            {
                INFO_LOG("Added 100 particles to particle_ptl");
            }
            else
            {
                ERROR_LOG("Failed to add particles to particle_ptl");
            }
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
    if(mousePositionText)
    {
        mousePositionText->setText("%d, %d", static_cast<int>(x), static_cast<int>(y));
        mousePositionText->position = mbm::VEC3(x, y, 0);
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
