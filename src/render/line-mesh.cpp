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
#include <shader-var-cfg.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <util-interface.h>
#include <util.h>
#include <gles-debug.h>


namespace mbm
{

    MY_LINES::MY_LINES() noexcept
    {
        this->vboVertexUvLine = 0;
    }
    
    MY_LINES::~MY_LINES()
    {
        
        this->release();
    }
    
    void MY_LINES::release()
    {
        arrayLinesVec3.clear();
        if (this->vboVertexUvLine)
        {
            GLDeleteBuffers(1, &this->vboVertexUvLine);
        }
        vboVertexUvLine = 0;
    }
    
    VEC3 * MY_LINES::getArray()
    {
        return arrayLinesVec3.data();
    }
    
    unsigned int MY_LINES::getSize() const
    {
        return arrayLinesVec3.size();
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

        GLUniformMatrix4fv(shader->mvpMatrixHandle, 1, GL_FALSE, SHADER::mvpMatrix.p);
        GLBindTexture(GL_TEXTURE_2D, 0);
        GLDrawArrays(GL_LINE_STRIP, 0, static_cast<GLsizei>(this->arrayLinesVec3.size()));
        GLBindBuffer(GL_ARRAY_BUFFER, 0);
        return true;
    }
    
  
    LINE_MESH::LINE_MESH(const SCENE *scene, const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_LINE_MESH, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->enableRender = true;
        this->device->addRenderizable(this);
    }
    
    LINE_MESH::~LINE_MESH()
    {
        this->enableRender = false;
        this->device->removeRenderizable(this);
        this->release();
    }
    
    void LINE_MESH::release()
    {
        for (auto line : this->lsLines)
        {
            delete line;
        }
        this->lsLines.clear();
    }
    
    unsigned int LINE_MESH::set(std::vector<VEC3> && arrayLines, unsigned int index)
    {
        if (index < this->lsLines.size())
        {
            MY_LINES *myLine = this->lsLines[index];
            if (myLine && myLine->setLines(std::move(arrayLines), is2dS))
            {
                return index;
            }
        }
        return 0xffffffff;
    }
    
    unsigned int LINE_MESH::add(std::vector<VEC3> && arrayLines)
    {
        
        if (this->lsLines.size() == 0)
        {
            if (this->createAnimationAndShader2Line() == false)
            {
                return 0xffffffff;
            }
        }
        auto myLine = new MY_LINES();
        if (myLine->setLines(std::move(arrayLines), is2dS))
        {
            this->lsLines.push_back(myLine);
            const auto index = static_cast<unsigned int>(this->lsLines.size() - 1);
            return index;
        }
        else
        {
            delete myLine;
            return 0xffffffff;
        }
    }
    
    unsigned int LINE_MESH::getTotalLines() const
    {
        return static_cast<unsigned int>(this->lsLines.size());
    }
    
    unsigned int LINE_MESH::getTotalPoints(const unsigned int idLine) const
    {
        if (idLine < static_cast<unsigned int>(this->lsLines.size()))
            return this->lsLines[idLine]->getSize();
        return 0;
    }
    
    mbm::INFO_PHYSICS * LINE_MESH::getNotConstInfoPhysics()
    {
        return &infoPhysics;
    }

