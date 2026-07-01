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
#ifndef OPENGL_ES_SHADER_SPECIFIC_H
#define OPENGL_ES_SHADER_SPECIFIC_H

#include <specific-opengl_es.h>

namespace mbm
{
    struct GLES_PS_VS
    {
        GLint positionHandle;
        GLint texCoordHandle;
        GLint normalHandle;
        GLint mvpMatrixHandle; // Handle para matrix x projection
        GLint mvMatrixHandle;  // Handle para a matrix do modelo
        GLint samplerHandle0;
        GLint samplerHandle1;
        GLint samplerHandle2;
        GLint samplerHandle3;
        GLint samplerHandle4;
        GLint samplerHandle5;

        GLuint programObject;

        GLES_PS_VS() noexcept;
        ~GLES_PS_VS();
        void release() noexcept;
        GLES_PS_VS(const GLES_PS_VS&) = delete;
        GLES_PS_VS& operator=(const GLES_PS_VS&) = delete;
    };
}

#endif // OPENGL_ES_SHADER_SPECIFIC_H
#endif // USE_OPENGL_ES
