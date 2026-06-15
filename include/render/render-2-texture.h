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

#ifndef RENDER_2_TEXTURE__GLES__H
#define RENDER_2_TEXTURE__GLES__H

#include <core_mbm/core-exports.h>
#include <core_mbm/device.h>
#include <core_mbm/shader-fx.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/animation.h>
#include <core_mbm/physics.h>
#include <memory>

namespace mbm
{

    class CAMERA_TARGET
    {
      public:
        VEC3        position;
        VEC3        scale;
        VEC3        angle;
        VEC3        focus, up;
        float       zNear;
        float       zFar;
        mbm::MATRIX matrixOrtho;
        mbm::MATRIX matrixProj, matrixView;
        API_IMPL CAMERA_TARGET()noexcept;
        API_IMPL void enableMode2D(mbm::DEVICE *device, const float width, const float height);
        API_IMPL void enableMode3D(mbm::DEVICE *device, const float width, const float height);
    
    };
    //is not right --- must be investigate
    class RENDER_2_TEXTURE : public RENDERIZABLE_TO_TARGET, public ANIMATION_MANAGER
    {
      public:
        API_IMPL RENDER_2_TEXTURE(const SCENE* scene, const bool _is3d, const bool _is2dScreen);
        API_IMPL virtual ~RENDER_2_TEXTURE();
        API_IMPL void removeFromRender2Texture(RENDERIZABLE *ptr) override;
        API_IMPL virtual void release();
        API_IMPL TEXTURE* load(const uint32_t widthFrame, const uint32_t heightFrame, const uint32_t _widthTexture,const uint32_t _heightTexture, const char *nickName, const bool hasAlpha);
        API_IMPL void flip_vertically(uint8_t *pixels, const int width, const int height, const int bytes_per_pixel);
        API_IMPL bool saveAsPNG(const char* newFileOutNamePNG,const int x,const int y,const int _width,const int _height);//specific backend engine
        API_IMPL bool addObject2Render(RENDERIZABLE *ptr);
        API_IMPL bool removeObject2Render(RENDERIZABLE *ptr);
        API_IMPL void clear();
        API_IMPL FX*  getFx() const override;
        API_IMPL ANIMATION_MANAGER*  getAnimationManager() override;
        API_IMPL bool isTextureOnlyModeEnabled() const noexcept;
        API_IMPL void setTextureOnlyMode(const bool mode) noexcept;
        API_IMPL CAMERA_TARGET & getCamera2d() noexcept;
        API_IMPL const CAMERA_TARGET & getCamera2d() const noexcept;
        API_IMPL CAMERA_TARGET & getCamera3d() noexcept;
        API_IMPL const CAMERA_TARGET & getCamera3d() const noexcept;
    
      protected:
        virtual bool render() override;
        bool render2Texture() override;
        virtual bool isOnFrustum() override;
        bool onRestoreDevice() override;
        void fillvertexQuad(VEC3 *_position, VEC3 *normal, VEC2 *uv, const float width, const float height);
        FVF_PROVIDE_BY_ENGINE getFvfFromBuffer() const noexcept override;
        bool createAnimationAndShader2Render2Texture();
        const mbm::INFO_PHYSICS *getInfoPhysics() const override;
        const MESH_MBM *getMesh() const override;
        bool isLoaded() const override;
        void clearRenderObjectLists() noexcept;
        mbm::TEXTURE * getRenderTargetTexture() const noexcept;
        void setRenderTargetTexture(mbm::TEXTURE *renderTargetTexture) noexcept;
        
        BUFFER_GL         bufferGL;
        mbm::INFO_PHYSICS infoPhysics;

      private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
};

#endif
