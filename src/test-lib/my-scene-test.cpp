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
#include <core_mbm/shader-var-cfg.h>
#include <cstdio>
#include <cstdarg>
#include <cmath>
#include <random>

static inline const char* modeToStr(RenderMode mode)
{
    switch (mode)
    {
        case RenderMode::SCREEN_2D: return "2DS";
        case RenderMode::WORLD_2D:  return "2DW";
        case RenderMode::WORLD_3D:  return "3D";
        default:                    return "NONE";
    }
}

MY_SCENE::MY_SCENE()
{
    texBox             = nullptr;
    gif                = nullptr;
    sprite             = nullptr;
    mesh               = nullptr;
    shape              = nullptr;
    line               = nullptr;
    particle           = nullptr;
    particle_ptl       = nullptr;
    render2Texture     = nullptr;
    steeredParticle    = nullptr;
    background         = nullptr;
    fontDrawNoShader   = nullptr;
    hmd                = nullptr;
    tile               = nullptr;
    texture            = nullptr;
    hintsText          = nullptr;
    shaderInfoText     = nullptr;
    btn2dS             = nullptr;
    btn2dW             = nullptr;
    btn3d              = nullptr;
    trackMouse         = nullptr;
    lineFontIsOver     = nullptr;
    menuVisible        = true;
    for (uint32_t j = 0; j < sizeof(posMenuTexts) / sizeof(posMenuTexts[0]); j++) 
    {
        posMenuTexts[j] = nullptr;
    }
    posMenuSelected    = 0;
    posMenuVisible     = true;
    worldMenuVisible   = true;
    lastLoadedRowIdx    = -1;
    statusText          = nullptr;
    mouseScreenX        = 0.0f;
    mouseScreenY        = 0.0f;
    notificationText    = nullptr;
    notificationTimer   = 0.0f;
    shaderMenuVisible   = true;
    currentPsShaderIdx = -1;
    currentVsShaderIdx = -1;
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
    if (fontDrawNoShader)
        delete fontDrawNoShader;
    if(hmd)
        delete hmd;
    if (tile)
        delete tile;
    if (texture)
        delete texture;
    if(particle_ptl)
        delete particle_ptl;
    if(lineFontIsOver)
        delete lineFontIsOver;
}

void MY_SCENE::startLoading()
{
    INFO_LOG("Starting loading scene...");
}
    
void MY_SCENE::endLoading()
{
    INFO_LOG("End loading scene...");
}

void MY_SCENE::onInitScene()
{
    mbm::DEVICE* device    = mbm::DEVICE::getInstance();
    device->getCamera().position = mbm::VEC3(0, 280, -900);
    device->getCamera().focus    = mbm::VEC3(0, 280, 0);
    mbm::COLOR backgroundColor = device->getColorClearBackGround();
    backgroundColor.b = 0.5f;
    device->setColorClearBackGround(backgroundColor);

    util::addPath(__FILE__);

    this->fontDrawNoShader = new mbm::FONT_DRAW(this);
    float heightLetter   = 0;
    short spaceWidth     = 0;
    short spaceHeight    = 0;
    bool  saveTexAsPng   = false;
    if (this->fontDrawNoShader->loadFont("Font-test-no-shader-50.fnt", heightLetter, spaceWidth, spaceHeight, saveTexAsPng))
    {
        INFO_LOG("Font loaded successfully");
        buildMenu();
        buildPosMenu();
        buildWorldMenu();
        buildShaderMenu();
        lineFontIsOver = new mbm::LINE_MESH(this, false, true);
        std::vector<mbm::VEC3> linePoints(4);
        lineFontIsOver->add(std::move(linePoints));
    }
    else
    {
        ERROR_LOG("Failed to load font - menu will not be available");
    }
}

void MY_SCENE::onLoop()
{
    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    if (statusText)
    {
        statusText->setText(
            "Mouse(%.0f,%.0f)  Cam2D(%.0f,%.0f)  Cam3D(%.0f,%.0f,%.0f)",
            mouseScreenX, mouseScreenY,
            device->getCamera().position2d.x, device->getCamera().position2d.y,
            device->getCamera().position.x, device->getCamera().position.y, device->getCamera().position.z);
    }
    if (notificationTimer > 0.0f)
    {
        notificationTimer -= device->delta;
        if (notificationTimer <= 0.0f)
        {
            notificationTimer = 0.0f;
            if (notificationText)
                notificationText->enableRender = false;
        }
    }
    for(size_t i = 0; i < menuItems.size(); i++)
    {
        MenuRow& row = menuItems[i];
        if (row.object)
        {
            if(row.object->is3D)
            {
                row.object->angle.y += device->delta * 3.0f;
            }
            else
            {
                row.object->angle.y = 0.0f;
            }
        }
    }
    {
        mbm::RENDERIZABLE* latestObj = (lastLoadedRowIdx >= 0 && lastLoadedRowIdx < static_cast<int>(menuItems.size()))
            ? menuItems[static_cast<size_t>(lastLoadedRowIdx)].object
            : nullptr;
        if (latestObj && shaderInfoText)
        {
            mbm::FX* fx = latestObj->getFx();
            if (fx)
            {
                std::string PixeShader;
                std::string VertexShader;
                std::vector<mbm::VAR_SHADER *> *vars = fx->getVarsPS();
                if (vars)
                    PixeShader = getShaderInfoText(true, vars, fx);
                vars = fx->getVarsVS();
                if (vars)
                    VertexShader = getShaderInfoText(false, vars, fx);
                shaderInfoText->setText((PixeShader + VertexShader).c_str());
            }
            else
            {
                shaderInfoText->setText("");
            }
        }
    }
}

std::string MY_SCENE::getShaderInfoText(const bool isPS, std::vector<mbm::VAR_SHADER *> *vars, mbm::FX* fx)
{
    char text[256];
    std::string finalText;
    snprintf(text, sizeof(text), "%s shader vars: %zu", isPS ? "Pixel" : "Vertex", vars->size());
    finalText += text;
    finalText += "\n";
    float data[4];
    for(mbm::VAR_SHADER* var : *vars)
    {
        if (isPS)
        {
            fx->getVarPShader(var->name.c_str(), data);
        }
        else
        {
            fx->getVarVShader(var->name.c_str(), data);
        }
        switch(var->typeVar)
        {
            case mbm::TYPE_VAR_SHADER::VAR_FLOAT:
            {
                snprintf(text, sizeof(text), "%s[%0.3f]", var->name.c_str(), data[0]);
                break;
            }
            case mbm::TYPE_VAR_SHADER::VAR_VECTOR2:
            {
                snprintf(text, sizeof(text), "%s[%0.3f,%0.3f]", var->name.c_str(), data[0], data[1]);
                break;
            }
            default:
            case mbm::TYPE_VAR_SHADER::VAR_VECTOR:
            case mbm::TYPE_VAR_SHADER::VAR_COLOR_RGB:
            {
                snprintf(text, sizeof(text), "%s[%0.3f,%0.3f,%0.3f]", var->name.c_str(), data[0], data[1], data[2]);
            }
            break;
            case mbm::TYPE_VAR_SHADER::VAR_COLOR_RGBA:
            {
                snprintf(text, sizeof(text), "%s[%0.3f,%0.3f,%0.3f,%0.3f]", var->name.c_str(), data[0], data[1], data[2], data[3]);
            }
            break;
        }
        finalText += text;
        finalText += "\n";
    }
    return finalText;
}

