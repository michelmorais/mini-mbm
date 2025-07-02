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

#ifndef PARTICLE_GLES_H
#define PARTICLE_GLES_H

#include <core_mbm/core-exports.h>
#include <core_mbm/device.h>
#include <core_mbm/shader-fx.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/animation.h>
#include <core_mbm/physics.h>

namespace util
{
    struct STAGE_PARTICLE;
}

namespace mbm
{
    struct ATT_PARTICLE;
    struct VERTEX_PARTICLE;
    
    class PARTICLE : public RENDERIZABLE, public COMMON_DEVICE, public ANIMATION_MANAGER
    {
      public:
        API_IMPL PARTICLE(const SCENE *scene, const bool _is3d, const bool _is2dScreen);
        API_IMPL virtual ~PARTICLE();
        API_IMPL void release();
        API_IMPL bool load(const char *fileNameTextureOrMesh, const char *operatorShader, const char *newCodeLine,
                  const uint32_t sizeOfParticle = 0, const bool initializeParticleData = true);
        API_IMPL bool addParticle(const uint32_t numParticles,const bool forceNow);
        API_IMPL uint32_t getTotalParticleAlive() const;
        API_IMPL uint32_t getTotalParticle() const;
        API_IMPL uint32_t getTotalParticleByStage(const uint32_t index) const;
        API_IMPL void setTotalParticleByStage(const uint32_t index,const uint32_t numParticles);
        API_IMPL util::STAGE_PARTICLE* getStageParticle(const uint32_t index);
        API_IMPL util::STAGE_PARTICLE* getStageParticle();
        API_IMPL uint32_t addStage();
        API_IMPL uint32_t getIndexStageParticle() const;
        API_IMPL void setIndexStageParticle(const uint32_t index);
        API_IMPL uint32_t getTotalStage() const;
        API_IMPL void restartAnimationParticle();
        API_IMPL const char* getTextureFileName()const;
        API_IMPL FX*  getFx() const override;
        API_IMPL ANIMATION_MANAGER*  getAnimationManager() override;
        API_IMPL bool setTexture(const MESH_MBM *mesh,const char *fileNametexture, const uint32_t stage, const bool hasAlpha) override;
    
      private:
        bool _addParticle(const uint32_t numParticles);
        void onResuscitate(const util::STAGE_PARTICLE* sPart, const uint32_t total_To_Resuscitate);
        void restartParticle(const util::STAGE_PARTICLE* sPart, mbm::ATT_PARTICLE *particle, VERTEX_PARTICLE pPartBuffer[4], const VEC2 *dist);
        bool isOnFrustum() override;
        bool render() override;
        void updateAnimationParticle();
        void onStop() override;
        bool releaseOnFail();
        bool onRestoreDevice() override;
        bool renderParticle(const util::STAGE_PARTICLE * sPart);
        bool loadParticleShader(const char *operatorShader, const char *newCodeLine);
        bool createAnimationAndShader2Particle(const char *operatorShader, const char *newCodeLine);
        const mbm::INFO_PHYSICS *getInfoPhysics() const override;
        const MESH_MBM *getMesh() const override;
        bool isLoaded() const override;
        uint32_t vboIndexBuffer;
    
        uint32_t     lenArrayParticlesData;
        uint32_t     totalAlive;
        uint32_t     indexStage;
        float           currentTimeArise;
        std::vector<util::STAGE_PARTICLE*> lsParticleStage;
        mbm::TEXTURE *   texture;
        float            wTexture, hTexture;
        VEC2             minv;
        VEC2             maxv;
        ATT_PARTICLE *   particles;
        VERTEX_PARTICLE *buffer;
        std::string      _newCodeLine;          // onRestore
        char             _operatorShader;       // onRestore
    };
}

#endif
