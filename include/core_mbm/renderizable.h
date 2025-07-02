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
#include <string>
#include <map>


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
        TYPE_CLASS_LIGHT             = 8,
        TYPE_CLASS_UNKNOWN           = 9,
        TYPE_CLASS_TEMP              = 10,
        TYPE_CLASS_SHAPE_MESH        = 11,
        TYPE_CLASS_LINE_MESH         = 12,
        TYPE_CLASS_PARTICLE          = 13,
        TYPE_CLASS_STEERED_PARTICLE  = 14,
        TYPE_CLASS_RENDER_2_TEX      = 15,
        TYPE_CLASS_TILE              = 16,
        TYPE_CLASS_TILE_OBJ          = 17,
    };

    class RENDERIZABLE
    {
        friend class CORE_MANAGER;
        friend class DEVICE;
        friend class RENDER_2_TEXTURE;
        friend class ANIMATION_MANAGER;

      public:
        const int        idScene;
        const TYPE_CLASS typeClass;
        const bool       is3D;
        const bool       is2dS;
        mbm::VEC3        position;
        mbm::VEC3        scale;
        mbm::VEC3        angle;
        mbm::VEC3        bounding_AABB;
        bool             alwaysRenderize;
        bool             isObjectOnFrustum;
        bool             enableRender;
        std::map<std::string, DYNAMIC_VAR *> lsDynamicVar;
        bool             isRender2Texture;
        void *           userData;
        RENDER_STATE     blend;
        API_IMPL RENDERIZABLE(const int idSceneMe, const TYPE_CLASS newTypeClass, const bool _is3d, const bool _is2ds)noexcept;
        API_IMPL virtual ~RENDERIZABLE()noexcept;
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

      protected:
        API_IMPL virtual bool isOnFrustum()     = 0;
        API_IMPL virtual bool render()          = 0;
        API_IMPL virtual bool onRestoreDevice() = 0;
        API_IMPL virtual void onStop()          = 0;
      public:
        API_IMPL virtual void updateAABB();

      protected:
        std::string fileName;
        float       __distFromView;
    };

    class RENDERIZABLE_TO_TARGET : public RENDERIZABLE
    {
      public:
        uint32_t idFrameBuffer;
        uint32_t idDepthRenderbuffer;
        int          idTextureDynamic;
        uint32_t widthTexture;
        uint32_t heightTexture;
        COLOR        colorClearBackGround;
        API_IMPL RENDERIZABLE_TO_TARGET(const SCENE* scene, const TYPE_CLASS newTypeClass, const bool _is3d, const bool _is2ds) noexcept;
        API_IMPL virtual ~RENDERIZABLE_TO_TARGET();
        API_IMPL virtual bool render2Texture() = 0;
        API_IMPL virtual void removeFromRender2Texture(RENDERIZABLE *ptr) = 0;
    };

}

#endif
