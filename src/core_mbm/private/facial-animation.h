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

#ifndef FACIAL_ANIMATION_H
#define FACIAL_ANIMATION_H

#include <core_mbm/primitives.h>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>
#include "../mesh-io-primitives.h"

namespace mbm::facial
{
    // Defensive input budgets, not GPU capacities. Both decoded sections are bounded before allocation.
    constexpr uint32_t MAX_SECTION_BYTES = 64u * 1024u * 1024u;
    constexpr uint32_t MAX_VERTICES = 1000000;
    constexpr uint32_t MAX_TARGETS = 256;
    constexpr uint32_t MAX_CLIPS = 256;
    constexpr uint32_t MAX_WEIGHT_VALUES = 1000000;
    struct TARGET
    {
        uint64_t id = 0;
        std::string name;
        std::string convention; // Optional author label, e.g. ARKit; never interpreted by the runtime.
        float defaultWeight = 0;
        std::vector<VEC3> positions;
        std::vector<VEC3> normals;
    };
    struct KEY
    {
        float time = 0;
        std::vector<float> weights;
    };
    struct CLIP
    {
        uint64_t id = 0;
        std::string name;
        float duration = 0;
        bool loop = false;
        uint32_t interpolation = 1; // 0 step, 1 linear.
        std::vector<KEY> keys;
    };
    struct ASSET
    {
        uint64_t meshId = 0;
        uint64_t geometrySignature = 0;
        uint32_t vertexCount = 0;
        std::vector<TARGET> targets;
        std::vector<CLIP> clips;
    };
    uint64_t geometrySignature(const VEC3 *positions, const VEC3 *normals, uint32_t vertices,
                               const uint16_t *indices, uint32_t indexCount) noexcept;
    bool validate(const ASSET &asset, std::string &error);
    bool validateGeometry(const ASSET &asset, const VEC3 *positions, const VEC3 *normals,
                           uint32_t vertices, const uint16_t *indices, uint32_t indexCount,
                           std::string &error);
    bool readTargets(util::MEM_CURSOR_V11 &cursor, uint16_t version, ASSET &out, std::string &error);
    bool readClips(util::MEM_CURSOR_V11 &cursor, uint16_t version, ASSET &out, std::string &error);
    bool writeTargets(FILE *file, const ASSET &asset);
    bool writeClips(FILE *file, const ASSET &asset);
    // Evaluates only immutable bind data; output buffers are caller/instance-owned.
    bool deform(const ASSET &asset, const std::vector<VEC3> &basePositions,
                 const std::vector<VEC3> &baseNormals, const std::vector<float> &weights,
                 std::vector<VEC3> &positions, std::vector<VEC3> &normals, std::string &error);
}
#endif
