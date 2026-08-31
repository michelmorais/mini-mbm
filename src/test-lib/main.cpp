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
#include "directx9-skeletal-parity-tests.h"
#include "directx11-skeletal-parity-tests.h"
#include <cstdlib>
#include <cstring>
#if defined(USE_DIRECTX11)
#include <specific-directx11-context.h>
#include <core_mbm/device.h>
#include <core_mbm/util-interface.h>
#include <vector>
#endif

#if defined(USE_DIRECTX11)
namespace
{
    struct DIRECTX11_LIFECYCLE_DEBUG
    {
        ID3D11Debug *debug = nullptr;
        ID3D11InfoQueue *infoQueue = nullptr;
    };

    bool validateDirectX11DebugMessages()
    {
#if defined(_DEBUG)
        mbm::DEVICE *device = mbm::DEVICE::getInstance();
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        if (!context || !context->device || !context->immediateContext)
        {
            ERROR_LOG("testLib: DirectX 11 debug-message validation has no active device");
            return false;
        }
        context->immediateContext->Flush();
        ID3D11InfoQueue *infoQueue = nullptr;
        if (FAILED(context->device->QueryInterface(__uuidof(ID3D11InfoQueue),
                                                   reinterpret_cast<void **>(&infoQueue))) || !infoQueue)
        {
            ERROR_LOG("testLib: DirectX 11 debug layer is unavailable; install Windows Graphics Tools");
            return false;
        }
        uint64_t failureCount = 0;
        const uint64_t messageCount = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
        for (uint64_t index = 0; index < messageCount; ++index)
        {
            SIZE_T messageSize = 0;
            if (FAILED(infoQueue->GetMessage(index, nullptr, &messageSize)) || !messageSize)
                continue;
            std::vector<uint8_t> messageStorage(messageSize);
            D3D11_MESSAGE *message = reinterpret_cast<D3D11_MESSAGE *>(messageStorage.data());
            if (FAILED(infoQueue->GetMessage(index, message, &messageSize)))
                continue;
            if (message->Severity != D3D11_MESSAGE_SEVERITY_CORRUPTION &&
                message->Severity != D3D11_MESSAGE_SEVERITY_ERROR &&
                message->Severity != D3D11_MESSAGE_SEVERITY_WARNING)
                continue;
            ERROR_LOG("testLib: DirectX 11 debug message severity=%d id=%d: %s",
                      static_cast<int>(message->Severity), static_cast<int>(message->ID),
                      message->pDescription ? message->pDescription : "no description");
            ++failureCount;
        }
        infoQueue->ClearStoredMessages();
        infoQueue->Release();
        if (failureCount)
        {
            ERROR_LOG("testLib: DirectX 11 debug-layer validation failed with %llu message(s)", failureCount);
            return false;
        }
        INFO_LOG("testLib: DirectX 11 debug-layer validation passed");
#endif
        return true;
    }

    bool captureDirectX11LifecycleDebug(DIRECTX11_LIFECYCLE_DEBUG &lifecycleDebug)
    {
#if defined(_DEBUG)
        mbm::DEVICE *device = mbm::DEVICE::getInstance();
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        if (!context || !context->device)
            return false;
        if (FAILED(context->device->QueryInterface(__uuidof(ID3D11Debug),
                                                   reinterpret_cast<void **>(&lifecycleDebug.debug))) ||
            FAILED(context->device->QueryInterface(__uuidof(ID3D11InfoQueue),
                                                   reinterpret_cast<void **>(&lifecycleDebug.infoQueue))))
        {
            if (lifecycleDebug.infoQueue)
                lifecycleDebug.infoQueue->Release();
            if (lifecycleDebug.debug)
                lifecycleDebug.debug->Release();
            lifecycleDebug = {};
            return false;
        }
#endif
        return true;
    }

