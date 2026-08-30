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
#include "directx11-skeletal-parity-tests.h"
#include "directx11-builtin-shader-tests.h"
#include "gles-skeletal-parity-tests.h"
#include "directx9-skeletal-parity-tests.h"
#if defined(USE_METAL)
#include "metal-skeletal-parity-tests.h"
#endif
#if defined(__APPLE__)
#include "macos-window-tests.h"
#endif
#include <core_mbm/texture-manager.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/shader-resource.h>
#include <core_mbm/util-interface.h>
#include <core_mbm/shader-var-cfg.h>
#include <core_mbm/light.h>
#if defined(USE_DIRECTX11)
#include <specific-directx11-context.h>
#include <core_mbm/draw-compatibility.h>
#endif
#if defined(_WIN32)
#include <core_mbm/platform-win32.h>
#endif
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cmath>
#include <random>
#if defined(USE_METAL)
#include <lodepng/lodepng.h>
#endif

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
    testTimeoutSeconds = -1.0f;
    testElapsedSeconds = 0.0f;
    cliMeshMode        = RenderMode::NONE;
    cliSkeletalMethod  = mbm::SKELETAL_SHADER_METHOD::LBS;
    cliSkeletalExecutionPath = mbm::SKELETAL_EXECUTION_PATH::AUTO;
    cliSkeletalExecutionPathSet = false;
    testGlesDqsShader  = false;
    testGlesSkeletalParity = false;
    testDirectX9SkeletalParity = false;
    testDirectX11Foundation = false;
    testDirectX11BuiltinShaders = false;
    testDirectX11ShaderProfiles = false;
    testDirectX11TextureFailure = false;
    testDirectX11ScreenSize = false;
    testDirectX11Resize = false;
    testDirectX11ResizeRequested = false;
    testDirectX11ResizeNotified = false;
    testDirectX11SkeletalParity = false;
    testDirectX11Lighting = false;
    testDirectX11CustomLighting = false;
    testDirectX11MeshReadback = false;
    testDirectX11Rasterizer = false;
    testDirectX11DepthState = false;
    testDirectX11BlendState = false;
    testDirectX11SamplerState = false;
    testDirectX11SamplerPhase = 0;
    testDirectX11TextureUpload = false;
    testDirectX11TextureStages = false;
    testDirectX11TextureStagePhase = 0;
    for (mbm::TEXTURE *&stageTexture : testDirectX11StageTextures)
        stageTexture = nullptr;
    testMetalEditorShaders = false;
    testMetalSkeletalParity = false;
    testMetalRenderToTexture = false;
    testMacOSResize = false;
    testMacOSResizeRequested = false;
    testMacOSResizeNotified = false;
    testMacOSInput = false;
    testMacOSInputPhase = 0;
    testMacOSKeyDownMask = 0;
    testMacOSKeyUpMask = 0;
    testMacOSTouchDownMask = 0;
    testMacOSTouchUpMask = 0;
    testMacOSZoomMask = 0;
    testMacOSMoveReceived = false;
    testMacOSDoubleClickReceived = false;
    testMacOSCoordinatesValid = true;
    testMacOSObservedX = -1.0f;
    testMacOSObservedY = -1.0f;
    testMacOSClose = false;
    testMacOSCloseRequested = false;
    testCoreManager = nullptr;
    automatedTestFailed = false;
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
    mbm::CAMERA &camera = device->getCamera();
    camera.position = mbm::VEC3(0, 280, -900);
    camera.focus    = mbm::VEC3(0, 280, 0);
    mbm::COLOR backgroundColor = device->getColorClearBackGround();
    backgroundColor.b = 0.5f;
    device->setColorClearBackGround(backgroundColor);

    util::addPath(__FILE__);

