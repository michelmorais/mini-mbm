/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include <shader.h>
#include <gles-debug.h>
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
               isIndexBuffer(false),
               mode_draw(GL_TRIANGLES),
               mode_cull_face(GL_BACK),
               mode_front_face_direction(GL_CW)
    {
        memset(vboVertNorTexIB, 0, sizeof(vboVertNorTexIB));
    }

    BUFFER_GL::~BUFFER_GL()
    {
        this->release();
    }

    bool BUFFER_GL::isLoadedBuffer() const
    {
        return this->totalSubset != 0;
    }

    void BUFFER_GL::release()
    {
        if (vboVertNorTexIB[0])
        {
            GLDeleteBuffers(3, vboVertNorTexIB);
        }
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
        GLGenBuffers(static_cast<GLsizei>(this->totalSubset), this->vboVertexSubsetVB);
        if (!this->vboVertexSubsetVB[0])
        {
            this->release();
            return false;
        }

        if (normal)
        {
            GLGenBuffers(static_cast<GLsizei>(this->totalSubset), this->vboNormalSubsetVB);
        }

        if (uv)
        {
            GLGenBuffers(static_cast<GLsizei>(this->totalSubset), this->vboTextureSubsetVB);
        }
        for (uint32_t i = 0; i < totalSubset; ++i)
        {
            vertexStartVB[i] = vertexStartSubset[i];
            vertexCountVB[i] = vertexCountSubset[i];
            GLBindBuffer(GL_ARRAY_BUFFER, this->vboVertexSubsetVB[i]);
            GLBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(mbm::VEC3) *  static_cast<size_t>(vertexCountVB[i])), &vertex[vertexStartVB[i]],GL_STATIC_DRAW);
            if (normal)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, this->vboNormalSubsetVB[i]);
                GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeof(mbm::VEC3) * static_cast<size_t>(vertexCountVB[i])), &normal[vertexStartVB[i]],
                             GL_STATIC_DRAW);
            }
            if (uv)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, this->vboTextureSubsetVB[i]);
                GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeof(mbm::VEC2) * static_cast<size_t>(vertexCountVB[i])), &uv[vertexStartVB[i]],
                             GL_STATIC_DRAW);
            }
        }
        GLBindBuffer(GL_ARRAY_BUFFER, 0);
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
        GLGenBuffers(3, this->vboVertNorTexIB);
        if (this->vboVertNorTexIB[0] == 0)
            return false;
        this->totalSubset      = totalSubsets;
        this->vboIndexSubsetIB = new uint32_t[totalSubset];
        this->indexStartIB     = new int[totalSubset];
        this->indexCountIB     = new int[totalSubset];
        memset(this->vboIndexSubsetIB, 0, sizeof(uint32_t) * static_cast<size_t>(totalSubset));
        GLGenBuffers(static_cast<GLsizei>(this->totalSubset), this->vboIndexSubsetIB);
        if (!this->vboIndexSubsetIB[0])
        {
            this->release();
            return false;
        }

        GLBindBuffer(GL_ARRAY_BUFFER, this->vboVertNorTexIB[0]);
        GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeOfArrayVertex * sizeof(mbm::VEC3)), vertex, GL_STATIC_DRAW);

        if (normal)
        {
            GLBindBuffer(GL_ARRAY_BUFFER, this->vboVertNorTexIB[1]);
            GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeOfArrayVertex * sizeof(mbm::VEC3)), normal, GL_STATIC_DRAW);
        }

        if (uv)
        {
            GLBindBuffer(GL_ARRAY_BUFFER, this->vboVertNorTexIB[2]);
            GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeOfArrayVertex * sizeof(mbm::VEC2)), uv, GL_STATIC_DRAW);
        }

        for (uint32_t i = 0; i < this->totalSubset; ++i)
        {
            this->indexStartIB[i] = indexStartSubset[i];
            this->indexCountIB[i] = indexCountSubset[i];
            GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexSubsetIB[i]);
            GLBufferData(GL_ELEMENT_ARRAY_BUFFER,static_cast<GLsizeiptr>(sizeof(unsigned short) * static_cast<size_t>(this->indexCountIB[i])),&arrayIndices[this->indexStartIB[i]], GL_STATIC_DRAW);
        }

        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
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
        GLGenBuffers(static_cast<GLsizei>(this->totalSubset), this->vboIndexSubsetIB);
        if (!this->vboIndexSubsetIB[0])
        {
            this->release();
            return false;
        }

        for (uint32_t i = 0; i < this->totalSubset; ++i)
        {
            this->indexStartIB[i] = indexStartSubset[i];
            this->indexCountIB[i] = indexCountSubset[i];
            GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->vboIndexSubsetIB[i]);
            GLBufferData(GL_ELEMENT_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeof(uint16_t) * static_cast<size_t>(this->indexCountIB[i])),&arrayIndices[this->indexStartIB[i]], GL_STATIC_DRAW);
        }

        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
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

    BASE_SHADER::BASE_SHADER() noexcept
    = default;

    BASE_SHADER::~BASE_SHADER()
    {
        this->releaseVars();
        this->fileName.clear();
        this->stringCodeShader.clear();
    }

    const char * BASE_SHADER::getCode()
    {
        return this->stringCodeShader.c_str();
    }

    VAR_SHADER * BASE_SHADER::getVarByName(const char *nameVar)
    {
        if (nameVar == nullptr)
            return nullptr;
        std::vector<VAR_SHADER *>::size_type s = lsVar.size();
        for (std::vector<VAR_SHADER *>::size_type i = 0; i < s; ++i)
        {
            VAR_SHADER *var = lsVar[i];
            if (strcmp(var->name.c_str(), nameVar) == 0)
                return var;
        }
        return nullptr;
    }

    VAR_SHADER * BASE_SHADER::getVar(const uint32_t indexVar)
    {
        if (indexVar < static_cast<uint32_t>(lsVar.size()))
            return lsVar[static_cast<std::vector<VAR_SHADER *>::size_type>(indexVar)];
        return nullptr;
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

    uint32_t BASE_SHADER::getTotalVar() const noexcept
    {
        return static_cast<uint32_t>(lsVar.size());
    }

    void BASE_SHADER::releaseVars()
    {
        const std::vector<VAR_SHADER *>::size_type s = lsVar.size();
        for (std::vector<VAR_SHADER *>::size_type i = 0; i < s; ++i)
        {
            VAR_SHADER *var = lsVar[i];
            if (var)
                delete var;
            var      = nullptr;
            lsVar[i] = nullptr;
        }
        lsVar.clear();
    }

    bool BASE_SHADER::loadShader(const char *fileNameShaderVS_PS, const char *code)
    {
        this->stringCodeShader.clear();
        this->fileName.clear();
        if (fileNameShaderVS_PS && code)
        {
            this->fileName         = fileNameShaderVS_PS;
            this->stringCodeShader = code;
            return true;
        }
        return false;
    }

    std::vector<VAR_SHADER*> * BASE_SHADER::getVars()
    {
        return &this->lsVar;
    }

    bool BASE_SHADER::isThereVarIntoLsVars(const char *nameVar)
    {
        if (nameVar == nullptr)
            return false;
        const std::vector<VAR_SHADER *>::size_type s = lsVar.size();
        for (std::vector<VAR_SHADER *>::size_type i = 0; i < s; ++i)
        {
            VAR_SHADER *var = lsVar[i];
            if (var && strcmp(var->name.c_str(), nameVar) == 0)
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

    SHADER::SHADER() noexcept : programObject(0),
                                  mvpMatrixHandle(-1),
                                  mvMatrixHandle(-1),
                                  positionHandle(-1),
                                  texCoordHandle(-1),
                                  samplerHandle0(-1),
                                  samplerHandle1(-1),
                                  normalHandle(-1),
                                  pShader(nullptr),
                                  vShader(nullptr)
    {
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

    void SHADER::onRestore() // Libera o pShader da memória e pode ser carregado novamente
    {
        this->mvpMatrixHandle = -1;
        this->mvMatrixHandle  = -1;
        this->positionHandle  = -1;
        this->texCoordHandle  = -1;
        this->samplerHandle0  = -1;
        this->samplerHandle1  = -1;
        this->normalHandle    = -1;
        this->programObject   = 0;
        this->pShader         = nullptr;
        this->vShader         = nullptr;
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

    bool SHADER::isLoad()
    {
        return this->programObject != 0;
    }

    bool SHADER::render(const BUFFER_GL *pBufferId) const
    {
		GLCullFace(pBufferId->mode_cull_face);//GL_FRONT 1028, GL_BACK 1029, GL_FRONT_AND_BACK 1032(CullFaceMode)
		GLFrontFace(pBufferId->mode_front_face_direction);//GL_CCW 2305 , GL_CW 2304(FrontFaceDirection)
		
        if (pBufferId->isIndexBuffer) // Index buffer
        {
            if (!pBufferId->vboVertNorTexIB[0])
                return false;
            GLUseProgram(this->programObject);
            //-----------------------------------------------------------------------------------------------------------
            GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->vboVertNorTexIB[0]);
            GLEnableVertexAttribArray(this->positionHandle);
            GLVertexAttribPointer(this->positionHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            //-----------------------------------------------------------------------------------------------------------
            if (this->normalHandle != -1) // Normal (nem sempre temos normal nos shaders)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->vboVertNorTexIB[1]);
                GLEnableVertexAttribArray(this->normalHandle);
                GLVertexAttribPointer(this->normalHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            }
            //-----------------------------------------------------------------------------------------------------------
            GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->vboVertNorTexIB[2]);
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
                GLBindTexture(GL_TEXTURE_2D, pBufferId->idTexture0[i]);
                GLUniform1i(samplerHandle0, 0);
                GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pBufferId->vboIndexSubsetIB[i]);

                GLActiveTexture(GL_TEXTURE1);
                if (pBufferId->idTexture1)
                {
                    GLBindTexture(GL_TEXTURE_2D, pBufferId->idTexture1);
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
            if (!pBufferId->vboVertexSubsetVB)
                return false;
            GLUseProgram(this->programObject);
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->vboVertexSubsetVB[i]);
                GLEnableVertexAttribArray(this->positionHandle);
                GLVertexAttribPointer(this->positionHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
                //-----------------------------------------------------------------------------------------------------------
                if (this->normalHandle != -1) // Normal  (nem sempre temos normal nos shaders)
                {
                    GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->vboNormalSubsetVB[i]);
                    GLEnableVertexAttribArray(this->normalHandle);
                    GLVertexAttribPointer(this->normalHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
                }
                //-----------------------------------------------------------------------------------------------------------
                GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->vboTextureSubsetVB[i]);
                GLEnableVertexAttribArray(this->texCoordHandle);
                GLVertexAttribPointer(this->texCoordHandle, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
                //-----------------------------------------------------------------------------------------------------------
                GLUniformMatrix4fv(this->mvpMatrixHandle, 1, GL_FALSE, mvpMatrix.p);
                GLUniformMatrix4fv(this->mvMatrixHandle, 1, GL_FALSE, modelView.p);
                //-----------------------------------------------------------------------------------------------------------
                GLActiveTexture(GL_TEXTURE0);
                // if(pBufferId->hasColorKeying[i])
                //  glEnable(GL_BLEND);
                GLBindTexture(GL_TEXTURE_2D, pBufferId->idTexture0[i]);
                GLUniform1i(samplerHandle0, 0);

                GLActiveTexture(GL_TEXTURE1);
                if (pBufferId->idTexture1)
                {
                    GLBindTexture(GL_TEXTURE_2D, pBufferId->idTexture1);
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

        if (pBufferId->isIndexBuffer) // Index buffer
        {
            if (!pBufferId->vboIndexSubsetIB)
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
                GLBindTexture(GL_TEXTURE_2D, pBufferId->idTexture0[i]);
                GLUniform1i(samplerHandle0, 0);
                GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pBufferId->vboIndexSubsetIB[i]);

                GLActiveTexture(GL_TEXTURE1);
                if (pBufferId->idTexture1)
                {
                    GLBindTexture(GL_TEXTURE_2D, pBufferId->idTexture1);
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
                GLBindTexture(GL_TEXTURE_2D, pBufferId->idTexture0[i]);
                GLUniform1i(samplerHandle0, 0);

                GLActiveTexture(GL_TEXTURE1);
                if (pBufferId->idTexture1)
                {
                    GLBindTexture(GL_TEXTURE_2D, pBufferId->idTexture1);
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

    void SHADER::update()
    {
        if (this->pShader)
            this->pShader->update(this->programObject);
#if defined _DEBUG
        else if (this->programObject == 0)
            PRINT_IF_DEBUG("missed shader!");
#endif
        if (this->vShader)
            this->vShader->update(this->programObject);
#if defined _DEBUG
        else if (this->programObject == 0)
            PRINT_IF_DEBUG("missed shader!");
#endif
    }

    uint32_t SHADER::compileCodeShader(const GLenum type, const char *shaderSrc)
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

    mbm::MATRIX mbm::SHADER::modelView; // Matrix do modelo (ModelView)
    mbm::MATRIX mbm::SHADER::mvpMatrix; // ModelView x projection (perspectiva) (automaticamente setada)
}
