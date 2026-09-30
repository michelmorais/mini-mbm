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
#include "normal-map-preparation-tests.h"
#include "normal-map-native-resource-tests.h"
#include "gles-skeletal-parity-tests.h"
#include "directx9-skeletal-parity-tests.h"
#include "directx11-skeletal-parity-tests.h"
#include <core_mbm/light.h>
#include <cstdio>
#if defined(USE_OPENGL_ES) && defined(__linux__)
int runGlesNormalMapFailureTests();
int createGlesNormalMapBenchmarkFixtures();
int runGlesNormalMapBenchmark();
int runGlesNormalMapContextTests(mbm::CORE_MANAGER &, const mbm::SCENE &);
#endif
#include <cstdlib>
#include <cstring>
#if defined(USE_OPENGL_ES)
#include <specific-opengl_es.h>
#include <specific-opengl_es-buffer.h>
#include <specific-opengl_es-shader.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/light.h>
#include <cstdio>

static int runNormalMapLazyResourceTests()
{
    using namespace mbm;
    const char *dir = std::getenv("MBM_NORMAL_MAP_FIXTURE_DIR");
    if (!dir) return -1;
    int failures = 0;
    const auto check = [&failures](bool ok, const char *message)
    { if (!ok) { ++failures; std::printf("NORMAL MAP LAZY FAIL: %s\n", message); } return ok; };
    auto *manager = MESH_MANAGER::getInstance();
    const auto *mesh = manager->load((std::string(dir)+"/author-import.msh").c_str());
    if (!check(mesh != nullptr, "load retained basis without map")) return -1;
    auto *buffer = mesh->getBuffer(0)->pBufferGL;
    SHADER shader;
    shader.setUseReservedLightDefault(true);
    if (!check(shader.compileShader(nullptr, nullptr, buffer->fvf), "compile geometric shader")) return -1;
    auto *geometry = static_cast<GLES_PS_VS *>(shader.getBackendShaderSpecific());
    check(glGetAttribLocation(geometry->programObject,"aTangent") == -1, "geometric program has no tangent input");
    check(glGetUniformLocation(geometry->programObject,"NormalMapSettings") == -1, "geometric program has no normal-map settings");
    MatrixIdentity(&SHADER::modelView);
    MatrixIdentity(&SHADER::mvMatrixLightSpace);
    MatrixIdentity(&SHADER::mvpMatrix);
    DEVICE::getInstance()->setLightTargetForRender(LIGHT_TARGET_3D);
    setLightEnabled(LIGHT_TARGET_3D, true);
    const auto currentProgram = []() { GLint id=0; glGetIntegerv(GL_CURRENT_PROGRAM,&id); return static_cast<GLuint>(id); };
#if USE_NORMAL_MAPPING_3D
    check(buffer->getBackendBuffer()->normalMapSubsets.empty(), "retained basis has no GPU allocation at load");
#endif
    check(shader.render(buffer), "draw without map");
    check(currentProgram() == geometry->programObject, "no map selects geometric program");
    check(mesh->setMaterialTexture(0,0,TEXTURE_ROLE_NORMAL,"#FF8080FF",false), "late map assignment");
    check(mesh->setNormalMapSettings(0,0,1,0), "zero strength");
    check(shader.render(buffer), "zero-strength draw");
    check(currentProgram() == geometry->programObject, "zero strength selects geometric program");
#if USE_NORMAL_MAPPING_3D
    check(buffer->getBackendBuffer()->normalMapSubsets.empty(), "zero strength does not upload");
#endif
    check(mesh->setNormalMapSettings(0,0,1,1), "enable strength");
    DEVICE::getInstance()->setLightTargetForRender(LIGHT_TARGET_2DW);
    check(shader.render(buffer), "2dw draw");
#if USE_NORMAL_MAPPING_3D
    check(buffer->getBackendBuffer()->normalMapSubsets.empty(), "2dw does not upload a 3d basis");
#endif
    DEVICE::getInstance()->setLightTargetForRender(LIGHT_TARGET_3D);
    check(shader.render(buffer), "first effective mapped draw");
    const GLuint mapped = currentProgram();
#if USE_NORMAL_MAPPING_3D
    check(mapped != geometry->programObject && glGetAttribLocation(mapped,"aTangent") >= 0, "mapped variant has its own tangent input");
    auto &subsets = buffer->getBackendBuffer()->normalMapSubsets;
    if (!check(!subsets.empty() && !subsets[0].batches.empty(), "first use uploads GPU batches")) return -1;
    const GLuint vertices = subsets[0].batches[0].buffers[0];
    check(glIsBuffer(vertices), "derived buffer is live");
    GLint geometricAttributes=0, mappedAttributes=0, vertexBytes=0, indexBytes=0;
    glGetProgramiv(geometry->programObject,GL_ACTIVE_ATTRIBUTES,&geometricAttributes);
    glGetProgramiv(mapped,GL_ACTIVE_ATTRIBUTES,&mappedAttributes);
    glBindBuffer(GL_ARRAY_BUFFER,vertices);
    glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&vertexBytes);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,subsets[0].batches[0].buffers[1]);
    glGetBufferParameteriv(GL_ELEMENT_ARRAY_BUFFER,GL_BUFFER_SIZE,&indexBytes);
    std::printf("NORMAL MAP LAZY METRICS attributes geometric=%d mapped=%d, derived GPU bytes before=0 after=%d\n",
                geometricAttributes,mappedAttributes,vertexBytes+indexBytes);
