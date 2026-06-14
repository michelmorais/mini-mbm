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
#include <header-mesh.h>
#include <draw-compatibility.h>
#include <util-interface.h>
#include <core_mbm/scene.h>
#include <shader-resource.h>
#include <shader-var-cfg.h>


namespace mbm
{

    MY_LINES::~MY_LINES()
    {
        this->release();
    }

    void MY_LINES::release()
    {
        arrayLinesVec3.clear();
        buffer.release();
    }

    bool MY_LINES::setLines(std::vector<VEC3>&& arrayPoints, const bool invert_Y)
    {
        arrayLinesVec3 = std::move(arrayPoints);
        const int vertexStartSubset = 0;
        const int vertexCountSubset = static_cast<int>(arrayLinesVec3.size());
        if (buffer.isLoadedBuffer())
        {
            if (buffer.sizeOfArrayVertex != arrayLinesVec3.size())
            {
                buffer.release();
            }
        }

        if (buffer.isLoadedBuffer() == false)
        {
            const uint32_t totalSubsets = 1;

            constexpr bool isDynamic = true;
            util::INFO_DRAW_MODE infoDraw;
            infoDraw.mode_cull_face = util::CULL_MODE::CULL_FRONT_AND_BACK;
            infoDraw.mode_draw = util::MODE_DRAW::MODE_DRAW_LINE_STRIP;
            infoDraw.mode_front_face_direction = util::FACE_DIRECTION::CCW;
            if (buffer.loadBuffer(arrayLinesVec3.data(), nullptr, nullptr, static_cast<uint32_t>(arrayLinesVec3.size()), totalSubsets, &vertexStartSubset, &vertexCountSubset, &infoDraw, isDynamic) == false)
            {
                arrayLinesVec3.clear();
                return false;
            }
        }
        if (invert_Y)
        {
            for (auto& vec3 : arrayLinesVec3)
            {
                vec3.y = -vec3.y;
            }
        }
        return buffer.updateDynamic(arrayLinesVec3.data(), nullptr, nullptr, &vertexStartSubset, &vertexCountSubset);
    }
    
    VEC3 * MY_LINES::getArray()
    {
        return arrayLinesVec3.data();
    }
    
    unsigned int MY_LINES::getSize() const
    {
        return static_cast<unsigned int>(arrayLinesVec3.size());
    }

    bool MY_LINES::renderLines(SHADER* shader)
    {
        if (buffer.isLoadedBuffer() == false)
            return false;
        //TODO: check the need of set GLBlendFunc(GL_SRC_ALPHA, 0x0303); (old way)
        return shader->render(&buffer);
    }

    bool MY_LINES::onRestore()
    {
        buffer.release();
        std::vector<VEC3> the_arrayLinesVec3(arrayLinesVec3);
        return setLines(std::move(the_arrayLinesVec3), false);
    }

    LINE_MESH::LINE_MESH(const SCENE *scene, const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_LINE_MESH, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->setEnableRender(true);
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->addRenderizable(this);
    }
    
    LINE_MESH::~LINE_MESH()
    {
        this->setEnableRender(false);
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
        auto myLine = new MY_LINES();
        if (!myLine->setLines(std::move(arrayLines), is2dS))
        {
            delete myLine;
            return 0xffffffff;
        }
        this->lsLines.push_back(myLine);
        if (this->lsLines.size() == 1)
        {
            if (this->createAnimationAndShader2Line() == false)
            {
                return 0xffffffff;
            }
        }
        return static_cast<unsigned int>(this->lsLines.size() - 1);
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
        if(ptr->is3DObject())
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
        VEC3 &position = this->getPosition();
        const VEC3 &targetPosition = ptr->getPosition();
        if(this->is3DObject())
        {
            position = targetPosition;
        }
        else
        {
            position.x = targetPosition.x;
            position.y = targetPosition.y;
        }
        if(ptr->getTypeClass() == TYPE_CLASS::TYPE_CLASS_TEXT)
        {
            if(ptr->is2dScreenObject())
            {
                position.x += w;
                position.y += h;
            }
            else if(ptr->is3DObject() == false)
            {
                position.x += w;
                position.y -= h;
            }
            else
            {
                position.x += w;
                position.y -= h;
            }
        }
    }
    
    bool LINE_MESH::isOnFrustum()
    {
        if (this->isRender2TextureEnabled())
            return false;
        return this->lsLines.size() != 0;
    }
    
