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

#include <renderizable.h>
#include <dynamic-var.h>
#include <device.h>
#include <animation.h>
#include <physics.h>
#include <util-interface.h>
#include <algorithm>
#include <cfloat>
#include <map>
#include <utility>

namespace mbm

{
    struct RENDERIZABLE::Impl
    {
        const int        idScene;
        const TYPE_CLASS typeClass;
        const bool       is3D;
        const bool       is2dS;
        std::string fileName;
        float       distanceFromView;
        bool        isObjectOnFrustum;
        bool        isRender2Texture;
        bool        alwaysRenderize;
        bool        enableRender;
        void *      userData;
        RENDER_STATE blend;
        VEC3        position;
        VEC3        scale;
        VEC3        angle;
        VEC3        boundingAABB;
        // True offset, in world space, from `position` to the AABB's actual geometric center --
        // (0,0,0) whenever the underlying geometry happens to be centered on its own pivot (the
        // overwhelmingly common case), non-zero for anything anchored elsewhere (a building
        // pivoted at its floor, a character pivoted at its feet). `boundingAABB` alone (the size)
        // has always been silently assuming this offset is zero -- see MBM_VERSION 6.9.0.
        VEC3        boundingAABBCenterOffset;
        std::map<std::string, DYNAMIC_VAR *> lsDynamicVar;

        Impl(const int idSceneMe, const TYPE_CLASS newTypeClass, const bool _is3d, const bool _is2ds) noexcept :
            idScene(idSceneMe),
            typeClass(newTypeClass),
            is3D(_is3d),
            is2dS(_is2ds),
            distanceFromView(0.0f),
            isObjectOnFrustum(true),
            isRender2Texture(false),
            alwaysRenderize(false),
            enableRender(true),
            userData(nullptr),
            position(0, 0, 0),
            scale(1, 1, 1),
            angle(0, 0, 0),
            boundingAABB(0, 0, 0),
            boundingAABBCenterOffset(0, 0, 0)
        {
        }
    };

    RENDERIZABLE::RENDERIZABLE(const int idSceneMe, const TYPE_CLASS newTypeClass, const bool _is3d,
                               const bool _is2ds) noexcept : impl(std::make_unique<Impl>(idSceneMe, newTypeClass, _is3d, _is2ds))
    {
    }

    RENDERIZABLE::~RENDERIZABLE() noexcept
    {
        std::map<std::string, DYNAMIC_VAR *>::const_iterator it;
        for (it = this->impl->lsDynamicVar.cbegin(); it != this->impl->lsDynamicVar.cend(); ++it)
        {
            DYNAMIC_VAR *dVar = it->second;
            if (dVar)
                delete dVar;
        }
        this->impl->lsDynamicVar.clear();
    }

    struct RENDERIZABLE_TO_TARGET::BackendData
    {
        void *specificConfig;
        uint32_t widthTexture;
        uint32_t heightTexture;
        COLOR colorClearBackGround;

        BackendData() noexcept :
            specificConfig(nullptr),
            widthTexture(0),
            heightTexture(0),
            colorClearBackGround(255, 255, 255)
        {
        }
    };

    void RENDERIZABLE_TO_TARGET::BackendDataDeleter::operator()(BackendData *data) const noexcept
    {
        delete data;
    }

    RENDERIZABLE_TO_TARGET::BackendData * RENDERIZABLE_TO_TARGET::ensureRenderTargetBackendData() noexcept
    {
        if (!this->backendData)
        {
            this->backendData.reset(new BackendData());
        }
        return this->backendData.get();
    }

    const RENDERIZABLE_TO_TARGET::BackendData * RENDERIZABLE_TO_TARGET::getRenderTargetBackendData() const noexcept
    {
        return this->backendData.get();
    }

    uint32_t RENDERIZABLE_TO_TARGET::getRenderTargetWidth() const noexcept
    {
        const BackendData *data = this->getRenderTargetBackendData();
        return data ? data->widthTexture : 0;
    }

