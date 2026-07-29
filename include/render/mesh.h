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

#ifndef MESH_3D_GLES_H
#define MESH_3D_GLES_H

#pragma once

#include <core_mbm/core-exports.h>
#include <core_mbm/device.h>
#include <core_mbm/shader-fx.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/animation.h>
#include <core_mbm/physics.h>
#include <functional>

namespace mbm
{

class MESH : public RENDERIZABLE, public ANIMATION_MANAGER
{
  public:
    API_IMPL MESH(const SCENE *scene, const bool _is3d, const bool _is2dScreen);
    API_IMPL virtual ~MESH();
    API_IMPL void release();
    API_IMPL bool load(const char *fileName);
    // Background-thread-friendly equivalent of load(): does the file I/O + v11 parsing on a worker
    // thread (MESH_MANAGER::loadAsync), runs the exact same
    // finish logic load() runs, then invokes callback(success) - always from pumpAsyncLoads() on
    // the main thread, never inline, matching MESH_MANAGER::loadAsync's own contract - EXCEPT when
    // this->mesh is already set on this specific MESH instance (see the top of the .cpp), which
    // fires callback(true) inline immediately; that early-out is independent of and predates
    // MESH_MANAGER's own (queue-only, never-inline) cache-hit handling.
    API_IMPL void loadAsync(const char *fileName, std::function<void(bool success)> callback);
    API_IMPL const char *getFileName() const;
    API_IMPL bool playArticulatedAnimation(const char *name, const int priority = 0);
    API_IMPL bool pauseArticulatedAnimation(const char *name) noexcept;
    API_IMPL bool resumeArticulatedAnimation(const char *name) noexcept;
    API_IMPL bool disableArticulatedAnimation(const char *name) noexcept;
    API_IMPL FX*  getFx() const override;
	  API_IMPL ANIMATION_MANAGER*  getAnimationManager() override;
    FVF_PROVIDE_BY_ENGINE getFvfFromBuffer() const noexcept override;

  private:
    bool                     render() override;
    bool                     onRestoreDevice() override;
    bool                     isOnFrustum() override;
    const mbm::INFO_PHYSICS *getInfoPhysics() const override;
    const MESH_MBM *         getMesh() const override;
    bool                     isLoaded() const override;
    MESH_MBM *               mesh;
    };
}

#endif