    bool LINE_MESH::render()
    {
        if (this->lsLines.size())
        {
            mbm::DEVICE* device = mbm::DEVICE::getInstance();
            const CAMERA &camera = device->getCamera();
            const VEC3 &position = this->getPosition();
            const VEC3 &angle = this->getAngle();
            const VEC3 &scale = this->getScale();
            if (this->is3DObject())
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &position, &angle, &scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &camera.matrixPerspective);
            }
            else if (this->is2dScreenObject())
            {
                VEC3 positionScreen(position.x * camera.scaleScreen2d.x,
                                    position.y * camera.scaleScreen2d.y, position.z);
                device->transformeScreen2dToWorld2d_scaled(position.x, position.y, positionScreen);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionScreen, &angle, &scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &camera.matrixPerspective2d);
            }
            else
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &position, &angle, &scale);
                MatrixMultiply(&SHADER::mvpMatrix, &SHADER::modelView, &camera.matrixPerspective2d);
            }
            mbm::ANIMATION *anim = this->getAnimation();
            FX &fx = anim->getFx();
            this->setBlendState(anim->getBlendState());
            anim->updateAnimation(device->delta, this, this->getOnEndAnimation(), this->getOnEndFx());
            fx.shader.update(); // glUseProgram
            fx.setBlendOp();
            for (auto line : this->lsLines)
            {
                if (!line->renderLines(&fx.shader))
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
            #if defined DEBUG
            PRINT_IF_DEBUG("line successfully restored");
            #endif
            return true;
        }
        else
        {
            #if defined DEBUG
            PRINT_IF_DEBUG("Failed to restore line");
            #endif
            return false;
        }
    }
    
    bool LINE_MESH::createAnimationAndShader2Line()
    {
        this->releaseAnimation();
        auto anim = new mbm::ANIMATION();
        this->appendAnimation(anim);
        if (!loadShaderDefault())
            return false;
        return true;
    }

    bool LINE_MESH::loadShaderDefault()
    {
        auto* anim = this->getAnimation();
        if (anim == nullptr)
            return false;
        const char* fileNamePs = "__line_color.ps";
        const char* fileNameVs = "__line_color.vs";

        FX &fx = anim->getFx();
        fx.fxPS->setCurrentShader(fx.fxPS->loadEffect(fileNamePs, getCodePScolorFor_LINE_MESH(), TYPE_ANIMATION_PAUSED));
        fx.fxVS->setCurrentShader(fx.fxVS->loadEffect(fileNameVs, getCodeVScolorFor_LINE_MESH(), TYPE_ANIMATION_PAUSED));
        if (!fx.fxPS->getCurrentShader() || !fx.fxVS->getCurrentShader())
            return false;
        const bool ret = fx.shader.compileShader(fx.fxPS->getCurrentShader(), fx.fxVS->getCurrentShader(), getFvfFromBuffer());
        if (!ret)
        {
            PRINT_IF_DEBUG("failed to compile shader:%s", fileNamePs);
            return false;
        }
        else
        {
            float c[4] = { 1, 0, 0, 1 };
            void *backendShaderSpecific = fx.shader.getBackendShaderSpecific();
            if (!fx.fxPS->getCurrentShader()->addVar("color", VAR_COLOR_RGBA, c, backendShaderSpecific, true))
            {
#if defined _DEBUG
                PRINT_IF_DEBUG("failed to included variable %s shader %s!", "color", fileNamePs);
#endif
            }
            for (unsigned int i = 0; i < fx.fxPS->getCurrentShader()->getTotalVar(); ++i)
            {
                VAR_SHADER* varShader = fx.fxPS->getCurrentShader()->getVar(i);
                if (varShader)
                {
                    varShader->set(c, c, 1.0f);
                }
            }
        }
        return true;
    }

    FVF_PROVIDE_BY_ENGINE LINE_MESH::getFvfFromBuffer() const noexcept
    {
        if (lsLines.empty() || !lsLines[0]->buffer.isLoadedBuffer())
            return FVF_PROVIDE_BY_ENGINE::FVF_NONE;
        return lsLines[0]->buffer.fvf;
    }

    FX*  LINE_MESH::getFx()const
    {
        auto * anim = getAnimation();
        if (anim)
            return &anim->getFx();
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
