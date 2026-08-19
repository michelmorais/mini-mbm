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
#include <skeletal-execution-policy.h>

#include <algorithm>
#include <vector>

namespace mbm
{
    struct MESH::SKELETAL_POSE_SHARING_STATE
    {
        MESH *source = nullptr;
        std::vector<MESH *> followers;
    };

    struct MESH::CPU_SKELETAL_RENDER_STATE
    {
        BUFFER_MESH dynamicBuffer;
        std::vector<VEC3> positions;
        std::vector<VEC3> normals;
        std::vector<VEC2> uvs;
        SKELETAL_EXECUTION_PATH requestedExecutionPath = skeletal::defaultSkeletalExecutionPath();
        SKELETAL_EXECUTION_PATH resolvedExecutionPath = SKELETAL_EXECUTION_PATH::GPU;
        const char *executionReason = "not-loaded";
        bool initialized = false;
    };

    MESH::MESH(const SCENE *scene, const bool _is3d, const bool _is2dScreen)
        : RENDERIZABLE(scene->getIdScene(), TYPE_CLASS_MESH, _is3d && _is2dScreen == false, _is2dScreen)
    {
        this->setIndexAnimation(0);
        this->mesh                  = nullptr;
        this->skeletalPoseSharingState = new SKELETAL_POSE_SHARING_STATE();
        this->cpuSkeletalRenderState = new CPU_SKELETAL_RENDER_STATE();
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->addRenderizable(this);
    }
    
