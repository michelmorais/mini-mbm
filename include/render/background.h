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

#ifndef BACKGROUND_GLES_H
#define BACKGROUND_GLES_H

#include <core_mbm/core-exports.h>
#include <core_mbm/device.h>
#include <core_mbm/shader-fx.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/animation.h>
#include <core_mbm/physics.h>

namespace util
{
    enum TYPE_MESH : char;
}

namespace mbm
{

    class BACKGROUND : public RENDERIZABLE, public COMMON_DEVICE, public ANIMATION_MANAGER
    {
      public:
    
        CUBE  bound;
        float howFar3d;
    
        std::string text;
        float       spaceXCharacter;
        float       spaceYCharacter;
        bool        isMajorScale;
    
        API_IMPL BACKGROUND(const SCENE *scene, const bool isBackGround3d);
        API_IMPL virtual ~BACKGROUND();
        API_IMPL void release();
        API_IMPL const char *getFileName();
        API_IMPL bool loadMesh(const char *fileNameMeshMbm);
        API_IMPL bool loadFont(const char *fileNameMeshMbm, const char *newText);
        API_IMPL util::TYPE_MESH getType() const;
        API_IMPL TEXTURE *getTexture() const;
        API_IMPL bool loadTexture(const char *fileNameMeshMbm, const bool hasAlpha);
        API_IMPL void setFrontGround(const bool enable);
        API_IMPL bool load(const char *fileName, const bool hasAlpha = false, const bool majorScale = true);
        API_IMPL bool setTexture(const MESH_MBM *_mesh, // fixa textura para o estagio 0 e 1, stage = 1 para textura de estagio 2
        const char *fileNametexture, const uint32_t stage, const bool hasAlpha) override;
		API_IMPL FX*  getFx() const override;
		API_IMPL ANIMATION_MANAGER*  getAnimationManager() override;

      private:
        float getValueFromMinMaxValue(const float min, const float max, const float value_0_100_percent);
        bool isOnFrustum() override;
        bool render() override;
        bool onRestoreDevice() override;
        bool setScale(const bool majorScale);
        void fillvertexQuadTexture(VEC3 *_position, VEC3 *normal, VEC2 *uv, const float width,const float height);
        void onStop() override;
        const mbm::INFO_PHYSICS *getInfoPhysics() const override;
        const MESH_MBM *getMesh() const override;
        bool isLoaded() const override;
    
        MESH_MBM *          mesh;
        util::TYPE_MESH type;
        TEXTURE *           texture;
        BUFFER_GL *         buffer; 
        uint32_t        lasIndexAnimation;
        bool                isFrontGround;
    };
};

#endif