void MY_SCENE::onTouchDown(int key, float x, float y)
{
    INFO_LOG("Touch down key: %d %g %g", key, x, y);
    if (key == 0)
    {
        if (menuVisible && handleMenuTouchDown(x, y))
            return;
        if (posMenuVisible && handlePosMenuTouchDown(x, y))
            return;
        if (shaderMenuVisible && handleShaderMenuTouchDown(x, y))
            return;
        RenderMode mode_selected;
        if(worldMenuVisible && handleWorldMenuTouchDown(x, y, mode_selected))
        {
            if(lastLoadedRowIdx != -1)
            {
                releaseObjectAt(lastLoadedRowIdx);
                loadObjectAt(lastLoadedRowIdx, mode_selected);
            }
        }
    }
    if(key == 1 && render2Texture)
    {
        if(render2Texture->saveAsPNG("render2texture-out.png", 0, 0, render2Texture->widthTexture, render2Texture->heightTexture))
        {
            INFO_LOG("Saved render2Texture content to render2texture-out.png");
        }
        else
        {
            INFO_LOG("Failed to save render2Texture content");
        }
    }
}

void MY_SCENE::onTouchUp(int, float, float)
{
}

void MY_SCENE::onTouchMove(int, float x, float y)
{
    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    mouseScreenX = x;
    mouseScreenY = y;
    if(trackMouse)
    {
        if(trackMouse->is3D)
        {
            device->transformeScreen2dToWorld3d_scaled(x, y, &trackMouse->position, 800.0f);
        }
        else if(trackMouse->is2dS)
        {
            trackMouse->position.x = x;
            trackMouse->position.y = y;
        }
        else
        {
            device->transformeScreen2dToWorld2d_scaled(x, y, trackMouse->position);
        }
    }
    if(lineFontIsOver && lineFontIsOver->getTotalLines() > 0)
    {
        std::vector<mbm::VEC3> linePoints(4);
        bool found = false;

        for (auto& row : menuItems)
        {
            if(row.labelText &&  row.labelText->isOver2ds(device, x, y))
            {
                updateBoundsForTextDraw(row.labelText);
                found = true;
                break;
            }
        }
        if(found == false && hintsText && hintsText->isOver2ds(device, x, y))
        {
            updateBoundsForTextDraw(hintsText);
            found = true;
        }
        for (uint32_t j = 0; j < sizeof(posMenuTexts) / sizeof(posMenuTexts[0]); j++)
        {
            if(posMenuTexts[j] && posMenuTexts[j]->isOver2ds(device, x, y))
            {
                updateBoundsForTextDraw(posMenuTexts[j]);
                found = true;
                break;
            }
        }
        if(found == false && statusText && statusText->isOver2ds(device, x, y))
        {
            updateBoundsForTextDraw(statusText);
            found = true;
        }
        if(found == false && notificationText && notificationText->isOver2ds(device, x, y))
        {
            updateBoundsForTextDraw(notificationText);
            found = true;
        }
        if(found == false  && btn2dS && btn2dS->isOver2ds(device, x, y))
        {
            updateBoundsForTextDraw(btn2dS);
            found = true;
        }
        if(found == false  && btn2dW && btn2dW->isOver2ds(device, x, y))
        {
            updateBoundsForTextDraw(btn2dW);
            found = true;
        }
        if(found == false  && btn3d && btn3d->isOver2ds(device, x, y))
        {
            updateBoundsForTextDraw(btn3d);
            found = true;
        }
        if (!found && shaderRowPS.labelText && shaderRowPS.labelText->isOver2ds(device, x, y))
        {
            updateBoundsForTextDraw(shaderRowPS.labelText);
            found = true;
        }
        if (!found && shaderRowPS.btnPrev && shaderRowPS.btnPrev->isOver2ds(device, x, y))
        {
            updateBoundsForTextDraw(shaderRowPS.btnPrev);
            found = true;
        }
        if (!found && shaderRowPS.btnNext && shaderRowPS.btnNext->isOver2ds(device, x, y))
        {
            updateBoundsForTextDraw(shaderRowPS.btnNext);
            found = true;
        }
        if (!found && shaderRowVS.labelText && shaderRowVS.labelText->isOver2ds(device, x, y))
        {
            updateBoundsForTextDraw(shaderRowVS.labelText);
            found = true;
        }
        if (!found && shaderRowVS.btnPrev && shaderRowVS.btnPrev->isOver2ds(device, x, y))
        {
            updateBoundsForTextDraw(shaderRowVS.btnPrev);
            found = true;
        }
        if (!found && shaderRowVS.btnNext && shaderRowVS.btnNext->isOver2ds(device, x, y))
        {
            updateBoundsForTextDraw(shaderRowVS.btnNext);
            found = true;
        }
        if (!found && shaderBtnPause && shaderBtnPause->isOver2ds(device, x, y))
        {
            updateBoundsForTextDraw(shaderBtnPause);
            found = true;
        }

        if(found)
        {
            lineFontIsOver->enableRender = true;
        }
        else
        {
            lineFontIsOver->enableRender = false;
        }
    }
}

