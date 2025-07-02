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

#ifndef TEXTURE_VIEW_MBM_GLES_H
#define TEXTURE_VIEW_MBM_GLES_H

#include <core_mbm/core-exports.h>
#include <core_mbm/device.h>
#include <core_mbm/shader-fx.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/animation.h>
#include <core_mbm/physics.h>

namespace mbm
{
    struct IMAGE_RESOURCE;

    class TEXTURE_VIEW : public RENDERIZABLE, public COMMON_DEVICE, public ANIMATION_MANAGER
    {
      public:
        API_IMPL TEXTURE_VIEW(const SCENE *scene, const bool _is3d, const bool _is2dScreen);
        API_IMPL TEXTURE_VIEW(const bool _is3d, const bool _is2dScreen);//no scene - just restore texture
        API_IMPL virtual ~TEXTURE_VIEW();
        API_IMPL void release();
        API_IMPL bool createAnimationAndShader2Texture();
        API_IMPL bool load(const IMAGE_RESOURCE *image);
        API_IMPL bool load(const char *fileNameTexture, const float w = 0.0f, const float h = 0.0f, const bool alpha = true);
        API_IMPL bool setFrame(const float diameter);
        API_IMPL bool setFrame(const float width, const float height);
        API_IMPL BUFFER_GL *getFrame();
        API_IMPL TEXTURE *getTexture() const;
        API_IMPL virtual bool setTexture(
            const MESH_MBM *mesh, // fixa textura para o estagio 0 e 1, mesh == nullptr e stage = 1 para textura de estagio 2
            const char *fileNametexture, const uint32_t stage, const bool hasAlpha) override;
        API_IMPL void setTextureToNull();
		std::string getFileNameTexture()const;
		API_IMPL FX*  getFx() const override;
		API_IMPL ANIMATION_MANAGER*  getAnimationManager() override;
		
      private:
        bool isOnFrustum() override;
        bool render() override;
        void onStop() override;
        bool onRestoreDevice() override;
        void fillvertexQuadTexture(VEC3 *_position, VEC3 *normal, VEC2 *uv, const float width,
                                          const float height);
        void updateRestoreTexture(const float w, const float h);
        const mbm::INFO_PHYSICS *getInfoPhysics() const override;
        const MESH_MBM *getMesh() const override;
        bool isLoaded() const override;
    
        INFO_PHYSICS infoPhysics;
        TEXTURE *    texture;
        BUFFER_GL    bufferGL;
    };
}

#endif
