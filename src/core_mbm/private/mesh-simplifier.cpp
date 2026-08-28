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

#include "mesh-simplifier.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace mbm::mesh_simplifier
{
    namespace
    {
        struct QUADRIC
        {
            double v[10] = {};

            void addPlane(const double a, const double b, const double c, const double d)
            {
                const double p[4] = {a, b, c, d};
                static constexpr int row[10] = {0, 0, 0, 0, 1, 1, 1, 2, 2, 3};
                static constexpr int col[10] = {0, 1, 2, 3, 1, 2, 3, 2, 3, 3};
                for (int i = 0; i < 10; ++i) v[i] += p[row[i]] * p[col[i]];
            }

            QUADRIC &operator+=(const QUADRIC &other)
            {
                for (int i = 0; i < 10; ++i) v[i] += other.v[i];
                return *this;
            }

            double evaluate(const VEC3 &p) const
            {
                const double x = p.x, y = p.y, z = p.z;
                return v[0]*x*x + 2.0*v[1]*x*y + 2.0*v[2]*x*z + 2.0*v[3]*x +
                       v[4]*y*y + 2.0*v[5]*y*z + 2.0*v[6]*y + v[7]*z*z +
                       2.0*v[8]*z + v[9];
            }
        };

        struct TRIANGLE { uint32_t a, b, c; };
        struct EDGE_INFO { uint32_t count = 0; };
        struct CANDIDATE { uint32_t a, b; VEC3 position; double cost; uint32_t removedTriangles; };

        uint64_t edgeKey(uint32_t a, uint32_t b)
        {
            if (a > b) std::swap(a, b);
            return (static_cast<uint64_t>(a) << 32u) | b;
        }

        VEC3 cross(const VEC3 &a, const VEC3 &b)
        {
            return VEC3(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x);
        }

        double dot(const VEC3 &a, const VEC3 &b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
        double lengthSquared(const VEC3 &v) { return dot(v, v); }

        VEC3 normalized(const VEC3 &v)
        {
            const double length = std::sqrt(lengthSquared(v));
            return length > 1.0e-20 ? v * static_cast<float>(1.0 / length) : VEC3(0.0f, 0.0f, 0.0f);
        }

        bool solveOptimal(const QUADRIC &q, VEC3 &out)
        {
            const double a00=q.v[0], a01=q.v[1], a02=q.v[2];
            const double a10=q.v[1], a11=q.v[4], a12=q.v[5];
            const double a20=q.v[2], a21=q.v[5], a22=q.v[7];
            const double b0=-q.v[3], b1=-q.v[6], b2=-q.v[8];
            const double determinant = a00*(a11*a22-a12*a21)-a01*(a10*a22-a12*a20)+a02*(a10*a21-a11*a20);
            if (std::fabs(determinant) <= 1.0e-12) return false;
            const double x = (b0*(a11*a22-a12*a21)-a01*(b1*a22-a12*b2)+a02*(b1*a21-a11*b2))/determinant;
            const double y = (a00*(b1*a22-a12*b2)-b0*(a10*a22-a12*a20)+a02*(a10*b2-b1*a20))/determinant;
            const double z = (a00*(a11*b2-b1*a21)-a01*(a10*b2-b1*a20)+b0*(a10*a21-a11*a20))/determinant;
            if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return false;
            out = VEC3(static_cast<float>(x), static_cast<float>(y), static_cast<float>(z));
            return true;
        }

        bool preservesOrientation(const CANDIDATE &candidate, const std::vector<VEC3> &positions,
                                  const std::vector<TRIANGLE> &triangles,
                                  const std::vector<std::vector<uint32_t>> &adjacent)
        {
            std::unordered_set<uint32_t> inspected;
            for (const uint32_t vertex : {candidate.a, candidate.b})
            {
                for (const uint32_t triangleIndex : adjacent[vertex])
                {
                    if (!inspected.insert(triangleIndex).second) continue;
                    const TRIANGLE &triangle = triangles[triangleIndex];
                    uint32_t index[3] = {triangle.a, triangle.b, triangle.c};
                    for (uint32_t &item : index)
                        if (item == candidate.a || item == candidate.b) item = candidate.a;
                    if (index[0] == index[1] || index[1] == index[2] || index[2] == index[0]) continue;
                    const VEC3 oldNormal = cross(positions[triangle.b] - positions[triangle.a],
                                                 positions[triangle.c] - positions[triangle.a]);
                    VEC3 updated[3] = {positions[index[0]], positions[index[1]], positions[index[2]]};
                    for (int i = 0; i < 3; ++i)
                        if (index[i] == candidate.a) updated[i] = candidate.position;
                    const VEC3 newNormal = cross(updated[1] - updated[0], updated[2] - updated[0]);
                    const double oldLength = lengthSquared(oldNormal);
                    const double newLength = lengthSquared(newNormal);
                    if (oldLength <= 1.0e-20 || newLength <= 1.0e-20) return false;
                    if (dot(oldNormal, newNormal) <= std::sqrt(oldLength * newLength) * 0.1) return false;
                }
            }
            return true;
        }
    }

    bool simplify(const INPUT &input, const uint32_t targetTriangleCount, OUTPUT &output,
                  std::string &errorOut)
    {
        output = {};
        if (input.positions.empty() || input.indices.empty() || input.indices.size() % 3 != 0)
        { errorOut = "indexed triangle input is empty or malformed"; return false; }
        if ((!input.normals.empty() && input.normals.size() != input.positions.size()) ||
            (!input.uvs.empty() && input.uvs.size() != input.positions.size()))
        { errorOut = "vertex attribute counts do not match positions"; return false; }

        std::vector<VEC3> positions = input.positions;
        std::vector<VEC3> normals = input.normals;
        std::vector<VEC2> uvs = input.uvs;
        std::vector<TRIANGLE> triangles;
        triangles.reserve(input.indices.size() / 3);
        for (size_t i = 0; i < input.indices.size(); i += 3)
        {
            if (input.indices[i] >= positions.size() || input.indices[i+1] >= positions.size() ||
                input.indices[i+2] >= positions.size())
            { errorOut = "triangle index is out of range"; return false; }
            triangles.push_back({input.indices[i], input.indices[i+1], input.indices[i+2]});
        }
        if (targetTriangleCount == 0 || targetTriangleCount >= triangles.size())
        { errorOut = "target triangle count must be smaller than the source count"; return false; }

        double maximumCost = 0.0;
        while (triangles.size() > targetTriangleCount)
        {
            std::vector<QUADRIC> quadrics(positions.size());
            std::vector<std::vector<uint32_t>> adjacent(positions.size());
            std::unordered_map<uint64_t, EDGE_INFO> edges;
            edges.reserve(triangles.size() * 2);
            for (uint32_t i = 0; i < triangles.size(); ++i)
            {
                const TRIANGLE &triangle = triangles[i];
                const VEC3 normal = normalized(cross(positions[triangle.b] - positions[triangle.a],
                                                     positions[triangle.c] - positions[triangle.a]));
                if (lengthSquared(normal) <= 1.0e-20) continue;
                const double d = -dot(normal, positions[triangle.a]);
                for (const uint32_t vertex : {triangle.a, triangle.b, triangle.c})
                { quadrics[vertex].addPlane(normal.x, normal.y, normal.z, d); adjacent[vertex].push_back(i); }
                for (const uint64_t key : {edgeKey(triangle.a,triangle.b), edgeKey(triangle.b,triangle.c), edgeKey(triangle.c,triangle.a)})
                    ++edges[key].count;
            }

            std::vector<bool> boundary(positions.size(), false);
            for (const auto &entry : edges)
                if (entry.second.count != 2)
                { boundary[entry.first >> 32u] = true; boundary[static_cast<uint32_t>(entry.first)] = true; }

            std::vector<CANDIDATE> candidates;
            candidates.reserve(edges.size());
            for (const auto &entry : edges)
            {
                const uint32_t a = static_cast<uint32_t>(entry.first >> 32u);
                const uint32_t b = static_cast<uint32_t>(entry.first);
                if (boundary[a] != boundary[b] || (boundary[a] && entry.second.count != 1)) continue;
                QUADRIC combined = quadrics[a]; combined += quadrics[b];
                VEC3 point;
                if (!solveOptimal(combined, point)) point = (positions[a] + positions[b]) * 0.5f;
                CANDIDATE candidate{a, b, point, combined.evaluate(point), entry.second.count};
                if (std::isfinite(candidate.cost) && preservesOrientation(candidate, positions, triangles, adjacent))
                    candidates.push_back(candidate);
            }
            std::sort(candidates.begin(), candidates.end(), [](const CANDIDATE &a, const CANDIDATE &b)
            { return a.cost < b.cost || (a.cost == b.cost && edgeKey(a.a,a.b) < edgeKey(b.a,b.b)); });

            const size_t needed = triangles.size() - targetTriangleCount;
            size_t predicted = 0;
            std::vector<bool> used(positions.size(), false);
            std::vector<CANDIDATE> selected;
            for (const CANDIDATE &candidate : candidates)
            {
                if (used[candidate.a] || used[candidate.b]) continue;
                if (candidate.removedTriangles >= triangles.size()) continue;
                if (!selected.empty() && predicted + candidate.removedTriangles > needed) continue;
                used[candidate.a] = used[candidate.b] = true;
                selected.push_back(candidate);
                predicted += candidate.removedTriangles;
                if (predicted >= needed) break;
            }
            if (selected.empty())
            { errorOut = "topology constraints prevent reaching the requested triangle count"; return false; }

            std::vector<uint32_t> replacement(positions.size());
            for (uint32_t i = 0; i < replacement.size(); ++i) replacement[i] = i;
            for (const CANDIDATE &candidate : selected)
            {
                replacement[candidate.b] = candidate.a;
                positions[candidate.a] = candidate.position;
                if (!normals.empty()) normals[candidate.a] = normalized(normals[candidate.a] + normals[candidate.b]);
                if (!uvs.empty()) uvs[candidate.a] = (uvs[candidate.a] + uvs[candidate.b]) * 0.5f;
                maximumCost = std::max(maximumCost, std::max(0.0, candidate.cost));
            }
            std::vector<TRIANGLE> next;
            next.reserve(triangles.size());
            for (TRIANGLE triangle : triangles)
            {
                triangle.a = replacement[triangle.a]; triangle.b = replacement[triangle.b]; triangle.c = replacement[triangle.c];
                if (triangle.a != triangle.b && triangle.b != triangle.c && triangle.c != triangle.a)
                    next.push_back(triangle);
            }
            if (next.size() >= triangles.size())
            { errorOut = "edge-collapse pass made no progress"; return false; }
            triangles = std::move(next);
        }

        std::vector<uint32_t> compact(positions.size(), UINT32_MAX);
        for (const TRIANGLE &triangle : triangles)
            for (const uint32_t vertex : {triangle.a, triangle.b, triangle.c})
                if (compact[vertex] == UINT32_MAX)
                {
                    compact[vertex] = static_cast<uint32_t>(output.positions.size());
                    output.positions.push_back(positions[vertex]);
                    if (!normals.empty()) output.normals.push_back(normals[vertex]);
                    if (!uvs.empty()) output.uvs.push_back(uvs[vertex]);
                }
        output.indices.reserve(triangles.size() * 3);
        for (const TRIANGLE &triangle : triangles)
        { output.indices.push_back(compact[triangle.a]); output.indices.push_back(compact[triangle.b]); output.indices.push_back(compact[triangle.c]); }
        output.maximumError = static_cast<float>(std::sqrt(maximumCost));
        return true;
    }
}
