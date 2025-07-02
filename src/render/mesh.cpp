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

#include <mesh.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <util-interface.h>
#include <file-util.h>


namespace mbm
{

    MESH::MESH(const SCENE *scene, const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_MESH, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->indexCurrentAnimation = 0;
        this->mesh                  = nullptr;
        this->device->addRenderizable(this);
    }
    
    MESH::~MESH()
    {
        this->device->removeRenderizable(this);
        this->release();
    }
    
    void MESH::release()
    {
        this->releaseAnimation();
        this->indexCurrentAnimation = 0;
        this->mesh                  = nullptr;
    }
    
    bool MESH::load(const char *fileName)
    {
        if (this->mesh)
            return true;
        MESH_MANAGER *mehManager = MESH_MANAGER::getInstance();
        this->mesh               = mehManager->load(fileName);
        if (this->mesh)
        {
            // adicionamos as animações
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
            // carregamos a textura do estagio 2
            this->populateTextureStage2FromMesh(this->mesh);
            this->fileName = fileName;
            this->restartAnimation();
            this->updateAABB();
            return true;
        }

        return false;
    }
    
    const char * MESH::getFileName() const
    {
        if (this->mesh)
            return this->mesh->getFilenameMesh();
        return nullptr;
    }

    bool MESH::render()
    {
        if (!mesh)
            return false;
        if (this->indexCurrentAnimation < this->lsAnimation.size())
        {
            ANIMATION *anim = this->lsAnimation[this->indexCurrentAnimation];
            anim->updateAnimation(this->device->delta,this,this->onEndAnimation,this->onEndFx);
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
                if (!mesh->render(static_cast<unsigned int>(anim->indexCurrentFrame), &anim->fx.shader,0))
                    return false;
            }
            return true;
        }
        return false;
    }
    
    bool MESH::onRestoreDevice()
    {
        const unsigned int oldIndexCurrentAnimation = this->indexCurrentAnimation;
        this->releaseAnimation();
        const bool ret = this->load(this->fileName.c_str());
        if (ret)
        {
            this->indexCurrentAnimation = oldIndexCurrentAnimation;
            this->lsAnimation[this->indexCurrentAnimation]->restartAnimation();
            #if defined DEBUG_RESTORE
                PRINT_IF_DEBUG( "Mesh [%s] successfully restored",log_util::basename(this->fileName.c_str()));
            #endif
        }
        #if defined DEBUG_RESTORE
        else
        {
            PRINT_IF_DEBUG( "Failed to restore mesh [%s]",log_util::basename( this->fileName.c_str()));
        }
        #endif
        return ret;
    }
    
    bool MESH::isOnFrustum()
    {
        if (this->mesh && this->mesh->isLoaded())
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
    
    void MESH::onStop()
    {
        this->releaseAnimation();
        this->mesh = nullptr;
    }
    
    const mbm::INFO_PHYSICS * MESH::getInfoPhysics() const
    {
        if (this->mesh)
            return &this->mesh->infoPhysics;
        return nullptr;
    }
    
    const MESH_MBM * MESH::getMesh() const
    {
        return this->mesh;
    }

	FX*  MESH::getFx()const
	{
		auto * anim = getAnimation();
		if (anim)
			return &anim->fx;
		return nullptr;
	}

	ANIMATION_MANAGER*  MESH::getAnimationManager()
	{
		return this;
	}
    
    bool MESH::isLoaded() const
    {
        return this->mesh != nullptr;
    }

}


