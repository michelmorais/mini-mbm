/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED.                                    |
|-----------------------------------------------------------------------------------------------------------------------*/

// Metal shader stubs.
// Provides the backend-specific shader, buffer, and shader-variable functions
// that are defined in shader-opengl_es.cpp and guarded by USE_OPENGL_ES.
// For Milestone 1 (empty scene) none of these are called at runtime, but the
// linker requires their symbols.

#if defined(USE_METAL)

#include <shader.h>
#include <shader-var-cfg.h>
#include <texture-manager.h>
#include "specific-metal-context.h"
#include "specific-metal-buffer.h"
#include <device.h>
#include <util-interface.h>
#include <particle-control.h>
#include <header-mesh.h>

// ---- file-scope Metal helpers -----------------------------------------------

static mbm::SPECIFIC_AUX_CONTEXT_DEVICE* getMetalCtx()
{
    mbm::DEVICE* dev = mbm::DEVICE::getInstance();
    return dev ? dev->getSpecificContextDevice() : nullptr;
}

static NSUInteger strideForFVF(mbm::FVF_PROVIDE_BY_ENGINE fvf)
{
    using F = mbm::FVF_PROVIDE_BY_ENGINE;
    switch (fvf)
    {
        case F::FVF_POS:        return 12;
        case F::FVF_POS_UV:     return 20;
        case F::FVF_POS_NOR:    return 24;
        case F::FVF_POS_NOR_UV: return 32;
        default:                return 12;
    }
}

static MTLPrimitiveType metalPrimitive(const uint32_t m)
{
    // Engine MODE_DRAW values: 0=points 1=lines 2=line_loop 3=line_strip
    //                          4=triangles 5=triangle_strip 6=triangle_fan
    switch (m)
    {
        case 0:  return MTLPrimitiveTypePoint;
        case 1:  return MTLPrimitiveTypeLine;
        case 2:  return MTLPrimitiveTypeLine;           // LINE_LOOP → Line (approx.)
        case 3:  return MTLPrimitiveTypeLineStrip;
        case 4:  return MTLPrimitiveTypeTriangle;
        case 5:  return MTLPrimitiveTypeTriangleStrip;
        default: return MTLPrimitiveTypeTriangle;
    }
}

static id<MTLSamplerState> getOrCreateSampler(mbm::SPECIFIC_AUX_CONTEXT_DEVICE* ctx)
{
    if (ctx->useNearestSampler)
    {
        if (!ctx->nearestSampler)
        {
            MTLSamplerDescriptor* sd = [MTLSamplerDescriptor new];
            sd.minFilter    = MTLSamplerMinMagFilterNearest;
            sd.magFilter    = MTLSamplerMinMagFilterNearest;
            sd.sAddressMode = MTLSamplerAddressModeRepeat;
            sd.tAddressMode = MTLSamplerAddressModeRepeat;
            ctx->nearestSampler = [ctx->mtlDevice newSamplerStateWithDescriptor:sd];
        }
        return ctx->nearestSampler;
    }
    if (!ctx->defaultSampler)
    {
        MTLSamplerDescriptor* sd = [MTLSamplerDescriptor new];
        sd.minFilter    = MTLSamplerMinMagFilterLinear;
        sd.magFilter    = MTLSamplerMinMagFilterLinear;
        sd.sAddressMode = MTLSamplerAddressModeClampToEdge;
        sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
        ctx->defaultSampler = [ctx->mtlDevice newSamplerStateWithDescriptor:sd];
    }
    return ctx->defaultSampler;
}

// Maps engine CULL_MODE values (== GL constants) to MTLCullMode.
static MTLCullMode metalCullMode(uint32_t mode_cull_face)
{
    switch (mode_cull_face)
    {
        case 0x0404: return MTLCullModeFront;  // CULL_FRONT
        case 0x0405: return MTLCullModeBack;   // CULL_BACK
        case 0x0408: return MTLCullModeBack;   // CULL_FRONT_AND_BACK — best Metal approximation
        default:     return MTLCullModeNone;   // 0 / unknown = no culling
    }
}

// Maps engine FACE_DIRECTION values (== GL constants) to MTLWinding.
static MTLWinding metalWinding(uint32_t mode_front_face_direction)
{
    // GL_CCW = 0x0901 → CCW front faces; GL_CW = 0x0900 → CW front faces.
    return (mode_front_face_direction == 0x0901)
        ? MTLWindingCounterClockwise
        : MTLWindingClockwise;
}

// Lazily creates a depth-stencil state: less-or-equal comparison, depth writes enabled.
// LessEqual (not Less) mirrors GLDepthFunc(GL_LEQUAL) used by all OpenGL ES platforms.
// This allows same-Z tiles in the same layer to overwrite each other so that transparent
// bricks do not block subsequently rendered bricks at the identical depth value.
static id<MTLDepthStencilState> getOrCreateDepthStencilState(mbm::SPECIFIC_AUX_CONTEXT_DEVICE* ctx)
{
    if (!ctx->defaultDepthStencilState)
    {
        MTLDepthStencilDescriptor* dsd = [MTLDepthStencilDescriptor new];
        dsd.depthCompareFunction = MTLCompareFunctionLessEqual;
        dsd.depthWriteEnabled    = YES;
        ctx->defaultDepthStencilState =
            [ctx->mtlDevice newDepthStencilStateWithDescriptor:dsd];
    }
    return ctx->defaultDepthStencilState;
}

// Lazily creates a depth-stencil state with depth test disabled (for particles).
// Mirrors OpenGL's glDisable(GL_DEPTH_TEST) used in renderParticle.
static id<MTLDepthStencilState> getOrCreateNoDepthState(mbm::SPECIFIC_AUX_CONTEXT_DEVICE* ctx)
{
    if (!ctx->noDepthStencilState)
    {
        MTLDepthStencilDescriptor* dsd = [MTLDepthStencilDescriptor new];
        dsd.depthCompareFunction = MTLCompareFunctionAlways;
        dsd.depthWriteEnabled    = NO;
        ctx->noDepthStencilState =
            [ctx->mtlDevice newDepthStencilStateWithDescriptor:dsd];
    }
    return ctx->noDepthStencilState;
}

// Thin ObjC wrapper that stores two PSOs for one compiled program:
//   standardPSO  — standard alpha blend (src_alpha * src + (1-src_alpha) * dst)
//   additivePSO  — additive blend       (src_alpha * src + 1 * dst)
// Both are immutable MTLRenderPipelineState objects.
// 'ptrShaderSpecific' on SHADER holds a CF-retained reference to one of these.
@interface MBMPSOPair : NSObject
@property (nonatomic, strong) id<MTLRenderPipelineState> standardPSO;
@property (nonatomic, strong) id<MTLRenderPipelineState> additivePSO;
@end
@implementation MBMPSOPair
@end

