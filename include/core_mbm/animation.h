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
}

namespace mbm
{
    class TEXTURE;
    class MESH_MBM;
    
    typedef void (*OnEndAnimation)(const char *nameAnimation, RENDERIZABLE *renderizable);
    typedef void(*OnEndEffect)(const char *shaderName,RENDERIZABLE *renderizable);
    typedef void (*OnEndAnimationCallBack)(const char *fileNameAnimation, void *renderizable);

    class EFFECT_SHADER
    {
    public:
        STATUS_FX			statusFx; 
        TYPE_ANIMATION      typeAnim;
        BASE_SHADER *       ptrCurrentShader;
        float               timeAnimation;

        API_IMPL EFFECT_SHADER() noexcept;
        API_IMPL virtual ~EFFECT_SHADER();

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
        char           nameAnimation[32];
        float          intervalChangeFrame;  
        int            indexInitialFrame;    
        int            indexFinalFrame;      
        int            indexCurrentFrame;    
        BLEND_STATE    blendState;
        bool           isEndedThisAnimation; 
        bool           currentWayGrowingOfAnimation;
        TYPE_ANIMATION type; // 0:Pause animation 1:Increasing (e.g. 1 to 5, stops at 5)
                             // 2:Increasing with loop (e.g. 1 to 5, loops) 3:Decreasing (e.g. 5 to 1, stops at 1)
                             // 4:Decreasing with loop 5:Recursive (increasing then decreasing)
                             // 6:Recursive with loop
        FX					fx;//the effect shader to this animations
    
        API_IMPL ANIMATION();
        inline virtual ~ ANIMATION() = default;
        API_IMPL void restartAnimation();
        API_IMPL void updateAnimation(const float delta, RENDERIZABLE *me,
                                        OnEndAnimation onEndAnimation,
                                        OnEndEffect onEndFX);
      private:
        float currentTimeToChangeAnimation; 
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

    class ANIMATION_MANAGER
    {
      public:
        API_IMPL ANIMATION_MANAGER() noexcept;
        API_IMPL virtual ~ANIMATION_MANAGER();
        API_IMPL void populateTextureStage2FromMesh(MESH_MBM *mesh);
        API_IMPL bool populateAnimationFromHeader(MESH_MBM *mesh, util::HEADER_ANIMATION *header, const uint32_t index);
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
    
        uint32_t                    indexCurrentAnimation;
        OnEndAnimation              onEndAnimation;
        OnEndEffect                 onEndFx;
      private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    
}

#endif
