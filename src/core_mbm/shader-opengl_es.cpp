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
#include <specific-opengl_es.h>
#include <util-interface.h>
#include <shader-var-cfg.h>
#include <draw-compatibility.h>
#include <header-mesh.h>
#include <particle-control.h>

namespace mbm
{
    static uint32_t loadShaderProgram(BASE_SHADER* pShader, BASE_SHADER* vShader, void* ptrShaderSpecific, const char* vertShaderSrc, const char* fragShaderSrc);
    static uint32_t compileCodeShader(BASE_SHADER* ptrShader, const unsigned int type, const char* shaderSrc);

    static GLenum getOpenGlEsModeDraw(const uint32_t mode_draw)
    {
        switch (mode_draw)
        {
            case util::MODE_DRAW_POINTS:
            {
                return GL_POINTS;
            }
            break;
            case util::MODE_DRAW_LINES:
            {
                return GL_LINES;
            }
            break;
            case util::MODE_DRAW_LINE_LOOP:
            {
                return GL_LINE_LOOP;
            }
            break;
            case util::MODE_DRAW_LINE_STRIP:
            {
                return GL_LINE_STRIP;
            }
            break;
            case util::MODE_DRAW_TRIANGLES:
            {
                return GL_TRIANGLES;
            }
            break;
            case util::MODE_DRAW_TRIANGLE_STRIP:
            {
                return GL_TRIANGLE_STRIP;
            }
            break;
            case util::MODE_DRAW_TRIANGLE_FAN:
            {
                return GL_TRIANGLE_FAN;
            }
            break;
            default:
            {
                ERROR_AT(__LINE__, __FILE__, "Mode draw for OpenGlEs incorrect!, returning GL_TRIANGLES");
            }
            break;
        }
        return GL_TRIANGLES;
    }

