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


#if defined (USE_OPENGL_ES)

#include <shader.h>
#include <texture-manager.h>
#include <opengl_es-specific.h>
#include <util-interface.h>
#include <shader-var-cfg.h>
#include <cstdlib>
#include <header-mesh.h>

namespace mbm
{
    BUFFER_GL::BUFFER_GL() noexcept :
        indexStartIB(nullptr),
        indexCountIB(nullptr),
        mode_draw(GL_TRIANGLES),
        mode_cull_face(GL_BACK),
        mode_front_face_direction(GL_CW),
        totalSubset(0),
        texture1(nullptr)
    {
        bs = new BUFFER_SPECIFIC();
    }

    BUFFER_GL::~BUFFER_GL()
    {
        if(bs)
            delete bs;
        bs = nullptr;
        texture1 = nullptr;
        texture0.clear();
    }

    BUFFER_SPECIFIC::BUFFER_SPECIFIC() noexcept : 
        vboIndexSubsetIB(nullptr),
        vboVertexSubsetVB(nullptr),
        vboNormalSubsetVB(nullptr),
        vboTextureSubsetVB(nullptr)
    {
        memset(vboVertNorTexIB, 0, sizeof(vboVertNorTexIB));
    }

    BUFFER_SPECIFIC::~BUFFER_SPECIFIC()
    {
        this->release();
    }

    void BUFFER_SPECIFIC::release()
    {
        if (vboVertNorTexIB[0])
        {
            GLDeleteBuffers(3, vboVertNorTexIB);
        }
        memset(vboVertNorTexIB, 0, sizeof(vboVertNorTexIB));
        
        if (vboIndexSubsetIB)
            delete[] vboIndexSubsetIB;
        vboIndexSubsetIB = nullptr;

        if (vboVertexSubsetVB)
            delete[] vboVertexSubsetVB;
        vboVertexSubsetVB = nullptr;

        if (vboNormalSubsetVB)
            delete[] vboNormalSubsetVB;
        vboNormalSubsetVB = nullptr;

        if (vboTextureSubsetVB)
            delete[] vboTextureSubsetVB;
        vboTextureSubsetVB = nullptr;
    }