// Compile one MTLRenderPipelineState.  When additive==true the destination blend
// factor for both RGB and alpha is MTLBlendFactorOne, matching OpenGL BLEND_ONE:
//   glBlendFunc(GL_SRC_ALPHA, GL_ONE).
static id<MTLRenderPipelineState> compileSinglePSO(
    id<MTLDevice> device,
    id<MTLFunction> vertFn,
    id<MTLFunction> fragFn,
    MTLVertexDescriptor* vtxDesc,
    MTLPixelFormat colorFmt,
    bool additive)
{
    MTLRenderPipelineDescriptor* psd = [[MTLRenderPipelineDescriptor alloc] init];
    psd.label            = @"MBM";
    psd.vertexFunction   = vertFn;
    psd.fragmentFunction = fragFn;
    psd.vertexDescriptor = vtxDesc;
    psd.colorAttachments[0].pixelFormat          = colorFmt;
    psd.colorAttachments[0].blendingEnabled      = YES;
    psd.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    if (additive)
    {
        psd.colorAttachments[0].destinationRGBBlendFactor   = MTLBlendFactorOne;
        psd.colorAttachments[0].sourceAlphaBlendFactor      = MTLBlendFactorSourceAlpha;
        psd.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOne;
    }
    else
    {
        psd.colorAttachments[0].destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
        psd.colorAttachments[0].sourceAlphaBlendFactor      = MTLBlendFactorOne;
        psd.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    }
    psd.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    NSError* err = nil;
    id<MTLRenderPipelineState> pso =
        [device newRenderPipelineStateWithDescriptor:psd error:&err];
    if (!pso)
        ERROR_LOG("Metal: pipeline state error: %s", [[err localizedDescription] UTF8String]);
    return pso;
}

// Builds only the vertex preamble (header + vert_main) for a given FVF.
// Returns the MSL 'struct VIn { ... };' declaration matching the given FVF.
// Prewritten VS programs (scale.vs, simple texture.vs) hardcode attribute(1)=uv
// which is wrong for FVF_POS_NOR_UV where attribute(1) is the normal and
// attribute(2) is the UV in the interleaved vertex buffer.
static NSString* vinStructForFVF(mbm::FVF_PROVIDE_BY_ENGINE fvf)
{
    using F = mbm::FVF_PROVIDE_BY_ENGINE;
    const bool hasNor = (fvf == F::FVF_POS_NOR || fvf == F::FVF_POS_NOR_UV);
    const bool hasUV  = (fvf == F::FVF_POS_UV  || fvf == F::FVF_POS_NOR_UV);
    NSMutableString* s = [NSMutableString stringWithString:@"struct VIn { float3 pos [[attribute(0)]];"];
    if (hasNor) [s appendString:@" float3 nor [[attribute(1)]];"];
    if (hasNor && hasUV) [s appendString:@" float2 uv [[attribute(2)]];"];
    else if (hasUV)      [s appendString:@" float2 uv [[attribute(1)]];"];
    [s appendString:@" };"];
    return s;
}

// Replaces the 'struct VIn { ... };' block in 'vsStr' with the FVF-correct version.
// This fixes prewritten VS programs whose VIn has hardcoded attribute indices.
static NSString* patchVInStruct(NSString* vsStr, mbm::FVF_PROVIDE_BY_ENGINE fvf)
{
    NSRange vinBegin = [vsStr rangeOfString:@"struct VIn"];
    if (vinBegin.location == NSNotFound) return vsStr;
    NSRange searchFrom = NSMakeRange(vinBegin.location, vsStr.length - vinBegin.location);
    NSRange vinClose = [vsStr rangeOfString:@"};" options:0 range:searchFrom];
    if (vinClose.location == NSNotFound) return vsStr;
    NSUInteger endPos = vinClose.location + vinClose.length;
    NSRange vinFullRange = NSMakeRange(vinBegin.location, endPos - vinBegin.location);
    return [vsStr stringByReplacingCharactersInRange:vinFullRange withString:vinStructForFVF(fvf)];
}

// compileShader() appends the PS fragment function from the shader-resource entry.
static NSString* buildVertexHeader(mbm::FVF_PROVIDE_BY_ENGINE fvf)
{
    using F = mbm::FVF_PROVIDE_BY_ENGINE;
    const bool hasNor = (fvf == F::FVF_POS_NOR || fvf == F::FVF_POS_NOR_UV);
    const bool hasUV  = (fvf == F::FVF_POS_UV  || fvf == F::FVF_POS_NOR_UV);

    NSMutableString* src = [NSMutableString stringWithString:
        @"#include <metal_stdlib>\n"
         "using namespace metal;\n"
         "struct MbmUniforms { float4x4 mvp; float4x4 mv; float4 color; };\n"];

    [src appendString:@"struct VIn { float3 pos [[attribute(0)]];"];
    if (hasNor) [src appendString:@" float3 nor [[attribute(1)]];"];
    if (hasNor && hasUV) [src appendString:@" float2 uv [[attribute(2)]];"];
    else if (hasUV)      [src appendString:@" float2 uv [[attribute(1)]];"];
    [src appendString:@" };\n"];
    [src appendString:@"struct VOut { float4 pos [[position]]; float2 uv; };\n"];

    [src appendString:
        @"vertex VOut vert_main(VIn in [[stage_in]], constant MbmUniforms& u [[buffer(1)]]) {\n"
         "    VOut o;\n"
         "    o.pos = u.mvp * float4(in.pos, 1.0f);\n"];
    [src appendString: hasUV ? @"    o.uv = in.uv;\n" : @"    o.uv = float2(0.0f);\n"];
    [src appendString: @"    return o;\n}\n"];
    return src;
}

// Builds the MSL source for the default shader matching the given FVF.
static NSString* defaultMSLSource(mbm::FVF_PROVIDE_BY_ENGINE fvf)
{
    using F = mbm::FVF_PROVIDE_BY_ENGINE;
    const bool hasNor = (fvf == F::FVF_POS_NOR || fvf == F::FVF_POS_NOR_UV);
    const bool hasUV  = (fvf == F::FVF_POS_UV  || fvf == F::FVF_POS_NOR_UV);

    NSMutableString* src = [NSMutableString stringWithString:
        @"#include <metal_stdlib>\n"
         "using namespace metal;\n"
         "struct Uniforms { float4x4 mvpMatrix; float4x4 mvMatrix; float4 color; };\n"];

    // vertex input struct
    [src appendString:@"struct VIn { float3 pos [[attribute(0)]];"];
    if (hasNor) [src appendString:@" float3 nor [[attribute(1)]];"];
    if (hasNor && hasUV) [src appendString:@" float2 uv [[attribute(2)]];"];
    else if (hasUV)      [src appendString:@" float2 uv [[attribute(1)]];"];
    [src appendString:@" };\n"];

    // vertex output struct
    [src appendString:@"struct VOut { float4 pos [[position]];"];
    if (hasNor) [src appendString:@" float3 nor;"];
    if (hasUV)  [src appendString:@" float2 uv;"];
    [src appendString:@" };\n"];

    // vertex function
    [src appendString:
        @"vertex VOut vert_main(VIn in [[stage_in]], constant Uniforms& u [[buffer(1)]]) {\n"
         "  VOut out;\n"
         "  out.pos = u.mvpMatrix * float4(in.pos, 1.0);\n"];
    if (hasNor) [src appendString:@"  out.nor = (u.mvMatrix * float4(in.nor, 0.0)).xyz;\n"];
    if (hasUV)  [src appendString:@"  out.uv = in.uv;\n"];
    [src appendString:@"  return out;\n}\n"];

    // fragment function
    if (hasUV)
    {
        [src appendString:
            @"fragment float4 frag_main(VOut in [[stage_in]],"
             " texture2d<float> tex [[texture(0)]],"
             " sampler samp [[sampler(0)]],"
             " constant Uniforms& u [[buffer(1)]]) {\n"
             "  return tex.sample(samp, in.uv) * u.color;\n}\n"];
    }
    else
    {
        [src appendString:
            @"fragment float4 frag_main(VOut in [[stage_in]],"
             " constant Uniforms& u [[buffer(1)]]) {\n"
             "  return u.color;\n}\n"];
    }
    return src;
}

