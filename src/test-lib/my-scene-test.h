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

#ifndef MY_SCENE_TEST_H
#define MY_SCENE_TEST_H

#include <core_mbm/scene.h>
#include <render/texture-view.h>
#include <core_mbm/core-manager.h>
#include <render/gif-view.h>
#include <render/sprite.h>
#include <render/mesh.h>
#include <render/shape-mesh.h>
#include <render/line-mesh.h>
#include <render/particle.h>
#include <render/render-2-texture.h>
#include <render/steered_particle.h>
#include <render/background.h>
#include <render/font.h>
#include <render/HMD.h>
#include <core_mbm/mesh-manager.h> // mesh debug
#include <render/tile.h>
#include <vector>
#include <string>

enum class RenderMode { NONE, SCREEN_2D, WORLD_2D, WORLD_3D };

enum class MenuObjectType
{
    GIF_VIEW,
    TEXTURE_VIEW,
    SPRITE,
    BACKGROUND,
    MESH,
    SHAPE_MESH,
    LINE_MESH,
    PARTICLE,
    STEERED_PARTICLE,
    RENDER_2_TEXTURE,
    TILE
};

struct MenuRow
{
    const char*         typeName;
    MenuObjectType      objType;
    bool                supports2dS;
    bool                supports2dW;
    bool                supports3d;
    RenderMode          currentMode;
    mbm::TEXT_DRAW*     labelText;    // "[ ] TYPE" or "[X] TYPE (MODE)"
    mbm::RENDERIZABLE*  object;       // non-owning view; nullptr when unloaded
};

struct ShaderMenuRow
{
    mbm::TEXT_DRAW* labelText;  // "[X] name.ps" or "[ ] (none)"
    mbm::TEXT_DRAW* btnPrev;    // "<"
    mbm::TEXT_DRAW* btnNext;    // ">"
};

class MY_SCENE : public mbm::SCENE
{
  public:
    // Owned renderizable objects (nullptr when unloaded)
    mbm::TEXTURE_VIEW*      texBox;
    mbm::GIF_VIEW*          gif;
    mbm::SPRITE*            sprite;
    mbm::BACKGROUND*        background;
    mbm::MESH*              mesh;
    mbm::SHAPE_MESH*        shape;
    mbm::LINE_MESH*         line;
    mbm::LINE_MESH*         lineFontIsOver;
    mbm::PARTICLE*          particle;
    mbm::PARTICLE*          particle_ptl;
    mbm::RENDER_2_TEXTURE*  render2Texture;
    mbm::STEERED_PARTICLE*  steeredParticle;
    mbm::FONT_DRAW*         fontDrawNoShader;
    mbm::MESH_MBM_DEBUG     meshDebug;
    mbm::HMD*               hmd;
    mbm::TILE*              tile;
    mbm::TEXTURE_VIEW*      texture;
    mbm::RENDERIZABLE*      trackMouse;

    // Menu
    std::vector<MenuRow>    menuItems;
    bool                    menuVisible;
    mbm::TEXT_DRAW*         hintsText;     // always-visible keyboard shortcut help
    mbm::TEXT_DRAW*         shaderInfoText;
    mbm::TEXT_DRAW*     btn2dS;       // "(2dS)"
    mbm::TEXT_DRAW*     btn2dW;       // "(2dW)"
    mbm::TEXT_DRAW*     btn3d;        // "(3d)"

    // Shader test menu (right-center)
    ShaderMenuRow           shaderRowPS;
    ShaderMenuRow           shaderRowVS;
    mbm::TEXT_DRAW*         shaderBtnPause;
    bool                    shaderMenuVisible;
    int                     currentPsShaderIdx;  // -1 = none
    int                     currentVsShaderIdx;  // -1 = none

    // Position preset right menu
    mbm::TEXT_DRAW*         posMenuTexts[6];
    int                     posMenuSelected;
    bool                    posMenuVisible;
    bool                    worldMenuVisible;
    int                     lastLoadedRowIdx;  // -1 if none

    // Status display
    mbm::TEXT_DRAW*         statusText;
    float                   mouseScreenX;
    float                   mouseScreenY;

    // Timed notification (5 seconds)
    mbm::TEXT_DRAW*         notificationText;
    float                   notificationTimer;

    // Automated-test exit: when >= 0, onLoop() we  quit the loop exiting the application
    // testElapsedSeconds reaches testTimeoutSeconds. Set via main()'s optional
    // <seconds> command-line argument so agent-driven test runs are not stuck
    // waiting forever in the render loop.
    float                   testTimeoutSeconds;
    float                   testElapsedSeconds;

    // Automated-test mesh preload: when cliMeshMode != RenderMode::NONE, onInitScene()
    // loads cliMeshFile (or the default "Crate.msh" when empty) into that world/coord
    // space right away via the same loadObjectAt() path the interactive menu uses, so
    // an agent can verify a specific mesh without driving the mouse-only menu UI.
    // Set via main()'s optional <mesh_file> <world> command-line arguments.
    std::string             cliMeshFile;
    RenderMode              cliMeshMode;
    mbm::SKELETAL_SHADER_METHOD cliSkeletalMethod;
    mbm::SKELETAL_EXECUTION_PATH cliSkeletalExecutionPath;
    bool                    cliSkeletalExecutionPathSet;
    bool                    testGlesDqsShader;
    bool                    testGlesSkeletalParity;
    bool                    testMetalEditorShaders;
    bool                    automatedTestFailed;

    MY_SCENE();
    virtual ~MY_SCENE();
    void startLoading();
    void endLoading();
    void onInitScene();
    void onLoop();
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
    void onInfoDeviceJoystick(int player, int maxNumberButton, const char* strDeviceName, const char* extraInfo);
    void onResizeWindow();

  private:
    void buildMenu();
    void buildPosMenu();
    void buildWorldMenu();
    void updateMenuRow(size_t i);
    void updatePosMenu();
    void loadObjectAt(size_t i, RenderMode mode);
    void releaseObjectAt(size_t i);
    void applyPosPreset(int idx);
    bool handleMenuTouchDown(float x, float y);
    bool handlePosMenuTouchDown(float x, float y);
    bool handleWorldMenuTouchDown(float x, float y, RenderMode &mode_selected);
    void randomSteeredParticlePositions();
    void addObjectsToRender2Texture();
    void showNotification(const char* fmt, ...);
    void updateBoundsForTextDraw(mbm::TEXT_DRAW* textDraw);
    std::string getShaderInfoText(const bool isPS, std::vector<mbm::VAR_SHADER *> *vars, mbm::FX* fx);
    void buildShaderMenu();
    void updateShaderMenu();
    bool handleShaderMenuTouchDown(float x, float y);
    void applyCurrentShaders();
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
