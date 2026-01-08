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
#include <directx9-specific.h>
#include <header-mesh.h>

namespace mbm
{
    BUFFER_GL::BUFFER_GL() noexcept
    {
        bs = new BUFFER_SPECIFIC();
        totalSubset = 0;
        texture1 = nullptr;
    }

    BUFFER_GL::~BUFFER_GL()
    {
        if (bs)
            delete bs;
        bs = nullptr;
        texture1 = nullptr;
        texture0.clear();
    }

    BUFFER_SPECIFIC::BUFFER_SPECIFIC() noexcept :
        FVF(FVF_PROVIDE_BY_ENGINE::FVF_POS),
        pVertexBuffer(nullptr)
    {

    }

    BUFFER_SPECIFIC::~BUFFER_SPECIFIC()
    {
        if (pVertexBuffer)
            pVertexBuffer->Release();
        pVertexBuffer = nullptr;
    }
    

    void BUFFER_GL::release()
    {
        #pragma message(REMINDER_TODO "  implement delete buffer");
        totalSubset   = 0;
    }

    bool BUFFER_GL::loadBuffer(const mbm::VEC3 *vertex, // type vertex buffer
		const mbm::VEC3 *normal,const mbm::VEC2 *uv,const uint32_t sizeOfArrayVertex,
		const uint32_t totalSubsets,const int *vertexStartSubset,const int *vertexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        this->release();
        if (!vertex || !sizeOfArrayVertex || !totalSubsets || !vertexStartSubset || !vertexCountSubset)
            return false;
        this->totalSubset        = totalSubsets;
        
        #pragma message(REMINDER_TODO "  generate buffers");

        /*if (FAILED(pd3dDevice->CreateVertexBuffer(//Tamanho Do Vertex Buffer (array * sturtura)
            sizeStructVertexInBytes * sizeOfArray,
            0, //Usage D3DUSAGE_WRITEONLY
            FVF,//FVF Usado
            D3DPOOL_DEFAULT,//Local Da mem�ria
            &buffer,//Endere�o Do IDirect3DVertexBuffer9
            NULL)))				//sempre zero
        {
            return false;//Erro Ao criar Vertex Buffer
        }*/
        
        return true;
    }

    


    SMART_VERTEX::SMART_VERTEX(const VEC3* _pos, const VEC3* _normal, const VEC2* _uv, unsigned int _size_array) noexcept:
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

    void SMART_VERTEX::copyTod3dVertexBuffer(void* pvertex) const noexcept
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

    FVF_PROVIDE_BY_ENGINE SMART_VERTEX::getFVF() const noexcept
    {
        return FVF;
    }

    uint32_t SMART_VERTEX::getSizeOfStructureInBytes() const noexcept
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
    

