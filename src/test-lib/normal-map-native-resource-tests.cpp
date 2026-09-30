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
#if defined(USE_DIRECTX9) || defined(USE_DIRECTX11) || defined(USE_METAL)
#include <core_mbm/device.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/light.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#if defined(USE_DIRECTX9)
#include <core_mbm/shader-resource.h>
#include <core_mbm/draw-compatibility.h>
#include <vector>
#include "specific-directx9-buffer.h"
#include "specific-directx9-shader.h"
#elif defined(USE_DIRECTX11)
#include "specific-directx11-buffer.h"
#include "specific-directx11-context.h"
#else
#include "specific-metal-buffer.h"
#endif

#if defined(USE_METAL)
// Observe the real encoder binding without exposing engine-private PSO storage.
@interface MBMNormalMapEncoderObserver : NSObject
@property(nonatomic, strong) id<MTLRenderCommandEncoder> encoder;
@property(nonatomic, strong) id<MTLRenderPipelineState> pipeline;
@end
@implementation MBMNormalMapEncoderObserver
- (id)forwardingTargetForSelector:(SEL)selector
{
    return self.encoder;
}
- (void)setRenderPipelineState:(id<MTLRenderPipelineState>)pipeline
{
    [self.encoder setRenderPipelineState:pipeline];
    self.pipeline = pipeline;
}
@end
#endif

