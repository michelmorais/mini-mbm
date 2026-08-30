/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026 by Michel Braz de Morais <michel.braz.morais@gmail.com>                                             |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation       |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions.         |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef MESH_SIMPLIFIER_H
#define MESH_SIMPLIFIER_H

#include <core_mbm/primitives.h>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace mbm::mesh_simplifier
{
    struct INPUT
    {
        std::vector<VEC3> positions;
        std::vector<VEC3> normals;
        std::vector<VEC2> uvs;
        std::vector<uint32_t> indices;
        std::vector<uint32_t> triangleGroups;
        std::vector<std::vector<VEC3>> deformationDeltas;
        bool preserveDetails = true;
    };

    struct OUTPUT
    {
        std::vector<VEC3> positions;
        std::vector<VEC3> normals;
        std::vector<VEC2> uvs;
        std::vector<uint32_t> indices;
        std::vector<uint32_t> triangleGroups;
        std::vector<std::vector<std::pair<uint32_t, float>>> sourceContributions;
        std::vector<std::vector<VEC3>> deformationDeltas;
        float maximumError = 0.0f;
        float maximumPoseError = 0.0f;
        float maximumRelativeError = 0.0f;
        uint32_t collapseCount = 0;
        uint32_t boundaryRejectedCollapseCount = 0;
        uint32_t topologyRejectedCollapseCount = 0;
        uint32_t orientationRejectedCollapseCount = 0;
        uint32_t invalidRejectedCollapseCount = 0;
        uint32_t degenerateTriangleCount = 0;
        uint32_t nonManifoldEdgeCount = 0;
        uint32_t connectedComponentCount = 0;
        uint32_t detailPenalizedCandidateCount = 0;
        uint32_t detailPenalizedCollapseCount = 0;
        uint32_t clearanceRejectedCollapseCount = 0;
    };

    bool simplify(const INPUT &input, uint32_t targetTriangleCount, OUTPUT &output,
                  std::string &errorOut);
}

#endif
