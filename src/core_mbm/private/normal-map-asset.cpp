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

#include "normal-map-asset.h"
#include "skeletal-animation-foundation.h"
#include "../mesh-io-primitives.h"
#include <core_mbm/draw-compatibility.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>

namespace mbm { namespace normal_map
{
namespace
{
    bool fail(std::string &error, const char *message) { error = message; return false; }

    bool validTangent(const TANGENT &t)
    {
        if (!std::isfinite(t.x) || !std::isfinite(t.y) || !std::isfinite(t.z))
            return false;
        if (t.sign == 0)
            return t.x == 0 && t.y == 0 && t.z == 0;
        const double length = static_cast<double>(t.x)*t.x + static_cast<double>(t.y)*t.y +
                              static_cast<double>(t.z)*t.z;
        return (t.sign == 1 || t.sign == -1) && std::abs(length-1.0) <= 0.002;
    }

    size_t cornerCount(const SUBSET_INPUT &subset)
    {
        if (subset.topology == TOPOLOGY::TRIANGLES)
            return subset.indices.size();
        return subset.indices.size() < 3 ? 0 : (subset.indices.size()-2)*3;
    }

    uint32_t sourceCorner(const SUBSET_INPUT &subset, size_t corner)
    {
        const size_t face = corner/3, local = corner%3;
        if (subset.topology == TOPOLOGY::TRIANGLES)
            return subset.indices[corner];
        if (subset.topology == TOPOLOGY::TRIANGLE_FAN)
            return subset.indices[local == 0 ? 0 : face+local];
        if (face%2 && local < 2)
            return subset.indices[face+1-local];
        return subset.indices[face+local];
    }
}

bool makeInput(const VEC3 *positions, const VEC3 *normals, const VEC2 *uv,
               uint32_t vertexCount, const uint16_t *indices, uint32_t indexCount,
               uint32_t drawMode, const std::vector<SUBSET_VIEW> &subsets,
               INPUT &output, std::string &error)
{
    error.clear();
    INPUT input;
    if (!std::any_of(subsets.begin(), subsets.end(), [](const SUBSET_VIEW &s) { return s.requested; }))
    {
        output = std::move(input);
        return true;
    }
    TOPOLOGY topology;
    if (drawMode == util::MODE_DRAW_TRIANGLES) topology = TOPOLOGY::TRIANGLES;
    else if (drawMode == util::MODE_DRAW_TRIANGLE_STRIP) topology = TOPOLOGY::TRIANGLE_STRIP;
    else if (drawMode == util::MODE_DRAW_TRIANGLE_FAN) topology = TOPOLOGY::TRIANGLE_FAN;
    else
    {
        // Points/lines have no surface basis.
        output = std::move(input);
        return true;
    }
    if (!normals || !uv)
    {
        output = std::move(input);
        return true;
    }
    if (!positions || vertexCount == 0)
        return fail(error, "normal-map source has no positions");
    input.positions.assign(positions, positions+vertexCount);
    input.normals.assign(normals, normals+vertexCount);
    input.uv.assign(uv, uv+vertexCount);
    for (const SUBSET_VIEW &view : subsets)
    {
        SUBSET_INPUT subset;
        subset.requested = view.requested;
        subset.topology = topology;
        if (view.requested)
        {
            const uint32_t limit = indices ? indexCount : vertexCount;
            if (view.start < 0 || view.count < 0 || static_cast<uint32_t>(view.start) > limit ||
                static_cast<uint32_t>(view.count) > limit-static_cast<uint32_t>(view.start))
                return fail(error, "normal-map subset range exceeds the source buffer");
            for (uint32_t i = 0; i < static_cast<uint32_t>(view.count); ++i)
            {
                const uint32_t offset = static_cast<uint32_t>(view.start)+i;
                const uint32_t index = indices ? indices[offset] : offset;
                if (index >= vertexCount)
                    return fail(error, "normal-map source index exceeds the vertex buffer");
                subset.indices.push_back(index);
            }
            if ((topology == TOPOLOGY::TRIANGLES && subset.indices.size()%3) ||
                (!subset.indices.empty() && subset.indices.size() < 3))
                return fail(error, "normal-map subset has an incomplete primitive");
        }
        input.subsets.push_back(std::move(subset));
    }
    output = std::move(input);
    return true;
}

uint64_t sourceSignature(const INPUT &input)
{
    // FNV-1a over canonical little-endian scalar bytes; never hash padding/pointers.
    uint64_t hash = 14695981039346656037ull;
    const auto integer = [&hash](uint32_t value)
    {
        for (int i = 0; i < 4; ++i)
        {
            hash ^= value & 255u;
            hash *= 1099511628211ull;
            value >>= 8;
        }
    };
    const auto scalar = [&integer](float value)
    {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        integer(bits);
    };
    integer(PREPARATION_REVISION);
    integer(static_cast<uint32_t>(input.positions.size()));
    for (const VEC3 &v : input.positions) { scalar(v.x); scalar(v.y); scalar(v.z); }
    integer(static_cast<uint32_t>(input.normals.size()));
    for (const VEC3 &v : input.normals) { scalar(v.x); scalar(v.y); scalar(v.z); }
    integer(static_cast<uint32_t>(input.uv.size()));
    for (const VEC2 &v : input.uv) { scalar(v.x); scalar(v.y); }
    integer(static_cast<uint32_t>(input.subsets.size()));
    for (const SUBSET_INPUT &s : input.subsets)
    {
        integer(static_cast<uint32_t>(s.topology)); integer(s.requested ? 1 : 0);
        integer(static_cast<uint32_t>(s.indices.size()));
        for (uint32_t index : s.indices) integer(index);
    }
    return hash;
}

bool validate(const INPUT &input, const ASSET_FRAME &frame, std::string &error)
{
    error.clear();
    if (frame.sourceSignature != sourceSignature(input) || frame.prepared.batches.empty())
        return fail(error, "normal-map source signature mismatch or empty prepared section");
    if (input.normals.size() != input.positions.size() || input.uv.size() != input.positions.size())
        return fail(error, "normal-map source arrays are inconsistent");
    for (size_t i = 0; i < input.positions.size(); ++i)
    {
        const VEC3 &p = input.positions[i], &n = input.normals[i];
        const VEC2 &uv = input.uv[i];
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) ||
            !std::isfinite(n.x) || !std::isfinite(n.y) || !std::isfinite(n.z) ||
            !std::isfinite(uv.x) || !std::isfinite(uv.y))
            return fail(error, "non-finite normal-map source data");
    }
    std::vector<size_t> corners(input.subsets.size(), 0);
    uint32_t unusable = 0, previousSubset = 0;
    for (const BATCH &batch : frame.prepared.batches)
    {
        if (batch.subset >= input.subsets.size() || batch.subset < previousSubset ||
            !input.subsets[batch.subset].requested || batch.sourceVertices.empty() ||
            batch.sourceVertices.size() > 65536 || batch.sourceVertices.size() != batch.tangents.size() ||
            batch.indices.empty() || batch.indices.size()%3)
            return fail(error, "invalid normal-map batch");
        previousSubset = batch.subset;
        const SUBSET_INPUT &subset = input.subsets[batch.subset];
        size_t &corner = corners[batch.subset];
        const size_t expected = cornerCount(subset);
        if (corner > expected || batch.indices.size() > expected-corner)
            return fail(error, "normal-map batch exceeds source triangle count");
        std::vector<bool> used(batch.sourceVertices.size(), false);
        for (size_t i = 0; i < batch.sourceVertices.size(); ++i)
        {
            const uint32_t source = batch.sourceVertices[i];
            const TANGENT &t = batch.tangents[i];
            if (source >= input.positions.size() || !validTangent(t))
                return fail(error, "invalid normal-map source vertex or tangent");
            const VEC3 &n = input.normals[source];
            const double length = static_cast<double>(n.x)*n.x+static_cast<double>(n.y)*n.y+static_cast<double>(n.z)*n.z;
            const double dot = static_cast<double>(t.x)*n.x+static_cast<double>(t.y)*n.y+static_cast<double>(t.z)*n.z;
            if (t.sign != 0 && (!std::isfinite(length) || length <= 1.0e-30 ||
                               !std::isfinite(dot) || std::abs(dot) > 0.002*std::sqrt(length)))
                return fail(error, "normal-map tangent is not orthogonal to its source normal");
        }
        for (size_t i = 0; i < batch.indices.size(); ++i)
        {
            const uint16_t index = batch.indices[i];
            if (index >= batch.sourceVertices.size() || batch.sourceVertices[index] != sourceCorner(subset, corner++))
                return fail(error, "normal-map triangles do not match source geometry");
            used[index] = true;
            if (i%3 == 0)
            {
                const bool disabled = batch.tangents[index].sign == 0;
                for (size_t j = 1; j < 3; ++j)
                {
                    if (batch.indices[i+j] >= batch.tangents.size() ||
                        (batch.tangents[batch.indices[i+j]].sign == 0) != disabled)
                        return fail(error, "normal-map fallback must cover an entire triangle");
                }
                if (disabled) ++unusable;
            }
        }
        if (std::find(used.begin(), used.end(), false) != used.end())
            return fail(error, "normal-map batch contains unused vertices");
    }
    for (size_t i = 0; i < input.subsets.size(); ++i)
        if (input.subsets[i].requested && corners[i] != cornerCount(input.subsets[i]))
            return fail(error, "normal-map section omits source triangles");
    if (unusable != frame.prepared.unusableTriangles)
        return fail(error, "normal-map fallback triangle count mismatch");
    return true;
}