namespace
{
    using namespace mbm;
#if defined(USE_METAL)
    // Optional native trace for the acceptance matrix; never enabled by the engine.
    struct METAL_CAPTURE
    {
        bool active = false;
        bool begin(id<MTLDevice> device)
        {
            const char *path = std::getenv("MBM_NORMAL_MAP_METAL_CAPTURE");
            if (!path || !path[0]) return true;
            auto *manager = [MTLCaptureManager sharedCaptureManager];
            auto *descriptor = [MTLCaptureDescriptor new];
            descriptor.captureObject = device;
            descriptor.destination = MTLCaptureDestinationGPUTraceDocument;
            descriptor.outputURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
            NSError *error = nil;
            active = [manager startCaptureWithDescriptor:descriptor error:&error];
            if (!active)
                std::printf("NORMAL MAP LAZY FAIL: Metal capture: %s\n", error.localizedDescription.UTF8String);
            return active;
        }
        ~METAL_CAPTURE()
        {
            if (active)
            {
                [[MTLCaptureManager sharedCaptureManager] stopCapture];
                std::printf("NORMAL MAP METAL CAPTURE SAVED\n");
            }
        }
    };
#endif
    // Own the native submission scope; this command runs after initGraphics,
    // before the engine loop. Never issue Metal draws without an encoder.
    struct SUBMISSION
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = DEVICE::getInstance()->getSpecificContextDevice();
        bool active = false;
#if defined(USE_METAL)
        id<MTLCommandBuffer> commands = nil;
        id<MTLTexture> color = nil, depth = nil;
        MBMNormalMapEncoderObserver *observer = nil;
#endif
        bool begin()
        {
#if defined(USE_DIRECTX9)
            active = SUCCEEDED(context->pd3dDevice->BeginScene());
#elif defined(USE_METAL)
            if (context->currentEncoder) return false;
            auto *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:context->metalLayer.pixelFormat
                width:64 height:64 mipmapped:NO];
            desc.usage = MTLTextureUsageRenderTarget;
            color = [context->mtlDevice newTextureWithDescriptor:desc];
            desc.pixelFormat = MTLPixelFormatDepth32Float;
            depth = [context->mtlDevice newTextureWithDescriptor:desc];
            if (!color || !depth) return false;
            auto *pass = [MTLRenderPassDescriptor renderPassDescriptor];
            pass.colorAttachments[0].texture = color;
            pass.colorAttachments[0].loadAction = MTLLoadActionClear;
            pass.colorAttachments[0].storeAction = MTLStoreActionDontCare;
            pass.depthAttachment.texture = depth;
            pass.depthAttachment.loadAction = MTLLoadActionClear;
            pass.depthAttachment.clearDepth = 1.0;
            pass.depthAttachment.storeAction = MTLStoreActionDontCare;
            commands = [context->commandQueue commandBuffer];
            observer = [MBMNormalMapEncoderObserver new];
            observer.encoder = [commands renderCommandEncoderWithDescriptor:pass];
            active = observer.encoder != nil;
            if (active)
            {
                id forwardingEncoder = observer;
                context->currentEncoder = forwardingEncoder;
            }
#else
            active = context && context->immediateContext;
#endif
            return active;
        }
        bool finish()
        {
            if (!active) return false;
            active = false;
#if defined(USE_DIRECTX9)
            return SUCCEEDED(context->pd3dDevice->EndScene());
#elif defined(USE_METAL)
            [context->currentEncoder endEncoding];
            context->currentEncoder = nil;
            [commands commit];
            [commands waitUntilCompleted];
            if (commands.status == MTLCommandBufferStatusError)
            {
                std::printf("NORMAL MAP LAZY FAIL: Metal submission: %s\n", commands.error.localizedDescription.UTF8String);
                return false;
            }
#endif
            return true;
        }
        ~SUBMISSION() { if (active) finish(); }
    };

    // Loaded fixtures have static source storage. updateDynamic requires writable
    // source buffers on DirectX; convert only that storage, preserving the staged
    // and derived normal-map resources whose invalidation is under test.
    bool makeSourceDynamic(BUFFER_GL *buffer)
    {
#if defined(USE_DIRECTX9)
        auto *backend = buffer->getBackendBuffer();
        auto *device = DEVICE::getInstance()->getSpecificContextDevice()->pd3dDevice;
        D3DVERTEXBUFFER_DESC desc = {};
        if (FAILED(backend->pVertexBuffer->GetDesc(&desc))) return false;
        IDirect3DVertexBuffer9 *replacement = nullptr;
        if (FAILED(device->CreateVertexBuffer(desc.Size, D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
                desc.FVF, D3DPOOL_DEFAULT, &replacement, nullptr))) return false;
        void *source = nullptr, *destination = nullptr;
        if (FAILED(backend->pVertexBuffer->Lock(0,0,&source,D3DLOCK_READONLY)))
        { replacement->Release(); return false; }
        const bool copied = SUCCEEDED(replacement->Lock(0,0,&destination,D3DLOCK_DISCARD));
        if (copied) { std::memcpy(destination,source,desc.Size); replacement->Unlock(); }
        backend->pVertexBuffer->Unlock();
        if (!copied) { replacement->Release(); return false; }
        backend->pVertexBuffer->Release();
        backend->pVertexBuffer = replacement;
#elif defined(USE_DIRECTX11)
        auto *backend = buffer->getBackendBuffer();
        auto *context = DEVICE::getInstance()->getSpecificContextDevice();
        D3D11_BUFFER_DESC desc = {};
        backend->vertexBuffer->GetDesc(&desc);
        D3D11_BUFFER_DESC stagingDesc = desc;
        stagingDesc.Usage = D3D11_USAGE_STAGING;
        stagingDesc.BindFlags = 0;
        stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ID3D11Buffer *staging = nullptr, *replacement = nullptr;
        if (FAILED(context->device->CreateBuffer(&stagingDesc,nullptr,&staging))) return false;
        context->immediateContext->CopyResource(staging,backend->vertexBuffer);
        D3D11_MAPPED_SUBRESOURCE mapped = {};
        if (FAILED(context->immediateContext->Map(staging,0,D3D11_MAP_READ,0,&mapped)))
        { staging->Release(); return false; }
        desc.Usage = D3D11_USAGE_DYNAMIC;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        D3D11_SUBRESOURCE_DATA initial = {};
        initial.pSysMem = mapped.pData;
        const bool copied = SUCCEEDED(context->device->CreateBuffer(&desc,&initial,&replacement));
        context->immediateContext->Unmap(staging,0);
        staging->Release();
        if (!copied) return false;
        backend->vertexBuffer->Release();
        backend->vertexBuffer = replacement;
        backend->dynamicVertexBuffer = true;
#else
        (void)buffer; // Metal source geometry is CPU-accessible already.
#endif
        return true;
    }

    uintptr_t currentProgram()
    {
        auto *context = DEVICE::getInstance()->getSpecificContextDevice();
#if defined(USE_METAL)
        id observer = context->currentEncoder;
        return reinterpret_cast<uintptr_t>((__bridge void *)[observer pipeline]);
#else
#if defined(USE_DIRECTX9)
        IDirect3DVertexShader9 *shader = nullptr;
        context->pd3dDevice->GetVertexShader(&shader);
#else
        ID3D11VertexShader *shader = nullptr;
        context->immediateContext->VSGetShader(&shader, nullptr, nullptr);
#endif
        const auto identity = reinterpret_cast<uintptr_t>(shader);
        if (shader) shader->Release(); // Get* owns a reference; identity is borrowed.
        return identity;
#endif
    }
