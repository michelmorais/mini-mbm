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

#include <line-mesh.h>

#if defined(USE_OPENGL_ES)

#include <shader-var-cfg.h>
#include <util-interface.h>
#include <specific-opengl_es.h>

namespace mbm
{

    void MY_LINES::release()
    {
        arrayLinesVec3.clear();
        if (this->vboVertexUvLine)
        {
            GLDeleteBuffers(1, &this->vboVertexUvLine);
        }
        vboVertexUvLine = 0;
    }
    
    bool MY_LINES::onRestore()
    {
        if (this->vboVertexUvLine)
        {
            GLDeleteBuffers(1, &this->vboVertexUvLine);
        }
        vboVertexUvLine = 0;
        GLGenBuffers(1, &this->vboVertexUvLine); // somente para os vertices
        if (this->vboVertexUvLine == 0)
            return false;
        GLBindBuffer(GL_ARRAY_BUFFER, this->vboVertexUvLine);
        GLBufferData(GL_ARRAY_BUFFER, sizeof(mbm::VEC3) * arrayLinesVec3.size(), arrayLinesVec3.data(), GL_DYNAMIC_DRAW);
        return true;
    }
    
    bool MY_LINES::setLines(std::vector<VEC3> && arrayPoints,const bool invert_Y)
    {
        arrayLinesVec3 = std::move(arrayPoints);
        if (this->vboVertexUvLine == 0)
        {
            GLGenBuffers(1, &this->vboVertexUvLine); // somente para os vertices
            if (this->vboVertexUvLine == 0)
                return false;
        }
        if(invert_Y)
        {
            for(auto & vec3 : arrayLinesVec3 )
            {
                vec3.y = -vec3.y;
            }
        }
        GLBindBuffer(GL_ARRAY_BUFFER, this->vboVertexUvLine);
        GLBufferData(GL_ARRAY_BUFFER, sizeof(mbm::VEC3) * arrayLinesVec3.size(), arrayLinesVec3.data(), GL_DYNAMIC_DRAW);
        return true;
    }
    
    bool MY_LINES::renderLines(SHADER *shader)
    {
        if (!this->vboVertexUvLine)
            return false;
		//GLCullFace(pBufferId->mode_cull_face);
		//GLFrontFace(pBufferId->mode_front_face_direction);

        GLUseProgram(shader->programObject);

        GLBlendFunc(GL_SRC_ALPHA, 0x0303);

        GLBindBuffer(GL_ARRAY_BUFFER, this->vboVertexUvLine);
        GLEnableVertexAttribArray(shader->positionHandle);
        GLVertexAttribPointer(shader->positionHandle, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);

        GLint* imvpMatrixHandle = static_cast<GLint*>(shader->mvpMatrixHandle);
        
        GLUniformMatrix4fv(*imvpMatrixHandle, 1, GL_FALSE, SHADER::mvpMatrix.p);
        GLBindTexture(GL_TEXTURE_2D, 0);
        GLDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(this->arrayLinesVec3.size()));
        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        return true;
    }
    
    void LINE_MESH::onStop()
    {
        for (auto line : this->lsLines)
        {
            if (line->vboVertexUvLine)
            {
                GLDeleteBuffers(1, &line->vboVertexUvLine);
            }
            line->vboVertexUvLine = 0;
        }
    }
    
    bool LINE_MESH::loadShaderDefault()
    {
		auto * anim = this->getAnimation();
		if (anim == nullptr)
			return false;
        const char *fileNamePs  = "__line_color.ps";
        const char *fileNameVs  = "__line_color.vs";
        const char *codePScolor = "precision mediump float;\n"
                                  "uniform vec4 color;\n"
                                  "void main()\n"
                                  "{\n"
                                  " gl_FragColor =  color;\n"
                                  "}\n";

        const char *codeVsColor = "attribute vec4 aPosition;\n"
                                  "uniform mat4 mvpMatrix;\n"
                                  "void main()\n"
                                  "{\n"
                                  "   gl_Position = mvpMatrix * aPosition;\n"
                                  "}\n";

        anim->fx.fxPS->ptrCurrentShader = anim->fx.fxPS->loadEffect(fileNamePs, codePScolor, TYPE_ANIMATION_PAUSED);
        anim->fx.fxVS->ptrCurrentShader = anim->fx.fxVS->loadEffect(fileNameVs, codeVsColor, TYPE_ANIMATION_PAUSED);
        if (!anim->fx.fxPS->ptrCurrentShader || !anim->fx.fxVS->ptrCurrentShader)
            return false;
        const bool ret = anim->fx.shader.compileShader(anim->fx.fxPS->ptrCurrentShader, anim->fx.fxVS->ptrCurrentShader);
        if (!ret)
        {
            PRINT_IF_DEBUG("failed to compile shader:%s", fileNamePs);
            return false;
        }
        else
        {
            float c[4] = {1, 0, 0, 1};
            if (!anim->fx.fxPS->ptrCurrentShader->addVar("color", VAR_COLOR_RGBA, c,anim->fx.shader.programObject))
            {
#if defined _DEBUG
                PRINT_IF_DEBUG("failed to included variable %s shader %s!", "color", fileNamePs);
#endif
            }
            for (unsigned int i = 0; i < anim->fx.fxPS->ptrCurrentShader->getTotalVar(); ++i)
            {
                VAR_SHADER *varShader = anim->fx.fxPS->ptrCurrentShader->getVar(i);
                if (varShader)
                {
                    varShader->set(c, c, 1.0f);
                }
            }
        }
        return true;
    }
}
#endif // USE_OPENGL_ES