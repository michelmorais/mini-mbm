/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef SPRITE_GLES_H
#define SPRITE_GLES_H

#include <core_mbm/core-exports.h>
#include <core_mbm/device.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/animation.h>
#include <core_mbm/physics.h>

namespace mbm
{

class SPRITE : public RENDERIZABLE, public COMMON_DEVICE, public ANIMATION_MANAGER
{
  public:
    friend class PHYSICS_BOX2D;
    friend class RENDER_2_TEXTURE;
    API_IMPL SPRITE(const SCENE *scene, const bool _is3d, const bool _is2dScreen);
    API_IMPL virtual ~SPRITE();
    API_IMPL void release();
    API_IMPL bool load(const char *fileName);
    API_IMPL const char *getFileName();
	API_IMPL FX*  getFx() const override;
	API_IMPL ANIMATION_MANAGER*  getAnimationManager() override;

  private:
    bool                     isOnFrustum() override;
    bool                     render() override;
    bool                     onRestoreDevice() override;
    void                     onStop() override;
    const mbm::INFO_PHYSICS *getInfoPhysics() const override;
    const MESH_MBM *         getMesh() const override;
    bool                     isLoaded() const override;

    MESH_MBM *mesh;
    };
}

#endif
