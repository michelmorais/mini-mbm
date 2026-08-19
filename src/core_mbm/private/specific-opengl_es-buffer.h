/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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
#if defined(USE_OPENGL_ES)
#ifndef OPENGL_ES_BUFFER_SPECIFIC_H
#define OPENGL_ES_BUFFER_SPECIFIC_H

#include <specific-opengl_es.h>

namespace mbm
{
    struct BUFFER_SPECIFIC
    {
        BUFFER_SPECIFIC() noexcept;
        ~BUFFER_SPECIFIC();
        // Index buffer
        uint32_t  vboVertNorTexIB[3]; //(Index buffer: Vertex, Normal, texture) (vertex buffer: Normal, texture, unused)
        uint32_t *vboIndexSubsetIB;   // vbo index buffer IB
        // Vertex buffer
        uint32_t *vboVertexSubsetVB;  // Vertex buffer do subset VB
        uint32_t *vboNormalSubsetVB;  // Normal buffer do subset VB
        uint32_t *vboTextureSubsetVB; // Textura buffer do subset VB
        uint32_t vboBoneIndicesIB;
        uint32_t vboBoneWeightsIB;
        uint32_t *vboBoneIndicesVB;
        uint32_t *vboBoneWeightsVB;
        uint32_t skinSubsetCount;
        void release();
    };
}

#endif // OPENGL_ES_BUFFER_SPECIFIC_H
#endif // USE_OPENGL_ES