bool writePayload(FILE *file, uint32_t frameIndex, const ASSET_FRAME &frame)
{
    using namespace util::le_io;
    if (frame.prepared.batches.empty() || frame.prepared.batches.size() > UINT32_MAX ||
        !writeU32LE(file, frameIndex) || !writeU32LE(file, PREPARATION_REVISION) ||
        !writeU64LE(file, frame.sourceSignature) || !writeU32LE(file, frame.prepared.unusableTriangles) ||
        !writeU32LE(file, static_cast<uint32_t>(frame.prepared.batches.size())))
        return false;
    for (const BATCH &b : frame.prepared.batches)
    {
        if (b.sourceVertices.empty() || b.sourceVertices.size() > 65536 ||
            b.sourceVertices.size() != b.tangents.size() || b.indices.empty() ||
            b.indices.size() > UINT32_MAX || b.indices.size()%3 ||
            !writeU32LE(file, b.subset) || !writeU32LE(file, static_cast<uint32_t>(b.sourceVertices.size())) ||
            !writeU32LE(file, static_cast<uint32_t>(b.indices.size())))
            return false;
        for (size_t i = 0; i < b.sourceVertices.size(); ++i)
        {
            const TANGENT &t = b.tangents[i];
            if (!validTangent(t) || !writeU32LE(file, b.sourceVertices[i]) ||
                !writeF32LE(file, t.x) || !writeF32LE(file, t.y) || !writeF32LE(file, t.z) || !writeF32LE(file, t.sign))
                return false;
        }
        for (uint16_t index : b.indices)
            if (index >= b.sourceVertices.size() || !writeU16LE(file, index)) return false;
    }
    return true;
}