// MTLVertexDescriptor for the interleaved vertex layout used by loadBuffer().
static MTLVertexDescriptor* buildVtxDesc(mbm::FVF_PROVIDE_BY_ENGINE fvf)
{
    using F = mbm::FVF_PROVIDE_BY_ENGINE;
    const bool hasNor = (fvf == F::FVF_POS_NOR || fvf == F::FVF_POS_NOR_UV);
    const bool hasUV  = (fvf == F::FVF_POS_UV  || fvf == F::FVF_POS_NOR_UV);

    MTLVertexDescriptor* vd = [MTLVertexDescriptor new];
    vd.attributes[0].format      = MTLVertexFormatFloat3;
    vd.attributes[0].offset      = 0;
    vd.attributes[0].bufferIndex = 0;

    NSUInteger off     = 12;
    uint32_t   attrIdx = 1;
    if (hasNor)
    {
        vd.attributes[attrIdx].format      = MTLVertexFormatFloat3;
        vd.attributes[attrIdx].offset      = off;
        vd.attributes[attrIdx].bufferIndex = 0;
        ++attrIdx; off += 12;
    }
    if (hasUV)
    {
        vd.attributes[attrIdx].format      = MTLVertexFormatFloat2;
        vd.attributes[attrIdx].offset      = off;
        vd.attributes[attrIdx].bufferIndex = 0;
    }
    vd.layouts[0].stride       = strideForFVF(fvf);
    vd.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    return vd;
}

// Fills 'out' with an interleaved vertex buffer for 'count' vertices.
// Layout: [pos(3f)] [nor(3f)?] [uv(2f)?]
static void buildInterleavedVB(uint8_t* out, const NSUInteger stride,
                               const mbm::VEC3* pos, const mbm::VEC3* nor, const mbm::VEC2* uv,
                               const uint32_t count)
{
    for (uint32_t i = 0; i < count; ++i)
    {
        auto* p = reinterpret_cast<float*>(out + (NSUInteger)i * stride);
        p[0] = pos[i].x; p[1] = pos[i].y; p[2] = pos[i].z;
        if (nor) { p[3] = nor[i].x; p[4] = nor[i].y; p[5] = nor[i].z;
                   if (uv) { p[6] = uv[i].x; p[7] = uv[i].y; } }
        else if (uv) { p[3] = uv[i].x; p[4] = uv[i].y; }
    }
}

// -----------------------------------------------------------------------------

namespace mbm
{
    // ---- BUFFER_GL constructor / destructor ----

    BUFFER_GL::BUFFER_GL() :
        indexStartIB(nullptr),
        indexCountIB(nullptr),
        vertexStartVB(nullptr),
        vertexCountVB(nullptr),
        sizeOfArrayVertex(0),
        fvf(FVF_PROVIDE_BY_ENGINE::FVF_POS_UV),
        mode_draw(0),              // MTLPrimitiveTypeTriangle will be set per-draw
        mode_cull_face(0),
        mode_front_face_direction(0),
        totalSubset(0),
        initializedIndexBuffer(false),
        texture1(nullptr)
    {
        setBackendBuffer(new BUFFER_SPECIFIC());
    }

    BUFFER_GL::~BUFFER_GL()
    {
        BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
        if (backendBuffer)
        {
            delete static_cast<BUFFER_SPECIFIC*>(backendBuffer);
        }
        setBackendBuffer(nullptr);
        texture1 = nullptr;
        texture0.clear();
    }

    // ---- BUFFER_GL backend methods ---- (Must be provided by each backend)

    void BUFFER_GL::release()
    {
        BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
        if (backendBuffer) backendBuffer->release();
        totalSubset = 0;
    }

    bool BUFFER_GL::loadBuffer(const VEC3* vertex, const VEC3* normal, const VEC2* uv,
                               const uint32_t sizeOfArrayVertex, const uint32_t totalSubsets,
                               const int* vertexStartSubset, const int* vertexCountSubset,
                               const util::INFO_DRAW_MODE* info_draw_mode, const bool isDynamic)
    {
        release();
        if (!vertex || !sizeOfArrayVertex || !totalSubsets || !vertexStartSubset || !vertexCountSubset)
            return false;
        auto* ctx = getMetalCtx();
        if (!ctx || !ctx->mtlDevice) return false;

        this->totalSubset = totalSubsets;
        this->initializeVertexBufferControl(totalSubsets, sizeOfArrayVertex,
                                            vertexStartSubset, vertexCountSubset, info_draw_mode);
        this->fvf = normal && uv ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV
                  : normal       ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR
                  : uv           ? FVF_PROVIDE_BY_ENGINE::FVF_POS_UV
                                 : FVF_PROVIDE_BY_ENGINE::FVF_POS;

        const NSUInteger stride  = strideForFVF(this->fvf);
        const NSUInteger bufSize = (NSUInteger)sizeOfArrayVertex * stride;
        auto* data = new uint8_t[bufSize];
        buildInterleavedVB(data, stride, vertex, normal, uv, sizeOfArrayVertex);

        const MTLResourceOptions opts = isDynamic
            ? MTLResourceStorageModeShared
            : MTLResourceStorageModeShared; // shared is fine for both on Apple Silicon
        id<MTLBuffer> vbuf = [ctx->mtlDevice newBufferWithBytes:data length:bufSize options:opts];
        delete[] data;
        if (!vbuf) return false;
        BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
        backendBuffer->vertexBuffer = vbuf;
        backendBuffer->vertexCount  = sizeOfArrayVertex;
        return true;
    }

