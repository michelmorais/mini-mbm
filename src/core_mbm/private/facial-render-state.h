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

#ifndef FACIAL_RENDER_STATE_H
#define FACIAL_RENDER_STATE_H
#include <core_mbm/mesh-manager.h>
#include <core_mbm/shapes.h>
#include <string>
#include <vector>
namespace mbm::facial
{
    struct RENDER_STATE
    {
        BUFFER_MESH buffer;
        INFO_PHYSICS bounds;
        std::vector<float> weights, sampledWeights, palette;
        std::vector<uint8_t> manualOverrides;
        std::vector<VEC3> morphPositions, morphNormals, positions, normals;
        std::vector<int> vertexStarts, vertexCounts;
        std::string status = "no-facial-data";
        uint32_t targetCount = 0, clipCount = 0;
        uint32_t clipIndex = UINT32_MAX;
        float clipTime = 0.0f, clipSpeed = 1.0f;
        uint64_t evaluations = 0, uploads = 0;
        bool ready = false, dirty = true, uploadDirty = true, initialized = false;
        bool clipPlaying = false, clipPaused = false;
        bool hadSkin = false, poseAdvancedForRender = false;
        RENDER_STATE() { bounds.lsCube.push_back(new CUBE()); }
    };
}
#endif