    void BUFFER_GL::release()
    {
        bs->release();
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
        this->bs->vboVertexSubsetVB  = new uint32_t[totalSubset];
        this->bs->vboNormalSubsetVB  = new uint32_t[totalSubset];
        this->bs->vboTextureSubsetVB = new uint32_t[totalSubset];
        this->initializeVertexBufferControl(totalSubsets, vertexStartSubset, vertexCountSubset, info_draw_mode);
        memset(this->bs->vboVertexSubsetVB, 0, sizeof(uint32_t) *  static_cast<size_t>(totalSubset));
        memset(this->bs->vboNormalSubsetVB, 0, sizeof(uint32_t) *  static_cast<size_t>(totalSubset));
        memset(this->bs->vboTextureSubsetVB, 0, sizeof(uint32_t) * static_cast<size_t>(totalSubset));
        GLGenBuffers(static_cast<GLsizei>(this->totalSubset), this->bs->vboVertexSubsetVB);
        if (!this->bs->vboVertexSubsetVB[0])
        {
            this->release();
            return false;
        }

        if (normal)
        {
            GLGenBuffers(static_cast<GLsizei>(this->totalSubset), this->bs->vboNormalSubsetVB);
        }

        if (uv)
        {
            GLGenBuffers(static_cast<GLsizei>(this->totalSubset), this->bs->vboTextureSubsetVB);
        }
        for (uint32_t i = 0; i < totalSubset; ++i)
        {
            GLBindBuffer(GL_ARRAY_BUFFER, this->bs->vboVertexSubsetVB[i]);
            GLBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(mbm::VEC3) *  static_cast<size_t>(this->vertexCountVB[i])), &vertex[this->vertexStartVB[i]],GL_STATIC_DRAW);
            if (normal)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, this->bs->vboNormalSubsetVB[i]);
                GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeof(mbm::VEC3) * static_cast<size_t>(this->vertexCountVB[i])), &normal[this->vertexStartVB[i]],
                             GL_STATIC_DRAW);
            }
            if (uv)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, this->bs->vboTextureSubsetVB[i]);
                GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeof(mbm::VEC2) * static_cast<size_t>(this->vertexCountVB[i])), &uv[this->vertexStartVB[i]],
                             GL_STATIC_DRAW);
            }
        }
        GLBindBuffer(GL_ARRAY_BUFFER, 0);

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
        GLGenBuffers(3, this->bs->vboVertNorTexIB);
        if (this->bs->vboVertNorTexIB[0] == 0)
            return false;
        this->totalSubset      = totalSubsets;
        this->bs->vboIndexSubsetIB = new uint32_t[totalSubset];
        this->initializeIndexBufferControl(totalSubsets, indexStartSubset, indexCountSubset, info_draw_mode);
        memset(this->bs->vboIndexSubsetIB, 0, sizeof(uint32_t) * static_cast<size_t>(totalSubset));
        GLGenBuffers(static_cast<GLsizei>(this->totalSubset), this->bs->vboIndexSubsetIB);
        if (!this->bs->vboIndexSubsetIB[0])
        {
            this->release();
            return false;
        }

        GLBindBuffer(GL_ARRAY_BUFFER, this->bs->vboVertNorTexIB[0]);
        GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeOfArrayVertex * sizeof(mbm::VEC3)), vertex, GL_STATIC_DRAW);

        if (normal)
        {
            GLBindBuffer(GL_ARRAY_BUFFER, this->bs->vboVertNorTexIB[1]);
            GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeOfArrayVertex * sizeof(mbm::VEC3)), normal, GL_STATIC_DRAW);
        }

        if (uv)
        {
            GLBindBuffer(GL_ARRAY_BUFFER, this->bs->vboVertNorTexIB[2]);
            GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeOfArrayVertex * sizeof(mbm::VEC2)), uv, GL_STATIC_DRAW);
        }

        for (uint32_t i = 0; i < this->totalSubset; ++i)
        {
            GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->bs->vboIndexSubsetIB[i]);
            GLBufferData(GL_ELEMENT_ARRAY_BUFFER,static_cast<GLsizeiptr>(sizeof(unsigned short) * static_cast<size_t>(this->indexCountIB[i])),&arrayIndices[this->indexStartIB[i]], GL_STATIC_DRAW);
        }

        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        return true;
    }

    bool BUFFER_GL::loadBufferDynamic(uint16_t *arrayIndices, uint32_t totalSubsets, int *indexStartSubset,
                                  int *indexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode)
    {
        release();
        if (!arrayIndices || !totalSubsets || !indexStartSubset || !indexCountSubset)
            return false;
        this->totalSubset      = totalSubsets;
        this->bs->vboIndexSubsetIB = new uint32_t[totalSubset];
        memset(this->bs->vboIndexSubsetIB, 0, sizeof(uint32_t) * totalSubset);
        this->initializeIndexBufferControl(totalSubsets, indexStartSubset, indexCountSubset, info_draw_mode);
        GLGenBuffers(static_cast<GLsizei>(this->totalSubset), this->bs->vboIndexSubsetIB);
        if (!this->bs->vboIndexSubsetIB[0])
        {
            this->release();
            return false;
        }

        for (uint32_t i = 0; i < this->totalSubset; ++i)
        {
            GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->bs->vboIndexSubsetIB[i]);
            GLBufferData(GL_ELEMENT_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeof(uint16_t) * static_cast<size_t>(this->indexCountIB[i])),&arrayIndices[this->indexStartIB[i]], GL_STATIC_DRAW);
        }

        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
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
            var->handleVar = GLGetUniformLocation(programObject, nameVar);
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
        GLUseProgram(programObject);
        const std::vector<VAR_SHADER *>::size_type s = lsVar.size();
        for (std::vector<VAR_SHADER *>::size_type i = 0; i < s; ++i)
        {
            VAR_SHADER *var = lsVar[i];
            if (var)
            {
                switch (var->typeVar)
                {
                    // Uniform
                    case VAR_FLOAT: { GLUniform1f(var->handleVar, var->current[0]);
                    }
                    break;
                    case VAR_VECTOR2: { GLUniform2f(var->handleVar, var->current[0], var->current[1]);
                    }
                    break;
                    case VAR_COLOR_RGB:
                    case VAR_VECTOR: { GLUniform3f(var->handleVar, var->current[0], var->current[1], var->current[2]);
                    }
                    break;
                    case VAR_COLOR_RGBA:
                    {
                        GLUniform4f(var->handleVar, var->current[0], var->current[1], var->current[2], var->current[3]);
                    }
                    break;
                    default: {
                    }
                    break;
                }
            }
        }
    }

    SHADER::~SHADER()
    {
        if (this->programObject)
        {
            GLDeleteProgram(this->programObject);
        }
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
        if (this->programObject)
        {
            GLDeleteProgram(this->programObject);
        }
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
        GLint aPosition       = GLGetAttribLocation(programObject, "aPosition");
        this->positionHandle  = static_cast<GLint>(aPosition);
        this->mvpMatrixHandle = GLGetUniformLocation(programObject, "mvpMatrix");
        this->mvMatrixHandle  = GLGetUniformLocation(programObject, "mvMatrix");
        GLint aTextCoord      = GLGetAttribLocation(programObject, "aTextCoord");
        this->texCoordHandle  = static_cast<GLint>(aTextCoord);
        this->samplerHandle0  = GLGetUniformLocation(programObject, "sample0");
        this->samplerHandle1  = GLGetUniformLocation(programObject, "sample1");
        GLint aNormal         = GLGetAttribLocation(programObject, "aNormal") 
        this->normalHandle    = static_cast<GLint>(aNormal);
        return true;
    }


    bool SHADER::render(const BUFFER_GL *pBufferId) const
    {
		GLCullFace(pBufferId->mode_cull_face);//GL_FRONT 1028, GL_BACK 1029, GL_FRONT_AND_BACK 1032(CullFaceMode)
		GLFrontFace(pBufferId->mode_front_face_direction);//GL_CCW 2305 , GL_CW 2304(FrontFaceDirection)
		
        if (pBufferId->isIndexBuffer()) // Index buffer
        {
            if (!pBufferId->bs->vboVertNorTexIB[0])
                return false;
            GLUseProgram(this->programObject);
            //-----------------------------------------------------------------------------------------------------------
            GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->bs->vboVertNorTexIB[0]);
            GLEnableVertexAttribArray(this->positionHandle);
            GLVertexAttribPointer(this->positionHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            //-----------------------------------------------------------------------------------------------------------
            if (this->normalHandle != -1) // Normal (nem sempre temos normal nos shaders)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->bs->vboVertNorTexIB[1]);
                GLEnableVertexAttribArray(this->normalHandle);
                GLVertexAttribPointer(this->normalHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            }
            //-----------------------------------------------------------------------------------------------------------
            GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->bs->vboVertNorTexIB[2]);
            GLEnableVertexAttribArray(this->texCoordHandle);
            GLVertexAttribPointer(this->texCoordHandle, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
            //-----------------------------------------------------------------------------------------------------------
            GLUniformMatrix4fv(this->mvpMatrixHandle, 1, GL_FALSE, mvpMatrix.p);
            GLUniformMatrix4fv(this->mvMatrixHandle, 1, GL_FALSE, modelView.p);
            //-----------------------------------------------------------------------------------------------------------
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                GLActiveTexture(GL_TEXTURE0);
                // if(pBufferId->hasColorKeying[i])
                //  glEnable(GL_BLEND);
                const TEXTURE* texture0 = pBufferId->getTextureByStage(0, i);
                GLBindTexture(GL_TEXTURE_2D, texture0 ? texture0->idTexture : 0);
                GLUniform1i(samplerHandle0, 0);
                GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pBufferId->bs->vboIndexSubsetIB[i]);

                GLActiveTexture(GL_TEXTURE1);
                const TEXTURE* texture1 = pBufferId->getTextureByStage(1, 0);
                if (texture1)
                {
                    GLBindTexture(GL_TEXTURE_2D, texture1->idTexture);
                    GLUniform1i(samplerHandle1, 1);
                }
                else
                {
                    GLBindTexture(GL_TEXTURE_2D, 0);
                }
                GLDrawElements(pBufferId->mode_draw, pBufferId->indexCountIB[i], GL_UNSIGNED_SHORT, nullptr);
            }
        }
        else // Vertex buffer
        {
            if (!pBufferId->bs->vboVertexSubsetVB)
                return false;
            GLUseProgram(this->programObject);
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->bs->vboVertexSubsetVB[i]);
                GLEnableVertexAttribArray(this->positionHandle);
                GLVertexAttribPointer(this->positionHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
                //-----------------------------------------------------------------------------------------------------------
                if (this->normalHandle != -1) // Normal  (nem sempre temos normal nos shaders)
                {
                    GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->bs->vboNormalSubsetVB[i]);
                    GLEnableVertexAttribArray(this->normalHandle);
                    GLVertexAttribPointer(this->normalHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
                }
                //-----------------------------------------------------------------------------------------------------------
                GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->bs->vboTextureSubsetVB[i]);
                GLEnableVertexAttribArray(this->texCoordHandle);
                GLVertexAttribPointer(this->texCoordHandle, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
                //-----------------------------------------------------------------------------------------------------------
                GLUniformMatrix4fv(this->mvpMatrixHandle, 1, GL_FALSE, mvpMatrix.p);
                GLUniformMatrix4fv(this->mvMatrixHandle, 1, GL_FALSE, modelView.p);
                //-----------------------------------------------------------------------------------------------------------
                GLActiveTexture(GL_TEXTURE0);
                // if(pBufferId->hasColorKeying[i])
                //  glEnable(GL_BLEND);
                const TEXTURE* texture0 = pBufferId->getTextureByStage(0, i);
                GLBindTexture(GL_TEXTURE_2D, texture0 ? texture0->idTexture : 0);
                GLUniform1i(samplerHandle0, 0);

                GLActiveTexture(GL_TEXTURE1);
                const TEXTURE* texture1 = pBufferId->getTextureByStage(0, i);
                if (texture1)
                {
                    GLBindTexture(GL_TEXTURE_2D, texture1->idTexture);
                    GLUniform1i(samplerHandle1, 1);
                }
                else
                {
                    GLBindTexture(GL_TEXTURE_2D, 0);
                }

                GLDrawArrays(pBufferId->mode_draw, 0, pBufferId->vertexCountVB[i]);
            }
        }
        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        return true;
    }

    bool SHADER::renderDynamic(const BUFFER_GL *pBufferId,const VEC3 *vertex,const VEC3 *normal,const VEC2 *uv) const
    {
		GLCullFace(pBufferId->mode_cull_face);//GL_FRONT, GL_BACK, GL_FRONT_AND_BACK (CullFaceMode)
		GLFrontFace(pBufferId->mode_front_face_direction);//GL_CCW, GL_CW (FrontFaceDirection)

        if (pBufferId->isIndexBuffer()) // Index buffer
        {
            if (!pBufferId->bs->vboIndexSubsetIB)
                return false;
            GLUseProgram(this->programObject);
            //-----------------------------------------------------------------------------------------------------------
            GLEnableVertexAttribArray(this->positionHandle);
            GLVertexAttribPointer(this->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), vertex);
            //-----------------------------------------------------------------------------------------------------------
            if (this->normalHandle != -1)
            {
                GLEnableVertexAttribArray(this->normalHandle);
                GLVertexAttribPointer(this->normalHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), normal);
            }
            //-----------------------------------------------------------------------------------------------------------
            GLEnableVertexAttribArray(this->texCoordHandle);
            GLVertexAttribPointer(this->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2), uv);
            //-----------------------------------------------------------------------------------------------------------
            GLUniformMatrix4fv(this->mvpMatrixHandle, 1, GL_FALSE, mvpMatrix.p);
            GLUniformMatrix4fv(this->mvMatrixHandle, 1, GL_FALSE, modelView.p);
            //-----------------------------------------------------------------------------------------------------------
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                GLActiveTexture(GL_TEXTURE0);
                const TEXTURE * texture0 = pBufferId->getTextureByStage(0, i);
                GLBindTexture(GL_TEXTURE_2D, texture0 ? texture0->idTexture : 0);
                GLUniform1i(samplerHandle0, 0);
                GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pBufferId->bs->vboIndexSubsetIB[i]);

                const TEXTURE * texture1 = pBufferId->getTextureByStage(1, 0);
                GLActiveTexture(GL_TEXTURE1);
                if (texture1)
                {
                    GLBindTexture(GL_TEXTURE_2D, texture1->idTexture);
                    GLUniform1i(samplerHandle1, 1);
                }
                else
                {
                    GLBindTexture(GL_TEXTURE_2D, 0);
                }
                GLDrawElements(pBufferId->mode_draw, pBufferId->indexCountIB[i], GL_UNSIGNED_SHORT, nullptr);
            }
        }
        else // Vertex buffer
        {
            if (!pBufferId->vertexCountVB)
                return false;
            GLUseProgram(this->programObject);
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                GLEnableVertexAttribArray(this->positionHandle);
                GLVertexAttribPointer(this->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), vertex);
                //-----------------------------------------------------------------------------------------------------------
                if (this->normalHandle != -1) // Normal  (nem sempre temos normal nos shaders)
                {
                    GLEnableVertexAttribArray(this->normalHandle);
                    GLVertexAttribPointer(this->normalHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), normal);
                }
                //-----------------------------------------------------------------------------------------------------------
                GLEnableVertexAttribArray(this->texCoordHandle);
                GLVertexAttribPointer(this->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2), uv);
                //-----------------------------------------------------------------------------------------------------------
                GLUniformMatrix4fv(this->mvpMatrixHandle, 1, GL_FALSE, mvpMatrix.p);
                GLUniformMatrix4fv(this->mvMatrixHandle, 1, GL_FALSE, modelView.p);
                //-----------------------------------------------------------------------------------------------------------
                GLActiveTexture(GL_TEXTURE0);
                const TEXTURE * texture0 = pBufferId->getTextureByStage(0, i);
                GLBindTexture(GL_TEXTURE_2D, texture0 ? texture0->idTexture : 0);
                GLUniform1i(samplerHandle0, 0);

                GLActiveTexture(GL_TEXTURE1);
                const TEXTURE * texture1 = pBufferId->getTextureByStage(1, 0);
                if (texture1)
                {
                    GLBindTexture(GL_TEXTURE_2D, texture1->idTexture);
                    GLUniform1i(samplerHandle1, 1);
                }
                else
                {
                    GLBindTexture(GL_TEXTURE_2D, 0);
                }

                GLDrawArrays(pBufferId->mode_draw, 0, pBufferId->vertexCountVB[i]);
            }
        }
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        return true;
    }

    uint32_t SHADER::compileCodeShader(const unsigned int type, const char *shaderSrc)
    {
        uint32_t shader;
        int          compiled;
        // Create the shader object
        shader = GLCreateShader(type);
        if (shader == 0)
        {
            PRINT_IF_DEBUG("GLCreateShader returned 0");
            return 0;
        }
        // Load the shader source
        GLShaderSource(shader, 1, &shaderSrc, nullptr);
        // Compile the shader
        GLCompileShader(shader);
        // Check the compile status
        GLGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled)
        {
            PRINT_IF_DEBUG("failed on compile shader [%s]",shaderSrc ? shaderSrc : "null");
            GLint infoLen = 0;
            GLGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen > 1)
            {
                auto *infoLog = static_cast<char *>(malloc(sizeof(char) * static_cast<size_t>(infoLen)));
                GLGetShaderInfoLog(shader, infoLen, nullptr, infoLog);
                PRINT_IF_DEBUG("Error compiling shader:%s\n%s\n",
                             this->pShader ? this->pShader->fileName.c_str() : "nullptr", infoLog);
                free(infoLog);
            }
            GLDeleteShader(shader);
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
        // Load the vertex/fragment shaders
        vertexShader = compileCodeShader(GL_VERTEX_SHADER, vertShaderSrc);
        if (vertexShader == 0)
        {
            PRINT_IF_DEBUG("vertexShader == 0");
            return 0;
        }
        fragmentShader = compileCodeShader(GL_FRAGMENT_SHADER, fragShaderSrc);
        if (fragmentShader == 0)
        {
            PRINT_IF_DEBUG("fragmentShader == 0");
            GLDeleteShader(vertexShader);
            return 0;
        }
        // Create the program object
        this->programObject = GLCreateProgram();
        if (programObject == 0)
        {
            PRINT_IF_DEBUG("Failed to create programObject");
            return 0;
        }
        GLAttachShader(programObject, vertexShader);
        GLAttachShader(programObject, fragmentShader);
        // Link the program
        GLLinkProgram(programObject);
        // Check the link status
        GLGetProgramiv(programObject, GL_LINK_STATUS, &linked);
        if (!linked)
        {
            GLDeleteShader(vertexShader);
            GLDeleteShader(fragmentShader);
            PRINT_IF_DEBUG("linked status failed");
            GLint infoLen = 0;
            GLGetProgramiv(programObject, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen > 1)
            {
                auto *infoLog = static_cast<char *>(malloc(sizeof(char) * static_cast<size_t>(infoLen)));
                GLGetProgramInfoLog(programObject, infoLen, nullptr, infoLog);
                PRINT_IF_DEBUG("Error linking program:\n%s\n", infoLog);
                free(infoLog);
            }
            GLDeleteProgram(programObject);
            programObject = 0;
            return 0;
        }
        // Free up no longer needed shader resources
        GLDeleteShader(vertexShader);
        GLDeleteShader(fragmentShader);
        return programObject;
    }
}

#endif // USE_OPENGL_ES