bool readPayload(util::MEM_CURSOR_V11 &cursor, uint16_t version,
                 uint32_t &frameIndex, ASSET_FRAME &frame, std::string &error)
{
    using namespace util::le_io;
    error.clear();
    ASSET_FRAME candidate;
    if (cursor.pos > cursor.size) return fail(error, "invalid normal-map payload cursor");
    uint32_t index, revision, count;
    if (version != SECTION_VERSION || !readU32LE(cursor, index) || !readU32LE(cursor, revision) ||
        revision != PREPARATION_REVISION || !readU64LE(cursor, candidate.sourceSignature) ||
        !readU32LE(cursor, candidate.prepared.unusableTriangles) || !readU32LE(cursor, count) ||
        count == 0 || count > (cursor.size-cursor.pos)/38)
        return fail(error, "invalid normal-map section header or revision");
    for (uint32_t i = 0; i < count; ++i)
    {
        BATCH batch;
        uint32_t vertices, indices;
        if (!readU32LE(cursor, batch.subset) || !readU32LE(cursor, vertices) || !readU32LE(cursor, indices) ||
            vertices == 0 || vertices > 65536 || indices == 0 || indices%3 ||
            static_cast<uint64_t>(vertices)*20+static_cast<uint64_t>(indices)*2 > cursor.size-cursor.pos)
            return fail(error, "invalid normal-map batch lengths");
        batch.sourceVertices.resize(vertices); batch.tangents.resize(vertices); batch.indices.resize(indices);
        for (uint32_t j = 0; j < vertices; ++j)
        {
            TANGENT &t = batch.tangents[j];
            if (!readU32LE(cursor, batch.sourceVertices[j]) || !readF32LE(cursor, t.x) ||
                !readF32LE(cursor, t.y) || !readF32LE(cursor, t.z) || !readF32LE(cursor, t.sign) || !validTangent(t))
                return fail(error, "invalid normal-map tangent record");
        }
        for (uint16_t &local : batch.indices)
            if (!readU16LE(cursor, local) || local >= vertices)
                return fail(error, "normal-map local index out of range");
        candidate.prepared.batches.push_back(std::move(batch));
    }
    if (cursor.pos != cursor.size)
        return fail(error, "trailing normal-map section bytes");
    frameIndex = index;
    frame = std::move(candidate);
    return true;
}

bool validMaterialSettings(int greenSign, float strength) noexcept
{
    return (greenSign == 1 || greenSign == -1) && std::isfinite(strength) && strength >= 0.0f;
}

