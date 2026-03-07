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
#include <specific-metal.h>
#include <device.h>
#include <util-interface.h>
#include <particle-control.h>
#include <header-mesh.h>

// ---- file-scope Metal helpers -----------------------------------------------

static mbm::SPECIFIC_AUX_CONTEXT_DEVICE* getMetalCtx()
{
    mbm::DEVICE* dev = mbm::DEVICE::getInstance();
    return dev ? dev->specificContextDevice : nullptr;
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
        bs = new BUFFER_SPECIFIC();
    }

    BUFFER_GL::~BUFFER_GL()
    {
        if (bs)
        {
            delete static_cast<BUFFER_SPECIFIC*>(bs);
        }
        bs       = nullptr;
        texture1 = nullptr;
        texture0.clear();
    }

    // ---- BUFFER_GL backend methods ---- (Must be provided by each backend)

    void BUFFER_GL::release()
    {
        if (bs) bs->release();
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
        bs->vertexBuffer = vbuf;
        bs->vertexCount  = sizeOfArrayVertex;
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
        bs->vertexBuffer = vbuf;
        bs->indexBuffer  = ibuf;
        bs->vertexCount  = sizeOfArrayVertex;
        bs->indexCount   = maxEnd;
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

        bs->vertexBuffer = vbuf;
        bs->indexBuffer  = ibuf;
        bs->indexCount   = maxEnd;
        bs->vertexCount  = this->sizeOfArrayVertex;
        return true;
    }

    bool BUFFER_GL::updateDynamic(const VEC3* vertex, const VEC3* normal, const VEC2* uv,
                                  const int* vertexStartSubset, const int* vertexCountSubset)
    {
        if (!vertex || !vertexStartSubset || !vertexCountSubset) return false;
        if (!bs || !bs->vertexBuffer) return false;

        const NSUInteger stride = strideForFVF(this->fvf);
        uint8_t* dst = reinterpret_cast<uint8_t*>(bs->vertexBuffer.contents);
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
        bs->indexBuffer = ibuf;
        bs->indexCount  = 6;
        return true;
    }

    // ---- BASE_SHADER ----

    bool BASE_SHADER::addVar(const char* nameVar, const TYPE_VAR_SHADER typeVar,
                             const float* defaultValue, void* /*ptrShaderSpecific*/, const bool isPS)
    {
        if (!nameVar) return false;
        if (isThereVarIntoLsVars(nameVar)) return false;
        auto* var = new VAR_SHADER(std::string(nameVar), typeVar, isPS);
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
    // (GLES_PS_VS is declared in specific-opengl_es.h and only needed by
    //  the OpenGL backend.  Metal does not include that header.)

    // ---- SHADER ----

    SHADER::SHADER() : ptrShaderSpecific(nullptr),
        pShader(nullptr),
        vShader(nullptr)
    {
    }

    SHADER::~SHADER()
    {
        if (ptrShaderSpecific)
        {
            // Release the retained MTLRenderPipelineState CFBridging object.
            CFRelease(ptrShaderSpecific);
            ptrShaderSpecific = nullptr;
        }
    }

    void SHADER::onRestore()
    {
        releaseShader();
    }

    void SHADER::releaseShader()
    {
        if (ptrShaderSpecific)
        {
            CFRelease(ptrShaderSpecific);
            ptrShaderSpecific = nullptr;
        }
        pShader = nullptr;
        vShader = nullptr;
    }

    bool SHADER::isLoad() const noexcept
    {
        return ptrShaderSpecific != nullptr;
    }

    bool SHADER::compileShader(BASE_SHADER* ptrPshader, BASE_SHADER* ptrVshader,
                               FVF_PROVIDE_BY_ENGINE fvf)
    {
        if (fvf == FVF_PROVIDE_BY_ENGINE::FVF_NONE) return false;
        if (ptrShaderSpecific) return true;  // already compiled

        auto* ctx = getMetalCtx();
        if (!ctx || !ctx->mtlDevice || !ctx->metalLayer) return false;

        this->pShader = ptrPshader;
        this->vShader = ptrVshader;

        @autoreleasepool
        {
            NSString* mslSrc = defaultMSLSource(fvf);
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
            MTLRenderPipelineDescriptor* psd = [[MTLRenderPipelineDescriptor alloc] init];
            psd.label            = @"MBM";
            psd.vertexFunction   = vertFn;
            psd.fragmentFunction = fragFn;
            psd.vertexDescriptor = buildVtxDesc(fvf);
            psd.colorAttachments[0].pixelFormat             = ctx->metalLayer.pixelFormat;
            psd.colorAttachments[0].blendingEnabled         = YES;
            psd.colorAttachments[0].sourceRGBBlendFactor        = MTLBlendFactorSourceAlpha;
            psd.colorAttachments[0].destinationRGBBlendFactor   = MTLBlendFactorOneMinusSourceAlpha;
            psd.colorAttachments[0].sourceAlphaBlendFactor      = MTLBlendFactorOne;
            psd.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
            id<MTLRenderPipelineState> pso =
                [ctx->mtlDevice newRenderPipelineStateWithDescriptor:psd error:&err];
            if (!pso)
            {
                ERROR_LOG("Metal: pipeline state error: %s",
                          [[err localizedDescription] UTF8String]);
                return false;
            }
            ptrShaderSpecific = (__bridge_retained void*)pso;
        }
        return true;
    }

    bool SHADER::render(const BUFFER_GL* pBufferId) const
    {
        if (!ptrShaderSpecific || !pBufferId || !pBufferId->bs) return false;
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
            id<MTLRenderPipelineState>  pso =
                (__bridge id<MTLRenderPipelineState>)ptrShaderSpecific;

            [enc setRenderPipelineState:pso];
            [enc setVertexBytes:&uni   length:sizeof(uni) atIndex:1];
            [enc setFragmentBytes:&uni length:sizeof(uni) atIndex:1];
            [enc setFragmentSamplerState:getOrCreateSampler(ctx) atIndex:0];

            const MTLPrimitiveType prim   = metalPrimitive(pBufferId->mode_draw);
            const NSUInteger       stride = strideForFVF(pBufferId->fvf);

            if (pBufferId->isIndexBuffer())
            {
                if (!pBufferId->bs->vertexBuffer || !pBufferId->bs->indexBuffer) return false;
                [enc setVertexBuffer:pBufferId->bs->vertexBuffer offset:0 atIndex:0];
                for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
                {
                    const TEXTURE* t = pBufferId->getTextureByStage(0, i);
                    [enc setFragmentTexture:(t && t->ptrTexture
                        ? (__bridge id<MTLTexture>)t->ptrTexture : nil) atIndex:0];
                    const NSUInteger off =
                        (NSUInteger)pBufferId->indexStartIB[i] * sizeof(uint16_t);
                    [enc drawIndexedPrimitives:prim
                                    indexCount:(NSUInteger)pBufferId->indexCountIB[i]
                                     indexType:MTLIndexTypeUInt16
                                   indexBuffer:pBufferId->bs->indexBuffer
                             indexBufferOffset:off];
                }
            }
            else
            {
                if (!pBufferId->bs->vertexBuffer) return false;
                for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
                {
                    const TEXTURE* t = pBufferId->getTextureByStage(0, i);
                    [enc setFragmentTexture:(t && t->ptrTexture
                        ? (__bridge id<MTLTexture>)t->ptrTexture : nil) atIndex:0];
                    const NSUInteger off =
                        (NSUInteger)pBufferId->vertexStartVB[i] * stride;
                    [enc setVertexBuffer:pBufferId->bs->vertexBuffer offset:off atIndex:0];
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
        if (!ptrShaderSpecific || !pBufferId || !vertex) return false;
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
            id<MTLRenderPipelineState>  pso =
                (__bridge id<MTLRenderPipelineState>)ptrShaderSpecific;

            [enc setRenderPipelineState:pso];
            [enc setVertexBytes:&uni   length:sizeof(uni) atIndex:1];
            [enc setFragmentBytes:&uni length:sizeof(uni) atIndex:1];
            [enc setFragmentSamplerState:getOrCreateSampler(ctx) atIndex:0];
            [enc setVertexBuffer:vbuf offset:0 atIndex:0];

            const MTLPrimitiveType prim = metalPrimitive(pBufferId->mode_draw);

            if (pBufferId->isIndexBuffer())
            {
                if (!pBufferId->bs || !pBufferId->bs->indexBuffer) return false;
                for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
                {
                    const TEXTURE* t = pBufferId->getTextureByStage(0, i);
                    [enc setFragmentTexture:(t && t->ptrTexture
                        ? (__bridge id<MTLTexture>)t->ptrTexture : nil) atIndex:0];
                    const NSUInteger off =
                        (NSUInteger)pBufferId->indexStartIB[i] * sizeof(uint16_t);
                    [enc drawIndexedPrimitives:prim
                                    indexCount:(NSUInteger)pBufferId->indexCountIB[i]
                                     indexType:MTLIndexTypeUInt16
                                   indexBuffer:pBufferId->bs->indexBuffer
                             indexBufferOffset:off];
                }
            }
            else
            {
                for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
                {
                    const TEXTURE* t = pBufferId->getTextureByStage(0, i);
                    [enc setFragmentTexture:(t && t->ptrTexture
                        ? (__bridge id<MTLTexture>)t->ptrTexture : nil) atIndex:0];
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
        if (!ptrShaderSpecific || !pBufferId || !particleControl) return false;
        if (!pBufferId->bs || !pBufferId->bs->indexBuffer) return false;
        auto* ctx = getMetalCtx();
        if (!ctx || !ctx->currentEncoder) return false;

        const uint32_t totalAlive = particleControl->getTotalAlive();
        if (!totalAlive) return true;

        @autoreleasepool
        {
            id<MTLRenderCommandEncoder> enc = ctx->currentEncoder;
            id<MTLRenderPipelineState>  pso =
                (__bridge id<MTLRenderPipelineState>)ptrShaderSpecific;

            [enc setRenderPipelineState:pso];
            [enc setFragmentSamplerState:getOrCreateSampler(ctx) atIndex:0];

            const TEXTURE* tex0 = pBufferId->getTextureByStage(0, 0);
            [enc setFragmentTexture:(tex0 && tex0->ptrTexture
                ? (__bridge id<MTLTexture>)tex0->ptrTexture : nil) atIndex:0];

            const VERTEX_UV*    vbuf      = particleControl->getVertexBuffer();
            const ATT_PARTICLE* particles = particleControl->getAttParticle();
            const bool          hasColor  =
                (pShader && pShader->getVarByName("color") != nullptr);

            struct MetalUniforms { float mvp[16]; float mv[16]; float color[4]; };
            MetalUniforms uni;
            memcpy(uni.mvp, SHADER::mvpMatrix.p, sizeof(uni.mvp));
            memcpy(uni.mv,  SHADER::modelView.p,  sizeof(uni.mv));

            for (uint32_t i = 0; i < totalAlive; ++i)
            {
                if (hasColor)
                {
                    const ATT_PARTICLE& p = particles[i];
                    uni.color[0] = p.r; uni.color[1] = p.g;
                    uni.color[2] = p.b; uni.color[3] = p.a;
                }
                else
                {
                    uni.color[0] = uni.color[1] = uni.color[2] = uni.color[3] = 1.0f;
                }
                [enc setVertexBytes:&vbuf[i * 4]  length:sizeof(VERTEX_UV) * 4 atIndex:0];
                [enc setVertexBytes:&uni           length:sizeof(uni)          atIndex:1];
                [enc setFragmentBytes:&uni         length:sizeof(uni)          atIndex:1];
                [enc drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                indexCount:6
                                 indexType:MTLIndexTypeUInt16
                               indexBuffer:pBufferId->bs->indexBuffer
                         indexBufferOffset:0];
            }
        }
        return true;
    }

    bool SHADER::renderParticle(const BUFFER_GL* /*pBufferId*/,
                                const FLUID_GROUP* /*pGroup*/) const
    {
        return false;
    }

} // namespace mbm

#endif // USE_METAL
