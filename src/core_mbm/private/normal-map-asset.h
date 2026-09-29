/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef NORMAL_MAP_ASSET_H
#define NORMAL_MAP_ASSET_H

#include "normal-map-preparation.h"
#include <core_mbm/header-mesh.h>
#include <cstdio>
#include <map>

namespace mbm { namespace skeletal { struct CANONICAL_SKELETON; struct CANONICAL_WEIGHTS; } }

namespace mbm { namespace normal_map
{
    // Build transient skin weights in prepared vertex order. Validate the canonical
    // source once, retain its palette/frame identity, and never mutate authoring data.
    // Empty preparation needs no skeleton. Failure leaves output unchanged.
    bool remapSkinWeights(const skeletal::CANONICAL_SKELETON &skeleton,
                          const skeletal::CANONICAL_WEIGHTS &source,
                          uint32_t sourceFrame, uint32_t sourceVertexCount,
                          const PREPARED &prepared,
                          std::vector<skeletal::CANONICAL_WEIGHTS> &output, std::string &error);

    constexpr uint16_t SECTION_VERSION = 1;
    constexpr uint32_t PREPARATION_REVISION = 1;

    struct ASSET_FRAME
    {
        uint64_t sourceSignature = 0;
        PREPARED prepared;
    };
    using ASSET_FRAMES = std::map<uint32_t, ASSET_FRAME>;

    struct MATERIAL_SETTINGS
    {
        int greenSign = 1;
        float strength = 1.0f;
    };
    using MATERIAL_KEY = std::pair<uint32_t, uint32_t>; // frame, subset
    using MATERIAL_SETTINGS_MAP = std::map<MATERIAL_KEY, MATERIAL_SETTINGS>;
    bool validMaterialSettings(int greenSign, float strength) noexcept;
    MATERIAL_SETTINGS getMaterialSettings(const MATERIAL_SETTINGS_MAP &settings, uint32_t frame, uint32_t subset);
    bool setMaterialSettings(MATERIAL_SETTINGS_MAP &settings, uint32_t frame, uint32_t subset,
                             int greenSign, float strength);
    bool writeMaterialPayload(FILE *file, const MATERIAL_SETTINGS_MAP &settings);
    bool readMaterialPayload(util::MEM_CURSOR_V11 &cursor, uint16_t version,
                             MATERIAL_SETTINGS_MAP &settings, std::string &error);

    struct SUBSET_VIEW
    {
        int32_t start = 0;
        int32_t count = 0;
        bool requested = false;
    };

    // CPU pointers only; indices are absent for sequential non-indexed subsets.
    bool makeInput(const VEC3 *positions, const VEC3 *normals, const VEC2 *uv,
                   uint32_t vertexCount, const uint16_t *indices, uint32_t indexCount,
                   uint32_t drawMode, const std::vector<SUBSET_VIEW> &subsets,
                   INPUT &output, std::string &error);
    uint64_t sourceSignature(const INPUT &input);
    // Validate persisted batches against source data without running MikkTSpace.
    bool validate(const INPUT &input, const ASSET_FRAME &frame, std::string &error);
    bool writePayload(FILE *file, uint32_t frameIndex, const ASSET_FRAME &frame);
    bool readPayload(util::MEM_CURSOR_V11 &cursor, uint16_t version,
                     uint32_t &frameIndex, ASSET_FRAME &frame, std::string &error);
}}
#endif
