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

#ifndef MESH_3D_GLES_H
#define MESH_3D_GLES_H

#pragma once

#include <core_mbm/core-exports.h>
#include <core_mbm/device.h>
#include <core_mbm/shader-fx.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/animation.h>
#include <core_mbm/physics.h>
#include <functional>

namespace mbm
{

struct SKELETAL_SHARING_COMPATIBILITY;

class MESH : public RENDERIZABLE, public ANIMATION_MANAGER
{
  public:
    API_IMPL MESH(const SCENE *scene, const bool _is3d, const bool _is2dScreen);
    API_IMPL virtual ~MESH();
    API_IMPL void release();
    API_IMPL bool load(const char *fileName);
    // Background-thread-friendly equivalent of load(): does the file I/O + v11 parsing on a worker
    // thread (MESH_MANAGER::loadAsync), runs the exact same
    // finish logic load() runs, then invokes callback(success) - always from pumpAsyncLoads() on
    // the main thread, never inline, matching MESH_MANAGER::loadAsync's own contract - EXCEPT when
    // this->mesh is already set on this specific MESH instance (see the top of the .cpp), which
    // fires callback(true) inline immediately; that early-out is independent of and predates
    // MESH_MANAGER's own (queue-only, never-inline) cache-hit handling.
    API_IMPL void loadAsync(const char *fileName, std::function<void(bool success)> callback);
    API_IMPL const char *getFileName() const;
    API_IMPL bool playArticulatedAnimation(const char *name, const int priority = 0,
                                           const float blendDuration = 0.0f,
                                           const float weight = 1.0f);
    API_IMPL uint32_t getTotalArticulatedAnimations() const noexcept;
    API_IMPL const char *getArticulatedAnimationName(const uint32_t index) const noexcept;
    API_IMPL bool pauseArticulatedAnimation(const char *name) noexcept;
    API_IMPL bool resumeArticulatedAnimation(const char *name) noexcept;
    API_IMPL bool disableArticulatedAnimation(const char *name) noexcept;
    API_IMPL bool seekArticulatedAnimation(const char *name, const float time) noexcept;
    API_IMPL bool getArticulatedAnimationTime(const char *name, float *time) const noexcept;
    API_IMPL uint32_t getTotalSkeletalAnimations() const noexcept;
    API_IMPL const char *getSkeletalAnimationName(uint32_t index) const noexcept;
    API_IMPL bool getSkeletalAnimationDuration(uint32_t index, float *duration) const noexcept;
    API_IMPL bool setSkeletalSkinningMethod(SKELETAL_SHADER_METHOD method) noexcept;
    API_IMPL SKELETAL_SHADER_METHOD getSkeletalSkinningMethod() const noexcept;
    API_IMPL SKELETAL_SHADER_METHOD getResolvedSkeletalSkinningMethod() const noexcept;
    API_IMPL bool setSkeletalExecutionPath(SKELETAL_EXECUTION_PATH path) noexcept;
    API_IMPL SKELETAL_EXECUTION_PATH getSkeletalExecutionPath() const noexcept;
    API_IMPL void getSkeletalSkinningReport(const char **status, const char **resolutionReason,
                                            uint32_t *requiredBoneCount,
                                            uint32_t *effectiveBoneCapacity,
                                            const char **executionPath = nullptr,
                                            const char **executionStatus = nullptr) const noexcept;
    API_IMPL bool playSkeletalAnimation(const char *name);
    API_IMPL bool crossFadeSkeletalAnimation(const char *name, float duration);
    API_IMPL bool pauseSkeletalAnimation() noexcept;
    API_IMPL bool resumeSkeletalAnimation() noexcept;
    API_IMPL bool stopSkeletalAnimation() noexcept;
    API_IMPL bool seekSkeletalAnimation(float time);
    API_IMPL bool getSkeletalAnimationTime(float *time) const noexcept;
    API_IMPL bool setSkeletalAnimationPlaybackSpeed(float speed) noexcept;
    API_IMPL float getSkeletalAnimationPlaybackSpeed() const noexcept;
    // Transient per-instance second clip, composed in parent-relative local TRS with strict weight
    // [0,1]. Absolute and bind-relative Additive modes are explicit and never serialized.
    API_IMPL bool playSkeletalAnimationAbsoluteLayer(const char *name, float weight);
    API_IMPL bool playSkeletalAnimationAdditiveLayer(const char *name, float weight);
    API_IMPL bool pauseSkeletalAnimationLayer() noexcept;
    API_IMPL bool resumeSkeletalAnimationLayer() noexcept;
    API_IMPL bool isSkeletalAnimationLayerPaused() const noexcept;
    API_IMPL bool stopSkeletalAnimationAbsoluteLayer() noexcept;
    API_IMPL bool seekSkeletalAnimationAbsoluteLayer(float time);
    API_IMPL bool setSkeletalAnimationAbsoluteLayerWeight(float weight);
    API_IMPL bool fadeSkeletalAnimationAbsoluteLayer(float targetWeight, float duration);
    API_IMPL bool getSkeletalAnimationAbsoluteLayerWeight(float *weight) const noexcept;
    API_IMPL bool getSkeletalAnimationAbsoluteLayerTime(float *time) const noexcept;
    API_IMPL bool setSkeletalAnimationLayerBoneWeight(uint64_t boneId, float weight);
    API_IMPL bool setSkeletalAnimationLayerBoneWeights(const uint64_t *boneIds,
                                                       const float *weights, uint32_t count);
    API_IMPL bool getSkeletalAnimationLayerBoneWeight(uint64_t boneId, float *weight) const noexcept;
    API_IMPL bool clearSkeletalAnimationLayerMask();
    API_IMPL uint32_t getSkeletalAnimationPoseBoneCount() const noexcept;
    API_IMPL bool getSkeletalAnimationPoseBone(uint32_t boneIndex, uint64_t *boneId,
                                               int32_t *parentIndex,
                                               MATRIX *globalMatrix) const noexcept;
    API_IMPL bool getSkeletalBoneTransform(const char *boneName, bool worldSpace,
                                           uint64_t *boneId, MATRIX *matrix, VEC3 *position,
                                           float rotation[4], VEC3 *angle, VEC3 *scale) const noexcept;
    API_IMPL bool getSkeletalRootMotionDelta(const char *boneName, bool worldSpace,
                                             uint64_t *boneId,
                                             VEC3 *translation) const noexcept;
    API_IMPL bool getSkeletalSharingCompatibility(const MESH &other,
                                                  SKELETAL_SHARING_COMPATIBILITY &out) const noexcept;
    API_IMPL bool enableSkeletalPoseSharing(MESH &source) noexcept;
    API_IMPL bool disableSkeletalPoseSharing() noexcept;
    API_IMPL bool getSkeletalPoseSharing(const MESH **source, bool *active,
                                         const char **reason) const noexcept;
    API_IMPL bool enableAutomaticSkeletalRootMotion(const char *boneName,
                                                    bool applyRotation = false) noexcept;
    API_IMPL bool disableAutomaticSkeletalRootMotion() noexcept;
    API_IMPL bool getAutomaticSkeletalRootMotionBone(const char **boneName,
                                                     uint64_t *boneId,
                                                     bool *applyRotation = nullptr) const noexcept;
    API_IMPL bool setSkeletalAuthoringPalette(SKELETAL_SHADER_METHOD method,
                                              const float *rows, uint32_t rowCount,
                                              const uint64_t *orderedBoneIds, uint32_t boneIdCount,
                                              float time, char *errorOut, int errorOutLen) noexcept;
    API_IMPL FX*  getFx() const override;
	  API_IMPL ANIMATION_MANAGER*  getAnimationManager() override;
    FVF_PROVIDE_BY_ENGINE getFvfFromBuffer() const noexcept override;

  private:
    struct SKELETAL_POSE_SHARING_STATE;
    struct CPU_SKELETAL_RENDER_STATE;
    bool                     render() override;
    bool                     onRestoreDevice() override;
    bool                     isOnFrustum() override;
    const mbm::INFO_PHYSICS *getInfoPhysics() const override;
    const MESH_MBM *         getMesh() const override;
    bool                     isLoaded() const override;
    bool                     canUseSkeletalPoseSharing(const char **reason) const noexcept;
    void                     detachSkeletalPoseSharingSource() noexcept;
    void                     detachSkeletalPoseSharingFollowers() noexcept;
    bool                     renderCpuSkeletal(const SKELETAL_ANIMATION_PLAYER &player,
                                               uint32_t frameIndex, SHADER *shader);
    void                     releaseCpuSkeletalRenderState() noexcept;
    MESH_MBM *               mesh;
    SKELETAL_POSE_SHARING_STATE *skeletalPoseSharingState;
    CPU_SKELETAL_RENDER_STATE *cpuSkeletalRenderState;
    };
}

#endif
