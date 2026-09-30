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


#include "normal-map-native-resource-tests.h"
#if defined(USE_DIRECTX11)
#include <core_mbm/device.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/light.h>
#include <core_mbm/camera.h>
#include <core_mbm/draw-compatibility.h>
#include "specific-directx11-buffer.h"
#include "specific-directx11-context.h"
#include "faults/directx11-device-proxy.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <fstream>
#include <algorithm>
#include <cstring>

namespace
{
    // Tokens hold no device/resource references. Their lifetime follows the
    // actual COM child even when mesh-manager ownership outlives this suite.
    struct COUNTERS
    {
        std::atomic<int> liveBuffers{0}, liveDerived{0}, createdDerived{0};
    };
    const GUID tokenId = {0x238cb159,0x304c,0x49e6,{0xad,0x14,0x72,0x48,0x19,0x3e,0x92,0x41}};
    class RESOURCE_TOKEN final : public IUnknown
    {
        std::atomic<ULONG> refs{1};
        const bool derived;
        const std::shared_ptr<COUNTERS> counters;
    public:
        RESOURCE_TOKEN(bool value, const std::shared_ptr<COUNTERS> &state) noexcept : derived(value), counters(state)
        {
            ++counters->liveBuffers;
            if (derived) { ++counters->liveDerived; ++counters->createdDerived; }
        }
        ~RESOURCE_TOKEN() { --counters->liveBuffers; if (derived) --counters->liveDerived; }
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void **out) override
        {
            if (!out) return E_POINTER;
            *out = nullptr;
            if (id != __uuidof(IUnknown)) return E_NOINTERFACE;
            *out = this; AddRef(); return S_OK;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return ++refs; }
        ULONG STDMETHODCALLTYPE Release() override
        { const ULONG count = --refs; if (!count) delete this; return count; }
    };
    class TRACKED_DEVICE final : public DIRECTX11_DEVICE_PROXY
    {
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context;
    public:
        bool tracked = true;
        const std::shared_ptr<COUNTERS> counters = std::make_shared<COUNTERS>();
        explicit TRACKED_DEVICE(mbm::SPECIFIC_AUX_CONTEXT_DEVICE *value)
            : DIRECTX11_DEVICE_PROXY(value->device), context(value) { context->device = this; }
        ~TRACKED_DEVICE() { context->device = real; }
        void created(ID3D11DeviceChild *child, bool derived) override
        {
            // The runtime may intern shader/state objects. Only buffers have
            // independent lifetimes here; whole-pipeline leaks are checked by
            // the existing post-teardown DX11 live-object validation.
            ID3D11Buffer *buffer = nullptr;
            if (FAILED(child->QueryInterface(__uuidof(ID3D11Buffer),reinterpret_cast<void **>(&buffer)))) return;
            auto *token = new RESOURCE_TOKEN(derived,counters);
            tracked = SUCCEEDED(buffer->SetPrivateDataInterface(tokenId,token)) && tracked;
            token->Release();
            buffer->Release();
        }
    };

    // All readback resources are created before installing the fault proxy.
    struct PIXEL_TARGET
    {
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE *context;
        ID3D11Texture2D *color = nullptr, *staging = nullptr;
        ID3D11RenderTargetView *view = nullptr, *oldView = nullptr;
        ID3D11DepthStencilView *oldDepth = nullptr;
        ID3D11DepthStencilState *oldDepthState = nullptr;
        ID3D11BlendState *oldBlend = nullptr;
        ID3D11RasterizerState *oldRasterizer = nullptr;
        D3D11_VIEWPORT oldViewport = {};
        mbm::MATRIX oldLightView;
        UINT stencil = 0, mask = 0;
        float blend[4] = {};
        bool saved = false;
        explicit PIXEL_TARGET(mbm::SPECIFIC_AUX_CONTEXT_DEVICE *value) noexcept : context(value) {}
        bool init()
        {
            D3D11_TEXTURE2D_DESC desc = {};
            desc.Width = desc.Height = 64;
            desc.MipLevels = desc.ArraySize = desc.SampleDesc.Count = 1;
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            if (FAILED(context->device->CreateTexture2D(&desc,nullptr,&color)) ||
                FAILED(context->device->CreateRenderTargetView(color,nullptr,&view))) return false;
            desc.Usage = D3D11_USAGE_STAGING;
            desc.BindFlags = 0;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            if (FAILED(context->device->CreateTexture2D(&desc,nullptr,&staging))) return false;
            auto *immediate = context->immediateContext;
            immediate->OMGetRenderTargets(1,&oldView,&oldDepth);
            immediate->OMGetDepthStencilState(&oldDepthState,&stencil);
            immediate->OMGetBlendState(&oldBlend,blend,&mask);
            immediate->RSGetState(&oldRasterizer);
            UINT count = 1;
            immediate->RSGetViewports(&count,&oldViewport);
            // This command runs before the normal frame loop updates the camera.
            // Match light-space conversion to the identity geometry matrices.
            auto &camera = mbm::DEVICE::getInstance()->getCamera();
            oldLightView = camera.matrixView;
            mbm::MatrixIdentity(&camera.matrixView);
            saved = true;
            return true;
        }
        void begin()
        {
            auto *immediate = context->immediateContext;
            immediate->OMSetRenderTargets(1,&view,nullptr);
            immediate->OMSetDepthStencilState(context->depthDisabledState,0);
            immediate->OMSetBlendState(nullptr,nullptr,0xffffffffu);
            const D3D11_VIEWPORT viewport = {0,0,64,64,0,1};
            immediate->RSSetViewports(1,&viewport);
            const float clear[4] = {};
            immediate->ClearRenderTargetView(view,clear);
        }
        bool read(std::vector<uint32_t> &pixels)
        {
            auto *immediate = context->immediateContext;
            immediate->CopyResource(staging,color);
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (FAILED(immediate->Map(staging,0,D3D11_MAP_READ,0,&mapped))) return false;
            pixels.resize(64*64);
            for (unsigned y = 0; y < 64; ++y)
                std::memcpy(pixels.data()+y*64,static_cast<const char *>(mapped.pData)+y*mapped.RowPitch,64*4);
            immediate->Unmap(staging,0);
            return true;
        }
        bool draw(mbm::SHADER &shader, mbm::BUFFER_GL *buffer, std::vector<uint32_t> &pixels)
        { begin(); return shader.render(buffer) && read(pixels); }
        ~PIXEL_TARGET()
        {
            auto *immediate = context->immediateContext;
            if (saved)
            {
                mbm::DEVICE::getInstance()->getCamera().matrixView = oldLightView;
                immediate->OMSetRenderTargets(1,&oldView,oldDepth);
                immediate->OMSetDepthStencilState(oldDepthState,stencil);
                immediate->OMSetBlendState(oldBlend,blend,mask);
                immediate->RSSetState(oldRasterizer);
                immediate->RSSetViewports(1,&oldViewport);
            }
            if (oldView) oldView->Release();
            if (oldDepth) oldDepth->Release();
            if (oldDepthState) oldDepthState->Release();
            if (oldBlend) oldBlend->Release();
            if (oldRasterizer) oldRasterizer->Release();
            if (view) view->Release();
            if (color) color->Release();
            if (staging) staging->Release();
        }
    };

    bool savePixels(const std::filesystem::path &path, const std::vector<uint32_t> &pixels)
    {
        if (pixels.size() != 64*64) return false;
        std::ofstream file(path,std::ios::binary);
        file << "P6\n64 64\n255\n";
        for (const uint32_t pixel : pixels)
        {
            const char rgb[3] = {static_cast<char>(pixel),static_cast<char>(pixel>>8),static_cast<char>(pixel>>16)};
            file.write(rgb,3);
        }
        return file.good();
    }

    bool savePartitionedFixture(const std::filesystem::path &path, bool twoSubsets = false)
    {
        using namespace mbm;
        // One non-indexed subset exceeds the 16-bit derived-batch limit.
        // Distinct source indices prevent deduplication of repeated triangles.
        const uint32_t vertices = twoSubsets ? 6 : 65538;
        MESH_MBM_DEBUG mesh;
        mesh.addBuffer();
        mesh.addSubset(0);
        auto *frame = mesh.getFrameBuffer(0);
        frame->position = new float[vertices * 3]{};
        frame->normal = new float[vertices * 3]{};
        frame->uv = new float[vertices * 2]{};
        for (uint32_t i = 0; i < vertices; ++i)
        {
            const float x = i % 3 == 1 ? 1.0f : 0.0f;
            const float y = i % 3 == 2 ? 1.0f : 0.0f;
            // Show one triangle from each batch in separate image halves.
            // Keep the remaining triangles subpixel to avoid excessive overdraw.
            const bool first = i < 3, last = i >= vertices-3;
            frame->position[i*3] = -0.9f + x*0.00001f;
            frame->position[i*3+1] = -0.9f + y*0.00001f;
            if (first || last)
            {
                frame->position[i*3] = (first ? -0.8f : 0.2f) + x*0.6f;
                frame->position[i*3+1] = -0.4f + y*0.8f;
            }
            frame->normal[i*3+2] = 1;
            frame->uv[i*2] = x;
            frame->uv[i*2+1] = y;
        }
        frame->headerFrame.sizeVertexBuffer = vertices;
        frame->headerFrame.totalSubset = 1;
        auto *subset = mesh.getSubset(0,0);
        subset->vertexCount = vertices;
        subset->vertexStart = subset->indexCount = subset->indexStart = 0;
        subset->texture = "#FFFFFFFF";
        if (twoSubsets)
        {
            subset->vertexCount = 3;
            mesh.addSubset(0);
            frame->headerFrame.totalSubset = 2;
            auto *second = mesh.getSubset(0,1);
            second->vertexStart = second->vertexCount = 3;
            second->indexStart = second->indexCount = 0;
            second->texture = "#FFFFFFFF";
        }
        mesh.setHasNormal(HAS_NOR_IN_FILE);
        mesh.setHasTexture(HAS_TEX_EACH_FRAME);
        mesh.setMeshType(util::TYPE_MESH_3D);
        mesh.setModeDraw(util::MODE_DRAW_TRIANGLES);
        const std::vector<NORMAL_MAP_CORNER> corners(vertices,NORMAL_MAP_CORNER{1,0,0,1});
        NORMAL_MAP_REPORT report;
        char error[512] = {};
        bool prepared = true;
        for (unsigned s = 0; s < (twoSubsets ? 2u : 1u); ++s)
        {
            const unsigned count = twoSubsets ? 3 : vertices;
            prepared = prepared && mesh.prepareNormalMap(0,s,NORMAL_MAP_POLICY::IMPORT,report,error,sizeof(error),
                                                        corners.data(),count);
            prepared = prepared && report.batches == (twoSubsets ? 1u : 2u) && report.vertices == count;
        }
        const bool saved = prepared && mesh.saveV11(path.string().c_str(),false,false,false,error,sizeof(error));
        if (!saved) std::printf("NORMAL MAP RECOVERY FAIL: partition fixture: %s\n",error);
        return saved;
    }
}

