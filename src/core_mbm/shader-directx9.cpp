/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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


#if defined (USE_DIRECTX9)

#include "dummy-engine.h" // for compiler_message, you can remove it after implement the functions

#include <shader.h>
#include <util-interface.h>
#include <shader-var-cfg.h>
#include <device.h>
#include <specific-directx9.h>
#include <header-mesh.h>
#include <draw-compatibility.h>
#include <texture-manager.h>
namespace mbm
{
    BUFFER_SPECIFIC::BUFFER_SPECIFIC() noexcept :
        FVF(FVF_PROVIDE_BY_ENGINE::FVF_POS),
        sizeStructVertexInBytes(0),
        pVertexBuffer(nullptr),
        pIndexBuffer(nullptr)
    {

    }

    BUFFER_SPECIFIC::~BUFFER_SPECIFIC()
    {
        this->release();
    }

    void BUFFER_SPECIFIC::release()
    {
        if (pVertexBuffer)
            pVertexBuffer->Release();
        pVertexBuffer = nullptr;

        if (pIndexBuffer)
            pIndexBuffer->Release();
        pIndexBuffer = nullptr;
        sizeStructVertexInBytes = 0;
    }

    BUFFER_GL::BUFFER_GL() :
        indexStartIB(nullptr),
        indexCountIB(nullptr),
        vertexStartVB(nullptr),
        vertexCountVB(nullptr),
        sizeOfArrayVertex(0),
        mode_draw(util::MODE_DRAW_TRIANGLES),
        mode_cull_face(util::CULL_BACK),
        mode_front_face_direction(util::CCW),
        totalSubset(0),
        initializedIndexBuffer(false),
        texture1(nullptr)
    {
        //we initialize this at the moment (just once)
        bs = new BUFFER_SPECIFIC();
    }

    BUFFER_GL::~BUFFER_GL()
    {
        if (bs)
            delete bs;
        bs = nullptr;
        texture1 = nullptr;
        texture0.clear();
    }

    void BUFFER_GL::release()
    {
        if (this->vertexStartVB)
            delete[] this->vertexStartVB;
        if (this->vertexCountVB)
            delete[] this->vertexCountVB;
        if (this->indexStartIB)
            delete[] this->indexStartIB;
        if (this->indexCountIB)
            delete[] this->indexCountIB;

        this->vertexStartVB = nullptr;
        this->vertexCountVB = nullptr;
        this->indexStartIB = nullptr;
        this->indexCountIB = nullptr;
        this->sizeOfArrayVertex = 0;
        this->initializedIndexBuffer = false;
        //we do not delete bs
        bs->release();
        totalSubset   = 0;
    }

    D3D_VERTEX_CONVERTER::D3D_VERTEX_CONVERTER(const VEC3* _pos, const VEC3* _normal, const VEC2* _uv, unsigned int _size_array) noexcept:
        pos(_pos), normal(_normal),uv(_uv), size_array(_size_array)
    {
        if (pos && uv && normal)
        {
            FVF = FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV;
        }
        else if (pos && normal)
        {
            FVF = FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR;
        }
        else if (pos && uv)
        {
            FVF = FVF_PROVIDE_BY_ENGINE::FVF_POS_UV;
        }
        else
        {
            FVF = FVF_PROVIDE_BY_ENGINE::FVF_POS;
        }
    }

    void D3D_VERTEX_CONVERTER::copyTod3dVertexBuffer(void* pvertex) const noexcept
    {
        if (pvertex)
        {
            switch (FVF)
            {
                case FVF_PROVIDE_BY_ENGINE::FVF_POS:
                {
                    VEC3* vertex = static_cast<VEC3*>(pvertex);
                    for (unsigned int i = 0; i < size_array; ++i)
                    {
                        vertex[i].x = pos[i].x;
                        vertex[i].y = pos[i].y;
                        vertex[i].z = pos[i].z;
                    }
                }
                break;
                case FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR:
                {
                    VERTEX_NORMAL* vertex = static_cast<VERTEX_NORMAL*>(pvertex);
                    for (unsigned int i = 0; i < size_array; ++i)
                    {
                        vertex[i].x = pos[i].x;
                        vertex[i].y = pos[i].y;
                        vertex[i].z = pos[i].z;
                        vertex[i].nx = normal[i].x;
                        vertex[i].ny = normal[i].y;
                        vertex[i].nz = normal[i].z;
                    }
                }
                break;
                case FVF_PROVIDE_BY_ENGINE::FVF_POS_UV:
                {
                    VERTEX_UV* vertex = static_cast<VERTEX_UV*>(pvertex);
                    for (unsigned int i = 0; i < size_array; ++i)
                    {
                        vertex[i].x = pos[i].x;
                        vertex[i].y = pos[i].y;
                        vertex[i].z = pos[i].z;
                        vertex[i].u = uv[i].x;
                        vertex[i].v = uv[i].y;
                    }
                }
                break;
                case FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV:
                {
                    VERTEX_NORMAL_UV* vertex = static_cast<VERTEX_NORMAL_UV*>(pvertex);
                    for (unsigned int i = 0; i < size_array; ++i)
                    {
                        vertex[i].x = pos[i].x;
                        vertex[i].y = pos[i].y;
                        vertex[i].z = pos[i].z;
                        vertex[i].nx = normal[i].x;
                        vertex[i].ny = normal[i].y;
                        vertex[i].nz = normal[i].z;
                        vertex[i].u = uv[i].x;
                        vertex[i].v = uv[i].y;
                    }
                }
                break;
            }
        }
    }

