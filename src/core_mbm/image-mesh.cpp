/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include <core_mbm/image-mesh.h>
#include "private/image-mesh-topology.h"
#include <core_mbm/mesh-manager.h>
#include <core_mbm/draw-compatibility.h>
#include <core_mbm/util-interface.h>
#include <stb/stb-interface.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <vector>

namespace mbm
{
    namespace
    {
        struct VERTEX
        {
            VEC3 position, normal;
            VEC2 uv;
        };

        bool fail(char *out, int size, const char *message)
        {
            if (out && size > 0) std::snprintf(out, static_cast<size_t>(size), "%s", message);
            return false;
        }
    }

    bool generateImageMesh(const char *imagePath, const IMAGE_MESH_OPTIONS &o,
                           MESH_MBM_DEBUG &destination, IMAGE_MESH_REPORT &report,
                           char *errorOut, int errorOutLen)
    {
        report = IMAGE_MESH_REPORT{};
        if (errorOut && errorOutLen > 0) errorOut[0] = 0;
        if (!imagePath || !*imagePath || destination.getTotalFrames() != 0)
            return fail(errorOut, errorOutLen, "Image path required; destination must be empty");
        const auto dimension = [](float v) { return std::isfinite(v) && v >= 0.001f && v <= 1000000.0f; };
        if (!dimension(o.width) || !dimension(o.height) || !dimension(o.depth) ||
            !std::isfinite(o.relief) || o.relief < 0.0f || o.relief > 1000000.0f ||
            !std::isfinite(o.borderWidth) || o.borderWidth < 0.0f || o.borderWidth > 0.5f ||
            o.columns == 0 || o.rows == 0 || o.columns > 255 || o.rows > 255)
            return fail(errorOut, errorOutLen, "Invalid dimensions, relief, border width or grid (1..255 cells per axis)");
        try
        {
            image_mesh::TOPOLOGY topology;
            std::string topologyError;
            if (!image_mesh::buildTopology(o, topology, topologyError))
                return fail(errorOut, errorOutLen, topologyError.c_str());
            const uint32_t gridSize = static_cast<uint32_t>(topology.points.size());
            const uint32_t vertexCount = 2 * gridSize + 4 * static_cast<uint32_t>(topology.boundary.size());
            const uint32_t triangleCount = 2 * static_cast<uint32_t>(topology.triangles.size() + topology.boundary.size());
            bool exists = false;
            const char *resolved = util::getFullPath(imagePath, &exists);
            const std::string path = exists && resolved ? resolved : imagePath;
            int iw = 0, ih = 0, channels = 0;
            if (!stbi_info(path.c_str(), &iw, &ih, &channels) || iw <= 0 || ih <= 0 ||
                static_cast<uint64_t>(iw) * static_cast<uint64_t>(ih) > 16777216)
                return fail(errorOut, errorOutLen, "Cannot inspect image or image exceeds 16 megapixels");
            const uint32_t imageWidth = static_cast<uint32_t>(iw), imageHeight = static_cast<uint32_t>(ih);
            if (o.x >= imageWidth || o.y >= imageHeight)
                return fail(errorOut, errorOutLen, "Crop origin outside image");
            const uint32_t cw = o.cropWidth ? o.cropWidth : imageWidth - o.x;
            const uint32_t ch = o.cropHeight ? o.cropHeight : imageHeight - o.y;
            if (cw > imageWidth - o.x || ch > imageHeight - o.y)
                return fail(errorOut, errorOutLen, "Crop outside image");
            std::unique_ptr<stbi_uc, decltype(&std::free)> pixels(
                stbi_load(path.c_str(), &iw, &ih, &channels, 4), &std::free);
            if (!pixels || static_cast<uint32_t>(iw) != imageWidth || static_cast<uint32_t>(ih) != imageHeight)
                return fail(errorOut, errorOutLen, "Cannot decode image or dimensions changed");
            const auto intensity = [&](uint32_t x, uint32_t y)
            {
                const auto *p = pixels.get() + (static_cast<size_t>(y) * imageWidth + x) * 4;
                return (0.2126f * p[0] + 0.7152f * p[1] + 0.0722f * p[2]) / 255.0f;
            };
            std::vector<VERTEX> vertices;
            std::vector<uint16_t> indices;
            vertices.reserve(vertexCount);
            indices.reserve(triangleCount * 3);
            float minHeight = o.relief, maxHeight = 0.0f;
            std::vector<bool> boundary(gridSize, false);
            for (uint32_t index : topology.boundary) boundary[index] = true;
            for (uint32_t pointIndex = 0; pointIndex < gridSize; ++pointIndex)
            {
                const auto &point = topology.points[pointIndex];
                const float u = point.x, v = point.y;
                const float px = o.x + u * (cw - 1), py = o.y + v * (ch - 1);
                const uint32_t x0 = static_cast<uint32_t>(px), y0 = static_cast<uint32_t>(py);
                const uint32_t x1 = std::min(x0 + 1, o.x + cw - 1), y1 = std::min(y0 + 1, o.y + ch - 1);
                const float fx = px - x0, fy = py - y0;
                float level = (intensity(x0, y0) * (1 - fx) + intensity(x1, y0) * fx) * (1 - fy) +
                              (intensity(x0, y1) * (1 - fx) + intensity(x1, y1) * fx) * fy;
                if (o.invert) level = 1.0f - level;
                float height = level * o.relief;
                if (o.lockBorder)
                {
                    const float distance = image_mesh::borderDistance(point, topology);
                    if (boundary[pointIndex]) height = 0;
                    else if (o.borderWidth > 0) height *= std::min(1.0f, distance / o.borderWidth);
                }
                minHeight = std::min(minHeight, height);
                maxHeight = std::max(maxHeight, height);
                vertices.push_back({VEC3((u - 0.5f) * o.width, (0.5f - v) * o.height, -o.depth * 0.5f - height),
                                    VEC3(0, 0, 0), VEC2((px + 0.5f) / imageWidth, (py + 0.5f) / imageHeight)});
            }
            for (uint32_t i = 0; i < gridSize; ++i)
            {
                VERTEX back = vertices[i];
                back.position.z = o.depth * 0.5f;
                vertices.push_back(back);
            }
            const auto triangle = [&](uint32_t a, uint32_t b, uint32_t c)
            {
                indices.push_back(static_cast<uint16_t>(a));
                indices.push_back(static_cast<uint16_t>(b));
                indices.push_back(static_cast<uint16_t>(c));
            };
            for (const auto &face : topology.triangles)
            {
                triangle(face[0], face[1], face[2]);
                triangle(face[0] + gridSize, face[2] + gridSize, face[1] + gridSize);
            }
            // Clockwise perimeter viewed from -Z. Duplicate side vertices for hard seams.
            const auto side = [&](uint32_t a, uint32_t b)
            {
                const uint32_t start = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertices[a]); vertices.push_back(vertices[b]);
                vertices.push_back(vertices[a + gridSize]); vertices.push_back(vertices[b + gridSize]);
                triangle(start, start + 2, start + 1); triangle(start + 1, start + 2, start + 3);
            };
            for (size_t i = 0; i < topology.boundary.size(); ++i)
                side(topology.boundary[i], topology.boundary[(i + 1) % topology.boundary.size()]);
            for (size_t i = 0; i < indices.size(); i += 3)
            {
                VERTEX &a = vertices[indices[i]], &b = vertices[indices[i + 1]], &c = vertices[indices[i + 2]];
                const VEC3 ab(b.position.x - a.position.x, b.position.y - a.position.y, b.position.z - a.position.z);
                const VEC3 ac(c.position.x - a.position.x, c.position.y - a.position.y, c.position.z - a.position.z);
                const VEC3 normal(ab.y * ac.z - ab.z * ac.y, ab.z * ac.x - ab.x * ac.z, ab.x * ac.y - ab.y * ac.x);
                if (normal.x == 0 && normal.y == 0 && normal.z == 0)
                    return fail(errorOut, errorOutLen, "Degenerate triangle after coordinate conversion");
                for (auto *vertex : {&a, &b, &c})
                {
                    vertex->normal.x += normal.x; vertex->normal.y += normal.y; vertex->normal.z += normal.z;
                }
            }
            destination.setMeshType(util::TYPE_MESH_3D);
            util::MATERIAL &material = destination.getMaterial();
            material.Specular = COLOR(0.0f, 0.0f, 0.0f, 1.0f);
            material.Power = 0;
            destination.setModeDraw(util::MODE_DRAW_TRIANGLES);
            destination.setModeCullFace(util::CULL_BACK);
            destination.setModeFrontFaceDirection(util::CW);
            destination.setHasNormal(HAS_NOR_IN_FILE);
            destination.setHasTexture(HAS_TEX_EACH_FRAME);
            destination.addBuffer(3);
            destination.addSubset(0);
            if (!destination.addVertex(0, 0, vertexCount))
                return fail(errorOut, errorOutLen, "Cannot allocate mesh vertices");
            VEC3 *positions = destination.getPositionArray(0);
            VEC3 *normals = destination.getNormalArray(0);
            VEC2 *uvs = destination.getUvArray(0);
            for (uint32_t i = 0; i < vertexCount; ++i)
            {
                const VERTEX &vertex = vertices[i];
                const VEC3 &n = vertex.normal;
                const double length = std::sqrt(static_cast<double>(n.x) * n.x + static_cast<double>(n.y) * n.y + static_cast<double>(n.z) * n.z);
                if (!(length > 0)) return fail(errorOut, errorOutLen, "Degenerate geometry");
                positions[i] = vertex.position;
                normals[i] = VEC3(static_cast<float>(n.x / length), static_cast<float>(n.y / length), static_cast<float>(n.z / length));
                uvs[i] = vertex.uv;
            }
            destination.getSubset(0, 0)->texture = path;
            if (!destination.addIndex(0, 0, indices.data(), static_cast<uint32_t>(indices.size()), errorOut, errorOutLen)) return false;
            if (destination.addAnimation("Static", 0, 0, 1.0f, 0, errorOut, errorOutLen) <= 0) return false;
            if (!destination.check(errorOut, errorOutLen)) return false;
            report.vertices = vertexCount; report.triangles = triangleCount;
            report.minHeight = minHeight; report.maxHeight = maxHeight;
            return true;
        }
        catch (const std::exception &e)
        {
            return fail(errorOut, errorOutLen, e.what());
        }
    }
}
