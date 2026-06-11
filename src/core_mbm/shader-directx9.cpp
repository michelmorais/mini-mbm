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

#include <shader.h>
#include <util-interface.h>
#include <shader-var-cfg.h>
#include <device.h>
#include <specific-directx9.h>
#include <header-mesh.h>
#include <draw-compatibility.h>
#include <texture-manager.h>
#include <particle-control.h>
#include <shader-resource.h>
#include <unordered_set>

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
        fvf(FVF_PROVIDE_BY_ENGINE::FVF_POS_UV),
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
        if(bs)
        {
            // Deleting a void* pointer directly in C++ is undefined behavior and should be avoided. 
            delete static_cast<BUFFER_SPECIFIC*>(bs);
        }
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
        pos(_pos), normal(_normal), uv(_uv), size_array(_size_array)
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
    
    bool BUFFER_GL::loadBuffer(const VEC3* vertex,
                               const VEC3* normal,
                               const VEC2* uv,
                               const uint32_t sizeOfArrayVertex,
                               const uint32_t totalSubsets,
                               const int* vertexStartSubset,
                               const int* vertexCountSubset,
                               const util::INFO_DRAW_MODE* info_draw_mode,
                               const bool isDynamic)
    {
        this->release();
        if (!vertex || !sizeOfArrayVertex || !totalSubsets || !vertexStartSubset || !vertexCountSubset)
            return false;
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        
        IDirect3DDevice9* pd3dDevice = device->getSpecificContextDevice()->pd3dDevice;
        this->initializeVertexBufferControl(totalSubsets, sizeOfArrayVertex, vertexStartSubset, vertexCountSubset, info_draw_mode);
        const D3D_VERTEX_CONVERTER d3d_converter(vertex, normal, uv, sizeOfArrayVertex);
        this->fvf = this->bs->FVF = d3d_converter.getFVF();
        const DWORD DFVF = d3d_converter.get3d3FVF();
        this->bs->sizeStructVertexInBytes = d3d_converter.getSizeOfStructureInBytes();
        
        const DWORD bufferUsage = isDynamic ? (D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY) : D3DUSAGE_WRITEONLY;
        const D3DPOOL d3dPoll = isDynamic ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
        if (FAILED(pd3dDevice->CreateVertexBuffer(//Tamanho Do Vertex Buffer (array * sturtura)
            this->bs->sizeStructVertexInBytes  * sizeOfArrayVertex,
            bufferUsage, //Usage
            DFVF,//FVF
            d3dPoll,//memory
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
        IDirect3DDevice9* pd3dDevice = device->getSpecificContextDevice()->pd3dDevice;
        this->initializeIndexBufferControl(totalSubsets, sizeOfArrayVertex, indexStartSubset, indexCountSubset, info_draw_mode);
        const D3D_VERTEX_CONVERTER d3d_converter(vertex, normal, uv, sizeOfArrayVertex);
        this->fvf = this->bs->FVF = d3d_converter.getFVF();
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
        for (uint32_t i = 0; i < totalSubsets; ++i)
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

    bool BUFFER_GL::loadBufferDynamic(  const uint16_t* arrayIndices,
                                        const unsigned int totalSubsets,
                                        const int* indexStartSubset,
                                        const int* indexCountSubset,
                                        const bool hasNormal,
                                        const bool hasUv,
                                        const util::INFO_DRAW_MODE* info_draw_mode)
    {
        release();
        if ( !arrayIndices || !totalSubsets || !indexStartSubset || !indexCountSubset)
            return false;
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        IDirect3DDevice9* pd3dDevice = device->getSpecificContextDevice()->pd3dDevice;
        // Find the max vertex count
        // and max size of index buffer
        UINT sizeIndexBuffer = 0;
        for (uint32_t i = 0; i < totalSubsets; ++i)
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
        const std::vector<VEC3> normal(hasNormal ? sizeOfArrayVertex : 0);
        const std::vector<VEC2> uv(hasUv ? sizeOfArrayVertex : 0);
        this->initializeIndexBufferControl(totalSubsets, sizeOfArrayVertex, indexStartSubset, indexCountSubset, info_draw_mode);
        const D3D_VERTEX_CONVERTER d3d_converter(vertex.data(), hasNormal ? normal.data() : nullptr, hasUv ? uv.data() : nullptr, sizeOfArrayVertex);
        this->fvf = this->bs->FVF = d3d_converter.getFVF();
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

    bool BUFFER_GL::loadParticleBuffer()
    {
        constexpr uint32_t totalSubset = 1;
        constexpr uint16_t arrayIndices[6] = { 0, 1, 2, 2, 1, 3 };
        constexpr int indexStartSubset = 0;
        constexpr int indexCountSubset = sizeof(arrayIndices) / sizeof(uint16_t);
        constexpr bool hasNormal       = false;
        constexpr bool hasUv           = true;
        return loadBufferDynamic(arrayIndices, totalSubset, &indexStartSubset, &indexCountSubset, hasNormal, hasUv, nullptr);
    }

    bool BUFFER_GL::updateDynamic(const VEC3* vertex,
                                  const VEC3* normal,
                                  const VEC2* uv,
                                  const int* vertexStartSubset,
                                  const int* vertexCountSubset)// update when dynamic
    {
        
        for (uint32_t i = 0; i < this->totalSubset; ++i)
        {
            const uint32_t vertexStart = vertexStartSubset[i];
            const uint32_t vertexCount = vertexCountSubset[i];
            if (vertexCount > this->sizeOfArrayVertex)
            {
                return false;
            }
            if ((vertexStart + vertexCount) > this->sizeOfArrayVertex)
            {
                return false;
            }
            const mbm::VEC3* pVertexStart = &vertex[vertexStart];
            const mbm::VEC3* pNormalStart = normal ? &normal[vertexStart] : nullptr;
            const mbm::VEC2* pUvStart     = uv     ? &uv[vertexStart]:      nullptr;
            if (this->initializedIndexBuffer)
            {
                // TODO: for index buffer
                ERROR_AT(__LINE__, __FILE__, "TODO: Update vertex not implemented for index buffer");
                return false;
            }
            else
            {
                const D3D_VERTEX_CONVERTER d3d_converter(pVertexStart, normal, uv, vertexCount);
                void* pvertex = nullptr;
                // Dynamic buffers must be created in D3DPOOL_DEFAULT (not MANAGED) and typically with WRITEONLY.
                // If you later need to update parts of the dynamic buffer, use D3DLOCK_NOOVERWRITE for partial updates and D3DLOCK_DISCARD when rewriting whole buffer.
                if (FAILED(this->bs->pVertexBuffer->Lock(0, 0, (void**)&pvertex, D3DLOCK_DISCARD)))
                {
                    ERROR_AT(__LINE__, __FILE__, "failed to lock VERTEX BUFFER");
                    return false;
                }
                d3d_converter.copyTod3dVertexBuffer(pvertex);
                this->bs->pVertexBuffer->Unlock();
            }
        }
        return true;
    }

    bool BASE_SHADER::addVar(const char *nameVar, const TYPE_VAR_SHADER typeVar, const float *defaultValue,
                       void* ptrShaderSpecific, const bool isPS) // Adiciona uma variavel para o shader indicando o nome da mesma
                                                         // no código e o tipo.
    {
        if (nameVar)
        {
            if (strlen(nameVar) >= 255)
            {
                ERROR_LOG("max size 255!");
                return false;
            }
            if (isThereVarIntoLsVars(nameVar))
            {
                ERROR_LOG("Variable [%s] already exist.", nameVar);
                return false;
            }
            auto var       = new VAR_SHADER(std::string(nameVar), typeVar, isPS);
            
            if (var->ptrHandleVar == nullptr)
            {
                ERROR_LOG("Not initialized ptrHandleVar: '%s' into shader HLSL! \"", nameVar);
                delete var;
                return true;
            }
            const D3D_PS_VS* d3dPsVs = static_cast<const D3D_PS_VS*>(ptrShaderSpecific);
            D3DXHANDLE* pHandleVar = static_cast<D3DXHANDLE*>(var->ptrHandleVar);
            if (isPS)
            {
                *pHandleVar = d3dPsVs->constantTablePS->GetConstantByName(nullptr, nameVar);
            }
            else
            {
                *pHandleVar = d3dPsVs->constantTableVS->GetConstantByName(nullptr, nameVar);
            }
            
            if (*pHandleVar == nullptr)
            {
                ERROR_LOG("Not found variable: '%s' into shader HLSL! \"", nameVar);
                delete var;
                return true;
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

    void BASE_SHADER::update(void * ptrShaderSpecific) const
    {
        const D3D_PS_VS* d3dPsVs = static_cast<const D3D_PS_VS*>(ptrShaderSpecific);
        if (d3dPsVs->pd3dPixelShader == nullptr && d3dPsVs->pd3dVertexShader == nullptr) // simple check
            return;
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        IDirect3DDevice9* pd3dDevice = device->getSpecificContextDevice()->pd3dDevice;
        const std::vector<VAR_SHADER *>::size_type s = lsVar.size();
        for (std::vector<VAR_SHADER *>::size_type i = 0; i < s; ++i)
        {
            VAR_SHADER *var = lsVar[i];
            if (var)
            {
                D3DXHANDLE* pHandleVar            = static_cast<D3DXHANDLE*>(var->ptrHandleVar);
                ID3DXConstantTable* constantTable = var->isPS ? d3dPsVs->constantTablePS : d3dPsVs->constantTableVS;
                switch (var->typeVar)
                {
                    // Uniform
                    case VAR_FLOAT: { constantTable->SetFloat(pd3dDevice, *pHandleVar, var->current[0]); }
                    break;
                    case VAR_VECTOR2:{ constantTable->SetFloatArray(pd3dDevice, *pHandleVar, var->current, 2); }
                    break;
                    case VAR_COLOR_RGB:
                    case VAR_VECTOR: { constantTable->SetFloatArray(pd3dDevice, *pHandleVar, var->current, 3); }
                    break;
                    case VAR_COLOR_RGBA: { constantTable->SetFloatArray(pd3dDevice, *pHandleVar, var->current, 4); }
                    break;
                    default: {}
                    break;
                }
            }
        }
    }

    D3D_PS_VS::D3D_PS_VS() noexcept :
        pd3dPixelShader(nullptr),
        pd3dVertexShader(nullptr),
        constantTablePS(nullptr),
        constantTableVS(nullptr),
        mvpMatrixHandle(nullptr),
        mvMatrixHandle(nullptr),
        samplerHandle0(nullptr),
        samplerHandle1(nullptr)
    {
    }

    D3D_PS_VS::~D3D_PS_VS()
    {
        release();
    }

    void D3D_PS_VS::release() noexcept
    {
        if (pd3dPixelShader)
        {
            pd3dPixelShader->Release();
            pd3dPixelShader = nullptr;
        }
        if (pd3dVertexShader)
        {
            pd3dVertexShader->Release();
            pd3dVertexShader = nullptr;
        }
        if (constantTablePS)
        {
            constantTablePS->Release();
            constantTablePS = nullptr;
        }
        if (constantTableVS)
        {
            constantTableVS->Release();
            constantTableVS = nullptr;
        }

        mvpMatrixHandle = nullptr;
        mvMatrixHandle  = nullptr;
        samplerHandle0  = nullptr;
        samplerHandle1  = nullptr;
    }

    SHADER::SHADER() : 
        ptrShaderSpecific(new D3D_PS_VS()),
        pShader(nullptr),
        vShader(nullptr)
    {
    }

    SHADER::~SHADER()
    {
        // Deleting a void* pointer directly in C++ is undefined behavior and should be avoided. 
        delete static_cast<D3D_PS_VS*>(ptrShaderSpecific);
    }

    void SHADER::onRestore()
    {
        static_cast<D3D_PS_VS*>(ptrShaderSpecific)->release();
        this->pShader = nullptr;
        this->vShader = nullptr;
    }

    void SHADER::releaseShader()
    {
        static_cast<D3D_PS_VS*>(ptrShaderSpecific)->release();
        this->pShader         = nullptr;
        this->vShader         = nullptr;
    }

    bool SHADER::compileShader(mbm::BASE_SHADER *ptrPshader, mbm::BASE_SHADER *ptrVshader, mbm::FVF_PROVIDE_BY_ENGINE fvf)
    {
        if (fvf == FVF_PROVIDE_BY_ENGINE::FVF_NONE)
            return false;
        this->pShader             = ptrPshader;
        this->vShader             = ptrVshader;
        const bool hasNormal = (fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR || fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
        const bool hasUV = (fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_UV || fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);

        std::string defaultCodePs;
        if (hasUV)
        {
            defaultCodePs = "sampler2D sample0 : register(s0);"
                "float4 main(float2 texCoord : TEXCOORD0) : COLOR"
                "{ return tex2D(sample0, texCoord); }";
        }
        else
        {
            defaultCodePs = "float4 main() : COLOR"
                "{ return float4(1,1,1,1); }";
        }

        std::string defaultCodeVs = "float4x4 mvpMatrix : register(c0);"
            "struct VS_INPUT { float4 position : POSITION;";
        if (hasNormal) defaultCodeVs += " float3 normal : NORMAL;";
        if (hasUV) defaultCodeVs += " float2 texCoord : TEXCOORD0;";
        defaultCodeVs += " };"
            "struct VS_OUTPUT { float4 position : POSITION;";
        if (hasUV) defaultCodeVs += " float2 texCoord : TEXCOORD0;";
        defaultCodeVs += " };"
            "VS_OUTPUT main(VS_INPUT input)"
            "{ VS_OUTPUT output; output.position = mul(input.position, mvpMatrix);";
        if (hasUV) defaultCodeVs += " output.texCoord = input.texCoord;";
        defaultCodeVs += " return output; }";

        constexpr const char* mainFunction = "main";
        const char* versionPS        = getPSVersion();
        const char* versionVS       = getVSVersion();
        ID3DXBuffer* bufferPS       = nullptr;
        ID3DXBuffer* bufferVS       = nullptr;
        ID3DXBuffer* errorBuffer    = nullptr;

        D3D_PS_VS* d3dPsVs = static_cast<D3D_PS_VS*>(ptrShaderSpecific);

        const char* codePS = ptrPshader ? this->pShader->getCode() : defaultCodePs.c_str();
        const char* codeVS = ptrVshader ? this->vShader->getCode() : defaultCodeVs.c_str();
        const int sizeOfCodePS = strlen(codePS);
        const int sizeOfCodeVS = strlen(codeVS);
#if defined _DEBUG
        constexpr DWORD flag = D3DXSHADER_DEBUG;
#else
        constexpr DWORD flag = D3DXSHADER_SKIPVALIDATION;
#endif
        if (ptrPshader || (ptrPshader == nullptr && useDefaultPSWhenNoShader()))
        {
            if (FAILED(D3DXCompileShader(codePS, sizeOfCodePS, 0, 0, mainFunction, versionPS, flag, &bufferPS, &errorBuffer, &d3dPsVs->constantTablePS)))
            {
                if (errorBuffer)
                {
                    ERROR_AT(__LINE__, __FILE__, "error on load pixel shader:\n [%s]", static_cast<const char*>(errorBuffer->GetBufferPointer()));
                    errorBuffer->Release();
                    pShader = NULL;
                    return false;
                }
            }
        }
        if (ptrVshader || (ptrVshader == nullptr && useDefaultVSWhenNoShader()))
        {
            if (FAILED(D3DXCompileShader(codeVS, sizeOfCodeVS, 0, 0, mainFunction, versionVS, flag, &bufferVS, &errorBuffer, &d3dPsVs->constantTableVS)))
            {
                if (errorBuffer)
                {
                    ERROR_AT(__LINE__, __FILE__, "error on load vertex shader:\n [%s]", static_cast<const char*>(errorBuffer->GetBufferPointer()));
                    errorBuffer->Release();
                    pShader = NULL;
                    return false;
                }
            }
        }
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        IDirect3DDevice9* pd3dDevice = device->getSpecificContextDevice()->pd3dDevice;

        if (d3dPsVs->pd3dPixelShader)
        {
            d3dPsVs->pd3dPixelShader->Release();
            d3dPsVs->pd3dPixelShader = nullptr;
        }
        if (d3dPsVs->pd3dVertexShader)
        {
            d3dPsVs->pd3dVertexShader->Release();
            d3dPsVs->pd3dVertexShader = nullptr;
        }

        if (bufferPS && FAILED(pd3dDevice->CreatePixelShader(static_cast<DWORD*>(bufferPS->GetBufferPointer()), &d3dPsVs->pd3dPixelShader)))
        {
            ERROR_AT(__LINE__, __FILE__, "error on create pixel shader:\n [%s]", static_cast<const char*>(errorBuffer->GetBufferPointer()));
            return false;
        }
        if (bufferVS && FAILED(pd3dDevice->CreateVertexShader(static_cast<DWORD*>(bufferVS->GetBufferPointer()), &d3dPsVs->pd3dVertexShader)))
        {
            ERROR_AT(__LINE__, __FILE__, "error on create vertex shader:\n [%s]", static_cast<const char*>(errorBuffer->GetBufferPointer()));
            return false;
        }
        if (d3dPsVs->constantTablePS)
        {
            d3dPsVs->constantTablePS->SetDefaults(pd3dDevice);
            d3dPsVs->samplerHandle0 = d3dPsVs->constantTablePS->GetConstantByName(nullptr, "sample0");
            d3dPsVs->samplerHandle1 = d3dPsVs->constantTablePS->GetConstantByName(nullptr, "sample1");

            //D3DXCONSTANT_DESC desc;
            //UINT count = 1;
            //if (d3dPsVs->samplerHandle0 && SUCCEEDED(constantTablePS->GetConstantDesc(d3dPsVs->samplerHandle0, &desc, &count)))
            //{
            //    int pixelSamplerRegister0 = static_cast<int>(desc.RegisterIndex); // store as member int
            //}
            // set texture to sampler0 (pixel shader sampler index) (later)
            //if (this->pixelSamplerRegister0 >= 0)
            //    pd3dDevice->SetTexture(this->pixelSamplerRegister0, myTexture);

        }
        if (d3dPsVs->constantTableVS)
        {
            d3dPsVs->constantTableVS->SetDefaults(pd3dDevice);
            d3dPsVs->mvpMatrixHandle = d3dPsVs->constantTableVS->GetConstantByName(nullptr, "mvpMatrix");
            d3dPsVs->mvMatrixHandle  = d3dPsVs->constantTableVS->GetConstantByName(nullptr, "mvMatrix");

            //D3DXCONSTANT_DESC desc;
            //UINT count = 1;
            //if (d3dPsVs->mvpMatrixHandle && SUCCEEDED(d3dPsVs->constantTableVS->GetConstantDesc(d3dPsVs->mvpMatrixHandle, &desc, &count)))
            //{
            //    int vertexMvpRegister = static_cast<int>(desc.RegisterIndex);
            //    int vertexMvpRegisterCount = static_cast<int>(desc.RegisterCount); // number of float4 registers
            //
            //    // set mvp matrix for vertex shader (uses float4 registers)
            //    if (vertexMvpRegister >= 0 && vertexMvpRegisterCount > 0)
            //    {
            //        // assume 'mat' is a float[4*vertexMvpRegisterCount] or D3DXMATRIX compatible
            //        pd3dDevice->SetVertexShaderConstantF(vertexMvpRegister, reinterpret_cast<const float*>(&SHADER::mvpMatrix), vertexMvpRegisterCount);
            //    }
            //}
        }
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
        IDirect3DDevice9* pd3dDevice = device->getSpecificContextDevice()->pd3dDevice;

        const D3DMATRIX* modelView = reinterpret_cast<const D3DMATRIX*>(&SHADER::modelView);
        if (FAILED(pd3dDevice->SetTransform(D3DTS_WORLD, modelView)))
        {
            ERROR_AT(__LINE__, __FILE__, "Failed to set SetTransform D3DTS_WORLD for modelView");
            return false;
        };

        D3D_PS_VS* d3dPsVs = static_cast<D3D_PS_VS*>(ptrShaderSpecific);

        if (d3dPsVs->pd3dPixelShader)
        {
            if (FAILED(pd3dDevice->SetPixelShader(d3dPsVs->pd3dPixelShader)))
            {
                ERROR_AT(__LINE__, __FILE__, "Failed to set Pixel Shader");
                return false;
            }
        }
        else
        {
            if (FAILED(pd3dDevice->SetPixelShader(nullptr)))
            {
                ERROR_AT(__LINE__, __FILE__, "Failed to set Pixel Shader to null");
                return false;
            }
        }

        if (d3dPsVs->pd3dVertexShader)
        {
            if (FAILED(pd3dDevice->SetVertexShader(d3dPsVs->pd3dVertexShader)))
            {
                ERROR_AT(__LINE__, __FILE__, "Failed to set Vertex Shader");
                return false;
            }
            const D3DXMATRIX* pMvpMatrix    = reinterpret_cast<const D3DXMATRIX*>(&SHADER::mvpMatrix);
            const D3DXMATRIX* pMatrixHandle = reinterpret_cast<const D3DXMATRIX*>(&SHADER::modelView);

            d3dPsVs->constantTableVS->SetMatrix(pd3dDevice, d3dPsVs->mvpMatrixHandle, pMvpMatrix);
            d3dPsVs->constantTableVS->SetMatrix(pd3dDevice, d3dPsVs->mvMatrixHandle, pMatrixHandle);
        }
        else
        {
            if (FAILED(pd3dDevice->SetVertexShader(nullptr)))
            {
                ERROR_AT(__LINE__, __FILE__, "Failed to set Vertex Shader to null");
                return false;
            }
        }
        #if defined DEBUG_SHADER_D3D_MINIMIZE_ERROR
        // You might have problem with shader, untill now the flow works fine, but in case suspicios if the constants are lost..
        // Re-apply PS/VS constants after shaders are bound (D3D9 can lose constants otherwise, e.g. pie.ps)
        if (this->pShader)
            this->pShader->update(this->ptrShaderSpecific);
        if (this->vShader)
            this->vShader->update(this->ptrShaderSpecific);
        #endif
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
            if (FAILED(pd3dDevice->SetVertexDeclaration(device->getSpecificContextDevice()->getFVF(pBufferId->bs->FVF))))
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

            // texture stage 1 (2nd stage are used in some special shaders, and they are not per subset, are per BUFFER_GL
            TEXTURE* texture1 = pBufferId->getTextureByStage(1, 0);
            if (texture1)
            {
                IDirect3DTexture9* pp3DTexture9 = static_cast<IDirect3DTexture9*>(texture1->getBackendTexturePointer());
                pd3dDevice->SetTexture(1, pp3DTexture9);
            }
            else
            {
                pd3dDevice->SetTexture(1, nullptr);
            }
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                TEXTURE* texture0 = pBufferId->getTextureByStage(0, i);
                if (texture0)
                {
                    IDirect3DTexture9* pp3DTexture9 = static_cast<IDirect3DTexture9*>(texture0->getBackendTexturePointer());
                    pd3dDevice->SetTexture(0, pp3DTexture9);
                }
                else
                {
                    pd3dDevice->SetTexture(0, nullptr);
                }

                //https://learn.microsoft.com/en-us/windows/win32/direct3d9/rendering-from-vertex-and-index-buffers

                switch (pBufferId->mode_draw)
                {
                    case util::MODE_DRAW_POINTS:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_POINTS");
                        return false;
                    };
                    break;
                    case util::MODE_DRAW_LINES:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_LINES");
                        return false;
                    };
                    break;
                    case util::MODE_DRAW_LINE_LOOP:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_LINE_LOOP");
                        return false;
                    };
                    break;
                    case util::MODE_DRAW_LINE_STRIP:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_LINE_STRIP");
                        return false;
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
                        return false;
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
            pd3dDevice->SetVertexDeclaration(device->getSpecificContextDevice()->getFVF(pBufferId->bs->FVF));
            if (FAILED(pd3dDevice->SetStreamSource(0,//Stream Se houver Multiplos Streams
                pBufferId->bs->pVertexBuffer,//Ponteiro De Nosso Objeto Criado
                0,		//Posicao Em Bytes Do inicio  Do Stream Atual
                pBufferId->bs->sizeStructVertexInBytes)))//Tamanho Da Estrutura De Nosso Vertex
                return false;

            // texture stage 1 (2nd stage are used in some special shaders, and they are not per subset, are per BUFFER_GL
            TEXTURE* texture1 = pBufferId->getTextureByStage(1, 0);
            if (texture1)
            {
                IDirect3DTexture9* pp3DTexture9 = static_cast<IDirect3DTexture9*>(texture1->getBackendTexturePointer());
                pd3dDevice->SetTexture(1, pp3DTexture9);
            }
            else
            {
                pd3dDevice->SetTexture(1, nullptr);
            }

            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                TEXTURE* texture0 = pBufferId->getTextureByStage(0, i);
                if (texture0)
                {
                    IDirect3DTexture9* pp3DTexture9 = static_cast<IDirect3DTexture9*>(texture0->getBackendTexturePointer());
                    pd3dDevice->SetTexture(0, pp3DTexture9);
                }
                else
                {
                    pd3dDevice->SetTexture(0, nullptr);
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
                        const UINT countLine = pBufferId->vertexCountVB[i] - 1;
                        if (FAILED(pd3dDevice->DrawPrimitive(D3DPT_LINESTRIP, 0, countLine)))
                            return false;
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
            // Dynamic buffers must be created in D3DPOOL_DEFAULT (not MANAGED) and typically with WRITEONLY.
            // If you later need to update parts of the dynamic buffer, use D3DLOCK_NOOVERWRITE for partial updates and D3DLOCK_DISCARD when rewriting whole buffer.
            if (FAILED(pBufferId->bs->pVertexBuffer->Lock(0, 0, (void**)&pvertex, D3DLOCK_DISCARD)))
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

    bool SHADER::renderParticle(const BUFFER_GL* pBufferId, const PARTICLE_CONTROL* particleControl) const
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        IDirect3DDevice9* pd3dDevice = device->getSpecificContextDevice()->pd3dDevice;
        DWORD depthTestEnabled = FALSE;

        pd3dDevice->GetRenderState(D3DRS_ZENABLE, &depthTestEnabled);
        pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

        const D3DMATRIX* modelView = reinterpret_cast<const D3DMATRIX*>(&SHADER::modelView);
        if (FAILED(pd3dDevice->SetTransform(D3DTS_WORLD, modelView)))
        {
            ERROR_AT(__LINE__, __FILE__, "Failed to set SetTransform D3DTS_WORLD for modelView");
            return false;
        };

        D3D_PS_VS* d3dPsVs = static_cast<D3D_PS_VS*>(ptrShaderSpecific);

        if (d3dPsVs->pd3dPixelShader)
        {
            if (FAILED(pd3dDevice->SetPixelShader(d3dPsVs->pd3dPixelShader)))
            {
                ERROR_AT(__LINE__, __FILE__, "Failed to set Pixel Shader");
                return false;
            }
        }
        else
        {
            if (FAILED(pd3dDevice->SetPixelShader(nullptr)))
            {
                ERROR_AT(__LINE__, __FILE__, "Failed to set Pixel Shader to null");
                return false;
            }
        }

        if (d3dPsVs->pd3dVertexShader)
        {
            if (FAILED(pd3dDevice->SetVertexShader(d3dPsVs->pd3dVertexShader)))
            {
                ERROR_AT(__LINE__, __FILE__, "Failed to set Vertex Shader");
                return false;
            }
            const D3DXMATRIX* pMvpMatrix    = reinterpret_cast<const D3DXMATRIX*>(&SHADER::mvpMatrix);
            const D3DXMATRIX* pMatrixHandle = reinterpret_cast<const D3DXMATRIX*>(&SHADER::modelView);

            d3dPsVs->constantTableVS->SetMatrix(pd3dDevice, d3dPsVs->mvpMatrixHandle, pMvpMatrix);
            d3dPsVs->constantTableVS->SetMatrix(pd3dDevice, d3dPsVs->mvMatrixHandle,  pMatrixHandle);
        }
        else
        {
            if (FAILED(pd3dDevice->SetVertexShader(nullptr)))
            {
                ERROR_AT(__LINE__, __FILE__, "Failed to set Vertex Shader to null");
                return false;
            }
        }
        #if defined DEBUG_SHADER_D3D_MINIMIZE_ERROR
        // You might have problem with shader, untill now the flow works fine, but in case suspicios if the constants are lost..
        // Re-apply PS/VS constants after shaders are bound (D3D9 can lose constants otherwise, e.g. pie.ps)
        if (this->pShader)
            this->pShader->update(this->ptrShaderSpecific);
        if (this->vShader)
            this->vShader->update(this->ptrShaderSpecific);
        #endif
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
            if (FAILED(pd3dDevice->SetVertexDeclaration(device->getSpecificContextDevice()->getFVF(pBufferId->bs->FVF))))
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

            VAR_SHADER* varColor = this->pShader
                ? this->pShader->getVarByName("color")
                : nullptr;

            const uint32_t totalAlive = particleControl->getTotalAlive();
            const VERTEX_UV* buffer = particleControl->getVertexBuffer();

            // texture stage 1 (2nd stage are used in some special shaders, and they are not per subset, are per BUFFER_GL
            TEXTURE* texture1 = pBufferId->getTextureByStage(1, 0);
            if (texture1)
            {
                IDirect3DTexture9* pp3DTexture9 = static_cast<IDirect3DTexture9*>(texture1->getBackendTexturePointer());
                pd3dDevice->SetTexture(1, pp3DTexture9);
            }
            else
            {
                pd3dDevice->SetTexture(1, nullptr);
            }

            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                TEXTURE* texture0 = pBufferId->getTextureByStage(0, i);
                if (texture0)
                {
                    IDirect3DTexture9* pp3DTexture9 = static_cast<IDirect3DTexture9*>(texture0->getBackendTexturePointer());
                    pd3dDevice->SetTexture(0, pp3DTexture9);
                }
                else
                {
                    pd3dDevice->SetTexture(0, nullptr);
                }

                //https://learn.microsoft.com/en-us/windows/win32/direct3d9/rendering-from-vertex-and-index-buffers

                switch (pBufferId->mode_draw)
                {
                    case util::MODE_DRAW_POINTS:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_POINTS for particles");
                        return false;
                    };
                    break;
                    case util::MODE_DRAW_LINES:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_LINES for particles");
                        return false;
                    };
                    break;
                    case util::MODE_DRAW_LINE_LOOP:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_LINE_LOOP for particles");
                        return false;
                    };
                    break;
                    case util::MODE_DRAW_LINE_STRIP:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_LINE_STRIP for particles");
                        return false;
                    };
                    break;
                    case util::MODE_DRAW_TRIANGLES:
                    {
                        
                        const UINT countTriangle      = pBufferId->indexCountIB[i] / 3;
                        const UINT numVertices        = pBufferId->sizeOfArrayVertex;
                        const UINT vertexStartVB      = 0;
                        constexpr UINT MinVertexIndex = 0;
                        const ATT_PARTICLE* particles = particleControl->getAttParticle();

                        if (varColor)
                        {
                            const D3DXHANDLE handleVarColor = *static_cast<const D3DXHANDLE*>(varColor->ptrHandleVar);

                            for (unsigned int j = 0; j < totalAlive; ++j)
                            {
                                const VERTEX_UV* vertex = &buffer[j * 4];
                                const ATT_PARTICLE*  particle = &particles[j];
                                void* pvertex = nullptr;
                                // Dynamic buffers must be created in D3DPOOL_DEFAULT (not MANAGED) and typically with WRITEONLY.
                                //•	If you later need to update parts of the dynamic buffer, use D3DLOCK_NOOVERWRITE for partial updates and D3DLOCK_DISCARD when rewriting whole buffer.
                                if (FAILED(pBufferId->bs->pVertexBuffer->Lock(0, 0, (void**)&pvertex, D3DLOCK_DISCARD)))
                                {
                                    ERROR_AT(__LINE__, __FILE__, "failed to lock VERTEX BUFFER");
                                    return false;
                                }
                                memcpy(pvertex, vertex, sizeof(VERTEX_UV) * 4);
                                pBufferId->bs->pVertexBuffer->Unlock();

                                d3dPsVs->constantTablePS->SetFloatArray(pd3dDevice, handleVarColor, particle->color, 4);

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
                            }
                        }
                        else
                        {
                            for (unsigned int j = 0; j < totalAlive; ++j)
                            {
                                const VERTEX_UV* vertex = &buffer[j * 4];
                                const ATT_PARTICLE* particle  = &particles[j];
                                void* pvertex = nullptr;
                                // Dynamic buffers must be created in D3DPOOL_DEFAULT (not MANAGED) and typically with WRITEONLY.
                                //•	If you later need to update parts of the dynamic buffer, use D3DLOCK_NOOVERWRITE for partial updates and D3DLOCK_DISCARD when rewriting whole buffer.
                                if (FAILED(pBufferId->bs->pVertexBuffer->Lock(0, 0, (void**)&pvertex, D3DLOCK_DISCARD)))
                                {
                                    ERROR_AT(__LINE__, __FILE__, "failed to lock VERTEX BUFFER");
                                    return false;
                                }
                                memcpy(pvertex, vertex, sizeof(VERTEX_UV) * 4);
                                pBufferId->bs->pVertexBuffer->Unlock();

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
                            }
                        }
                    };
                    break;
                    case util::MODE_DRAW_TRIANGLE_STRIP:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_TRIANGLE_STRIP for particles");
                        return false;
                    };
                    break;
                    case util::MODE_DRAW_TRIANGLE_FAN:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_TRIANGLE_FAN for particles");
                        return false;
                    };
                    break;
                    default:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Wrong mode draw for particles");
                        return false;
                    }
                }
            }
        }
        else // Vertex buffer
        {
            ERROR_AT(__LINE__, __FILE__, "Not implemented vertex buffer for particle");
            return false;
        }

        if (depthTestEnabled)
        {
            pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        }
        return true;
    }

    bool SHADER::renderParticle(const BUFFER_GL* pBufferId, const FLUID_GROUP* pGroup) const
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        IDirect3DDevice9* pd3dDevice = device->getSpecificContextDevice()->pd3dDevice;
        DWORD depthTestEnabled = FALSE;

        pd3dDevice->GetRenderState(D3DRS_ZENABLE, &depthTestEnabled);
        pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);

        const D3DMATRIX* modelView = reinterpret_cast<const D3DMATRIX*>(&SHADER::modelView);
        if (FAILED(pd3dDevice->SetTransform(D3DTS_WORLD, modelView)))
        {
            ERROR_AT(__LINE__, __FILE__, "Failed to set SetTransform D3DTS_WORLD for modelView");
            return false;
        };

        D3D_PS_VS* d3dPsVs = static_cast<D3D_PS_VS*>(ptrShaderSpecific);

        if (d3dPsVs->pd3dPixelShader)
        {
            if (FAILED(pd3dDevice->SetPixelShader(d3dPsVs->pd3dPixelShader)))
            {
                ERROR_AT(__LINE__, __FILE__, "Failed to set Pixel Shader");
                return false;
            }
        }
        else
        {
            if (FAILED(pd3dDevice->SetPixelShader(nullptr)))
            {
                ERROR_AT(__LINE__, __FILE__, "Failed to set Pixel Shader to null");
                return false;
            }
        }

        if (d3dPsVs->pd3dVertexShader)
        {
            if (FAILED(pd3dDevice->SetVertexShader(d3dPsVs->pd3dVertexShader)))
            {
                ERROR_AT(__LINE__, __FILE__, "Failed to set Vertex Shader");
                return false;
            }
            const D3DXMATRIX* pMvpMatrix    = reinterpret_cast<const D3DXMATRIX*>(&SHADER::mvpMatrix);
            const D3DXMATRIX* pMatrixHandle = reinterpret_cast<const D3DXMATRIX*>(&SHADER::modelView);

            d3dPsVs->constantTableVS->SetMatrix(pd3dDevice, d3dPsVs->mvpMatrixHandle, pMvpMatrix);
            d3dPsVs->constantTableVS->SetMatrix(pd3dDevice, d3dPsVs->mvMatrixHandle, pMatrixHandle);
        }
        else
        {
            if (FAILED(pd3dDevice->SetVertexShader(nullptr)))
            {
                ERROR_AT(__LINE__, __FILE__, "Failed to set Vertex Shader to null");
                return false;
            }
        }
        #if defined DEBUG_SHADER_D3D_MINIMIZE_ERROR
        // You might have problem with shader, untill now the flow works fine, but in case suspicios if the constants are lost..
        // Re-apply PS/VS constants after shaders are bound (D3D9 can lose constants otherwise, e.g. pie.ps)
        if (this->pShader)
            this->pShader->update(this->ptrShaderSpecific);
        if (this->vShader)
            this->vShader->update(this->ptrShaderSpecific);
        #endif
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
            if (FAILED(pd3dDevice->SetVertexDeclaration(device->getSpecificContextDevice()->getFVF(pBufferId->bs->FVF))))
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

            VAR_SHADER* varColor = this->pShader
                ? this->pShader->getVarByName("color")
                : nullptr;

            // texture stage 1 (2nd stage are used in some special shaders, and they are not per subset, are per BUFFER_GL
            TEXTURE* texture1 = pBufferId->getTextureByStage(1, 0);
            if (texture1)
            {
                IDirect3DTexture9* pp3DTexture9 = static_cast<IDirect3DTexture9*>(texture1->getBackendTexturePointer());
                pd3dDevice->SetTexture(1, pp3DTexture9);
            }
            else
            {
                pd3dDevice->SetTexture(1, nullptr);
            }

            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                TEXTURE* texture0 = pBufferId->getTextureByStage(0, i);
                if (texture0)
                {
                    IDirect3DTexture9* pp3DTexture9 = static_cast<IDirect3DTexture9*>(texture0->getBackendTexturePointer());
                    pd3dDevice->SetTexture(0, pp3DTexture9);
                }
                else
                {
                    pd3dDevice->SetTexture(0, nullptr);
                }

                //https://learn.microsoft.com/en-us/windows/win32/direct3d9/rendering-from-vertex-and-index-buffers

                switch (pBufferId->mode_draw)
                {
                    case util::MODE_DRAW_POINTS:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_POINTS for particles");
                        return false;
                    };
                    break;
                    case util::MODE_DRAW_LINES:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_LINES for particles");
                        return false;
                    };
                    break;
                    case util::MODE_DRAW_LINE_LOOP:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_LINE_LOOP for particles");
                        return false;
                    };
                    break;
                    case util::MODE_DRAW_LINE_STRIP:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_LINE_STRIP for particles");
                        return false;
                    };
                    break;
                    case util::MODE_DRAW_TRIANGLES:
                    {
                        const UINT countTriangle      = pBufferId->indexCountIB[i] / 3;
                        const UINT numVertices        = pBufferId->sizeOfArrayVertex;
                        constexpr UINT vertexStartVB  = 0;
                        constexpr UINT MinVertexIndex = 0;

                        if (varColor && pGroup->color)
                        {
                            const D3DXHANDLE handleVarColor = *static_cast<const D3DXHANDLE*>(varColor->ptrHandleVar);
                            d3dPsVs->constantTablePS->SetFloatArray(pd3dDevice, handleVarColor, *pGroup->color, 4);
                        }
                        if (pGroup->segmented)
                        {
                            for (unsigned int j = 0; j < pGroup->totalParticleToRender; ++j)
                            {
                                const VEC3* pParticle = &pGroup->vertex_particle[j * 4];
                                const VEC2* uv = &pGroup->uv[j * 4];
                                VERTEX_UV vertex[4];
                                vertex[0].x = pParticle[0].x;
                                vertex[0].y = pParticle[0].y;
                                vertex[0].z = pParticle[0].z;
                                vertex[0].u = uv[0].x;
                                vertex[0].v = uv[0].y;

                                vertex[1].x = pParticle[1].x;
                                vertex[1].y = pParticle[1].y;
                                vertex[1].z = pParticle[1].z;
                                vertex[1].u = uv[1].x;
                                vertex[1].v = uv[1].y;

                                vertex[2].x = pParticle[2].x;
                                vertex[2].y = pParticle[2].y;
                                vertex[2].z = pParticle[2].z;
                                vertex[2].u = uv[2].x;
                                vertex[2].v = uv[2].y;

                                vertex[3].x = pParticle[3].x;
                                vertex[3].y = pParticle[3].y;
                                vertex[3].z = pParticle[3].z;
                                vertex[3].u = uv[3].x;
                                vertex[3].v = uv[3].y;

                                void* pvertex = nullptr;
                                // Dynamic buffers must be created in D3DPOOL_DEFAULT (not MANAGED) and typically with WRITEONLY.
                                //	If you later need to update parts of the dynamic buffer, use D3DLOCK_NOOVERWRITE for partial updates and D3DLOCK_DISCARD when rewriting whole buffer.
                                if (FAILED(pBufferId->bs->pVertexBuffer->Lock(0, 0, (void**)&pvertex, D3DLOCK_DISCARD)))
                                {
                                    ERROR_AT(__LINE__, __FILE__, "failed to lock VERTEX BUFFER");
                                    return false;
                                }
                                memcpy(pvertex, vertex, sizeof(vertex));
                                pBufferId->bs->pVertexBuffer->Unlock();


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
                            }
                        }
                        else
                        {
                            for (unsigned int j = 0; j < pGroup->totalParticleToRender; ++j)
                            {
                                const VEC3* pParticle = &pGroup->vertex_particle[j * 4];
                                const VEC2* uv = pGroup->uv; // when not segemented we always use the same uv
                                VERTEX_UV vertex[4];
                                vertex[0].x = pParticle[0].x;
                                vertex[0].y = pParticle[0].y;
                                vertex[0].z = pParticle[0].z;
                                vertex[0].u = uv[0].x;
                                vertex[0].v = uv[0].y;

                                vertex[1].x = pParticle[1].x;
                                vertex[1].y = pParticle[1].y;
                                vertex[1].z = pParticle[1].z;
                                vertex[1].u = uv[1].x;
                                vertex[1].v = uv[1].y;

                                vertex[2].x = pParticle[2].x;
                                vertex[2].y = pParticle[2].y;
                                vertex[2].z = pParticle[2].z;
                                vertex[2].u = uv[2].x;
                                vertex[2].v = uv[2].y;

                                vertex[3].x = pParticle[3].x;
                                vertex[3].y = pParticle[3].y;
                                vertex[3].z = pParticle[3].z;
                                vertex[3].u = uv[3].x;
                                vertex[3].v = uv[3].y;

                                void* pvertex = nullptr;
                                // Dynamic buffers must be created in D3DPOOL_DEFAULT (not MANAGED) and typically with WRITEONLY.
                                //	If you later need to update parts of the dynamic buffer, use D3DLOCK_NOOVERWRITE for partial updates and D3DLOCK_DISCARD when rewriting whole buffer.
                                if (FAILED(pBufferId->bs->pVertexBuffer->Lock(0, 0, (void**)&pvertex, D3DLOCK_DISCARD)))
                                {
                                    ERROR_AT(__LINE__, __FILE__, "failed to lock VERTEX BUFFER");
                                    return false;
                                }
                                memcpy(pvertex, vertex, sizeof(vertex));
                                pBufferId->bs->pVertexBuffer->Unlock();


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
                            }
                        }
                    };
                    break;
                    case util::MODE_DRAW_TRIANGLE_STRIP:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_TRIANGLE_STRIP for particles");
                        return false;
                    };
                    break;
                    case util::MODE_DRAW_TRIANGLE_FAN:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Not implemented mode draw for render MODE_DRAW_TRIANGLE_FAN for particles");
                        return false;
                    };
                    break;
                    default:
                    {
                        ERROR_AT(__LINE__, __FILE__, "Wrong mode draw for particles");
                        return false;
                    }
                }
            }
        }
        else // Vertex buffer
        {
            ERROR_AT(__LINE__, __FILE__, "Not implemented vertex buffer for particle");
            return false;
        }

        if (depthTestEnabled)
        {
            pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_TRUE);
        }
        return true;
    }
}

#endif // USE_DIRECTX9