void MY_SCENE::updateBoundsForTextDraw(mbm::TEXT_DRAW* textDraw)
{
    float w = 0, h = 0;
    if (textDraw)
    {
        std::vector<mbm::VEC3> linePoints(4);
        textDraw->getAABB(&w, &h);
        // TEXT_DRAW::position is the top-left starting corner in screen coords (Y-down).
        // getAABB() returns screen-pixel dimensions, so the box spans
        // [position.x .. position.x+w] x [position.y .. position.y+h].
        linePoints[0] = mbm::VEC3(textDraw->position.x,     textDraw->position.y,     textDraw->position.z);
        linePoints[1] = mbm::VEC3(textDraw->position.x + w, textDraw->position.y,     textDraw->position.z);
        linePoints[2] = mbm::VEC3(textDraw->position.x + w, textDraw->position.y + h, textDraw->position.z);
        linePoints[3] = mbm::VEC3(textDraw->position.x,     textDraw->position.y + h, textDraw->position.z);

        lineFontIsOver->set(std::move(linePoints), 0);
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
    printf("Key down: %d\n", key);
    if (key == 77) // M - toggle left menu
    {
        menuVisible = !menuVisible;
        for (size_t i = 0; i < menuItems.size(); i++)
            updateMenuRow(i);
        return;
    }
    if (key == 80) // P - toggle position menu
    {
        posMenuVisible = !posMenuVisible;
        updatePosMenu();
        return;
    }
    if (key == 83) // S - toggle shader menu
    {
        shaderMenuVisible = !shaderMenuVisible;
        updateShaderMenu();
        return;
    }
    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    if (key == 39)      // right
        device->getCamera().position2d.x += 10;
    else if (key == 37) // left
        device->getCamera().position2d.x -= 10;
    else if (key == 38) // up
        device->getCamera().position2d.y += 10;
    else if (key == 40) // down
        device->getCamera().position2d.y -= 10;
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

void MY_SCENE::buildMenu()
{
    struct TypeDef { const char* name; MenuObjectType type; bool s2dS; bool s2dW; bool s3d; };
    static const TypeDef defs[] =
    {
        { "GIF_VIEW",         MenuObjectType::GIF_VIEW,         true,  true,  true  },
        { "TEXTURE_VIEW",     MenuObjectType::TEXTURE_VIEW,     true,  true,  true  },
        { "SPRITE",           MenuObjectType::SPRITE,           true,  true,  true  },
        { "BACKGROUND",       MenuObjectType::BACKGROUND,       false, true,  true  },
        { "MESH",             MenuObjectType::MESH,             true,  true,  true  },
        { "SHAPE_MESH",       MenuObjectType::SHAPE_MESH,       true,  true,  true  },
        { "LINE_MESH",        MenuObjectType::LINE_MESH,        true,  true,  true  },
        { "PARTICLE",         MenuObjectType::PARTICLE,         true,  true,  true  },
        { "STEERED_PARTICLE", MenuObjectType::STEERED_PARTICLE, true,  true,  true  },
        { "RENDER_2_TEXTURE (Prefer 2DW)", MenuObjectType::RENDER_2_TEXTURE, true,  false, false },
        { "TILE",             MenuObjectType::TILE,             true,  true,  true  },
    };

    constexpr bool  IS_2D_FONT  = true;
    constexpr bool  IS_SCREEN   = true;

    for (size_t i = 0; i < sizeof(defs) / sizeof(defs[0]); i++)
    {
        MenuRow row;
        row.typeName    = defs[i].name;
        row.objType     = defs[i].type;
        row.supports2dS = defs[i].s2dS;
        row.supports2dW = defs[i].s2dW;
        row.supports3d  = defs[i].s3d;
        row.currentMode = RenderMode::NONE;
        row.object      = nullptr;

        char labelBuf[64];
        snprintf(labelBuf, sizeof(labelBuf), "[ ] %s", defs[i].name);
        row.labelText = this->fontDrawNoShader->addText(labelBuf, mbm::VEC2(0.0f, 0.0f), IS_2D_FONT, IS_SCREEN);
        row.labelText->scale = mbm::VEC3(1.0f, 1.0f, 1.0f);
        row.labelText->forceCalcSize();
        row.labelText->position.z    = -1.0f;
        row.labelText->alwaysRenderize = true;
        row.labelText->enableRender  = false;

        menuItems.push_back(row);
    }
    float latestY = 0.0f;
    for (size_t i = 0; i < sizeof(defs) / sizeof(defs[0]); i++)
    {
        MenuRow& row = menuItems[i];
        row.labelText->position.x = 10.0f;
        row.labelText->position.y = 10.0f + static_cast<float>(i) * 50.0f;
        latestY = row.labelText->position.y;
    }
    // Hints text — always visible at the bottom of the screen
    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    hintsText = this->fontDrawNoShader->addText("[M] menu | [P] pos | [S] shader | [Arrows] camera", IS_2D_FONT, IS_SCREEN);
    hintsText->scale = mbm::VEC3(0.5f, 0.5f, 0.5f);
    hintsText->forceCalcSize();
    float hw = 0.0f, hh = 0.0f;
    hintsText->getAABB(&hw, &hh);
    hintsText->position.x      = 10.0f;
    hintsText->position.y      = static_cast<float>(device->getBackBufferHeight()) - hh - 5.0f;
    hintsText->position.z      = -1.0f;
    hintsText->alwaysRenderize = true;
    hintsText->enableRender    = true;


    shaderInfoText = this->fontDrawNoShader->addText("Shader information", IS_2D_FONT, IS_SCREEN);
    shaderInfoText->scale = mbm::VEC3(0.5f, 0.5f, 0.5f);
    shaderInfoText->forceCalcSize();
    shaderInfoText->position.x      = 10.0f;
    shaderInfoText->position.y      = latestY + 50.0f;
    shaderInfoText->position.z      = -1.0f;
    shaderInfoText->alwaysRenderize = true;
    shaderInfoText->enableRender    = true;

    // Show initial menu state
    for (size_t i = 0; i < menuItems.size(); i++)
        updateMenuRow(i);
}

void MY_SCENE::updateMenuRow(size_t i)
{
    MenuRow&   row    = menuItems[i];
    const bool loaded = (row.object != nullptr);

    if (loaded)
        row.labelText->setText("[X] %s (%s)", row.typeName, modeToStr(row.currentMode));
    else
        row.labelText->setText("[ ] %s", row.typeName);

    row.labelText->enableRender  = menuVisible;
}

void MY_SCENE::loadObjectAt(size_t i, RenderMode mode)
{
    MenuRow& row = menuItems[i];
    if (row.object != nullptr)
        return;

    const bool is3d  = (mode == RenderMode::WORLD_3D);
    const bool is2dS = (mode == RenderMode::SCREEN_2D);

    switch (row.objType)
    {
        case MenuObjectType::GIF_VIEW:
        {
            gif = new mbm::GIF_VIEW(this, is3d, is2dS);
            if (gif->load("Lion-King.gif", 600, 400))
            {
                INFO_LOG("GIF_VIEW loaded (%s)", modeToStr(mode));
                row.object = gif;
            }
            else
            {
                ERROR_LOG("Failed to load GIF_VIEW");
                delete gif;
                gif = nullptr;
            }
        }
        break;

        case MenuObjectType::TEXTURE_VIEW:
        {
            texBox = new mbm::TEXTURE_VIEW(this, is3d, is2dS);
            if (texBox->load("wooden-box.jpg", 200, 200))
            {
                INFO_LOG("TEXTURE_VIEW loaded (%s)", modeToStr(mode));
                row.object = texBox;
            }
            else
            {
                ERROR_LOG("Failed to load TEXTURE_VIEW");
                delete texBox;
                texBox = nullptr;
            }
        }
        break;

        case MenuObjectType::SPRITE:
        {
            sprite = new mbm::SPRITE(this, is3d, is2dS);
            if (sprite->load("box.spt"))
            {
                INFO_LOG("SPRITE loaded (%s)", modeToStr(mode));
                row.object = sprite;
            }
            else
            {
                ERROR_LOG("Failed to load SPRITE");
                delete sprite;
                sprite = nullptr;
            }
        }
        break;

        case MenuObjectType::BACKGROUND:
        {
            background = new mbm::BACKGROUND(this, is3d);
            if (background->load("ground.png", true, true))
            {
                INFO_LOG("BACKGROUND loaded (%s)", modeToStr(mode));
                row.object = background;
            }
            else
            {
                ERROR_LOG("Failed to load BACKGROUND");
                delete background;
                background = nullptr;
            }
        }
        break;

        case MenuObjectType::MESH:
        {
            mesh = new mbm::MESH(this, is3d, is2dS);
            if (mesh->load("Barrel_NoTop.msh"))
            {
                mesh->scale = mbm::VEC3(3.5f, 3.5f, 3.5f);
                INFO_LOG("MESH loaded (%s)", modeToStr(mode));
                row.object = mesh;
            }
            else
            {
                ERROR_LOG("Failed to load MESH");
                delete mesh;
                mesh = nullptr;
            }
        }
        break;

        case MenuObjectType::SHAPE_MESH:
        {
            shape = new mbm::SHAPE_MESH(this, is3d, is2dS);
            if (shape->loadRectangle("quad", 100, 100, true, 2))
            {
                INFO_LOG("SHAPE_MESH loaded (%s)", modeToStr(mode));
                row.object = shape;
            }
            else
            {
                ERROR_LOG("Failed to load SHAPE_MESH");
                delete shape;
                shape = nullptr;
            }
        }
        break;

        case MenuObjectType::LINE_MESH:
        {
            line = new mbm::LINE_MESH(this, is3d, is2dS);
            if (is3d)
            {
                // Low-poly globe wireframe: R=100, 5 stacks, 8 slices
                static constexpr float PI    = 3.14159265358979f;
                static constexpr float R     = 100.0f;
                static constexpr int   STACKS = 5;
                static constexpr int   SLICES = 8;
                // Latitude rings (exclude poles: stacks-1 inner rings)
                for (int st = 1; st < STACKS; ++st)
                {
                    const float phi = PI * static_cast<float>(st) / static_cast<float>(STACKS);
                    const float y   = R * std::cos(phi);
                    const float r   = R * std::sin(phi);
                    std::vector<mbm::VEC3> ring;
                    ring.reserve(SLICES + 1);
                    for (int sl = 0; sl <= SLICES; ++sl)
                    {
                        const float theta = 2.0f * PI * static_cast<float>(sl) / static_cast<float>(SLICES);
                        ring.push_back(mbm::VEC3(r * std::cos(theta), y, r * std::sin(theta)));
                    }
                    line->add(std::move(ring));
                }
                // Longitude meridians (north pole to south pole)
                for (int sl = 0; sl < SLICES; ++sl)
                {
                    const float theta = 2.0f * PI * static_cast<float>(sl) / static_cast<float>(SLICES);
                    std::vector<mbm::VEC3> meridian;
                    meridian.reserve(STACKS + 1);
                    for (int st = 0; st <= STACKS; ++st)
                    {
                        const float phi = PI * static_cast<float>(st) / static_cast<float>(STACKS);
                        meridian.push_back(mbm::VEC3(
                            R * std::sin(phi) * std::cos(theta),
                            R * std::cos(phi),
                            R * std::sin(phi) * std::sin(theta)));
                    }
                    line->add(std::move(meridian));
                }
            }
            else
            {
                // Square outline with X mark (half-size = 50)
                static constexpr float H = 50.0f;
                // Closed square outline
                std::vector<mbm::VEC3> sq = {
                    mbm::VEC3(-H, -H, 0.0f),
                    mbm::VEC3(-H,  H, 0.0f),
                    mbm::VEC3( H,  H, 0.0f),
                    mbm::VEC3( H, -H, 0.0f),
                    mbm::VEC3(-H, -H, 0.0f)
                };
                line->add(std::move(sq));
                // Diagonal: bottom-left to top-right
                std::vector<mbm::VEC3> d1 = { mbm::VEC3(-H, -H, 0.0f), mbm::VEC3(H, H, 0.0f) };
                line->add(std::move(d1));
                // Diagonal: top-left to bottom-right
                std::vector<mbm::VEC3> d2 = { mbm::VEC3(-H, H, 0.0f), mbm::VEC3(H, -H, 0.0f) };
                line->add(std::move(d2));
            }
            line->enableRender = true;
            INFO_LOG("LINE_MESH loaded (%s)", modeToStr(mode));
            row.object = line;
        }
        break;

        case MenuObjectType::PARTICLE:
        {
            particle = new mbm::PARTICLE(this, is3d, is2dS);
            if (particle->load("particle.png", nullptr, nullptr, 100, true))
            {
                particle->addParticle(1000, true);
                particle->addStage();
                particle->restartAnimationParticle();
                INFO_LOG("PARTICLE loaded (%s)", modeToStr(mode));
                row.object = particle;
            }
            else
            {
                ERROR_LOG("Failed to load PARTICLE");
                delete particle;
                particle = nullptr;
            }
        }
        break;

        case MenuObjectType::STEERED_PARTICLE:
        {
            mbm::INFO_PHYSICS infoPhysics;
            infoPhysics.lsCube.push_back(new mbm::CUBE(200, 200, 200));
            mbm::COLOR colorParticle(1.0f, 0.0f, 0.0f, 1.0f);
            steeredParticle = new mbm::STEERED_PARTICLE(this, is3d, is2dS, false, nullptr);
            if (steeredParticle->load("particle.png", &colorParticle, &infoPhysics))
            {
                steeredParticle->addParticle(1432, steeredParticle->addGroup(&colorParticle) - 1);
                steeredParticle->setRadiusScale(2);
                mbm::FLUID_GROUP* group = steeredParticle->getParticleGroup(0);
                if (group)
                    group->aSizeParticle = 20.0f;
                steeredParticle->restartAnimationParticle();
                steeredParticle->restartAnimation();
                randomSteeredParticlePositions();
                INFO_LOG("STEERED_PARTICLE loaded (%s)", modeToStr(mode));
                row.object = steeredParticle;
            }
            else
            {
                ERROR_LOG("Failed to load STEERED_PARTICLE");
                delete steeredParticle;
                steeredParticle = nullptr;
            }
        }
        break;

        case MenuObjectType::RENDER_2_TEXTURE:
        {
            mbm::DEVICE* device = mbm::DEVICE::getInstance();
            render2Texture      = new mbm::RENDER_2_TEXTURE(this, is3d, is2dS);
            const uint32_t widthFrame = static_cast<uint32_t>(device->getBackBufferWidth() * 0.60f);
            const uint32_t heightFrame = static_cast<uint32_t>(device->getBackBufferHeight() * 0.60f);
            if (render2Texture->load(widthFrame, heightFrame, widthFrame, heightFrame, "my-render", true))
            {
                if (gif)
                    render2Texture->addObject2Render(gif);
                INFO_LOG("RENDER_2_TEXTURE loaded (%s)", modeToStr(mode));
                row.object = render2Texture;
                addObjectsToRender2Texture();
                posMenuSelected = 0; // objects inside r2t always start at origin
            }
            else
            {
                ERROR_LOG("Failed to load RENDER_2_TEXTURE");
                delete render2Texture;
                render2Texture = nullptr;
            }
        }
        break;

        case MenuObjectType::TILE:
        {
            tile = new mbm::TILE(this, is3d, is2dS);
            if (tile->load("tile-map-test.tile"))
            {
                tile->scale = mbm::VEC3(0.3f, 0.3f, 0.3f);
                INFO_LOG("TILE loaded (%s)", modeToStr(mode));
                row.object = tile;
            }
            else
            {
                ERROR_LOG("Failed to load TILE");
                delete tile;
                tile = nullptr;
            }
        }
        break;
    }

    if (row.object)
    {
        row.currentMode  = mode;
        lastLoadedRowIdx = static_cast<int>(i);
        // Ensure loaded 2D objects sit behind menu text (z=-1)
        if (!is3d)
            row.object->position.z = 0.0f;
    }
    addObjectsToRender2Texture();
    updateMenuRow(i);
    applyPosPreset(posMenuSelected);
    if (row.object)
    {
        const bool insideR2T = (render2Texture != nullptr) && (row.object != render2Texture);
        showNotification("%s loaded (%s) %s | pos(%.0f,%.0f,%.0f)",
            row.typeName, modeToStr(mode),
            insideR2T ? "in render2texture" : "in scene",
            row.object->position.x, row.object->position.y, row.object->position.z);
    }
    //Do not apply shader since some object are loaded with shader.
    //applyCurrentShaders();
}

void MY_SCENE::addObjectsToRender2Texture()
{
    if (render2Texture)
    {
        if (gif)
        {
            render2Texture->addObject2Render(gif);
        }
        if (sprite)
        {
            render2Texture->addObject2Render(sprite);
        }
        if (shape)
        {
            render2Texture->addObject2Render(shape);
        }
        if (line)
        {
            render2Texture->addObject2Render(line);
        }
        if (particle)
        {
            render2Texture->addObject2Render(particle);
        }
        if (steeredParticle)
        {
            render2Texture->addObject2Render(steeredParticle);
        }
        if (tile)
        {
            render2Texture->addObject2Render(tile);
        }
        if(background)
        {
            render2Texture->addObject2Render(background);
        }
        if(mesh)
        {
            render2Texture->addObject2Render(mesh);
        }
        if(texture)
        {
            render2Texture->addObject2Render(texture);
        }
        if(particle_ptl)
        {
            render2Texture->addObject2Render(particle_ptl);
        }
        if(hmd)
        {
            render2Texture->addObject2Render(hmd);
        }
        if(texBox)
        {
            render2Texture->addObject2Render(texBox);
        }
    }
}

void MY_SCENE::releaseObjectAt(size_t i)
{
    trackMouse = nullptr;
    MenuRow& row = menuItems[i];
    if (row.object == nullptr)
        return;

    showNotification("%s released", row.typeName);

    if (render2Texture)
        render2Texture->removeObject2Render(row.object);
    switch (row.objType)
    {
        case MenuObjectType::GIF_VIEW:
            delete gif;
            gif = nullptr;
            break;
        case MenuObjectType::TEXTURE_VIEW:
            delete texBox;
            texBox = nullptr;
            break;
        case MenuObjectType::SPRITE:
            delete sprite;
            sprite = nullptr;
            break;
        case MenuObjectType::BACKGROUND:
            delete background;
            background = nullptr;
            break;
        case MenuObjectType::MESH:
            delete mesh;
            mesh = nullptr;
            break;
        case MenuObjectType::SHAPE_MESH:
            delete shape;
            shape = nullptr;
            break;
        case MenuObjectType::LINE_MESH:
            delete line;
            line = nullptr;
            break;
        case MenuObjectType::PARTICLE:
            delete particle;
            particle = nullptr;
            break;
        case MenuObjectType::STEERED_PARTICLE:
            delete steeredParticle;
            steeredParticle = nullptr;
            break;
        case MenuObjectType::RENDER_2_TEXTURE:
            delete render2Texture;
            render2Texture = nullptr;
            updatePosMenu(); // restore full pos menu now that r2t is gone
            break;
        case MenuObjectType::TILE:
            delete tile;
            tile = nullptr;
            break;
    }

    row.object      = nullptr;
    row.currentMode = RenderMode::NONE;
    updateMenuRow(i);
}

bool MY_SCENE::handleMenuTouchDown(float x, float y)
{
    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    for (size_t i = 0; i < menuItems.size(); i++)
    {
        MenuRow& row = menuItems[i];
        if (btn2dS->enableRender && btn2dS->getText().find("[x]") != std::string::npos && row.labelText->isOver2ds(device, x, y))
        {
            if(row.labelText->getText().find("[X]") != std::string::npos)
                releaseObjectAt(i);
            else
                loadObjectAt(i, RenderMode::SCREEN_2D);
            return true;
        }
        if (btn2dW->enableRender && btn2dW->getText().find("[x]") != std::string::npos && row.labelText->isOver2ds(device, x, y))
        {
            if(row.labelText->getText().find("[X]") != std::string::npos)
                releaseObjectAt(i);
            else
                loadObjectAt(i, RenderMode::WORLD_2D);
            return true;
        }
        if (btn3d->enableRender && btn3d->getText().find("[x]") != std::string::npos && row.labelText->isOver2ds(device, x, y))
        {
            if(row.labelText->getText().find("[X]") != std::string::npos)
                releaseObjectAt(i);
            else
                loadObjectAt(i, RenderMode::WORLD_3D);
            return true;
        }
    }
    return false;
}

void MY_SCENE::buildPosMenu()
{
    static const char* const baseLabels[6] =
    {
        "Apply (X=0,Y=0,Z=0)",
        "Apply (Left-Bottom)",
        "Apply (Left-Up)",
        "Apply (Right-Bottom)",
        "Apply (Right-Up)",
        "Apply (Track Mouse)",
    };

    mbm::DEVICE* device     = mbm::DEVICE::getInstance();
    
    float maxWidth  = 0.0f;
    float maxHeight = 0.0f;
    constexpr bool  IS_2D_FONT  = true;
    constexpr bool  IS_SCREEN   = true;

    for (uint32_t j = 0; j < sizeof(posMenuTexts) / sizeof(posMenuTexts[0]); j++)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "%s %s", j == 0 ? "[X]" : "[ ]", baseLabels[j]);
        posMenuTexts[j] = this->fontDrawNoShader->addText(buf, mbm::VEC2(0, 0), IS_2D_FONT, IS_SCREEN);
        posMenuTexts[j]->scale         = mbm::VEC3(1.0f, 1.0f, 1.0f);
        posMenuTexts[j]->forceCalcSize();
        posMenuTexts[j]->position.z    = -1.0f;
        posMenuTexts[j]->alwaysRenderize = true;
        posMenuTexts[j]->enableRender  = true;

        float w = 0.0f, h = 0.0f;
        posMenuTexts[j]->getAABB(&w, &h);
        if (w > maxWidth)
            maxWidth = w;
        if (h > maxHeight)
            maxHeight = h;
    }

    for (uint32_t j = 0; j < sizeof(posMenuTexts) / sizeof(posMenuTexts[0]); j++)
    {
        if (posMenuTexts[j])
        {
            posMenuTexts[j]->position.x = static_cast<float>(device->getBackBufferWidth()) - maxWidth - 10.0f;
            posMenuTexts[j]->position.y = 10.0f + static_cast<float>(j) * (maxHeight + 5.0f);
        }
    }

    // Status text: one line above hints
    float hw = 0.0f, hh = 0.0f;
    if (hintsText)
        hintsText->getAABB(&hw, &hh);
    const float statusY = static_cast<float>(device->getBackBufferHeight()) - hh * 2.0f - 12.0f;
    statusText = this->fontDrawNoShader->addText(
        "Mouse(0,0)  Cam2D(0,0)  Cam3D(0,0,0)", mbm::VEC2(10.0f, statusY), IS_2D_FONT, IS_SCREEN);
    statusText->scale         = mbm::VEC3(0.5f, 0.5f, 0.5f);
    statusText->forceCalcSize();
    statusText->position.z    = -1.0f;
    statusText->alwaysRenderize = true;
    statusText->enableRender  = true;

    // Notification text: one line above statusText
    const float notifyY = statusY - hh - 5.0f;
    notificationText = this->fontDrawNoShader->addText("", mbm::VEC2(10.0f, notifyY), IS_2D_FONT, IS_SCREEN);
    notificationText->scale         = mbm::VEC3(0.5f, 0.5f, 0.5f);
    notificationText->forceCalcSize();
    notificationText->position.z    = -1.0f;
    notificationText->alwaysRenderize = true;
    notificationText->enableRender  = false;
}