#if USE_NORMAL_MAPPING_3D
    uintptr_t vertexIdentity(const BUFFER_GL *buffer)
    {
        const auto &subsets = buffer->getBackendBuffer()->normalMapSubsets;
        if (subsets.empty() || subsets[0].batches.empty()) return 0;
#if defined(USE_METAL)
        return reinterpret_cast<uintptr_t>((__bridge void *)subsets[0].batches[0].vertices);
#else
        return reinterpret_cast<uintptr_t>(subsets[0].batches[0].vertices);
#endif
    }
    size_t payloadBytes(const BUFFER_GL *buffer)
    {
        size_t bytes = 0;
        for (const auto &subset : buffer->getBackendBuffer()->normalMapSubsets)
            for (const auto &batch : subset.batches)
            {
                if (!batch.vertices || !batch.indices) return 0;
#if defined(USE_DIRECTX9)
                D3DVERTEXBUFFER_DESC vb = {}; D3DINDEXBUFFER_DESC ib = {};
                if (FAILED(batch.vertices->GetDesc(&vb)) || FAILED(batch.indices->GetDesc(&ib))) return 0;
                bytes += vb.Size + ib.Size;
#elif defined(USE_DIRECTX11)
                D3D11_BUFFER_DESC vb = {}, ib = {};
                batch.vertices->GetDesc(&vb); batch.indices->GetDesc(&ib);
                bytes += vb.ByteWidth + ib.ByteWidth;
#else
                if (!batch.tangents) return 0;
                bytes += batch.vertices.length + batch.tangents.length + batch.indices.length;
#endif
            }
        return bytes;
    }
#endif
}

