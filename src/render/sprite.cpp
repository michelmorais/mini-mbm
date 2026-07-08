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
#include <core_mbm/scene.h>

namespace mbm
{

    SPRITE::SPRITE(const SCENE *scene, const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_SPRITE, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->mesh = nullptr;
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->addRenderizable(this);
    }
    
    SPRITE::~SPRITE()
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->removeRenderizable(this);
        this->release();
    }
    
    void SPRITE::release()
    {
        this->releaseAnimation();
        this->mesh                  = nullptr;
        this->setIndexAnimation(0);
    }
    
    bool SPRITE::load(const char *fileName)
    {
        if (this->mesh != nullptr)
            return true;
        this->mesh = MESH_MANAGER::getInstance()->load(fileName, this);
        if (this->mesh)
        {
            const util::TYPE_MESH expectedType = util::TYPE_MESH_SPRITE;
            const MeshLoadFinishResult result = this->populateAnimationsFromMesh(this->mesh, &expectedType, "sprite");
            if (result == MeshLoadFinishResult::ANIMATION_FAILED)
            {
                this->release();
                return false;
            }
            else if (result != MeshLoadFinishResult::OK)
                return false;
            this->setInternalFileName(fileName);
            this->restartAnimation();
            this->updateAABB();
            return true;
        }
        return false;
    }

    void SPRITE::loadAsync(const char *fileName, std::function<void(bool success)> callback)
    {
        if (this->mesh != nullptr)
        {
            if (callback)
                callback(true);
            return;
        }
        const std::string fileNameCopy(fileName);
        MESH_MANAGER::getInstance()->loadAsync(fileName, [this, fileNameCopy, callback](MESH_MBM *mesh, bool ok)
        {
            if (!ok || !mesh)
            {
                if (callback)
                    callback(false);
                return;
            }
            this->mesh = mesh;
            this->getPosition() += mesh->getPositionOffset();
            this->setAngle(mesh->getAngleDefault());
            const util::TYPE_MESH expectedType = util::TYPE_MESH_SPRITE;
            const MeshLoadFinishResult result = this->populateAnimationsFromMesh(this->mesh, &expectedType, "sprite");
            if (result == MeshLoadFinishResult::ANIMATION_FAILED)
            {
                this->release();
                if (callback)
                    callback(false);
                return;
            }
            else if (result != MeshLoadFinishResult::OK)
            {
                if (callback)
                    callback(false);
                return;
            }
            this->setInternalFileName(fileNameCopy.c_str());
            this->restartAnimation();
            this->updateAABB();
            if (callback)
                callback(true);
        });
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
    
    bool SPRITE::render()
    {
        if (!this->mesh)
            return false;
        const uint32_t indexAnimation = this->getIndexAnimation();
        ANIMATION *anim = this->getAnimation(indexAnimation);
        if (anim)
        {
            mbm::DEVICE* device = mbm::DEVICE::getInstance();
            const CAMERA &camera = device->getCamera();
            anim->updateAnimation(device->delta, this, this->getOnEndAnimation(),this->getOnEndFx());
            const VEC3 &position = this->getPosition();
            const VEC3 &angle = this->getAngle();
            const VEC3 &scale = this->getScale();
            if (this->is3DObject())
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &position, &angle, &scale);
                SHADER::updateMvpAndLightMatrices(camera.matrixView, camera.matrixPerspective);
            }
            else if (this->is2dScreenObject())
            {
                VEC3 positionScreen(position.x * camera.scaleScreen2d.x,
                                    position.y * camera.scaleScreen2d.y, position.z);
                device->transformeScreen2dToWorld2d_scaled(position.x, position.y, positionScreen);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionScreen, &angle, &scale);
                SHADER::updateMvpAndLightMatrices(camera.matrixView2d, camera.matrixPerspective2d);
            }
            else
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &position, &angle, &scale);
                SHADER::updateMvpAndLightMatrices(camera.matrixView2d, camera.matrixPerspective2d);
            }
            FX &fx = anim->getFx();
            this->setBlendState(anim->getBlendState());
            fx.shader.update();
            fx.setBlendOp();
            BUFFER_MESH *frameBuffer = this->mesh->getBuffer(static_cast<unsigned int>(anim->getIndexCurrentFrame()));
            fx.bindTextureAnimationEffect(frameBuffer ? frameBuffer->getRenderBuffer() : nullptr);
            if (!this->mesh->render(static_cast<unsigned int>(anim->getIndexCurrentFrame()), &fx.shader, this))
                return false;
            return true;
        }
        return false;
    }
    
    bool SPRITE::onRestoreDevice()
    {
        this->mesh = nullptr;
        const char *internalFileName = this->getInternalFileName();
        if(this->load(internalFileName))
        {
            #if defined DEBUG
            PRINT_INFO_IF_DEBUG("sprite [%s] successfully restored", log_util::basename(internalFileName));
            #endif
            return true;
        }
        else
        {
            #if defined DEBUG
            PRINT_IF_DEBUG("Failed to restore sprite  [%s]", log_util::basename(internalFileName));
            #endif
            return false;
        }
    }
    
    const mbm::INFO_PHYSICS * SPRITE::getInfoPhysics() const
    {
        if(this->mesh)
            return &this->mesh->getPhysicsInfo();
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
            return &anim->getFx();
        return nullptr;
    }

    ANIMATION_MANAGER*  SPRITE::getAnimationManager()
    {
        return this;
    }

    FVF_PROVIDE_BY_ENGINE SPRITE::getFvfFromBuffer() const noexcept
    {
        if (mesh)
        {
            BUFFER_MESH* buf = mesh->getBuffer(0);
            if (buf && buf->hasLoadedRenderBuffer())
                return buf->getRenderBuffer()->fvf;
        }
        return FVF_PROVIDE_BY_ENGINE::FVF_NONE;
    }
    
    bool SPRITE::isLoaded() const
    {
        return this->mesh != nullptr;
    }

}
