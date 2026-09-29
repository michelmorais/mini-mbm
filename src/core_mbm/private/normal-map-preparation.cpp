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

#include "normal-map-preparation.h"
#include "../../../third-party/mikktspace/mikktspace.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>

namespace mbm { namespace normal_map
{
namespace
{
    struct TRIANGLE
    {
        uint32_t subset;
        std::array<uint32_t, 3> vertex;
        std::array<TANGENT, 3> tangent;
        bool usable = false;
    };

    bool finite(const VEC3 &v)
    {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
    }

    double lengthSquared(const VEC3 &v)
    {
        return static_cast<double>(v.x)*v.x + static_cast<double>(v.y)*v.y + static_cast<double>(v.z)*v.z;
    }

    bool validNormal(const VEC3 &v)
    {
        return finite(v) && lengthSquared(v) > 1.0e-30;
    }

    bool validTangent(const TANGENT &t, const VEC3 &n)
    {
        if (!std::isfinite(t.x) || !std::isfinite(t.y) || !std::isfinite(t.z) ||
            (t.sign != 1.0f && t.sign != -1.0f))
            return false;
        const double length = static_cast<double>(t.x)*t.x + static_cast<double>(t.y)*t.y + static_cast<double>(t.z)*t.z;
        const double dot = static_cast<double>(t.x)*n.x + static_cast<double>(t.y)*n.y + static_cast<double>(t.z)*n.z;
        return std::abs(length - 1.0) <= 0.002 &&
            std::abs(dot) <= 0.002 * std::sqrt(lengthSquared(n));
    }

    bool usableTriangle(const INPUT &input, const TRIANGLE &triangle)
    {
        for (uint32_t index : triangle.vertex)
        {
            if (!validNormal(input.normals[index]))
                return false;
        }
        const VEC3 &a = input.positions[triangle.vertex[0]];
        const VEC3 &b = input.positions[triangle.vertex[1]];
        const VEC3 &c = input.positions[triangle.vertex[2]];
        const double ex = static_cast<double>(b.x)-a.x, ey = static_cast<double>(b.y)-a.y, ez = static_cast<double>(b.z)-a.z;
        const double fx = static_cast<double>(c.x)-a.x, fy = static_cast<double>(c.y)-a.y, fz = static_cast<double>(c.z)-a.z;
        const double cx = ey*fz-ez*fy, cy = ez*fx-ex*fz, cz = ex*fy-ey*fx;
        const double scale = (ex*ex+ey*ey+ez*ez)*(fx*fx+fy*fy+fz*fz);
        if (scale == 0 || cx*cx+cy*cy+cz*cz <= scale*1.0e-20)
            return false;
        const VEC2 &u = input.uv[triangle.vertex[0]];
        const VEC2 &v = input.uv[triangle.vertex[1]];
        const VEC2 &w = input.uv[triangle.vertex[2]];
        const double ux = static_cast<double>(v.x)-u.x, uy = static_cast<double>(v.y)-u.y;
        const double vx = static_cast<double>(w.x)-u.x, vy = static_cast<double>(w.y)-u.y;
        const double determinant = ux*vy-uy*vx;
        const double uvScale = (ux*ux+uy*uy)*(vx*vx+vy*vy);
        return uvScale > 0 && determinant*determinant > uvScale*1.0e-20;
    }

    struct MIKK_CONTEXT
    {
        const INPUT &input;
        std::vector<TRIANGLE> &triangles;
        std::vector<size_t> faces;
    };

    MIKK_CONTEXT &data(const SMikkTSpaceContext *context)
    {
        return *static_cast<MIKK_CONTEXT *>(context->m_pUserData);
    }

    uint32_t sourceIndex(const SMikkTSpaceContext *context, int face, int corner)
    {
        const MIKK_CONTEXT &d = data(context);
        return d.triangles[d.faces[static_cast<size_t>(face)]].vertex[static_cast<size_t>(corner)];
    }

    int faceCount(const SMikkTSpaceContext *context)
    {
        return static_cast<int>(data(context).faces.size());
    }

    int cornerCount(const SMikkTSpaceContext *, int) { return 3; }

    void position(const SMikkTSpaceContext *context, float out[], int face, int corner)
    {
        const VEC3 &v = data(context).input.positions[sourceIndex(context, face, corner)];
        out[0] = v.x; out[1] = v.y; out[2] = v.z;
    }

