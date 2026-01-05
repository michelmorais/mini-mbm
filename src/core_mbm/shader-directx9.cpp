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
#include <cstdlib>
#include <header-mesh.h>

namespace mbm
{
    BUFFER_GL::BUFFER_GL()
    noexcept : vboIndexSubsetIB(nullptr),
               indexStartIB(nullptr),
               indexCountIB(nullptr),
               vboVertexSubsetVB(nullptr),
               vboNormalSubsetVB(nullptr),
               vboTextureSubsetVB(nullptr),
               vertexStartVB(nullptr),
               vertexCountVB(nullptr),
               totalSubset(0),
               idTexture0(nullptr),
               useAlpha(nullptr),
               idTexture1(0),
               isIndexBuffer(false)
               //TODO: fix these values
               //mode_draw(GL_TRIANGLES),
               //mode_cull_face(GL_BACK),
               //mode_front_face_direction(GL_CW)
    {
        #pragma message(REMINDER_TODO "  initialize mode values");
        memset(vboVertNorTexIB, 0, sizeof(vboVertNorTexIB));
    }

    void BUFFER_GL::release()
    {
        #pragma message(REMINDER_TODO "  implement delete buffer");
        memset(vboVertNorTexIB, 0, sizeof(vboVertNorTexIB));
        
        if (vboIndexSubsetIB)
            delete[] vboIndexSubsetIB;
        vboIndexSubsetIB = nullptr;

        if (indexStartIB)
            delete[] indexStartIB;
        indexStartIB = nullptr;

        if (indexCountIB)
            delete[] indexCountIB;
        indexCountIB = nullptr;

        if (idTexture0)
            delete[] idTexture0;
        idTexture0 = nullptr;

        if (useAlpha)
            delete[] useAlpha;
        useAlpha = nullptr;

        if (vboVertexSubsetVB)
            delete[] vboVertexSubsetVB;
        vboVertexSubsetVB = nullptr;

        if (vboNormalSubsetVB)
            delete[] vboNormalSubsetVB;
        vboNormalSubsetVB = nullptr;

        if (vboTextureSubsetVB)
            delete[] vboTextureSubsetVB;
        vboTextureSubsetVB = nullptr;

        if (vertexStartVB)
            delete[] vertexStartVB;
        vertexStartVB = nullptr;

        if (vertexCountVB)
            delete[] vertexCountVB;
        vertexCountVB = nullptr;

        idTexture1    = 0;
        totalSubset   = 0;
        isIndexBuffer = false;
    }

