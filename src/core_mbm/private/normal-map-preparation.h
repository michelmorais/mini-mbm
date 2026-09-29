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

#ifndef NORMAL_MAP_PREPARATION_H
#define NORMAL_MAP_PREPARATION_H

#include <core_mbm/primitives.h>
#include <cstdint>
#include <string>
#include <vector>

namespace mbm { namespace normal_map
{
    // Private CPU preparation data. No graphics context or public mesh mutation.
    struct TANGENT
    {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        // Zero marks an unusable basis: render with the mesh normal instead.
        float sign = 0.0f;
    };

    enum class TOPOLOGY : uint8_t { TRIANGLES, TRIANGLE_STRIP, TRIANGLE_FAN };
    enum class TANGENT_POLICY : uint8_t { GENERATE, IMPORT };

    struct SUBSET_INPUT
    {
        TOPOLOGY topology = TOPOLOGY::TRIANGLES;
        // Source vertex indices, also for non-indexed geometry (sequential indices).
        std::vector<uint32_t> indices;
        // The caller requests preparation only for a normal map or explicit precomputation.
        bool requested = false;
        // IMPORT policy: one tangent per expanded triangle corner, never per source vertex.
        std::vector<TANGENT> importedCorners;
    };

    struct INPUT
    {
        std::vector<VEC3> positions;
        std::vector<VEC3> normals;
        std::vector<VEC2> uv;
        std::vector<SUBSET_INPUT> subsets;
        TANGENT_POLICY policy = TANGENT_POLICY::GENERATE;
    };

    struct BATCH
    {
        uint32_t subset = 0;
        // Copy position/normal/UV and skin influences from this source vertex.
        std::vector<uint32_t> sourceVertices;
        std::vector<TANGENT> tangents;
        // Triangle list in source draw order; each batch fits 16-bit indices.
        std::vector<uint16_t> indices;
    };

    struct PREPARED
    {
        std::vector<BATCH> batches;
        uint32_t unusableTriangles = 0;
    };

    // All-or-nothing: failures leave output unchanged and provide an error.
    // No requested subsets, or missing normal/UV arrays, produce an empty result.
    // Missing arrays are valid for unlit/no-UV geometry; malformed arrays are errors.
    bool prepare(const INPUT &input, PREPARED &output, std::string &error);
}}

#endif