    FVF_PROVIDE_BY_ENGINE D3D_VERTEX_CONVERTER::getFVF() const noexcept
    {
        return FVF;
    }

    DWORD D3D_VERTEX_CONVERTER::get3d3FVF() const
    {
        DWORD d3dFVF = 0;
        switch (this->FVF)
        {
        case FVF_PROVIDE_BY_ENGINE::FVF_POS:
        {
            d3dFVF = (D3DFVF_XYZ);
        }
        break;
        case FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR:
        {
            d3dFVF = (D3DFVF_XYZ | D3DFVF_NORMAL);
        }
        break;
        case FVF_PROVIDE_BY_ENGINE::FVF_POS_UV:
        {
            d3dFVF = (D3DFVF_XYZ | D3DFVF_TEX0);
        }
        break;
        case FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV:
        {
            d3dFVF = (D3DFVF_XYZ | D3DFVF_NORMAL| D3DFVF_TEX0);
        }
        break;
        }
        return d3dFVF;
    }

    uint32_t D3D_VERTEX_CONVERTER::getSizeOfStructureInBytes() const noexcept
    {
        uint32_t sizeStructVertexInBytes = 0;
        switch (this->FVF)
        {
            case FVF_PROVIDE_BY_ENGINE::FVF_POS:
            {
                sizeStructVertexInBytes = sizeof(VEC3);
            }
            break;
            case FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR:
            {
                sizeStructVertexInBytes = sizeof(VERTEX_NORMAL);
            }
            break;
            case FVF_PROVIDE_BY_ENGINE::FVF_POS_UV:
            {
                sizeStructVertexInBytes = sizeof(VERTEX_UV);
            }
            break;
            case FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV:
            {
                sizeStructVertexInBytes = sizeof(VERTEX_NORMAL_UV);
            }
            break;
        }
        return sizeStructVertexInBytes;
    }
    
    bool BUFFER_GL::loadBuffer(const mbm::VEC3* vertex, // type vertex buffer
        const mbm::VEC3* normal, const mbm::VEC2* uv, const uint32_t sizeOfArrayVertex,
        const uint32_t totalSubsets, const int* vertexStartSubset, const int* vertexCountSubset, const util::INFO_DRAW_MODE* info_draw_mode)
    {
        this->release();
        if (!vertex || !sizeOfArrayVertex || !totalSubsets || !vertexStartSubset || !vertexCountSubset)
            return false;
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        
        IDirect3DDevice9* pd3dDevice = device->specificContextDevice->pd3dDevice;
        this->initializeVertexBufferControl(totalSubsets, sizeOfArrayVertex, vertexStartSubset, vertexCountSubset, info_draw_mode);
        const D3D_VERTEX_CONVERTER d3d_converter(vertex, normal, uv, sizeOfArrayVertex);
        this->bs->FVF = d3d_converter.getFVF();
        const DWORD DFVF = d3d_converter.get3d3FVF();
        this->bs->sizeStructVertexInBytes = d3d_converter.getSizeOfStructureInBytes();
        
        if (FAILED(pd3dDevice->CreateVertexBuffer(//Tamanho Do Vertex Buffer (array * sturtura)
            this->bs->sizeStructVertexInBytes  * sizeOfArrayVertex,
            D3DUSAGE_WRITEONLY, //Usage D3DUSAGE_WRITEONLY
            DFVF,//FVF
            D3DPOOL_MANAGED,//local memory
            &this->bs->pVertexBuffer,//IDirect3DVertexBuffer9
            nullptr)))				//Always null
        {
            ERROR_AT(__LINE__, __FILE__, "failed to create VERTEX BUFFER");
            return false;
        }
        void* pvertex = nullptr;
        if (FAILED(this->bs->pVertexBuffer->Lock(0, 0, (void**)&pvertex, 0)))
        {
            ERROR_AT(__LINE__, __FILE__, "failed to lock VERTEX BUFFER");
            return false;
        }
        d3d_converter.copyTod3dVertexBuffer(pvertex);
        this->bs->pVertexBuffer->Unlock();
        this->totalSubset = totalSubsets;
        return true;
    }

