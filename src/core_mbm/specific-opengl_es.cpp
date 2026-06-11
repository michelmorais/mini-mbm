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
#if defined (USE_OPENGL_ES)

#include <specific-opengl_es.h>
#include "specific-opengl_es-render-target.h"
#include <util-interface.h>

namespace mbm
{
    RENDER2TARGET_GLES::RENDER2TARGET_GLES():
        idFrameBuffer(0),
        idDepthRenderbuffer(0),
        idTextureDynamic(0)
    {
    }

    void RENDER2TARGET_GLES::release()
    {
        if (this->idDepthRenderbuffer)
        {
            GLDeleteRenderbuffers(1, &this->idDepthRenderbuffer);
        }
        this->idDepthRenderbuffer = 0;

        if (this->idFrameBuffer)
        {
            GLDeleteFramebuffers(1, &this->idFrameBuffer);
        }
        this->idFrameBuffer = 0;
        this->idTextureDynamic = 0;

    }

    RENDER2TARGET_GLES::~RENDER2TARGET_GLES()
    {
        release();
    }

}

GLint checkUniformLocation(const GLint location, const char * name)
{
    if (location == -1)
    {
        ERROR_LOG("Uniform location invalid [%s] in shader program.\n"
            "The variable name does not correspond to an active uniform in the program or \n"
            "The name starts with the reserved prefix \"gl_\"\n"
            "or The uniform is part of a structure, an array of structures, "
            "a vector/matrix subcomponent, an atomic counter, or a named uniform block.", name);
    }
	return location;
}

GLint checkAttribLocation(const GLint location, const char* name)
{
    if (location == -1)
    {
        ERROR_LOG("Attribute location invalid [%s] in shader program.\n"
            "The attribute variable is not active in the program (i.e., not used in the shader) or \n"
            "The name starts with the reserved prefix \"gl_\"\n", name);
    }
    return location;
}

GLint checkAttribLocationOptional(const GLint location, const char* name)
{
    if (location == -1)
    {
        static bool warned = false;
        if (!warned)
        {
            warned = true;
            WARN_LOG("Attribute [%s] was optimized out (unused in shader).\n"
                "Your vertex buffer may include this data (e.g. normals) but the shader does not use it.\n"
                "Consider: use FVF_POS or FVF_POS_UV if you don't need normals, or use the normal in the shader (e.g. lighting).\n", name);
        }
    }
    return location;
}

#endif