    void normal(const SMikkTSpaceContext *context, float out[], int face, int corner)
    {
        const VEC3 &v = data(context).input.normals[sourceIndex(context, face, corner)];
        const double inverseLength = 1.0 / std::sqrt(lengthSquared(v));
        out[0] = static_cast<float>(v.x * inverseLength);
        out[1] = static_cast<float>(v.y * inverseLength);
        out[2] = static_cast<float>(v.z * inverseLength);
    }

    void texcoord(const SMikkTSpaceContext *context, float out[], int face, int corner)
    {
        const VEC2 &v = data(context).input.uv[sourceIndex(context, face, corner)];
        out[0] = v.x; out[1] = v.y;
    }

    void tangent(const SMikkTSpaceContext *context, const float value[], float sign, int face, int corner)
    {
        MIKK_CONTEXT &d = data(context);
        d.triangles[d.faces[static_cast<size_t>(face)]].tangent[static_cast<size_t>(corner)] =
            {value[0], value[1], value[2], sign};
    }

    // Exact identity preserves imported bases; never weld across source vertices or signs.
    using KEY = std::array<uint32_t, 5>;
    struct KEY_HASH
    {
        size_t operator()(const KEY &key) const noexcept
        {
            size_t hash = 0;
            for (uint32_t value : key)
                hash ^= std::hash<uint32_t>{}(value) + size_t(0x9e3779b9u) + (hash << 6) + (hash >> 2);
            return hash;
        }
    };

