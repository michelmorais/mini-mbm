/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                              |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------*/
#if defined(USE_METAL)
#include "metal-skeletal-parity-tests.h"
#include "skeletal-parity-tests.h"
#include <skeletal-metal-shader-source.h>
#include <facial-metal-shader-source.h>
#include <facial-gpu-upload.h>
#include <specific-metal-buffer.h>
#include <core_mbm/shader.h>
#include <cmath>
#include <chrono>
#include <algorithm>
#include <specific-metal-context.h>
#include <core_mbm/device.h>
#include <core_mbm/util-interface.h>
#import <Metal/Metal.h>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    using namespace mbm;
    using namespace mbm::skeletal;
    using namespace mbm::skeletal::test;

    struct METAL_PARITY_VERTEX
    {
        float position[3];
        float normal[3];
        float boneIndices[4];
        float boneWeights[4];
    };

    bool facialParity(const uint32_t vertexCount, const uint32_t targetCount, const uint32_t width)
    {
        auto *context = DEVICE::getInstance()->getSpecificContextDevice();
        if (!context || !context->mtlDevice) return false;
        facial::ASSET asset;
        asset.vertexCount = vertexCount;
        std::vector<VEC3> base, normals(vertexCount, VEC3(0, 0, 1));
        for (uint32_t v = 0; v < vertexCount; ++v) base.emplace_back(float(v % 5), float(v % 5) * .5f, -1.0f);
        for (uint32_t t = 0; t < targetCount; ++t)
        {
            facial::TARGET target;
            for (uint32_t v = 0; v < vertexCount; ++v)
            {
                target.positions.emplace_back(.1f * float(v % 5 + 1), .2f * float(t % 2 + 1), -.3f);
                target.normals.emplace_back(.1f * float(t % 2 + 1), -.1f * float(v % 5), .05f);
            }
            asset.targets.push_back(target);
        }
        // Force row tiling and a partial final row, independently of desktop texture limits.
        auto capability = facial::calculateGpuMorphCapability(16, 4096, true);
        facial::GPU_MORPH_LAYOUT layout;
        std::string error;
        if (!facial::buildGpuMorphLayout(asset, capability, true, layout, error)) return false;
        layout.textureWidth = width; layout.rowsPerVertexBlock = (vertexCount + width - 1) / width;
        layout.rowsPerTarget = layout.rowsPerVertexBlock * 2; layout.textureHeight = layout.rowsPerTarget * targetCount;
        BUFFER_GL buffer;
        buffer.sizeOfArrayVertex = vertexCount;
        if (!facial::uploadGpuMorphStream(&buffer, asset, layout)) return false;
        std::string source = "#include <metal_stdlib>\nusing namespace metal;\n"
            "struct VIn{float3 pos;float3 nor;float4 boneIndices;float4 boneWeights;};\n";
        appendMetalSkeletalFunctions(source, SKELETAL_SHADER_METHOD::LBS);
        source += "kernel void facial_test(uint facialVertexId [[thread_position_in_grid]],"
            "device const float4* base [[buffer(0)]],device float4* result [[buffer(1)]],"
            "device const float4* bonePalette [[buffer(19)]],constant float* facialWeights [[buffer(22)]],"
            "constant uint& facialVertexOffset [[buffer(23)]],texture2d<float,access::read> facialDeltas [[texture(6)]]){"
            "uint id=facialVertexId+facialVertexOffset;VIn in;in.pos=base[id*2].xyz;in.nor=base[id*2+1].xyz;"
            "in.boneIndices=float4(0);in.boneWeights=float4(1,0,0,0);\n";
        facial::appendMetalFacialDeformation(source, targetCount, width, layout.rowsPerTarget, true);
        appendMetalSkeletalDeformation(source, SKELETAL_SHADER_METHOD::LBS, true);
        source += "result[facialVertexId*2]=skinnedPosition;result[facialVertexId*2+1]=float4(skinnedNormal,0);}";
        NSError *compileError = nil;
        id<MTLLibrary> library = [context->mtlDevice newLibraryWithSource:
            [NSString stringWithUTF8String:source.c_str()] options:nil error:&compileError];
        if (!library) { ERROR_LOG("facial parity MSL: %s", compileError.localizedDescription.UTF8String); return false; }
        id<MTLComputePipelineState> pipeline = [context->mtlDevice newComputePipelineStateWithFunction:
            [library newFunctionWithName:@"facial_test"] error:&compileError];
        if (!pipeline) return false;
        std::vector<float> input(vertexCount * 8, 0.0f);
        for (size_t v = 0; v < vertexCount; ++v)
        {
            input[v*8] = base[v].x; input[v*8+1] = base[v].y; input[v*8+2] = base[v].z;
            input[v*8+6] = 1;
        }
        // Rotation plus translation detects applying deltas after skeletal deformation.
        const float palette[12] = {0,-1,0,2, 1,0,0,3, 0,0,1,4};
        id<MTLBuffer> inputs = [context->mtlDevice newBufferWithBytes:input.data() length:input.size()*sizeof(float) options:MTLResourceStorageModeShared];
        id<MTLBuffer> outputs = [context->mtlDevice newBufferWithLength:input.size()*sizeof(float) options:MTLResourceStorageModeShared];
        id<MTLBuffer> bones = [context->mtlDevice newBufferWithBytes:palette length:sizeof(palette) options:MTLResourceStorageModeShared];
        if (!inputs || !outputs || !bones) return false;
        float maximumError = 0;
        double cpuMilliseconds = 0, gpuMilliseconds = 0;
        for (const float weight : {0.0f, .25f, 1.0f})
        {
            std::vector<float> weights(targetCount, weight);
            if (weight == .25f) weights.back() = .75f;
            std::vector<VEC3> expectedPositions, expectedNormals;
            const auto cpuStart = std::chrono::steady_clock::now();
            if (!facial::deform(asset, base, normals, weights, expectedPositions, expectedNormals, error)) return false;
            cpuMilliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-cpuStart).count();
            for (const uint32_t offset : {0u, 2u})
            {
                const auto gpuStart = std::chrono::steady_clock::now();
                id<MTLCommandBuffer> command = [context->commandQueue commandBuffer];
                id<MTLComputeCommandEncoder> encoder = [command computeCommandEncoder];
                [encoder setComputePipelineState:pipeline];
                [encoder setBuffer:inputs offset:0 atIndex:0];
                [encoder setBuffer:outputs offset:0 atIndex:1];
                [encoder setBuffer:bones offset:0 atIndex:19];
                [encoder setBytes:weights.data() length:weights.size()*sizeof(float) atIndex:22];
                [encoder setBytes:&offset length:sizeof(offset) atIndex:23];
                [encoder setTexture:buffer.getBackendBuffer()->facialMorphTexture atIndex:6];
                [encoder dispatchThreads:MTLSizeMake(vertexCount-offset,1,1) threadsPerThreadgroup:MTLSizeMake(32,1,1)];
                [encoder endEncoding]; [command commit]; [command waitUntilCompleted];
                gpuMilliseconds += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-gpuStart).count();
                if (command.status == MTLCommandBufferStatusError) return false;
                const float *actual = static_cast<const float *>(outputs.contents);
                for (size_t v = offset; v < vertexCount; ++v)
                {
                    const auto &p = expectedPositions[v]; const auto &n = expectedNormals[v];
                    const float expected[8] = {-p.y+2,p.x+3,p.z+4,1,-n.y,n.x,n.z,0};
                    for (size_t c = 0; c < 8; ++c)
                    {
                        const float difference = std::fabs(actual[(v-offset)*8+c]-expected[c]);
                        if (!std::isfinite(difference) || difference > 1e-4f) return false;
                        maximumError = std::max(maximumError, difference);
                    }
                }
            }
        }
        INFO_LOG("METAL FACIAL PARITY PASS vertices=%u targets=%u max_error=%g cpu_mean_ms=%.3f gpu_submit_wait_mean_ms=%.3f (zero/mixed/full weights, tiled deltas, normals, LBS, subset offset)", vertexCount, targetCount, maximumError, cpuMilliseconds/3.0, gpuMilliseconds/6.0);
        return true;
    }

    bool captureRgba8(const SKELETAL_PARITY_CASE &testCase,
                      const SKELETAL_PARITY_ENCODING &encoding,
                      std::vector<uint8_t> &positionPixels,
                      std::vector<uint8_t> &normalPixels,
                      std::string &error)
    {
        DEVICE *device = DEVICE::getInstance();
        SPECIFIC_AUX_CONTEXT_DEVICE *context = device ? device->getSpecificContextDevice() : nullptr;
        if (!context || !context->mtlDevice || !context->commandQueue)
        {
            error = "Metal skeletal parity requires an initialized native Metal device";
            return false;
        }
        std::string source = "#include <metal_stdlib>\nusing namespace metal;\n"
            "struct VIn{packed_float3 pos;packed_float3 nor;packed_float4 boneIndices;packed_float4 boneWeights;};\n"
            "struct VOut{float4 pos [[position]];float3 value;};\n"
            "struct Params{packed_float3 encodeCenter;float encodeExtent;uint sampleCount;uint outputNormal;};\n";
        appendMetalSkeletalFunctions(source, testCase.method);
        source += "vertex VOut vert_main(device const VIn* vertices [[buffer(0)]],device const float4* bonePalette [[buffer(19)]],constant Params& params [[buffer(1)]],uint vertexId [[vertex_id]]){uint sample=vertexId/6;uint corner=vertexId%6;VIn in=vertices[sample];";
        appendMetalSkeletalDeformation(source, testCase.method, true);
        source += "VOut out;float x0=(float(sample)/float(params.sampleCount))*2.0f-1.0f;float x1=(float(sample+1)/float(params.sampleCount))*2.0f-1.0f;float2 corners[6]={float2(x0,-1.0f),float2(x1,-1.0f),float2(x0,1.0f),float2(x0,1.0f),float2(x1,-1.0f),float2(x1,1.0f)};out.pos=float4(corners[corner],0.0f,1.0f);out.value=params.outputNormal!=0?skinnedNormal*0.5f+0.5f:(skinnedPosition.xyz-float3(params.encodeCenter))/(2.0f*params.encodeExtent)+0.5f;return out;}\n"
                  "fragment float4 frag_main(VOut in [[stage_in]]){return float4(clamp(in.value,0.0f,1.0f),1.0f);}\n";

        NSError *compileError = nil;
        id<MTLLibrary> library = [context->mtlDevice
            newLibraryWithSource:[NSString stringWithUTF8String:source.c_str()]
            options:nil error:&compileError];
        if (!library)
        {
            error = std::string("Metal skeletal parity shader compile failed: ") +
                [[compileError localizedDescription] UTF8String];
            return false;
        }
        MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipelineDescriptor.vertexFunction = [library newFunctionWithName:@"vert_main"];
        pipelineDescriptor.fragmentFunction = [library newFunctionWithName:@"frag_main"];
        pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
        NSError *pipelineError = nil;
        id<MTLRenderPipelineState> pipeline = [context->mtlDevice
            newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&pipelineError];
        if (!pipeline)
        {
            error = std::string("Metal skeletal parity pipeline creation failed: ") +
                [[pipelineError localizedDescription] UTF8String];
            return false;
        }

        const size_t sampleCount = testCase.positions.size();
        std::vector<METAL_PARITY_VERTEX> vertices(sampleCount);
        for (size_t index = 0; index < sampleCount; ++index)
        {
            std::memcpy(vertices[index].position, &testCase.positions[index].x, sizeof(float) * 3);
            std::memcpy(vertices[index].normal, &testCase.normals[index].x, sizeof(float) * 3);
            for (size_t influence = 0; influence < 4; ++influence)
            {
                vertices[index].boneIndices[influence] =
                    static_cast<float>(testCase.influences[index].boneIndex[influence]);
                vertices[index].boneWeights[influence] = testCase.influences[index].weight[influence];
            }
        }
        id<MTLBuffer> vertexBuffer = [context->mtlDevice newBufferWithBytes:vertices.data()
            length:vertices.size() * sizeof(METAL_PARITY_VERTEX) options:MTLResourceStorageModeShared];
        id<MTLBuffer> paletteBuffer = [context->mtlDevice newBufferWithBytes:testCase.palette.data()
            length:testCase.palette.size() * sizeof(float) options:MTLResourceStorageModeShared];
        if (!vertexBuffer || !paletteBuffer)
        {
            error = "Metal skeletal parity could not allocate input buffers";
            return false;
        }
        MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor
            texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
            width:sampleCount height:1 mipmapped:NO];
        textureDescriptor.usage = MTLTextureUsageRenderTarget;
        id<MTLTexture> target = [context->mtlDevice newTextureWithDescriptor:textureDescriptor];
        if (!target)
        {
            error = "Metal skeletal parity could not allocate the RGBA8 render target";
            return false;
        }
        struct PARAMS
        {
            float encodeCenter[3];
            float encodeExtent;
            uint32_t sampleCount;
            uint32_t outputNormal;
        } params = {{encoding.positionCenter.x, encoding.positionCenter.y,
                     encoding.positionCenter.z}, encoding.positionExtent,
                    static_cast<uint32_t>(sampleCount), 0};
        positionPixels.resize(sampleCount * 4);
        normalPixels.resize(sampleCount * 4);
        for (uint32_t normalPass = 0; normalPass < 2; ++normalPass)
        {
            params.outputNormal = normalPass;
            MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
            pass.colorAttachments[0].texture = target;
            pass.colorAttachments[0].loadAction = MTLLoadActionClear;
            pass.colorAttachments[0].storeAction = MTLStoreActionStore;
            pass.colorAttachments[0].clearColor = MTLClearColorMake(0, 0, 0, 0);
            id<MTLCommandBuffer> commandBuffer = [context->commandQueue commandBuffer];
            id<MTLRenderCommandEncoder> encoder = [commandBuffer renderCommandEncoderWithDescriptor:pass];
            [encoder setRenderPipelineState:pipeline];
            [encoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
            [encoder setVertexBytes:&params length:sizeof(params) atIndex:1];
            [encoder setVertexBuffer:paletteBuffer offset:0 atIndex:19];
            [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:sampleCount * 6];
            [encoder endEncoding];
            [commandBuffer commit];
            [commandBuffer waitUntilCompleted];
            if (commandBuffer.status == MTLCommandBufferStatusError)
            {
                error = std::string("Metal skeletal parity command failed: ") +
                    [[commandBuffer.error localizedDescription] UTF8String];
                return false;
            }
            std::vector<uint8_t> &pixels = normalPass ? normalPixels : positionPixels;
            [target getBytes:pixels.data() bytesPerRow:sampleCount * 4
                  fromRegion:MTLRegionMake2D(0, 0, sampleCount, 1) mipmapLevel:0];
        }
        return true;
    }
}

bool runMetalSkeletalParityTests()
{
    std::string error;
    if (facialParity(5, 2, 2) && facialParity(34164, 6, 4096) && facialParity(34164, 52, 4096) && mbm::skeletal::test::runSkeletalParitySuite("Metal", captureRgba8, error))
        return true;
    ERROR_LOG("testLib: %s", error.c_str());
    return false;
}
#endif