    void LINE_MESH::drawBounding(RENDERIZABLE* ptr,const bool useAABB)noexcept
    {
        if(ptr == nullptr)
            return;
        float w,h = 0.0f;
		ptr->updateAABB();
        if(ptr->is3D)
        {
            /*
			   f________________g
			   /               /|
			  /               / |
		   b /_______________/c |
			|   |           |   |
			|   |           |   |
			|   |   back    |   |
			|  e|___________|___|h
			|  /            |  /
			| /             | /
			|/______________|/
			a   front       d

	        */
            float d = 0.0f;
            if(useAABB)
                ptr->getAABB(&w,&h,&d);
            else
                ptr->getWidthHeight(&w,&h,&d);
			std::vector<VEC3> box(16);
            w = (w * 0.5f);
			h = (h * 0.5f);
			const float z = d > 0.0f ? (d * 0.5f) : 1.0f;

			box[0 ]  = VEC3(-w,-h,-z);// --a 1
			box[1 ]  = VEC3(-w, h,-z);// --b 2
			box[2 ]  = VEC3( w, h,-z);// --c 3 
			box[3 ]  = VEC3( w,-h,-z);// --d 4
			box[4 ]  = VEC3(-w,-h,-z);// --a 1
			box[5 ]  = VEC3(-w,-h, z);// --e 5
			box[6 ]  = VEC3(-w, h, z);// --f 6
			box[7 ]  = VEC3(-w, h,-z);// --b 2
			box[8 ]  = VEC3(-w, h, z);// --f 6
			box[9 ]  = VEC3( w, h, z);// --g 7
			box[10]  = VEC3( w,-h, z);// --h 8
			box[11]  = VEC3( w,-h,-z);// --d 4
			box[12]  = VEC3( w, h,-z);// --c 3 
			box[13]  = VEC3( w, h, z);// --g 7
			box[14]  = VEC3( w,-h, z);// --h 8
			box[15]  = VEC3(-w,-h, z);// --e 5

            if(this->lsLines.size() > 0)
                this->set(std::move(box),0);
            else
                this->add(std::move(box));
        }       
        else
        {
            if(useAABB)
                ptr->getAABB(&w,&h);
            else
                ptr->getWidthHeight(&w,&h);
            w *= 0.5f;
            h *= 0.5f;
            std::vector<VEC3> line = {VEC3(-w,-h,0), VEC3(-w,h,0), VEC3(w,h,0), VEC3(w,-h,0), VEC3(-w,-h,0)};
            if(this->lsLines.size() > 0)
                this->set(std::move(line),0);
            else
                this->add(std::move(line));
        }
        if(this->is3D)
        {
            this->position = ptr->position;
        }
        else
        {
            this->position.x = ptr->position.x;
            this->position.y = ptr->position.y;
        }
        if(ptr->typeClass == TYPE_CLASS::TYPE_CLASS_TEXT)
        {
            if(ptr->is2dS)
            {
                this->position.x += w;
                this->position.y += h;
            }
            else if(ptr->is3D == false)
            {
                this->position.x += w;
                this->position.y -= h;
            }
            else
            {
                this->position.x += w;
                this->position.y -= h;
            }
        }
    }
    
    bool LINE_MESH::isOnFrustum()
    {
        if (this->isRender2Texture)
            return false;
        return this->lsLines.size() != 0;
    }
    
    bool LINE_MESH::render()
    {
        if (this->lsLines.size())
        {
            if (this->is3D)
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective);
            }
            else if (this->is2dS)
            {
                VEC3 positionScreen(this->position.x * this->device->camera.scaleScreen2d.x,
                                    this->position.y * this->device->camera.scaleScreen2d.y, this->position.z);
                this->device->transformeScreen2dToWorld2d_scaled(this->position.x, this->position.y, positionScreen);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionScreen, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective2d);
            }
            else
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &this->device->camera.matrixPerspective2d);
            }
            mbm::ANIMATION *anim = this->getAnimation();
            this->blend.set(anim->blendState);
            anim->updateAnimation(this->device->delta, this, this->onEndAnimation, this->onEndFx);
            anim->fx.shader.update(); // glUseProgram
            anim->fx.setBlendOp();
            for (auto line : this->lsLines)
            {
                if (!line->renderLines(&anim->fx.shader))
                    return false;
            }
            return true;
        }
        return false;
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
    
    bool LINE_MESH::onRestoreDevice()
    {
        bool ret = true;
        for (auto line : this->lsLines)
        {
            if(line)
            {
                if(line->onRestore() == false)
                    ret = false;
            }
        }
        if(ret && this->createAnimationAndShader2Line())
        {
            #if defined DEBUG_RESTORE
            PRINT_IF_DEBUG("line successfully restored");
            #endif
            return true;
        }
        else
        {
            #if defined DEBUG_RESTORE
            PRINT_IF_DEBUG("Failed to restore line");
            #endif
            return false;
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
    
    bool LINE_MESH::createAnimationAndShader2Line()
    {
        this->releaseAnimation();
        auto anim = new mbm::ANIMATION();
        this->lsAnimation.push_back(anim);
        if (!loadShaderDefault())
            return false;
        return true;
    }

	FX*  LINE_MESH::getFx()const
	{
		auto * anim = getAnimation();
		if (anim)
			return &anim->fx;
		return nullptr;
	}

	ANIMATION_MANAGER*  LINE_MESH::getAnimationManager()
	{
		return this;
	}
    
    const mbm::INFO_PHYSICS * LINE_MESH::getInfoPhysics() const
    {
        return &infoPhysics;
    }
    
    const MESH_MBM * LINE_MESH::getMesh() const
    {
        return nullptr;
    }
    
    bool LINE_MESH::isLoaded() const
    {
        return this->lsLines.size() > 0;
    }

}
