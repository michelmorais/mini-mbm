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

#ifndef RENDERIZABLE_CLASS_H
#define RENDERIZABLE_CLASS_H

#include "core-exports.h"
#include "primitives.h"
#include "blend.h"
#include "shader.h"
#include <memory>
#include <string>


namespace mbm
{
  class FX;
  class MESH_MBM;
  class ANIMATION_MANAGER;
  class DEVICE;
  class CORE_MANAGER;
  class SCENE;
  struct INFO_PHYSICS;
  struct DYNAMIC_VAR;

    enum TYPE_CLASS : char
    {
        TYPE_CLASS_MESH              = 1,
        TYPE_CLASS_SPRITE            = 2,
        TYPE_CLASS_TEXTURE           = 3,
        TYPE_CLASS_BACKGROUND        = 4,
        TYPE_CLASS_GIF               = 5,
        TYPE_CLASS_TEXT              = 6,
        TYPE_CLASS_PRIMITIVE         = 7,
        TYPE_CLASS_UNKNOWN           = 9,
        TYPE_CLASS_TEMP              = 10,
        TYPE_CLASS_SHAPE_MESH        = 11,
        TYPE_CLASS_LINE_MESH         = 12,
        TYPE_CLASS_PARTICLE          = 13,
        TYPE_CLASS_STEERED_PARTICLE  = 14,
        TYPE_CLASS_RENDER_2_TEX      = 15,
        TYPE_CLASS_TILE              = 16,
        TYPE_CLASS_TILE_OBJ          = 17,
        TYPE_CLASS_TILE_LAYER        = 18,
    };

    class RENDERIZABLE
    {
        friend class CORE_MANAGER;
        friend class DEVICE;
        friend class RENDER_2_TEXTURE;
        friend class ANIMATION_MANAGER;

      public:
        API_IMPL RENDERIZABLE(const int idSceneMe, const TYPE_CLASS newTypeClass, const bool _is3d, const bool _is2ds)noexcept;
        API_IMPL virtual ~RENDERIZABLE()noexcept;
        API_IMPL TYPE_CLASS getTypeClass() const noexcept;
        API_IMPL bool is3DObject() const noexcept;
        API_IMPL bool is2dScreenObject() const noexcept;
        API_IMPL VEC3 & getPosition() noexcept;
        API_IMPL const VEC3 & getPosition() const noexcept;
        API_IMPL void setPosition(const VEC3 &newPosition) noexcept;
        API_IMPL VEC3 & getScale() noexcept;
        API_IMPL const VEC3 & getScale() const noexcept;
        API_IMPL void setScale(const VEC3 &newScale) noexcept;
        API_IMPL VEC3 & getAngle() noexcept;
        API_IMPL const VEC3 & getAngle() const noexcept;
        API_IMPL void setAngle(const VEC3 &newAngle) noexcept;
        API_IMPL VEC3 & getBoundingAABB() noexcept;
        API_IMPL const VEC3 & getBoundingAABB() const noexcept;
        API_IMPL void setBoundingAABB(const VEC3 &newBoundingAABB) noexcept;
        API_IMPL float getDistanceFromView() const noexcept;
        API_IMPL void setDistanceFromView(const float distance) noexcept;
        API_IMPL bool isAlwaysRenderizeEnabled() const noexcept;
        API_IMPL void setAlwaysRenderize(const bool enabled) noexcept;
        API_IMPL bool getIsObjectOnFrustum() const noexcept;
        API_IMPL void setIsObjectOnFrustum(const bool onFrustum) noexcept;
        API_IMPL bool isRenderEnabled() const noexcept;
        API_IMPL void setEnableRender(const bool enabled) noexcept;
        API_IMPL bool isRender2TextureEnabled() const noexcept;
        API_IMPL void setRender2Texture(const bool enabled) noexcept;
        API_IMPL void * getUserData() const noexcept;
        API_IMPL void setUserData(void *data) noexcept;
        API_IMPL RENDER_STATE & getBlend() noexcept;
        API_IMPL const RENDER_STATE & getBlend() const noexcept;
        API_IMPL void setBlendState(const BLEND_STATE blendState) noexcept;
        API_IMPL DYNAMIC_VAR *getDynamicVar(const char *nameVar)noexcept;
        API_IMPL void setDynamicVar(const char *nameVar, DYNAMIC_VAR *nDvar)noexcept;
        API_IMPL int getIdScene() const noexcept;
        API_IMPL const char *getFileName() const noexcept;
        API_IMPL virtual void getAABB(float *w, float *h) const;
        API_IMPL virtual void getAABB(float *w, float *h, float *d) const;
        API_IMPL virtual bool getWidthHeight(float *w, float *h, const bool consider_scale = true) const;
        API_IMPL virtual bool getWidthHeight(float *w, float *h, float *d, const bool consider_scale = true) const;
        API_IMPL virtual bool isOver3d(DEVICE *device, const float x, const float y) const;
        API_IMPL virtual bool isOver2dw(DEVICE *device, const float x, const float y) const;
        API_IMPL virtual bool isOver2ds(DEVICE *device, const float x, const float y) const;
        API_IMPL bool clone(RENDERIZABLE* renderizable_clone) const;
        API_IMPL const char * getTypeClassName() const noexcept;