    MESH::~MESH()
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        device->removeRenderizable(this);
        this->release();
        delete this->cpuSkeletalRenderState;
        this->cpuSkeletalRenderState = nullptr;
        delete this->skeletalPoseSharingState;
        this->skeletalPoseSharingState = nullptr;
    }
    
    void MESH::release()
    {
        this->detachSkeletalPoseSharingSource();
        this->detachSkeletalPoseSharingFollowers();
        this->releaseAnimation();
        this->releaseCpuSkeletalRenderState();
        this->setIndexAnimation(0);
        this->mesh                  = nullptr;
        this->resetArticulatedAnimationPlayer();
        this->resetSkeletalAnimationPlayer();
    }

    void MESH::releaseCpuSkeletalRenderState() noexcept
    {
        if (!cpuSkeletalRenderState)
            return;
        cpuSkeletalRenderState->dynamicBuffer.release();
        cpuSkeletalRenderState->positions.clear();
        cpuSkeletalRenderState->normals.clear();
        cpuSkeletalRenderState->uvs.clear();
        cpuSkeletalRenderState->resolvedExecutionPath =
            cpuSkeletalRenderState->requestedExecutionPath == SKELETAL_EXECUTION_PATH::AUTO
                ? SKELETAL_EXECUTION_PATH::GPU
                : cpuSkeletalRenderState->requestedExecutionPath;
        cpuSkeletalRenderState->executionReason =
            cpuSkeletalRenderState->requestedExecutionPath == SKELETAL_EXECUTION_PATH::AUTO ? "not-loaded" :
            cpuSkeletalRenderState->requestedExecutionPath == SKELETAL_EXECUTION_PATH::CPU ? "explicit-cpu" :
            "explicit-gpu";
        cpuSkeletalRenderState->initialized = false;
    }
    
    bool MESH::load(const char *fileName)
    {
        if (this->mesh)
            return true;
        MESH_MANAGER *mehManager = MESH_MANAGER::getInstance();
        this->mesh               = mehManager->load(fileName, this);
        if (this->mesh)
        {
            this->mesh->resolveSkeletalSkinningMethod(this->getSkeletalAnimationPlayer());
            this->resolveSkeletalExecutionPath();
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
            this->mesh->resolveSkeletalSkinningMethod(this->getSkeletalAnimationPlayer());
            this->resolveSkeletalExecutionPath();
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

    bool MESH::setSkeletalSkinningMethod(const SKELETAL_SHADER_METHOD method) noexcept
    {
        if (mesh || (method != SKELETAL_SHADER_METHOD::LBS &&
                     method != SKELETAL_SHADER_METHOD::DQS_RIGID &&
                     method != SKELETAL_SHADER_METHOD::AUTO))
            return false;
        getSkeletalAnimationPlayer().setSkinningMethod(method);
        return true;
    }

    SKELETAL_SHADER_METHOD MESH::getSkeletalSkinningMethod() const noexcept
    {
        return getSkeletalAnimationPlayer().getSkinningMethod();
    }

    SKELETAL_SHADER_METHOD MESH::getResolvedSkeletalSkinningMethod() const noexcept
    {
        return getSkeletalAnimationPlayer().getResolvedSkinningMethod();
    }

    bool MESH::setSkeletalExecutionPath(const SKELETAL_EXECUTION_PATH path) noexcept
    {
        if (mesh || (path != SKELETAL_EXECUTION_PATH::GPU &&
                     path != SKELETAL_EXECUTION_PATH::CPU &&
                     path != SKELETAL_EXECUTION_PATH::AUTO))
            return false;
        if (cpuSkeletalRenderState)
        {
            cpuSkeletalRenderState->initialized = false;
            cpuSkeletalRenderState->requestedExecutionPath = path;
            cpuSkeletalRenderState->resolvedExecutionPath =
                path == SKELETAL_EXECUTION_PATH::AUTO ? SKELETAL_EXECUTION_PATH::GPU : path;
            cpuSkeletalRenderState->executionReason =
                path == SKELETAL_EXECUTION_PATH::AUTO ? "not-loaded" :
                path == SKELETAL_EXECUTION_PATH::CPU ? "explicit-cpu" : "explicit-gpu";
        }
        return true;
    }

    SKELETAL_EXECUTION_PATH MESH::getSkeletalExecutionPath() const noexcept
    {
        return cpuSkeletalRenderState ?
            cpuSkeletalRenderState->requestedExecutionPath : skeletal::defaultSkeletalExecutionPath();
    }

    SKELETAL_EXECUTION_PATH MESH::getResolvedSkeletalExecutionPath() const noexcept
    {
        return cpuSkeletalRenderState ?
            cpuSkeletalRenderState->resolvedExecutionPath : SKELETAL_EXECUTION_PATH::GPU;
    }

    void MESH::resolveSkeletalExecutionPath() noexcept
    {
        if (!cpuSkeletalRenderState)
            return;
        const SKELETAL_EXECUTION_PATH requested = cpuSkeletalRenderState->requestedExecutionPath;
        if (requested == SKELETAL_EXECUTION_PATH::GPU)
        {
            cpuSkeletalRenderState->resolvedExecutionPath = SKELETAL_EXECUTION_PATH::GPU;
            cpuSkeletalRenderState->executionReason = "explicit-gpu";
            return;
        }
        if (requested == SKELETAL_EXECUTION_PATH::CPU)
        {
            cpuSkeletalRenderState->resolvedExecutionPath = SKELETAL_EXECUTION_PATH::CPU;
            cpuSkeletalRenderState->executionReason = "explicit-cpu";
            return;
        }
        const char *cpuReason = nullptr;
        const SKELETAL_SHADER_METHOD method = getResolvedSkeletalSkinningMethod();
        const bool gpuSupported = mesh && mesh->supportsGpuSkeletalPath(method);
        const bool cpuAvailable = mesh && mesh->canUseCpuSkeletalPath(method, &getSkeletalAnimationPlayer(), &cpuReason);
        const skeletal::SKELETAL_EXECUTION_RESOLUTION resolution =
            skeletal::resolveSkeletalExecutionPolicy(requested, mesh != nullptr, gpuSupported, cpuAvailable, cpuReason);
        cpuSkeletalRenderState->resolvedExecutionPath = resolution.resolvedPath;
        cpuSkeletalRenderState->executionReason = resolution.reason;
    }

    void MESH::getSkeletalSkinningReport(const char **status, const char **resolutionReason,
                                         uint32_t *requiredBoneCount,
                                         uint32_t *effectiveBoneCapacity,
                                         const char **executionPath,
                                         const char **executionStatus,
                                         const char **requestedExecutionPath,
                                         const char **resolvedExecutionPath,
                                         const char **executionReason) const noexcept
    {
        const auto pathName = [](const SKELETAL_EXECUTION_PATH path) -> const char *
        {
            return path == SKELETAL_EXECUTION_PATH::AUTO ? "auto" :
                path == SKELETAL_EXECUTION_PATH::CPU ? "cpu" : "gpu";
        };
        if (resolutionReason)
            *resolutionReason = getSkeletalAnimationPlayer().getSkinningResolutionReason();
        if (requestedExecutionPath)
            *requestedExecutionPath = pathName(getSkeletalExecutionPath());
        if (resolvedExecutionPath)
            *resolvedExecutionPath = pathName(getResolvedSkeletalExecutionPath());
        if (executionReason)
            *executionReason = cpuSkeletalRenderState ? cpuSkeletalRenderState->executionReason : "not-loaded";
        if (executionPath)
            *executionPath = pathName(getResolvedSkeletalExecutionPath());
        if (executionStatus)
        {
            if (!mesh)
                *executionStatus = "not-loaded";
            else if (getResolvedSkeletalExecutionPath() == SKELETAL_EXECUTION_PATH::GPU)
                *executionStatus = mesh->supportsGpuSkeletalPath(getResolvedSkeletalSkinningMethod())
                    ? "gpu-ready" : "gpu-skeletal-unavailable";
            else
            {
                const char *reason = nullptr;
                const SKELETAL_SHADER_METHOD method = getResolvedSkeletalSkinningMethod();
                if (mesh->canUseCpuSkeletalPath(method, &getSkeletalAnimationPlayer(), &reason))
                    *executionStatus = method == SKELETAL_SHADER_METHOD::DQS_RIGID
                        ? "cpu-dqs-ready" : "cpu-lbs-ready";
                else
                    *executionStatus = reason ? reason : "cpu-skeletal-unavailable";
            }
        }
        if (mesh)
            mesh->getSkeletalSkinningReport(getResolvedSkeletalSkinningMethod(), status,
                                             requiredBoneCount, effectiveBoneCapacity);
        else
        {
            if (status) *status = "not-loaded";
            if (requiredBoneCount) *requiredBoneCount = 0;
            if (effectiveBoneCapacity) *effectiveBoneCapacity = 0;
        }
    }

    bool MESH::playSkeletalAnimation(const char *name)
    {
        return mesh ? mesh->playSkeletalAnimation(getSkeletalAnimationPlayer(), name) : false;
    }

    bool MESH::crossFadeSkeletalAnimation(const char *name, const float duration)
    {
        return mesh ? mesh->crossFadeSkeletalAnimation(
            getSkeletalAnimationPlayer(), name, duration) : false;
    }

    bool MESH::pauseSkeletalAnimation() noexcept
    {
        return mesh ? mesh->pauseSkeletalAnimation(getSkeletalAnimationPlayer()) : false;
    }

    bool MESH::resumeSkeletalAnimation() noexcept
    {
        return mesh ? mesh->resumeSkeletalAnimation(getSkeletalAnimationPlayer()) : false;
    }

    bool MESH::stopSkeletalAnimation() noexcept
    {
        return mesh ? mesh->stopSkeletalAnimation(getSkeletalAnimationPlayer()) : false;
    }

    bool MESH::seekSkeletalAnimation(const float time)
    {
        return mesh ? mesh->seekSkeletalAnimation(getSkeletalAnimationPlayer(), time) : false;
    }

    bool MESH::getSkeletalAnimationTime(float *time) const noexcept
    {
        return mesh ? mesh->getSkeletalAnimationTime(getSkeletalAnimationPlayer(), time) : false;
    }

    bool MESH::setSkeletalAnimationPlaybackSpeed(const float speed) noexcept
    {
        return mesh ? mesh->setSkeletalAnimationPlaybackSpeed(
            getSkeletalAnimationPlayer(), speed) : false;
    }

    float MESH::getSkeletalAnimationPlaybackSpeed() const noexcept
    {
        return mesh ? mesh->getSkeletalAnimationPlaybackSpeed(getSkeletalAnimationPlayer()) : 1.0f;
    }

    bool MESH::playSkeletalAnimationAbsoluteLayer(const char *name, const float weight)
    {
        return mesh ? mesh->playSkeletalAnimationAbsoluteLayer(
            getSkeletalAnimationPlayer(), name, weight) : false;
    }

    bool MESH::playSkeletalAnimationAdditiveLayer(const char *name, const float weight)
    {
        return mesh ? mesh->playSkeletalAnimationAdditiveLayer(
            getSkeletalAnimationPlayer(), name, weight) : false;
    }

    bool MESH::pauseSkeletalAnimationLayer() noexcept
    {
        return mesh ? mesh->pauseSkeletalAnimationLayer(getSkeletalAnimationPlayer()) : false;
    }

    bool MESH::resumeSkeletalAnimationLayer() noexcept
    {
        return mesh ? mesh->resumeSkeletalAnimationLayer(getSkeletalAnimationPlayer()) : false;
    }

    bool MESH::isSkeletalAnimationLayerPaused() const noexcept
    {
        return mesh ? mesh->isSkeletalAnimationLayerPaused(getSkeletalAnimationPlayer()) : false;
    }

    bool MESH::stopSkeletalAnimationAbsoluteLayer() noexcept
    {
        return mesh ? mesh->stopSkeletalAnimationAbsoluteLayer(getSkeletalAnimationPlayer()) : false;
    }

    bool MESH::seekSkeletalAnimationAbsoluteLayer(const float time)
    {
        return mesh ? mesh->seekSkeletalAnimationAbsoluteLayer(
            getSkeletalAnimationPlayer(), time) : false;
    }

    bool MESH::setSkeletalAnimationAbsoluteLayerWeight(const float weight)
    {
        return mesh ? mesh->setSkeletalAnimationAbsoluteLayerWeight(
            getSkeletalAnimationPlayer(), weight) : false;
    }

    bool MESH::fadeSkeletalAnimationAbsoluteLayer(const float targetWeight, const float duration)
    {
        return mesh ? mesh->fadeSkeletalAnimationAbsoluteLayer(
            getSkeletalAnimationPlayer(), targetWeight, duration) : false;
    }

    bool MESH::getSkeletalAnimationAbsoluteLayerWeight(float *weight) const noexcept
    {
        return mesh ? mesh->getSkeletalAnimationAbsoluteLayerWeight(
            getSkeletalAnimationPlayer(), weight) : false;
    }

    bool MESH::getSkeletalAnimationAbsoluteLayerTime(float *time) const noexcept
    {
        return mesh ? mesh->getSkeletalAnimationAbsoluteLayerTime(
            getSkeletalAnimationPlayer(), time) : false;
    }

    bool MESH::setSkeletalAnimationLayerBoneWeight(const uint64_t boneId, const float weight)
    {
        return mesh ? mesh->setSkeletalAnimationLayerBoneWeight(
            getSkeletalAnimationPlayer(), boneId, weight) : false;
    }

    bool MESH::setSkeletalAnimationLayerBoneWeights(const uint64_t *boneIds,
                                                     const float *weights,
                                                     const uint32_t count)
    {
        return mesh ? mesh->setSkeletalAnimationLayerBoneWeights(
            getSkeletalAnimationPlayer(), boneIds, weights, count) : false;
    }

    bool MESH::getSkeletalAnimationLayerBoneWeight(const uint64_t boneId,
                                                    float *weight) const noexcept
    {
        return mesh ? mesh->getSkeletalAnimationLayerBoneWeight(
            getSkeletalAnimationPlayer(), boneId, weight) : false;
    }

    bool MESH::clearSkeletalAnimationLayerMask()
    {
        return mesh ? mesh->clearSkeletalAnimationLayerMask(getSkeletalAnimationPlayer()) : false;
    }

    uint32_t MESH::getSkeletalAnimationPoseBoneCount() const noexcept
    {
        return mesh ? mesh->getSkeletalAnimationPoseBoneCount(getSkeletalAnimationPlayer()) : 0;
    }

    bool MESH::getSkeletalAnimationPoseBone(const uint32_t boneIndex, uint64_t *boneId,
                                             int32_t *parentIndex,
                                             MATRIX *globalMatrix) const noexcept
    {
        if (!mesh || !boneId || !parentIndex || !globalMatrix)
            return false;
        SKELETAL_RUNTIME_POSE_BONE_INFO out;
        if (!mesh->getSkeletalAnimationPoseBone(getSkeletalAnimationPlayer(), boneIndex, out))
            return false;
        *boneId = out.boneId;
        *parentIndex = out.parentIndex;
        *globalMatrix = out.globalMatrix;
        return true;
    }

    bool MESH::getSkeletalBoneTransform(const char *boneName, const bool worldSpace,
                                        uint64_t *boneId, MATRIX *matrix, VEC3 *position,
                                        float rotation[4], VEC3 *angle, VEC3 *scale) const noexcept
    {
        if (!mesh)
            return false;
        MATRIX modelMatrix;
        const MATRIX *modelMatrixPtr = nullptr;
        if (worldSpace)
        {
            const VEC3 &objectPosition = getPosition();
            const VEC3 &objectAngle = getAngle();
            const VEC3 &objectScale = getScale();
            MatrixTranslationRotationScale(&modelMatrix, &objectPosition, &objectAngle, &objectScale);
            modelMatrixPtr = &modelMatrix;
        }
        return mesh->getSkeletalBoneTransform(getSkeletalAnimationPlayer(), boneName,
                                              modelMatrixPtr, boneId, matrix, position,
                                              rotation, angle, scale);
    }

    bool MESH::getSkeletalRootMotionDelta(const char *boneName, const bool worldSpace,
                                           uint64_t *boneId, VEC3 *translation) const noexcept
    {
        if (!mesh)
            return false;
        MATRIX modelMatrix;
        const MATRIX *modelMatrixPtr = nullptr;
        if (worldSpace)
        {
            const VEC3 &objectPosition = getPosition();
            const VEC3 &objectAngle = getAngle();
            const VEC3 &objectScale = getScale();
            MatrixTranslationRotationScale(&modelMatrix, &objectPosition, &objectAngle,
                                           &objectScale);
            modelMatrixPtr = &modelMatrix;
        }
        return mesh->getSkeletalRootMotionDelta(getSkeletalAnimationPlayer(), boneName,
                                                modelMatrixPtr, boneId, translation);
    }

    bool MESH::getSkeletalSharingCompatibility(const MESH &other,
                                                SKELETAL_SHARING_COMPATIBILITY &out) const noexcept
    {
        if (!mesh || !other.mesh)
        {
            out = SKELETAL_SHARING_COMPATIBILITY();
            return false;
        }
        return mesh->getSkeletalSharingCompatibility(*other.mesh, out);
    }

    bool MESH::enableSkeletalPoseSharing(MESH &source) noexcept
    {
        if (&source == this || !mesh || !source.mesh)
            return false;
        // This first slice is deliberately one level deep. A mesh cannot become a follower while
        // other meshes follow it, and a follower cannot be used as another source.
        if (!skeletalPoseSharingState->followers.empty() ||
            source.skeletalPoseSharingState->source)
            return false;
        SKELETAL_SHARING_COMPATIBILITY report;
        if (!mesh->getSkeletalSharingCompatibility(*source.mesh, report) || !report.compatible)
            return false;
        if (getResolvedSkeletalSkinningMethod() != source.getResolvedSkeletalSkinningMethod())
            return false;
        if (skeletalPoseSharingState->source == &source)
            return true;
        detachSkeletalPoseSharingSource();
        skeletalPoseSharingState->source = &source;
        source.skeletalPoseSharingState->followers.push_back(this);
        return true;
    }

    bool MESH::disableSkeletalPoseSharing() noexcept
    {
        detachSkeletalPoseSharingSource();
        return true;
    }

    bool MESH::getSkeletalPoseSharing(const MESH **source, bool *active,
                                      const char **reason) const noexcept
    {
        if (source)
            *source = skeletalPoseSharingState->source;
        const bool sharingActive = canUseSkeletalPoseSharing(reason);
        if (active)
            *active = sharingActive;
        return skeletalPoseSharingState->source != nullptr;
    }

    bool MESH::canUseSkeletalPoseSharing(const char **reason) const noexcept
    {
        MESH *source = skeletalPoseSharingState->source;
        if (!source)
        {
            if (reason) *reason = "disabled";
            return false;
        }
        if (!mesh)
        {
            if (reason) *reason = "not_loaded";
            return false;
        }
        if (!source->mesh)
        {
            if (reason) *reason = "source_not_loaded";
            return false;
        }
        SKELETAL_SHARING_COMPATIBILITY report;
        if (!mesh->getSkeletalSharingCompatibility(*source->mesh, report) ||
            !report.compatible)
        {
            if (reason) *reason = report.reason ? report.reason : "incompatible";
            return false;
        }
        if (getResolvedSkeletalSkinningMethod() !=
            source->getResolvedSkeletalSkinningMethod())
        {
            if (reason) *reason = "skinning_method_mismatch";
            return false;
        }
        if (getResolvedSkeletalExecutionPath() != source->getResolvedSkeletalExecutionPath())
        {
            if (reason) *reason = "execution_path_mismatch";
            return false;
        }
        if (!source->mesh->hasSkeletalRenderPalette(source->getSkeletalAnimationPlayer()))
        {
            if (reason) *reason = "source_pose_inactive";
            return false;
        }
        if (reason) *reason = "active";
        return true;
    }

    void MESH::detachSkeletalPoseSharingSource() noexcept
    {
        MESH *source = skeletalPoseSharingState->source;
        if (!source)
            return;
        auto &followers = source->skeletalPoseSharingState->followers;
        followers.erase(std::remove(followers.begin(), followers.end(), this), followers.end());
        skeletalPoseSharingState->source = nullptr;
    }

    void MESH::detachSkeletalPoseSharingFollowers() noexcept
    {
        for (MESH *follower : skeletalPoseSharingState->followers)
            if (follower && follower->skeletalPoseSharingState->source == this)
                follower->skeletalPoseSharingState->source = nullptr;
        skeletalPoseSharingState->followers.clear();
    }

    bool MESH::enableAutomaticSkeletalRootMotion(const char *boneName,
                                                 const bool applyRotation) noexcept
    {
        return mesh ? mesh->enableAutomaticSkeletalRootMotion(
            getSkeletalAnimationPlayer(), boneName, applyRotation) : false;
    }

    bool MESH::disableAutomaticSkeletalRootMotion() noexcept
    {
        return mesh ? mesh->disableAutomaticSkeletalRootMotion(
            getSkeletalAnimationPlayer()) : false;
    }

    bool MESH::getAutomaticSkeletalRootMotionBone(const char **boneName,
                                                  uint64_t *boneId,
                                                  bool *applyRotation) const noexcept
    {
        return mesh ? mesh->getAutomaticSkeletalRootMotionBone(
            getSkeletalAnimationPlayer(), boneName, boneId, applyRotation) : false;
    }

    bool MESH::setSkeletalAuthoringPalette(const SKELETAL_SHADER_METHOD method,
                                           const float *rows, const uint32_t rowCount,
                                           const uint64_t *orderedBoneIds, const uint32_t boneIdCount,
                                           const float time, char *errorOut,
                                           const int errorOutLen) noexcept
    {
        return mesh && mesh->setSkeletalAuthoringPalette(getSkeletalAnimationPlayer(), method,
                                                         rows, rowCount, orderedBoneIds,
                                                         boneIdCount, time, errorOut, errorOutLen);
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
            const bool hasSharedSkeletal = this->canUseSkeletalPoseSharing(nullptr);
            const bool hasSkeletal = hasSharedSkeletal ||
                this->mesh->hasActiveSkeletalAnimation(this->getSkeletalAnimationPlayer());
            if (!hasSharedSkeletal && hasSkeletal && !this->mesh->updateSkeletalAnimation(
                    this->getSkeletalAnimationPlayer(), device->delta, this, this->getOnEndAnimation()))
                return false;
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
            const bool useCpuSkeletal = hasSkeletal &&
                getResolvedSkeletalExecutionPath() == SKELETAL_EXECUTION_PATH::CPU;
            const bool rendered = useCpuSkeletal
                ? this->renderCpuSkeletal(
                    hasSharedSkeletal ? skeletalPoseSharingState->source->getSkeletalAnimationPlayer() :
                    this->getSkeletalAnimationPlayer(), frameIndex, &fx.shader)
                : hasSkeletal
                ? this->mesh->renderSkeletal(
                    hasSharedSkeletal ? skeletalPoseSharingState->source->getSkeletalAnimationPlayer() :
                    this->getSkeletalAnimationPlayer(), frameIndex, &fx.shader, this)
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

    bool MESH::renderCpuSkeletal(const SKELETAL_ANIMATION_PLAYER &player,
                                 const uint32_t frameIndex, SHADER *shader)
    {
        if (!mesh || !cpuSkeletalRenderState)
            return false;
        return mesh->renderCpuSkeletal(player, frameIndex, cpuSkeletalRenderState->dynamicBuffer,
                                       cpuSkeletalRenderState->positions,
                                       cpuSkeletalRenderState->normals,
                                       cpuSkeletalRenderState->uvs,
                                       cpuSkeletalRenderState->initialized,
                                       shader, this);
    }
    
    bool MESH::onRestoreDevice()
    {
        this->releaseCpuSkeletalRenderState();
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
                const bool hasSharedSkeletal = this->canUseSkeletalPoseSharing(nullptr);
                if (!hasSharedSkeletal &&
                    this->mesh->hasActiveSkeletalAnimation(this->getSkeletalAnimationPlayer()))
                    this->mesh->updateSkeletalAnimation(this->getSkeletalAnimationPlayer(),
                                                        device->delta, this,
                                                        this->getOnEndAnimation());
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
