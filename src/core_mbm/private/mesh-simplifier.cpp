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
#include <utility>
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

        struct TRIANGLE { uint32_t a, b, c, group; };
        struct EDGE_INFO { uint32_t count = 0; };
        struct CELL_KEY
        {
            int x, y, z;
            bool operator==(const CELL_KEY &other) const
            { return x == other.x && y == other.y && z == other.z; }
        };
        struct CELL_KEY_HASH
        {
            size_t operator()(const CELL_KEY &key) const
            {
                size_t value = static_cast<size_t>(key.x) * 73856093u;
                value ^= static_cast<size_t>(key.y) * 19349663u;
                value ^= static_cast<size_t>(key.z) * 83492791u;
                return value;
            }
        };
        struct CANDIDATE
        {
            uint32_t a, b;
            VEC3 position;
            float interpolation;
            double cost;
            double geometricCost;
            double poseCost;
            double detailCost;
            uint32_t removedTriangles;
        };

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

        double pointTriangleDistanceSquared(const VEC3 &point, const VEC3 &a,
                                            const VEC3 &b, const VEC3 &c)
        {
            const VEC3 ab = b - a, ac = c - a, ap = point - a;
            const double d1 = dot(ab, ap), d2 = dot(ac, ap);
            if (d1 <= 0.0 && d2 <= 0.0) return lengthSquared(ap);
            const VEC3 bp = point - b;
            const double d3 = dot(ab, bp), d4 = dot(ac, bp);
            if (d3 >= 0.0 && d4 <= d3) return lengthSquared(bp);
            const double vc = d1 * d4 - d3 * d2;
            if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0)
            {
                const double v = d1 / (d1 - d3);
                return lengthSquared(point - (a + ab * static_cast<float>(v)));
            }
            const VEC3 cp = point - c;
            const double d5 = dot(ab, cp), d6 = dot(ac, cp);
            if (d6 >= 0.0 && d5 <= d6) return lengthSquared(cp);
            const double vb = d5 * d2 - d1 * d6;
            if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0)
            {
                const double w = d2 / (d2 - d6);
                return lengthSquared(point - (a + ac * static_cast<float>(w)));
            }
            const double va = d3 * d6 - d5 * d4;
            if (va <= 0.0 && d4 - d3 >= 0.0 && d5 - d6 >= 0.0)
            {
                const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
                return lengthSquared(point - (b + (c - b) * static_cast<float>(w)));
            }
            const double denominator = 1.0 / (va + vb + vc);
            const double v = vb * denominator, w = vc * denominator;
            return lengthSquared(point - (a + ab * static_cast<float>(v) +
                                           ac * static_cast<float>(w)));
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

        bool preservesTopology(const uint32_t a, const uint32_t b, const uint32_t edgeTriangleCount,
                               const std::vector<TRIANGLE> &triangles,
                               const std::vector<std::vector<uint32_t>> &adjacent)
        {
            std::unordered_set<uint32_t> neighborsA;
            std::unordered_set<uint32_t> neighborsB;
            auto collect = [&triangles](const uint32_t vertex, const uint32_t other,
                                        const std::vector<uint32_t> &triangleIndices,
                                        std::unordered_set<uint32_t> &out)
            {
                for (const uint32_t triangleIndex : triangleIndices)
                {
                    const TRIANGLE &triangle = triangles[triangleIndex];
                    for (const uint32_t item : {triangle.a, triangle.b, triangle.c})
                        if (item != vertex && item != other) out.insert(item);
                }
            };
            collect(a, b, adjacent[a], neighborsA);
            collect(b, a, adjacent[b], neighborsB);
            uint32_t sharedNeighborCount = 0;
            for (const uint32_t neighbor : neighborsA)
                if (neighborsB.find(neighbor) != neighborsB.end()) ++sharedNeighborCount;
            return sharedNeighborCount == edgeTriangleCount;
        }
    }

    bool simplify(const INPUT &input, const uint32_t targetTriangleCount, OUTPUT &output,
                  std::string &errorOut, const std::function<void(float)> &onProgress)
    {
        output = {};
        if (input.positions.empty() || input.indices.empty() || input.indices.size() % 3 != 0)
        { errorOut = "indexed triangle input is empty or malformed"; return false; }
        if ((!input.normals.empty() && input.normals.size() != input.positions.size()) ||
            (!input.uvs.empty() && input.uvs.size() != input.positions.size()))
        { errorOut = "vertex attribute counts do not match positions"; return false; }
        if (!input.triangleGroups.empty() && input.triangleGroups.size() != input.indices.size() / 3)
        { errorOut = "triangle-group count does not match triangles"; return false; }
        for (const std::vector<VEC3> &sample : input.deformationDeltas)
            if (sample.size() != input.positions.size())
            { errorOut = "pose-sample vertex count does not match positions"; return false; }
        if (!std::isfinite(input.boundaryCollapseThreshold) ||
            input.boundaryCollapseThreshold < 0.0f || input.boundaryCollapseThreshold > 1.0f)
        { errorOut = "boundary collapse threshold must be finite and between zero and one"; return false; }

        std::vector<VEC3> positions = input.positions;
        std::vector<VEC3> normals = input.normals;
        std::vector<VEC2> uvs = input.uvs;
        std::vector<std::vector<VEC3>> deformationDeltas = input.deformationDeltas;
        std::vector<std::vector<std::pair<uint32_t, float>>> contributions(positions.size());
        for (uint32_t i = 0; i < contributions.size(); ++i)
            contributions[i].push_back({i, 1.0f});
        std::vector<TRIANGLE> triangles;
        triangles.reserve(input.indices.size() / 3);
        for (size_t i = 0; i < input.indices.size(); i += 3)
        {
            if (input.indices[i] >= positions.size() || input.indices[i+1] >= positions.size() ||
                input.indices[i+2] >= positions.size())
            { errorOut = "triangle index is out of range"; return false; }
            const size_t triangleIndex = i / 3;
            triangles.push_back({input.indices[i], input.indices[i+1], input.indices[i+2],
                input.triangleGroups.empty() ? 0u : input.triangleGroups[triangleIndex]});
        }
        if (targetTriangleCount == 0 || targetTriangleCount >= triangles.size())
        { errorOut = "target triangle count must be smaller than the source count"; return false; }

        const size_t sourceTriangleCount = triangles.size();
        if (onProgress) onProgress(0.0f);
        double maximumCost = 0.0;
        double maximumPoseCost = 0.0;
        VEC3 minimum = positions.front();
        VEC3 maximum = positions.front();
        for (const VEC3 &position : positions)
        {
            minimum.x = std::min(minimum.x, position.x);
            minimum.y = std::min(minimum.y, position.y);
            minimum.z = std::min(minimum.z, position.z);
            maximum.x = std::max(maximum.x, position.x);
            maximum.y = std::max(maximum.y, position.y);
            maximum.z = std::max(maximum.z, position.z);
        }
        const double sourceDiagonal = std::sqrt(lengthSquared(maximum - minimum));
        while (triangles.size() > targetTriangleCount)
        {
            if (onProgress)
            {
                const size_t requestedRemoval = sourceTriangleCount - targetTriangleCount;
                const size_t removed = sourceTriangleCount - triangles.size();
                onProgress(requestedRemoval > 0
                    ? static_cast<float>(removed) / static_cast<float>(requestedRemoval) : 1.0f);
            }
            std::vector<QUADRIC> quadrics(positions.size());
            std::vector<std::vector<uint32_t>> adjacent(positions.size());
            std::vector<VEC3> triangleNormals(triangles.size());
            std::unordered_map<uint64_t, EDGE_INFO> edges;
            std::unordered_map<CELL_KEY, std::vector<uint32_t>, CELL_KEY_HASH> triangleGrid;
            edges.reserve(triangles.size() * 2);
            for (uint32_t i = 0; i < triangles.size(); ++i)
            {
                const TRIANGLE &triangle = triangles[i];
                const VEC3 normal = normalized(cross(positions[triangle.b] - positions[triangle.a],
                                                     positions[triangle.c] - positions[triangle.a]));
                triangleNormals[i] = normal;
                if (lengthSquared(normal) <= 1.0e-20) continue;
                const double d = -dot(normal, positions[triangle.a]);
                for (const uint32_t vertex : {triangle.a, triangle.b, triangle.c})
                { quadrics[vertex].addPlane(normal.x, normal.y, normal.z, d); adjacent[vertex].push_back(i); }
                for (const uint64_t key : {edgeKey(triangle.a,triangle.b), edgeKey(triangle.b,triangle.c), edgeKey(triangle.c,triangle.a)})
                    ++edges[key].count;
            }
            const double cellSize = std::max(sourceDiagonal / 96.0, 1.0e-6);
            auto cellCoordinate = [cellSize](const float value)
            { return static_cast<int>(std::floor(static_cast<double>(value) / cellSize)); };
            for (uint32_t i = 0; i < triangles.size(); ++i)
            {
                const TRIANGLE &triangle = triangles[i];
                const VEC3 &a = positions[triangle.a], &b = positions[triangle.b], &c = positions[triangle.c];
                const int minX = cellCoordinate(std::min(a.x, std::min(b.x, c.x)));
                const int minY = cellCoordinate(std::min(a.y, std::min(b.y, c.y)));
                const int minZ = cellCoordinate(std::min(a.z, std::min(b.z, c.z)));
                const int maxX = cellCoordinate(std::max(a.x, std::max(b.x, c.x)));
                const int maxY = cellCoordinate(std::max(a.y, std::max(b.y, c.y)));
                const int maxZ = cellCoordinate(std::max(a.z, std::max(b.z, c.z)));
                for (int x = minX; x <= maxX; ++x)
                    for (int y = minY; y <= maxY; ++y)
                        for (int z = minZ; z <= maxZ; ++z)
                            triangleGrid[{x, y, z}].push_back(i);
            }

            std::vector<bool> boundary(positions.size(), false);
            std::vector<bool> irregularBoundary(positions.size(), false);
            for (const auto &entry : edges)
                if (entry.second.count != 2)
                {
                    const uint32_t a = static_cast<uint32_t>(entry.first >> 32u);
                    const uint32_t b = static_cast<uint32_t>(entry.first);
                    boundary[a] = boundary[b] = true;
                    if (entry.second.count != 1) irregularBoundary[a] = irregularBoundary[b] = true;
                }

            std::vector<CANDIDATE> candidates;
            candidates.reserve(edges.size());
            for (const auto &entry : edges)
            {
                const uint32_t a = static_cast<uint32_t>(entry.first >> 32u);
                const uint32_t b = static_cast<uint32_t>(entry.first);
                const bool touchesBoundary = boundary[a] || boundary[b];
                const double boundaryEdgeLimit = sourceDiagonal * input.boundaryCollapseThreshold;
                const bool collapsibleBoundary = input.boundaryCollapseThreshold > 0.0f &&
                    boundary[a] && boundary[b] && !irregularBoundary[a] && !irregularBoundary[b] &&
                    entry.second.count == 1 &&
                    lengthSquared(positions[b] - positions[a]) <= boundaryEdgeLimit * boundaryEdgeLimit;
                if (touchesBoundary && !collapsibleBoundary)
                { ++output.boundaryRejectedCollapseCount; continue; }
                if (!preservesTopology(a, b, entry.second.count, triangles, adjacent))
                { ++output.topologyRejectedCollapseCount; continue; }
                QUADRIC combined = quadrics[a]; combined += quadrics[b];
                VEC3 point;
                float interpolation = 0.5f;
                if (solveOptimal(combined, point))
                {
                    const VEC3 edge = positions[b] - positions[a];
                    const double edgeLengthSquared = lengthSquared(edge);
                    if (edgeLengthSquared > 1.0e-20)
                    {
                        const double projected = dot(point - positions[a], edge) / edgeLengthSquared;
                        interpolation = static_cast<float>(std::max(0.0, std::min(1.0, projected)));
                        point = positions[a] + edge * interpolation;
                    }
                    else
                    {
                        interpolation = 0.0f;
                        point = positions[a];
                    }
                }
                else
                    point = (positions[a] + positions[b]) * 0.5f;
                double poseCost = 0.0;
                for (const std::vector<VEC3> &sample : deformationDeltas)
                    poseCost = std::max(poseCost, lengthSquared(sample[a] - sample[b]));
                const double geometricCost = combined.evaluate(point);
                double detailCost = 0.0;
                if (input.preserveDetails)
                {
                    double maximumNormalVariation = 0.0;
                    for (const uint32_t triangleA : adjacent[a])
                        for (const uint32_t triangleB : adjacent[b])
                        {
                            const double normalDot = std::max(-1.0, std::min(1.0,
                                dot(triangleNormals[triangleA], triangleNormals[triangleB])));
                            maximumNormalVariation = std::max(maximumNormalVariation,
                                (1.0 - normalDot) * 0.5);
                        }
                    detailCost = sourceDiagonal * sourceDiagonal * maximumNormalVariation *
                                 maximumNormalVariation * 1.0e-4;
                    if (detailCost > 1.0e-20) ++output.detailPenalizedCandidateCount;
                }
                CANDIDATE candidate{a, b, point, interpolation,
                                    geometricCost + poseCost + detailCost,
                                    geometricCost, poseCost, detailCost, entry.second.count};
                if (!std::isfinite(candidate.cost))
                { ++output.invalidRejectedCollapseCount; continue; }
                if (!preservesOrientation(candidate, positions, triangles, adjacent))
                { ++output.orientationRejectedCollapseCount; continue; }
                candidates.push_back(candidate);
            }
            std::sort(candidates.begin(), candidates.end(), [](const CANDIDATE &a, const CANDIDATE &b)
            { return a.cost < b.cost || (a.cost == b.cost && edgeKey(a.a,a.b) < edgeKey(b.a,b.b)); });

            const size_t needed = triangles.size() - targetTriangleCount;
            size_t predicted = 0;
            std::vector<bool> used(positions.size(), false);
            std::vector<bool> usedTriangles(triangles.size(), false);
            std::vector<CANDIDATE> selected;
            auto preservesClearance = [&](const CANDIDATE &candidate)
            {
                const uint32_t groupA = triangles[adjacent[candidate.a].front()].group;
                const uint32_t groupB = triangles[adjacent[candidate.b].front()].group;
                const CELL_KEY pointCell{cellCoordinate(candidate.position.x),
                                         cellCoordinate(candidate.position.y),
                                         cellCoordinate(candidate.position.z)};
                std::unordered_set<uint32_t> inspected;
                for (int x = pointCell.x - 1; x <= pointCell.x + 1; ++x)
                    for (int y = pointCell.y - 1; y <= pointCell.y + 1; ++y)
                        for (int z = pointCell.z - 1; z <= pointCell.z + 1; ++z)
                        {
                            const auto found = triangleGrid.find({x, y, z});
                            if (found == triangleGrid.end()) continue;
                            for (const uint32_t triangleIndex : found->second)
                            {
                                if (!inspected.insert(triangleIndex).second) continue;
                                const TRIANGLE &other = triangles[triangleIndex];
                                if (other.group == groupA || other.group == groupB) continue;
                                auto violatesSample = [&](const std::vector<VEC3> *sample)
                                {
                                    auto displaced = [&](const uint32_t vertex)
                                    { return sample ? positions[vertex] + (*sample)[vertex] : positions[vertex]; };
                                    const VEC3 otherA = displaced(other.a), otherB = displaced(other.b),
                                               otherC = displaced(other.c);
                                    const VEC3 sourceA = displaced(candidate.a);
                                    const VEC3 sourceB = displaced(candidate.b);
                                    const VEC3 collapsed = sourceA * (1.0f - candidate.interpolation) +
                                                           sourceB * candidate.interpolation;
                                    const double sourceDistance = std::min(
                                        pointTriangleDistanceSquared(sourceA, otherA, otherB, otherC),
                                        pointTriangleDistanceSquared(sourceB, otherA, otherB, otherC));
                                    return sourceDistance > 1.0e-16 &&
                                        pointTriangleDistanceSquared(collapsed, otherA, otherB, otherC) <
                                        sourceDistance * 0.5625;
                                };
                                if (violatesSample(nullptr)) return false;
                                for (const std::vector<VEC3> &sample : deformationDeltas)
                                    if (violatesSample(&sample)) return false;
                            }
                        }
                return true;
            };
            for (const CANDIDATE &candidate : candidates)
            {
                if (used[candidate.a] || used[candidate.b]) continue;
                bool overlapsSelectedTriangle = false;
                for (const uint32_t vertex : {candidate.a, candidate.b})
                    for (const uint32_t triangleIndex : adjacent[vertex])
                        if (usedTriangles[triangleIndex])
                        {
                            overlapsSelectedTriangle = true;
                            break;
                        }
                if (overlapsSelectedTriangle) continue;
                if (candidate.removedTriangles >= triangles.size()) continue;
                if (!selected.empty() && predicted + candidate.removedTriangles > needed) continue;
                if (!preservesClearance(candidate))
                { ++output.clearanceRejectedCollapseCount; continue; }
                used[candidate.a] = used[candidate.b] = true;
                for (const uint32_t vertex : {candidate.a, candidate.b})
                    for (const uint32_t triangleIndex : adjacent[vertex])
                        usedTriangles[triangleIndex] = true;
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
                ++output.collapseCount;
                if (boundary[candidate.a] || boundary[candidate.b]) ++output.boundaryCollapseCount;
                if (candidate.detailCost > 1.0e-20) ++output.detailPenalizedCollapseCount;
                replacement[candidate.b] = candidate.a;
                positions[candidate.a] = candidate.position;
                const float aWeight = 1.0f - candidate.interpolation;
                const float bWeight = candidate.interpolation;
                if (!normals.empty())
                    normals[candidate.a] = normalized(normals[candidate.a] * aWeight +
                                                      normals[candidate.b] * bWeight);
                if (!uvs.empty())
                    uvs[candidate.a] = uvs[candidate.a] * aWeight + uvs[candidate.b] * bWeight;
                for (std::vector<VEC3> &sample : deformationDeltas)
                    sample[candidate.a] = sample[candidate.a] * aWeight + sample[candidate.b] * bWeight;
                for (auto &entry : contributions[candidate.a]) entry.second *= aWeight;
                for (auto entry : contributions[candidate.b])
                {
                    entry.second *= bWeight;
                    contributions[candidate.a].push_back(entry);
                }
                maximumCost = std::max(maximumCost, std::max(0.0, candidate.geometricCost));
                maximumPoseCost = std::max(maximumPoseCost, std::max(0.0, candidate.poseCost));
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
        output.deformationDeltas.resize(deformationDeltas.size());
        std::unordered_map<uint64_t, uint32_t> finalEdgeCounts;
        finalEdgeCounts.reserve(triangles.size() * 2);
        for (const TRIANGLE &triangle : triangles)
            for (const uint64_t key : {edgeKey(triangle.a, triangle.b), edgeKey(triangle.b, triangle.c),
                                       edgeKey(triangle.c, triangle.a)})
                if (++finalEdgeCounts[key] > 2)
                {
                    ++output.nonManifoldEdgeCount;
                    errorOut = "simplified topology contains a non-manifold edge";
                    output = {};
                    return false;
                }
        std::vector<std::vector<uint32_t>> finalAdjacentVertices(positions.size());
        std::vector<bool> finalUsedVertices(positions.size(), false);
        for (const TRIANGLE &triangle : triangles)
        {
            if (triangle.a == triangle.b || triangle.b == triangle.c || triangle.c == triangle.a)
            { ++output.degenerateTriangleCount; continue; }
            finalUsedVertices[triangle.a] = finalUsedVertices[triangle.b] = finalUsedVertices[triangle.c] = true;
            finalAdjacentVertices[triangle.a].push_back(triangle.b);
            finalAdjacentVertices[triangle.a].push_back(triangle.c);
            finalAdjacentVertices[triangle.b].push_back(triangle.a);
            finalAdjacentVertices[triangle.b].push_back(triangle.c);
            finalAdjacentVertices[triangle.c].push_back(triangle.a);
            finalAdjacentVertices[triangle.c].push_back(triangle.b);
        }
        std::vector<bool> visited(positions.size(), false);
        std::vector<uint32_t> pending;
        for (uint32_t start = 0; start < positions.size(); ++start)
        {
            if (!finalUsedVertices[start] || visited[start]) continue;
            ++output.connectedComponentCount;
            pending.push_back(start);
            visited[start] = true;
            while (!pending.empty())
            {
                const uint32_t vertex = pending.back();
                pending.pop_back();
                for (const uint32_t neighbor : finalAdjacentVertices[vertex])
                    if (!visited[neighbor])
                    { visited[neighbor] = true; pending.push_back(neighbor); }
            }
        }
        for (const TRIANGLE &triangle : triangles)
            for (const uint32_t vertex : {triangle.a, triangle.b, triangle.c})
                if (compact[vertex] == UINT32_MAX)
                {
                    compact[vertex] = static_cast<uint32_t>(output.positions.size());
                    output.positions.push_back(positions[vertex]);
                    if (!normals.empty()) output.normals.push_back(normals[vertex]);
                    if (!uvs.empty()) output.uvs.push_back(uvs[vertex]);
                    output.sourceContributions.push_back(contributions[vertex]);
                    for (size_t sampleIndex = 0; sampleIndex < deformationDeltas.size(); ++sampleIndex)
                        output.deformationDeltas[sampleIndex].push_back(
                            deformationDeltas[sampleIndex][vertex]);
                }
        output.indices.reserve(triangles.size() * 3);
        output.triangleGroups.reserve(triangles.size());
        for (const TRIANGLE &triangle : triangles)
        {
            output.indices.push_back(compact[triangle.a]);
            output.indices.push_back(compact[triangle.b]);
            output.indices.push_back(compact[triangle.c]);
            output.triangleGroups.push_back(triangle.group);
        }
        output.maximumError = static_cast<float>(std::sqrt(maximumCost));
        output.maximumPoseError = static_cast<float>(std::sqrt(maximumPoseCost));
        const double maximumAbsoluteError = std::max(std::sqrt(maximumCost), std::sqrt(maximumPoseCost));
        output.maximumRelativeError = sourceDiagonal > 1.0e-20
            ? static_cast<float>(maximumAbsoluteError / sourceDiagonal) : 0.0f;
        if (onProgress) onProgress(1.0f);
        return true;
    }
}