    bool BUFFER_GL::loadBuffer(const VEC3* vertex, const VEC3* normal, const VEC2* uv,
                               const uint32_t sizeOfArrayVertex, const uint16_t* arrayIndices,
                               const unsigned int totalSubsets, const int* indexStartSubset,
                               const int* indexCountSubset, const util::INFO_DRAW_MODE* info_draw_mode)
    {
        release();
        if (!vertex || !sizeOfArrayVertex || !arrayIndices || !totalSubsets ||
            !indexStartSubset || !indexCountSubset)
            return false;
        auto* ctx = getMetalCtx();
        if (!ctx || !ctx->mtlDevice) return false;

        this->totalSubset = totalSubsets;
        this->initializeIndexBufferControl(totalSubsets, sizeOfArrayVertex,
                                           indexStartSubset, indexCountSubset, info_draw_mode);
        this->fvf = normal && uv ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV
                  : normal       ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR
                  : uv           ? FVF_PROVIDE_BY_ENGINE::FVF_POS_UV
                                 : FVF_PROVIDE_BY_ENGINE::FVF_POS;

        // Build interleaved vertex buffer
        const NSUInteger stride  = strideForFVF(this->fvf);
        const NSUInteger vbufSz  = (NSUInteger)sizeOfArrayVertex * stride;
        auto* vdata = new uint8_t[vbufSz];
        buildInterleavedVB(vdata, stride, vertex, normal, uv, sizeOfArrayVertex);
        id<MTLBuffer> vbuf = [ctx->mtlDevice newBufferWithBytes:vdata length:vbufSz
                                                        options:MTLResourceStorageModeShared];
        delete[] vdata;
        if (!vbuf) return false;

        // Total index elements = max(start[i] + count[i]) across all subsets
        NSUInteger maxEnd = 0;
        for (uint32_t i = 0; i < totalSubsets; ++i)
        {
            NSUInteger e = (NSUInteger)indexStartSubset[i] + (NSUInteger)indexCountSubset[i];
            if (e > maxEnd) maxEnd = e;
        }
        id<MTLBuffer> ibuf = [ctx->mtlDevice newBufferWithBytes:arrayIndices
                                                         length:maxEnd * sizeof(uint16_t)
                                                        options:MTLResourceStorageModeShared];
        if (!ibuf) return false;
        BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
        backendBuffer->vertexBuffer = vbuf;
        backendBuffer->indexBuffer  = ibuf;
        backendBuffer->vertexCount  = sizeOfArrayVertex;
        backendBuffer->indexCount   = maxEnd;
        return true;
    }

    bool BUFFER_GL::loadBufferDynamic(const uint16_t* arrayIndices, const unsigned int totalSubsets,
                                      const int* indexStartSubset, const int* indexCountSubset,
                                      const bool hasNormal, const bool hasUv,
                                      const util::INFO_DRAW_MODE* info_draw_mode)
    {
        release();
        if (!arrayIndices || !totalSubsets || !indexStartSubset || !indexCountSubset)
            return false;

        auto* ctx = getMetalCtx();
        if (!ctx || !ctx->mtlDevice) return false;

        this->totalSubset = totalSubsets;
        this->fvf = (hasNormal && hasUv) ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV
                  : hasNormal            ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR
                  : hasUv                ? FVF_PROVIDE_BY_ENGINE::FVF_POS_UV
                                         : FVF_PROVIDE_BY_ENGINE::FVF_POS;

        // Scan indices to determine how many unique vertices are needed.
        NSUInteger maxIdx = 0;
        for (uint32_t i = 0; i < totalSubsets; ++i)
        {
            const int start = indexStartSubset[i];
            const int count = indexCountSubset[i];
            for (int j = 0; j < count; ++j)
            {
                const uint16_t idx = arrayIndices[start + j];
                if ((NSUInteger)idx > maxIdx) maxIdx = (NSUInteger)idx;
            }
        }
        this->sizeOfArrayVertex = static_cast<uint32_t>(maxIdx) + 1; // indices are zero-based

        this->initializeIndexBufferControl(totalSubsets, this->sizeOfArrayVertex,
                                           indexStartSubset, indexCountSubset, info_draw_mode);

        // Find the end of the flat index array (handles sparse indexStartSubset offsets).
        NSUInteger maxEnd = 0;
        for (uint32_t i = 0; i < totalSubsets; ++i)
        {
            NSUInteger end = (NSUInteger)indexStartSubset[i] + (NSUInteger)indexCountSubset[i];
            if (end > maxEnd) maxEnd = end;
        }

        // Create static index buffer.
        id<MTLBuffer> ibuf = [ctx->mtlDevice newBufferWithBytes:arrayIndices
                                                         length:maxEnd * sizeof(uint16_t)
                                                        options:MTLResourceStorageModeShared];
        if (!ibuf) return false;

        // Create dynamic (CPU-writable) vertex buffer – zero-initialised, filled by updateDynamic.
        const NSUInteger stride  = strideForFVF(this->fvf);
        const NSUInteger vbufSz  = (NSUInteger)this->sizeOfArrayVertex * stride;
        id<MTLBuffer> vbuf = [ctx->mtlDevice newBufferWithLength:vbufSz
                                                         options:MTLResourceStorageModeShared];
        if (!vbuf) return false;

        BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
        backendBuffer->vertexBuffer = vbuf;
        backendBuffer->indexBuffer  = ibuf;
        backendBuffer->indexCount   = maxEnd;
        backendBuffer->vertexCount  = this->sizeOfArrayVertex;
        return true;
    }

    bool BUFFER_GL::updateDynamic(const VEC3* vertex, const VEC3* normal, const VEC2* uv,
                                  const int* vertexStartSubset, const int* vertexCountSubset)
    {
        if (!vertex || !vertexStartSubset || !vertexCountSubset) return false;
        BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
        if (!backendBuffer || !backendBuffer->vertexBuffer) return false;

        const NSUInteger stride = strideForFVF(this->fvf);
        uint8_t* dst = reinterpret_cast<uint8_t*>(backendBuffer->vertexBuffer.contents);
        if (!dst) return false; // buffer not CPU-accessible

        for (uint32_t i = 0; i < this->totalSubset; ++i)
        {
            const uint32_t vertexStart = (uint32_t)vertexStartSubset[i];
            const uint32_t vertexCount = (uint32_t)vertexCountSubset[i];
            if (vertexCount > this->sizeOfArrayVertex) return false;
            if ((vertexStart + vertexCount) > this->sizeOfArrayVertex) return false;

            buildInterleavedVB(dst + (NSUInteger)vertexStart * stride, stride,
                               &vertex[vertexStart],
                               normal ? &normal[vertexStart] : nullptr,
                               uv     ? &uv[vertexStart]     : nullptr,
                               vertexCount);
        }
        return true;
    }

    bool BUFFER_GL::loadParticleBuffer()
    {
        release();
        // One static quad index buffer: {0,1,2, 2,1,3} — reused every particle draw.
        constexpr uint16_t indices[6]  = { 0, 1, 2, 2, 1, 3 };
        constexpr int      is          = 0;
        constexpr int      ic          = 6;
        constexpr uint32_t siz         = 0;
        this->totalSubset = 1;
        this->initializeIndexBufferControl(1, siz, &is, &ic, nullptr);

        auto* ctx = getMetalCtx();
        if (!ctx || !ctx->mtlDevice) return false;
        id<MTLBuffer> ibuf = [ctx->mtlDevice newBufferWithBytes:indices
                                                         length:sizeof(indices)
                                                        options:MTLResourceStorageModeShared];
        if (!ibuf) return false;
        BUFFER_SPECIFIC *backendBuffer = getBackendBuffer();
        backendBuffer->indexBuffer = ibuf;
        backendBuffer->indexCount  = 6;
        return true;
    }

    // ---- BASE_SHADER ----