    bool BUFFER_GL::loadBuffer(const VEC3 *vertex, // type index buffer
		const VEC3 *normal,const VEC2 *uv,const uint32_t sizeOfArrayVertex,
		const uint16_t *arrayIndices,const uint32_t totalSubsets,const int *indexStartSubset,
		const int *indexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        release();
        if (!vertex || !sizeOfArrayVertex || !arrayIndices || !totalSubsets || !indexStartSubset || !indexCountSubset)
            return false;
        mbm::DEVICE* device = mbm::DEVICE::getInstance();

        constexpr DWORD DFVF = 0;// Non-FVF buffers
        IDirect3DDevice9* pd3dDevice = device->specificContextDevice->pd3dDevice;
        SMART_VERTEX smatVertex(vertex, normal, uv, sizeOfArrayVertex);
        this->bs->FVF = smatVertex.getFVF();
        uint32_t sizeStructVertexInBytes = smatVertex.getSizeOfStructureInBytes();
        
        if (FAILED(pd3dDevice->CreateVertexBuffer(//Tamanho Do Vertex Buffer (array * sturtura)
            sizeStructVertexInBytes * sizeOfArrayVertex,
            D3DUSAGE_WRITEONLY, //Usage D3DUSAGE_WRITEONLY
            DFVF,//FVF
            D3DPOOL_DEFAULT,//local memory
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
        smatVertex.copyTod3dVertexBuffer(pvertex);
        this->bs->pVertexBuffer->Unlock();
        return true;
    }

    bool BUFFER_GL::loadBufferDynamic(uint16_t *arrayIndices, uint32_t totalSubsets, int *indexStartSubset,
                                  int *indexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        release();
        if (!arrayIndices || !totalSubsets || !indexStartSubset || !indexCountSubset)
            return false;
        #pragma message(REMINDER_TODO "  generate buffers");

        for (uint32_t i = 0; i < this->totalSubset; ++i)
        {
        }

        if(info_draw_mode)
		{
		}
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
        const char *defaultCodePs = "precision mediump float;"
                                    "varying vec2 vTexCoord;"
                                    "uniform sampler2D sample0;"
                                    "void main()"
                                    "{"
                                    "    gl_FragColor = texture2D( sample0, vTexCoord );"
                                    "}";

        const char *defaultCodeVs =
            "attribute vec4 aPosition;"
            "attribute vec2 aTextCoord;"
            "attribute vec3 aNormal;" // Per-vertex normal information we will pass in.
            "uniform mat4 mvpMatrix;" // A constant representing the combined model/view/projection matrix.
            "varying vec2 vTexCoord;"
            "void main()"
            "{"
            "     gl_Position = mvpMatrix * aPosition;"
            "     vTexCoord = aTextCoord;"
            "}";
        return true;
        if (this->programObject)
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
        }
        #pragma message(REMINDER_TODO "  implement get attrib and uniform locations");
        return true;
    }


    bool SHADER::render(const BUFFER_GL *pBufferId) const
    {
        #pragma message(REMINDER_TODO "  implement set cull face and front face");
		
        if (pBufferId) // Index buffer
        {
            if (!pBufferId)
                return false;
            #pragma message(REMINDER_TODO "  implement use program");
            #pragma message(REMINDER_TODO "  implement bind buffers and set attrib pointers");
            #pragma message(REMINDER_TODO "  draw elements");
            //-----------------------------------------------------------------------------------------------------------
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                if (pBufferId)
                {
                    #pragma message(REMINDER_TODO "  implement active texture and bind texture");
                }
                else
                {
                    #pragma message(REMINDER_TODO "  implement bind texture 0");
                }
                #pragma message(REMINDER_TODO "  implement draw elements");
            }
        }
        else // Vertex buffer
        {
            if (!pBufferId)
                return false;
            #pragma message(REMINDER_TODO "  implement use program");
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                #pragma message(REMINDER_TODO "  implement bind buffer and set attrib pointer");
                //-----------------------------------------------------------------------------------------------------------
                if (this->normalHandle != -1) // Normal  (nem sempre temos normal nos shaders)
                {
                    #pragma message(REMINDER_TODO "  implement bind buffer and set attrib pointer");
                }
                //-----------------------------------------------------------------------------------------------------------
                #pragma message(REMINDER_TODO "  implement bind buffer and set attrib pointer");
                //-----------------------------------------------------------------------------------------------------------
                #pragma message(REMINDER_TODO "  implement set uniform matrices");
                //-----------------------------------------------------------------------------------------------------------
                #pragma message(REMINDER_TODO "  implement active texture");
                if (pBufferId)
                {
                    #pragma message(REMINDER_TODO "  implement bind texture and set uniform");
                }
                else
                {
                    #pragma message(REMINDER_TODO "  implement bind texture 0");
                }

                #pragma message(REMINDER_TODO "  implement draw arrays");
            }
        }
        #pragma message(REMINDER_TODO "  implement unbind buffer");
        return true;
    }

    bool SHADER::renderDynamic(const BUFFER_GL *pBufferId,const VEC3 *vertex,const VEC3 *normal,const VEC2 *uv) const
    {
		#pragma message(REMINDER_TODO "  implement set cull face and front face");

        if (pBufferId) // Index buffer
        {
            if (!pBufferId)
                return false;
            //TODO: implement use program
            //-----------------------------------------------------------------------------------------------------------
            //TODO: Bind vertex array
            //-----------------------------------------------------------------------------------------------------------
            if (this->normalHandle != -1)
            {
                //TODO: implement enable vertex attrib array and set pointer
            }
            //-----------------------------------------------------------------------------------------------------------
            //TODO: implement enable vertex attrib array and set pointer
            //-----------------------------------------------------------------------------------------------------------
            //TODO: implement set uniform matrices
            //-----------------------------------------------------------------------------------------------------------
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                //TODO: implement active texture and bind texture

                if (pBufferId)
                {
                    //TODO: implement bind texture and set uniform
                }
                else
                {
                    //TODO: implement bind texture 0
                }
                //TODO: implement draw elements
            }
        }
        else // Vertex buffer
        {
            if (!pBufferId)
                return false;
            //TODO: implement use program
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                //TODO: implement enable vertex attrib array and set pointer
                //-----------------------------------------------------------------------------------------------------------
                if (this->normalHandle != -1) // Normal  (nem sempre temos normal nos shaders)
                {
                    //TODO: implement enable vertex attrib array and set pointer
                }
                //-----------------------------------------------------------------------------------------------------------
                //TODO: implement enable vertex attrib array and set pointer
                //-----------------------------------------------------------------------------------------------------------
                //TODO: implement set uniform matrices
                //-----------------------------------------------------------------------------------------------------------
                //TODO: implement active texture
                
                if (pBufferId)
                {
                    //TODO: implement bind texture and set uniform
                }
                else
                {
                    //TODO: implement bind texture 0
                }

                //TODO: implement draw arrays
            }
        }
        //TODO: implement unbind buffer
        return true;
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