    bool BUFFER_GL::loadBuffer(const VEC3 *vertex, // type index buffer
		const VEC3 *normal,const VEC2 *uv,const uint32_t sizeOfArrayVertex,
		const uint16_t *arrayIndices,const uint32_t totalSubsets,const int *indexStartSubset,
		const int *indexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        release();
        if (!vertex || !sizeOfArrayVertex || !arrayIndices || !totalSubsets || !indexStartSubset || !indexCountSubset)
            return false;
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        IDirect3DDevice9* pd3dDevice = device->specificContextDevice->pd3dDevice;
        this->initializeIndexBufferControl(totalSubsets, sizeOfArrayVertex, indexStartSubset, indexCountSubset, info_draw_mode);
        const D3D_VERTEX_CONVERTER d3d_converter(vertex, normal, uv, sizeOfArrayVertex);
        this->bs->FVF = d3d_converter.getFVF();
        const DWORD DFVF = d3d_converter.get3d3FVF();
        this->bs->sizeStructVertexInBytes = d3d_converter.getSizeOfStructureInBytes();

        if (FAILED(pd3dDevice->CreateVertexBuffer(//Tamanho Do Vertex Buffer (array * sturtura)
            this->bs->sizeStructVertexInBytes * sizeOfArrayVertex,
            D3DUSAGE_WRITEONLY, //Usage D3DUSAGE_WRITEONLY
            DFVF,//FVF
            D3DPOOL_MANAGED,//local memory
            &this->bs->pVertexBuffer,//IDirect3DVertexBuffer9
            nullptr)))				//Always null
        {
            ERROR_AT(__LINE__, __FILE__, "failed to create VERTEX BUFFER");
            return false;
        }
        void* pvertex = nullptr;
        if (FAILED(this->bs->pVertexBuffer->Lock(0, 0, (void**)&pvertex, 0)))
        {
            ERROR_AT(__LINE__, __FILE__, "failed to lock VERTEX BUFFER");
            return false;
        }
        d3d_converter.copyTod3dVertexBuffer(pvertex);
        this->bs->pVertexBuffer->Unlock();

        // index vertex
        UINT sizeIndexBuffer = 0;
        for (int i = 0; i < totalSubsets; ++i)
        {
            sizeIndexBuffer += static_cast<UINT>(indexCountSubset[i]);
        }
        const UINT sizeIndexBufferInBytes = sizeIndexBuffer * sizeof(uint16_t);

        if (FAILED(pd3dDevice->CreateIndexBuffer(sizeIndexBufferInBytes,
            D3DUSAGE_WRITEONLY,
            D3DFMT_INDEX16,
            D3DPOOL_MANAGED,
            &this->bs->pIndexBuffer, nullptr)))
        {
            ERROR_AT(__LINE__, __FILE__, "failed to create INDEX BUFFER");
            return false;
        }

        int16_t* pIndex = nullptr;
        void** ppIndex = reinterpret_cast<void**>(&pIndex);
        if (FAILED(this->bs->pIndexBuffer->Lock(0, 0, ppIndex, 0)))
        {
            ERROR_AT(__LINE__, __FILE__, "failed to lock INDEX BUFFER");
            return false;
        }
        memcpy(pIndex, arrayIndices, sizeIndexBufferInBytes);
        this->bs->pIndexBuffer->Unlock();

        return true;
    }