    bool BASE_SHADER::addVar(const char* nameVar, const TYPE_VAR_SHADER typeVar,
                             const float* defaultValue, void* /*ptrShaderSpecific*/, const bool isPS)
    {
        if (!nameVar) return false;
        if (isThereVarIntoLsVars(nameVar)) return false;
        auto* var = new VAR_SHADER(std::string(nameVar), typeVar, isPS);
        // Compute the sequential float offset in buffer(2) [PS] or buffer(3) [VS].
        // Vars in each stage are packed in CFG declaration order.
        int32_t floatOffset = 0;
        for (const auto* existing : lsVar)
            if (existing->isPS == isPS)
                floatOffset += existing->sizeVar;
        *static_cast<int32_t*>(var->ptrHandleVar) = floatOffset;
        if (defaultValue)
            memcpy(var->current, defaultValue, var->sizeVar * sizeof(float));
        lsVar.push_back(var);
        return true;
    }

    void BASE_SHADER::update(void* /*ptrShaderSpecific*/) const
    {
        // In the Metal backend all per-draw uniforms (MVP matrices, color, etc.) are
        // pushed inline inside render() / renderDynamic() / renderParticle().
        // Custom VAR_SHADER values are read at draw-call time via getVarByName(), so
        // there is nothing extra to do here.
    }

    // ---- GLES_PS_VS — not used for Metal ----
    // (GLES_PS_VS is private to the OpenGL ES backend. Metal keeps its own
    // Obj-C pipeline holder in MetalShaderObjects.)

    // ---- SHADER ----

    SHADER::SHADER() :
        pShader(nullptr),
        vShader(nullptr)
    {
    }

    SHADER::~SHADER()
    {
        void *backendShaderSpecific = getBackendShaderSpecific();
        if (backendShaderSpecific)
        {
            // Release the retained MTLRenderPipelineState CFBridging object.
            CFRelease(backendShaderSpecific);
            setBackendShaderSpecific(nullptr);
        }
    }

    void SHADER::onRestore()
    {
        releaseShader();
    }

    void SHADER::releaseShader()
    {
        void *backendShaderSpecific = getBackendShaderSpecific();
        if (backendShaderSpecific)
        {
            CFRelease(backendShaderSpecific);
            setBackendShaderSpecific(nullptr);
        }
        pShader = nullptr;
        vShader = nullptr;
    }

    bool SHADER::isLoad() const noexcept
    {
        void *backendShaderSpecific = getBackendShaderSpecific();
        return backendShaderSpecific != nullptr;
    }

    bool SHADER::compileShader(BASE_SHADER* ptrPshader, BASE_SHADER* ptrVshader,
                               FVF_PROVIDE_BY_ENGINE fvf)
    {
        if (fvf == FVF_PROVIDE_BY_ENGINE::FVF_NONE) return false;
        void *backendShaderSpecific = getBackendShaderSpecific();
        if (backendShaderSpecific) return true;  // already compiled

        auto* ctx = getMetalCtx();
        if (!ctx || !ctx->mtlDevice || !ctx->metalLayer) return false;

        this->pShader = ptrPshader;
        this->vShader = ptrVshader;

        @autoreleasepool
        {
            // Choose MSL source: complete VS program, PS fragment + auto vertex, or default.
            NSString* mslSrc = nil;
            {
                const bool hasPshader = (ptrPshader && ptrPshader->getCode() && ptrPshader->getCode()[0] != '\0');
                const bool hasVshader = (ptrVshader && ptrVshader->getCode() && ptrVshader->getCode()[0] != '\0');
                if (hasVshader && hasPshader)
                {
                    // Both: VS holds a complete MSL program (vert_main + default frag_main);
                    // PS holds a custom fragment-only function.
                    // Build: VS structs + vert_main, then replace the VS default frag_main
                    // with the PS entry's frag_main so that, e.g., scale.vs + blend.ps works.
                    NSString* vsStr = [NSString stringWithUTF8String:ptrVshader->getCode()];
                    NSString* psStr = [NSString stringWithUTF8String:ptrPshader->getCode()];
                    // Fix VIn attribute indices to match the interleaved mesh FVF layout
                    // (e.g. FVF_POS_NOR_UV puts normal at attr(1) and uv at attr(2)).
                    vsStr = patchVInStruct(vsStr, fvf);
                    // Strip the VS default fragment function (everything from the last
                    // "\nfragment " onwards) and append the PS fragment code.
                    NSRange fragRange = [vsStr rangeOfString:@"\nfragment "
                                                    options:NSBackwardsSearch];
                    if (fragRange.location != NSNotFound)
                        vsStr = [vsStr substringToIndex:fragRange.location];
                    mslSrc = [vsStr stringByAppendingString:psStr];
                }
                else if (hasVshader)
                {
                    // Complete MSL program stored in the VS entry (e.g. scale.vs standalone).
                    // Fix VIn attribute indices to match the interleaved mesh FVF layout.
                    NSString* vsStr = [NSString stringWithUTF8String:ptrVshader->getCode()];
                    mslSrc = patchVInStruct(vsStr, fvf);
                }
                else if (hasPshader)
                    // Fragment-only PS entry — prepend the auto-generated vertex preamble.
                    mslSrc = [buildVertexHeader(fvf)
                        stringByAppendingString:[NSString stringWithUTF8String:ptrPshader->getCode()]];
                else
                    mslSrc = defaultMSLSource(fvf);
            }
            NSError* err = nil;
            id<MTLLibrary> lib = [ctx->mtlDevice newLibraryWithSource:mslSrc options:nil error:&err];
            if (!lib)
            {
                ERROR_LOG("Metal: shader compile error: %s",
                          [[err localizedDescription] UTF8String]);
                return false;
            }
            id<MTLFunction> vertFn = [lib newFunctionWithName:@"vert_main"];
            id<MTLFunction> fragFn = [lib newFunctionWithName:@"frag_main"];
            if (!vertFn || !fragFn)
            {
                ERROR_LOG("Metal: missing vert_main / frag_main.");
                return false;
            }
            MTLVertexDescriptor* vtxDesc    = buildVtxDesc(fvf);
            MTLPixelFormat      colorFmt    = ctx->metalLayer.pixelFormat;

            // Compile both blend-mode variants.  renderParticle() selects additivePSO;
            // render() / renderDynamic() select standardPSO.
            MBMPSOPair* pair = [MBMPSOPair new];
            pair.standardPSO = compileSinglePSO(ctx->mtlDevice, vertFn, fragFn,
                                                vtxDesc, colorFmt, /*additive=*/false);
            pair.additivePSO = compileSinglePSO(ctx->mtlDevice, vertFn, fragFn,
                                                vtxDesc, colorFmt, /*additive=*/true);
            if (!pair.standardPSO || !pair.additivePSO)
                return false;
            setBackendShaderSpecific((__bridge_retained void*)pair);
        }
        return true;
    }