void MY_SCENE::buildWorldMenu()
{
    float maxWidth  = 0.0f;
    float maxHeight = 0.0f;
    float hw = 0.0f, hh = 0.0f;
    constexpr bool  IS_2D_FONT  = true;
    constexpr bool  IS_SCREEN   = true;
    btn2dS = this->fontDrawNoShader->addText("[x](2dS)", mbm::VEC2(0.0f, 0.0f), IS_2D_FONT, IS_SCREEN);
    btn2dS->scale = mbm::VEC3(1.0f, 1.0f, 1.0f);
    btn2dS->forceCalcSize();
    btn2dS->position.z    = -1.0f;
    btn2dS->alwaysRenderize = true;
    btn2dS->enableRender  = true;
    btn2dS->getAABB(&maxWidth, &maxHeight);

    btn2dW = this->fontDrawNoShader->addText("[ ](2dW)", mbm::VEC2(0.0f, 0.0f), IS_2D_FONT, IS_SCREEN);
    btn2dW->scale = mbm::VEC3(1.0f, 1.0f, 1.0f);
    btn2dW->forceCalcSize();
    btn2dW->position.z    = -1.0f;
    btn2dW->alwaysRenderize = true;
    btn2dW->enableRender  = true;
    btn2dW->getAABB(&hw, &hh);
    if (hw > maxWidth)        
    {
        maxWidth = hw;
    }
    if (hh > maxHeight)       
    {
        maxHeight = hh;
    }

    btn3d = this->fontDrawNoShader->addText("[ ](3d)", mbm::VEC2(0.0f, 0.0f), IS_2D_FONT, IS_SCREEN);
    btn3d->scale = mbm::VEC3(1.0f, 1.0f, 1.0f);
    btn3d->forceCalcSize();
    btn3d->position.z    = -1.0f;
    btn3d->alwaysRenderize = true;
    btn3d->enableRender  = true;
    btn3d->getAABB(&hw, &hh);
    if (hw > maxWidth)        
    {
        maxWidth = hw;
    }
    if (hh > maxHeight)       
    {
        maxHeight = hh;
    }

    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    float widthM = 0;
    float heightM = 0;
    btn3d->getWidthHeightString(&widthM, &heightM,"M");
    const float defaultPosY = device->getBackBufferHeight() - maxHeight;
    btn2dS->position.x  = device->getBackBufferWidth() - (maxWidth * 3) - widthM;
    btn2dS->position.y  = defaultPosY;
    btn2dW->position.x  = device->getBackBufferWidth() - (maxWidth * 2) - widthM;
    btn2dW->position.y  = defaultPosY;
    btn3d->position.x   = device->getBackBufferWidth() - maxWidth;
    btn3d->position.y   = defaultPosY;
}