    bool BUFFER_GL::loadBufferDynamic(uint16_t *arrayIndices, uint32_t totalSubsets, int *indexStartSubset,
                                  int *indexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        release();
        if ( !arrayIndices || !totalSubsets || !indexStartSubset || !indexCountSubset)
            return false;
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        IDirect3DDevice9* pd3dDevice = device->specificContextDevice->pd3dDevice;
        // Find the max vertex count
        // and max size of index buffer
        UINT sizeIndexBuffer = 0;
        for (int i = 0; i < totalSubsets; ++i)
        {
            const int ii = indexStartSubset[i];
            sizeIndexBuffer += static_cast<UINT>(indexCountSubset[i]);
            for (int j = 0; j < indexCountSubset[i]; ++j)
            {
                const uint16_t indexVertex = arrayIndices[ii + j];
                sizeOfArrayVertex = std::max<uint16_t>(indexVertex, sizeOfArrayVertex);
            }
        }
        sizeOfArrayVertex += 1; //because index start from zero base
        const std::vector<VEC3> vertex(sizeOfArrayVertex);
        const std::vector<VEC3> normal(sizeOfArrayVertex);
        const std::vector<VEC2> uv(sizeOfArrayVertex);
        this->initializeIndexBufferControl(totalSubsets, sizeOfArrayVertex, indexStartSubset, indexCountSubset, info_draw_mode);
        const D3D_VERTEX_CONVERTER d3d_converter(vertex.data(), normal.data(), uv.data(), sizeOfArrayVertex);
        this->bs->FVF = d3d_converter.getFVF();
        const DWORD DFVF = d3d_converter.get3d3FVF();
        this->bs->sizeStructVertexInBytes = d3d_converter.getSizeOfStructureInBytes();

        // Dynamic buffers must be created in D3DPOOL_DEFAULT (not MANAGED) and typically with WRITEONLY.
        //•	If you later need to update parts of the dynamic buffer, use D3DLOCK_NOOVERWRITE for partial updates and D3DLOCK_DISCARD when rewriting whole buffer.
        const DWORD vbUsage = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;
        if (FAILED(pd3dDevice->CreateVertexBuffer(//Tamanho Do Vertex Buffer (array * sturtura)
            this->bs->sizeStructVertexInBytes * sizeOfArrayVertex,
            vbUsage,
            DFVF,//FVF
            D3DPOOL_DEFAULT,//local memory
            &this->bs->pVertexBuffer,//IDirect3DVertexBuffer9
            nullptr)))				//Always null
        {
            ERROR_AT(__LINE__, __FILE__, "failed to create VERTEX BUFFER");
            return false;
        }

        const UINT sizeIndexBufferInBytes = sizeIndexBuffer * sizeof(uint16_t);

        if (FAILED(pd3dDevice->CreateIndexBuffer(sizeIndexBufferInBytes,
            D3DUSAGE_WRITEONLY,
            D3DFMT_INDEX16,
            D3DPOOL_MANAGED,
            &this->bs->pIndexBuffer, nullptr)))
        {
            ERROR_AT(__LINE__, __FILE__, "failed to create INDEX BUFFER");
            return false;
        }

        int16_t* pIndex = nullptr;
        void** ppIndex = reinterpret_cast<void**>(&pIndex);
        if (FAILED(this->bs->pIndexBuffer->Lock(0, 0, ppIndex, 0)))
        {
            ERROR_AT(__LINE__, __FILE__, "failed to lock INDEX BUFFER");
            return false;
        }
        memcpy(pIndex, arrayIndices, sizeIndexBufferInBytes);
        this->bs->pIndexBuffer->Unlock();

        return true;
    }

    bool BASE_SHADER::addVar(const char *nameVar, const TYPE_VAR_SHADER typeVar, const float *defaultValue,
                       const uint32_t programObject) // Adiciona uma variavel para o shader indicando o nome da mesma
                                                         // no código e o tipo.
    {
        if (nameVar)
        {
            if (strlen(nameVar) >= 255)
            {
#if defined _DEBUG
                PRINT_IF_DEBUG("max size 255!");
#endif
                return false;
            }
            if (isThereVarIntoLsVars(nameVar))
            {
#if defined _DEBUG
                PRINT_IF_DEBUG("Variable [%s] already exist.", nameVar);
#endif
                return false;
            }
            auto var       = new VAR_SHADER(typeVar);
            var->name      = nameVar;
            #pragma message(REMINDER_TODO "  implement get uniform location");
            
            if (var->handleVar == -1)
            {
#if defined _DEBUG
                PRINT_IF_DEBUG("wasn't found: '%s' into shader GLES! \"", nameVar);
#endif
                delete var;
                return false;
            }
            switch (typeVar)
            {
                case VAR_FLOAT: { var->current[0] = defaultValue[0];
                }
                break;
                case VAR_VECTOR2:
                {
                    for (uint32_t i = 0; i < 2; ++i)
                    {
                        var->current[i] = defaultValue[i];
                    }
                }
                break;
                case VAR_COLOR_RGB:
                case VAR_VECTOR:
                {
                    for (uint32_t i = 0; i < 3; ++i)
                    {
                        var->current[i] = defaultValue[i];
                    }
                }
                break;
                case VAR_COLOR_RGBA:
                {
                    for (uint32_t i = 0; i < 4; ++i)
                    {
                        var->current[i] = defaultValue[i];
                    }
                }
                break;
                default:
                {
                    delete var;
                    return false;
                }
            }
            lsVar.push_back(var);
            return true;
        }
        return false;
    }

    void BASE_SHADER::update(const uint32_t programObject)
    {
        if (programObject == 0)
            return;
        #pragma message(REMINDER_TODO "  implement use program");
        const std::vector<VAR_SHADER *>::size_type s = lsVar.size();
        for (std::vector<VAR_SHADER *>::size_type i = 0; i < s; ++i)
        {
            VAR_SHADER *var = lsVar[i];
            if (var)
            {
                switch (var->typeVar)
                {
                    // Uniform
                    case VAR_FLOAT: {}
                    break;
                    case VAR_VECTOR2:{}
                    break;
                    case VAR_COLOR_RGB:{}
                    case VAR_VECTOR: {};
                    break;
                    case VAR_COLOR_RGBA:{}
                    break;
                    default: {}
                    break;
                }
            }
        }
    }

