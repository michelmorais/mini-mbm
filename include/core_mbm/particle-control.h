/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef PARTICLE_CONTROL_H
#define PARTICLE_CONTROL_H

#include "primitives.h"
#include <vector>

#if (defined(__MINGW32__) || defined(__CYGWIN__) || defined(_WIN32))
#pragma warning(push)
#pragma warning(disable : 4201) // nonstandard extension used : nameless struct/union
#endif

namespace util
{
    struct STAGE_PARTICLE;
}

namespace mbm
{
    
    class ANIMATION;

struct ATT_PARTICLE
{
    VEC3  direction;
    float speed;
    float timeLife;
    float timeLifeCurrent;
    float aSize;
    union {
        struct
        {
            float a, r, g, b;
        };
        struct
        {
            float color[4];
        };
    };
};


typedef void (*OnEndAnimationParticleControl)(void* owner, const char* nameAnimation);

struct PARTICLE_CONTROL
{
    PARTICLE_CONTROL() noexcept;
    ~PARTICLE_CONTROL();
    PARTICLE_CONTROL(const PARTICLE_CONTROL&) = delete;
    PARTICLE_CONTROL& operator=(const PARTICLE_CONTROL&) = delete;
    void release();
    void initializeBuffer(const uint32_t totalParticleToLoad, const float width, const float height);
    inline uint32_t getTotalParticle() const noexcept { return lenArrayParticlesData; }
    inline uint32_t getTotalParticleAlive() const noexcept { return totalAlive; }
    void updateParticleStage(const util::STAGE_PARTICLE* sPart, const float delta);
    bool addParticle(const unsigned int numParticles, const bool forceNow);
    uint32_t getTotalParticleByStage(const uint32_t index) const;
    void setTotalParticleByStage(const uint32_t index, const uint32_t numParticles);
    util::STAGE_PARTICLE* getStageParticle(const unsigned int index) const;
    util::STAGE_PARTICLE* getStageParticle() const;
    uint32_t addStage();
    uint32_t getIndexStageParticle() const;
    void setIndexStageParticle(uint32_t index);
    uint32_t getTotalStage() const;
    void restartAnimationParticle();
    uint32_t addStageFromOther(const util::STAGE_PARTICLE* other);
    void onResuscitate(const util::STAGE_PARTICLE* sPart, const unsigned int total_To_Resuscitate);
    void setTotalAlive(uint32_t total);
    inline VEC2 getMinv() const noexcept { return minv; }
    inline VEC2 getMaxv() const noexcept { return maxv; }
    inline VEC2 getDim() const noexcept { return VEC2(maxv - minv); }
    inline float getWTexture() const noexcept { return wTexture; }
    inline float getHTexture() const noexcept { return hTexture; }
    inline uint32_t getTotalAlive() const noexcept { return totalAlive; }
    inline void setOnEndAnimationParticleControl(OnEndAnimationParticleControl _onEndAnimationParticleControl) noexcept { onEndAnimationParticleControl = _onEndAnimationParticleControl; }
    void updateAnimationParticle(void* owner, ANIMATION* anim, const float delta);
    inline const ATT_PARTICLE* getAttParticle() const noexcept { return particles;}
    inline const VERTEX_UV* getVertexBuffer() const noexcept { return buffer; }
    void moveFrom(PARTICLE_CONTROL& other);//backup copy move
protected:
    void restartParticle(const util::STAGE_PARTICLE* sPart, ATT_PARTICLE* particle, VERTEX_UV pPartBuffer[4], const VEC2* dist);
    bool _addParticle(const unsigned int numParticles);
private:

    uint32_t         lenArrayParticlesData;
    uint32_t         totalAlive;
    uint32_t         indexStage;
    float            currentTimeArise;
    float            wTexture;
    float            hTexture;
    VEC2             minv;
    VEC2             maxv;
    ATT_PARTICLE*    particles;
    VERTEX_UV*       buffer;
    std::vector<util::STAGE_PARTICLE*> lsParticleStage;
    OnEndAnimationParticleControl onEndAnimationParticleControl;
};

struct FLUID_GROUP
{
    uint32_t       size_particle_array;
    uint32_t       totalParticleToRender;
    float          aSizeParticle;//
    float          radiusScale;// The actual size of particle is: aSizeParticle * radiusScale (let 1.0f to not influence
    VEC3* particle_positions;
    VEC3* vertex_particle;
    VEC2* uv;
    bool           segmented;//when true: we have uv calculation based on the aabb of the group of particles, when false: uv is set to (0,0)(1,0)(0,1)(1,1) for each particle (so the size is constant)
    COLOR*         color;

    API_IMPL FLUID_GROUP(const bool b_segmented, const float _radiusScale, const COLOR *theColor) noexcept;
    API_IMPL ~FLUID_GROUP();
    FLUID_GROUP (const FLUID_GROUP&) = delete;
    FLUID_GROUP & operator=(const FLUID_GROUP&) = delete;

    API_IMPL void resizeParticleData(const uint32_t new_size);
    API_IMPL void setVertex(const VEC3* const position, VEC3 pVertex[4]) noexcept;
    API_IMPL void setUv(VEC2 pUv[4], const VEC2& pos, const VEC2& halParticleSizeInUv) noexcept;
};

}

#if (defined(__MINGW32__) || defined(__CYGWIN__) || defined(_WIN32))
#pragma warning(pop) // nonstandard extension used : nameless struct/union
#endif

#endif