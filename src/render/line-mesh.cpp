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
#include <util-interface.h>
#include <core_mbm/scene.h>

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
    
    VEC3 * MY_LINES::getArray()
    {
        return arrayLinesVec3.data();
    }
    
    unsigned int MY_LINES::getSize() const
    {
        return arrayLinesVec3.size();
    }
    
  
    LINE_MESH::LINE_MESH(const SCENE *scene, const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_LINE_MESH, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->enableRender = true;
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->addRenderizable(this);
    }
    
    LINE_MESH::~LINE_MESH()
    {
        this->enableRender = false;
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->removeRenderizable(this);
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
            mbm::DEVICE* device = mbm::DEVICE::getInstance();
            if (this->is3D)
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &device->camera.matrixPerspective);
            }
            else if (this->is2dS)
            {
                VEC3 positionScreen(this->position.x * device->camera.scaleScreen2d.x,
                                    this->position.y * device->camera.scaleScreen2d.y, this->position.z);
                device->transformeScreen2dToWorld2d_scaled(this->position.x, this->position.y, positionScreen);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionScreen, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &device->camera.matrixPerspective2d);
            }
            else
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &this->position, &this->angle, &this->scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &device->camera.matrixPerspective2d);
            }
            mbm::ANIMATION *anim = this->getAnimation();
            this->blend.set(anim->blendState);
            anim->updateAnimation(device->delta, this, this->onEndAnimation, this->onEndFx);
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