#else
    check(mapped == geometry->programObject, "disabled build retains geometric program");
#endif
    check(mesh->setMaterialTexture(0,0,TEXTURE_ROLE_NORMAL,nullptr,false), "remove map");
    check(shader.render(buffer) && currentProgram() == geometry->programObject, "removal returns to geometric program");
    check(mesh->setMaterialTexture(0,0,TEXTURE_ROLE_NORMAL,"#FF8080FF",false), "reassign map");
    for (int i=0; i<20; ++i)
        check(shader.render(buffer) && currentProgram() == mapped, "repeated mapped draws reuse program");
#if USE_NORMAL_MAPPING_3D
    check(subsets[0].batches.size() == 1 && subsets[0].batches[0].buffers[0] == vertices,
          "reassignment and idle draws reuse GPU allocation");
#endif
    SHADER cached;
    cached.setUseReservedLightDefault(true);
    check(cached.compileShader(nullptr,nullptr,buffer->fvf), "compile another geometric shader");
    check(static_cast<GLES_PS_VS *>(cached.getBackendShaderSpecific())->programObject == geometry->programObject,
          "cache keeps geometric and mapped programs distinct");
    check(cached.render(buffer) && currentProgram() == mapped, "mapped variant uses process cache");
    shader.onRestore();
    check(shader.compileShader(nullptr,nullptr,buffer->fvf) && shader.render(buffer), "restore rebuilds variant ownership");
    const auto *unprepared = manager->load((std::string(dir)+"/no-map.msh").c_str());
    if (!check(unprepared != nullptr, "load asset without any prepared basis")) return -1;
    check(unprepared->setMaterialTexture(0,0,TEXTURE_ROLE_NORMAL,"#FF8080FF",false), "assign map without basis");
    check(shader.render(unprepared->getBuffer(0)->pBufferGL) && currentProgram() == geometry->programObject,
          "map without basis retains geometric fallback");
    const VEC3 positions[3] = {{0,0,0},{1,0,0},{0,1,0}};
    const VEC3 normals[3] = {{0,0,1},{0,0,1},{0,0,1}};
    const VEC2 uv[3] = {{0,0},{1,0},{0,1}};
    const int starts[1] = {0}, counts[1] = {3};
    check(buffer->updateDynamic(positions,normals,uv,starts,counts), "update source geometry");
#if USE_NORMAL_MAPPING_3D
    check(buffer->getBackendBuffer()->normalMapSubsets.empty() && !glIsBuffer(vertices), "dynamic edit releases derived resources");
#endif
    check(shader.render(buffer) && currentProgram() == geometry->programObject,
          "dynamic edit cannot re-upload stale staged geometry");
    check(glGetError() == GL_NO_ERROR, "no GL errors");
    std::printf("NORMAL MAP LAZY RESOURCES %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? -1 : 0;
}
#endif
#if defined(USE_DIRECTX9)
#include <specific-directx9-shader.h>
#include <core_mbm/shader.h>
#include <core_mbm/shader-resource.h>
#include <core_mbm/light.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <vector>

