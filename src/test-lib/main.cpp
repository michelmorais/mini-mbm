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

#if defined _WIN32
// libraries necessary
#pragma comment(lib, "core_mbm.lib")
//#pragma comment(lib, "box2d.lib")      // optional (if you include it, you might have to change the dependency of lib to core_mbm instead of mini-mbm (windows only))
//#pragma comment(lib, "bullet2.84.lib") // optional (if you include it, you might have to change the dependency of lib to core_mbm instead of mini-mbm (windows only)) 
#endif

#include "my-scene-test.h"
#include "skeletal-foundation-tests.h"
#include "gles-skeletal-parity-tests.h"
#include <cstdlib>
#include <cstring>

// Usage: testLib --skeletal-foundation-tests
//        testLib --gles-dqs-shader-test
//        testLib --gles-skeletal-parity-test
//        testLib --metal-editor-shader-test
//        testLib [seconds] [mesh_file] [world] [lbs|dqs|auto] [gpu|cpu|auto]
//   seconds    Exit on its own once this many seconds have elapsed in the
//              render loop, instead of running forever. Meant for
//              agent-driven / CI test runs, where nothing is present to
//              press a key or close the window. Pass 0 to keep running
//              indefinitely while still setting mesh_file/world below.
//   mesh_file  Optional .msh to preload immediately in onInitScene(), via
//              the same path the interactive MESH menu row uses, so a mesh
//              feature can be verified without driving the mouse-only menu.
//              Looked up via the engine's normal asset search paths (see
//              util::addPath) — same rules as the interactive menu.
//   world      Coordinate space for mesh_file: "2ds", "2dw", or "3d"
//              (default "3d" when mesh_file is given but world is omitted).
int main(int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--skeletal-foundation-tests") == 0)
        return runSkeletalFoundationTests();

    GAME game;
#if defined(USE_METAL)
    if (argc == 2 && std::strcmp(argv[1], "--metal-editor-shader-test") == 0)
    {
        game.myScene.testMetalEditorShaders = true;
        game.myScene.testTimeoutSeconds = 1.0f;
    }
    else
#endif
#if defined(USE_OPENGL_ES)
    if (argc == 2 && std::strcmp(argv[1], "--gles-dqs-shader-test") == 0)
    {
        game.myScene.testGlesDqsShader = true;
        game.myScene.testTimeoutSeconds = 1.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--gles-skeletal-parity-test") == 0)
    {
        game.myScene.testGlesSkeletalParity = true;
        game.myScene.testTimeoutSeconds = 1.0f;
    }
    else
#endif
    if (argc > 1)
    {
        const float seconds = static_cast<float>(std::atof(argv[1]));
        if (seconds > 0.0f)
            game.myScene.testTimeoutSeconds = seconds;
    }
    if (argc > 2)
    {
        game.myScene.cliMeshFile = argv[2];
        RenderMode mode = RenderMode::WORLD_3D;
        if (argc > 3)
        {
            if (strcmp(argv[3], "2ds") == 0)
                mode = RenderMode::SCREEN_2D;
            else if (strcmp(argv[3], "2dw") == 0)
                mode = RenderMode::WORLD_2D;
            else if (strcmp(argv[3], "3d") == 0)
                mode = RenderMode::WORLD_3D;
        }
        game.myScene.cliMeshMode = mode;
        if (argc > 4)
        {
            if (strcmp(argv[4], "dqs") == 0)
                game.myScene.cliSkeletalMethod = mbm::SKELETAL_SHADER_METHOD::DQS_RIGID;
            else if (strcmp(argv[4], "auto") == 0)
                game.myScene.cliSkeletalMethod = mbm::SKELETAL_SHADER_METHOD::AUTO;
        }
        if (argc > 5)
        {
            if (strcmp(argv[5], "cpu") == 0)
            {
                game.myScene.cliSkeletalExecutionPath = mbm::SKELETAL_EXECUTION_PATH::CPU;
                game.myScene.cliSkeletalExecutionPathSet = true;
            }
            else if (strcmp(argv[5], "gpu") == 0)
            {
                game.myScene.cliSkeletalExecutionPath = mbm::SKELETAL_EXECUTION_PATH::GPU;
                game.myScene.cliSkeletalExecutionPathSet = true;
            }
            else if (strcmp(argv[5], "auto") == 0)
            {
                game.myScene.cliSkeletalExecutionPath = mbm::SKELETAL_EXECUTION_PATH::AUTO;
                game.myScene.cliSkeletalExecutionPathSet = true;
            }
        }
    }
	// this is workaround where  (false, false) the engine does not use default shaders when no shader is set in the objects (so, no shader is used, mostlly in directx)
    game.setUsageOfDefaultPS_VS_WhenNoShader(true, true);
    constexpr bool singleLoop    = false;
    constexpr bool doSwapBuffers = true;
    if(game.initGraphics("Hello-world", 1600, 900, 100, 100, true, true))
    {
        const int result = game.onLoop(singleLoop, doSwapBuffers);
        return game.myScene.automatedTestFailed ? -1 : result;
    }
    return -1;
}
