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


#if defined(USE_METAL)
#include <core_mbm/mesh-manager.h>
#include <core_mbm/light.h>
#include <core_mbm/device.h>
#include <core_mbm/camera.h>
#include <render/mesh.h>
#include "specific-metal-buffer.h"
#include <mach/mach.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace
{
    using namespace mbm;
    using CLOCK = std::chrono::steady_clock;
    double micros(CLOCK::time_point start)
    {
        return std::chrono::duration<double, std::micro>(CLOCK::now() - start).count();
    }
    int parameter(const char *name, int fallback, int low, int high)
    {
        const char *value = std::getenv(name);
        if (!value) return fallback;
        char *end = nullptr;
        const long parsed = std::strtol(value, &end, 10);
        return end != value && *end == 0 && parsed >= low && parsed <= high ? static_cast<int>(parsed) : -1;
    }
    size_t rss()
    {
        mach_task_basic_info_data_t info = {};
        mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
        return task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count)
            == KERN_SUCCESS ? static_cast<size_t>(info.resident_size) : 0;
    }
    size_t gpuBytes(const BUFFER_GL *buffer, bool derived)
    {
        const auto *backend = buffer->getBackendBuffer();
        std::set<uintptr_t> seen;
        size_t bytes = 0;
        const auto add = [&](id<MTLBuffer> resource)
        {
            if (resource && seen.insert(reinterpret_cast<uintptr_t>((__bridge void *)resource)).second)
                bytes += resource.length;
        };
        if (!derived)
        {
            add(backend->vertexBuffer); add(backend->indexBuffer); add(backend->skinVertexBuffer);
        }
#if USE_NORMAL_MAPPING_3D
        else
            for (const auto &subset : backend->normalMapSubsets)
                for (const auto &batch : subset.batches)
                {
                    add(batch.vertices); add(batch.tangents); add(batch.indices);
                }
#endif
        return bytes;
    }

    struct TIMING
    {
        double submit = 0, synchronized = 0, gpu = 0;
    };
    // One offscreen pass per timed block, no presentation/vsync or readback in
    // the timed interval. GPU duration includes pass clear/store, not just draws.
    struct TARGET
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context;
        id<MTLTexture> color = nil, depth = nil;
        bool oldDepth;
        int oldBlend;
        explicit TARGET(SPECIFIC_AUX_CONTEXT_DEVICE *ctx) noexcept
            : context(ctx), oldDepth(ctx->depthTestEnabled), oldBlend(ctx->currentBlendState) {}
        ~TARGET()
        {
            context->depthTestEnabled = oldDepth;
            context->currentBlendState = oldBlend;
        }
        bool init()
        {
            if (context->currentEncoder) return false;
            auto *desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:context->metalLayer.pixelFormat
                width:128 height:128 mipmapped:NO];
            desc.storageMode = MTLStorageModeShared;
            desc.usage = MTLTextureUsageRenderTarget;
            color = [context->mtlDevice newTextureWithDescriptor:desc];
            desc.pixelFormat = MTLPixelFormatDepth32Float;
            desc.storageMode = MTLStorageModePrivate;
            depth = [context->mtlDevice newTextureWithDescriptor:desc];
            context->depthTestEnabled = false;
            context->currentBlendState = 1; // standard alpha; the fixture is opaque
            return color && depth;
        }
        bool draw(SHADER &shader, const BUFFER_GL *buffer, const RENDERIZABLE &owner, int count, TIMING &timing)
        {
            @autoreleasepool
            {
                const auto start = CLOCK::now();
                auto *pass = [MTLRenderPassDescriptor renderPassDescriptor];
                pass.colorAttachments[0].texture = color;
                pass.colorAttachments[0].loadAction = MTLLoadActionClear;
                pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 1);
                pass.colorAttachments[0].storeAction = MTLStoreActionStore;
                pass.depthAttachment.texture = depth;
                pass.depthAttachment.loadAction = MTLLoadActionClear;
                pass.depthAttachment.clearDepth = 1;
                pass.depthAttachment.storeAction = MTLStoreActionDontCare;
                id<MTLCommandBuffer> commands = [context->commandQueue commandBuffer];
                id<MTLRenderCommandEncoder> encoder = [commands renderCommandEncoderWithDescriptor:pass];
                if (!encoder) return false;
                context->currentEncoder = encoder;
                [encoder setViewport:MTLViewport{0, 0, 128, 128, 0, 1}];
                bool rendered = true;
                for (int i = 0; i < count && rendered; ++i) rendered = shader.render(buffer, &owner);
                [encoder endEncoding];
                context->currentEncoder = nil;
                [commands commit];
                timing.submit = micros(start) / count;
                [commands waitUntilCompleted];
                timing.synchronized = micros(start) / count;
                if (!rendered || commands.status != MTLCommandBufferStatusCompleted)
                {
                    std::printf("NORMAL MAP BENCH FAIL: Metal submission: %s\n", commands.error.localizedDescription.UTF8String);
                    return false;
                }
                const double begin = commands.GPUStartTime, end = commands.GPUEndTime;
                if (!std::isfinite(begin) || !std::isfinite(end) || begin <= 0 || end <= begin) return false;
                timing.gpu = (end - begin) * 1e6 / count;
                return true;
            }
        }
        bool visible()
        {
            unsigned char pixel[4] = {};
            [color getBytes:pixel bytesPerRow:4 fromRegion:MTLRegionMake2D(64, 64, 1, 1) mipmapLevel:0];
            return pixel[0] || pixel[1] || pixel[2];
        }
    };
    void printArray(const char *name, const std::vector<double> &values)
    {
        std::printf(",\"%s\":[", name);
        for (size_t i = 0; i < values.size(); ++i) std::printf("%s%.3f", i ? "," : "", values[i]);
        std::printf("]");
    }
}

