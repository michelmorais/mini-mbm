/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef ANIMATION_SHADERS_GLES_H
#define ANIMATION_SHADERS_GLES_H

#include <memory>

#include "core-exports.h"
#include "shader.h"
#include "renderizable.h"
#include "blend.h"
#include "shader-fx.h"

namespace util
{
    struct HEADER_ANIMATION;
    enum TYPE_MESH : char;
}

namespace mbm
{
    class ARTICULATED_ANIMATION_PLAYER;
    class SKELETAL_ANIMATION_PLAYER;
    class TEXTURE;
    class MESH_MBM;
    
    typedef void (*OnEndAnimation)(const char *nameAnimation, RENDERIZABLE *renderizable);
    typedef void(*OnEndEffect)(const char *shaderName,RENDERIZABLE *renderizable);
    typedef void (*OnEndAnimationCallBack)(const char *fileNameAnimation, void *renderizable);

    class EFFECT_SHADER
    {
    public:
        API_IMPL EFFECT_SHADER() noexcept;
        API_IMPL virtual ~EFFECT_SHADER();

        API_IMPL STATUS_FX getStatusFx() const noexcept;
        API_IMPL void setStatusFx(const STATUS_FX status) noexcept;
        API_IMPL TYPE_ANIMATION getTypeAnim() const noexcept;
        API_IMPL void setTypeAnim(const TYPE_ANIMATION type) noexcept;
        API_IMPL BASE_SHADER *getCurrentShader() const noexcept;
        API_IMPL void setCurrentShader(BASE_SHADER *shader) noexcept;
        API_IMPL float getTimeAnimation() const noexcept;
        API_IMPL void setTimeAnimation(const float time) noexcept;
        API_IMPL BASE_SHADER *loadEffect(const char *fileNameShader, const char *code, const TYPE_ANIMATION typeAnimationShader);
    
        API_IMPL void disableEffect();
        API_IMPL void restart();
        API_IMPL void updateEffect(const float delta);
        API_IMPL bool endEffect();
        API_IMPL bool isEndedFx()const;
        API_IMPL void forceEndFx();
        API_IMPL bool setNewTimeAnim(const float newTimeAnim);
        API_IMPL bool adjustMinMax(const uint32_t indexVar, const float min[4], const float max[4], const float timeAnim);
      private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };


    class ANIMATION
    {
        friend class ANIMATION_MANAGER;
        friend class ANIMATION_BACKUP;
      public:
        static constexpr int NAME_ANIMATION_SIZE = 32;
    
        API_IMPL ANIMATION();
        API_IMPL virtual ~ANIMATION();
        API_IMPL const char *getNameAnimation() const noexcept;
        API_IMPL void setNameAnimation(const char *name) noexcept;
        API_IMPL float getIntervalChangeFrame() const noexcept;
        API_IMPL void setIntervalChangeFrame(const float interval) noexcept;
        API_IMPL int getIndexInitialFrame() const noexcept;
        API_IMPL void setIndexInitialFrame(const int index) noexcept;
        API_IMPL int getIndexFinalFrame() const noexcept;
        API_IMPL void setIndexFinalFrame(const int index) noexcept;
        API_IMPL int getIndexCurrentFrame() const noexcept;
        API_IMPL void setIndexCurrentFrame(const int index) noexcept;
        API_IMPL BLEND_STATE getBlendState() const noexcept;
        API_IMPL void setBlendState(const BLEND_STATE blend) noexcept;
        API_IMPL bool isEnded() const noexcept;
        API_IMPL void setEnded(const bool ended) noexcept;
        API_IMPL bool isCurrentWayGrowing() const noexcept;
        API_IMPL void setCurrentWayGrowing(const bool growing) noexcept;
        API_IMPL TYPE_ANIMATION getType() const noexcept;
        API_IMPL void setType(const TYPE_ANIMATION typeAnimation) noexcept;
        API_IMPL FX &getFx() noexcept;
        API_IMPL const FX &getFx() const noexcept;
        API_IMPL void restartAnimation();
        API_IMPL void updateAnimation(const float delta, RENDERIZABLE *me,
                                        OnEndAnimation onEndAnimation,
                                        OnEndEffect onEndFX);
      private:
        struct Impl;
        std::unique_ptr<Impl> impl;
        float getCurrentTimeToChangeAnimation() const noexcept;
        void setCurrentTimeToChangeAnimation(const float time) noexcept;
        void addCurrentTimeToChangeAnimation(const float delta) noexcept;
    };