void MY_SCENE::updatePosMenu()
{
    static const char* const baseLabels[6] =
    {
        "Apply (X=0,Y=0,Z=0)",
        "Apply (Left-Bottom)",
        "Apply (Left-Up)",
        "Apply (Right-Bottom)",
        "Apply (Right-Up)",
        "Apply (Track Mouse)",
    };
    for (uint32_t j = 0; j < sizeof(posMenuTexts) / sizeof(posMenuTexts[0]); j++)
    {
        if (!posMenuTexts[j])
            continue;
        posMenuTexts[j]->setText("%s %s", static_cast<size_t>(j) == static_cast<size_t>(posMenuSelected) ? "[X]" : "[ ]", baseLabels[j]);
        // If render2Texture is active, only show the "Apply (X=0,Y=0,Z=0)" preset since the others don't make sense inside the texture frame
        // Uncommenting the line below will show all presets, but they will all apply the position based on the main screen dimensions, which can be confusing when the object is inside render2texture
        //posMenuTexts[j]->enableRender = posMenuVisible && (render2Texture == nullptr || j == 0);
    }
}

void MY_SCENE::applyPosPreset(int idx)
{
    posMenuSelected = idx;
    updatePosMenu();

    if (lastLoadedRowIdx < 0 || lastLoadedRowIdx >= static_cast<int>(menuItems.size()))
        return;

    MenuRow& row = menuItems[static_cast<size_t>(lastLoadedRowIdx)];
    if (!row.object || row.currentMode == RenderMode::NONE)
        return;

    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    float w = 0.0f, h = 0.0f;
    row.object->getAABB(&w, &h);

    if (idx == 0) // explicit origin
    {
        row.object->position.x = 0.0f;
        row.object->position.y = 0.0f;
        row.object->position.z = (row.currentMode == RenderMode::WORLD_3D) ? 0.0f : row.object->position.z;
        return;
    }

    trackMouse = nullptr;
    // Compute desired screen-space anchor
    float sx = 0.0f, sy = 0.0f;
    float backBufferHeight = static_cast<float>(device->getBackBufferHeight());
    float backBufferWidth = static_cast<float>(device->getBackBufferWidth());
    if(render2Texture && row.object != render2Texture)
    {
        // If the object is inside render2Texture, use its dimensions instead of the device backbuffer for positioning
        backBufferHeight = static_cast<float>(render2Texture->heightTexture);
        backBufferWidth = static_cast<float>(render2Texture->widthTexture);
    }
    switch (idx)
    {
        case 1: // Left-Bottom
            sx = w / 2.0f;
            sy = backBufferHeight - h / 2.0f;
            break;
        case 2: // Left-Up
            sx = w / 2.0f;
            sy = h / 2.0f;
            break;
        case 3: // Right-Bottom
            sx = backBufferWidth - w / 2.0f;
            sy = backBufferHeight - h / 2.0f;
            break;
        case 4: // Right-Up
            sx = backBufferWidth - w / 2.0f;
            sy = h / 2.0f;
            break;
        case 5: // Track Mouse
            trackMouse = row.object;
            break;
        default:
            return;
    }

    const float savedZ = row.object->position.z;
    const bool insideR2T = (render2Texture != nullptr) && (row.object != render2Texture);
    if (row.currentMode == RenderMode::SCREEN_2D)
    {
        if (insideR2T)
        {
            // render2texture camera uses matrixOrthoLH(tw, th). The SCREEN_2D render path
            // calls transformeScreen2dToWorld2d_scaled(position) at draw time, then applies
            // matrixPerspective2d. We need world = (sx - tw/2, -(sy - th/2)), so store the
            // main-screen-equivalent pixel coords that produce those world coords.
            row.object->position.x = device->getBackBufferWidth() * 0.5f + sx - backBufferWidth * 0.5f;
            row.object->position.y = device->getBackBufferHeight() * 0.5f - backBufferHeight * 0.5f + sy;
        }
        else
        {
            row.object->position.x = sx;
            row.object->position.y = sy;
        }
        row.object->position.z = savedZ;
    }
    else if (row.currentMode == RenderMode::WORLD_2D)
    {
        if (insideR2T)
        {
            // render2texture camera uses matrixOrthoLH(tw, th) which maps [-tw/2, tw/2] to
            // clip space. WORLD_2D render path uses position directly — place in texture world.
            row.object->position.x = sx - backBufferWidth * 0.5f;
            row.object->position.y = -(sy - backBufferHeight * 0.5f);
            row.object->position.z = savedZ;
        }
        else
        {
            device->transformeScreen2dToWorld2d_scaled(sx, sy, row.object->position);
            row.object->position.z = savedZ;
        }
    }
    else // WORLD_3D
    {
        device->transformeScreen2dToWorld3d_scaled(sx, sy, &row.object->position, 800.0f);
        // z is determined by the 3D transform — intentional (how far)
    }
}