    uint32_t RENDERIZABLE_TO_TARGET::getRenderTargetHeight() const noexcept
    {
        const BackendData *data = this->getRenderTargetBackendData();
        return data ? data->heightTexture : 0;
    }

    void RENDERIZABLE_TO_TARGET::setRenderTargetSize(const uint32_t width, const uint32_t height) noexcept
    {
        BackendData *data = this->ensureRenderTargetBackendData();
        data->widthTexture = width;
        data->heightTexture = height;
    }

    COLOR & RENDERIZABLE_TO_TARGET::getRenderTargetClearColor() noexcept
    {
        return this->ensureRenderTargetBackendData()->colorClearBackGround;
    }

    const COLOR & RENDERIZABLE_TO_TARGET::getRenderTargetClearColor() const noexcept
    {
        static const COLOR defaultClearColor(255, 255, 255);
        const BackendData *data = this->getRenderTargetBackendData();
        return data ? data->colorClearBackGround : defaultClearColor;
    }

    void RENDERIZABLE_TO_TARGET::setRenderTargetClearColor(const COLOR &color) noexcept
    {
        this->ensureRenderTargetBackendData()->colorClearBackGround = color;
    }

    void * RENDERIZABLE_TO_TARGET::getRenderTargetSpecificConfig() const noexcept
    {
        const BackendData *data = this->getRenderTargetBackendData();
        return data ? data->specificConfig : nullptr;
    }

    void RENDERIZABLE_TO_TARGET::setRenderTargetSpecificConfig(void *newSpecificConfig) noexcept
    {
        this->ensureRenderTargetBackendData()->specificConfig = newSpecificConfig;
    }

    TYPE_CLASS RENDERIZABLE::getTypeClass() const noexcept
    {
        return this->impl->typeClass;
    }

    bool RENDERIZABLE::is3DObject() const noexcept
    {
        return this->impl->is3D;
    }

    bool RENDERIZABLE::is2dScreenObject() const noexcept
    {
        return this->impl->is2dS;
    }

    VEC3 & RENDERIZABLE::getPosition() noexcept
    {
        return this->impl->position;
    }

    const VEC3 & RENDERIZABLE::getPosition() const noexcept
    {
        return this->impl->position;
    }

    void RENDERIZABLE::setPosition(const VEC3 &newPosition) noexcept
    {
        this->impl->position = newPosition;
    }

    VEC3 & RENDERIZABLE::getScale() noexcept
    {
        return this->impl->scale;
    }

    const VEC3 & RENDERIZABLE::getScale() const noexcept
    {
        return this->impl->scale;
    }

    void RENDERIZABLE::setScale(const VEC3 &newScale) noexcept
    {
        this->impl->scale = newScale;
    }

    VEC3 & RENDERIZABLE::getAngle() noexcept
    {
        return this->impl->angle;
    }

    const VEC3 & RENDERIZABLE::getAngle() const noexcept
    {
        return this->impl->angle;
    }

    void RENDERIZABLE::setAngle(const VEC3 &newAngle) noexcept
    {
        this->impl->angle = newAngle;
    }

    VEC3 & RENDERIZABLE::getBoundingAABB() noexcept
    {
        return this->impl->boundingAABB;
    }

    const VEC3 & RENDERIZABLE::getBoundingAABB() const noexcept
    {
        return this->impl->boundingAABB;
    }

    void RENDERIZABLE::setBoundingAABB(const VEC3 &newBoundingAABB) noexcept
    {
        this->impl->boundingAABB = newBoundingAABB;
    }

    const VEC3 & RENDERIZABLE::getBoundingAABBCenterOffset() const noexcept
    {
        return this->impl->boundingAABBCenterOffset;
    }

    void RENDERIZABLE::setBoundingAABBCenterOffset(const VEC3 &newOffset) noexcept
    {
        this->impl->boundingAABBCenterOffset = newOffset;
    }

    float RENDERIZABLE::getDistanceFromView() const noexcept
    {
        return this->impl->distanceFromView;
    }

    void RENDERIZABLE::setDistanceFromView(const float distance) noexcept
    {
        this->impl->distanceFromView = distance;
    }