#if defined(USE_DIRECTX11)
    if (testDirectX11BuiltinShaders)
    {
        automatedTestFailed = runDirectX11BuiltinShaderTests() != 0;
        device->setRun(false);
        return;
    }
    if (testDirectX11TextureStages)
    {
        texture = new mbm::TEXTURE_VIEW(this, false, true);
        bool valid = texture->load("wooden-box.jpg", 256.0f, 256.0f, true);
        mbm::BUFFER_GL *buffer = valid ? texture->getFrame() : nullptr;
        testDirectX11StageTextures[0] = valid ? texture->getTexture() : nullptr;
        static const uint8_t stagePixels[5][4] = {
            { 13, 17, 19, 23 }, { 29, 31, 37, 41 }, { 43, 47, 53, 59 },
            { 61, 67, 71, 73 }, { 79, 83, 89, 97 }
        };
        mbm::TEXTURE_MANAGER *textureManager = mbm::TEXTURE_MANAGER::getInstance();
        for (uint32_t stage = 1; valid && stage < 6; ++stage)
        {
            char nickname[48] = {};
            snprintf(nickname, sizeof(nickname), "directx11-stage-%u-smoke", stage);
            testDirectX11StageTextures[stage] = textureManager->load(
                1, 1, stagePixels[stage - 1], nickname, 8, 4, true);
            valid = buffer && testDirectX11StageTextures[stage];
            if (valid)
            {
                buffer->setTextureByStage(testDirectX11StageTextures[stage], stage, 0);
                if (stage == 1)
                    texture->getAnimation()->getFx().textureAnimationEffect = testDirectX11StageTextures[stage];
            }
        }
        if (!valid)
        {
            ERROR_LOG("testLib: DirectX 11 six-stage texture fixture setup failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        INFO_LOG("testLib: DirectX 11 six-stage texture fixture started");
        return;
    }
    if (testDirectX11TextureUpload)
    {
        static const uint8_t rgbPixels[] = {
            1, 2, 3,       17, 31, 47,
            63, 79, 95,    127, 191, 251
        };
        static const uint8_t expectedRgbAsRgba[] = {
            1, 2, 3, 255,       17, 31, 47, 255,
            63, 79, 95, 255,    127, 191, 251, 255
        };
        static const uint8_t rgbaPixels[] = {
            5, 7, 11, 13,       19, 23, 29, 31,
            37, 41, 43, 47,     53, 59, 61, 67
        };
        mbm::TEXTURE_MANAGER *textureManager = mbm::TEXTURE_MANAGER::getInstance();
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        mbm::TEXTURE *rgbTexture = textureManager->load(
            2, 2, rgbPixels, "directx11-rgb-upload-smoke", 8, 3, false);
        mbm::TEXTURE *rgbaTexture = textureManager->load(
            2, 2, rgbaPixels, "directx11-rgba-upload-smoke", 8, 4, true);
        auto matchesGpuTexture = [context](mbm::TEXTURE *source, const uint8_t *expected) -> bool
        {
            if (!context || !context->device || !context->immediateContext || !source || !expected)
                return false;
            ID3D11ShaderResourceView *view =
                static_cast<ID3D11ShaderResourceView *>(source->getBackendTexturePointer());
            if (!view)
                return false;
            ID3D11Resource *resource = nullptr;
            ID3D11Texture2D *gpuTexture = nullptr;
            ID3D11Texture2D *stagingTexture = nullptr;
            view->GetResource(&resource);
            HRESULT result = resource ? resource->QueryInterface(
                __uuidof(ID3D11Texture2D), reinterpret_cast<void **>(&gpuTexture)) : E_FAIL;
            D3D11_TEXTURE2D_DESC description = {};
            if (SUCCEEDED(result))
            {
                gpuTexture->GetDesc(&description);
                result = description.Width == 2 && description.Height == 2 && description.MipLevels == 1 &&
                         description.Format == DXGI_FORMAT_R8G8B8A8_UNORM ? S_OK : E_FAIL;
            }
            if (SUCCEEDED(result))
            {
                description.Usage = D3D11_USAGE_STAGING;
                description.BindFlags = 0;
                description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                description.MiscFlags = 0;
                result = context->device->CreateTexture2D(&description, nullptr, &stagingTexture);
            }
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (SUCCEEDED(result))
            {
                context->immediateContext->CopyResource(stagingTexture, gpuTexture);
                result = context->immediateContext->Map(stagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
            }
            bool matches = SUCCEEDED(result);
            for (uint32_t row = 0; matches && row < 2; ++row)
            {
                const uint8_t *gpuRow = static_cast<const uint8_t *>(mapped.pData) + mapped.RowPitch * row;
                matches = memcmp(gpuRow, expected + row * 8u, 8u) == 0;
            }
            if (SUCCEEDED(result))
                context->immediateContext->Unmap(stagingTexture, 0);
            if (stagingTexture)
                stagingTexture->Release();
            if (gpuTexture)
                gpuTexture->Release();
            if (resource)
                resource->Release();
            return matches;
        };
        const bool valid = rgbTexture && rgbaTexture && !rgbTexture->hasAlphaChannel() &&
                           rgbaTexture->hasAlphaChannel() &&
                           matchesGpuTexture(rgbTexture, expectedRgbAsRgba) &&
                           matchesGpuTexture(rgbaTexture, rgbaPixels);
        if (!valid)
        {
            ERROR_LOG("testLib: DirectX 11 RGB/RGBA texture upload readback failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        INFO_LOG("testLib: DirectX 11 RGB/RGBA texture upload readback passed");
        device->setRun(false);
        return;
    }
    if (testDirectX11SamplerState)
    {
        texture = new mbm::TEXTURE_VIEW(this, false, true);
        if (!texture->load("wooden-box.jpg", 256.0f, 256.0f, true))
        {
            ERROR_LOG("testLib: DirectX 11 sampler-state fixture failed to load");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        device->enableFilteringAfterPixelPerfect();
        INFO_LOG("testLib: DirectX 11 sampler-state fixture started");
        return;
    }
    if (testDirectX11BlendState)
    {
        static const D3D11_BLEND expectedDestination[] = {
            D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_ZERO, D3D11_BLEND_ONE, D3D11_BLEND_SRC_COLOR,
            D3D11_BLEND_INV_SRC_COLOR, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_INV_SRC_ALPHA,
            D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_DEST_COLOR,
            D3D11_BLEND_INV_DEST_COLOR
        };
        static const D3D11_BLEND_OP expectedOperation[] = {
            D3D11_BLEND_OP_ADD, D3D11_BLEND_OP_SUBTRACT, D3D11_BLEND_OP_REV_SUBTRACT,
            D3D11_BLEND_OP_MIN, D3D11_BLEND_OP_MAX
        };
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        mbm::RENDER_STATE renderState;
        mbm::FX blendFx;
        bool valid = context && context->immediateContext;
        for (int stateIndex = mbm::BLEND_DISABLE;
             valid && stateIndex <= mbm::BLEND_INVDESTCOLOR; ++stateIndex)
        {
            for (int operationIndex = 1; valid && operationIndex <= 5; ++operationIndex)
            {
                renderState.set(static_cast<mbm::BLEND_STATE>(stateIndex));
                blendFx.blendOperation = operationIndex;
                blendFx.setBlendOp();
                ID3D11BlendState *activeState = nullptr;
                FLOAT blendFactor[4] = {};
                UINT sampleMask = 0;
                context->immediateContext->OMGetBlendState(&activeState, blendFactor, &sampleMask);
                D3D11_BLEND_DESC description = {};
                if (activeState)
                    activeState->GetDesc(&description);
                const D3D11_RENDER_TARGET_BLEND_DESC &target = description.RenderTarget[0];
                valid = activeState && target.BlendEnable == TRUE &&
                        target.SrcBlend == D3D11_BLEND_SRC_ALPHA &&
                        target.DestBlend == expectedDestination[stateIndex] &&
                        target.BlendOp == expectedOperation[operationIndex - 1] &&
                        target.SrcBlendAlpha == D3D11_BLEND_ONE &&
                        target.DestBlendAlpha == D3D11_BLEND_INV_SRC_ALPHA &&
                        target.BlendOpAlpha == D3D11_BLEND_OP_ADD &&
                        target.RenderTargetWriteMask == D3D11_COLOR_WRITE_ENABLE_ALL &&
                        sampleMask == 0xffffffffu;
                if (activeState)
                    activeState->Release();
            }
        }
        renderState.set(mbm::BLEND_DISABLE);
        blendFx.setBlendDefaultOp();
        if (!valid)
        {
            ERROR_LOG("testLib: DirectX 11 legacy blend-state mapping failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        INFO_LOG("testLib: DirectX 11 legacy blend-state mapping passed all 55 cases");
        device->setRun(false);
        return;
    }
    if (testDirectX11DepthState)
    {
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        bool valid = context && context->immediateContext && context->depthEnabledState &&
                     context->depthDisabledState && context->depthView;
        D3D11_DEPTH_STENCIL_DESC disabledDescription = {};
        D3D11_DEPTH_STENCIL_DESC enabledDescription = {};
        ID3D11DepthStencilState *activeState = nullptr;
        UINT stencilReference = 0;
        if (valid)
        {
            device->setDepthTest(false);
            context->immediateContext->OMGetDepthStencilState(&activeState, &stencilReference);
            valid = activeState != nullptr;
            if (valid)
                activeState->GetDesc(&disabledDescription);
            valid = valid && activeState == context->depthDisabledState && disabledDescription.DepthEnable == FALSE &&
                    disabledDescription.StencilEnable == FALSE && stencilReference == 0;
            if (activeState)
                activeState->Release();
            activeState = nullptr;
        }
        if (valid)
        {
            device->setDepthTest(true);
            context->immediateContext->OMGetDepthStencilState(&activeState, &stencilReference);
            valid = activeState != nullptr;
            if (valid)
                activeState->GetDesc(&enabledDescription);
            valid = valid && activeState == context->depthEnabledState && enabledDescription.DepthEnable == TRUE &&
                    enabledDescription.DepthWriteMask == D3D11_DEPTH_WRITE_MASK_ALL &&
                    enabledDescription.DepthFunc == D3D11_COMPARISON_LESS_EQUAL &&
                    enabledDescription.StencilEnable == TRUE && stencilReference == 0;
            if (activeState)
                activeState->Release();
            activeState = nullptr;
        }
        if (valid)
        {
            device->clearDepth();
            device->clearDepthColored();
        }
        if (!valid)
        {
            ERROR_LOG("testLib: DirectX 11 depth/stencil-state contract failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        INFO_LOG("testLib: DirectX 11 depth enable/disable and clear contract passed");
        device->setRun(false);
        return;
    }
    if (testDirectX11Resize)
    {
#if defined(_WIN32)
        mbm::hideConsoleWindow();
#endif
    }
    if (testDirectX11Rasterizer)
    {
        struct RASTERIZER_CASE
        {
            uint32_t cullMode;
            uint32_t frontFace;
            D3D11_CULL_MODE expectedCull;
            BOOL expectedCounterClockwise;
        };
        const RASTERIZER_CASE cases[] = {
            { util::CULL_FRONT, util::CW, D3D11_CULL_FRONT, FALSE },
            { util::CULL_FRONT, util::CCW, D3D11_CULL_FRONT, TRUE },
            { util::CULL_BACK, util::CW, D3D11_CULL_BACK, FALSE },
            { util::CULL_BACK, util::CCW, D3D11_CULL_BACK, TRUE },
            { util::CULL_FRONT_AND_BACK, util::CW, D3D11_CULL_NONE, FALSE },
            { util::CULL_FRONT_AND_BACK, util::CCW, D3D11_CULL_NONE, TRUE }
        };
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        bool valid = context && context->immediateContext;
        for (const RASTERIZER_CASE &testCase : cases)
        {
            valid = valid && context->applyRasterizerState(testCase.cullMode, testCase.frontFace);
            ID3D11RasterizerState *state = nullptr;
            if (valid)
                context->immediateContext->RSGetState(&state);
            D3D11_RASTERIZER_DESC description = {};
            if (state)
            {
                state->GetDesc(&description);
                state->Release();
            }
            valid = valid && state && description.CullMode == testCase.expectedCull &&
                    description.FrontCounterClockwise == testCase.expectedCounterClockwise &&
                    description.DepthClipEnable == TRUE;
        }
        if (context)
            context->applyRasterizerState(util::CULL_BACK, util::CW);
        if (!valid)
        {
            ERROR_LOG("testLib: DirectX 11 rasterizer-state mapping failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        INFO_LOG("testLib: DirectX 11 rasterizer-state mapping passed all six cases");
        return;
    }
    if (testDirectX11MeshReadback)
    {
        mesh = new mbm::MESH(this, true, false);
        mbm::MESH_MBM_DEBUG expected;
        mbm::MESH_MBM *runtimeMesh = nullptr;
        bool valid = mesh->load("Crate.msh") && expected.loadV11("Crate.msh");
        if (valid)
            runtimeMesh = mbm::MESH_MANAGER::getInstance()->getIfExists("Crate.msh");
        valid = valid && runtimeMesh && meshDebug.loadDebugFromMemory(runtimeMesh) &&
                expected.getTotalFrames() == meshDebug.getTotalFrames();
        for (uint32_t frame = 0; valid && frame < expected.getTotalFrames(); ++frame)
        {
            const util::BUFFER_MESH_DEBUG *source = expected.getFrameBuffer(frame);
            const util::BUFFER_MESH_DEBUG *readback = meshDebug.getFrameBuffer(frame);
            valid = source && readback &&
                source->headerFrame.sizeVertexBuffer == readback->headerFrame.sizeVertexBuffer &&
                source->headerFrame.sizeIndexBuffer == readback->headerFrame.sizeIndexBuffer &&
                source->subset.size() == readback->subset.size();
            if (!valid)
                break;
            const size_t vertexCount = static_cast<size_t>(source->headerFrame.sizeVertexBuffer);
            valid = memcmp(source->position, readback->position, vertexCount * sizeof(float) * 3u) == 0;
            if (valid && source->normal)
                valid = readback->normal &&
                    memcmp(source->normal, readback->normal, vertexCount * sizeof(float) * 3u) == 0;
            if (valid && source->uv)
                valid = readback->uv &&
                    memcmp(source->uv, readback->uv, vertexCount * sizeof(float) * 2u) == 0;
            const size_t indexCount = static_cast<size_t>(source->headerFrame.sizeIndexBuffer);
            if (valid && indexCount)
                valid = readback->indexBuffer &&
                    memcmp(source->indexBuffer, readback->indexBuffer,
                           indexCount * sizeof(uint16_t)) == 0;
        }
        if (!valid)
        {
            ERROR_LOG("testLib: DirectX 11 mesh GPU readback parity failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        INFO_LOG("testLib: DirectX 11 mesh GPU readback parity passed");
        return;
    }
    if (testDirectX11Lighting || testDirectX11CustomLighting)
    {
#if defined(_WIN32)
        mbm::hideConsoleWindow();
#endif
        const mbm::COLOR ambient(0.15f, 0.18f, 0.22f, 1.0f);
        const mbm::COLOR warm(1.0f, 0.45f, 0.2f, 1.0f);
        const mbm::COLOR cool(0.2f, 0.5f, 1.0f, 1.0f);
        mbm::resetLight(mbm::LIGHT_TARGET_3D);
        mbm::resetLight(mbm::LIGHT_TARGET_2DW);
        mbm::setLightEnabled(mbm::LIGHT_TARGET_3D, true);
        mbm::setAmbientLight(mbm::LIGHT_TARGET_3D, ambient);
        mbm::setDirectionalLight(mbm::LIGHT_TARGET_3D, mbm::VEC3(0.0f, -1.0f, -1.0f), cool);
        mbm::addPointLight(mbm::LIGHT_TARGET_3D, mbm::VEC3(-150.0f, 300.0f, -150.0f), 700.0f, warm);
        mbm::addPointLight(mbm::LIGHT_TARGET_3D, mbm::VEC3(180.0f, 220.0f, -80.0f), 550.0f, cool);
        mbm::setLightEnabled(mbm::LIGHT_TARGET_2DW, true);
        mbm::setAmbientLight(mbm::LIGHT_TARGET_2DW, ambient);
        mbm::addPointLight(mbm::LIGHT_TARGET_2DW, mbm::VEC3(-80.0f, 0.0f, 120.0f), 420.0f, warm);
        mbm::addPointLight(mbm::LIGHT_TARGET_2DW, mbm::VEC3(120.0f, 0.0f, 120.0f), 360.0f, cool);
        mesh = new mbm::MESH(this, true, false);
        texture = new mbm::TEXTURE_VIEW(this, false, false);
        if (!mesh->setSkeletalSkinningMethod(mbm::SKELETAL_SHADER_METHOD::LBS) ||
            !mesh->setSkeletalExecutionPath(mbm::SKELETAL_EXECUTION_PATH::GPU) ||
            !mesh->load("Lorekeeper-walk.msh") || !texture->load("wooden-box.jpg", 260.0f, 260.0f))
        {
            ERROR_LOG("testLib: DirectX 11 lighting setup failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        mesh->setPosition(mbm::VEC3(-180.0f, 180.0f, 0.0f));
        texture->setPosition(mbm::VEC3(260.0f, 180.0f, 0.0f));
        const char *animationName = mesh->getSkeletalAnimationName(0);
        if (animationName)
            mesh->playSkeletalAnimation(animationName);
        if (testDirectX11CustomLighting)
        {
            mbm::SHADER_CFG customLightShader("directx11-custom-reserved-light.ps");
            customLightShader.codeShader =
                "cbuffer LightValues:register(b2){int LightEnabled;int LightCount;int LightMode;int HasNormalMap;"
                "float4 AmbientColor;float3 LightDirectionView;float4 DirectionalColor;"
                "float3 LightPositionView[4];float LightRadius[4];float4 LightColor[4];};"
                "cbuffer MaterialValues:register(b3){"
                "float4 MaterialDiffuse;float4 MaterialAmbient;float4 MaterialSpecular;"
                "float4 MaterialEmissive;float MaterialPower;};Texture2D TextureDiffuse:register(t0);"
                "SamplerState DiffuseSampler:register(s0);struct PSInput{float4 position:SV_POSITION;"
                "float2 uv:TEXCOORD0;float3 normalIn:TEXCOORD1;float3 positionIn:TEXCOORD2;};"
                "float4 main(PSInput input):SV_TARGET{float2 uv=input.uv;float3 normalIn=input.normalIn;"
                "float3 positionIn=input.positionIn;float4 tex=TextureDiffuse.Sample(DiffuseSampler,uv);"
                "if(LightEnabled==0||LightMode==0)return tex;"
                "float3 n=normalize(normalIn);float3 v=normalize(-positionIn);"
                "float3 illumination=AmbientColor.rgb*MaterialAmbient.rgb;float3 specular=0;"
                "if(LightMode==1){float3 travel=normalize(LightDirectionView);float d=max(dot(n,-travel),0);"
                "illumination+=DirectionalColor.rgb*d;if(d>0&&MaterialPower>0){"
                "float3 h=normalize(-travel+v);specular+=DirectionalColor.rgb*MaterialSpecular.rgb*"
                "pow(max(dot(n,h),0),MaterialPower);}}"
                "for(int i=0;i<4;++i){if(i>=LightCount)break;float3 toLight=LightPositionView[i]-positionIn;"
                "float distanceToLight=length(toLight);if(LightRadius[i]>0.0001){"
                "float3 l=toLight/max(distanceToLight,0.0001);float d=max(dot(n,l),0);"
                "float attenuation=1-saturate(distanceToLight/LightRadius[i]);attenuation*=attenuation;"
                "illumination+=LightColor[i].rgb*d*attenuation;}}"
                "float3 result=saturate(tex.rgb*MaterialDiffuse.rgb*saturate(illumination)+"
                "MaterialEmissive.rgb+specular);return float4(result,tex.a*MaterialDiffuse.a);}";
            mbm::FX *fx = mesh->getFx();
            if (!fx || !fx->loadNewShader(&customLightShader, nullptr,
                                           mbm::TYPE_ANIMATION_PAUSED, 1.0f,
                                           mbm::TYPE_ANIMATION_PAUSED, 1.0f,
                                           mbm::FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV))
            {
                ERROR_LOG("testLib: DirectX 11 custom reserved-light shader setup failed");
                automatedTestFailed = true;
                device->setRun(false);
                return;
            }
            INFO_LOG("testLib: DirectX 11 custom reserved-light shader smoke test started");
            return;
        }
        INFO_LOG("testLib: DirectX 11 3D/2DW directional and multi-point lighting smoke test started");
        return;
    }
    if (testDirectX11TextureFailure)
    {
        mbm::TEXTURE_MANAGER *textureManager = mbm::TEXTURE_MANAGER::getInstance();
        INFO_LOG("testLib: BEGIN expected invalid-texture error diagnostics");
        mbm::TEXTURE *first = textureManager->load("my-scene-test.cpp", true);
        mbm::TEXTURE *second = textureManager->load("my-scene-test.cpp", true);
        INFO_LOG("testLib: END expected invalid-texture error diagnostics");
        if (first || second || textureManager->existTexture("my-scene-test.cpp"))
        {
            ERROR_LOG("testLib: DirectX 11 invalid native texture was returned or cached");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        INFO_LOG("testLib: DirectX 11 invalid native texture rejection passed");
        device->setRun(false);
        return;
    }
    if (testDirectX11ScreenSize)
    {
        int expectedWidth = 0;
        int expectedHeight = 0;
        int actualWidth = 0;
        int actualHeight = 0;
        util::getDisplayMetrics(&expectedWidth, &expectedHeight);
        device->getCoreManager()->getScreenSize(&actualWidth, &actualHeight);
        if (actualWidth <= 0 || actualHeight <= 0 ||
            actualWidth != expectedWidth || actualHeight != expectedHeight)
        {
            ERROR_LOG("testLib: DirectX 11 screen-size query mismatch expected=%dx%d actual=%dx%d",
                      expectedWidth, expectedHeight, actualWidth, actualHeight);
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        INFO_LOG("testLib: DirectX 11 screen-size query passed (%dx%d)", actualWidth, actualHeight);
        device->setRun(false);
        return;
    }
    if (testDirectX11Foundation)
    {
        if (testDirectX11ShaderProfiles &&
            (strcmp(mbm::getVSVersion(), "vs_4_1") != 0 || strcmp(mbm::getPSVersion(), "ps_4_1") != 0))
        {
            ERROR_LOG("testLib: DirectX 11 shader profile override was not preserved");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        shape = new mbm::SHAPE_MESH(this, false, true);
        if (!shape->loadRectangle("directx11-basic-quad", 240.0f, 160.0f, false, 2))
        {
            ERROR_LOG("testLib: DirectX 11 basic quad buffer/shader setup failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        render2Texture = new mbm::RENDER_2_TEXTURE(this, false, true);
        // Match editor thumbnail generators: keep the display quad offscreen while forcing
        // the capture pass to run. This guards each backend against bypassing alwaysRender.
        render2Texture->setPosition(mbm::VEC3(0.0f, -1000000.0f, 0.0f));
        render2Texture->setAlwaysRenderize(true);
        render2Texture->setRenderTargetClearColor(mbm::COLOR(
            static_cast<uint8_t>(17), static_cast<uint8_t>(34),
            static_cast<uint8_t>(51), static_cast<uint8_t>(68)));
        if (!render2Texture->load(320, 240, 320, 240, "directx11-render-target-smoke", true) ||
            !render2Texture->addObject2Render(shape))
        {
            ERROR_LOG("testLib: DirectX 11 render-to-texture setup failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        mbm::SHADER_CFG *tintShader = device->getShaderConfig().getShader("tint.ps");
        mbm::SHADER_CFG *scaleShader = device->getShaderConfig().getShader("scale.vs");
        mbm::FX *renderTargetFx = render2Texture->getFx();
        if (!tintShader || !scaleShader || !renderTargetFx ||
            !renderTargetFx->loadNewShader(tintShader, scaleShader,
                mbm::TYPE_ANIMATION_PAUSED, 1.0f, mbm::TYPE_ANIMATION_PAUSED, 1.0f,
                mbm::FVF_PROVIDE_BY_ENGINE::FVF_POS_UV))
        {
            ERROR_LOG("testLib: DirectX 11 custom pixel shader setup failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        line = new mbm::LINE_MESH(this, false, true);
        std::vector<mbm::VEC3> linePoints = {
            mbm::VEC3(-120.0f, -100.0f, 0.0f),
            mbm::VEC3(120.0f, 100.0f, 0.0f)
        };
        if (line->add(std::move(linePoints)) == 0xffffffffu)
        {
            ERROR_LOG("testLib: DirectX 11 line buffer/shader setup failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        particle = new mbm::PARTICLE(this, false, true);
        if (!particle->load("particle.png", nullptr, nullptr, 32, true) ||
            !particle->addParticle(32, true) || particle->addStage() == 0xffffffffu)
        {
            ERROR_LOG("testLib: DirectX 11 particle buffer/shader setup failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        particle->restartAnimationParticle();
        if (!render2Texture->addObject2Render(particle))
        {
            ERROR_LOG("testLib: DirectX 11 particle render-to-texture setup failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        hmd = new mbm::HMD(this);
        if (!hmd->load() || !hmd->addObject2Render(shape) || !hmd->addObject2Render(particle))
        {
            ERROR_LOG("testLib: DirectX 11 HMD render-target setup failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        mbm::INFO_PHYSICS particlePhysics;
        particlePhysics.lsCube.push_back(new mbm::CUBE(200.0f, 200.0f, 200.0f));
        mbm::COLOR steeredColor(1.0f, 0.0f, 0.0f, 1.0f);
        steeredParticle = new mbm::STEERED_PARTICLE(this, false, true, false, nullptr);
        if (!steeredParticle->load("particle.png", &steeredColor, &particlePhysics))
        {
            ERROR_LOG("testLib: DirectX 11 steered-particle buffer/shader setup failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        const uint32_t steeredGroup = steeredParticle->addGroup(&steeredColor);
        if (steeredGroup == 0 || !steeredParticle->addParticle(32, steeredGroup - 1u))
        {
            ERROR_LOG("testLib: DirectX 11 steered-particle buffer/shader setup failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        steeredParticle->setRadiusScale(2.0f);
        mbm::FLUID_GROUP *group = steeredParticle->getParticleGroup(steeredGroup - 1u);
        if (group)
            group->aSizeParticle = 20.0f;
        steeredParticle->restartAnimationParticle();
        steeredParticle->restartAnimation();
        randomSteeredParticlePositions();
        INFO_LOG("testLib: DirectX 11 custom shaders, render-to-texture, HMD, line, and particles smoke test started");
        return;
    }
    if (testDirectX11SkeletalParity && !runDirectX11SkeletalParityTests())
    {
        ERROR_LOG("testLib: DirectX 11 skeletal CPU/GPU parity failed");
        automatedTestFailed = true;
        device->setRun(false);
        return;
    }
#endif

#if defined(USE_OPENGL_ES)
    if (testGlesDqsShader)
    {
        mbm::SHADER shader;
        shader.setUseReservedLightDefault(true);
        if (!shader.compileShader(nullptr, nullptr, mbm::FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV,
                                  23, mbm::SKELETAL_SHADER_METHOD::DQS_RIGID))
        {
            ERROR_LOG("testLib: GLES rigid-DQS default shader compile failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        INFO_LOG("testLib: GLES rigid-DQS default shader compiled successfully for 23 bones");
    }
    if (testGlesSkeletalParity && !runGlesSkeletalParityTests())
    {
        ERROR_LOG("testLib: GLES skeletal CPU/GPU parity failed");
        automatedTestFailed = true;
        device->setRun(false);
        return;
    }
#endif

#if defined(USE_DIRECTX9)
    if (testDirectX9SkeletalParity && !runDirectX9SkeletalParityTests())
    {
        ERROR_LOG("testLib: DirectX 9 skeletal CPU/GPU parity failed");
        automatedTestFailed = true;
        device->setRun(false);
        return;
    }
#endif

#if defined(USE_METAL)
    if (testMetalRenderToTexture)
    {
        render2Texture = new mbm::RENDER_2_TEXTURE(this, false, true);
        render2Texture->setPosition(mbm::VEC3(0.0f, -1000000.0f, 0.0f));
        render2Texture->setAlwaysRenderize(true);
        render2Texture->setRenderTargetClearColor(mbm::COLOR(
            static_cast<uint8_t>(17), static_cast<uint8_t>(34),
            static_cast<uint8_t>(51), static_cast<uint8_t>(68)));
        if (!render2Texture->load(64, 64, 64, 64, "metal-render-target-smoke", true))
        {
            ERROR_LOG("testLib: Metal render-to-texture setup failed");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
    }
    if (testMetalSkeletalParity && !runMetalSkeletalParityTests())
    {
        ERROR_LOG("testLib: Metal skeletal CPU/GPU parity failed");
        automatedTestFailed = true;
        device->setRun(false);
        return;
    }
    if (testMetalEditorShaders)
    {
        const char *shaderSources[] = {
            "fragment float4 frag_main(VOut in [[stage_in]]) {"
            "float t=clamp(in.uv.x,0.0f,1.0f);return float4(t,1.0f-t,0.25f,1.0f);}",
            "fragment float4 frag_main(VOut in [[stage_in]]) {"
            "float influence=clamp(in.uv.x,0.0f,1.0f);if(influence<=0.001f) discard_fragment();"
            "return float4(1.0f,0.12f,0.05f,sqrt(influence)*0.65f);}"};
        for (uint32_t index = 0; index < 2; ++index)
        {
            mbm::BASE_SHADER pixelShader;
            pixelShader.loadShader(index == 0 ? "metal-editor-heat.ps" : "metal-editor-brush.ps",
                                   shaderSources[index]);
            mbm::SHADER shader;
            if (!shader.compileShader(&pixelShader, nullptr,
                                      mbm::FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV, 23,
                                      mbm::SKELETAL_SHADER_METHOD::DQS_RIGID))
            {
                ERROR_LOG("testLib: Metal editor shader %u compile failed", index);
                automatedTestFailed = true;
                device->setRun(false);
                return;
            }
        }
        INFO_LOG("testLib: Metal editor heatmap/brush shaders compiled with skeletal DQS vertex stage");
    }
#endif

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

        if (cliMeshMode != RenderMode::NONE)
        {
            for (size_t i = 0; i < menuItems.size(); ++i)
            {
                if (menuItems[i].objType == MenuObjectType::MESH)
                {
                    loadObjectAt(i, cliMeshMode);
                    break;
                }
            }
        }
    }
    else
    {
        ERROR_LOG("Failed to load font - menu will not be available");
    }
}

void MY_SCENE::onLoop()
{
    mbm::DEVICE* device = mbm::DEVICE::getInstance();
#if defined(__APPLE__)
    if (testMacOSInput)
    {
        if (testMacOSInputPhase == 0)
        {
            testMacOSInputPhase = 1;
            if (!postMacOSInputTestEvents())
            {
                ERROR_LOG("testLib: failed to post synthetic macOS input events");
                automatedTestFailed = true;
                device->setRun(false);
            }
            return;
        }
        if (testMacOSInputPhase == 1 && testCoreManager && testCoreManager->isKeyCapsLockOn())
        {
            testMacOSInputPhase = 2;
            if (!postMacOSCapsLockRelease())
            {
                ERROR_LOG("testLib: failed to post synthetic macOS Caps Lock release");
                automatedTestFailed = true;
                device->setRun(false);
            }
            return;
        }
        if (testMacOSInputPhase == 2 && testCoreManager && !testCoreManager->isKeyCapsLockOn())
        {
            const uint32_t expectedKeyMask = 0x3fu;
            const bool valid = testMacOSKeyDownMask == expectedKeyMask &&
                testMacOSKeyUpMask == expectedKeyMask && testMacOSTouchDownMask == 0x7u &&
                testMacOSTouchUpMask == 0x7u && testMacOSZoomMask == 0x3u &&
                testMacOSMoveReceived && testMacOSDoubleClickReceived && testMacOSCoordinatesValid;
            if (!valid)
            {
                ERROR_LOG("testLib: macOS input validation failed keys=%x/%x buttons=%x/%x zoom=%x move=%d double=%d coords=%d",
                    testMacOSKeyDownMask, testMacOSKeyUpMask, testMacOSTouchDownMask,
                    testMacOSTouchUpMask, testMacOSZoomMask, testMacOSMoveReceived,
                    testMacOSDoubleClickReceived, testMacOSCoordinatesValid);
                automatedTestFailed = true;
            }
            else
            {
                INFO_LOG("testLib: macOS keyboard/modifiers/mouse/scroll/double-click validation passed");
            }
            device->setRun(false);
            return;
        }
    }
    if (testMacOSClose && !testMacOSCloseRequested)
    {
        testMacOSCloseRequested = true;
        if (!requestMacOSWindowClose())
        {
            ERROR_LOG("testLib: failed to request macOS window close");
            automatedTestFailed = true;
            device->setRun(false);
        }
        return;
    }
    if (testMacOSResize && !testMacOSResizeRequested)
    {
        testMacOSResizeRequested = true;
        if (!requestMacOSWindowResize(960, 640))
        {
            ERROR_LOG("testLib: failed to request a 960x640 macOS content-area resize");
            automatedTestFailed = true;
            device->setRun(false);
        }
        return;
    }
    if (testMacOSResize &&
        static_cast<int>(device->getBackBufferWidth()) == 960 &&
        static_cast<int>(device->getBackBufferHeight()) == 640)
    {
        const bool valid = testMacOSResizeNotified && validateMacOSWindowResize(960, 640);
        if (!valid)
        {
            ERROR_LOG("testLib: macOS resize/Retina layer validation failed");
            automatedTestFailed = true;
        }
        else
        {
            INFO_LOG("testLib: macOS resize passed (logical size, scene callback, scale, and drawable)");
        }
        device->setRun(false);
        return;
    }
#endif
#if defined(USE_DIRECTX11)
    if (testDirectX11TextureStages)
    {
        if (testDirectX11TextureStagePhase == 0)
        {
            testDirectX11TextureStagePhase = 1;
            return;
        }
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        ID3D11ShaderResourceView *activeViews[6] = {};
        bool valid = context && context->immediateContext;
        uint32_t mismatchMask = valid ? 0u : 0x3fu;
        if (valid)
            context->immediateContext->PSGetShaderResources(0, 6, activeViews);
        for (uint32_t stage = 0; stage < 6; ++stage)
        {
            ID3D11ShaderResourceView *expected = testDirectX11StageTextures[stage] ?
                static_cast<ID3D11ShaderResourceView *>(
                    testDirectX11StageTextures[stage]->getBackendTexturePointer()) : nullptr;
            const bool stageValid = activeViews[stage] && activeViews[stage] == expected;
            if (!stageValid)
                mismatchMask |= 1u << stage;
            valid = valid && stageValid;
            if (activeViews[stage])
                activeViews[stage]->Release();
        }
        if (!valid)
        {
            ERROR_LOG("testLib: DirectX 11 texture stages 0-5 binding failed; mismatch mask 0x%02x",
                mismatchMask);
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        INFO_LOG("testLib: DirectX 11 texture stages 0-5 binding passed");
        device->setRun(false);
        return;
    }
    if (testDirectX11SamplerState)
    {
        if (testDirectX11SamplerPhase == 0)
        {
            testDirectX11SamplerPhase = 1;
            return;
        }
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        ID3D11SamplerState *samplers[6] = {};
        bool valid = context && context->immediateContext;
        if (valid)
            context->immediateContext->PSGetSamplers(0, 6, samplers);
        const D3D11_FILTER expectedFilter = testDirectX11SamplerPhase == 1 ?
            D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT : D3D11_FILTER_MIN_MAG_MIP_POINT;
        const D3D11_TEXTURE_ADDRESS_MODE expectedAddress = testDirectX11SamplerPhase == 1 ?
            D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
        for (ID3D11SamplerState *sampler : samplers)
        {
            D3D11_SAMPLER_DESC description = {};
            if (sampler)
                sampler->GetDesc(&description);
            valid = valid && sampler && description.Filter == expectedFilter &&
                    description.AddressU == expectedAddress && description.AddressV == expectedAddress &&
                    description.AddressW == expectedAddress && description.MaxLOD == D3D11_FLOAT32_MAX;
            if (sampler)
                sampler->Release();
        }
        if (!valid)
        {
            ERROR_LOG("testLib: DirectX 11 sampler-state binding failed in phase %d",
                      testDirectX11SamplerPhase);
            automatedTestFailed = true;
            device->enableFilteringAfterPixelPerfect();
            device->setRun(false);
            return;
        }
        if (testDirectX11SamplerPhase == 1)
        {
            device->disableFilteringForPixelPerfect();
            testDirectX11SamplerPhase = 2;
            return;
        }
        device->enableFilteringAfterPixelPerfect();
        INFO_LOG("testLib: DirectX 11 default and pixel-perfect sampler bindings passed all six slots");
        device->setRun(false);
        return;
    }
    if (testDirectX11Resize && !testDirectX11ResizeRequested)
    {
        testDirectX11ResizeRequested = true;
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        RECT windowRect = {};
        RECT clientRect = {};
        const HWND windowHandle = context ? context->window.getHwnd() : nullptr;
        const bool measured = windowHandle && GetWindowRect(windowHandle, &windowRect) &&
                              GetClientRect(windowHandle, &clientRect);
        const int borderWidth = measured ? (windowRect.right - windowRect.left) - clientRect.right : 0;
        const int borderHeight = measured ? (windowRect.bottom - windowRect.top) - clientRect.bottom : 0;
        if (!measured || !SetWindowPos(windowHandle, nullptr, 0, 0, 960 + borderWidth, 640 + borderHeight,
                                       SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE))
        {
            ERROR_LOG("testLib: failed to request a 960x640 DirectX 11 client-area resize");
            automatedTestFailed = true;
            device->setRun(false);
            return;
        }
        INFO_LOG("testLib: DirectX 11 window resize requested for a 960x640 client area");
        return;
    }
    if (testDirectX11Resize &&
        static_cast<int>(device->getBackBufferWidth()) == 960 &&
        static_cast<int>(device->getBackBufferHeight()) == 640)
    {
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        ID3D11RenderTargetView *renderTarget = nullptr;
        ID3D11DepthStencilView *depthView = nullptr;
        D3D11_VIEWPORT viewport = {};
        UINT viewportCount = 1;
        bool valid = context && context->immediateContext && context->backBufferView && context->depthView &&
                     testDirectX11ResizeNotified;
        if (valid)
        {
            context->immediateContext->OMGetRenderTargets(1, &renderTarget, &depthView);
            context->immediateContext->RSGetViewports(&viewportCount, &viewport);
            valid = renderTarget == context->backBufferView && depthView == context->depthView &&
                    viewportCount == 1 && static_cast<int>(viewport.Width) == 960 &&
                    static_cast<int>(viewport.Height) == 640;
        }
        if (renderTarget)
            renderTarget->Release();
        if (depthView)
            depthView->Release();
        if (!valid)
        {
            ERROR_LOG("testLib: DirectX 11 resize state validation failed");
            automatedTestFailed = true;
        }
        else
        {
            INFO_LOG("testLib: DirectX 11 resize passed (back buffer, depth buffer, viewport, and scene callback)");
        }
        device->setRun(false);
        return;
    }
#endif
    if (testTimeoutSeconds >= 0.0f)
    {
        testElapsedSeconds += device->delta;
#if defined(USE_DIRECTX11)
        if (testDirectX11Foundation)
        {
            if (testElapsedSeconds < testTimeoutSeconds * 0.5f)
                device->disableFilteringForPixelPerfect();
            else
                device->enableFilteringAfterPixelPerfect();
        }
#endif
        if (testElapsedSeconds >= testTimeoutSeconds)
        {
#if defined(USE_METAL)
            if (testMacOSInput)
            {
                ERROR_LOG("testLib: macOS input event validation timed out");
                automatedTestFailed = true;
            }
            if (testMacOSClose)
            {
                ERROR_LOG("testLib: macOS window close timed out");
                automatedTestFailed = true;
            }
            if (testMacOSResize)
            {
                ERROR_LOG("testLib: macOS resize event timed out");
                automatedTestFailed = true;
            }
            if (testMetalRenderToTexture)
            {
                const char *pngPath = "metal-render-target-smoke.png";
                unsigned char *pixels = nullptr;
                unsigned int width = 0;
                unsigned int height = 0;
                bool valid = render2Texture &&
                    render2Texture->saveAsPNG(pngPath, 0, 0, 64, 64);
                const unsigned int decodeError = valid ?
                    lodepng_decode32_file(&pixels, &width, &height, pngPath) : 1u;
                valid = valid && decodeError == 0u && width == 64u && height == 64u;
                const size_t sampleOffsets[] = {0u, 32u * 4u, (32u * 64u + 32u) * 4u,
                                                (63u * 64u + 63u) * 4u};
                for (const size_t offset : sampleOffsets)
                {
                    if (!valid)
                        break;
                    valid = pixels[offset] == 17u && pixels[offset + 1u] == 34u &&
                            pixels[offset + 2u] == 51u && pixels[offset + 3u] == 68u;
                }
                free(pixels);
                std::remove(pngPath);
                if (!valid)
                {
                    ERROR_LOG("testLib: Metal render-to-texture clear/readback validation failed");
                    automatedTestFailed = true;
                }
                else
                {
                    INFO_LOG("testLib: Metal render-to-texture clear/readback validation passed");
                }
            }
#endif
#if defined(USE_DIRECTX11)
            if (testDirectX11Resize)
            {
                ERROR_LOG("testLib: DirectX 11 resize event timed out");
                automatedTestFailed = true;
            }
            if (testDirectX11Foundation && render2Texture &&
                !render2Texture->saveAsPNG("directx11-render-target-smoke.png", 96, 72, 128, 96))
            {
                ERROR_LOG("testLib: DirectX 11 render-target PNG readback failed");
                automatedTestFailed = true;
            }
#endif
            INFO_LOG("testLib: test timeout of %.2fs reached, quitting.", testTimeoutSeconds);
            // setRun(false) lets CORE_MANAGER::onLoop's while(device->isRunning())
            // exit cleanly at the top of its next iteration. mbm::DEVICE::quit()
            // is NOT the right call here: it immediately deletes the DEVICE
            // singleton mid-frame (it's meant to run once, from GAME's destructor,
            // after the loop has already returned), which segfaults the next time
            // this same onLoop() call touches device.
            device->setRun(false);
            return;
        }
    }
    if (statusText)
    {
        const mbm::CAMERA &camera = device->getCamera();
        statusText->setText(
            "Mouse(%.0f,%.0f)  Cam2D(%.0f,%.0f)  Cam3D(%.0f,%.0f,%.0f)",
            mouseScreenX, mouseScreenY,
            camera.position2d.x, camera.position2d.y,
            camera.position.x, camera.position.y, camera.position.z);
    }
    if (notificationTimer > 0.0f)
    {
        notificationTimer -= device->delta;
        if (notificationTimer <= 0.0f)
        {
            notificationTimer = 0.0f;
            if (notificationText)
                notificationText->setEnableRender(false);
        }
    }
    for(size_t i = 0; i < menuItems.size(); i++)
    {
        MenuRow& row = menuItems[i];
        if (row.object)
        {
            mbm::VEC3 &angle = row.object->getAngle();
            if(row.object->is3DObject())
            {
                angle.y += device->delta * 3.0f;
            }
            else
            {
                angle.y = 0.0f;
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
            case mbm::TYPE_VAR_SHADER::VAR_INT:
            {
                snprintf(text, sizeof(text), "%s[%d]", var->name.c_str(), var->getCurrentInt());
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
    if (testMacOSInput)
    {
        if (key >= 0 && key <= 2)
            testMacOSTouchDownMask |= 1u << static_cast<uint32_t>(key);
        if (testMacOSObservedX < 0.0f)
        {
            testMacOSObservedX = x;
            testMacOSObservedY = y;
            testMacOSCoordinatesValid = x > 50.0f && x < 200.0f && y > 40.0f && y < 150.0f;
        }
        testMacOSCoordinatesValid = testMacOSCoordinatesValid &&
            std::fabs(x - testMacOSObservedX) < 0.01f && std::fabs(y - testMacOSObservedY) < 0.01f;
        return;
    }
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
        if(render2Texture->saveAsPNG("render2texture-out.png", 0, 0,
                                     render2Texture->getRenderTargetWidth(),
                                     render2Texture->getRenderTargetHeight()))
        {
            INFO_LOG("Saved render2Texture content to render2texture-out.png");
        }
        else
        {
            INFO_LOG("Failed to save render2Texture content");
        }
    }
}

void MY_SCENE::onTouchUp(int key, float x, float y)
{
    if (testMacOSInput)
    {
        if (key >= 0 && key <= 2)
            testMacOSTouchUpMask |= 1u << static_cast<uint32_t>(key);
        testMacOSCoordinatesValid = testMacOSCoordinatesValid &&
            std::fabs(x - testMacOSObservedX) < 0.01f && std::fabs(y - testMacOSObservedY) < 0.01f;
    }
}

void MY_SCENE::onTouchMove(int, float x, float y)
{
    if (testMacOSInput)
    {
        testMacOSMoveReceived = true;
        testMacOSCoordinatesValid = testMacOSCoordinatesValid &&
            std::fabs(x - testMacOSObservedX) < 0.01f && std::fabs(y - testMacOSObservedY) < 0.01f;
        return;
    }
    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    mouseScreenX = x;
    mouseScreenY = y;
    if(trackMouse)
    {
        mbm::VEC3 &position = trackMouse->getPosition();
        if(trackMouse->is3DObject())
        {
            device->transformeScreen2dToWorld3d_scaled(x, y, &position, 800.0f);
        }
        else if(trackMouse->is2dScreenObject())
        {
            position.x = x;
            position.y = y;
        }
        else
        {
            device->transformeScreen2dToWorld2d_scaled(x, y, position);
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
            lineFontIsOver->setEnableRender(true);
        }
        else
        {
            lineFontIsOver->setEnableRender(false);
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
        const mbm::VEC3 &position = textDraw->getPosition();
        linePoints[0] = mbm::VEC3(position.x,     position.y,     position.z);
        linePoints[1] = mbm::VEC3(position.x + w, position.y,     position.z);
        linePoints[2] = mbm::VEC3(position.x + w, position.y + h, position.z);
        linePoints[3] = mbm::VEC3(position.x,     position.y + h, position.z);

        lineFontIsOver->set(std::move(linePoints), 0);
    }
    
}

void MY_SCENE::onTouchZoom(float zoom)
{
    if (testMacOSInput)
    {
        if (zoom > 0.0f)
            testMacOSZoomMask |= 1u;
        if (zoom < 0.0f)
            testMacOSZoomMask |= 2u;
    }
}

void MY_SCENE::onDoubleClick(float x, float y, int key)
{
    if (testMacOSInput)
    {
        testMacOSDoubleClickReceived = key == 0;
        testMacOSCoordinatesValid = testMacOSCoordinatesValid &&
            std::fabs(x - testMacOSObservedX) < 0.01f && std::fabs(y - testMacOSObservedY) < 0.01f;
    }
}

void MY_SCENE::onFinalizeScene()
{
}

void MY_SCENE::onKeyDown(int key)
{
    if (testMacOSInput)
    {
        const int keys[] = {'A', 0xFFE1, 0xFFE3, 0xFFE9, 0xFFEB, 0xFFE5};
        for (uint32_t index = 0; index < 6; ++index)
        {
            if (key == keys[index])
                testMacOSKeyDownMask |= 1u << index;
        }
        return;
    }
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
    mbm::CAMERA &camera = device->getCamera();
    if (key == 39)      // right
        camera.position2d.x += 10;
    else if (key == 37) // left
        camera.position2d.x -= 10;
    else if (key == 38) // up
        camera.position2d.y += 10;
    else if (key == 40) // down
        camera.position2d.y -= 10;
}

void MY_SCENE::onKeyUp(int key)
{
    if (testMacOSInput)
    {
        const int keys[] = {'A', 0xFFE1, 0xFFE3, 0xFFE9, 0xFFEB, 0xFFE5};
        for (uint32_t index = 0; index < 6; ++index)
        {
            if (key == keys[index])
                testMacOSKeyUpMask |= 1u << index;
        }
    }
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
#if defined(__APPLE__)
    if (testMacOSResize)
        testMacOSResizeNotified = true;
#endif
#if defined(USE_DIRECTX11)
    if (testDirectX11Resize)
        testDirectX11ResizeNotified = true;
#endif
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
        row.labelText->setScale(mbm::VEC3(1.0f, 1.0f, 1.0f));
        row.labelText->forceCalcSize();
        row.labelText->getPosition().z = -1.0f;
        row.labelText->setAlwaysRenderize(true);
        row.labelText->setEnableRender(false);

        menuItems.push_back(row);
    }
    float latestY = 0.0f;
    for (size_t i = 0; i < sizeof(defs) / sizeof(defs[0]); i++)
    {
        MenuRow& row = menuItems[i];
        mbm::VEC3 &position = row.labelText->getPosition();
        position.x = 10.0f;
        position.y = 10.0f + static_cast<float>(i) * 50.0f;
        latestY = position.y;
    }
    // Hints text — always visible at the bottom of the screen
    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    hintsText = this->fontDrawNoShader->addText("[M] menu | [P] pos | [S] shader | [Arrows] camera", IS_2D_FONT, IS_SCREEN);
    hintsText->setScale(mbm::VEC3(0.5f, 0.5f, 0.5f));
    hintsText->forceCalcSize();
    float hw = 0.0f, hh = 0.0f;
    hintsText->getAABB(&hw, &hh);
    mbm::VEC3 &hintsPosition = hintsText->getPosition();
    hintsPosition.x = 10.0f;
    hintsPosition.y = static_cast<float>(device->getBackBufferHeight()) - hh - 5.0f;
    hintsPosition.z = -1.0f;
    hintsText->setAlwaysRenderize(true);
    hintsText->setEnableRender(true);


    shaderInfoText = this->fontDrawNoShader->addText("Shader information", IS_2D_FONT, IS_SCREEN);
    shaderInfoText->setScale(mbm::VEC3(0.5f, 0.5f, 0.5f));
    shaderInfoText->forceCalcSize();
    mbm::VEC3 &shaderInfoPosition = shaderInfoText->getPosition();
    shaderInfoPosition.x = 10.0f;
    shaderInfoPosition.y = latestY + 50.0f;
    shaderInfoPosition.z = -1.0f;
    shaderInfoText->setAlwaysRenderize(true);
    shaderInfoText->setEnableRender(true);

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

    row.labelText->setEnableRender(menuVisible);
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
            const bool isCustomMesh = !cliMeshFile.empty();
            const char* meshFile = isCustomMesh ? cliMeshFile.c_str() : "Crate.msh";
            if (isCustomMesh && !mesh->setSkeletalSkinningMethod(cliSkeletalMethod))
                ERROR_LOG("Failed to select skeletal skinning method [%s]", meshFile);
            if (isCustomMesh && cliSkeletalExecutionPathSet &&
                !mesh->setSkeletalExecutionPath(cliSkeletalExecutionPath))
                ERROR_LOG("Failed to select skeletal execution path [%s]", meshFile);
            if (mesh->load(meshFile))
            {
                if (!isCustomMesh)
                    mesh->setScale(mbm::VEC3(3.5f, 3.5f, 3.5f)); // tuned for the bundled Crate.msh fixture only
                if (isCustomMesh && mesh->getTotalSkeletalAnimations() > 0)
                {
                    const char *animationName = mesh->getSkeletalAnimationName(0);
                    if (!animationName || !mesh->playSkeletalAnimation(animationName))
                        ERROR_LOG("Failed to start first skeletal animation [%s]", meshFile);
                    else
                        INFO_LOG("Skeletal animation started [%s]", animationName);
                }
                const char *status = nullptr, *reason = nullptr, *executionPath = nullptr, *executionStatus = nullptr;
                const char *requestedExecutionPath = nullptr, *resolvedExecutionPath = nullptr;
                const char *executionReason = nullptr;
                uint32_t requiredBones = 0, capacity = 0;
                mesh->getSkeletalSkinningReport(&status, &reason, &requiredBones, &capacity,
                                                &executionPath, &executionStatus,
                                                &requestedExecutionPath, &resolvedExecutionPath,
                                                &executionReason);
                INFO_LOG("MESH loaded (%s) [%s] skeletal execution=%s/%s status=%s reason=%s skinning=%s/%s",
                         modeToStr(mode), meshFile,
                         requestedExecutionPath ? requestedExecutionPath : "auto",
                         resolvedExecutionPath ? resolvedExecutionPath : "gpu",
                         executionStatus ? executionStatus : "unknown",
                         executionReason ? executionReason : "unknown", status ? status : "unknown",
                         reason ? reason : "unknown");
                row.object = mesh;
            }
            else
            {
                ERROR_LOG("Failed to load MESH [%s]", meshFile);
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
            line->setEnableRender(true);
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
                tile->setScale(mbm::VEC3(0.3f, 0.3f, 0.3f));
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
            row.object->getPosition().z = 0.0f;
    }
    addObjectsToRender2Texture();
    updateMenuRow(i);
    applyPosPreset(posMenuSelected);
    if (row.object)
    {
        const bool insideR2T = (render2Texture != nullptr) && (row.object != render2Texture);
        const mbm::VEC3 &position = row.object->getPosition();
        showNotification("%s loaded (%s) %s | pos(%.0f,%.0f,%.0f)",
            row.typeName, modeToStr(mode),
            insideR2T ? "in render2texture" : "in scene",
            position.x, position.y, position.z);
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
        if (btn2dS->isRenderEnabled() && btn2dS->getText().find("[x]") != std::string::npos && row.labelText->isOver2ds(device, x, y))
        {
            if(row.labelText->getText().find("[X]") != std::string::npos)
                releaseObjectAt(i);
            else
                loadObjectAt(i, RenderMode::SCREEN_2D);
            return true;
        }
        if (btn2dW->isRenderEnabled() && btn2dW->getText().find("[x]") != std::string::npos && row.labelText->isOver2ds(device, x, y))
        {
            if(row.labelText->getText().find("[X]") != std::string::npos)
                releaseObjectAt(i);
            else
                loadObjectAt(i, RenderMode::WORLD_2D);
            return true;
        }
        if (btn3d->isRenderEnabled() && btn3d->getText().find("[x]") != std::string::npos && row.labelText->isOver2ds(device, x, y))
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
        posMenuTexts[j]->setScale(mbm::VEC3(1.0f, 1.0f, 1.0f));
        posMenuTexts[j]->forceCalcSize();
        posMenuTexts[j]->getPosition().z = -1.0f;
        posMenuTexts[j]->setAlwaysRenderize(true);
        posMenuTexts[j]->setEnableRender(true);

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
            mbm::VEC3 &position = posMenuTexts[j]->getPosition();
            position.x = static_cast<float>(device->getBackBufferWidth()) - maxWidth - 10.0f;
            position.y = 10.0f + static_cast<float>(j) * (maxHeight + 5.0f);
        }
    }

    // Status text: one line above hints
    float hw = 0.0f, hh = 0.0f;
    if (hintsText)
        hintsText->getAABB(&hw, &hh);
    const float statusY = static_cast<float>(device->getBackBufferHeight()) - hh * 2.0f - 12.0f;
    statusText = this->fontDrawNoShader->addText(
        "Mouse(0,0)  Cam2D(0,0)  Cam3D(0,0,0)", mbm::VEC2(10.0f, statusY), IS_2D_FONT, IS_SCREEN);
    statusText->setScale(mbm::VEC3(0.5f, 0.5f, 0.5f));
    statusText->forceCalcSize();
    statusText->getPosition().z = -1.0f;
    statusText->setAlwaysRenderize(true);
    statusText->setEnableRender(true);

    // Notification text: one line above statusText
    const float notifyY = statusY - hh - 5.0f;
    notificationText = this->fontDrawNoShader->addText("", mbm::VEC2(10.0f, notifyY), IS_2D_FONT, IS_SCREEN);
    notificationText->setScale(mbm::VEC3(0.5f, 0.5f, 0.5f));
    notificationText->forceCalcSize();
    notificationText->getPosition().z = -1.0f;
    notificationText->setAlwaysRenderize(true);
    notificationText->setEnableRender(false);
}

void MY_SCENE::buildWorldMenu()
{
    float maxWidth  = 0.0f;
    float maxHeight = 0.0f;
    float hw = 0.0f, hh = 0.0f;
    constexpr bool  IS_2D_FONT  = true;
    constexpr bool  IS_SCREEN   = true;
    btn2dS = this->fontDrawNoShader->addText("[x](2dS)", mbm::VEC2(0.0f, 0.0f), IS_2D_FONT, IS_SCREEN);
    btn2dS->setScale(mbm::VEC3(1.0f, 1.0f, 1.0f));
    btn2dS->forceCalcSize();
    btn2dS->getPosition().z = -1.0f;
    btn2dS->setAlwaysRenderize(true);
    btn2dS->setEnableRender(true);
    btn2dS->getAABB(&maxWidth, &maxHeight);

    btn2dW = this->fontDrawNoShader->addText("[ ](2dW)", mbm::VEC2(0.0f, 0.0f), IS_2D_FONT, IS_SCREEN);
    btn2dW->setScale(mbm::VEC3(1.0f, 1.0f, 1.0f));
    btn2dW->forceCalcSize();
    btn2dW->getPosition().z = -1.0f;
    btn2dW->setAlwaysRenderize(true);
    btn2dW->setEnableRender(true);
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
    btn3d->setScale(mbm::VEC3(1.0f, 1.0f, 1.0f));
    btn3d->forceCalcSize();
    btn3d->getPosition().z = -1.0f;
    btn3d->setAlwaysRenderize(true);
    btn3d->setEnableRender(true);
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
    mbm::VEC3 &position2dS = btn2dS->getPosition();
    mbm::VEC3 &position2dW = btn2dW->getPosition();
    mbm::VEC3 &position3d  = btn3d->getPosition();
    position2dS.x = device->getBackBufferWidth() - (maxWidth * 3) - widthM;
    position2dS.y = defaultPosY;
    position2dW.x = device->getBackBufferWidth() - (maxWidth * 2) - widthM;
    position2dW.y = defaultPosY;
    position3d.x  = device->getBackBufferWidth() - maxWidth;
    position3d.y  = defaultPosY;
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
        //posMenuTexts[j]->setEnableRender(posMenuVisible && (render2Texture == nullptr || j == 0));
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
        mbm::VEC3 &position = row.object->getPosition();
        position.x = 0.0f;
        position.y = 0.0f;
        position.z = (row.currentMode == RenderMode::WORLD_3D) ? 0.0f : position.z;
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
        backBufferHeight = static_cast<float>(render2Texture->getRenderTargetHeight());
        backBufferWidth = static_cast<float>(render2Texture->getRenderTargetWidth());
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

    mbm::VEC3 &position = row.object->getPosition();
    const float savedZ = position.z;
    const bool insideR2T = (render2Texture != nullptr) && (row.object != render2Texture);
    if (row.currentMode == RenderMode::SCREEN_2D)
    {
        if (insideR2T)
        {
            // render2texture camera uses matrixOrthoLH(tw, th). The SCREEN_2D render path
            // calls transformeScreen2dToWorld2d_scaled(position) at draw time, then applies
            // matrixPerspective2d. We need world = (sx - tw/2, -(sy - th/2)), so store the
            // main-screen-equivalent pixel coords that produce those world coords.
            position.x = device->getBackBufferWidth() * 0.5f + sx - backBufferWidth * 0.5f;
            position.y = device->getBackBufferHeight() * 0.5f - backBufferHeight * 0.5f + sy;
        }
        else
        {
            position.x = sx;
            position.y = sy;
        }
        position.z = savedZ;
    }
    else if (row.currentMode == RenderMode::WORLD_2D)
    {
        if (insideR2T)
        {
            // render2texture camera uses matrixOrthoLH(tw, th) which maps [-tw/2, tw/2] to
            // clip space. WORLD_2D render path uses position directly — place in texture world.
            position.x = sx - backBufferWidth * 0.5f;
            position.y = -(sy - backBufferHeight * 0.5f);
            position.z = savedZ;
        }
        else
        {
            device->transformeScreen2dToWorld2d_scaled(sx, sy, position);
            position.z = savedZ;
        }
    }
    else // WORLD_3D
    {
        device->transformeScreen2dToWorld3d_scaled(sx, sy, &position, 800.0f);
        // z is determined by the 3D transform — intentional (how far)
    }
}

bool MY_SCENE::handleWorldMenuTouchDown(float x, float y, RenderMode& mode_selected)
{
    mbm::DEVICE* device = mbm::DEVICE::getInstance();
    if(btn3d && btn3d->isRenderEnabled() && btn3d->isOver2ds(device, x, y))
    {
        btn2dS->setText("[ ](2dS)");
        btn2dW->setText("[ ](2dW)");
        btn3d->setText("[x](3d)");
        mode_selected = RenderMode::WORLD_3D;
        return true;
    }

    if(btn2dW && btn2dW->isRenderEnabled() && btn2dW->isOver2ds(device, x, y))
    {
        btn2dS->setText("[ ](2dS)");
        btn2dW->setText("[x](2dW)");
        btn3d->setText("[ ](3d)");
        mode_selected = RenderMode::WORLD_2D;
        return true;
    }

    if(btn2dS && btn2dS->isRenderEnabled() && btn2dS->isOver2ds(device, x, y))
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
        if (posMenuTexts[j] && posMenuTexts[j]->isRenderEnabled() &&
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
    notificationText->setEnableRender(true);
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
        t->setScale(mbm::VEC3(0.75f, 0.75f, 0.75f));
        t->forceCalcSize();
        t->getPosition().z = -1.0f;
        t->setAlwaysRenderize(true);
        t->setEnableRender(true);// initially visible
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

    mbm::VEC3 &psLabelPosition = shaderRowPS.labelText->getPosition();
    mbm::VEC3 &psPrevPosition  = shaderRowPS.btnPrev->getPosition();
    mbm::VEC3 &psNextPosition  = shaderRowPS.btnNext->getPosition();
    psLabelPosition.x = labelX;
    psLabelPosition.y = rowY;
    psPrevPosition.x  = prevX;
    psPrevPosition.y  = rowY;
    psNextPosition.x  = nextX;
    psNextPosition.y  = rowY;
    rowY += spacing;

    mbm::VEC3 &vsLabelPosition = shaderRowVS.labelText->getPosition();
    mbm::VEC3 &vsPrevPosition  = shaderRowVS.btnPrev->getPosition();
    mbm::VEC3 &vsNextPosition  = shaderRowVS.btnNext->getPosition();
    vsLabelPosition.x = labelX;
    vsLabelPosition.y = rowY;
    vsPrevPosition.x  = prevX;
    vsPrevPosition.y  = rowY;
    vsNextPosition.x  = nextX;
    vsNextPosition.y  = rowY;
    rowY += spacing;

    mbm::VEC3 &pausePosition = shaderBtnPause->getPosition();
    pausePosition.x = labelX;
    pausePosition.y = rowY;
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
    shaderRowPS.labelText->setEnableRender(visible);
    shaderRowPS.btnPrev->setEnableRender(visible);
    shaderRowPS.btnNext->setEnableRender(visible);

    if (currentVsShaderIdx >= 0 && currentVsShaderIdx < static_cast<int>(device->getShaderConfig().lsVs.size()))
        shaderRowVS.labelText->setText("[X] %s", device->getShaderConfig().lsVs[static_cast<size_t>(currentVsShaderIdx)]->fileName.c_str());
    else
        shaderRowVS.labelText->setText("[ ] (VS none)");
    shaderRowVS.labelText->setEnableRender(visible);
    shaderRowVS.btnPrev->setEnableRender(visible);
    shaderRowVS.btnNext->setEnableRender(visible);

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
    shaderBtnPause->setEnableRender(visible);
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
        const mbm::TYPE_CLASS typeClass = obj->getTypeClass();
        if(typeClass != mbm::TYPE_CLASS_STEERED_PARTICLE && typeClass != mbm::TYPE_CLASS_PARTICLE)
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

    if (shaderRowPS.btnPrev->isRenderEnabled() && shaderRowPS.btnPrev->isOver2ds(device, x, y))
    {
        if (psCount > 0)
        {
            currentPsShaderIdx = (currentPsShaderIdx <= 0) ? psCount - 1 : currentPsShaderIdx - 1;
            applyCurrentShaders();
        }
        return true;
    }
    if (shaderRowPS.btnNext->isRenderEnabled() && shaderRowPS.btnNext->isOver2ds(device, x, y))
    {
        if (psCount > 0)
        {
            currentPsShaderIdx = (currentPsShaderIdx + 1) % psCount;
            applyCurrentShaders();
        }
        return true;
    }
    if (shaderRowPS.labelText->isRenderEnabled() && shaderRowPS.labelText->isOver2ds(device, x, y))
    {
        currentPsShaderIdx = (currentPsShaderIdx >= 0) ? -1 : (psCount > 0 ? 0 : -1);
        applyCurrentShaders();
        return true;
    }

    if (shaderRowVS.btnPrev->isRenderEnabled() && shaderRowVS.btnPrev->isOver2ds(device, x, y))
    {
        if (vsCount > 0)
        {
            currentVsShaderIdx = (currentVsShaderIdx <= 0) ? vsCount - 1 : currentVsShaderIdx - 1;
            applyCurrentShaders();
        }
        return true;
    }
    if (shaderRowVS.btnNext->isRenderEnabled() && shaderRowVS.btnNext->isOver2ds(device, x, y))
    {
        if (vsCount > 0)
        {
            currentVsShaderIdx = (currentVsShaderIdx + 1) % vsCount;
            applyCurrentShaders();
        }
        return true;
    }
    if (shaderRowVS.labelText->isRenderEnabled() && shaderRowVS.labelText->isOver2ds(device, x, y))
    {
        currentVsShaderIdx = (currentVsShaderIdx >= 0) ? -1 : (vsCount > 0 ? 0 : -1);
        applyCurrentShaders();
        return true;
    }

    if (shaderBtnPause->isRenderEnabled() && shaderBtnPause->isOver2ds(device, x, y))
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
}