    KEY keyFor(uint32_t source, const TANGENT &t)
    {
        KEY key = {source, 0, 0, 0, 0};
        const float values[] = {t.x, t.y, t.z, t.sign};
        for (size_t i = 0; i < 4; ++i)
        {
            // Treat positive and negative zero identically, without changing stored values.
            const float value = values[i] == 0.0f ? 0.0f : values[i];
            std::memcpy(&key[i+1], &value, sizeof(value));
        }
        return key;
    }
}

bool prepare(const INPUT &input, PREPARED &output, std::string &error)
{
    error.clear();
    PREPARED prepared;
    const bool requested = std::any_of(input.subsets.begin(), input.subsets.end(),
        [](const SUBSET_INPUT &s) { return s.requested; });
    if (!requested)
    {
        output = std::move(prepared);
        return true;
    }
    const auto fail = [&error](const char *message) { error = message; return false; };
    if (input.policy != TANGENT_POLICY::GENERATE && input.policy != TANGENT_POLICY::IMPORT)
        return fail("invalid tangent preparation policy");
    if (input.positions.size() > UINT32_MAX || input.subsets.size() > UINT32_MAX)
        return fail("tangent input exceeds source index limits");
    if ((!input.normals.empty() && input.normals.size() != input.positions.size()) ||
        (!input.uv.empty() && input.uv.size() != input.positions.size()))
        return fail("normal and UV arrays must match the source vertex count");
    if (input.normals.empty() || input.uv.empty())
    {
        output = std::move(prepared);
        return true;
    }
    std::vector<TRIANGLE> triangles;
    for (size_t subsetIndex = 0; subsetIndex < input.subsets.size(); ++subsetIndex)
    {
        const SUBSET_INPUT &subset = input.subsets[subsetIndex];
        if (!subset.requested)
            continue;
        if (subset.topology != TOPOLOGY::TRIANGLES && subset.topology != TOPOLOGY::TRIANGLE_STRIP &&
            subset.topology != TOPOLOGY::TRIANGLE_FAN)
            return fail("unsupported tangent topology");
        const size_t count = subset.indices.size();
        if ((subset.topology == TOPOLOGY::TRIANGLES && count % 3 != 0) ||
            (count != 0 && count < 3))
            return fail("incomplete tangent input primitive");
        const size_t faces = subset.topology == TOPOLOGY::TRIANGLES ? count/3 : (count ? count-2 : 0);
        // MikkTSpace uses signed int for triangle-corner indexing internally.
        if (faces > static_cast<size_t>(INT32_MAX)/3 - triangles.size())
            return fail("tangent input exceeds MikkTSpace corner limits");
        if (input.policy == TANGENT_POLICY::IMPORT && subset.importedCorners.size() != faces*3)
            return fail("imported tangents must be supplied per expanded triangle corner");
        for (uint32_t index : subset.indices)
        {
            if (index >= input.positions.size())
                return fail("tangent source index is out of range");
            const VEC2 &uv = input.uv[index];
            if (!finite(input.positions[index]) || !finite(input.normals[index]) ||
                !std::isfinite(uv.x) || !std::isfinite(uv.y))
                return fail("non-finite tangent source data");
        }
        for (size_t face = 0; face < faces; ++face)
        {
            TRIANGLE triangle = {};
            triangle.subset = static_cast<uint32_t>(subsetIndex);
            if (subset.topology == TOPOLOGY::TRIANGLES)
                triangle.vertex = {subset.indices[face*3], subset.indices[face*3+1], subset.indices[face*3+2]};
            else if (subset.topology == TOPOLOGY::TRIANGLE_FAN)
                triangle.vertex = {subset.indices[0], subset.indices[face+1], subset.indices[face+2]};
            else if (face % 2 == 0)
                triangle.vertex = {subset.indices[face], subset.indices[face+1], subset.indices[face+2]};
            else
                triangle.vertex = {subset.indices[face+1], subset.indices[face], subset.indices[face+2]};
            triangle.usable = usableTriangle(input, triangle);
            if (input.policy == TANGENT_POLICY::IMPORT)
            {
                for (size_t corner = 0; corner < 3; ++corner)
                {
                    const TANGENT &t = subset.importedCorners[face*3+corner];
                    if (!validTangent(t, input.normals[triangle.vertex[corner]]))
                        return fail("imported tangent must be finite, unit length, orthogonal and signed");
                    if (triangle.usable)
                        triangle.tangent[corner] = t;
                }
            }
            triangles.push_back(triangle);
        }
    }
    if (input.policy == TANGENT_POLICY::GENERATE)
    {
        MIKK_CONTEXT contextData = {input, triangles, {}};
        for (size_t i = 0; i < triangles.size(); ++i)
        {
            if (triangles[i].usable)
                contextData.faces.push_back(i);
        }
        if (!contextData.faces.empty())
        {
            SMikkTSpaceInterface callbacks = {};
            callbacks.m_getNumFaces = faceCount;
            callbacks.m_getNumVerticesOfFace = cornerCount;
            callbacks.m_getPosition = position;
            callbacks.m_getNormal = normal;
            callbacks.m_getTexCoord = texcoord;
            callbacks.m_setTSpaceBasic = tangent;
            SMikkTSpaceContext context = {&callbacks, &contextData};
            if (!genTangSpaceDefault(&context))
                return fail("MikkTSpace generation failed");
        }
    }
    std::unordered_map<KEY, uint16_t, KEY_HASH> vertices;
    for (TRIANGLE &triangle : triangles)
    {
        for (size_t corner = 0; corner < 3 && triangle.usable; ++corner)
            triangle.usable = validTangent(triangle.tangent[corner], input.normals[triangle.vertex[corner]]);
        if (!triangle.usable)
        {
            triangle.tangent = {};
            ++prepared.unusableTriangles;
        }
        std::array<KEY, 3> keys;
        for (size_t corner = 0; corner < 3; ++corner)
            keys[corner] = keyFor(triangle.vertex[corner], triangle.tangent[corner]);
        const bool newSubset = prepared.batches.empty() || prepared.batches.back().subset != triangle.subset;
        size_t newVertices = 0;
        for (size_t corner = 0; corner < 3; ++corner)
        {
            const bool repeated = (corner > 0 && keys[corner] == keys[0]) ||
                                  (corner > 1 && keys[corner] == keys[1]);
            if (!repeated && (newSubset || vertices.find(keys[corner]) == vertices.end()))
                ++newVertices;
        }
        if (newSubset || prepared.batches.back().sourceVertices.size() + newVertices > 65536u)
        {
            prepared.batches.emplace_back();
            prepared.batches.back().subset = triangle.subset;
            vertices.clear();
        }
        BATCH &batch = prepared.batches.back();
        for (size_t corner = 0; corner < 3; ++corner)
        {
            const auto found = vertices.find(keys[corner]);
            if (found != vertices.end())
                batch.indices.push_back(found->second);
            else
            {
                const uint16_t index = static_cast<uint16_t>(batch.sourceVertices.size());
                vertices.emplace(keys[corner], index);
                batch.sourceVertices.push_back(triangle.vertex[corner]);
                batch.tangents.push_back(triangle.tangent[corner]);
                batch.indices.push_back(index);
            }
        }
    }
    output = std::move(prepared);
    return true;
}
}}