    bool BUFFER_GL::loadBuffer(const mbm::VEC3 *vertex, // type vertex buffer
		const mbm::VEC3 *normal,const mbm::VEC2 *uv,const uint32_t sizeOfArrayVertex,
		const uint32_t totalSubsets,const int *vertexStartSubset,const int *vertexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        this->release();
        if (!vertex || !sizeOfArrayVertex || !totalSubsets || !vertexStartSubset || !vertexCountSubset)
            return false;
        this->totalSubset        = totalSubsets;
        this->vboVertexSubsetVB  = new uint32_t[totalSubset];
        this->vboNormalSubsetVB  = new uint32_t[totalSubset];
        this->vboTextureSubsetVB = new uint32_t[totalSubset];
        this->vertexStartVB      = new int[totalSubset];
        this->vertexCountVB      = new int[totalSubset];
        memset(this->vboVertexSubsetVB, 0, sizeof(uint32_t) *  static_cast<size_t>(totalSubset));
        memset(this->vboNormalSubsetVB, 0, sizeof(uint32_t) *  static_cast<size_t>(totalSubset));
        memset(this->vboTextureSubsetVB, 0, sizeof(uint32_t) * static_cast<size_t>(totalSubset));
        #pragma message(REMINDER_TODO "  generate buffers");
        
        this->idTexture0 = new uint32_t[totalSubset];
        memset(this->idTexture0, 0, sizeof(int) * totalSubset);

        this->useAlpha = new uint8_t[totalSubset];
        memset(this->useAlpha, 0, sizeof(uint8_t) * static_cast<size_t>(totalSubset));
        this->isIndexBuffer = false;
		if(info_draw_mode)
		{
			this->mode_draw = info_draw_mode->mode_draw;
			this->mode_cull_face = info_draw_mode->mode_cull_face;
			this->mode_front_face_direction = info_draw_mode->mode_front_face_direction;
		}
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
        #pragma message(REMINDER_TODO "  generate buffers");
        this->totalSubset      = totalSubsets;
        this->vboIndexSubsetIB = new uint32_t[totalSubset];
        this->indexStartIB     = new int[totalSubset];
        this->indexCountIB     = new int[totalSubset];
        memset(this->vboIndexSubsetIB, 0, sizeof(uint32_t) * static_cast<size_t>(totalSubset));
        #pragma message(REMINDER_TODO "  generate buffers");
        this->idTexture0 = new uint32_t[this->totalSubset];
        memset(this->idTexture0, 0, sizeof(uint32_t) * static_cast<size_t>(this->totalSubset));

        this->useAlpha = new uint8_t[totalSubset];
        memset(this->useAlpha, 0, sizeof(uint8_t) * static_cast<size_t>(totalSubset));
        this->isIndexBuffer = true;
		if(info_draw_mode)
		{
			this->mode_draw = info_draw_mode->mode_draw;
			this->mode_cull_face = info_draw_mode->mode_cull_face;
			this->mode_front_face_direction = info_draw_mode->mode_front_face_direction;
		}
		return true;
    }

    bool BUFFER_GL::loadBufferDynamic(uint16_t *arrayIndices, uint32_t totalSubsets, int *indexStartSubset,
                                  int *indexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        release();
        if (!arrayIndices || !totalSubsets || !indexStartSubset || !indexCountSubset)
            return false;
        this->totalSubset      = totalSubsets;
        this->vboIndexSubsetIB = new uint32_t[totalSubset];
        this->indexStartIB     = new int[totalSubset];
        this->indexCountIB     = new int[totalSubset];
        memset(this->vboIndexSubsetIB, 0, sizeof(uint32_t) * totalSubset);
        #pragma message(REMINDER_TODO "  generate buffers");

        for (uint32_t i = 0; i < this->totalSubset; ++i)
        {
            this->indexStartIB[i] = indexStartSubset[i];
            this->indexCountIB[i] = indexCountSubset[i];
        }

        this->idTexture0 = new uint32_t[this->totalSubset];
        memset(this->idTexture0, 0, sizeof(int) * static_cast<size_t>(this->totalSubset));

        this->useAlpha = new uint8_t[totalSubset];
        memset(this->useAlpha, 0, sizeof(uint8_t) * static_cast<size_t>(totalSubset));
        this->isIndexBuffer = true;
		if(info_draw_mode)
		{
			this->mode_draw = info_draw_mode->mode_draw;
			this->mode_cull_face = info_draw_mode->mode_cull_face;
			this->mode_front_face_direction = info_draw_mode->mode_front_face_direction;
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
		
        if (pBufferId->isIndexBuffer) // Index buffer
        {
            if (!pBufferId->vboVertNorTexIB[0])
                return false;
            #pragma message(REMINDER_TODO "  implement use program");
            #pragma message(REMINDER_TODO "  implement bind buffers and set attrib pointers");
            #pragma message(REMINDER_TODO "  draw elements");
            //-----------------------------------------------------------------------------------------------------------
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                if (pBufferId->idTexture1)
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
            if (!pBufferId->vboVertexSubsetVB)
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
                if (pBufferId->idTexture1)
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

        if (pBufferId->isIndexBuffer) // Index buffer
        {
            if (!pBufferId->vboIndexSubsetIB)
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

                if (pBufferId->idTexture1)
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
            if (!pBufferId->vertexCountVB)
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
                
                if (pBufferId->idTexture1)
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