int runDirectX11NormalMapFailureTests()
{
    using namespace mbm;
    int failures = 0;
    const auto check = [&failures](bool ok, const char *message)
    { if (!ok) { ++failures; std::printf("NORMAL MAP RECOVERY FAIL: %s\n",message); } return ok; };
    const char *dir = std::getenv("MBM_NORMAL_MAP_FIXTURE_DIR");
    if (!check(dir != nullptr,"fixture directory configured")) return -1;
    if (!check(savePartitionedFixture(std::filesystem::path(dir)/"recovery-partition.msh"),
               "save real two-batch partition")) return -1;
    if (!check(savePartitionedFixture(std::filesystem::path(dir)/"recovery-subsets.msh",true),
               "save visible two-subset fixture")) return -1;
    auto *device = DEVICE::getInstance();
    auto *context = device->getSpecificContextDevice();
    PIXEL_TARGET target(context);
    if (!check(target.init(),"offscreen readback target")) return -1;
    resetLight(LIGHT_TARGET_3D);
    device->setLightTargetForRender(LIGHT_TARGET_3D);
    setLightEnabled(LIGHT_TARGET_3D,true);
    setAmbientLight(LIGHT_TARGET_3D,COLOR(0.1f,0.1f,0.1f,1));
    setDirectionalLight(LIGHT_TARGET_3D,VEC3(0,0,-1),COLOR(0.8f,0.8f,0.8f,1));
    MatrixIdentity(&SHADER::modelView);
    MatrixIdentity(&SHADER::mvMatrixLightSpace);
    MatrixIdentity(&SHADER::mvpMatrix);
    struct CASE
    {
        const char *method;
        unsigned nth;
        const char *name;
        bool upload;
        const char *fixture = "author-import.msh";
        unsigned batches = 1, subsets = 1;
    };
    const CASE cases[] = {
        {"CreateVertexShader",1,"vertex-shader",false},
        {"CreatePixelShader",1,"pixel-shader",false},
        {"CreateInputLayout",1,"input-layout",false},
        {"CreateSamplerState",1,"linear-sampler",false},
        {"CreateSamplerState",2,"nearest-sampler",false},
        {"CreateBuffer",1,"matrix-buffer",false},
        {"CreateBuffer",2,"light-buffer",false},
        {"CreateBuffer",3,"normal-settings",false},
        {"CreateBuffer",4,"zero-tangent",false},
        {"CreateBuffer",5,"derived-vertices",true},
        {"CreateBuffer",6,"derived-indices",true},
        {"CreateBuffer",7,"partition-second-vertices",true,"recovery-partition.msh",2,1},
        {"CreateBuffer",8,"partition-second-indices",true,"recovery-partition.msh",2,1},
        {"CreateBuffer",7,"subset-second-vertices",true,"recovery-subsets.msh",2,2},
        {"CreateBuffer",8,"subset-second-indices",true,"recovery-subsets.msh",2,2}
    };
    for (const auto &entry : cases)
    {
        const int before = failures;
        const auto path = std::filesystem::path(dir) / (std::string("fault-")+entry.name+".msh");
        std::error_code error;
        std::filesystem::copy_file(std::filesystem::path(dir)/entry.fixture,path,
                                  std::filesystem::copy_options::overwrite_existing,error);
        if (!check(!error,"copy independent fixture")) return -1;
        const auto *mesh = MESH_MANAGER::getInstance()->load(path.string().c_str());
        if (!check(mesh != nullptr,"load prepared fixture")) return -1;
        auto *buffer = mesh->getBuffer(0)->pBufferGL;
        buffer->mode_cull_face = util::CULL_FRONT_AND_BACK;
        check(buffer->totalSubset == entry.subsets,"fixture subset count");
        // A distinct asset and shader establish the fault-free mapped image.
        const auto referencePath = std::filesystem::path(dir)/(std::string("reference-")+entry.name+".msh");
        std::filesystem::copy_file(path,referencePath,std::filesystem::copy_options::overwrite_existing,error);
        if (!check(!error,"copy independent reference")) return -1;
        const auto *reference = MESH_MANAGER::getInstance()->load(referencePath.string().c_str());
        if (!check(reference != nullptr,"load reference")) return -1;
        auto *referenceBuffer = reference->getBuffer(0)->pBufferGL;
        referenceBuffer->mode_cull_face = util::CULL_FRONT_AND_BACK;
        SHADER referenceShader;
        referenceShader.setUseReservedLightDefault(true);
        std::vector<uint32_t> geometricPixels, mappedPixels, pixels;
        if (!check(referenceShader.compileShader(nullptr,nullptr,referenceBuffer->fvf) &&
                   target.draw(referenceShader,referenceBuffer,geometricPixels),"reference geometric pixels")) return -1;
        for (unsigned subset = 0; subset < entry.subsets; ++subset)
        {
            check(reference->setMaterialTexture(0,subset,TEXTURE_ROLE_NORMAL,"#FF8080FF",false),"reference map");
            check(reference->setNormalMapSettings(0,subset,1,1),"reference strength");
        }
        if (!check(target.draw(referenceShader,referenceBuffer,mappedPixels),"reference mapped pixels")) return -1;
        unsigned visible[2] = {}, changed[2] = {};
        for (unsigned i = 0; i < geometricPixels.size(); ++i)
        {
            const unsigned half = (i%64)/32;
            if ((geometricPixels[i]&0x00ffffffu) != 0) ++visible[half];
            if (geometricPixels[i] != mappedPixels[i]) ++changed[half];
        }
        check(visible[0]+visible[1] > 32,"reference has visible geometry");
        if (entry.batches == 2) check(visible[0] > 32 && visible[1] > 32,"both batches/subsets are visible");
#if USE_NORMAL_MAPPING_3D
        check(changed[0]+changed[1] > 32,"normal map changes reference pixels");
        if (entry.batches == 2) check(changed[0] > 32 && changed[1] > 32,"normal map affects both image halves");
#else
        check(mappedPixels == geometricPixels,"OFF reference ignores normal map");
#endif
        SHADER shader;
        shader.setUseReservedLightDefault(true);
        if (!check(shader.compileShader(nullptr,nullptr,buffer->fvf) && target.draw(shader,buffer,pixels) &&
                   pixels == geometricPixels,
                   "geometric baseline")) return -1;
        ID3D11VertexShader *geometric = nullptr;
        context->immediateContext->VSGetShader(&geometric,nullptr,nullptr);
        // Identity remains valid because shader owns its geometric pipeline.
        if (geometric) geometric->Release();
        check(geometric != nullptr,"geometric shader bound");
        for (unsigned subset = 0; subset < entry.subsets; ++subset)
        {
            check(mesh->setMaterialTexture(0,subset,TEXTURE_ROLE_NORMAL,"#FF8080FF",false),"assign normal map");
            check(mesh->setNormalMapSettings(0,subset,1,1),"enable normal map");
        }
        TRACKED_DEVICE proxy(context);
        const auto &counts = *proxy.counters;
        proxy.arm(entry.method,entry.nth);
        target.begin();
        const bool firstDraw = shader.render(buffer);
        context->immediateContext->Flush();
#if USE_NORMAL_MAPPING_3D
        check(!firstDraw && proxy.hits == 1,"injected creation failure reached draw");
        check(target.read(pixels) && std::all_of(pixels.begin(),pixels.end(),[](uint32_t p) { return p == 0; }),
              "failed draw publishes no pixels");
        check(buffer->getBackendBuffer()->normalMapSubsets.empty(),"failed upload never published");
        check(counts.liveDerived == 0,"partial derived buffers released");
        if (entry.upload)
            check(counts.createdDerived == static_cast<int>(entry.nth-5),"failure follows expected partial upload");
        if (!entry.upload) check(counts.liveBuffers == 0,"failed shader buffers released");
        for (unsigned subset = 0; subset < entry.subsets; ++subset)
            check(mesh->setNormalMapSettings(0,subset,1,0),"disable normal strength after failure");
        proxy.arm();
        check(target.draw(shader,buffer,pixels) && pixels == geometricPixels && proxy.calls == 0,
              "geometric pixels survive failure");
        ID3D11VertexShader *fallback = nullptr;
        context->immediateContext->VSGetShader(&fallback,nullptr,nullptr);
        check(fallback == geometric,"same geometric shader after failure");
        if (fallback) fallback->Release();
        for (unsigned subset = 0; subset < entry.subsets; ++subset)
            check(mesh->setNormalMapSettings(0,subset,1,1),"restore normal strength");
        if (entry.upload)
        {
            // The mapped shader is cached, so only the upload is retried.
            const unsigned retryFault = entry.batches == 1 ? 2 : entry.nth-4;
            const int createdBefore = counts.createdDerived;
            proxy.arm("CreateBuffer",retryFault);
            target.begin();
            check(!shader.render(buffer) && proxy.hits == 1 && proxy.shaderCalls == 0,
                  "second upload failure preserves compiled variant");
            check(target.read(pixels) && std::all_of(pixels.begin(),pixels.end(),[](uint32_t p) { return p == 0; }),
                  "second failed upload publishes no pixels");
            context->immediateContext->Flush();
            check(counts.liveDerived == 0 && buffer->getBackendBuffer()->normalMapSubsets.empty(),
                  "second partial upload discarded");
            check(counts.createdDerived-createdBefore == static_cast<int>(retryFault-1),
                  "retry rebuilt preceding batches before failing again");
        }
        proxy.arm();
        check(target.draw(shader,buffer,pixels) && pixels == mappedPixels,"retry matches fault-free mapped pixels");
        const auto &published = buffer->getBackendBuffer()->normalMapSubsets;
        check(published.size() == entry.subsets && counts.liveDerived == static_cast<int>(entry.batches*2),
              "retry publishes all vertex/index batches");
        unsigned batches = 0, indices = 0;
        for (const auto &subset : published)
        {
            check(subset.batches.size() == entry.batches/entry.subsets,"batch distribution across subsets");
            for (const auto &batch : subset.batches)
            {
                ++batches;
                indices += batch.indexCount;
                if (!check(batch.vertices && batch.indices,"complete native batch")) continue;
                D3D11_BUFFER_DESC vertexDesc = {}, indexDesc = {};
                batch.vertices->GetDesc(&vertexDesc);
                batch.indices->GetDesc(&indexDesc);
                // These non-indexed fixtures have one derived vertex per corner:
                // position(3), normal(3), UV(2), tangent(4); indices are uint16_t.
                check(vertexDesc.ByteWidth == batch.indexCount*12*sizeof(float) &&
                      indexDesc.ByteWidth == batch.indexCount*sizeof(uint16_t),
                      "native buffer sizes cover all fixture corners");
            }
        }
        check(batches == entry.batches && indices == buffer->sizeOfArrayVertex,"complete geometry after retry");
        if (entry.upload) check(proxy.shaderCalls == 0 && proxy.calls == entry.batches*2,
                                "retry rebuilds buffers without recompiling mapped shader");
        proxy.arm();
        for (int i = 0; i < 20; ++i) check(shader.render(buffer),"warm mapped draw");
        check(proxy.calls == 0,"warm draws reuse recovered resources");
        check(target.draw(shader,buffer,pixels) && pixels == mappedPixels && proxy.calls == 0,
              "warm pixels match reference without allocation");
#else
        check(firstDraw && proxy.hits == 0 && proxy.calls == 0,"OFF performs no mapped resource creation");
        check(counts.liveBuffers == 0 && counts.liveDerived == 0,"OFF has no derived resources");
        check(target.read(pixels) && pixels == mappedPixels,"OFF pixels match reference with fault armed");
#endif
        check(savePixels(std::filesystem::path(dir)/(std::string("pixels-")+entry.name+"-reference.ppm"),mappedPixels) &&
              savePixels(std::filesystem::path(dir)/(std::string("pixels-")+entry.name+"-recovered.ppm"),pixels),
              "write pixel comparison artifacts");
        check(proxy.tracked,"all created resources tracked");
        std::printf("NORMAL MAP RECOVERY PIXELS %s %s normal=%d visible=%u changed=%u\n",entry.name,
                    failures == before ? "PASS" : "FAIL",USE_NORMAL_MAPPING_3D,
                    visible[0]+visible[1],changed[0]+changed[1]);
        std::printf("NORMAL MAP RECOVERY CASE %s %s normal=%d\n",entry.name,
                    failures == before ? "PASS" : "FAIL",USE_NORMAL_MAPPING_3D);
    }
    std::printf("NORMAL MAP RECOVERY %s cases=15 normal=%d\n",failures ? "FAIL" : "PASS",USE_NORMAL_MAPPING_3D);
    return failures ? -1 : 0;
}
#endif
