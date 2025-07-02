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
#include <physics.h>
#include <util-interface.h>
#include <gles-debug.h>
#include <algorithm>
#include <cfloat>

namespace mbm

{
RENDERIZABLE::RENDERIZABLE(const int idSceneMe, const TYPE_CLASS newTypeClass, const bool _is3d,
                           const bool _is2ds) noexcept : idScene(idSceneMe),
                                                         typeClass(newTypeClass),
                                                         is3D(_is3d),
                                                         is2dS(_is2ds),
                                                         position(0, 0, 0),
                                                         scale(1, 1, 1),
                                                         angle(0, 0, 0),
                                                         bounding_AABB(0, 0, 0)
{
    this->enableRender      = true;
    this->alwaysRenderize   = false;
    this->isRender2Texture  = false;
    this->__distFromView    = 0;
    this->userData          = nullptr;
    this->isObjectOnFrustum = true;
    }
    RENDERIZABLE::~RENDERIZABLE() noexcept
    {
        std::map<std::string, DYNAMIC_VAR *>::const_iterator it;
        for (it = this->lsDynamicVar.cbegin(); it != this->lsDynamicVar.cend(); ++it)
        {
            DYNAMIC_VAR *dVar = it->second;
            if (dVar)
                delete dVar;
        }
        this->lsDynamicVar.clear();
    }
    DYNAMIC_VAR * RENDERIZABLE::getDynamicVar(const char *nameVar)noexcept
    {
        return this->lsDynamicVar[nameVar];
    }
    void RENDERIZABLE::setDynamicVar(const char *nameVar, DYNAMIC_VAR *nDvar)noexcept
    {
        DYNAMIC_VAR *oldVar = this->lsDynamicVar[nameVar];
        if (oldVar)
            delete oldVar;
        oldVar                      = nullptr;
        this->lsDynamicVar[nameVar] = nDvar;
    }
    int RENDERIZABLE::getIdScene() const noexcept
    {
        return idScene;
    }
    const char * RENDERIZABLE::getFileName() const noexcept
    {
        return this->fileName.c_str();
    }
    void RENDERIZABLE::getAABB(float *w, float *h) const
    {
        *w = this->bounding_AABB.x;
        *h = this->bounding_AABB.y;
    }
    void RENDERIZABLE::getAABB(float *w, float *h, float *d) const
    {
        *w = this->bounding_AABB.x;
        *h = this->bounding_AABB.y;
        *d = this->bounding_AABB.z;
    }
    bool RENDERIZABLE::getWidthHeight(float *w, float *h, const bool consider_scale) const
    {
        const INFO_PHYSICS *infoPhysics = this->getInfoPhysics();
        float               x = 0, y = 0;
        if (infoPhysics && infoPhysics->getBounds(&x, &y))
        {
            if(consider_scale)
            {
                *w = x * this->scale.x, *h = y * this->scale.y;
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
                VEC3 halfDim(x * 0.5f * this->scale.x, y * 0.5f * this->scale.y, z * 0.5f * this->scale.z);
                *w = x * this->scale.x, *h = y * this->scale.y;
                *d = z * this->scale.z;
            }
            else
            {
                VEC3 halfDim(x * 0.5f, y * 0.5f, z * 0.5f);
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
        // dir is unit direction vector of ray
        const VEC3 dirfrac(dir.x != 0.0f ? 1.0f / dir.x : 0.0f, dir.y != 0.0f ? 1.0f / dir.y : 0.0f,
                           dir.z != 0.0f ? 1.0f / dir.z : 0.0f);
        float t1 = ((this->position.x + w) - p1.x) * dirfrac.x;
        float t2 = ((this->position.x - w) - p1.x) * dirfrac.x;
        float t3 = ((this->position.y + h) - p1.y) * dirfrac.y;
        float t4 = ((this->position.y - h) - p1.y) * dirfrac.y;
        float t5 = ((this->position.z + d) - p1.z) * dirfrac.z;
        float t6 = ((this->position.z - d) - p1.z) * dirfrac.z;

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
        if (device->isPointScreen2DOnRectangleWorld2d(point, halfDim, this->position))
            return true;
        return false;
    }
    bool RENDERIZABLE::isOver2ds(DEVICE *device, const float x, const float y) const
    {
        float w, h;
        this->getAABB(&w, &h);
        const VEC2 point(x, y);
        VEC2       halfDim(w * 0.5f, h * 0.5f);
        if (device->isPointScreen2DOnRectangleScreen2d(point, halfDim, this->position))
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
                if (this->is3D)
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
                    MatrixTranslationRotationScale(&matrix, &this->position, &this->angle, &this->scale);
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

                    this->bounding_AABB.x = (box_max.x - box_min.x);
                    this->bounding_AABB.y = (box_max.y - box_min.y);
                    this->bounding_AABB.z = (box_max.z - box_min.z);
                }
                else
                {
                    infoPhysics->getBounds(&x, &y);
                    VEC2 halfDim(x * 0.5f * this->scale.x, y * 0.5f * this->scale.y);
                    util::getAABB(halfDim, this->angle.z, &this->bounding_AABB.x, &this->bounding_AABB.y);
                }
            }
        }
    }


    RENDERIZABLE_TO_TARGET::RENDERIZABLE_TO_TARGET(const SCENE* scene, const TYPE_CLASS newTypeClass, const bool _is3d, const bool _is2ds) noexcept:
    RENDERIZABLE(scene->getIdScene() ,newTypeClass,_is3d,_is2ds)
    {
        this->idDepthRenderbuffer    = 0;
        this->idFrameBuffer          = 0;
        this->idTextureDynamic       = 0;
        this->colorClearBackGround   = COLOR(255, 255, 255); // alpha em 0 significa transparente
        this->colorClearBackGround.a = 1.0f;
        this->widthTexture           = 0;
        this->heightTexture          = 0;
    }

    RENDERIZABLE_TO_TARGET::~RENDERIZABLE_TO_TARGET()
    {
        if (this->idDepthRenderbuffer)
        {
            GLDeleteRenderbuffers(1, &this->idDepthRenderbuffer);
        }
        this->idDepthRenderbuffer = 0;

        if (this->idFrameBuffer)
        {
            GLDeleteFramebuffers(1, &this->idFrameBuffer);
        }
        this->idFrameBuffer = 0;
    }

	bool RENDERIZABLE::clone(RENDERIZABLE* renderizable_clone) const
	{
		if(renderizable_clone && this->isLoaded())
		{
			renderizable_clone->fileName = this->fileName;
			if(renderizable_clone->onRestoreDevice())
			{
				renderizable_clone->position = this->position;
				renderizable_clone->scale    = this->scale;
				renderizable_clone->angle    = this->angle;
				return true;
			}
		}
		return false;
	}

	const char * RENDERIZABLE::getTypeClassName() const noexcept
    {
        switch (typeClass)
        {
            case TYPE_CLASS_MESH                : return "mesh";
			case TYPE_CLASS_SPRITE              : return "sprite";
			case TYPE_CLASS_TEXTURE             : return "texture";
			case TYPE_CLASS_BACKGROUND          : return "backGround";
			case TYPE_CLASS_GIF                 : return "gif";
			case TYPE_CLASS_TEXT                : return "font";
			case TYPE_CLASS_PRIMITIVE           : return "primitive";
			case TYPE_CLASS_LIGHT               : return "light";
			case TYPE_CLASS_TEMP                : return "temp";
			case TYPE_CLASS_SHAPE_MESH          : return "shape-mesh";
			case TYPE_CLASS_LINE_MESH           : return "line-mesh";
			case TYPE_CLASS_PARTICLE            : return "particle";
            case TYPE_CLASS_STEERED_PARTICLE    : return "steered-particle";
			case TYPE_CLASS_RENDER_2_TEX        : return "render-to-texture";
			case TYPE_CLASS_TILE                : return "tile";
			case TYPE_CLASS_TILE_OBJ            : return "tile-obj";
			default                             : return "unknown";
        }
    }
}