    bool SHADER::render(const BUFFER_GL* pBufferId) const
    {
        void *backendShaderSpecific = getBackendShaderSpecific();
        if (!backendShaderSpecific || !pBufferId) return false;
        BUFFER_SPECIFIC *backendBuffer = pBufferId->getBackendBuffer();
        if (!backendBuffer) return false;
        auto* ctx = getMetalCtx();
        if (!ctx || !ctx->currentEncoder) return false;

        @autoreleasepool
        {
            struct MetalUniforms { float mvp[16]; float mv[16]; float color[4]; };
            MetalUniforms uni;
            memcpy(uni.mvp, SHADER::mvpMatrix.p, sizeof(uni.mvp));
            memcpy(uni.mv,  SHADER::modelView.p,  sizeof(uni.mv));
            uni.color[0] = uni.color[1] = uni.color[2] = uni.color[3] = 1.0f;
            if (pShader)
            {
                const VAR_SHADER* cv = pShader->getVarByName("color");
                if (cv) memcpy(uni.color, cv->current, sizeof(uni.color));
            }

            id<MTLRenderCommandEncoder> enc = ctx->currentEncoder;
            MBMPSOPair* pair = (__bridge MBMPSOPair*)backendShaderSpecific;

            // Select PSO based on blend state set by RENDER_STATE::set().
            // BLEND_ONE (2) = additive (src_alpha*src + 1*dst); all others = standard alpha.
            [enc setRenderPipelineState:(ctx->currentBlendState == 2)
                ? pair.additivePSO : pair.standardPSO];
            [enc setFrontFacingWinding:metalWinding(pBufferId->mode_front_face_direction)];
            [enc setCullMode:metalCullMode(pBufferId->mode_cull_face)];
            [enc setDepthStencilState: ctx->depthTestEnabled
                ? getOrCreateDepthStencilState(ctx)
                : getOrCreateNoDepthState(ctx)];
            [enc setVertexBytes:&uni   length:sizeof(uni) atIndex:1];
            [enc setFragmentBytes:&uni length:sizeof(uni) atIndex:1];
            [enc setFragmentSamplerState:getOrCreateSampler(ctx) atIndex:0];

            // Bind stage-1 texture (constant across all subsets).
            {
                const TEXTURE* t1 = pBufferId->getTextureByStage(1, 0);
                void *texturePointer = t1 ? t1->getBackendTexturePointer() : nullptr;
                [enc setFragmentTexture:(texturePointer
                    ? (__bridge id<MTLTexture>)texturePointer : nil) atIndex:1];
            }

            // Upload custom fragment uniforms to [[buffer(2)]].
            if (pShader && !pShader->lsVar.empty())
            {
                int32_t totalF = 0;
                for (const auto* v : pShader->lsVar) totalF += v->sizeVar;
                if (totalF > 0)
                {
                    float fbuf[64] = {};
                    for (const auto* v : pShader->lsVar)
                    {
                        const int32_t off = *static_cast<const int32_t*>(v->ptrHandleVar);
                        memcpy(fbuf + off, v->current, (size_t)v->sizeVar * sizeof(float));
                    }
                    [enc setFragmentBytes:fbuf length:(NSUInteger)totalF * sizeof(float) atIndex:2];
                }
            }

            // Upload custom vertex uniforms to [[buffer(3)]].
            if (vShader && !vShader->lsVar.empty())
            {
                int32_t totalF = 0;
                for (const auto* v : vShader->lsVar) totalF += v->sizeVar;
                if (totalF > 0)
                {
                    float vbuf[64] = {};
                    for (const auto* v : vShader->lsVar)
                    {
                        const int32_t off = *static_cast<const int32_t*>(v->ptrHandleVar);
                        memcpy(vbuf + off, v->current, (size_t)v->sizeVar * sizeof(float));
                    }
                    [enc setVertexBytes:vbuf length:(NSUInteger)totalF * sizeof(float) atIndex:3];
                }
            }

            const MTLPrimitiveType prim   = metalPrimitive(pBufferId->mode_draw);
            const NSUInteger       stride = strideForFVF(pBufferId->fvf);

            if (pBufferId->isIndexBuffer())
            {
                if (!backendBuffer->vertexBuffer || !backendBuffer->indexBuffer) return false;
                [enc setVertexBuffer:backendBuffer->vertexBuffer offset:0 atIndex:0];
                for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
                {
                    const TEXTURE* t = pBufferId->getTextureByStage(0, i);
                    void *texturePointer = t ? t->getBackendTexturePointer() : nullptr;
                    [enc setFragmentTexture:(texturePointer
                        ? (__bridge id<MTLTexture>)texturePointer : nil) atIndex:0];
                    const NSUInteger off =
                        (NSUInteger)pBufferId->indexStartIB[i] * sizeof(uint16_t);
                    [enc drawIndexedPrimitives:prim
                                    indexCount:(NSUInteger)pBufferId->indexCountIB[i]
                                     indexType:MTLIndexTypeUInt16
                                   indexBuffer:backendBuffer->indexBuffer
                             indexBufferOffset:off];
                }
            }
            else
            {
                if (!backendBuffer->vertexBuffer) return false;
                for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
                {
                    const TEXTURE* t = pBufferId->getTextureByStage(0, i);
                    void *texturePointer = t ? t->getBackendTexturePointer() : nullptr;
                    [enc setFragmentTexture:(texturePointer
                        ? (__bridge id<MTLTexture>)texturePointer : nil) atIndex:0];
                    const NSUInteger off =
                        (NSUInteger)pBufferId->vertexStartVB[i] * stride;
                    [enc setVertexBuffer:backendBuffer->vertexBuffer offset:off atIndex:0];
                    [enc drawPrimitives:prim
                            vertexStart:0
                            vertexCount:(NSUInteger)pBufferId->vertexCountVB[i]];
                }
            }
        }
        return true;
    }