      public:
        API_IMPL virtual const INFO_PHYSICS *getInfoPhysics() const = 0;
        API_IMPL virtual const MESH_MBM *    getMesh() const        = 0;
        API_IMPL virtual bool                isLoaded() const       = 0;
        API_IMPL virtual FX*                 getFx() const          = 0;
        API_IMPL virtual ANIMATION_MANAGER*  getAnimationManager()  = 0;
        /** FVF from BUFFER_GL (single source of truth). Must be implemented by every RENDERIZABLE subclass. */
        API_IMPL virtual FVF_PROVIDE_BY_ENGINE getFvfFromBuffer() const noexcept = 0;

      protected:
        API_IMPL virtual bool isOnFrustum()     = 0;
        API_IMPL virtual bool render()          = 0;
        API_IMPL virtual bool onRestoreDevice() = 0;// In this function, make sure that the object is loaded, later the engine will fill in the animation state with onRestoreAnimationsState
        API_IMPL virtual void onStop() final;
        API_IMPL virtual void onRestoreAnimationsState() final;
        API_IMPL const char *getInternalFileName() const noexcept;
        API_IMPL const std::string &getInternalFileNameString() const noexcept;
        API_IMPL void setInternalFileName(const char *newFileName);
        API_IMPL void setInternalFileName(const std::string &newFileName);
        API_IMPL void setInternalFileName(std::string &&newFileName);
        API_IMPL void clearInternalFileName() noexcept;
      public:
        API_IMPL virtual void updateAABB();

      protected:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };

    class RENDERIZABLE_TO_TARGET : public RENDERIZABLE
    {
      public:
        FVF_PROVIDE_BY_ENGINE getFvfFromBuffer() const noexcept override;
        API_IMPL RENDERIZABLE_TO_TARGET(const SCENE* scene, const TYPE_CLASS newTypeClass, const bool _is3d, const bool _is2ds) noexcept;
        API_IMPL virtual ~RENDERIZABLE_TO_TARGET();
        API_IMPL virtual bool render2Texture() = 0;
        API_IMPL virtual void removeFromRender2Texture(RENDERIZABLE *ptr) = 0;
        API_IMPL uint32_t getRenderTargetWidth() const noexcept;
        API_IMPL uint32_t getRenderTargetHeight() const noexcept;
        API_IMPL void setRenderTargetSize(uint32_t width, uint32_t height) noexcept;
        API_IMPL COLOR & getRenderTargetClearColor() noexcept;
        API_IMPL const COLOR & getRenderTargetClearColor() const noexcept;
        API_IMPL void setRenderTargetClearColor(const COLOR &color) noexcept;
        API_IMPL void * getRenderTargetSpecificConfig() const noexcept;
        API_IMPL void setRenderTargetSpecificConfig(void *specificConfig) noexcept;
      private:
        struct BackendData;
        struct BackendDataDeleter
        {
            void operator()(BackendData *data) const noexcept;
        };

        BackendData * ensureRenderTargetBackendData() noexcept;
        const BackendData * getRenderTargetBackendData() const noexcept;
        std::unique_ptr<BackendData, BackendDataDeleter> backendData;
    };

}

#endif