int runNormalMapNativeResourceTests()
{
    using namespace mbm;
    int failures = 0;
    const auto check = [&failures](bool ok, const char *message)
    { if (!ok) { ++failures; std::printf("NORMAL MAP LAZY FAIL: %s\n", message); } return ok; };
    auto *device = DEVICE::getInstance();
#if defined(USE_METAL)
    METAL_CAPTURE capture;
    if (!capture.begin(device->getSpecificContextDevice()->mtlDevice)) return -1;
#endif
#if defined(USE_DIRECTX9)
    // This resource suite targets SM3; SM2 fallback needs separate expectations.
    D3DCAPS9 caps = {};
    auto *context = device->getSpecificContextDevice();
    if (!check(SUCCEEDED(context->pd3dDevice->GetDeviceCaps(&caps)) &&
               caps.VertexShaderVersion >= D3DVS_VERSION(3,0) &&
               caps.PixelShaderVersion >= D3DPS_VERSION(3,0) &&
               std::strcmp(getVSVersion(), "vs_3_0") == 0 &&
               std::strcmp(getPSVersion(), "ps_3_0") == 0, "DX9 SM3 capability and selected profiles"))
        return -1;
    std::printf("NORMAL MAP DX9 PROFILES PASS vs_3_0 ps_3_0\n");
#endif
    const char *dir = std::getenv("MBM_NORMAL_MAP_FIXTURE_DIR");
    if (!check(dir != nullptr, "fixture directory configured")) return -1;
    auto *manager = MESH_MANAGER::getInstance();
    const auto *mesh = manager->load((std::string(dir)+"/author-import.msh").c_str());
    if (!check(mesh != nullptr, "load retained basis without map")) return -1;
    auto *buffer = mesh->getBuffer(0)->pBufferGL;
    SHADER shader;
    shader.setUseReservedLightDefault(true);
    if (!check(shader.compileShader(nullptr,nullptr,buffer->fvf), "compile geometric shader")) return -1;
    MatrixIdentity(&SHADER::modelView);
    MatrixIdentity(&SHADER::mvMatrixLightSpace);
    MatrixIdentity(&SHADER::mvpMatrix);
    device->setLightTargetForRender(LIGHT_TARGET_3D);
    setLightEnabled(LIGHT_TARGET_3D, true);
    SUBMISSION submission;
    if (!check(submission.begin(), "begin native submission")) return -1;
    const auto noUpload = [&]()
    {
#if USE_NORMAL_MAPPING_3D
        return buffer->getBackendBuffer()->normalMapSubsets.empty();
#else
        return true; // The derived resource storage is compiled out.
#endif
    };
    check(noUpload(), "load does not upload derived buffers");
    check(shader.render(buffer), "draw without map");
    const auto geometric = currentProgram();
    check(geometric != 0, "geometric vertex shader bound");
    check(noUpload(), "no-map draw does not upload");
    check(mesh->setMaterialTexture(0,0,TEXTURE_ROLE_NORMAL,"#FF8080FF",false), "late map assignment");
    check(mesh->setNormalMapSettings(0,0,1,0), "zero strength");
    check(shader.render(buffer) && noUpload(), "zero strength does not upload");
    check(currentProgram() == geometric, "zero strength keeps geometric shader");
    check(mesh->setNormalMapSettings(0,0,1,1), "enable strength");
    device->setLightTargetForRender(LIGHT_TARGET_2DW);
    check(shader.render(buffer) && noUpload(), "2dw does not upload 3d basis");
    device->setLightTargetForRender(LIGHT_TARGET_3D);
    check(shader.render(buffer), "first effective mapped draw");
    const auto mapped = currentProgram();
    check(mapped != 0 && ((mapped != geometric) == (USE_NORMAL_MAPPING_3D != 0)), "variant follows build capability");
#if USE_NORMAL_MAPPING_3D
    const auto vertices = vertexIdentity(buffer);
    const auto bytes = payloadBytes(buffer);
    if (!check(vertices != 0 && bytes > 0, "first use creates native buffers")) return -1;
    std::printf("NORMAL MAP LAZY METRICS derived GPU bytes before=0 after=%zu\n", bytes);
#endif
    check(mesh->setMaterialTexture(0,0,TEXTURE_ROLE_NORMAL,nullptr,false), "remove map");
    check(shader.render(buffer), "draw after removal");
    check(currentProgram() == geometric, "removal selects geometric shader");
    check(mesh->setMaterialTexture(0,0,TEXTURE_ROLE_NORMAL,"#FF8080FF",false), "reassign map");
    for (int i=0; i<20; ++i)
    {
        check(shader.render(buffer), "repeat mapped draw");
        check(currentProgram() == mapped, "reuse mapped shader");
#if USE_NORMAL_MAPPING_3D
        check(vertexIdentity(buffer) == vertices && payloadBytes(buffer) == bytes, "reuse native buffers");
#endif
    }
    SHADER cached;
    cached.setUseReservedLightDefault(true);
    check(cached.compileShader(nullptr,nullptr,buffer->fvf) && cached.render(buffer), "second shader instance");
#if defined(USE_DIRECTX9) || defined(USE_METAL)
    // DX11 has no shared default-program cache; do not demand COM identity there.
    check(currentProgram() == mapped, "mapped shader cache identity");
#endif
    shader.onRestore();
    check(shader.compileShader(nullptr,nullptr,buffer->fvf) && shader.render(buffer), "restore shader ownership");
    const auto *unprepared = manager->load((std::string(dir)+"/no-map.msh").c_str());
    if (!check(unprepared != nullptr, "load without basis")) return -1;
    check(unprepared->setMaterialTexture(0,0,TEXTURE_ROLE_NORMAL,"#FF8080FF",false), "map without basis");
    check(shader.render(unprepared->getBuffer(0)->pBufferGL), "unprepared geometric fallback");
    const auto restoredGeometric = currentProgram();
    check(restoredGeometric != 0, "restored geometric shader bound");
    check(shader.render(buffer) && ((currentProgram() != restoredGeometric) == (USE_NORMAL_MAPPING_3D != 0)),
          "unprepared and prepared assets select different variants only when enabled");
    const VEC3 positions[3] = {{0,0,0},{1,0,0},{0,1,0}};
    const VEC3 normals[3] = {{0,0,1},{0,0,1},{0,0,1}};
    const VEC2 uv[3] = {{0,0},{1,0},{0,1}};
    const int starts[1] = {0}, counts[1] = {3};
    if (!check(makeSourceDynamic(buffer), "prepare writable source storage")) return -1;
    check(buffer->updateDynamic(positions,normals,uv,starts,counts), "dynamic edit");
    check(noUpload(), "dynamic edit discards derived buffers");
    check(shader.render(buffer) && noUpload(), "dynamic edit cannot upload stale basis");
    check(currentProgram() == restoredGeometric, "dynamic edit selects geometric fallback");
    check(submission.finish(), "complete native submission");
#if defined(USE_METAL)
    if (failures == 0) std::printf("NORMAL MAP METAL PIPELINES PASS: geometric/mapped selection, reuse, shared cache, restore, invalidation\n");
#endif
    std::printf("NORMAL MAP LAZY RESOURCES %s (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? -1 : 0;
}

#if defined(USE_DIRECTX9)
namespace
{
    struct SM2_PROFILES
    {
        std::string ps = getPSVersion(), vs = getVSVersion();
        SM2_PROFILES()
        {
            SHADER::clearDefaultProgramCache();
            setPSVersion("ps_2_0");
            setVSVersion("vs_2_0");
        }
        ~SM2_PROFILES()
        {
            SHADER::clearDefaultProgramCache();
            setPSVersion(ps.c_str());
            setVSVersion(vs.c_str());
        }
    };

    template<class T> bool hasSm2Bytecode(T *shader, const DWORD version)
    {
        UINT size = 0;
        if (!shader || FAILED(shader->GetFunction(nullptr, &size)) || size < sizeof(DWORD)) return false;
        std::vector<DWORD> code((size + sizeof(DWORD) - 1) / sizeof(DWORD));
        return SUCCEEDED(shader->GetFunction(code.data(), &size)) && code[0] == version;
    }

    struct SM2_TARGET
    {
        IDirect3DDevice9 *device;
        IDirect3DSurface9 *oldColor = nullptr, *oldDepth = nullptr, *color = nullptr, *readback = nullptr;
        D3DVIEWPORT9 oldViewport = {};
        explicit SM2_TARGET(IDirect3DDevice9 *value) : device(value) {}
        bool begin()
        {
            if (FAILED(device->GetRenderTarget(0, &oldColor)) || FAILED(device->GetViewport(&oldViewport))) return false;
            device->GetDepthStencilSurface(&oldDepth);
            return SUCCEEDED(device->CreateRenderTarget(64,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&color,nullptr)) &&
                SUCCEEDED(device->CreateOffscreenPlainSurface(64,64,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&readback,nullptr)) &&
                SUCCEEDED(device->SetDepthStencilSurface(nullptr)) && SUCCEEDED(device->SetRenderTarget(0,color));
        }
        bool draw(SHADER &shader, BUFFER_GL *buffer, std::vector<DWORD> &pixels)
        {
            D3DVIEWPORT9 viewport = {0,0,64,64,0,1};
            if (FAILED(device->SetViewport(&viewport)) ||
                FAILED(device->SetRenderState(D3DRS_ZENABLE,FALSE)) ||
                FAILED(device->Clear(0,nullptr,D3DCLEAR_TARGET,0xff000000,1,0)) ||
                FAILED(device->BeginScene())) return false;
            const bool rendered = shader.render(buffer);
            const HRESULT ended = device->EndScene();
            if (!rendered || FAILED(ended) || FAILED(device->GetRenderTargetData(color,readback))) return false;
            D3DLOCKED_RECT lock = {};
            if (FAILED(readback->LockRect(&lock,nullptr,D3DLOCK_READONLY))) return false;
            pixels.resize(64 * 64);
            for (size_t y = 0; y < 64; ++y)
                std::memcpy(pixels.data()+y*64, static_cast<const char *>(lock.pBits)+y*lock.Pitch,64*sizeof(DWORD));
            readback->UnlockRect();
            return true;
        }
        ~SM2_TARGET()
        {
            if (oldColor) { device->SetRenderTarget(0,oldColor); oldColor->Release(); }
            device->SetDepthStencilSurface(oldDepth);
            if (oldDepth) oldDepth->Release();
            device->SetViewport(&oldViewport);
            if (readback) readback->Release();
            if (color) color->Release();
        }
    };
}

int runNormalMapSm2Tests()
{
    using namespace mbm;
    int failures = 0;
    const auto check = [&failures](bool ok, const char *message)
    { if (!ok) { ++failures; std::printf("NORMAL MAP SM2 FAIL: %s\n",message); } return ok; };
    SM2_PROFILES profiles;
    const char *dir = std::getenv("MBM_NORMAL_MAP_FIXTURE_DIR");
    if (!check(dir != nullptr,"fixture directory configured")) return -1;
    const auto *mesh = MESH_MANAGER::getInstance()->load((std::string(dir)+"/author-import.msh").c_str());
    if (!check(mesh != nullptr,"load retained basis")) return -1;
    auto *buffer = mesh->getBuffer(0)->pBufferGL;
    buffer->mode_cull_face = util::CULL_MODE::CULL_FRONT_AND_BACK;
    auto *device = DEVICE::getInstance();
    auto *native = device->getSpecificContextDevice()->pd3dDevice;
    const auto noMapping = [&check,buffer](SHADER &shader)
    {
#if USE_NORMAL_MAPPING_3D
        const auto *backend = static_cast<const D3D_PS_VS *>(shader.getBackendShaderSpecific());
        return check(buffer->getBackendBuffer()->normalMapSubsets.empty() &&
                     !backend->normalMapDeclaration && !backend->zeroTangentBuffer && !backend->normalMapSettings,
                     "no derived buffers or mapping shader interface");
#else
        (void)shader;
        return true;
#endif
    };
    SHADER lit;
    lit.setUseReservedLightDefault(true);
    // Record the actual profile limit; never substitute cached SM3 bytecode.
    const bool litCompiled = lit.compileShader(nullptr,nullptr,buffer->fvf);
    if (!check(!litCompiled,"default geometric lighting exceeds strict SM2 budget")) return -1;
    noMapping(lit);
    std::printf("NORMAL MAP SM2 LIGHTING LIMIT: default lighting unavailable\n");
    SHADER unlit;
    if (!check(unlit.compileShader(nullptr,nullptr,buffer->fvf),"compile unlit SM2")) return -1;
    const auto *backend = static_cast<const D3D_PS_VS *>(unlit.getBackendShaderSpecific());
    if (check(hasSm2Bytecode(backend->pd3dPixelShader,D3DPS_VERSION(2,0)) &&
              hasSm2Bytecode(backend->pd3dVertexShader,D3DVS_VERSION(2,0)),"actual SM2 bytecode"))
        std::printf("NORMAL MAP SM2 BYTECODE PASS vs_2_0 ps_2_0\n");
    noMapping(unlit);
    MatrixIdentity(&SHADER::modelView);
    MatrixIdentity(&SHADER::mvMatrixLightSpace);
    MatrixIdentity(&SHADER::mvpMatrix);
    device->setLightTargetForRender(LIGHT_TARGET_3D);
    SM2_TARGET target(native);
    if (!check(target.begin(),"offscreen target")) return -1;
    std::vector<DWORD> baseline, mapped;
    check(target.draw(unlit,buffer,baseline),"baseline draw/readback");
    size_t visible = 0;
    for (const DWORD pixel : baseline) if ((pixel & 0x00ffffffu) != 0) ++visible;
    check(visible > 0,"visible baseline geometry");
    check(mesh->setMaterialTexture(0,0,TEXTURE_ROLE_NORMAL,"#FF8080FF",false),"assign normal map");
    check(mesh->setNormalMapSettings(0,0,1,1),"nonzero normal strength");
    for (int i = 0; i < 3; ++i)
    {
        check(target.draw(unlit,buffer,mapped),"mapped unlit draw/readback");
        check(mapped == baseline,"normal map cannot alter unlit pixels");
        noMapping(unlit);
    }
    check(mesh->setNormalMapSettings(0,0,1,0),"zero strength");
    check(target.draw(unlit,buffer,mapped) && mapped == baseline,"zero strength pixels");
    check(mesh->setMaterialTexture(0,0,TEXTURE_ROLE_NORMAL,nullptr,false),"remove map");
    check(target.draw(unlit,buffer,mapped) && mapped == baseline,"removed map pixels");
    noMapping(unlit);
    std::printf("NORMAL MAP SM2 %s (lighting unavailable; unlit bytecode/pixels/resources verified)\n",failures ? "FAIL" : "PASS");
    return failures ? -1 : 0;
}
#endif
#endif