int runMetalNormalMapBenchmark(const mbm::SCENE &scene)
{
#if defined(_DEBUG)
    std::printf("NORMAL MAP BENCH CONFIG release=0\n");
#else
    std::printf("NORMAL MAP BENCH CONFIG release=1\n");
#endif
    const char *directory = std::getenv("MBM_NORMAL_BENCH_DIR"), *caseName = std::getenv("MBM_NORMAL_BENCH_CASE");
    const int draws = parameter("MBM_NORMAL_BENCH_DRAWS", 32, 1, 4096);
    const int blocks = parameter("MBM_NORMAL_BENCH_BLOCKS", 8, 2, 100);
    const int pointLights = parameter("MBM_NORMAL_BENCH_POINT_LIGHTS", 0, 0, SUPPORTED_MAX_LIGHTS);
    if (!directory || !caseName || draws < 0 || blocks < 0 || pointLights < 0) return -1;
    const std::string mode(caseName);
    if (mode != "plain" && mode != "retained" && mode != "zero" && mode != "mapped" && mode != "mixed" && mode != "removed") return -1;
    const auto fail = [](const char *message) { std::printf("NORMAL MAP BENCH FAIL: %s\n", message); return -1; };
    auto *device = DEVICE::getInstance();
    auto *context = device->getSpecificContextDevice();
    TARGET target(context);
    if (!target.init()) return fail("offscreen target");
    // Supply the fixture's spatial bounds to production light selection without
    // owner.load() compiling its own shader ahead of the measured compile.
    MESH owner(&scene, true, false);
    owner.setBoundingAABB(VEC3(1.8f, 1.8f, 0));
    const size_t rssBefore = rss(), allocatedBefore = context->mtlDevice.currentAllocatedSize;
    auto start = CLOCK::now();
    const auto *mesh = MESH_MANAGER::getInstance()->load((std::filesystem::path(directory) /
        (mode == "plain" ? "plain.msh" : "prepared.msh")).string().c_str());
    const double loadUs = micros(start);
    if (!mesh) return fail("load");
    auto *buffer = mesh->getBuffer(0)->pBufferGL;
    const size_t rssLoaded = rss(), allocatedLoaded = context->mtlDevice.currentAllocatedSize;
    const size_t sourceBytes = gpuBytes(buffer, false), beforeBytes = gpuBytes(buffer, true);
    start = CLOCK::now();
    if (mode != "plain" && mode != "retained")
        for (uint32_t i = 0; i < (mode == "mixed" ? 1u : 2u); ++i)
            if (!mesh->setMaterialTexture(0, i, TEXTURE_ROLE_NORMAL, "#FF8080FF", false) ||
                !mesh->setNormalMapSettings(0, i, 1, mode == "zero" ? 0.0f : 1.0f)) return fail("material setup");
    const double materialUs = micros(start);
    CAMERA &camera = device->getCamera();
    MatrixIdentity(&camera.matrixView);
    device->setLightTargetForRender(LIGHT_TARGET_3D);
    setLightEnabled(LIGHT_TARGET_3D, true);
    setAmbientLight(LIGHT_TARGET_3D, COLOR(0.1f, 0.1f, 0.1f, 1));
    setDirectionalLight(LIGHT_TARGET_3D, VEC3(0, 0, -1), COLOR(0.7f, 0.7f, 0.7f, 1));
    clearPointLights(LIGHT_TARGET_3D);
    setRequestedMaxLights(LIGHT_TARGET_3D, SUPPORTED_MAX_LIGHTS);
    for (int i = 0; i < pointLights; ++i)
        if (!addPointLight(LIGHT_TARGET_3D, VEC3(0.2f * i, 0.3f, 2), 5, COLOR(0.1f, 0.1f, 0.1f, 1)))
            return fail("point light setup");
    LIGHT_POINT_SELECTION selected[SUPPORTED_MAX_LIGHTS];
    device->setRenderizableForCurrentRender(&owner);
    const uint32_t selectedCount = device->getSelectedPointLightsForCurrentRender(selected, SUPPORTED_MAX_LIGHTS);
    device->clearRenderizableForCurrentRender();
    if (selectedCount != static_cast<uint32_t>(pointLights)) return fail("active point light count");
    MatrixIdentity(&SHADER::modelView); MatrixIdentity(&SHADER::mvMatrixLightSpace); MatrixIdentity(&SHADER::mvpMatrix);
    buffer->mode_cull_face = 0;
    SHADER shader;
    shader.setUseReservedLightDefault(true);
    start = CLOCK::now();
    if (!shader.compileShader(nullptr, nullptr, buffer->fvf)) return fail("geometric compile");
    const double compileUs = micros(start);
    TIMING first;
    if (!target.draw(shader, buffer, owner, 1, first)) return fail("first draw or GPU timestamps");
    const size_t rssFirst = rss(), allocatedFirst = context->mtlDevice.currentAllocatedSize;
    const size_t afterBytes = gpuBytes(buffer, true);
    const bool effective = USE_NORMAL_MAPPING_3D && (mode == "mapped" || mode == "mixed" || mode == "removed");
    if (beforeBytes != 0 || (afterBytes > 0) != effective) return fail("derived allocation contract");
    if (!target.visible()) return fail("invisible geometry");
    start = CLOCK::now();
    if (mode == "removed")
        for (uint32_t i = 0; i < 2; ++i)
            if (!mesh->setMaterialTexture(0, i, TEXTURE_ROLE_NORMAL, nullptr, false)) return fail("remove map");
    const double removalUs = micros(start);
    TIMING warmup;
    if (!target.draw(shader, buffer, owner, 16, warmup)) return fail("warmup");
    std::vector<double> submit, synchronized, gpu;
    submit.reserve(blocks); synchronized.reserve(blocks); gpu.reserve(blocks);
    for (int block = 0; block < blocks; ++block)
    {
        TIMING timing;
        if (!target.draw(shader, buffer, owner, draws, timing)) return fail("warm draw or GPU timestamps");
        submit.push_back(timing.submit); synchronized.push_back(timing.synchronized); gpu.push_back(timing.gpu);
    }
    const size_t warmBytes = gpuBytes(buffer, true), rssWarm = rss(), allocatedWarm = context->mtlDevice.currentAllocatedSize;
    if (warmBytes != afterBytes || !target.visible()) return fail("warm resources or visibility");
    std::printf("NORMAL_MAP_BENCH_JSON {\"normal\":%d,\"lights\":%u,\"point_lights\":%d,\"vertices\":%u,\"case\":\"%s\","
        "\"load_sync_us\":%.3f,\"material_sync_us\":%.3f,\"geometric_compile_sync_us\":%.3f,"
        "\"first_submit_us\":%.3f,\"first_sync_us\":%.3f,\"first_gpu_us\":%.3f,\"removal_us\":%.3f,"
        "\"rss_before\":%zu,\"rss_loaded\":%zu,\"rss_first\":%zu,\"rss_warm\":%zu,"
        "\"metal_allocated_before\":%zu,\"metal_allocated_loaded\":%zu,\"metal_allocated_first\":%zu,\"metal_allocated_warm\":%zu,"
        "\"source_gpu_bytes\":%zu,\"derived_before\":%zu,\"derived_first\":%zu,\"derived_warm\":%zu",
        USE_NORMAL_MAPPING_3D, getSupportedMaxLights(LIGHT_TARGET_3D), pointLights, buffer->sizeOfArrayVertex, caseName,
        loadUs, materialUs, compileUs, first.submit, first.synchronized, first.gpu, removalUs,
        rssBefore, rssLoaded, rssFirst, rssWarm, allocatedBefore, allocatedLoaded, allocatedFirst, allocatedWarm,
        sourceBytes, beforeBytes, afterBytes, warmBytes);
    printArray("warm_submit_us", submit); printArray("warm_sync_us", synchronized); printArray("warm_gpu_us", gpu);
    std::printf("}\nNORMAL MAP BENCH PASS\n");
    return 0;
}
#endif