    SHADER::~SHADER()
    {
        #pragma message(REMINDER_TODO "  implement release shader");
        this->programObject = 0;
    }

    void SHADER::releaseShader()
    {
        this->mvpMatrixHandle = -1;
        this->mvMatrixHandle  = -1;
        this->positionHandle  = -1;
        this->texCoordHandle  = -1;
        this->samplerHandle0  = -1;
        this->samplerHandle1  = -1;
        this->normalHandle    = -1;
        this->pShader         = nullptr;
        this->vShader         = nullptr;
        #pragma message(REMINDER_TODO "  implement delete program");
        this->programObject = 0;
    }

    bool SHADER::compileShader(mbm::BASE_SHADER *ptrPshader, mbm::BASE_SHADER *ptrVshader)
    {
        this->pShader             = ptrPshader;
        this->vShader             = ptrVshader;
        constexpr char *defaultCodePs = "Texture2D sample0 : register(t0);"
                                        "SamplerState samplerState : register(s0);"
                                        ""
                                        "struct PS_INPUT"
                                        "{"
                                        "    float4 position : SV_POSITION;"
                                        "    float2 vTexCoord : TEXCOORD0;"
                                        "};"
                                        ""
                                        "float4 main(PS_INPUT input) : SV_TARGET"
                                        "{"
                                        "    return sample0.Sample(samplerState, input.vTexCoord);"
                                        "}";

        constexpr char *defaultCodeVs =
                                        "cbuffer TransformBuffer : register(b0)"
                                        "{"
                                        "    float4x4 mvpMatrix;"
                                        "};"
                                        ""
                                        "struct VS_INPUT"
                                        "{"
                                        "    float4 aPosition : POSITION;"
                                        "    float2 aTextCoord : TEXCOORD0;"
                                        "    float3 aNormal : NORMAL;"
                                        "};"
                                        ""
                                        "struct VS_OUTPUT"
                                        "{"
                                        "    float4 position : SV_POSITION;"
                                        "    float2 vTexCoord : TEXCOORD0;"
                                        "};"
                                        ""
                                        "VS_OUTPUT main(VS_INPUT input)"
                                        "{"
                                        "    VS_OUTPUT output;"
                                        "    output.position = mul(input.aPosition, mvpMatrix);"
                                        "    output.vTexCoord = input.aTextCoord;"
                                        "    return output;"
                                        "}";
        


        
        constexpr char* mainFunction = "main";
        constexpr char* versionPS = "ps_2_0";
        constexpr char* versionVS = "vs_2_0";
        ID3DXBuffer* bufferPS = nullptr;
        ID3DXBuffer* bufferVS = nullptr;
        ID3DXBuffer* errorBuffer = nullptr;
        ID3DXConstantTable* constantTablePS = nullptr;//Modo de acessar variaveis shader
        ID3DXConstantTable* constantTableVS = nullptr;
        IDirect3DPixelShader9* pd3dPixelShader = nullptr;//Pixel Shader
        IDirect3DVertexShader9* pd3dVertexShader = nullptr;//Vertex Shader
        D3DXHANDLE						mvpMatrixHandle;
        D3DXHANDLE						mvMatrixHandle;
        const char* codePS = ptrPshader ? this->pShader->getCode() : defaultCodePs;
        const char* codeVS = ptrPshader ? this->vShader->getCode() : defaultCodeVs;
        const int sizeOfCodePS = strlen(codePS);
        const int sizeOfCodeVS = strlen(codeVS);
#if defined _DEBUG
        constexpr DWORD flag = D3DXSHADER_DEBUG;
#else
        constexpr DWORD flag = D3DXSHADER_SKIPVALIDATION;
#endif
        if (FAILED(D3DXCompileShader(codePS, sizeOfCodePS, 0, 0, mainFunction, versionPS, flag, &bufferPS, &errorBuffer, &constantTablePS)))
        {
            if (errorBuffer)
            {
                ERROR_AT(__LINE__,__FILE__, "error on load pixel shader:\n [%s]",static_cast<const char*>(errorBuffer->GetBufferPointer()));
                errorBuffer->Release();
                pShader = NULL;
                return false;
            }
        }
        if (FAILED(D3DXCompileShader(codeVS, sizeOfCodeVS, 0, 0, mainFunction, versionVS, flag, &bufferVS, &errorBuffer, &constantTableVS)))
        {
            if (errorBuffer)
            {
                ERROR_AT(__LINE__, __FILE__, "error on load vertex shader:\n [%s]", static_cast<const char*>(errorBuffer->GetBufferPointer()));
                errorBuffer->Release();
                pShader = NULL;
                return false;
            }
        }
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        IDirect3DDevice9* pd3dDevice = device->specificContextDevice->pd3dDevice;
        if (FAILED(pd3dDevice->CreatePixelShader(static_cast<DWORD*>(bufferPS->GetBufferPointer()), &pd3dPixelShader)))
        {
            ERROR_AT(__LINE__, __FILE__, "error on create pixel shader:\n [%s]", static_cast<const char*>(errorBuffer->GetBufferPointer()));
            return false;
        }
        if (FAILED(pd3dDevice->CreateVertexShader(static_cast<DWORD*>(bufferVS->GetBufferPointer()), &pd3dVertexShader)))
        {
            ERROR_AT(__LINE__, __FILE__, "error on create vertex shader:\n [%s]", static_cast<const char*>(errorBuffer->GetBufferPointer()));
            return false;
        }
        if (constantTablePS)
        {
            constantTablePS->SetDefaults(pd3dDevice);
        }
        if (constantTableVS)
        {
            constantTableVS->SetDefaults(pd3dDevice);
        }

        //GLint aPosition = GLGetAttribLocation(programObject, "aPosition");
        //this->positionHandle = static_cast<GLint>(aPosition);
        //this->mvpMatrixHandle = GLGetUniformLocation(programObject, "mvpMatrix");
        //this->mvMatrixHandle = GLGetUniformLocation(programObject, "mvMatrix");
        //GLint aTextCoord = GLGetAttribLocation(programObject, "aTextCoord");
        //this->texCoordHandle = static_cast<GLint>(aTextCoord);
        //this->samplerHandle0 = GLGetUniformLocation(programObject, "sample0");
        //this->samplerHandle1 = GLGetUniformLocation(programObject, "sample1");
        //GLint aNormal = GLGetAttribLocation(programObject, "aNormal")
        //this->normalHandle = static_cast<GLint>(aNormal);
        
        
        //TODO: we do not need to loadShaderProgram (remove from common) 
        /*if (this->programObject)
        {
            PRINT_IF_DEBUG("programObject already has a value [%d]",this->programObject);
            return true;
        }
        if (this->pShader == nullptr && this->vShader == nullptr)
        {
            if (!this->loadShaderProgram(defaultCodeVs, defaultCodePs))
                return false;
        }
        else if (this->pShader == nullptr && this->vShader)
        {
            if (!this->loadShaderProgram(this->vShader->getCode(), defaultCodePs))
                return false;
        }
        else if (this->pShader && this->vShader == nullptr)
        {
            if (!this->loadShaderProgram(defaultCodeVs, this->pShader->getCode()))
                return false;
        }
        else if (this->pShader && this->vShader)
        {
            if (!this->loadShaderProgram(this->vShader->getCode(), this->pShader->getCode()))
                return false;
        }*/
        #pragma message(REMINDER_TODO "  implement get attrib and uniform locations");
        return true;
    }