MATERIAL_SETTINGS getMaterialSettings(const MATERIAL_SETTINGS_MAP &settings, uint32_t frame, uint32_t subset)
{
    const auto found = settings.find({frame, subset});
    return found == settings.end() ? MATERIAL_SETTINGS{} : found->second;
}

bool setMaterialSettings(MATERIAL_SETTINGS_MAP &settings, uint32_t frame, uint32_t subset,
                         int greenSign, float strength)
{
    if (!validMaterialSettings(greenSign, strength)) return false;
    if (greenSign == 1 && strength == 1.0f) settings.erase({frame, subset});
    else settings[{frame, subset}] = {greenSign, strength};
    return true;
}

bool writeMaterialPayload(FILE *file, const MATERIAL_SETTINGS_MAP &settings)
{
    using namespace util::le_io;
    if (settings.empty() || settings.size() > UINT32_MAX ||
        !writeU32LE(file, static_cast<uint32_t>(settings.size()))) return false;
    for (const auto &entry : settings)
    {
        const auto &value = entry.second;
        if (!validMaterialSettings(value.greenSign, value.strength) ||
            !writeU32LE(file, entry.first.first) || !writeU32LE(file, entry.first.second) ||
            !writeU32LE(file, value.greenSign == 1 ? 0u : 1u) || !writeF32LE(file, value.strength)) return false;
    }
    return true;
}

bool readMaterialPayload(util::MEM_CURSOR_V11 &cursor, uint16_t version,
                         MATERIAL_SETTINGS_MAP &settings, std::string &error)
{
    using namespace util::le_io;
    uint32_t count = 0;
    if (version != 1 || !readU32LE(cursor, count) || count == 0 ||
        cursor.pos > cursor.size || count != (cursor.size - cursor.pos) / 16 ||
        (cursor.size - cursor.pos) % 16 != 0)
        return fail(error, "invalid normal-map material section version or size");
    MATERIAL_SETTINGS_MAP candidate;
    for (uint32_t i = 0; i < count; ++i)
    {
        uint32_t frame = 0, subset = 0, convention = 0;
        float strength = 0;
        if (!readU32LE(cursor, frame) || !readU32LE(cursor, subset) ||
            !readU32LE(cursor, convention) || !readF32LE(cursor, strength) || convention > 1 ||
            !validMaterialSettings(convention == 0 ? 1 : -1, strength))
            return fail(error, "invalid normal-map material settings");
        if (!candidate.emplace(MATERIAL_KEY{frame, subset}, MATERIAL_SETTINGS{convention == 0 ? 1 : -1, strength}).second)
            return fail(error, "duplicate normal-map material entry");
    }
    settings = std::move(candidate);
    return true;
}
bool remapSkinWeights(const skeletal::CANONICAL_SKELETON &skeleton,
                      const skeletal::CANONICAL_WEIGHTS &source,
                      uint32_t sourceFrame, uint32_t sourceVertexCount,
                      const PREPARED &prepared,
                      std::vector<skeletal::CANONICAL_WEIGHTS> &output, std::string &error)
{
    error.clear();
    if (prepared.batches.empty())
    {
        output.clear();
        return true;
    }
    if (source.frameIndex != sourceFrame ||
        !skeletal::validateCanonicalWeights(skeleton, source, sourceVertexCount))
    {
        error = "Invalid canonical skin weights or source frame for tangent batches";
        return false;
    }
    std::vector<skeletal::CANONICAL_WEIGHTS> candidate;
    candidate.reserve(prepared.batches.size());
    for (const BATCH &batch : prepared.batches)
    {
        if (batch.sourceVertices.empty() || batch.sourceVertices.size() > 65536 ||
            batch.sourceVertices.size() != batch.tangents.size() ||
            batch.indices.empty() || batch.indices.size() % 3 != 0)
        {
            error = "Invalid tangent batch layout for skin remapping";
            return false;
        }
        for (uint16_t index : batch.indices)
        {
            if (index >= batch.sourceVertices.size())
            {
                error = "Invalid local tangent index for skin remapping";
                return false;
            }
        }
        skeletal::CANONICAL_WEIGHTS weights;
        weights.skeletonId = source.skeletonId;
        weights.frameIndex = source.frameIndex;
        weights.paletteBoneIds = source.paletteBoneIds;
        weights.vertices.reserve(batch.sourceVertices.size());
        for (uint32_t vertex : batch.sourceVertices)
        {
            if (vertex >= source.vertices.size())
            {
                error = "Tangent batch references an invalid skin source vertex";
                return false;
            }
            weights.vertices.push_back(source.vertices[vertex]);
        }
        candidate.push_back(std::move(weights));
    }
    output = std::move(candidate);
    return true;
}
}}
