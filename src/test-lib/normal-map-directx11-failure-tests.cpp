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
#include "specific-directx11-buffer.h"
#include "specific-directx11-context.h"
#include "faults/directx11-device-proxy.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>

namespace
{
    // Tokens hold no device/resource references. Their lifetime follows the
    // actual COM child even when mesh-manager ownership outlives this suite.
    struct COUNTERS
    {
        std::atomic<int> liveBuffers{0}, liveDerived{0};
    };
    const GUID tokenId = {0x238cb159,0x304c,0x49e6,{0xad,0x14,0x72,0x48,0x19,0x3e,0x92,0x41}};
    class RESOURCE_TOKEN final : public IUnknown
    {
        std::atomic<ULONG> refs{1};
        const bool derived;
        const std::shared_ptr<COUNTERS> counters;
    public:
        RESOURCE_TOKEN(bool value, const std::shared_ptr<COUNTERS> &state) noexcept : derived(value), counters(state)
        { ++counters->liveBuffers; if (derived) ++counters->liveDerived; }
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
}

int runDirectX11NormalMapFailureTests()
{
    using namespace mbm;
    int failures = 0;
    const auto check = [&failures](bool ok, const char *message)
    { if (!ok) { ++failures; std::printf("NORMAL MAP RECOVERY FAIL: %s\n",message); } return ok; };
    const char *dir = std::getenv("MBM_NORMAL_MAP_FIXTURE_DIR");
    if (!check(dir != nullptr,"fixture directory configured")) return -1;
    auto *device = DEVICE::getInstance();
    auto *context = device->getSpecificContextDevice();
    device->setLightTargetForRender(LIGHT_TARGET_3D);
    setLightEnabled(LIGHT_TARGET_3D,true);
    MatrixIdentity(&SHADER::modelView);
    MatrixIdentity(&SHADER::mvMatrixLightSpace);
    MatrixIdentity(&SHADER::mvpMatrix);
    struct CASE { const char *method; unsigned nth; const char *name; bool upload; };
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
        {"CreateBuffer",6,"derived-indices",true}
    };
    for (const auto &entry : cases)
    {
        const int before = failures;
        const auto path = std::filesystem::path(dir) / (std::string("fault-")+entry.name+".msh");
        std::error_code error;
        std::filesystem::copy_file(std::filesystem::path(dir)/"author-import.msh",path,
                                  std::filesystem::copy_options::overwrite_existing,error);
        if (!check(!error,"copy independent fixture")) return -1;
        const auto *mesh = MESH_MANAGER::getInstance()->load(path.string().c_str());
        if (!check(mesh != nullptr,"load prepared fixture")) return -1;
        auto *buffer = mesh->getBuffer(0)->pBufferGL;
        SHADER shader;
        shader.setUseReservedLightDefault(true);
        if (!check(shader.compileShader(nullptr,nullptr,buffer->fvf) && shader.render(buffer),
                   "geometric baseline")) return -1;
        ID3D11VertexShader *geometric = nullptr;
        context->immediateContext->VSGetShader(&geometric,nullptr,nullptr);
        // Identity remains valid because shader owns its geometric pipeline.
        if (geometric) geometric->Release();
        check(geometric != nullptr,"geometric shader bound");
        check(mesh->setMaterialTexture(0,0,TEXTURE_ROLE_NORMAL,"#FF8080FF",false),"assign normal map");
        check(mesh->setNormalMapSettings(0,0,1,1),"enable normal map");
        TRACKED_DEVICE proxy(context);
        const auto &counts = *proxy.counters;
        proxy.arm(entry.method,entry.nth);
        const bool firstDraw = shader.render(buffer);
        context->immediateContext->Flush();
#if USE_NORMAL_MAPPING_3D
        check(!firstDraw && proxy.hits == 1,"injected creation failure reached draw");
        check(buffer->getBackendBuffer()->normalMapSubsets.empty(),"failed upload never published");
        check(counts.liveDerived == 0,"partial derived buffers released");
        if (!entry.upload) check(counts.liveBuffers == 0,"failed shader buffers released");
        check(mesh->setNormalMapSettings(0,0,1,0),"disable normal strength after failure");
        proxy.arm();
        check(shader.render(buffer) && proxy.calls == 0,"geometric path survives failure");
        ID3D11VertexShader *fallback = nullptr;
        context->immediateContext->VSGetShader(&fallback,nullptr,nullptr);
        check(fallback == geometric,"same geometric shader after failure");
        if (fallback) fallback->Release();
        check(mesh->setNormalMapSettings(0,0,1,1),"restore normal strength");
        if (entry.upload)
        {
            // The mapped shader is cached, so only the upload is retried.
            proxy.arm("CreateBuffer",2);
            check(!shader.render(buffer) && proxy.hits == 1 && proxy.shaderCalls == 0,
                  "second upload failure preserves compiled variant");
            context->immediateContext->Flush();
            check(counts.liveDerived == 0 && buffer->getBackendBuffer()->normalMapSubsets.empty(),
                  "second partial upload discarded");
        }
        proxy.arm();
        check(shader.render(buffer),"retry succeeds using retained preparation");
        check(!buffer->getBackendBuffer()->normalMapSubsets.empty() && counts.liveDerived == 2,
              "retry publishes one vertex/index batch");
        if (entry.upload) check(proxy.shaderCalls == 0 && proxy.calls == 2,"retry only creates missing buffers");
        proxy.arm();
        for (int i = 0; i < 20; ++i) check(shader.render(buffer),"warm mapped draw");
        check(proxy.calls == 0,"warm draws reuse recovered resources");
#else
        check(firstDraw && proxy.hits == 0 && proxy.calls == 0,"OFF performs no mapped resource creation");
        check(counts.liveBuffers == 0 && counts.liveDerived == 0,"OFF has no derived resources");
#endif
        check(proxy.tracked,"all created resources tracked");
        std::printf("NORMAL MAP RECOVERY CASE %s %s normal=%d\n",entry.name,
                    failures == before ? "PASS" : "FAIL",USE_NORMAL_MAPPING_3D);
    }
    std::printf("NORMAL MAP RECOVERY %s cases=11 normal=%d\n",failures ? "FAIL" : "PASS",USE_NORMAL_MAPPING_3D);
    return failures ? -1 : 0;
}
#endif
