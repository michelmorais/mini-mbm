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

#ifndef LINE_MESH_GLES_H
#define LINE_MESH_GLES_H

#include <core_mbm/core-exports.h>
#include <core_mbm/device.h>
#include <core_mbm/shader-fx.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/animation.h>
#include <core_mbm/physics.h>

namespace mbm
{
    class MY_LINES
    {
      public:
        uint32_t vboVertexUvLine;
        MY_LINES()noexcept;
        virtual ~MY_LINES();
        void release();
        VEC3 *getArray();
        uint32_t getSize() const;
        bool setLines(std::vector<VEC3> && arrayPoints,const bool invert_Y);
        bool renderLines(SHADER *shader);
        bool onRestore();
      private:
        std::vector<VEC3> arrayLinesVec3;
    };

    class LINE_MESH : public RENDERIZABLE, public COMMON_DEVICE, public ANIMATION_MANAGER
    {
      public:
        mbm::COLOR color;
        API_IMPL LINE_MESH(const SCENE *scene, const bool _is3d, const bool _is2dScreen);
        API_IMPL virtual ~LINE_MESH();
        API_IMPL void release();
        API_IMPL uint32_t set(std::vector<VEC3> && arrayLines, uint32_t index);
        API_IMPL uint32_t add(std::vector<VEC3> && arrayLines);
        API_IMPL uint32_t getTotalLines() const;
        API_IMPL uint32_t getTotalPoints(const uint32_t idLine) const;
        API_IMPL mbm::INFO_PHYSICS *getNotConstInfoPhysics();
        API_IMPL void drawBounding(RENDERIZABLE* ptr,const bool useAABB)noexcept;
		API_IMPL FX*  getFx() const override;
		API_IMPL ANIMATION_MANAGER*  getAnimationManager() override;

    private:
        bool isOnFrustum() override;
        bool render() override;
        void onStop() override;
        bool onRestoreDevice() override;
        bool loadShaderDefault();
        bool createAnimationAndShader2Line();
        const mbm::INFO_PHYSICS *getInfoPhysics() const override;
        const MESH_MBM *getMesh() const override;
        bool isLoaded() const override;

        std::vector<MY_LINES *> lsLines;
        mbm::INFO_PHYSICS       infoPhysics;
    };
}

#endif