    bool validateDirectX11ResourceLifecycle(DIRECTX11_LIFECYCLE_DEBUG &lifecycleDebug)
    {
#if defined(_DEBUG)
        if (!lifecycleDebug.debug || !lifecycleDebug.infoQueue)
        {
            ERROR_LOG("testLib: DirectX 11 resource-lifecycle validation is unavailable");
            return false;
        }
        lifecycleDebug.infoQueue->ClearStoredMessages();
        lifecycleDebug.debug->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
        uint64_t liveObjectMessages = 0;
        const uint64_t messageCount = lifecycleDebug.infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
        for (uint64_t index = 0; index < messageCount; ++index)
        {
            SIZE_T messageSize = 0;
            if (FAILED(lifecycleDebug.infoQueue->GetMessage(index, nullptr, &messageSize)) || !messageSize)
                continue;
            std::vector<uint8_t> messageStorage(messageSize);
            D3D11_MESSAGE *message = reinterpret_cast<D3D11_MESSAGE *>(messageStorage.data());
            if (FAILED(lifecycleDebug.infoQueue->GetMessage(index, message, &messageSize)))
                continue;
            const char *description = message->pDescription ? message->pDescription : "no description";
            INFO_LOG("testLib: DirectX 11 lifecycle message id=%d: %s", static_cast<int>(message->ID), description);
            if (message->ID != D3D11_MESSAGE_ID_LIVE_DEVICE &&
                message->ID != D3D11_MESSAGE_ID_LIVE_OBJECT_SUMMARY)
                ++liveObjectMessages;
        }
        lifecycleDebug.infoQueue->Release();
        lifecycleDebug.debug->Release();
        lifecycleDebug = {};
        if (liveObjectMessages)
        {
            ERROR_LOG("testLib: DirectX 11 resource-lifecycle validation found %llu live object message(s)",
                      liveObjectMessages);
            return false;
        }
        INFO_LOG("testLib: DirectX 11 resource-lifecycle validation passed");
#endif
        return true;
    }
}
#endif