bool MY_SCENE::handleWorldMenuTouchDown(float x, float y, RenderMode& mode_selected)
{
    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    if(btn3d && btn3d->enableRender && btn3d->isOver2ds(device, x, y))
    {
        btn2dS->setText("[ ](2dS)");
        btn2dW->setText("[ ](2dW)");
        btn3d->setText("[x](3d)");
        mode_selected = RenderMode::WORLD_3D;
        return true;
    }

    if(btn2dW && btn2dW->enableRender && btn2dW->isOver2ds(device, x, y))
    {
        btn2dS->setText("[ ](2dS)");
        btn2dW->setText("[x](2dW)");
        btn3d->setText("[ ](3d)");
        mode_selected = RenderMode::WORLD_2D;
        return true;
    }

    if(btn2dS && btn2dS->enableRender && btn2dS->isOver2ds(device, x, y))
    {
        btn2dS->setText("[x](2dS)");
        btn2dW->setText("[ ](2dW)");
        btn3d->setText("[ ](3d)");
        mode_selected = RenderMode::SCREEN_2D;
        return true;
    }
    mode_selected = RenderMode::NONE;
    return false;
}

bool MY_SCENE::handlePosMenuTouchDown(float x, float y)
{
    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    for (uint32_t j = 0; j < sizeof(posMenuTexts) / sizeof(posMenuTexts[0]); j++)
    {
        if (posMenuTexts[j] && posMenuTexts[j]->enableRender &&
            posMenuTexts[j]->isOver2ds(device, x, y))
        {
            applyPosPreset(j);
            return true;
        }
    }
    return false;
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
            std::uniform_real_distribution<float> disX(-static_cast<float>(device->getBackBufferWidth()) * 0.25f, static_cast<float>(device->getBackBufferWidth()) * 0.25f);
            std::uniform_real_distribution<float> disY(-static_cast<float>(device->getBackBufferHeight()) * 0.25f, static_cast<float>(device->getBackBufferHeight()) * 0.25f);

            for (uint32_t i = 0; i < group->size_particle_array; i++)
            {
                float randomX = disX(gen);
                float randomY = disY(gen);
                group->particle_positions[i] = mbm::VEC3(randomX, randomY, 0);
            }
        }
    }
}