    bool SHADER::render(const BUFFER_GL *pBufferId) const
    {
        if (pBufferId->bs == nullptr || pBufferId->bs->pVertexBuffer == nullptr)
        {
            ERROR_AT(__LINE__, __FILE__, "IDirect3DVertexBuffer9 is null, you must load the object first");
            return false;
        };

        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        IDirect3DDevice9* pd3dDevice = device->specificContextDevice->pd3dDevice;

        const D3DMATRIX* modelView = reinterpret_cast<const D3DMATRIX*>(&SHADER::modelView);
        if (FAILED(pd3dDevice->SetTransform(D3DTS_WORLD, modelView)))
        {
            ERROR_AT(__LINE__, __FILE__, "Failed to set SetTransform D3DTS_WORLD for modelView");
            return false;
        };

        // There is no direct equivalent to the OpenGL constant GL_FRONT in DirectX 9, as the two APIs handle face culling and rendering differently.
        // In OpenGL, GL_FRONT is used to specify the front - facing polygons for operations like culling or lighting, 
        // but DirectX 9 does not use this specific constant or naming convention.

        // Instead, DirectX 9 uses the D3DCULL enumeration to define which polygon faces to cull during rendering.
        // The equivalent behavior to OpenGL's GL_FRONT culling can be achieved by setting the culling mode to D3DCULL_NONE (to render both front and back faces), 
        // D3DCULL_CCW (counter-clockwise, typically front-facing), or D3DCULL_CW (clockwise, typically back-facing), depending on the desired rendering behavior.
        // TODO: use the correct CULLMODE

        switch (pBufferId->mode_cull_face)
        {
            case util::CULL_MODE::CULL_FRONT:
            {
                pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
            }
            break;
            case util::CULL_MODE::CULL_BACK:
            {
                pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_CCW);
            }
            break;
            case util::CULL_MODE::CULL_FRONT_AND_BACK:
            {
                pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
            }
            break;
        }
        