    bool SHADER::renderDynamic(const BUFFER_GL* pBufferId, const VEC3* vertex,
                               const VEC3* normal, const VEC2* uv) const
    {
        void *backendShaderSpecific = getBackendShaderSpecific();
        if (!backendShaderSpecific || !pBufferId || !vertex) return false;
        BUFFER_SPECIFIC *backendBuffer = pBufferId->getBackendBuffer();
        if (!backendBuffer) return false;
        auto* ctx = getMetalCtx();
        if (!ctx || !ctx->currentEncoder) return false;

        const NSUInteger stride = strideForFVF(pBufferId->fvf);
        const uint32_t   totalV = pBufferId->sizeOfArrayVertex;
        if (!totalV) return false;

        @autoreleasepool
        {
            // Build CPU-side interleaved vertex data and upload as a one-shot buffer.
            auto* data = new uint8_t[(NSUInteger)totalV * stride];
            buildInterleavedVB(data, stride, vertex, normal, uv, totalV);
            id<MTLBuffer> vbuf =
                [ctx->mtlDevice newBufferWithBytes:data
                                           length:(NSUInteger)totalV * stride
                                          options:MTLResourceStorageModeShared];
            delete[] data;
            if (!vbuf) return false;

            struct MetalUniforms { float mvp[16]; float mv[16]; float color[4]; };
            MetalUniforms uni;
            memcpy(uni.mvp, SHADER::mvpMatrix.p, sizeof(uni.mvp));
            memcpy(uni.mv,  SHADER::modelView.p,  sizeof(uni.mv));
            uni.color[0] = uni.color[1] = uni.color[2] = uni.color[3] = 1.0f;

            id<MTLRenderCommandEncoder> enc = ctx->currentEncoder;
            MBMPSOPair* pairD = (__bridge MBMPSOPair*)backendShaderSpecific;

            // Select PSO based on blend state set by RENDER_STATE::set().
            // BLEND_ONE (2) = additive (src_alpha*src + 1*dst); all others = standard alpha.
            [enc setRenderPipelineState:(ctx->currentBlendState == 2)
                ? pairD.additivePSO : pairD.standardPSO];
            [enc setFrontFacingWinding:metalWinding(pBufferId->mode_front_face_direction)];
            [enc setCullMode:metalCullMode(pBufferId->mode_cull_face)];
            [enc setDepthStencilState: ctx->depthTestEnabled
                ? getOrCreateDepthStencilState(ctx)
                : getOrCreateNoDepthState(ctx)];
            [enc setVertexBytes:&uni   length:sizeof(uni) atIndex:1];
            [enc setFragmentBytes:&uni length:sizeof(uni) atIndex:1];
            [enc setFragmentSamplerState:getOrCreateSampler(ctx) atIndex:0];

            // Bind stage-1 texture (constant across all subsets).
            {
                const TEXTURE* t1 = pBufferId->getTextureByStage(1, 0);
                void *texturePointer = t1 ? t1->getBackendTexturePointer() : nullptr;
                [enc setFragmentTexture:(texturePointer
                    ? (__bridge id<MTLTexture>)texturePointer : nil) atIndex:1];
            }

            // Upload custom fragment uniforms to [[buffer(2)]].
            if (pShader && !pShader->lsVar.empty())
            {
                int32_t totalF = 0;
                for (const auto* v : pShader->lsVar) totalF += v->sizeVar;
                if (totalF > 0)
                {
                    float fbuf2[64] = {};
                    for (const auto* v : pShader->lsVar)
                    {
                        const int32_t off = *static_cast<const int32_t*>(v->ptrHandleVar);
                        memcpy(fbuf2 + off, v->current, (size_t)v->sizeVar * sizeof(float));
                    }
                    [enc setFragmentBytes:fbuf2 length:(NSUInteger)totalF * sizeof(float) atIndex:2];
                }
            }

            // Upload custom vertex uniforms to [[buffer(3)]].
            if (vShader && !vShader->lsVar.empty())
            {
                int32_t totalF = 0;
                for (const auto* v : vShader->lsVar) totalF += v->sizeVar;
                if (totalF > 0)
                {
                    float vbuf2[64] = {};
                    for (const auto* v : vShader->lsVar)
                    {
                        const int32_t off = *static_cast<const int32_t*>(v->ptrHandleVar);
                        memcpy(vbuf2 + off, v->current, (size_t)v->sizeVar * sizeof(float));
                    }
                    [enc setVertexBytes:vbuf2 length:(NSUInteger)totalF * sizeof(float) atIndex:3];
                }
            }

            [enc setVertexBuffer:vbuf offset:0 atIndex:0];

            const MTLPrimitiveType prim = metalPrimitive(pBufferId->mode_draw);

            if (pBufferId->isIndexBuffer())
            {
                if (!backendBuffer->indexBuffer) return false;
                for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
                {
                    const TEXTURE* t = pBufferId->getTextureByStage(0, i);
                    void *texturePointer = t ? t->getBackendTexturePointer() : nullptr;
                    [enc setFragmentTexture:(texturePointer
                        ? (__bridge id<MTLTexture>)texturePointer : nil) atIndex:0];
                    const NSUInteger off =
                        (NSUInteger)pBufferId->indexStartIB[i] * sizeof(uint16_t);
                    [enc drawIndexedPrimitives:prim
                                    indexCount:(NSUInteger)pBufferId->indexCountIB[i]
                                     indexType:MTLIndexTypeUInt16
                                   indexBuffer:backendBuffer->indexBuffer
                             indexBufferOffset:off];
                }
            }
            else
            {
                for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
                {
                    const TEXTURE* t = pBufferId->getTextureByStage(0, i);
                    void *texturePointer = t ? t->getBackendTexturePointer() : nullptr;
                    [enc setFragmentTexture:(texturePointer
                        ? (__bridge id<MTLTexture>)texturePointer : nil) atIndex:0];
                    const NSUInteger off =
                        (NSUInteger)pBufferId->vertexStartVB[i] * stride;
                    [enc setVertexBuffer:vbuf offset:off atIndex:0];
                    [enc drawPrimitives:prim
                            vertexStart:0
                            vertexCount:(NSUInteger)pBufferId->vertexCountVB[i]];
                }
            }
        }
        return true;
    }

    bool SHADER::renderParticle(const BUFFER_GL* pBufferId,
                                const PARTICLE_CONTROL* particleControl) const
    {
        void *backendShaderSpecific = getBackendShaderSpecific();
        if (!backendShaderSpecific || !pBufferId || !particleControl) return false;
        BUFFER_SPECIFIC *backendBuffer = pBufferId->getBackendBuffer();
        if (!backendBuffer || !backendBuffer->indexBuffer) return false;
        auto* ctx = getMetalCtx();
        if (!ctx || !ctx->currentEncoder) return false;

        const uint32_t totalAlive = particleControl->getTotalAlive();
        if (!totalAlive) return true;

        @autoreleasepool
        {
            id<MTLRenderCommandEncoder> enc = ctx->currentEncoder;
            MBMPSOPair* pairP = (__bridge MBMPSOPair*)backendShaderSpecific;

            // Match OpenGL: renderParticle() does not force additive blend — respect the
            // blend state last set by RENDER_STATE::set() so that particles such as the
            // black-hole timer (which need standard src-alpha blending to darken the
            // background) behave consistently with the OpenGL backend.
            [enc setRenderPipelineState:(ctx->currentBlendState == 2)
                ? pairP.additivePSO : pairP.standardPSO];
            [enc setFrontFacingWinding:metalWinding(pBufferId->mode_front_face_direction)];
            [enc setCullMode:MTLCullModeNone]; // match OpenGL: disable face culling for particles
            [enc setDepthStencilState:getOrCreateNoDepthState(ctx)]; // match OpenGL: no depth test
            [enc setFragmentSamplerState:getOrCreateSampler(ctx) atIndex:0];

            const TEXTURE* tex0 = pBufferId->getTextureByStage(0, 0);
            void *texturePointer = tex0 ? tex0->getBackendTexturePointer() : nullptr;
            [enc setFragmentTexture:(texturePointer
                ? (__bridge id<MTLTexture>)texturePointer : nil) atIndex:0];

            const VERTEX_UV*    vbuf      = particleControl->getVertexBuffer();
            const ATT_PARTICLE* particles = particleControl->getAttParticle();

            // Build the base fragment uniform buffer from pShader vars.
            // The particle PS reads color from [[buffer(2)]] f[0..3],
            // and enableAlphaFromColor from f[4]; per-particle color overrides f[0..3].
            const VAR_SHADER* colorVar = pShader ? pShader->getVarByName("color") : nullptr;
            const int32_t colorOff = colorVar
                ? *static_cast<const int32_t*>(colorVar->ptrHandleVar) : -1;
            const bool hasColor = (colorVar != nullptr);

            float fragBuf[64] = {};
            int32_t totalFragF = 0;
            if (pShader)
            {
                for (const auto* v : pShader->lsVar) totalFragF += v->sizeVar;
                for (const auto* v : pShader->lsVar)
                {
                    const int32_t off = *static_cast<const int32_t*>(v->ptrHandleVar);
                    if (off >= 0 && off + v->sizeVar <= 64)
                        memcpy(fragBuf + off, v->current, (size_t)v->sizeVar * sizeof(float));
                }
            }

            struct MetalUniforms { float mvp[16]; float mv[16]; float color[4]; };
            MetalUniforms uni;
            memcpy(uni.mvp, SHADER::mvpMatrix.p, sizeof(uni.mvp));
            memcpy(uni.mv,  SHADER::modelView.p,  sizeof(uni.mv));
            uni.color[0] = uni.color[1] = uni.color[2] = uni.color[3] = 1.0f;

            for (uint32_t i = 0; i < totalAlive; ++i)
            {
                if (hasColor && colorOff >= 0 && colorOff + 4 <= 64)
                {
                    const ATT_PARTICLE& p = particles[i];
                    fragBuf[colorOff + 0] = p.r; fragBuf[colorOff + 1] = p.g;
                    fragBuf[colorOff + 2] = p.b; fragBuf[colorOff + 3] = p.a;
                    uni.color[0] = p.r; uni.color[1] = p.g;
                    uni.color[2] = p.b; uni.color[3] = p.a;
                }
                [enc setVertexBytes:&vbuf[i * 4]  length:sizeof(VERTEX_UV) * 4 atIndex:0];
                [enc setVertexBytes:&uni           length:sizeof(uni)          atIndex:1];
                [enc setFragmentBytes:&uni         length:sizeof(uni)          atIndex:1];
                if (totalFragF > 0)
                    [enc setFragmentBytes:fragBuf
                                  length:(NSUInteger)totalFragF * sizeof(float)
                                 atIndex:2];
                [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                indexCount:6
                                 indexType:MTLIndexTypeUInt16
                               indexBuffer:backendBuffer->indexBuffer
                         indexBufferOffset:0];
            }
        }
        return true;
    }