    bool RENDERIZABLE::isAlwaysRenderizeEnabled() const noexcept
    {
        return this->impl->alwaysRenderize;
    }

    void RENDERIZABLE::setAlwaysRenderize(const bool enabled) noexcept
    {
        this->impl->alwaysRenderize = enabled;
    }

    bool RENDERIZABLE::getIsObjectOnFrustum() const noexcept
    {
        return this->impl->isObjectOnFrustum;
    }

    void RENDERIZABLE::setIsObjectOnFrustum(const bool onFrustum) noexcept
    {
        this->impl->isObjectOnFrustum = onFrustum;
    }

    bool RENDERIZABLE::isRenderEnabled() const noexcept
    {
        return this->impl->enableRender;
    }

    void RENDERIZABLE::setEnableRender(const bool enabled) noexcept
    {
        this->impl->enableRender = enabled;
    }

    bool RENDERIZABLE::isRender2TextureEnabled() const noexcept
    {
        return this->impl->isRender2Texture;
    }

    void RENDERIZABLE::setRender2Texture(const bool enabled) noexcept
    {
        this->impl->isRender2Texture = enabled;
    }

    void * RENDERIZABLE::getUserData() const noexcept
    {
        return this->impl->userData;
    }

    void RENDERIZABLE::setUserData(void *data) noexcept
    {
        this->impl->userData = data;
    }

    RENDER_STATE & RENDERIZABLE::getBlend() noexcept
    {
        return this->impl->blend;
    }

    const RENDER_STATE & RENDERIZABLE::getBlend() const noexcept
    {
        return this->impl->blend;
    }

    void RENDERIZABLE::setBlendState(const BLEND_STATE blendState) noexcept
    {
        this->impl->blend.set(blendState);
    }

    DYNAMIC_VAR * RENDERIZABLE::getDynamicVar(const char *nameVar)noexcept
    {
        return this->impl->lsDynamicVar[nameVar];
    }
    void RENDERIZABLE::setDynamicVar(const char *nameVar, DYNAMIC_VAR *nDvar)noexcept
    {
        DYNAMIC_VAR *oldVar = this->impl->lsDynamicVar[nameVar];
        if (oldVar)
            delete oldVar;
        oldVar                      = nullptr;
        this->impl->lsDynamicVar[nameVar] = nDvar;
    }
    int RENDERIZABLE::getIdScene() const noexcept
    {
        return this->impl->idScene;
    }
    const char * RENDERIZABLE::getFileName() const noexcept
    {
        return this->impl->fileName.c_str();
    }

    const char * RENDERIZABLE::getInternalFileName() const noexcept
    {
        return this->impl->fileName.c_str();
    }

    const std::string & RENDERIZABLE::getInternalFileNameString() const noexcept
    {
        return this->impl->fileName;
    }

    void RENDERIZABLE::setInternalFileName(const char *newFileName)
    {
        this->impl->fileName = newFileName ? newFileName : "";
    }

    void RENDERIZABLE::setInternalFileName(const std::string &newFileName)
    {
        this->impl->fileName = newFileName;
    }

    void RENDERIZABLE::setInternalFileName(std::string &&newFileName)
    {
        this->impl->fileName = std::move(newFileName);
    }