        if (pBufferId->isIndexBuffer()) // Index buffer
        {
            // When converting a legacy application to Direct3D 9, 
            // you must add a call to either IDirect3DDevice9::SetFVF to use the fixed function pipeline, 
            // or IDirect3DDevice9::SetVertexDeclaration to use a vertex shader before you make any Draw calls.
            // pd3dDevice->SetFVF(0);//Maybe not needed to disable
            if (FAILED(pd3dDevice->SetVertexDeclaration(device->specificContextDevice->getFVF(pBufferId->bs->FVF))))
            {
                ERROR_AT(__LINE__, __FILE__, "SetVertexDeclaration failed");
                return false;
            };

            if (FAILED(pd3dDevice->SetStreamSource(0,//Stream if have multiples
                pBufferId->bs->pVertexBuffer,//Pointer from IDirect3DVertexBuffer9 created
                0,		//Position in bytes of start stream
                pBufferId->bs->sizeStructVertexInBytes)))//Size of structure vertex
            {
                ERROR_AT(__LINE__, __FILE__, "SetStreamSource failed");
                return false;
            };

            if (FAILED(pd3dDevice->SetIndices(pBufferId->bs->pIndexBuffer)))
            {
                ERROR_AT(__LINE__, __FILE__, "Failed to set index vertex");
                return false;
            }

            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                TEXTURE* texture0 = pBufferId->getTextureByStage(0, i);
                if (texture0)
                {
                    IDirect3DTexture9* pp3DTexture9 = static_cast<IDirect3DTexture9*>(texture0->ptrTexture);
                    pd3dDevice->SetTexture(0, pp3DTexture9);
                }
                else
                {
                    pd3dDevice->SetTexture(0, nullptr);
                }

                TEXTURE* texture1 = pBufferId->getTextureByStage(1, 0);

                if (texture1)
                {
                    IDirect3DTexture9* pp3DTexture9 = static_cast<IDirect3DTexture9*>(texture1->ptrTexture);
                    pd3dDevice->SetTexture(1, pp3DTexture9);
                }
                else
                {
                    pd3dDevice->SetTexture(1, nullptr);
                }

                //https://learn.microsoft.com/en-us/windows/win32/direct3d9/rendering-from-vertex-and-index-buffers

                switch (pBufferId->mode_draw)
                {
                    case util::MODE_DRAW_POINTS:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_POINTS");
                    };
                    break;
                    case util::MODE_DRAW_LINES:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_LINES");
                    };
                    break;
                    case util::MODE_DRAW_LINE_LOOP:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_LINE_LOOP");
                    };
                    break;
                    case util::MODE_DRAW_LINE_STRIP:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_LINE_STRIP");
                    };
                    break;
                    case util::MODE_DRAW_TRIANGLES:
                    {
                        const UINT countTriangle = pBufferId->indexCountIB[i] / 3;
                        const UINT numVertices   = pBufferId->sizeOfArrayVertex;
                        const UINT vertexStartVB = 0;
                        constexpr UINT MinVertexIndex = 0;
                        
                        if (FAILED(pd3dDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
                                                                    vertexStartVB, 
                                                                    MinVertexIndex,
                                                                    numVertices,
                                                                    pBufferId->indexStartIB[i],
                                                                    countTriangle)))
                        {
                            ERROR_AT(__LINE__, __FILE__, "Failed to draw indexed primitive");
                            return false;
                        }
                    };
                    break;
                    case util::MODE_DRAW_TRIANGLE_STRIP:
                    {
                        const UINT countTriangle = pBufferId->indexCountIB[i] - 2;
                        if (FAILED(pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, pBufferId->indexStartIB[i], countTriangle)))
                            return false;
                    };
                    break;
                    case util::MODE_DRAW_TRIANGLE_FAN:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_TRIANGLE_FAN");
                    };
                    break;
                    default: 
                    {
                        ERROR_AT(__LINE__, __FILE__, "Wrong mode draw");
                        return false;
                    }
                }
            }
        }
        else // Vertex buffer
        {
            // When converting a legacy application to Direct3D 9, 
            // you must add a call to either IDirect3DDevice9::SetFVF to use the fixed function pipeline, 
            // or IDirect3DDevice9::SetVertexDeclaration to use a vertex shader before you make any Draw calls.
            // pd3dDevice->SetFVF(0);//Maybe not needed to disable
            pd3dDevice->SetVertexDeclaration(device->specificContextDevice->getFVF(pBufferId->bs->FVF));
            if (FAILED(pd3dDevice->SetStreamSource(0,//Stream Se houver Multiplos Streams
                pBufferId->bs->pVertexBuffer,//Ponteiro De Nosso Objeto Criado
                0,		//Posicao Em Bytes Do inicio  Do Stream Atual
                pBufferId->bs->sizeStructVertexInBytes)))//Tamanho Da Estrutura De Nosso Vertex
                return false;

            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                TEXTURE* texture0 = pBufferId->getTextureByStage(0, i);
                if (texture0)
                {
                    IDirect3DTexture9* pp3DTexture9 = static_cast<IDirect3DTexture9*>(texture0->ptrTexture);
                    pd3dDevice->SetTexture(0, pp3DTexture9);
                }
                else
                {
                    pd3dDevice->SetTexture(0, nullptr);
                }

                TEXTURE* texture1 = pBufferId->getTextureByStage(1, 0);

                if (texture1)
                {
                    IDirect3DTexture9* pp3DTexture9 = static_cast<IDirect3DTexture9*>(texture1->ptrTexture);
                    pd3dDevice->SetTexture(1, pp3DTexture9);
                }
                else
                {
                    pd3dDevice->SetTexture(1, nullptr);
                }
                    
                switch (pBufferId->mode_draw)
                {
                    case util::MODE_DRAW_POINTS:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_POINTS");
                    };
                    break;
                    case util::MODE_DRAW_LINES:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_LINES");
                    };
                    break;
                    case util::MODE_DRAW_LINE_LOOP:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_LINE_LOOP");
                    };
                    break;
                    case util::MODE_DRAW_LINE_STRIP:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_LINE_STRIP");
                    };
                    break;
                    case util::MODE_DRAW_TRIANGLES:
                    {
                        const UINT countTriangle = pBufferId->vertexCountVB[i] - 2;
                        if (FAILED(pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, pBufferId->vertexStartVB[i], countTriangle)))
                            return false;
                    };
                    break;
                    case util::MODE_DRAW_TRIANGLE_STRIP:
                    {
                        const UINT countTriangle = pBufferId->vertexCountVB[i] - 2;
                        if (FAILED(pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, pBufferId->vertexStartVB[i], countTriangle)))
                            return false;
                    };
                    break;
                    case util::MODE_DRAW_TRIANGLE_FAN:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_TRIANGLE_FAN");
                    };
                    break;
                    default: 
                    {
                        ERROR_AT(__LINE__, __FILE__, "Wrong mode draw");
                        return false;
                    }
                }
            }
        }
        return true;
    }

    bool SHADER::renderDynamic(const BUFFER_GL *pBufferId,const VEC3 *vertex,const VEC3 *normal,const VEC2 *uv) const
    {
		if (pBufferId && vertex && pBufferId->bs && pBufferId->bs->pVertexBuffer && pBufferId->sizeOfArrayVertex > 0)
        {
            const D3D_VERTEX_CONVERTER d3d_converter(vertex, normal, uv, pBufferId->sizeOfArrayVertex);
            void* pvertex = nullptr;
            if (FAILED(pBufferId->bs->pVertexBuffer->Lock(0, 0, (void**)&pvertex, 0)))
            {
                ERROR_AT(__LINE__, __FILE__, "failed to lock VERTEX BUFFER");
                return false;
            }
            d3d_converter.copyTod3dVertexBuffer(pvertex);
            pBufferId->bs->pVertexBuffer->Unlock();

            return render(pBufferId);
        }
        return false;
    }

    uint32_t SHADER::compileCodeShader(const unsigned int type, const char *shaderSrc)
    {
        uint32_t shader=0;
        int          compiled=0;
        // Create the shader object
        #pragma message(REMINDER_TODO "  implement create shader");
        if (shader == 0)
        {
            PRINT_IF_DEBUG("GLCreateShader returned 0");
            return 0;
        }
        //TODO: Load the shader source
        //TODO: Compile the shader
        //TODO: Check the compile status
        if (!compiled)
        {
            PRINT_IF_DEBUG("failed on compile shader [%s]",shaderSrc ? shaderSrc : "null");
            //TODO: implement get shader info log
            // Delete the shader object
            return 0;
        }
        return shader;
    }

    uint32_t SHADER::loadShaderProgram(const char *vertShaderSrc, const char *fragShaderSrc)
    {
        uint32_t vertexShader=0;
        uint32_t fragmentShader=0;
        int          linked=0;
        if (this->programObject)
        {
            PRINT_IF_DEBUG("programObject already exists");
            return programObject;
        }
        #pragma message(REMINDER_TODO "  Load the vertex/fragment shaders");
        if (vertexShader == 0)
        {
            PRINT_IF_DEBUG("vertexShader == 0");
            return 0;
        }
        //fragmentShader = compileCodeShader(GL_FRAGMENT_SHADER, fragShaderSrc);
        if (fragmentShader == 0)
        {
            PRINT_IF_DEBUG("fragmentShader == 0");
            return 0;
        }
        //TODO: implement create program
        if (programObject == 0)
        {
            PRINT_IF_DEBUG("Failed to create programObject");
            return 0;
        }
        //TODO: Link the program
        //TODO: Check the link status
        if (!linked)
        {
            PRINT_IF_DEBUG("linked status failed");
            //TODO: implement get program info log
            // Delete the program object
            programObject = 0;
            return 0;
        }
        #pragma message(REMINDER_TODO "  Free up no longer needed shader resources");
        return programObject;
    }
}

#endif // USE_DIRECTX9