    bool SHADER::renderParticle(const BUFFER_GL* pBufferId,
                                const FLUID_GROUP* pGroup) const
    {
        void *backendShaderSpecific = getBackendShaderSpecific();
        if (!backendShaderSpecific || !pBufferId || !pGroup) return false;
        BUFFER_SPECIFIC *backendBuffer = pBufferId->getBackendBuffer();
        if (!backendBuffer || !backendBuffer->indexBuffer) return false;
        auto* ctx = getMetalCtx();
        if (!ctx || !ctx->currentEncoder) return false;

        const uint32_t totalToRender = pGroup->totalParticleToRender;
        if (!totalToRender) return true;

        @autoreleasepool
        {
            id<MTLRenderCommandEncoder> enc = ctx->currentEncoder;
            MBMPSOPair* pairF = (__bridge MBMPSOPair*)backendShaderSpecific;

            [enc setRenderPipelineState:pairF.additivePSO];  // BLEND_ONE: src*alpha + dst*1
            [enc setFrontFacingWinding:MTLWindingCounterClockwise];
            [enc setCullMode:MTLCullModeNone]; // no culling for particles
            [enc setDepthStencilState:getOrCreateNoDepthState(ctx)]; // no depth test
            [enc setFragmentSamplerState:getOrCreateSampler(ctx) atIndex:0];

            const TEXTURE* tex0 = pBufferId->getTextureByStage(0, 0);
            void *texturePointer = tex0 ? tex0->getBackendTexturePointer() : nullptr;
            [enc setFragmentTexture:(texturePointer
                ? (__bridge id<MTLTexture>)texturePointer : nil) atIndex:0];

            struct MetalUniforms { float mvp[16]; float mv[16]; float color[4]; };
            MetalUniforms uni;
            memcpy(uni.mvp, SHADER::mvpMatrix.p, sizeof(uni.mvp));
            memcpy(uni.mv,  SHADER::modelView.p,  sizeof(uni.mv));
            uni.color[0] = uni.color[1] = uni.color[2] = uni.color[3] = 1.0f;

            // Build fragment uniform buffer from pShader vars; override color with pGroup->color.
            const VAR_SHADER* colorVar = pShader ? pShader->getVarByName("color") : nullptr;
            const int32_t colorOff = colorVar
                ? *static_cast<const int32_t*>(colorVar->ptrHandleVar) : -1;

            float fragBuf[64] = {};
            int32_t totalFragF = 0;
            if (pShader)
            {
                for (const auto* v : pShader->lsVar) totalFragF += v->sizeVar;
                for (const auto* v : pShader->lsVar)
                {
                    const int32_t off = *static_cast<const int32_t*>(v->ptrHandleVar);
                    if (off >= 0 && off + v->sizeVar <= 64)
                        memcpy(fragBuf + off, v->current, (size_t)v->sizeVar * sizeof(float));
                }
            }
            if (pGroup->color && colorOff >= 0 && colorOff + 4 <= 64)
            {
                fragBuf[colorOff + 0] = pGroup->color->r;
                fragBuf[colorOff + 1] = pGroup->color->g;
                fragBuf[colorOff + 2] = pGroup->color->b;
                fragBuf[colorOff + 3] = pGroup->color->a;
                uni.color[0] = pGroup->color->r; uni.color[1] = pGroup->color->g;
                uni.color[2] = pGroup->color->b; uni.color[3] = pGroup->color->a;
            }

            // Color is constant per group — set uniforms once before the particle loop.
            [enc setVertexBytes:&uni length:sizeof(uni) atIndex:1];
            [enc setFragmentBytes:&uni length:sizeof(uni) atIndex:1];
            if (totalFragF > 0)
                [enc setFragmentBytes:fragBuf
                              length:(NSUInteger)totalFragF * sizeof(float)
                             atIndex:2];

            // Interleaved pos+uv layout that matches buildVtxDesc(FVF_POS_UV): stride 20.
            struct PVert { float x, y, z, u, v; };
            PVert quad[4];

            const VEC3* vpos = pGroup->vertex_particle;
            const VEC2* uvs  = pGroup->uv;

            for (uint32_t i = 0; i < totalToRender; ++i)
            {
                const VEC3* p4  = &vpos[i * 4];
                const VEC2* uv4 = pGroup->segmented ? &uvs[i * 4] : uvs;
                for (int v = 0; v < 4; ++v)
                {
                    quad[v].x = p4[v].x;  quad[v].y = p4[v].y;  quad[v].z = p4[v].z;
                    quad[v].u = uv4[v].x; quad[v].v = uv4[v].y;
                }
                [enc setVertexBytes:quad length:sizeof(quad) atIndex:0];
                [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                indexCount:6
                                 indexType:MTLIndexTypeUInt16
                               indexBuffer:backendBuffer->indexBuffer
                         indexBufferOffset:0];
            }
        }
        return true;
    }

} // namespace mbm

#endif // USE_METAL
