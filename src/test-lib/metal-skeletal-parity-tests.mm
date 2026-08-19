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
    if (mbm::skeletal::test::runSkeletalParitySuite("Metal", captureRgba8, error))
        return true;
    ERROR_LOG("testLib: %s", error.c_str());
    return false;
}
#endif