    void RENDERIZABLE::clearInternalFileName() noexcept
    {
        this->impl->fileName.clear();
    }
    void RENDERIZABLE::getAABB(float *w, float *h) const
    {
        const VEC3 &boundingAABB = this->getBoundingAABB();
        *w = boundingAABB.x;
        *h = boundingAABB.y;
    }
    void RENDERIZABLE::getAABB(float *w, float *h, float *d) const
    {
        const VEC3 &boundingAABB = this->getBoundingAABB();
        *w = boundingAABB.x;
        *h = boundingAABB.y;
        *d = boundingAABB.z;
    }
    bool RENDERIZABLE::getWidthHeight(float *w, float *h, const bool consider_scale) const
    {
        const INFO_PHYSICS *infoPhysics = this->getInfoPhysics();
        float               x = 0, y = 0;
        if (infoPhysics && infoPhysics->getBounds(&x, &y))
        {
            if(consider_scale)
            {
                const VEC3 &scale = this->getScale();
                *w = x * scale.x, *h = y * scale.y;
            }
            else
            {
                *w = x , *h = y;
            }
            return true;
        }
        return false;
    }
    bool RENDERIZABLE::getWidthHeight(float *w, float *h, float *d, const bool consider_scale) const
    {
        const INFO_PHYSICS *infoPhysics = this->getInfoPhysics();
        float               x = 0, y = 0, z = 0;
        if (infoPhysics && infoPhysics->getBounds(&x, &y, &z))
        {
            if(consider_scale)
            {
                const VEC3 &scale = this->getScale();
                *w = x * scale.x, *h = y * scale.y;
                *d = z * scale.z;
            }
            else
            {
                *w = x, *h = y;
                *d = z;
            }
            return true;
        }
        return false;
    }
    bool RENDERIZABLE::isOver3d(DEVICE *device, const float x, const float y) const
    {
        float w, h, d;
        this->getAABB(&w, &h, &d);
        VEC3 p1, p2;
        device->transformeScreen2dToWorld3d_scaled(x, y, &p1, 100);
        device->transformeScreen2dToWorld3d_scaled(x, y, &p2, 1000);
        const VEC3 dir(p2 - p1);
        w *= 0.5f;
        h *= 0.5f;
        d *= 0.5f;
        const VEC3 &position = this->getPosition();
        // dir is unit direction vector of ray
        const VEC3 dirfrac(dir.x != 0.0f ? 1.0f / dir.x : 0.0f, dir.y != 0.0f ? 1.0f / dir.y : 0.0f,
                            dir.z != 0.0f ? 1.0f / dir.z : 0.0f);
        float t1 = ((position.x + w) - p1.x) * dirfrac.x;
        float t2 = ((position.x - w) - p1.x) * dirfrac.x;
        float t3 = ((position.y + h) - p1.y) * dirfrac.y;
        float t4 = ((position.y - h) - p1.y) * dirfrac.y;
        float t5 = ((position.z + d) - p1.z) * dirfrac.z;
        float t6 = ((position.z - d) - p1.z) * dirfrac.z;

        float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
        float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));
        // if tmax < 0, ray (line) is intersecting AABB, but whole AABB is behing us
        if (tmax < 0)
            return false;
        // if tmin > tmax, ray doesn't intersect AABB
        if (tmin > tmax)
            return false;
        return true;
    }
    bool RENDERIZABLE::isOver2dw(DEVICE *device, const float x, const float y) const
    {
        float w, h;
        this->getAABB(&w, &h);
        const VEC2 point(x, y);
        VEC2       halfDim(w * 0.5f, h * 0.5f);
        if (device->isPointScreen2DOnRectangleWorld2d(point, halfDim, this->getPosition()))
            return true;
        return false;
    }
    bool RENDERIZABLE::isOver2ds(DEVICE *device, const float x, const float y) const
    {
        float w, h;
        this->getAABB(&w, &h);
        const VEC2 point(x, y);
        VEC2       halfDim(w * 0.5f, h * 0.5f);
        if (device->isPointScreen2DOnRectangleScreen2d(point, halfDim, this->getPosition()))
            return true;
        return false;
    }

    void RENDERIZABLE::updateAABB()
    {
        if (this->isLoaded())
        {
            const INFO_PHYSICS *infoPhysics = this->getInfoPhysics();
            if (infoPhysics)
            {
                float x = 0, y = 0;
                if (this->is3DObject())
                {
                    float z = 0;
                    infoPhysics->getBounds(&x, &y, &z);
                    VEC3 p[8];
                    p[0] = VEC3(x * -0.5f, y * -0.5f, z * -0.5f);
                    p[1] = VEC3(x * 0.5f, y * -0.5f, z * -0.5f);
                    p[2] = VEC3(x * -0.5f, y * 0.5f, z * -0.5f);
                    p[3] = VEC3(x * 0.5f, y * 0.5f, z * -0.5f);

                    p[4] = VEC3(x * -0.5f, y * -0.5f, z * 0.5f);
                    p[5] = VEC3(x * 0.5f, y * -0.5f, z * 0.5f);
                    p[6] = VEC3(x * -0.5f, y * 0.5f, z * 0.5f);
                    p[7] = VEC3(x * 0.5f, y * 0.5f, z * 0.5f);

                    MATRIX matrix;
                    const VEC3 &position = this->getPosition();
                    const VEC3 &angle = this->getAngle();
                    const VEC3 &scale = this->getScale();
                    MatrixTranslationRotationScale(&matrix, &position, &angle, &scale);
                    VEC3 box_max(-FLT_MAX, -FLT_MAX, -FLT_MAX);
                    VEC3 box_min(FLT_MAX, FLT_MAX, FLT_MAX);

                    for (auto & i : p)
                    {
                        vec3TransformCoord(&i, &i, &matrix);

                        if (i.x > box_max.x)
                            box_max.x = i.x;
                        if (i.y > box_max.y)
                            box_max.y = i.y;
                        if (i.z > box_max.z)
                            box_max.z = i.z;

                        if (i.x < box_min.x)
                            box_min.x = i.x;
                        if (i.y < box_min.y)
                            box_min.y = i.y;
                        if (i.z < box_min.z)
                            box_min.z = i.z;
                    }

                    this->setBoundingAABB(VEC3(box_max.x - box_min.x, box_max.y - box_min.y, box_max.z - box_min.z));

                    // Same transform (translation/rotation/scale), but starting from the TRUE
                    // local min/max (getBoundsMinMax) instead of a box assumed symmetric around
                    // local origin -- this is what makes the offset non-zero for anything not
                    // pivoted at its own visual center (see MBM_VERSION 6.9.0).
                    VEC3 vminLocal, vmaxLocal;
                    if (infoPhysics->getBoundsMinMax(vminLocal, vmaxLocal))
                    {
                        VEC3 q[8];
                        q[0] = VEC3(vminLocal.x, vminLocal.y, vminLocal.z);
                        q[1] = VEC3(vmaxLocal.x, vminLocal.y, vminLocal.z);
                        q[2] = VEC3(vminLocal.x, vmaxLocal.y, vminLocal.z);
                        q[3] = VEC3(vmaxLocal.x, vmaxLocal.y, vminLocal.z);
                        q[4] = VEC3(vminLocal.x, vminLocal.y, vmaxLocal.z);
                        q[5] = VEC3(vmaxLocal.x, vminLocal.y, vmaxLocal.z);
                        q[6] = VEC3(vminLocal.x, vmaxLocal.y, vmaxLocal.z);
                        q[7] = VEC3(vmaxLocal.x, vmaxLocal.y, vmaxLocal.z);

                        VEC3 centerBoxMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
                        VEC3 centerBoxMin(FLT_MAX, FLT_MAX, FLT_MAX);
                        for (auto & qi : q)
                        {
                            vec3TransformCoord(&qi, &qi, &matrix);

                            if (qi.x > centerBoxMax.x)
                                centerBoxMax.x = qi.x;
                            if (qi.y > centerBoxMax.y)
                                centerBoxMax.y = qi.y;
                            if (qi.z > centerBoxMax.z)
                                centerBoxMax.z = qi.z;

                            if (qi.x < centerBoxMin.x)
                                centerBoxMin.x = qi.x;
                            if (qi.y < centerBoxMin.y)
                                centerBoxMin.y = qi.y;
                            if (qi.z < centerBoxMin.z)
                                centerBoxMin.z = qi.z;
                        }
                        const VEC3 trueWorldCenter((centerBoxMax.x + centerBoxMin.x) * 0.5f,
                                                    (centerBoxMax.y + centerBoxMin.y) * 0.5f,
                                                    (centerBoxMax.z + centerBoxMin.z) * 0.5f);
                        this->setBoundingAABBCenterOffset(trueWorldCenter - position);
                    }
                    else
                    {
                        this->setBoundingAABBCenterOffset(VEC3(0, 0, 0));
                    }
                }
                else
                {
                    infoPhysics->getBounds(&x, &y);
                    const VEC3 &scale = this->getScale();
                    const VEC3 &angle = this->getAngle();
                    VEC2 halfDim(x * 0.5f * scale.x, y * 0.5f * scale.y);
                    VEC3 boundingAABB = this->getBoundingAABB();
                    util::getAABB(halfDim, angle.z, &boundingAABB.x, &boundingAABB.y);
                    this->setBoundingAABB(boundingAABB);

                    // 2D only rotates around Z -- rotate/scale the true local center directly
                    // instead of reusing util::getAABB (which assumes a symmetric box, see above).
                    VEC2 vminLocal2d, vmaxLocal2d;
                    if (infoPhysics->getBoundsMinMax(vminLocal2d, vmaxLocal2d))
                    {
                        const float localCenterX = (vminLocal2d.x + vmaxLocal2d.x) * 0.5f;
                        const float localCenterY = (vminLocal2d.y + vmaxLocal2d.y) * 0.5f;
                        const float scaledX = localCenterX * scale.x;
                        const float scaledY = localCenterY * scale.y;
                        const float sin_o = sinf(angle.z);
                        const float cos_o = cosf(angle.z);
                        const float worldOffsetX = scaledX * cos_o - scaledY * sin_o;
                        const float worldOffsetY = scaledX * sin_o + scaledY * cos_o;
                        this->setBoundingAABBCenterOffset(VEC3(worldOffsetX, worldOffsetY, 0.0f));
                    }
                    else
                    {
                        this->setBoundingAABBCenterOffset(VEC3(0, 0, 0));
                    }
                }
            }
        }
    }

    bool RENDERIZABLE::clone(RENDERIZABLE* renderizable_clone) const
    {
        if(renderizable_clone && this->isLoaded())
        {
            renderizable_clone->setInternalFileName(this->getInternalFileNameString());
            if(renderizable_clone->onRestoreDevice())
            {
                renderizable_clone->setPosition(this->getPosition());
                renderizable_clone->setScale(this->getScale());
                renderizable_clone->setAngle(this->getAngle());
                return true;
            }
        }
        return false;
    }

    const char * RENDERIZABLE::getTypeClassName() const noexcept
    {
        switch (this->getTypeClass())
        {
            case TYPE_CLASS_MESH                : return "mesh";
            case TYPE_CLASS_SPRITE              : return "sprite";
            case TYPE_CLASS_TEXTURE             : return "texture";
            case TYPE_CLASS_BACKGROUND          : return "backGround";
            case TYPE_CLASS_GIF                 : return "gif";
            case TYPE_CLASS_TEXT                : return "font";
            case TYPE_CLASS_PRIMITIVE           : return "primitive";
            case TYPE_CLASS_TEMP                : return "temp";
            case TYPE_CLASS_SHAPE_MESH          : return "shape-mesh";
            case TYPE_CLASS_LINE_MESH           : return "line-mesh";
            case TYPE_CLASS_PARTICLE            : return "particle";
            case TYPE_CLASS_STEERED_PARTICLE    : return "steered-particle";
            case TYPE_CLASS_RENDER_2_TEX        : return "render-to-texture";
            case TYPE_CLASS_TILE                : return "tile";
            case TYPE_CLASS_TILE_OBJ            : return "tile-obj";
            case TYPE_CLASS_TILE_LAYER          : return "tile-layer";
            default                             : return "unknown";
        }
    }

    void RENDERIZABLE::onStop()
    {
        ANIMATION_MANAGER* AnimationManager = this->getAnimationManager();
        if(AnimationManager)
        {
            AnimationManager->backupAnimations();
        }
    }

    void RENDERIZABLE::onRestoreAnimationsState()
    {
        ANIMATION_MANAGER* AnimationManager = this->getAnimationManager();
        if (AnimationManager)
        {
            AnimationManager->restoreBackupAnimations();
        }
    }
}
