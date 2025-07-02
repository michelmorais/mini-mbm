/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef _SHADERS_GLES_H
#define _SHADERS_GLES_H

#include "core-exports.h"
#include "primitives.h"
#include <string.h>
#include <vector>
#include <string>

#if defined ANDROID
    #include <GLES2/gl2.h>
#elif defined __MINGW32__ | defined __CYGWIN__
    #include <gles/GLES2/gl2.h>
#elif defined _WIN32
    #include <GLES2/gl2.h>
#elif defined __linux__
    #include <GLES2/gl2.h>
#endif

namespace util
{
	struct INFO_DRAW_MODE;
};

namespace mbm
{
    struct VAR_SHADER;
    enum TYPE_VAR_SHADER : char;

	enum TYPE_ANIMATION : char
    { 
      TYPE_ANIMATION_PAUSED          = 0, // Pausa A Animação
      TYPE_ANIMATION_GROWING         = 1, // Incrementa Na Ordem Crescente Parando No Limite Maximo
      TYPE_ANIMATION_GROWING_LOOP    = 2, // Incrementa Na Ordem Crescente e Faz Loop Quando Ultrapassar Limite Maximo
      TYPE_ANIMATION_DECREASING      = 3, // Decrementa Na Ordem Decrescente E Para No Limite Mínimo
      TYPE_ANIMATION_DECREASING_LOOP = 4, // Decrementa Na Ordem Decrescente e Faz LoopQuando Ultrapassar O Limite Mínimo
      TYPE_ANIMATION_RECURSIVE       = 5, // icrementa Na Ordem Crescente e Decrescente; Para No Limite Mínimo
      TYPE_ANIMATION_RECURSIVE_LOOP  = 6 // Incrementa Na Ordem Crescente e Decrescente. Faz loop Entre O Limite Mínimo E O Limite Maximo.

    };

	enum STATUS_FX
    {
        FX_GROWING, FX_DECREASING, FX_END, FX_END_CALLBACK
    };

    class BUFFER_GL
    {
      public:
        API_IMPL BUFFER_GL() noexcept;
        API_IMPL virtual ~BUFFER_GL();
        API_IMPL bool isLoadedBuffer() const;
        API_IMPL void release();
        
        API_IMPL bool loadBuffer(const VEC3 *vertex,const VEC3 *normal,const VEC2 *uv,const uint32_t sizeOfArrayVertex,const uint32_t totalSubsets,const int *vertexStartSubset,const int *vertexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode);// type vertex buffer
        API_IMPL bool loadBuffer(const VEC3 *vertex,const VEC3 *normal,const VEC2 *uv,const uint32_t sizeOfArrayVertex,const uint16_t *arrayIndices,const uint32_t totalSubsets,const int *indexStartSubset,const int *indexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode);// type index buffer
        API_IMPL bool loadBufferDynamic(uint16_t *arrayIndices, uint32_t totalSubsets, int *indexStartSubset,int *indexCountSubset,const util::INFO_DRAW_MODE * info_draw_mode);
        // Index buffer
        uint32_t  vboVertNorTexIB[3]; //(Index buffer: Vertex, Normal, texture) (vertex buffer: Normal, texture, unused)
        uint32_t *vboIndexSubsetIB;   // vbo index buffer IB
        int *         indexStartIB;       // index start subset IB
        int *         indexCountIB;       // index count subset IB
        // Vertex buffer
        uint32_t *vboVertexSubsetVB;  // Vertex buffer do subset VB
        uint32_t *vboNormalSubsetVB;  // Normal buffer do subset VB
        uint32_t *vboTextureSubsetVB; // Textura buffer do subset VB
        int *         vertexStartVB;      // inicio do vertex buffer no subset VB
        int *         vertexCountVB;      // Total de vertex no subset VB
        // Control
        uint32_t   totalSubset;   // Total de subset deste buffer
        uint32_t * idTexture0;    // Existe 1 idtextura para cada subset. (stagio 0)
        uint8_t *useAlpha;      // Usa alpha para a textura
        uint32_t   idTexture1;    // id textura stagio 1 passado no momento de renderizar o shader
        bool           isIndexBuffer; // Flag informando se este buffer eh index buffer ou vertex buffer.
		uint32_t   mode_draw;     //default (GL_TRIANGLES), mode: GL_POINTS, GL_LINES, GL_LINE_LOOP, GL_LINE_STRIP, GL_TRIANGLES, GL_TRIANGLE_STRIP, GL_TRIANGLE_FAN
		uint32_t   mode_cull_face;//GL_FRONT, GL_BACK,GL_FRONT_AND_BACK
		uint32_t   mode_front_face_direction; //GL_CW, GL_CCW
    };

    class BASE_SHADER
    {
        friend class SHADER;
      public:
        std::string fileName; // shader
        API_IMPL BASE_SHADER() noexcept;
        API_IMPL virtual ~BASE_SHADER();
        API_IMPL const char *getCode();
        API_IMPL VAR_SHADER *getVarByName(const char *nameVar);
        API_IMPL VAR_SHADER *getVar(const uint32_t indexVar);
        API_IMPL bool addVar(const char *nameVar, const TYPE_VAR_SHADER typeVar, const float *defaultValue,const uint32_t programObject);
        API_IMPL uint32_t getTotalVar() const noexcept;
        API_IMPL void releaseVars();
        API_IMPL bool loadShader(const char *fileNameShaderVS_PS, const char *code);
        std::vector<VAR_SHADER *> *getVars();
      private:
        bool isThereVarIntoLsVars(const char *nameVar);
        void update(const uint32_t programObject);
      protected:
        std::vector<VAR_SHADER *> lsVar;
        std::string        stringCodeShader;
    };

    class API_IMPL SHADER
    {
      public:
        static MATRIX modelView;
        static MATRIX mvpMatrix; // ModelView x projection
        uint32_t programObject;   // Controle de uma entidade opengles 2.0 que linka um vertex shader e pixel shader a um objeto
        int mvpMatrixHandle; // Handle para matrix x projection
        int mvMatrixHandle;  // Handle para a matrix do modelo
        GLint positionHandle;
        GLint texCoordHandle;
        int samplerHandle0;
        int samplerHandle1;
        GLint normalHandle;
        SHADER() noexcept;
        virtual ~SHADER();
        void releaseShader();
        void onRestore();
        bool compileShader(BASE_SHADER *ptrPshader, BASE_SHADER *ptrVshader);
        bool isLoad();
        bool render(const BUFFER_GL *pBufferId) const;
        bool renderDynamic(const BUFFER_GL *pBufferId,const VEC3 *vertex,const VEC3 *normal,const VEC2 *uv) const;
        void update();
      private:
        uint32_t compileCodeShader(const GLenum type, const char *shaderSrc);
        uint32_t loadShaderProgram(const char *vertShaderSrc, const char *fragShaderSrc);
        BASE_SHADER *pShader;
        BASE_SHADER *vShader;
    };

}

#endif