    // Disable vertex attrib arrays not used for this draw so ANGLE (and strict drivers) do not see
    // stale enabled state from another program (e.g. ImGui) and raise GL_INVALID_OPERATION.
    static void disableUnusedVertexAttribs(const GLES_PS_VS* gles, bool useNormal, bool useTexCoord)
    {
        GLint maxAttribs = 0;
        glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxAttribs);
        const int pos = gles->positionHandle;
        const int norm = useNormal && gles->normalHandle >= 0 ? gles->normalHandle : -1;
        const int tex = useTexCoord && gles->texCoordHandle >= 0 ? gles->texCoordHandle : -1;
        for (GLint a = 0; a < maxAttribs; ++a)
        {
            if (a != pos && a != norm && a != tex)
                glDisableVertexAttribArray(static_cast<GLuint>(a));
        }
    }

    BUFFER_GL::BUFFER_GL():
        indexStartIB(nullptr),
        indexCountIB(nullptr),
        vertexStartVB(nullptr),
        vertexCountVB(nullptr),
        sizeOfArrayVertex(0),
        fvf(FVF_PROVIDE_BY_ENGINE::FVF_POS_UV),
        mode_draw(GL_TRIANGLES),
        mode_cull_face(GL_BACK),
        mode_front_face_direction(GL_CW),
        totalSubset(0),
        initializedIndexBuffer(false),
        texture1(nullptr)
    {
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
        const uint32_t totalSubsets,const int *vertexStartSubset,const int *vertexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode, const bool isDynamic)
    {
        this->release();
        if (!vertex || !sizeOfArrayVertex || !totalSubsets || !vertexStartSubset || !vertexCountSubset)
            return false;
        const GLenum usage           = isDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW;
        this->totalSubset            = totalSubsets;
        this->bs->vboVertexSubsetVB  = new uint32_t[totalSubset];
        this->bs->vboNormalSubsetVB  = new uint32_t[totalSubset];
        this->bs->vboTextureSubsetVB = new uint32_t[totalSubset];
        this->initializeVertexBufferControl(totalSubsets, sizeOfArrayVertex, vertexStartSubset, vertexCountSubset, info_draw_mode);
        this->fvf = (normal && uv) ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV : (normal ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR : (uv ? FVF_PROVIDE_BY_ENGINE::FVF_POS_UV : FVF_PROVIDE_BY_ENGINE::FVF_POS));
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
            GLBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(sizeof(mbm::VEC3) *  static_cast<size_t>(this->vertexCountVB[i])), &vertex[this->vertexStartVB[i]], usage);
            if (normal)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, this->bs->vboNormalSubsetVB[i]);
                GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeof(mbm::VEC3) * static_cast<size_t>(this->vertexCountVB[i])), &normal[this->vertexStartVB[i]],usage);
            }
            if (uv)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, this->bs->vboTextureSubsetVB[i]);
                GLBufferData(GL_ARRAY_BUFFER,static_cast<GLsizeiptr>( sizeof(mbm::VEC2) * static_cast<size_t>(this->vertexCountVB[i])), &uv[this->vertexStartVB[i]],usage);
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
        this->initializeIndexBufferControl(totalSubsets, sizeOfArrayVertex, indexStartSubset, indexCountSubset, info_draw_mode);
        this->fvf = (normal && uv) ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV : (normal ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR : (uv ? FVF_PROVIDE_BY_ENGINE::FVF_POS_UV : FVF_PROVIDE_BY_ENGINE::FVF_POS));
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

    bool BUFFER_GL::loadBufferDynamic(const uint16_t *arrayIndices, 
                                      const unsigned int totalSubsets, 
                                      const int *indexStartSubset,
                                      const int *indexCountSubset,
                                      const bool hasNormal,
                                      const bool hasUv,
                                      const util::INFO_DRAW_MODE * info_draw_mode)
    {
        release();
        if (!arrayIndices || !totalSubsets || !indexStartSubset || !indexCountSubset)
            return false;
        this->totalSubset      = totalSubsets;
        this->bs->vboIndexSubsetIB = new uint32_t[totalSubset];
        memset(this->bs->vboIndexSubsetIB, 0, sizeof(uint32_t) * totalSubset);
        this->initializeIndexBufferControl(totalSubsets, sizeOfArrayVertex, indexStartSubset, indexCountSubset, info_draw_mode);
        this->fvf = (hasNormal && hasUv) ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV : (hasNormal ? FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR : (hasUv ? FVF_PROVIDE_BY_ENGINE::FVF_POS_UV : FVF_PROVIDE_BY_ENGINE::FVF_POS));
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
            const mbm::VEC2* pUvStart     = uv ? &uv[vertexStart]         : nullptr;
            if (this->initializedIndexBuffer)
            {
                // TODO: for index buffer
                ERROR_AT(__LINE__, __FILE__, "TODO: Update vertex not implemented for index buffer");
                return false;
            }
            else
            {
                GLBindBuffer(GL_ARRAY_BUFFER, this->bs->vboVertexSubsetVB[i]);
                GLBufferData(GL_ARRAY_BUFFER, sizeof(mbm::VEC3) * vertexCount, pVertexStart, GL_DYNAMIC_DRAW);
                if (pNormalStart && this->bs->vboNormalSubsetVB[i] != 0)
                {
                    GLBindBuffer(GL_ARRAY_BUFFER, this->bs->vboNormalSubsetVB[i]);
                    GLBufferData(GL_ARRAY_BUFFER, sizeof(mbm::VEC3) * vertexCount, pNormalStart, GL_DYNAMIC_DRAW);
                }
                if (pUvStart && this->bs->vboTextureSubsetVB[i] != 0)
                {
                    GLBindBuffer(GL_ARRAY_BUFFER, this->bs->vboTextureSubsetVB[i]);
                    GLBufferData(GL_ARRAY_BUFFER, sizeof(mbm::VEC2) * vertexCount, pUvStart, GL_DYNAMIC_DRAW);
                }
            }
        }
        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        return true;
    }

    bool BUFFER_GL::loadParticleBuffer()// type index buffer only, must be implemented by specific backend engine
    {
        release();
        const uint16_t arrayIndices[6] = { 0, 1, 2, 2, 1, 3 };
        constexpr GLsizeiptr sizeIndexBuffer = sizeof(arrayIndices);
        constexpr int indexStartSubset = 0;
        constexpr int indexCountSubset = sizeof(arrayIndices) / sizeof(uint16_t);
        constexpr uint32_t sizeOfArrayVertex = 0;

        this->totalSubset = 1;
        this->bs->vboIndexSubsetIB = new uint32_t[this->totalSubset];

        this->initializeIndexBufferControl(this->totalSubset, sizeOfArrayVertex, &indexStartSubset, &indexCountSubset, nullptr);
        memset(this->bs->vboIndexSubsetIB, 0, sizeof(uint32_t) * static_cast<size_t>(totalSubset));
        GLGenBuffers(static_cast<GLsizei>(this->totalSubset), this->bs->vboIndexSubsetIB);
        if (!this->bs->vboIndexSubsetIB[0])
        {
            this->release();
            return false;
        }

        for (uint32_t i = 0; i < this->totalSubset; ++i)
        {
            GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->bs->vboIndexSubsetIB[i]);
            GLBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeIndexBuffer, arrayIndices, GL_STATIC_DRAW);
        }

        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
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
            const GLES_PS_VS* gles_shaderSpecific = static_cast<const GLES_PS_VS*>(ptrShaderSpecific);
            auto var       = new VAR_SHADER(std::string(nameVar), typeVar, isPS);
            int32_t *handleVar = static_cast<int32_t*>(var->ptrHandleVar);
            *handleVar = GLGetUniformLocation(gles_shaderSpecific->programObject, nameVar);
            if (*handleVar == -1)
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

    void BASE_SHADER::update(void* ptrShaderSpecific) const
    {
        const GLES_PS_VS* gles_shaderSpecific = static_cast<const GLES_PS_VS*>(ptrShaderSpecific);
        if (gles_shaderSpecific->programObject == 0)
            return;
        GLUseProgram(gles_shaderSpecific->programObject);
        const std::vector<VAR_SHADER *>::size_type s = lsVar.size();
        for (std::vector<VAR_SHADER *>::size_type i = 0; i < s; ++i)
        {
            VAR_SHADER *var = lsVar[i];
            if (var)
            {
                const int32_t handleVar = *static_cast<int32_t*>(var->ptrHandleVar);
                switch (var->typeVar)
                {
                    // Uniform
                    case VAR_FLOAT: { GLUniform1f(handleVar, var->current[0]);
                    }
                    break;
                    case VAR_VECTOR2: { GLUniform2f(handleVar, var->current[0], var->current[1]);
                    }
                    break;
                    case VAR_COLOR_RGB:
                    case VAR_VECTOR: { GLUniform3f(handleVar, var->current[0], var->current[1], var->current[2]);
                    }
                    break;
                    case VAR_COLOR_RGBA:
                    {
                        GLUniform4f(handleVar, var->current[0], var->current[1], var->current[2], var->current[3]);
                    }
                    break;
                    default: {
                    }
                    break;
                }
            }
        }
    }

    GLES_PS_VS::GLES_PS_VS() noexcept
        : positionHandle(-1),
          texCoordHandle(-1),
          normalHandle(-1),
          mvpMatrixHandle(-1),
          mvMatrixHandle(-1),
          samplerHandle0(-1),
          samplerHandle1(-1),
          programObject(0)
    {
	}

    GLES_PS_VS::~GLES_PS_VS()
    {
        release();
    }

    void GLES_PS_VS::release() noexcept
    {
        positionHandle  = -1;
        texCoordHandle  = -1;
        normalHandle    = -1;
        mvpMatrixHandle = -1;
        mvMatrixHandle  = -1;
        samplerHandle0  = -1;
        samplerHandle1  = -1;
        if (programObject)
        {
            GLDeleteProgram(programObject);
        }
        programObject  = 0;
    }

    SHADER::SHADER() : ptrShaderSpecific(new GLES_PS_VS()),
        pShader(nullptr),
        vShader(nullptr)
    {
    }

    SHADER::~SHADER()
    {
        // Deleting a void* pointer directly in C++ is undefined behavior and should be avoided. 
        delete static_cast<GLES_PS_VS*>(ptrShaderSpecific);
    }

    void SHADER::onRestore() // Libera o pShader da memória e pode ser carregado novamente
    {
        static_cast<GLES_PS_VS*>(ptrShaderSpecific)->release();//TODO: check this: maybe only attribute 0 is enough
        this->pShader            = nullptr;
        this->vShader            = nullptr;
    }

    void SHADER::releaseShader()
    {
        static_cast<GLES_PS_VS*>(ptrShaderSpecific)->release();
        this->pShader            = nullptr;
        this->vShader            = nullptr;
    }

    bool SHADER::isLoad() const noexcept
    {
        return static_cast<const GLES_PS_VS*>(ptrShaderSpecific)->programObject != 0;
    }

    bool SHADER::compileShader(mbm::BASE_SHADER *ptrPshader, mbm::BASE_SHADER *ptrVshader, mbm::FVF_PROVIDE_BY_ENGINE fvf)
    {
        if (fvf == FVF_PROVIDE_BY_ENGINE::FVF_NONE)
            return false;
        this->pShader             = ptrPshader;
        this->vShader            = ptrVshader;
        const bool hasNormal = (fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR || fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);
        const bool hasUV = (fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_UV || fvf == FVF_PROVIDE_BY_ENGINE::FVF_POS_NOR_UV);

        std::string defaultCodePs;
        if (hasUV)
        {
            defaultCodePs = "precision mediump float;"
                "varying vec2 vTexCoord;"
                "uniform sampler2D sample0;"
                "void main() { gl_FragColor = texture2D(sample0, vTexCoord); }";
        }
        else
        {
            defaultCodePs = "precision mediump float;"
                "void main() { gl_FragColor = vec4(1.0, 1.0, 1.0, 1.0); }";
        }

        std::string defaultCodeVs = "attribute vec4 aPosition;";
        if (hasNormal) defaultCodeVs += " attribute vec3 aNormal;";
        if (hasUV) defaultCodeVs += " attribute vec2 aTextCoord;";
        defaultCodeVs += " uniform mat4 mvpMatrix;";
        if (hasUV) defaultCodeVs += " varying vec2 vTexCoord;";
        defaultCodeVs += " void main() { gl_Position = mvpMatrix * aPosition;";
        if (hasUV) defaultCodeVs += " vTexCoord = aTextCoord;";
        defaultCodeVs += " }";
        GLES_PS_VS* gles_shaderSpecific = static_cast<GLES_PS_VS*>(ptrShaderSpecific);
        if (gles_shaderSpecific->programObject)
        {
            PRINT_IF_DEBUG("programObject already has a value [%d]", gles_shaderSpecific->programObject);
            return true;
        }
        if (this->pShader == nullptr && this->vShader == nullptr)
        {
            if (!loadShaderProgram(this->pShader, this->vShader, ptrShaderSpecific, defaultCodeVs.c_str(), defaultCodePs.c_str()))
                return false;
        }
        else if (this->pShader == nullptr && this->vShader)
        {
            if (!loadShaderProgram(this->pShader, this->vShader, ptrShaderSpecific, this->vShader->getCode(), defaultCodePs.c_str()))
                return false;
        }
        else if (this->pShader && this->vShader == nullptr)
        {
            if (!loadShaderProgram(this->pShader, this->vShader, ptrShaderSpecific, defaultCodeVs.c_str(), this->pShader->getCode()))
                return false;
        }
        else if (this->pShader && this->vShader)
        {
            if (!loadShaderProgram(this->pShader, this->vShader, ptrShaderSpecific, this->vShader->getCode(), this->pShader->getCode()))
                return false;
        }

        // In OpenGL, a uniform location of - 1 means "not found", 
        // but a handle of 0 suggests GLGetUniformLocation() isn't finding the uniform in your shader.

        const std::string vertexShaderCode(this->vShader ? this->vShader->getCode() : defaultCodeVs);
        const std::string pixelShaderCode(this->pShader ? this->pShader->getCode() : defaultCodePs);
        const std::string bothShaderCode(pixelShaderCode + vertexShaderCode);

        if (bothShaderCode.find("aPosition") != std::string::npos)
        {
            gles_shaderSpecific->positionHandle = GLGetAttribLocation(gles_shaderSpecific->programObject, "aPosition");
        }
        if (bothShaderCode.find("mvpMatrix") != std::string::npos)
        {
            gles_shaderSpecific->mvpMatrixHandle = GLGetUniformLocation(gles_shaderSpecific->programObject, "mvpMatrix");
        }
        if (bothShaderCode.find("mvMatrix") != std::string::npos)
        {
            gles_shaderSpecific->mvMatrixHandle = GLGetUniformLocation(gles_shaderSpecific->programObject, "mvMatrix");
        }
        if (vertexShaderCode.find("aNormal") != std::string::npos)
        {   // Attributes are vertex-only; use Optional - aNormal can be inactive if linker optimizes out unused varying
            gles_shaderSpecific->normalHandle = GLGetAttribLocationOptional(gles_shaderSpecific->programObject, "aNormal");
        }
        else
        {
            gles_shaderSpecific->normalHandle = -1;
        }
        if (bothShaderCode.find("aTextCoord") != std::string::npos)
        {
            gles_shaderSpecific->texCoordHandle = GLGetAttribLocation(gles_shaderSpecific->programObject, "aTextCoord");
        }
        else
        {
            gles_shaderSpecific->texCoordHandle = -1;
        }
        if (bothShaderCode.find("sample0") != std::string::npos)
        {
            gles_shaderSpecific->samplerHandle0 = GLGetUniformLocation(gles_shaderSpecific->programObject, "sample0");
        }
        if (bothShaderCode.find("sample1") != std::string::npos)
        {
            gles_shaderSpecific->samplerHandle1 = GLGetUniformLocation(gles_shaderSpecific->programObject, "sample1");
        }
        
        return true;
    }


    bool SHADER::render(const BUFFER_GL *pBufferId) const
    {
        const GLES_PS_VS* gles_shaderSpecific = static_cast<const GLES_PS_VS*>(ptrShaderSpecific);
        GLCullFace(pBufferId->mode_cull_face);//GL_FRONT 1028, GL_BACK 1029, GL_FRONT_AND_BACK 1032(CullFaceMode)
        GLFrontFace(pBufferId->mode_front_face_direction);//GL_CCW 2305 , GL_CW 2304(FrontFaceDirection)
        const GLenum modeDrawGl       = getOpenGlEsModeDraw(pBufferId->mode_draw);
        
        if (pBufferId->isIndexBuffer()) // Index buffer
        {
            if (!pBufferId->bs->vboVertNorTexIB[0])
                return false;
            GLUseProgram(gles_shaderSpecific->programObject);
            //-----------------------------------------------------------------------------------------------------------
            GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->bs->vboVertNorTexIB[0]);
            GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
            GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            //-----------------------------------------------------------------------------------------------------------
            if (gles_shaderSpecific->normalHandle != -1) // Normal (nem sempre temos normal nos shaders)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->bs->vboVertNorTexIB[1]);
                GLEnableVertexAttribArray(gles_shaderSpecific->normalHandle);
                GLVertexAttribPointer(gles_shaderSpecific->normalHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            }
            //-----------------------------------------------------------------------------------------------------------
            if (gles_shaderSpecific->texCoordHandle != -1)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->bs->vboVertNorTexIB[2]);
                GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
            }
            //-----------------------------------------------------------------------------------------------------------
            GLUniformMatrix4fv(gles_shaderSpecific->mvpMatrixHandle, 1, GL_FALSE, mvpMatrix.p);
            GLUniformMatrix4fv(gles_shaderSpecific->mvMatrixHandle, 1, GL_FALSE, modelView.p);
            //-----------------------------------------------------------------------------------------------------------
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                GLActiveTexture(GL_TEXTURE0);
                // if(pBufferId->hasColorKeying[i])
                //  glEnable(GL_BLEND);
                const TEXTURE* texture0 = pBufferId->getTextureByStage(0, i);
                GLBindTexture(GL_TEXTURE_2D, texture0 ? texture0->idTexture : 0);
                GLUniform1i(gles_shaderSpecific->samplerHandle0, 0);
                GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pBufferId->bs->vboIndexSubsetIB[i]);

                GLActiveTexture(GL_TEXTURE1);
                const TEXTURE* texture1 = pBufferId->getTextureByStage(1, 0);
                if (texture1)
                {
                    GLBindTexture(GL_TEXTURE_2D, texture1->idTexture);
                    GLUniform1i(gles_shaderSpecific->samplerHandle1, 1);
                }
                else
                {
                    GLBindTexture(GL_TEXTURE_2D, 0);
                }
                disableUnusedVertexAttribs(gles_shaderSpecific, gles_shaderSpecific->normalHandle != -1, gles_shaderSpecific->texCoordHandle != -1);
                GLDrawElements(modeDrawGl, pBufferId->indexCountIB[i], GL_UNSIGNED_SHORT, nullptr);
            }
        }
        else // Vertex buffer
        {
            if (!pBufferId->bs->vboVertexSubsetVB)
                return false;
            GLUseProgram(gles_shaderSpecific->programObject);
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->bs->vboVertexSubsetVB[i]);
                GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
                //-----------------------------------------------------------------------------------------------------------
                if (gles_shaderSpecific->normalHandle != -1 && pBufferId->bs->vboNormalSubsetVB && pBufferId->bs->vboNormalSubsetVB[i] != 0)
                {
                    GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->bs->vboNormalSubsetVB[i]);
                    GLEnableVertexAttribArray(gles_shaderSpecific->normalHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->normalHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
                }
                //-----------------------------------------------------------------------------------------------------------
                if (gles_shaderSpecific->texCoordHandle != -1 && pBufferId->bs->vboTextureSubsetVB && pBufferId->bs->vboTextureSubsetVB[i] != 0)
                {
                    GLBindBuffer(GL_ARRAY_BUFFER, pBufferId->bs->vboTextureSubsetVB[i]);
                    GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
                }
                //-----------------------------------------------------------------------------------------------------------
                GLUniformMatrix4fv(gles_shaderSpecific->mvpMatrixHandle, 1, GL_FALSE, mvpMatrix.p);
                GLUniformMatrix4fv(gles_shaderSpecific->mvMatrixHandle, 1, GL_FALSE, modelView.p);
                //-----------------------------------------------------------------------------------------------------------
                GLActiveTexture(GL_TEXTURE0);
                // if(pBufferId->hasColorKeying[i])
                //  glEnable(GL_BLEND);
                const TEXTURE* texture0 = pBufferId->getTextureByStage(0, i);
                GLBindTexture(GL_TEXTURE_2D, texture0 ? texture0->idTexture : 0);
                GLUniform1i(gles_shaderSpecific->samplerHandle0, 0);

                GLActiveTexture(GL_TEXTURE1);
                const TEXTURE* texture1 = pBufferId->getTextureByStage(0, i);
                if (texture1)
                {
                    GLBindTexture(GL_TEXTURE_2D, texture1->idTexture);
                    GLUniform1i(gles_shaderSpecific->samplerHandle1, 1);
                }
                else
                {
                    GLBindTexture(GL_TEXTURE_2D, 0);
                }

                const bool useNormal = (gles_shaderSpecific->normalHandle != -1)
                    && pBufferId->bs->vboNormalSubsetVB && pBufferId->bs->vboNormalSubsetVB[i] != 0;
                const bool useTexCoord = (gles_shaderSpecific->texCoordHandle != -1)
                    && pBufferId->bs->vboTextureSubsetVB && pBufferId->bs->vboTextureSubsetVB[i] != 0;
                disableUnusedVertexAttribs(gles_shaderSpecific, useNormal, useTexCoord);

                GLDrawArrays(modeDrawGl, 0, pBufferId->vertexCountVB[i]);
            }
        }
        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        return true;
    }

    bool SHADER::renderDynamic(const BUFFER_GL *pBufferId,const VEC3 *vertex,const VEC3 *normal,const VEC2 *uv) const
    {
        const GLES_PS_VS* gles_shaderSpecific = static_cast<const GLES_PS_VS*>(ptrShaderSpecific);
        GLCullFace(pBufferId->mode_cull_face);//GL_FRONT, GL_BACK, GL_FRONT_AND_BACK (CullFaceMode)
        GLFrontFace(pBufferId->mode_front_face_direction);//GL_CCW, GL_CW (FrontFaceDirection)
        const GLenum modeDrawGl       = getOpenGlEsModeDraw(pBufferId->mode_draw);

        if (pBufferId->isIndexBuffer()) // Index buffer
        {
            if (!pBufferId->bs->vboIndexSubsetIB)
                return false;
            GLUseProgram(gles_shaderSpecific->programObject);
            //-----------------------------------------------------------------------------------------------------------
            GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
            GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), vertex);
            //-----------------------------------------------------------------------------------------------------------
            if (gles_shaderSpecific->normalHandle != -1 && normal)
            {
                GLEnableVertexAttribArray(gles_shaderSpecific->normalHandle);
                GLVertexAttribPointer(gles_shaderSpecific->normalHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), normal);
            }
            //-----------------------------------------------------------------------------------------------------------
            GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
            GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2), uv);
            //-----------------------------------------------------------------------------------------------------------
            GLUniformMatrix4fv(gles_shaderSpecific->mvpMatrixHandle, 1, GL_FALSE, mvpMatrix.p);
            GLUniformMatrix4fv(gles_shaderSpecific->mvMatrixHandle, 1, GL_FALSE, modelView.p);
            //-----------------------------------------------------------------------------------------------------------
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                GLActiveTexture(GL_TEXTURE0);
                const TEXTURE * texture0 = pBufferId->getTextureByStage(0, i);
                GLBindTexture(GL_TEXTURE_2D, texture0 ? texture0->idTexture : 0);
                GLUniform1i(gles_shaderSpecific->samplerHandle0, 0);
                GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pBufferId->bs->vboIndexSubsetIB[i]);

                const TEXTURE * texture1 = pBufferId->getTextureByStage(1, 0);
                GLActiveTexture(GL_TEXTURE1);
                if (texture1)
                {
                    GLBindTexture(GL_TEXTURE_2D, texture1->idTexture);
                    GLUniform1i(gles_shaderSpecific->samplerHandle1, 1);
                }
                else
                {
                    GLBindTexture(GL_TEXTURE_2D, 0);
                }
                disableUnusedVertexAttribs(gles_shaderSpecific, gles_shaderSpecific->normalHandle != -1, gles_shaderSpecific->texCoordHandle != -1);
                GLDrawElements(modeDrawGl, pBufferId->indexCountIB[i], GL_UNSIGNED_SHORT, nullptr);
            }
        }
        else // Vertex buffer
        {
            if (!pBufferId->vertexCountVB)
                return false;
            GLUseProgram(gles_shaderSpecific->programObject);
            for (uint32_t i = 0; i < pBufferId->totalSubset; ++i)
            {
                GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), vertex);
                //-----------------------------------------------------------------------------------------------------------
                if (gles_shaderSpecific->normalHandle != -1 && normal)
                {
                    GLEnableVertexAttribArray(gles_shaderSpecific->normalHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->normalHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), normal);
                }
                //-----------------------------------------------------------------------------------------------------------
                if (gles_shaderSpecific->texCoordHandle != -1 && uv)
                {
                    GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2), uv);
                }
                //-----------------------------------------------------------------------------------------------------------
                GLUniformMatrix4fv(gles_shaderSpecific->mvpMatrixHandle, 1, GL_FALSE, mvpMatrix.p);
                GLUniformMatrix4fv(gles_shaderSpecific->mvMatrixHandle, 1, GL_FALSE, modelView.p);
                //-----------------------------------------------------------------------------------------------------------
                GLActiveTexture(GL_TEXTURE0);
                const TEXTURE * texture0 = pBufferId->getTextureByStage(0, i);
                GLBindTexture(GL_TEXTURE_2D, texture0 ? texture0->idTexture : 0);
                GLUniform1i(gles_shaderSpecific->samplerHandle0, 0);

                GLActiveTexture(GL_TEXTURE1);
                const TEXTURE * texture1 = pBufferId->getTextureByStage(1, 0);
                if (texture1)
                {
                    GLBindTexture(GL_TEXTURE_2D, texture1->idTexture);
                    GLUniform1i(gles_shaderSpecific->samplerHandle1, 1);
                }
                else
                {
                    GLBindTexture(GL_TEXTURE_2D, 0);
                }

                const bool useNormal = (gles_shaderSpecific->normalHandle != -1) && (normal != nullptr);
                const bool useTexCoord = (gles_shaderSpecific->texCoordHandle != -1) && (uv != nullptr);
                disableUnusedVertexAttribs(gles_shaderSpecific, useNormal, useTexCoord);

                GLDrawArrays(modeDrawGl, 0, pBufferId->vertexCountVB[i]);
            }
        }
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        return true;
    }

    bool SHADER::renderParticle(const BUFFER_GL* pBufferId, const PARTICLE_CONTROL* particleControl) const
    {
        const GLES_PS_VS* gles_shaderSpecific = static_cast<const GLES_PS_VS*>(ptrShaderSpecific);
        constexpr uint32_t index_subset = 0;
        const TEXTURE* texture0 = pBufferId->getTextureByStage(0, index_subset);
        GLActiveTexture(GL_TEXTURE0);
        GLBindTexture(GL_TEXTURE_2D, texture0 ? texture0->idTexture : 0);
        const GLenum modeDrawGl      = getOpenGlEsModeDraw(pBufferId->mode_draw);
        if (GL_TRIANGLES != modeDrawGl)
        {
            ERROR_AT(__LINE__, __FILE__, "Mode draw for OpenGlEs renderParticle not supported!");
            return false;
        }
        
        GLUniform1i(gles_shaderSpecific->samplerHandle0, 0);

        GLActiveTexture(GL_TEXTURE1);
        const TEXTURE* texture1 = pBufferId->getTextureByStage(1, index_subset);
        if (texture1)
        {
            GLBindTexture(GL_TEXTURE_2D, texture1->idTexture);
            GLUniform1i(gles_shaderSpecific->samplerHandle1, 1);
        }
        else
        {
            GLBindTexture(GL_TEXTURE_2D, 0);
        }

        GLboolean depthTestEnabled = true;
        glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
        GLDisable(GL_DEPTH_TEST);
        // Disable face culling for particle quads – a previous draw (e.g. LINE_MESH)
        // may have set glCullFace(GL_FRONT_AND_BACK) which would cull every triangle.
        const GLboolean cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
        if (cullFaceEnabled)
            GLDisable(GL_CULL_FACE);
        
        GLUniformMatrix4fv(gles_shaderSpecific->mvpMatrixHandle, 1, GL_FALSE, SHADER::mvpMatrix.p);
        GLUniformMatrix4fv(gles_shaderSpecific->mvMatrixHandle, 1, GL_FALSE,SHADER::modelView.p);
        // Unbind VBO so vertex pointers are treated as client-side arrays, and disable
        // stale attrib arrays left by a previous draw (e.g. LINE_MESH position-only shader)
        // to avoid GL_INVALID_OPERATION on strict GLES drivers (ANGLE, Mesa).
        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        disableUnusedVertexAttribs(gles_shaderSpecific, false, gles_shaderSpecific->texCoordHandle >= 0);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pBufferId->bs->vboIndexSubsetIB[index_subset]);
        VAR_SHADER* var = this->pShader
            ? this->pShader->getVarByName("color")
            : nullptr;

        const uint32_t totalAlive = particleControl->getTotalAlive();
        const VERTEX_UV* buffer = particleControl->getVertexBuffer();
        if (var)
        {
            const ATT_PARTICLE* particles = particleControl->getAttParticle();
            const int32_t handleVar = *static_cast<int32_t*>(var->ptrHandleVar);
            for (unsigned int i = 0; i < totalAlive; ++i)
            {
                const float* vertex = reinterpret_cast<const float*>(&buffer[i * 4]);
                const ATT_PARTICLE* particle = &particles[i];
                GLUniform4f(handleVar, particle->r, particle->g, particle->b, particle->a);
                GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VERTEX_UV), vertex);
        
                GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VERTEX_UV), &vertex[3]);
        
                GLDrawElements(modeDrawGl, 6, GL_UNSIGNED_SHORT, nullptr);
            }
        }
        else
        {
            
            for (unsigned int i = 0; i < totalAlive; ++i)
            {
                const float* vertex = reinterpret_cast<const float*>(&buffer[i * 4]);
                GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VERTEX_UV), vertex);
        
                GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VERTEX_UV), &vertex[3]);
        
                GLDrawElements(modeDrawGl, 6, GL_UNSIGNED_SHORT, nullptr);
            }
        }
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        if (cullFaceEnabled)
            GLEnable(GL_CULL_FACE);
        if (depthTestEnabled)
        {
            GLEnable(GL_DEPTH_TEST);
        }
        return true;
    }

    bool SHADER::renderParticle(const BUFFER_GL* pBufferId, const FLUID_GROUP* pGroup) const
    {
        const GLES_PS_VS* gles_shaderSpecific = static_cast<const GLES_PS_VS*>(ptrShaderSpecific);
        constexpr uint32_t index_subset = 0;
        const TEXTURE* texture0 = pBufferId->getTextureByStage(0, index_subset);
        GLActiveTexture(GL_TEXTURE0);
        GLBindTexture(GL_TEXTURE_2D, texture0 ? texture0->idTexture : 0);
        const GLenum modeDrawGl = getOpenGlEsModeDraw(pBufferId->mode_draw);
        if (GL_TRIANGLES != modeDrawGl)
        {
            ERROR_AT(__LINE__, __FILE__, "Mode draw for OpenGlEs renderParticle not supported!");
            return false;
        }

        GLUniform1i(gles_shaderSpecific->samplerHandle0, 0);

        GLActiveTexture(GL_TEXTURE1);
        const TEXTURE* texture1 = pBufferId->getTextureByStage(1, index_subset);
        if (texture1)
        {
            GLBindTexture(GL_TEXTURE_2D, texture1->idTexture);
            GLUniform1i(gles_shaderSpecific->samplerHandle1, 1);
        }
        else
        {
            GLBindTexture(GL_TEXTURE_2D, 0);
        }

        GLboolean depthTestEnabled = true;
        glGetBooleanv(GL_DEPTH_TEST, &depthTestEnabled);
        GLDisable(GL_DEPTH_TEST);
        // Disable face culling for particle quads – a previous draw (e.g. LINE_MESH)
        // may have set glCullFace(GL_FRONT_AND_BACK) which would cull every triangle.
        const GLboolean cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
        if (cullFaceEnabled)
            GLDisable(GL_CULL_FACE);

        GLUniformMatrix4fv(gles_shaderSpecific->mvpMatrixHandle, 1, GL_FALSE, SHADER::mvpMatrix.p);
        GLUniformMatrix4fv(gles_shaderSpecific->mvMatrixHandle, 1, GL_FALSE, SHADER::modelView.p);

        // Unbind VBO so vertex pointers are treated as client-side arrays, and disable
        // stale attrib arrays left by a previous draw (e.g. LINE_MESH position-only shader)
        // to avoid GL_INVALID_OPERATION on strict GLES drivers (ANGLE, Mesa).
        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        disableUnusedVertexAttribs(gles_shaderSpecific, false, gles_shaderSpecific->texCoordHandle >= 0);
        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, pBufferId->bs->vboIndexSubsetIB[index_subset]);
        VAR_SHADER* var = this->pShader
            ? this->pShader->getVarByName("color")
            : nullptr;
        if (pGroup->segmented)
        {
            if (var && pGroup->color)
            {
                const int32_t handleVar = *static_cast<int32_t*>(var->ptrHandleVar);
                GLUniform4f(handleVar, pGroup->color->r, pGroup->color->g, pGroup->color->b, pGroup->color->a);

                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    const float* vertex = reinterpret_cast<float*>(&pGroup->vertex_particle[i * 4]);
					const float* uv = reinterpret_cast<float*>(&pGroup->uv[i * 4]);// note that when segmented we have different uv for each particle
                    GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), vertex);
                    GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2), uv);
                    GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                }
            }
            else
            {
                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    //if(i != 168/2 && i != 168/2+1 && i != 168/2+2 && i != 168/2+4)
                    {
                        const float* vertex = reinterpret_cast<float*>(&pGroup->vertex_particle[i * 4]);// note that when segmented we have different uv for each particle
                        const float* uv = reinterpret_cast<float*>(&pGroup->uv[i * 4]);
                        GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                        GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), vertex);
                        GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                        GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2), uv);
                        GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                    }
                }
            }
        }
        else
        {
            if (var && pGroup->color)
            {
                const int32_t handleVar = *static_cast<int32_t*>(var->ptrHandleVar);
                GLUniform4f(handleVar, pGroup->color->r, pGroup->color->g, pGroup->color->b, pGroup->color->a);
                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    const float* vertex = reinterpret_cast<float*>(&pGroup->vertex_particle[i * 4]);
					const float* uv = reinterpret_cast<float*>(pGroup->uv);//note that when not segmented we have same uv for all particles
                    GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), vertex);
                    GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2), uv);
                    GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                }
            }
            else
            {
                for (unsigned int i = 0; i < pGroup->totalParticleToRender; ++i)
                {
                    const float* vertex = reinterpret_cast<float*>(&pGroup->vertex_particle[i * 4]);
                    const float* uv = reinterpret_cast<float*>(pGroup->uv);//note that when not segmented we have same uv for all particles
                    GLEnableVertexAttribArray(gles_shaderSpecific->positionHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->positionHandle, 3, GL_FLOAT, GL_FALSE, sizeof(VEC3), vertex);
                    GLEnableVertexAttribArray(gles_shaderSpecific->texCoordHandle);
                    GLVertexAttribPointer(gles_shaderSpecific->texCoordHandle, 2, GL_FLOAT, GL_FALSE, sizeof(VEC2), uv);
                    GLDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr);
                }
            }
        }

        GLBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        if (cullFaceEnabled)
            GLEnable(GL_CULL_FACE);
        if (depthTestEnabled)
        {
            GLEnable(GL_DEPTH_TEST);
        }
        return true;
    }

    static uint32_t compileCodeShader(BASE_SHADER* ptrShader,  const unsigned int type, const char *shaderSrc)
    {
        GLint compiled = 0;
        // Create the shader object
        uint32_t shader = GLCreateShader(type);
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
                    ptrShader ? ptrShader->fileName.c_str() : "nullptr", infoLog);
                free(infoLog);
            }
            GLDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    static uint32_t loadShaderProgram(BASE_SHADER* pShader, BASE_SHADER* vShader,  void * ptrShaderSpecific, const char *vertShaderSrc, const char *fragShaderSrc)
    {
        GLES_PS_VS* gles_shaderSpecific = static_cast<GLES_PS_VS*>(ptrShaderSpecific);
        GLint linked = 0;
        if (gles_shaderSpecific->programObject)
        {
            PRINT_IF_DEBUG("programObject already exists");
            return gles_shaderSpecific->programObject;
        }
        // Load the vertex/fragment shaders
        uint32_t vertexShader = compileCodeShader(pShader, GL_VERTEX_SHADER, vertShaderSrc);
        if (vertexShader == 0)
        {
            PRINT_IF_DEBUG("vertexShader == 0");
            return 0;
        }
        uint32_t fragmentShader = compileCodeShader(vShader, GL_FRAGMENT_SHADER, fragShaderSrc);
        if (fragmentShader == 0)
        {
            PRINT_IF_DEBUG("fragmentShader == 0");
            GLDeleteShader(vertexShader);
            return 0;
        }
        // Create the program object
        gles_shaderSpecific->programObject = GLCreateProgram();
        if (gles_shaderSpecific->programObject == 0)
        {
            PRINT_IF_DEBUG("Failed to create programObject");
            return 0;
        }
        GLAttachShader(gles_shaderSpecific->programObject, vertexShader);
        GLAttachShader(gles_shaderSpecific->programObject, fragmentShader);
        // Link the program
        GLLinkProgram(gles_shaderSpecific->programObject);
        // Check the link status
        GLGetProgramiv(gles_shaderSpecific->programObject, GL_LINK_STATUS, &linked);
        if (linked == 0)
        {
            GLDeleteShader(vertexShader);
            GLDeleteShader(fragmentShader);
            PRINT_IF_DEBUG("linked status failed");
            GLint infoLen = 0;
            GLGetProgramiv(gles_shaderSpecific->programObject, GL_INFO_LOG_LENGTH, &infoLen);
            if (infoLen > 1)
            {
                char *infoLog = static_cast<char *>(malloc(sizeof(char) * static_cast<size_t>(infoLen)));
                GLGetProgramInfoLog(gles_shaderSpecific->programObject, infoLen, nullptr, infoLog);
                PRINT_IF_DEBUG("Error linking program:\n%s\n", infoLog);
                free(infoLog);
            }
            GLDeleteProgram(gles_shaderSpecific->programObject);
            gles_shaderSpecific->programObject = 0;
            return 0;
        }
        // Free up no longer needed shader resources
        GLDeleteShader(vertexShader);
        GLDeleteShader(fragmentShader);
        return gles_shaderSpecific->programObject;
    }
}

#endif // USE_OPENGL_ES