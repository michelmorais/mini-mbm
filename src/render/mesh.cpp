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
        this->resetArticulatedAnimationPlayer();
        this->resetSkeletalAnimationPlayer();
    }
    
    bool MESH::load(const char *fileName)
    {
        if (this->mesh)
            return true;
        MESH_MANAGER *mehManager = MESH_MANAGER::getInstance();
        this->mesh               = mehManager->load(fileName, this);
        if (this->mesh)
        {
            const MeshLoadFinishResult result = this->populateAnimationsFromMesh(this->mesh, nullptr, "mesh");
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

    void MESH::loadAsync(const char *fileName, std::function<void(bool success)> callback)
    {
        if (this->mesh)
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
            const MeshLoadFinishResult result = this->populateAnimationsFromMesh(this->mesh, nullptr, "mesh");
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

    const char * MESH::getFileName() const
    {
        if (this->mesh)
            return this->mesh->getFilenameMesh();
        return nullptr;
    }

    bool MESH::playArticulatedAnimation(const char *name, const int priority,
                                        const float blendDuration, const float weight)
    {
        return this->mesh ? this->mesh->playArticulatedAnimation(
            this->getArticulatedAnimationPlayer(), name, priority, blendDuration, weight) : false;
    }

    uint32_t MESH::getTotalArticulatedAnimations() const noexcept
    {
        return this->mesh ? this->mesh->getTotalArticulatedAnimations() : 0;
    }

    const char *MESH::getArticulatedAnimationName(const uint32_t index) const noexcept
    {
        return this->mesh ? this->mesh->getArticulatedAnimationName(index) : nullptr;
    }

    bool MESH::pauseArticulatedAnimation(const char *name) noexcept
    {
        return this->mesh ? this->mesh->pauseArticulatedAnimation(this->getArticulatedAnimationPlayer(), name) : false;
    }

    bool MESH::resumeArticulatedAnimation(const char *name) noexcept
    {
        return this->mesh ? this->mesh->resumeArticulatedAnimation(this->getArticulatedAnimationPlayer(), name) : false;
    }

    bool MESH::disableArticulatedAnimation(const char *name) noexcept
    {
        return this->mesh ? this->mesh->disableArticulatedAnimation(this->getArticulatedAnimationPlayer(), name) : false;
    }

    bool MESH::seekArticulatedAnimation(const char *name, const float time) noexcept
    {
        return this->mesh ? this->mesh->seekArticulatedAnimation(this->getArticulatedAnimationPlayer(), name, time) : false;
    }

    bool MESH::getArticulatedAnimationTime(const char *name, float *time) const noexcept
    {
        return this->mesh ? this->mesh->getArticulatedAnimationTime(this->getArticulatedAnimationPlayer(), name, time) : false;
    }

    uint32_t MESH::getTotalSkeletalAnimations() const noexcept
    {
        return mesh ? mesh->getTotalSkeletalAnimations() : 0;
    }

    const char *MESH::getSkeletalAnimationName(const uint32_t index) const noexcept
    {
        return mesh ? mesh->getSkeletalAnimationName(index) : nullptr;
    }

    bool MESH::getSkeletalAnimationDuration(const uint32_t index, float *duration) const noexcept
    {
        return mesh ? mesh->getSkeletalAnimationDuration(index, duration) : false;
    }

    bool MESH::playSkeletalAnimation(const char *name)
    {
        return mesh ? mesh->playSkeletalAnimation(getSkeletalAnimationPlayer(), name) : false;
    }

    bool MESH::pauseSkeletalAnimation() noexcept
    {
        return mesh ? mesh->pauseSkeletalAnimation(getSkeletalAnimationPlayer()) : false;
    }

    bool MESH::resumeSkeletalAnimation() noexcept
    {
        return mesh ? mesh->resumeSkeletalAnimation(getSkeletalAnimationPlayer()) : false;
    }

    bool MESH::seekSkeletalAnimation(const float time)
    {
        return mesh ? mesh->seekSkeletalAnimation(getSkeletalAnimationPlayer(), time) : false;
    }

    bool MESH::getSkeletalAnimationTime(float *time) const noexcept
    {
        return mesh ? mesh->getSkeletalAnimationTime(getSkeletalAnimationPlayer(), time) : false;
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
            this->mesh->updateArticulatedAnimations(this->getArticulatedAnimationPlayer(), device->delta,
                                                     this, this->getOnEndAnimation());
            const VEC3 &position = this->getPosition();
            const VEC3 &angle = this->getAngle();
            const VEC3 &scale = this->getScale();
            const MATRIX *viewMatrix = nullptr;
            const MATRIX *perspectiveMatrix = nullptr;
            if (this->is3DObject())
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &position, &angle, &scale);
                SHADER::updateMvpAndLightMatrices(camera.matrixView, camera.matrixPerspective);
                viewMatrix = &camera.matrixView;
                perspectiveMatrix = &camera.matrixPerspective;
            }
            else if (this->is2dScreenObject())
            {
                VEC3 positionScreen(position.x * camera.scaleScreen2d.x,
                                    position.y * camera.scaleScreen2d.y, position.z);
                device->transformeScreen2dToWorld2d_scaled(position.x, position.y, positionScreen);
                MatrixTranslationRotationScale(&SHADER::modelView, &positionScreen, &angle, &scale);
                SHADER::updateMvpAndLightMatrices(camera.matrixView2d, camera.matrixPerspective2d);
                viewMatrix = &camera.matrixView2d;
                perspectiveMatrix = &camera.matrixPerspective2d;
            }
            else
            {
                MatrixTranslationRotationScale(&SHADER::modelView, &position, &angle, &scale);
                SHADER::updateMvpAndLightMatrices(camera.matrixView2d, camera.matrixPerspective2d);
                viewMatrix = &camera.matrixView2d;
                perspectiveMatrix = &camera.matrixPerspective2d;
            }
            FX &fx = anim->getFx();
            this->setBlendState(anim->getBlendState());
            fx.shader.update();
            fx.setBlendOp();
            BUFFER_MESH *frameBuffer = this->mesh->getBuffer(static_cast<unsigned int>(anim->getIndexCurrentFrame()));
            fx.bindTextureAnimationEffect(frameBuffer ? frameBuffer->getRenderBuffer() : nullptr);
            const uint32_t frameIndex = static_cast<unsigned int>(anim->getIndexCurrentFrame());
            const bool hasSkeletal = this->mesh->hasActiveSkeletalAnimation(this->getSkeletalAnimationPlayer());
            if (hasSkeletal && !this->mesh->updateSkeletalAnimation(this->getSkeletalAnimationPlayer(), device->delta))
                return false;
            const bool rendered = hasSkeletal
                ? this->mesh->renderSkeletal(this->getSkeletalAnimationPlayer(), frameIndex, &fx.shader, this)
                : this->mesh->hasActiveArticulatedAnimations(this->getArticulatedAnimationPlayer())
                ? this->mesh->renderArticulatedStatic(this->getArticulatedAnimationPlayer(), frameIndex, &fx.shader,
                                                      *viewMatrix, *perspectiveMatrix, this)
                : this->mesh->render(frameIndex, &fx.shader, this);
            if (!rendered)
                return false;
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
                this->mesh->updateArticulatedAnimations(this->getArticulatedAnimationPlayer(), device->delta,
                                                         this, this->getOnEndAnimation());
                if (this->mesh->hasActiveSkeletalAnimation(this->getSkeletalAnimationPlayer()))
                    this->mesh->updateSkeletalAnimation(this->getSkeletalAnimationPlayer(), device->delta);
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