static bool reportDirectX9ShaderBudget(IUnknown *object, bool pixel)
{
    UINT bytes = 0;
    HRESULT result = pixel ? static_cast<IDirect3DPixelShader9 *>(object)->GetFunction(nullptr,&bytes) :
                            static_cast<IDirect3DVertexShader9 *>(object)->GetFunction(nullptr,&bytes);
    if (FAILED(result) || !bytes) return false;
    std::vector<DWORD> code((bytes+3)/4);
    result = pixel ? static_cast<IDirect3DPixelShader9 *>(object)->GetFunction(code.data(),&bytes) :
                     static_cast<IDirect3DVertexShader9 *>(object)->GetFunction(code.data(),&bytes);
    ID3DBlob *assembly = nullptr;
    if (FAILED(result) || FAILED(D3DDisassemble(code.data(),bytes,0,nullptr,&assembly))) return false;
    const std::string text(static_cast<const char *>(assembly->GetBufferPointer()),assembly->GetBufferSize());
    const auto at = text.find("approximately");
    std::printf("DX9 %s budget: %s\n",pixel ? "PS" : "VS",
                at == std::string::npos ? "instruction estimate unavailable" : text.substr(at).c_str());
    assembly->Release();
    return true;
}

static int runDirectX9NormalMapShaderTests()
{
    using namespace mbm;
    const std::string savedPS = getPSVersion(), savedVS = getVSVersion();
    bool passed = savedPS == "ps_3_0" && savedVS == "vs_3_0";
    if (!passed) return -1;
    SHADER shader;
    shader.setUseReservedLightDefault(true);
    passed = shader.compileShader(nullptr,nullptr,FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
    auto *backend = static_cast<D3D_PS_VS *>(shader.getBackendShaderSpecific());
#if USE_NORMAL_MAPPING_3D
    passed = passed && !backend->normalMapDeclaration && !backend->normalMapSettings;
#endif
    passed = passed &&
        reportDirectX9ShaderBudget(backend->pd3dPixelShader,true) &&
        reportDirectX9ShaderBudget(backend->pd3dVertexShader,false);
    // Recompile the same instance and another instance to cover cached COM ownership.
    passed = passed && shader.compileShader(nullptr,nullptr,FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
    SHADER cached;
    cached.setUseReservedLightDefault(true);
    passed = passed && cached.compileShader(nullptr,nullptr,FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
    auto *cachedBackend = static_cast<D3D_PS_VS *>(cached.getBackendShaderSpecific());
    passed = passed && cachedBackend->pd3dPixelShader == backend->pd3dPixelShader;
#if USE_NORMAL_MAPPING_3D
    passed = passed && !cachedBackend->normalMapDeclaration;
#endif

    // This measures the pre-existing geometric lighting at SM2, not SM2 hardware.
    setPSVersion("ps_2_0"); setVSVersion("vs_2_0");
    SHADER sm2;
    sm2.setUseReservedLightDefault(true);
    const bool sm2Lit = sm2.compileShader(nullptr,nullptr,FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
    std::printf("DX9 existing geometric lighting at SM2: %s\n",sm2Lit ? "compiled" : "exceeds profile (expected diagnostic above)");
#if USE_NORMAL_MAPPING_3D
    passed = passed && !static_cast<D3D_PS_VS *>(sm2.getBackendShaderSpecific())->normalMapDeclaration;
#endif
    SHADER unlit;
    passed = passed && unlit.compileShader(nullptr,nullptr,FVF_PROVIDE_BY_ENGINE::FVF_POS_UV);
    setPSVersion(savedPS.c_str()); setVSVersion(savedVS.c_str());
    SHADER::clearDefaultProgramCache();
    std::printf("DIRECTX9 NORMAL MAP SHADER %s\n",passed ? "PASS" : "FAIL");
    return passed ? 0 : -1;
}
#endif
#if defined(USE_DIRECTX11)
#include <specific-directx11-context.h>
#include <core_mbm/device.h>
#include <core_mbm/light.h>
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

// Usage: testLib --normal-map-persistence-tests
//        testLib --normal-map-preparation-tests
//        testLib --skeletal-foundation-tests
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
#if defined(USE_OPENGL_ES) && defined(__linux__)
    if (argc == 2 && std::strcmp(argv[1],"--normal-map-benchmark-fixtures") == 0)
        return createGlesNormalMapBenchmarkFixtures();
#endif
    if (argc == 2 && std::strcmp(argv[1], "--normal-map-build-info") == 0)
    {
#if defined(USE_OPENGL_ES)
        const char *backend = "gles";
#elif defined(USE_DIRECTX9)
        const char *backend = "dx9";
#elif defined(USE_DIRECTX11)
        const char *backend = "dx11";
#elif defined(USE_METAL)
        const char *backend = "metal";
#else
        const char *backend = "unsupported";
#endif
        std::printf("NORMAL MAP BUILD backend=%s normal=%d lights=%u\n", backend,
                    USE_NORMAL_MAPPING_3D, mbm::getSupportedMaxLights(mbm::LIGHT_TARGET_3D));
        return 0;
    }
    if (argc == 2 && std::strcmp(argv[1], "--skeletal-foundation-tests") == 0)
        return runSkeletalFoundationTests();
    if (argc == 2 && std::strcmp(argv[1], "--normal-map-preparation-tests") == 0)
        return runNormalMapPreparationTests();
    if (argc == 2 && std::strcmp(argv[1], "--normal-map-persistence-tests") == 0)
        return runNormalMapPersistenceTests();
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
#if defined(USE_OPENGL_ES) && defined(__linux__)
        if (argc == 2 && std::strcmp(argv[1],"--normal-map-benchmark") == 0)
            return runGlesNormalMapBenchmark();
        if (argc == 2 && std::strcmp(argv[1],"--normal-map-context-test") == 0)
            return runGlesNormalMapContextTests(game,game.myScene);
        if (argc == 2 && std::strcmp(argv[1],"--normal-map-failure-test") == 0)
            return runGlesNormalMapFailureTests();
#endif
        if (argc == 2 && std::strcmp(argv[1],"--normal-map-lazy-resource-test") == 0)
        {
#if defined(USE_OPENGL_ES)
            return runNormalMapLazyResourceTests();
#elif defined(USE_DIRECTX9) || defined(USE_DIRECTX11) || defined(USE_METAL)
            const int result = runNormalMapNativeResourceTests();
#if defined(USE_DIRECTX11)
            const bool clean = validateDirectX11DebugMessages();
            validateLifecycle = captureDirectX11LifecycleDebug(lifecycleDebug);
            return result == 0 && clean && validateLifecycle ? 0 : -1;
#else
            return result;
#endif
#else
            std::printf("NORMAL MAP LAZY RESOURCES FAIL: unsupported backend\n");
            return -1;
#endif
        }
#if defined(USE_DIRECTX9)
        if (argc == 2 && std::strcmp(argv[1],"--directx9-normal-map-shader-test") == 0)
            return runDirectX9NormalMapShaderTests();
#endif
#if defined(USE_DIRECTX11) || defined(USE_DIRECTX9) || defined(USE_METAL)
        if (std::getenv("MBM_NORMAL_MAP_TEST_LIGHTING"))
        {
            mbm::setLightEnabled(mbm::LIGHT_TARGET_3D, true);
            mbm::setAmbientLight(mbm::LIGHT_TARGET_3D, mbm::COLOR(0.1f,0.1f,0.1f,1));
            mbm::setDirectionalLight(mbm::LIGHT_TARGET_3D, mbm::VEC3(-0.8f,-0.3f,0.6f), mbm::COLOR(0.6f,0.6f,0.6f,1));
        }
#endif
        const int result = game.onLoop(singleLoop, doSwapBuffers);
#if defined(USE_DIRECTX11)
        const bool directX11AutomatedTest = std::getenv("MBM_DIRECTX11_VALIDATE") != nullptr ||
            (argc == 2 && std::strncmp(argv[1], "--directx11-", sizeof("--directx11-") - 1u) == 0);
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
