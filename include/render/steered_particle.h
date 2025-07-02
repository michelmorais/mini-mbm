/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2022      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef FLUID_GLES_H
#define FLUID_GLES_H

#include <core_mbm/core-exports.h>
#include <core_mbm/device.h>
#include <core_mbm/shader-fx.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/animation.h>
#include <core_mbm/physics.h>

namespace mbm
{
 
    struct FLUID_GROUP
    {
        uint32_t       size_particle_array;
        uint32_t       totalParticleToRender;
        float          aSizeParticle;//
        float          radiusScale;
        VEC3 *         particle_positions;
        VEC3 *         vertex_particle;
        VEC2 *         uv;
        bool           segmented;

        API_IMPL FLUID_GROUP(const bool b_segmented,const float _radiusScale);
        API_IMPL ~FLUID_GROUP();
        API_IMPL void resizeParticleData(const uint32_t new_size);
        API_IMPL void setVertex(const VEC3 * const position, VEC3 pVertex[4]);
        API_IMPL void setUv(VEC2 pUv[4], const VEC2 & pos,const VEC2 & halParticleSizeInUv);
    };
    
    class STEERED_PARTICLE : public RENDERIZABLE, public COMMON_DEVICE, public ANIMATION_MANAGER
    {
      public:
        API_IMPL STEERED_PARTICLE(const SCENE *scene, const bool _is3d, const bool _is2dScreen,const bool b_segmented,const float* _scale_physics_engine);
        API_IMPL virtual ~STEERED_PARTICLE();
        API_IMPL void release();
        API_IMPL bool load(const char *fileNameTexture,const COLOR *p_color,const mbm::INFO_PHYSICS* const p_infoPhysics);
        API_IMPL bool addParticle(const uint32_t numParticle,const uint32_t index_group);
        API_IMPL uint32_t getTotalParticleToRender() const;
        API_IMPL uint32_t getTotalParticleByGroup(const uint32_t index) const;
        API_IMPL void setTotalParticleByGroup(const uint32_t index,const uint32_t numParticle);
        API_IMPL FLUID_GROUP* getParticleGroup(const uint32_t index);
        API_IMPL uint32_t addGroup();
        API_IMPL void removeGroup(const uint32_t index);
        API_IMPL uint32_t getTotalGroup() const;
        API_IMPL void restartAnimationParticle();
        API_IMPL const char* getTextureFileName()const;
        API_IMPL FX*  getFx() const override;
        API_IMPL ANIMATION_MANAGER*  getAnimationManager() override;
        API_IMPL bool setTexture(const MESH_MBM *mesh,const char *fileNametexture, const uint32_t stage, const bool hasAlpha) override;
        API_IMPL bool clonePhysics(const mbm::INFO_PHYSICS * const new_info_physics);
        API_IMPL const mbm::INFO_PHYSICS *getInfoPhysics() const override;
        API_IMPL inline const bool getSegmented() const {return this->segmented;};
        API_IMPL void getSizeTexture(uint32_t &width,uint32_t &height) const;
        API_IMPL inline const float getRadiusScale() const {return this->radiusScale;};
        API_IMPL void setRadiusScale(const float _radiusScale) { radiusScale = _radiusScale;};
        API_IMPL const float getScalePhysicsEngine() const { return *this->scale_physics_engine; };
        API_IMPL const COLOR getColor() const { return this->shader_color; };
        API_IMPL void setColor(const COLOR &color ) { shader_color = color; };
        
      private:
        bool isOnFrustum() override;
        bool render() override;
        void onStop() override;
        bool releaseOnFail();
        bool onRestoreDevice() override;
        bool renderParticle(FLUID_GROUP * pGroup);
        bool loadParticleShader(const COLOR *p_color);
        bool createAnimationAndShader2Particle(const COLOR *p_color);
        const MESH_MBM *getMesh() const override;
        const float* scale_physics_engine;
        mbm::INFO_PHYSICS infoPhysics;
        bool isLoaded() const override;
        uint32_t vboIndexBuffer;
    
        std::vector<FLUID_GROUP*> lsParticleGroup;
        mbm::TEXTURE *   texture;
        COLOR            shader_color;
        const bool       segmented;
        float            radiusScale;
    };
}

#endif
