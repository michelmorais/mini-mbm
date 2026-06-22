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
#include <core_mbm/scene.h>


namespace mbm
{

    MESH::MESH(const SCENE *scene, const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_MESH, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->setIndexAnimation(0);
        this->mesh                  = nullptr;
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->addRenderizable(this);
    }
    
    MESH::~MESH()
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->removeRenderizable(this);
        this->release();
    }
    
    void MESH::release()
    {
        this->releaseAnimation();
        this->setIndexAnimation(0);
        this->mesh                  = nullptr;
    }
    
    bool MESH::load(const char *fileName)
    {
        if (this->mesh)
            return true;
        MESH_MANAGER *mehManager = MESH_MANAGER::getInstance();
        this->mesh               = mehManager->load(fileName, this);
        if (this->mesh)
        {
            // adicionamos as animações
            const uint32_t totalAnimations = this->mesh->getTotalAnimations();
            for (uint32_t i = 0; i < totalAnimations; ++i)
            {
                util::INFO_ANIMATION::INFO_HEADER_ANIM *header = this->mesh->getAnimationHeader(i);
                if (!this->populateAnimationFromHeader(this->mesh, header->headerAnim, i))
                {
                    this->release();
                    ERROR_AT(__LINE__,__FILE__, "error on add animation!!");
                    return false;
                }
            }
            // carregamos a textura do estagio 2
            this->populateTextureStage2FromMesh(this->mesh);
            this->setInternalFileName(fileName);
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
        const uint32_t indexAnimation = this->getIndexAnimation();
        ANIMATION *anim = this->getAnimation(indexAnimation);
        if (anim)
        {
            mbm::DEVICE* device = mbm::DEVICE::getInstance();
            const CAMERA &camera = device->getCamera();
            anim->updateAnimation(device->delta,this,this->getOnEndAnimation(),this->getOnEndFx());
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
            FX &fx = anim->getFx();
            this->setBlendState(anim->getBlendState());
            fx.shader.update();
            fx.setBlendOp();
            if (fx.textureOverrideStage2)
            {
                if (!this->mesh->render(static_cast<unsigned int>(anim->getIndexCurrentFrame()), &fx.shader,
                                        fx.textureOverrideStage2, this))
                    return false;
            }
            else
            {
                if (!mesh->render(static_cast<unsigned int>(anim->getIndexCurrentFrame()), &fx.shader, nullptr, this))
                    return false;
            }
            return true;
        }
        return false;
    }
    
    bool MESH::onRestoreDevice()
    {
		this->mesh = nullptr;
        const char *internalFileName = this->getInternalFileName();
        const bool ret = this->load(internalFileName);
        if (ret)
        {
            #if defined DEBUG
            PRINT_INFO_IF_DEBUG( "Mesh [%s] successfully restored",log_util::basename(internalFileName));
            #endif
        }
        #if defined DEBUG
        else
        {
            PRINT_IF_DEBUG( "Failed to restore mesh [%s]",log_util::basename(internalFileName));
        }
        #endif
        return ret;
    }
    
    bool MESH::isOnFrustum()
    {
        if (this->mesh && this->mesh->isLoaded())
        {
            IS_ON_FRUSTUM verify(this);
            bool ret = verify.isOnFrustum(this->is3DObject(), this->is2dScreenObject());
            if(ret == false)
            {
                ANIMATION *anim = this->getAnimation();
                mbm::DEVICE* device = mbm::DEVICE::getInstance();
                anim->updateAnimation(device->delta, this, this->getOnEndAnimation(), this->getOnEndFx());
            }
            return ret;
        }
        return false;
    }
    
    //void MESH::onStop()
    //{
    //    this->releaseAnimation();
    //    this->mesh = nullptr;
    //}
    
    const mbm::INFO_PHYSICS * MESH::getInfoPhysics() const
    {
        if (this->mesh)
            return &this->mesh->getPhysicsInfo();
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
            return &anim->getFx();
        return nullptr;
    }

    ANIMATION_MANAGER*  MESH::getAnimationManager()
    {
        return this;
    }

    FVF_PROVIDE_BY_ENGINE MESH::getFvfFromBuffer() const noexcept
    {
        if (mesh)
        {
            BUFFER_MESH* buf = mesh->getBuffer(0);
            if (buf && buf->hasLoadedRenderBuffer())
                return buf->getRenderBuffer()->fvf;
        }
        return FVF_PROVIDE_BY_ENGINE::FVF_NONE;
    }
    
    bool MESH::isLoaded() const
    {
        return this->mesh != nullptr;
    }

}
