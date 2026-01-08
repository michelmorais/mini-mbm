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


#if defined (USE_DUMMY_BACK_END_ENGINE)

#include "dummy-engine.h" // for compiler_message, you can remove it after implement the functions

#include <shader.h>
#include <util-interface.h>
#include <shader-var-cfg.h>
#include <cstdlib>
#include <header-mesh.h>

namespace mbm
{
    BUFFER_GL::BUFFER_GL() noexcept
    {
     
    }
    BUFFER_GL::~BUFFER_GL(){}

    void BUFFER_GL::release()
    {
        #ifdef SHOW_PRAGMA_MESSAGE
        #pragma message(REMINDER_TODO "  implement delete buffer");
        #endif
    }

    bool BUFFER_GL::loadBuffer(const mbm::VEC3 *vertex, // type vertex buffer
        const mbm::VEC3 *normal,const mbm::VEC2 *uv,const uint32_t sizeOfArrayVertex,
        const uint32_t totalSubsets,const int *vertexStartSubset,const int *vertexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        this->totalSubset        = totalSubsets;
        #ifdef SHOW_PRAGMA_MESSAGE
        #pragma message(REMINDER_TODO "  generate buffers");
        #endif
        return false;
    }

    bool BUFFER_GL::loadBuffer(const VEC3 *vertex, // type index buffer
        const VEC3 *normal,const VEC2 *uv,const uint32_t sizeOfArrayVertex,
        const uint16_t *arrayIndices,const uint32_t totalSubsets,const int *indexStartSubset,
        const int *indexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        release();
        if (!vertex || !sizeOfArrayVertex || !arrayIndices || !totalSubsets || !indexStartSubset || !indexCountSubset)
            return false;
        #ifdef SHOW_PRAGMA_MESSAGE
        #pragma message(REMINDER_TODO "  generate buffers");
        #endif
        this->totalSubset      = totalSubsets;
        return false;
    }

    bool BUFFER_GL::loadBufferDynamic(uint16_t *arrayIndices, uint32_t totalSubsets, int *indexStartSubset,
                                  int *indexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        release();
        if (!arrayIndices || !totalSubsets || !indexStartSubset || !indexCountSubset)
            return false;
        this->totalSubset      = totalSubsets;
        #ifdef SHOW_PRAGMA_MESSAGE
        #pragma message(REMINDER_TODO "  generate buffers");
        #endif

        for (uint32_t i = 0; i < this->totalSubset; ++i)
        {
            
        }

        return false;
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
            #ifdef SHOW_PRAGMA_MESSAGE
            #pragma message(REMINDER_TODO "  implement get uniform location");
            #endif
            
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
        #ifdef SHOW_PRAGMA_MESSAGE
        #pragma message(REMINDER_TODO "  implement use program");
        #endif
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
        #ifdef SHOW_PRAGMA_MESSAGE
        #pragma message(REMINDER_TODO "  implement release shader");
        #endif
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
        #ifdef SHOW_PRAGMA_MESSAGE
        #pragma message(REMINDER_TODO "  implement delete program");
        #endif
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
        #ifdef SHOW_PRAGMA_MESSAGE
        #pragma message(REMINDER_TODO "  implement get attrib and uniform locations");
        #endif
        return true;
    }


    bool SHADER::render(const BUFFER_GL *pBufferId) const
    {
        #ifdef SHOW_PRAGMA_MESSAGE
        #pragma message(REMINDER_TODO "  implement set cull face and front face");
        #endif
        
        if (pBufferId) // Index buffer
        {
            #ifdef SHOW_PRAGMA_MESSAGE
            #pragma message(REMINDER_TODO "  implement use program");
            #pragma message(REMINDER_TODO "  implement bind buffers and set attrib pointers");
            #pragma message(REMINDER_TODO "  draw elements");
            #endif
            //-----------------------------------------------------------------------------------------------------------
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                #ifdef SHOW_PRAGMA_MESSAGE
                #pragma message(REMINDER_TODO "  implement active texture and bind texture");
                #endif
            }
        }
        else // Vertex buffer
        {
            if (!pBufferId)
                return false;
            #ifdef SHOW_PRAGMA_MESSAGE
            #pragma message(REMINDER_TODO "  implement use program");
            #endif
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                #ifdef SHOW_PRAGMA_MESSAGE
                #pragma message(REMINDER_TODO "  implement bind buffer and set attrib pointer");
                #endif
                //-----------------------------------------------------------------------------------------------------------
                if (this->normalHandle != -1) // Normal  (nem sempre temos normal nos shaders)
                {
                    #ifdef SHOW_PRAGMA_MESSAGE
                    #pragma message(REMINDER_TODO "  implement bind buffer and set attrib pointer");
                    #endif
                }
                #ifdef SHOW_PRAGMA_MESSAGE
                //-----------------------------------------------------------------------------------------------------------
                #pragma message(REMINDER_TODO "  implement bind buffer and set attrib pointer");
                //-----------------------------------------------------------------------------------------------------------
                #pragma message(REMINDER_TODO "  implement set uniform matrices");
                //-----------------------------------------------------------------------------------------------------------
                #pragma message(REMINDER_TODO "  implement active texture");
                #endif
                if (pBufferId)
                {
                    #ifdef SHOW_PRAGMA_MESSAGE
                    #pragma message(REMINDER_TODO "  implement bind texture and set uniform");
                    #endif
                }
                else
                {
                    #ifdef SHOW_PRAGMA_MESSAGE
                    #pragma message(REMINDER_TODO "  implement bind texture 0");
                    #endif
                }
                #ifdef SHOW_PRAGMA_MESSAGE
                #pragma message(REMINDER_TODO "  implement draw arrays");
                #endif
            }
        }
        #ifdef SHOW_PRAGMA_MESSAGE
        #pragma message(REMINDER_TODO "  implement unbind buffer");
        #endif
        return true;
    }

    bool SHADER::renderDynamic(const BUFFER_GL *pBufferId,const VEC3 *vertex,const VEC3 *normal,const VEC2 *uv) const
    {
        #ifdef SHOW_PRAGMA_MESSAGE
        #pragma message(REMINDER_TODO "  implement set cull face and front face");
        #endif

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
        uint32_t shader;
        int          compiled;
        // Create the shader object
        #ifdef SHOW_PRAGMA_MESSAGE
        #pragma message(REMINDER_TODO "  implement create shader");
        #endif
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
        uint32_t vertexShader;
        uint32_t fragmentShader;
        int          linked;
        if (this->programObject)
        {
            PRINT_IF_DEBUG("programObject already exists");
            return programObject;
        }
        #ifdef SHOW_PRAGMA_MESSAGE
        #pragma message(REMINDER_TODO "  Load the vertex/fragment shaders");
        #endif
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
        #ifdef SHOW_PRAGMA_MESSAGE
        #pragma message(REMINDER_TODO "  Free up no longer needed shader resources");
        #endif
        return programObject;
    }
}

#endif // USE_DUMMY_BACK_END_ENGINE