// Usage: testLib --skeletal-foundation-tests
//        testLib --gles-dqs-shader-test
//        testLib --gles-skeletal-parity-test
//        testLib --directx9-skeletal-parity-test
//        testLib --directx11-foundation-test
//        testLib --directx11-shader-profile-test
//        testLib --directx11-builtin-shader-test
//        testLib --directx11-texture-failure-test
//        testLib --directx11-screen-size-test
//        testLib --directx11-resize-test
//        testLib --directx11-skeletal-parity-test
//        testLib --directx11-lighting-test
//        testLib --directx11-custom-lighting-test (also covers multiple pixel cbuffers)
//        testLib --directx11-mesh-readback-test
//        testLib --directx11-rasterizer-test
//        testLib --directx11-depth-state-test
//        testLib --directx11-blend-state-test
//        testLib --directx11-sampler-state-test
//        testLib --directx11-texture-upload-test
//        testLib --directx11-texture-stage-test
//        testLib --metal-editor-shader-test
//        testLib --metal-skeletal-parity-test
//        testLib --metal-render-to-texture-test
//        testLib --macos-resize-test
//        testLib --macos-input-test
//        testLib --macos-close-test
//        testLib --macos-minimize-test
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
static int runTestLib(int argc, char **argv
#if defined(USE_DIRECTX11)
                      , DIRECTX11_LIFECYCLE_DEBUG &lifecycleDebug, bool &validateLifecycle
#endif
)
{
    if (argc == 2 && std::strcmp(argv[1], "--skeletal-foundation-tests") == 0)
        return runSkeletalFoundationTests();
    GAME game;
    game.myScene.testCoreManager = &game;
#if defined(USE_DIRECTX11)
    if (argc == 2 && std::strcmp(argv[1], "--directx11-builtin-shader-test") == 0)
    {
        game.myScene.testDirectX11BuiltinShaders = true;
        game.myScene.testTimeoutSeconds = 1.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--directx11-rasterizer-test") == 0)
    {
        game.myScene.testDirectX11Rasterizer = true;
        game.myScene.testTimeoutSeconds = 1.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--directx11-depth-state-test") == 0)
    {
        game.myScene.testDirectX11DepthState = true;
        game.myScene.testTimeoutSeconds = 1.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--directx11-blend-state-test") == 0)
    {
        game.myScene.testDirectX11BlendState = true;
        game.myScene.testTimeoutSeconds = 1.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--directx11-sampler-state-test") == 0)
    {
        game.myScene.testDirectX11SamplerState = true;
        game.myScene.testTimeoutSeconds = 3.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--directx11-texture-upload-test") == 0)
    {
        game.myScene.testDirectX11TextureUpload = true;
        game.myScene.testTimeoutSeconds = 1.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--directx11-texture-stage-test") == 0)
    {
        game.myScene.testDirectX11TextureStages = true;
        game.myScene.testTimeoutSeconds = 3.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--directx11-resize-test") == 0)
    {
        game.myScene.testDirectX11Resize = true;
        game.myScene.testTimeoutSeconds = 3.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--directx11-screen-size-test") == 0)
    {
        game.myScene.testDirectX11ScreenSize = true;
        game.myScene.testTimeoutSeconds = 1.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--directx11-texture-failure-test") == 0)
    {
        game.myScene.testDirectX11TextureFailure = true;
        game.myScene.testTimeoutSeconds = 1.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--directx11-shader-profile-test") == 0)
    {
        if (std::strcmp(mbm::getVSVersion(), "vs_4_0") != 0 ||
            std::strcmp(mbm::getPSVersion(), "ps_4_0") != 0)
            return -1;
        mbm::setVSVersion("vs_4_1");
        mbm::setPSVersion("ps_4_1");
        game.myScene.testDirectX11Foundation = true;
        game.myScene.testDirectX11ShaderProfiles = true;
        game.myScene.testTimeoutSeconds = 5.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--directx11-mesh-readback-test") == 0)
    {
        game.myScene.testDirectX11MeshReadback = true;
        game.myScene.testTimeoutSeconds = 1.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--directx11-custom-lighting-test") == 0)
    {
        game.myScene.testDirectX11CustomLighting = true;
        game.myScene.testTimeoutSeconds = 5.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--directx11-lighting-test") == 0)
    {
        game.myScene.testDirectX11Lighting = true;
        game.myScene.testTimeoutSeconds = 5.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--directx11-skeletal-parity-test") == 0)
    {
        game.myScene.testDirectX11SkeletalParity = true;
        game.myScene.testTimeoutSeconds = 1.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--directx11-foundation-test") == 0)
    {
        game.myScene.testDirectX11Foundation = true;
        game.myScene.testTimeoutSeconds = 5.0f;
    }
    else
#endif
#if defined(USE_METAL)
    if (argc == 2 && std::strcmp(argv[1], "--metal-skeletal-parity-test") == 0)
    {
        game.myScene.testMetalSkeletalParity = true;
        game.myScene.testTimeoutSeconds = 1.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--metal-editor-shader-test") == 0)
    {
        game.myScene.testMetalEditorShaders = true;
        game.myScene.testTimeoutSeconds = 1.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--metal-render-to-texture-test") == 0)
    {
        game.myScene.testMetalRenderToTexture = true;
        game.myScene.testTimeoutSeconds = 1.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--macos-resize-test") == 0)
    {
        game.myScene.testMacOSResize = true;
        game.myScene.testTimeoutSeconds = 3.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--macos-input-test") == 0)
    {
        game.myScene.testMacOSInput = true;
        game.myScene.testTimeoutSeconds = 3.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--macos-close-test") == 0)
    {
        game.myScene.testMacOSClose = true;
        game.myScene.testTimeoutSeconds = 3.0f;
    }
    else if (argc == 2 && std::strcmp(argv[1], "--macos-minimize-test") == 0)
    {
        game.myScene.testMacOSMinimize = true;
        game.myScene.testTimeoutSeconds = 5.0f;
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
#if defined(USE_DIRECTX9)
    if (argc == 2 && std::strcmp(argv[1], "--directx9-skeletal-parity-test") == 0)
    {
        game.myScene.testDirectX9SkeletalParity = true;
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
#if defined(USE_DIRECTX11)
        const bool directX11AutomatedTest = argc == 2 &&
            std::strncmp(argv[1], "--directx11-", sizeof("--directx11-") - 1u) == 0;
        const bool debugLayerClean = !directX11AutomatedTest || validateDirectX11DebugMessages();
        if (!debugLayerClean)
            return -1;
        if (directX11AutomatedTest)
        {
            validateLifecycle = true;
            if (!captureDirectX11LifecycleDebug(lifecycleDebug))
                return -1;
        }
#endif
        if (game.myScene.testMacOSMinimize && !game.myScene.testMacOSMinimizeCompleted)
            return -1;
        return game.myScene.automatedTestFailed ? -1 : result;
    }
    return -1;
}

int main(int argc, char **argv)
{
#if defined(USE_DIRECTX11)
    DIRECTX11_LIFECYCLE_DEBUG lifecycleDebug;
    bool validateLifecycle = false;
    const int result = runTestLib(argc, argv, lifecycleDebug, validateLifecycle);
    if (validateLifecycle && !validateDirectX11ResourceLifecycle(lifecycleDebug))
        return -1;
    return result;
#else
    return runTestLib(argc, argv);
#endif
}