    class ANIMATION_BACKUP
    {
    public:
        API_IMPL ANIMATION_BACKUP() noexcept;
        API_IMPL ~ANIMATION_BACKUP() noexcept;
        // Prevent copying
        // Move semantics
        ANIMATION_BACKUP(ANIMATION_BACKUP&& other) = delete;
        ANIMATION_BACKUP& operator=(ANIMATION_BACKUP&& other) = delete;

        // Prevent copying
        ANIMATION_BACKUP(const ANIMATION_BACKUP&) = delete;
        ANIMATION_BACKUP& operator=(const ANIMATION_BACKUP&) = delete;

        API_IMPL void backup(ANIMATION_MANAGER* animationManager);
        API_IMPL void restore(ANIMATION_MANAGER* animationManager);
    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
        void clearBackup() noexcept;
    };

    // Result of ANIMATION_MANAGER::populateAnimationsFromMesh - the caller (a concrete renderizable type's
    // own load()/loadAsync()) decides what to do on failure, since releasing the renderizable itself
    // on an animation failure requires calling that type's own release() override, which
    // ANIMATION_MANAGER/RENDERIZABLE have no shared virtual hook for.
    enum class MeshLoadFinishResult
    {
        OK,
        TYPE_MISMATCH,    // mesh->release() already called internally; caller's own mesh pointer is
                          // intentionally left as-is, matching this codebase's pre-existing behavior
        ANIMATION_FAILED, // caller must call its own release()
    };

    class ANIMATION_MANAGER
    {
      public:
        API_IMPL ANIMATION_MANAGER() noexcept;
        API_IMPL virtual ~ANIMATION_MANAGER();
        API_IMPL void populateTextureAnimationEffectFromMesh(MESH_MBM *mesh);
        API_IMPL bool populateAnimationFromHeader(MESH_MBM *mesh, util::HEADER_ANIMATION *header, const uint32_t index);
        // Shared by MESH/SPRITE/TILE's load()/loadAsync(): optional type validation (skip by passing
        // expectedType=nullptr, matching MESH's existing no-check behavior) + populating every
        // animation header + the legacy TextureAnimationEffect population. Deliberately excludes
        // setInternalFileName/restartAnimation/updateAABB/positioning - callers run those themselves,
        // exactly where they already do today.
        API_IMPL MeshLoadFinishResult populateAnimationsFromMesh(MESH_MBM *mesh, const util::TYPE_MESH *expectedType, const char *typeNameForError);
        API_IMPL ANIMATION *getAnimation() const;
        API_IMPL ANIMATION *getAnimation(const uint32_t index) const;
        API_IMPL uint32_t getTotalAnimation() const;
        API_IMPL uint32_t getIndexAnimation() const;
        API_IMPL void setIndexAnimation(const uint32_t newIndex) noexcept;
        API_IMPL OnEndAnimation getOnEndAnimation() const noexcept;
        API_IMPL void setOnEndAnimation(OnEndAnimation callback) noexcept;
        API_IMPL OnEndEffect getOnEndFx() const noexcept;
        API_IMPL void setOnEndFx(OnEndEffect callback) noexcept;
        API_IMPL bool setAnimationByIndex(const uint32_t newIndex);
        API_IMPL void setAnimation(const char *name);
        API_IMPL void restartAnimation();
        API_IMPL void removeAnimation(const uint32_t index);
        API_IMPL char *getNameAnimation(const uint32_t index) const;
        API_IMPL char *getNameAnimation() const;
        API_IMPL uint32_t addAnimation();
        API_IMPL void appendAnimation(ANIMATION *animation);
        API_IMPL bool isEndedAnimation() const noexcept;
        API_IMPL void releaseAnimation();
        API_IMPL virtual bool setTexture(const MESH_MBM *mesh,const char *fileNametexture, const uint32_t stage, const bool hasAlpha);
        API_IMPL void backupAnimations() noexcept; // called automatically by engine (CORE_MANAGER)
        API_IMPL void restoreBackupAnimations() noexcept;// called automatically by engine (CORE_MANAGER)

      protected:
        API_IMPL ARTICULATED_ANIMATION_PLAYER &getArticulatedAnimationPlayer() noexcept;
        API_IMPL const ARTICULATED_ANIMATION_PLAYER &getArticulatedAnimationPlayer() const noexcept;
        API_IMPL void resetArticulatedAnimationPlayer() noexcept;
        API_IMPL SKELETAL_ANIMATION_PLAYER &getSkeletalAnimationPlayer() noexcept;
        API_IMPL const SKELETAL_ANIMATION_PLAYER &getSkeletalAnimationPlayer() const noexcept;
        API_IMPL void resetSkeletalAnimationPlayer() noexcept;

      private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    
}

#endif
