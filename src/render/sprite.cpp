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

#include <sprite.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <util-interface.h>


namespace mbm
{

    SPRITE::SPRITE(const SCENE *scene, const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_SPRITE, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->mesh = nullptr;
        this->device->addRenderizable(this);
    }
    
    SPRITE::~SPRITE()
    {
        this->device->removeRenderizable(this);
        this->release();
    }
    
    void SPRITE::release()
    {
        this->releaseAnimation();
        this->mesh                  = nullptr;
        this->indexCurrentAnimation = 0;
    }
    
    bool SPRITE::load(const char *fileName)
    {
        if (this->mesh != nullptr)
            return true;
        this->mesh = MESH_MANAGER::getInstance()->load(fileName);
        if (this->mesh)
        {
            const util::TYPE_MESH type = this->mesh->getTypeMesh();
            if (type != util::TYPE_MESH_SPRITE)
            {
                this->mesh->release();
                ERROR_LOG( "type of file is not sprite!\ntype: %s",MESH_MANAGER::typeClassName(type));
                return false;
            }
            for (unsigned int i = 0; i < this->mesh->infoAnimation.lsHeaderAnim.size(); ++i)
            {
                util::INFO_ANIMATION::INFO_HEADER_ANIM *header = this->mesh->infoAnimation.lsHeaderAnim[i];
                if (!this->populateAnimationFromHeader(this->mesh, header->headerAnim, i))
                {
                    this->release();
                    ERROR_AT(__LINE__,__FILE__, "error on add animation!!");
                    return false;
                }
            }
            this->populateTextureStage2FromMesh(this->mesh);
            this->fileName = fileName;
            this->restartAnimation();
            this->updateAABB();
            return true;
        }
        return false;
    }
    
    const char * SPRITE::getFileName()
    {
        if (this->mesh)
            return this->mesh->getFilenameMesh();
        return nullptr;
    }
    
    bool SPRITE::isOnFrustum()
    {
        if (this->mesh)
        {
            IS_ON_FRUSTUM verify(this);
            bool ret = verify.isOnFrustum(this->is3D, this->is2dS);
            if(ret == false)
            {
                ANIMATION *anim = this->getAnimation();
                anim->updateAnimation(this->device->delta, this, this->onEndAnimation, this->onEndFx);
            }
            return ret;
        }
        return false;
    }
    
    bool SPRITE::render()
    {
        if (!this->mesh)
            return false;
        if (this->indexCurrentAnimation < this->lsAnimation.size())
        {
            ANIMATION *anim = this->lsAnimation[this->indexCurrentAnimation];
            anim->updateAnimation(this->device->delta, this, this->onEndAnimation,this->onEndFx);
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
            this->blend.set(anim->blendState);
            anim->fx.shader.update();
            anim->fx.setBlendOp();
            if (anim->fx.textureOverrideStage2)
            {
                if (!this->mesh->render(static_cast<unsigned int>(anim->indexCurrentFrame), &anim->fx.shader,anim->fx.textureOverrideStage2->idTexture))
                    return false;
            }
            else
            {
                if (!this->mesh->render(static_cast<unsigned int>(anim->indexCurrentFrame), &anim->fx.shader,0))
                    return false;
            }
            return true;
        }
        return false;
    }
    
    bool SPRITE::onRestoreDevice()
    {
        this->releaseAnimation();
        this->mesh = nullptr;
        if(this->load(this->fileName.c_str()))
        {
            #if defined DEBUG_RESTORE
            PRINT_IF_DEBUG("sprite [%s] successfully restored", log_util::basename(this->fileName.c_str()));
            #endif
            return true;
        }
        else
        {
            #if defined DEBUG_RESTORE
            PRINT_IF_DEBUG("Failed to restore sprite  [%s]", log_util::basename(this->fileName.c_str()));
            #endif
            return false;
        }
    }
    
    void SPRITE::onStop()
    {
        this->releaseAnimation();
        this->mesh = nullptr;
    }
    
    const mbm::INFO_PHYSICS * SPRITE::getInfoPhysics() const
    {
        if(this->mesh)
            return &this->mesh->infoPhysics;
        return nullptr;
    }
    
    const MESH_MBM * SPRITE::getMesh() const
    {
        return this->mesh;
    }

	FX*  SPRITE::getFx()const
	{
		auto * anim = getAnimation();
		if (anim)
			return &anim->fx;
		return nullptr;
	}

	ANIMATION_MANAGER*  SPRITE::getAnimationManager()
	{
		return this;
	}
    
    bool SPRITE::isLoaded() const
    {
        return this->mesh != nullptr;
    }

}