void MY_SCENE::showNotification(const char* fmt, ...)
{
    if (!notificationText)
        return;
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    notificationText->setText("%s", buf);
    notificationText->enableRender = true;
    notificationTimer = 5.0f;
}

void MY_SCENE::buildShaderMenu()
{
    constexpr bool IS_2D_FONT = true;
    constexpr bool IS_SCREEN  = true;
    mbm::DEVICE* device = mbm::DEVICE::getInstance();

    auto makeText = [&](const char* txt) -> mbm::TEXT_DRAW*
    {
        mbm::TEXT_DRAW* t = this->fontDrawNoShader->addText(txt, mbm::VEC2(0.0f, 0.0f), IS_2D_FONT, IS_SCREEN);
        t->scale           = mbm::VEC3(0.75f, 0.75f, 0.75f);
        t->forceCalcSize();
        t->position.z      = -1.0f;
        t->alwaysRenderize = true;
        t->enableRender    = true;// initially visible
        return t;
    };

    shaderRowPS.labelText = makeText("[ ] (PS none)");
    shaderRowPS.btnPrev   = makeText("<");
    shaderRowPS.btnNext   = makeText(">");
    shaderRowVS.labelText = makeText("[ ] (VS none)");
    shaderRowVS.btnPrev   = makeText("<");
    shaderRowVS.btnNext   = makeText(">");
    shaderBtnPause        = makeText("[ ] Pause shader animation");

    // Measure the widest possible label across all ps/vs shader names
    float maxLabelW = 0.0f;
    for (auto* s : device->getShaderConfig().lsPs)
    {
        shaderRowPS.labelText->setText("[X] %s", s->fileName.c_str());
        shaderRowPS.labelText->forceCalcSize();
        float w = 0.0f, h = 0.0f;
        shaderRowPS.labelText->getAABB(&w, &h);
        if (w > maxLabelW) maxLabelW = w;
    }
    for (auto* s : device->getShaderConfig().lsVs)
    {
        shaderRowPS.labelText->setText("[X] %s", s->fileName.c_str());
        shaderRowPS.labelText->forceCalcSize();
        float w = 0.0f, h = 0.0f;
        shaderRowPS.labelText->getAABB(&w, &h);
        if (w > maxLabelW) maxLabelW = w;
    }
    shaderRowPS.labelText->setText("[ ] (PS none)");
    shaderRowPS.labelText->forceCalcSize();

    // Measure < > button widths and row height
    float prevW = 0.0f, rowH = 0.0f;
    shaderRowPS.btnPrev->getAABB(&prevW, &rowH);
    float nextW = 0.0f, tmp = 0.0f;
    shaderRowPS.btnNext->getAABB(&nextW, &tmp);

    // Measure pause button height
    float pauseW = 0.0f, pauseH = 0.0f;
    shaderBtnPause->getAABB(&pauseW, &pauseH);
    if (pauseH > rowH) rowH = pauseH;

    const float gap       = 8.0f;
    const float rightEdge = static_cast<float>(device->getBackBufferWidth()) - 10.0f;
    const float labelX    = rightEdge - nextW - gap - prevW - gap - maxLabelW;
    const float prevX     = rightEdge - nextW - gap - prevW;
    const float nextX     = rightEdge - nextW;

    const float spacing    = rowH + 8.0f;
    //float rowY = static_cast<float>(device->getBackBufferHeight()) / 2.0f - totalMenuH / 2.0f;
    float rowY = 510.0f;

    shaderRowPS.labelText->position.x = labelX;
    shaderRowPS.labelText->position.y = rowY;
    shaderRowPS.btnPrev->position.x   = prevX;
    shaderRowPS.btnPrev->position.y   = rowY;
    shaderRowPS.btnNext->position.x   = nextX;
    shaderRowPS.btnNext->position.y   = rowY;
    rowY += spacing;

    shaderRowVS.labelText->position.x = labelX;
    shaderRowVS.labelText->position.y = rowY;
    shaderRowVS.btnPrev->position.x   = prevX;
    shaderRowVS.btnPrev->position.y   = rowY;
    shaderRowVS.btnNext->position.x   = nextX;
    shaderRowVS.btnNext->position.y   = rowY;
    rowY += spacing;

    shaderBtnPause->position.x = labelX;
    shaderBtnPause->position.y = rowY;
}

