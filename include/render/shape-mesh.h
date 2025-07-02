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

#ifndef SHAPE_MESH_GLES_H
#define SHAPE_MESH_GLES_H

#pragma once

#include <core_mbm/core-exports.h>
#include <core_mbm/device.h>
#include <core_mbm/shader-fx.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/animation.h>
#include <core_mbm/physics.h>
#include <memory>
#include <stdint.h>
#include <stdio.h>

namespace mbm 
{
    struct DeleteArrayUnShortInt 
    {
        void operator() (uint16_t* usi)  noexcept
        {
            delete [] usi;
        }
    };

    struct AUTO_VERTEX
    {
        float *             ls_xyz;
        float *             ls_normal;
        float *             ls_uv;
        uint16_t *ls_index;
        uint32_t        sizeArray;
        uint32_t        sizeIndex;

        constexpr AUTO_VERTEX() noexcept :
            ls_xyz    (nullptr),
            ls_normal (nullptr),
            ls_uv     (nullptr),
            ls_index  (nullptr),
            sizeArray (0),
            sizeIndex (0)
        {
        }
        ~AUTO_VERTEX()
        {
            if (this->ls_xyz)
                delete[] this->ls_xyz;
            if (this->ls_normal)
                delete[] this->ls_normal;
            if (this->ls_uv)
                delete[] this->ls_uv;
            if (this->ls_index)
                delete[] this->ls_index;
            this->ls_xyz    = nullptr;
            this->ls_normal = nullptr;
            this->ls_uv     = nullptr;
            this->ls_index  = nullptr;
        }
    };
    
    class SHAPE_MESH : public RENDERIZABLE, public COMMON_DEVICE, public ANIMATION_MANAGER
    {
      public:
        friend class PHYSICS_BOX2D;
		typedef void (*OnRenderDynamicBuffer)(SHAPE_MESH * shape, std::vector<float> & dynamicVertex,std::vector<float> & dynamicNormal,std::vector<float> & dynamicUV,const std::vector<uint16_t> & index_read_only);

        API_IMPL SHAPE_MESH(const SCENE *scene, const bool _is3d, const bool _is2dScreen);
        API_IMPL virtual ~SHAPE_MESH();
        API_IMPL void release();
        API_IMPL bool loadIndexedDynamic(const char *nickName, std::vector<float> &&_dynamicVertex,
                                       std::vector<float> &&_dynamicNormal, std::vector<float> &&_dynamicUV,
                                       std::unique_ptr<uint16_t[],DeleteArrayUnShortInt> &&indexArray,
                                       const uint32_t _sizeVertexArray, const uint32_t _sizeIndexArray,const util::INFO_DRAW_MODE * info_draw_mode);
    
        API_IMPL bool load(const char *nickName, mbm::AUTO_VERTEX *autoVertex,const util::INFO_DRAW_MODE * info_draw_mode);
        API_IMPL bool loadCircle(const char *nickName,float width, float height,bool dynamicBuffer, const int numTriangles = 8);
		API_IMPL bool loadRectangle(const char *nickName,float width, float height,bool dynamicBuffer, int numTriangles = 2);
		API_IMPL bool loadTriangle(const char *nickName,float width, float height,bool dynamicBuffer, const int numTriangles = 1);
        API_IMPL bool loadTriangle(const char *nickName,float points[6], bool dynamicBuffer);
        API_IMPL bool loadIndexed(const char *nickName, mbm::AUTO_VERTEX *autoVertex,const util::INFO_DRAW_MODE * info_draw_mode);
		API_IMPL bool setColor(const float r,const float g, const float b, const float a);
        API_IMPL const char *getFileName() const;
		API_IMPL FX*  getFx() const override;
		API_IMPL ANIMATION_MANAGER*  getAnimationManager()  noexcept override;
		API_IMPL void setOnRenderDynamicBuffer(OnRenderDynamicBuffer _onRenderDynamicBuffer) noexcept { onRenderDynamicBuffer = _onRenderDynamicBuffer;}
		API_IMPL const bool isDynamicBufferMode() const  noexcept { return dynamicVertex.size() > 0 && mesh != nullptr; }
		
      private:
        bool isOnFrustum() override;
        bool render() override;
        void onStop() override;
        bool onRestoreDevice() noexcept override;
        const mbm::INFO_PHYSICS *getInfoPhysics()  const  noexcept override;
        const MESH_MBM *getMesh() const noexcept override;
        bool isLoaded() const noexcept override;
    
        MESH_MBM *               mesh;
        std::vector<float> dynamicVertex;
        std::vector<float> dynamicNormal;
        std::vector<float> dynamicUV;
        std::vector<uint16_t> dynamicIndex;

		OnRenderDynamicBuffer onRenderDynamicBuffer;
    };
}

#endif