void MY_SCENE::updateShaderMenu()
{
    if (!shaderRowPS.labelText)
        return;

    mbm::DEVICE* device  = mbm::DEVICE::getInstance();
    const bool   visible = shaderMenuVisible;

    if (currentPsShaderIdx >= 0 && currentPsShaderIdx < static_cast<int>(device->getShaderConfig().lsPs.size()))
        shaderRowPS.labelText->setText("[X] %s", device->getShaderConfig().lsPs[static_cast<size_t>(currentPsShaderIdx)]->fileName.c_str());
    else
        shaderRowPS.labelText->setText("[ ] (PS none)");
    shaderRowPS.labelText->enableRender = visible;
    shaderRowPS.btnPrev->enableRender   = visible;
    shaderRowPS.btnNext->enableRender   = visible;

    if (currentVsShaderIdx >= 0 && currentVsShaderIdx < static_cast<int>(device->getShaderConfig().lsVs.size()))
        shaderRowVS.labelText->setText("[X] %s", device->getShaderConfig().lsVs[static_cast<size_t>(currentVsShaderIdx)]->fileName.c_str());
    else
        shaderRowVS.labelText->setText("[ ] (VS none)");
    shaderRowVS.labelText->enableRender = visible;
    shaderRowVS.btnPrev->enableRender   = visible;
    shaderRowVS.btnNext->enableRender   = visible;

    bool paused = false;
    if (lastLoadedRowIdx >= 0 && lastLoadedRowIdx < static_cast<int>(menuItems.size()))
    {
        mbm::RENDERIZABLE* obj = menuItems[static_cast<size_t>(lastLoadedRowIdx)].object;
        if (obj)
        {
            mbm::FX* fx = obj->getFx();
            if (fx)
                paused = (fx->getTypePS() == mbm::TYPE_ANIMATION_PAUSED &&
                          fx->getTypeVS() == mbm::TYPE_ANIMATION_PAUSED);
        }
    }
    shaderBtnPause->setText(paused ? "[X] Pause shader animation" : "[ ] Pause shader animation");
    shaderBtnPause->enableRender = visible;
}

void MY_SCENE::applyCurrentShaders()
{
    if (lastLoadedRowIdx < 0 || lastLoadedRowIdx >= static_cast<int>(menuItems.size()))
        return;
    mbm::RENDERIZABLE* obj = menuItems[static_cast<size_t>(lastLoadedRowIdx)].object;
    if (!obj)
        return;
    mbm::FX* fx = obj->getFx();
    if (!fx)
        return;
    if (currentPsShaderIdx < 0 && currentVsShaderIdx < 0)
    {
        if(obj->typeClass != mbm::TYPE_CLASS_STEERED_PARTICLE && obj->typeClass != mbm::TYPE_CLASS_PARTICLE)
        {
            mbm::SHADER_CFG* psCfg = nullptr;
            mbm::SHADER_CFG* vsCfg = nullptr;
            fx->loadNewShader(psCfg, vsCfg,
                        mbm::TYPE_ANIMATION_PAUSED, 1.0f,
                        mbm::TYPE_ANIMATION_PAUSED, 1.0f,
                        obj->getFvfFromBuffer());
        }
        updateShaderMenu();
        return;
    }

    mbm::DEVICE*     device = mbm::DEVICE::getInstance();
    mbm::SHADER_CFG* psCfg  = (currentPsShaderIdx >= 0 && currentPsShaderIdx < static_cast<int>(device->getShaderConfig().lsPs.size()))
                               ? device->getShaderConfig().lsPs[static_cast<size_t>(currentPsShaderIdx)] : nullptr;
    mbm::SHADER_CFG* vsCfg  = (currentVsShaderIdx >= 0 && currentVsShaderIdx < static_cast<int>(device->getShaderConfig().lsVs.size()))
                               ? device->getShaderConfig().lsVs[static_cast<size_t>(currentVsShaderIdx)] : nullptr;

    fx->loadNewShader(psCfg, vsCfg,
                      mbm::TYPE_ANIMATION_GROWING_LOOP, 1.0f,
                      mbm::TYPE_ANIMATION_GROWING_LOOP, 1.0f,
                      obj->getFvfFromBuffer());
    updateShaderMenu();
}

bool MY_SCENE::handleShaderMenuTouchDown(float x, float y)
{
    if (!shaderRowPS.labelText)
        return false;

    mbm::DEVICE* device  = mbm::DEVICE::getInstance();
    const int    psCount = static_cast<int>(device->getShaderConfig().lsPs.size());
    const int    vsCount = static_cast<int>(device->getShaderConfig().lsVs.size());

    if (shaderRowPS.btnPrev->enableRender && shaderRowPS.btnPrev->isOver2ds(device, x, y))
    {
        if (psCount > 0)
        {
            currentPsShaderIdx = (currentPsShaderIdx <= 0) ? psCount - 1 : currentPsShaderIdx - 1;
            applyCurrentShaders();
        }
        return true;
    }
    if (shaderRowPS.btnNext->enableRender && shaderRowPS.btnNext->isOver2ds(device, x, y))
    {
        if (psCount > 0)
        {
            currentPsShaderIdx = (currentPsShaderIdx + 1) % psCount;
            applyCurrentShaders();
        }
        return true;
    }
    if (shaderRowPS.labelText->enableRender && shaderRowPS.labelText->isOver2ds(device, x, y))
    {
        currentPsShaderIdx = (currentPsShaderIdx >= 0) ? -1 : (psCount > 0 ? 0 : -1);
        applyCurrentShaders();
        return true;
    }

    if (shaderRowVS.btnPrev->enableRender && shaderRowVS.btnPrev->isOver2ds(device, x, y))
    {
        if (vsCount > 0)
        {
            currentVsShaderIdx = (currentVsShaderIdx <= 0) ? vsCount - 1 : currentVsShaderIdx - 1;
            applyCurrentShaders();
        }
        return true;
    }
    if (shaderRowVS.btnNext->enableRender && shaderRowVS.btnNext->isOver2ds(device, x, y))
    {
        if (vsCount > 0)
        {
            currentVsShaderIdx = (currentVsShaderIdx + 1) % vsCount;
            applyCurrentShaders();
        }
        return true;
    }
    if (shaderRowVS.labelText->enableRender && shaderRowVS.labelText->isOver2ds(device, x, y))
    {
        currentVsShaderIdx = (currentVsShaderIdx >= 0) ? -1 : (vsCount > 0 ? 0 : -1);
        applyCurrentShaders();
        return true;
    }

    if (shaderBtnPause->enableRender && shaderBtnPause->isOver2ds(device, x, y))
    {
        if (lastLoadedRowIdx >= 0 && lastLoadedRowIdx < static_cast<int>(menuItems.size()))
        {
            mbm::RENDERIZABLE* obj = menuItems[static_cast<size_t>(lastLoadedRowIdx)].object;
            if (obj)
            {
                mbm::FX* fx = obj->getFx();
                if (fx)
                {
                    const bool isPaused = (fx->getTypePS() == mbm::TYPE_ANIMATION_PAUSED &&
                                          fx->getTypeVS() == mbm::TYPE_ANIMATION_PAUSED);
                    if (isPaused)
                    {
                        fx->setTypePS(mbm::TYPE_ANIMATION_GROWING_LOOP);
                        fx->setTypeVS(mbm::TYPE_ANIMATION_GROWING_LOOP);
                    }
                    else
                    {
                        fx->setTypePS(mbm::TYPE_ANIMATION_PAUSED);
                        fx->setTypeVS(mbm::TYPE_ANIMATION_PAUSED);
                    }
                }
            }
        }
        updateShaderMenu();
        return true;
    }

    return false;
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
