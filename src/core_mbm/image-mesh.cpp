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
#include "private/image-mesh-height.h"
#include "private/image-mesh-sides.h"
#include <lodepng/lodepng.h>
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
        if (static_cast<int>(o.backOpen)+o.backRelief+o.backRemap+o.backSolid+o.backExternal>1)
            return fail(errorOut,errorOutLen,"Back modes are mutually exclusive: open, copied relief, flat remap, solid color or external texture");
        if (o.sideMode!=IMAGE_MESH_SIDE::EDGE && o.sideMode!=IMAGE_MESH_SIDE::COLOR &&
            o.sideMode!=IMAGE_MESH_SIDE::REPEAT && o.sideMode!=IMAGE_MESH_SIDE::BAND)
            return fail(errorOut,errorOutLen,"Invalid side texture mode");
        if (!std::isfinite(o.sideRepeatU) || !std::isfinite(o.sideRepeatV) || o.sideRepeatU<0.1f ||
            o.sideRepeatV<0.1f || o.sideRepeatU>64 || o.sideRepeatV>64 || o.sideColor>0xffffff)
            return fail(errorOut,errorOutLen,"Side repeats must be 0.1..64; color must be RGB 0..0xFFFFFF");
        if (o.backColor>0xffffff) return fail(errorOut,errorOutLen,"Back color must be RGB 0..0xFFFFFF");
        try
        {
            image_mesh::HEIGHT_FIELD field;
            std::string topologyError;
            if (!field.load(imagePath,o,topologyError)) return fail(errorOut,errorOutLen,topologyError.c_str());
            const uint32_t backWidth=o.backCropWidth?o.backCropWidth:field.width;
            const uint32_t backHeight=o.backCropHeight?o.backCropHeight:field.height;
            if (o.backRemap && (o.backX>=field.imageWidth || o.backY>=field.imageHeight ||
                backWidth>field.imageWidth-o.backX || backHeight>field.imageHeight-o.backY))
                return fail(errorOut,errorOutLen,"Back UV crop is outside the source image");
            image_mesh::TOPOLOGY topology;
            if (!image_mesh::buildTopology(o,topology,topologyError,o.followImage?&field:nullptr))
                return fail(errorOut,errorOutLen,topologyError.c_str());
            std::vector<IMAGE_MESH_POINT> sideInner;
            const bool separateBack=o.backSolid || o.backExternal;
            std::string backTexture=field.path;
            int backImageWidth=0,backImageHeight=0;
            const bool externalBackImage=o.backExternal && o.backTexture && *o.backTexture;
            if (externalBackImage)
            {
                bool exists=false;
                const char *resolved=util::getFullPath(o.backTexture,&exists);
                int channels=0;
                if (!exists || !resolved || !stbi_info(resolved,&backImageWidth,&backImageHeight,&channels) ||
                    backImageWidth<1 || backImageHeight<1 || static_cast<uint64_t>(backImageWidth)*backImageHeight>16777216)
                    return fail(errorOut,errorOutLen,"Back texture is missing or invalid (maximum 16 million pixels)");
                backTexture=resolved;
                std::unique_ptr<stbi_uc,decltype(&std::free)> decoded(stbi_load(backTexture.c_str(),&backImageWidth,&backImageHeight,&channels,4),&std::free);
                if (!decoded) return fail(errorOut,errorOutLen,"Cannot decode back texture");
            }
            std::string sideTexture=field.path;
            const bool separateSides=separateBack || o.sideMode==IMAGE_MESH_SIDE::COLOR || o.sideMode==IMAGE_MESH_SIDE::REPEAT;
            const bool repeatSourceCrop=o.sideMode==IMAGE_MESH_SIDE::REPEAT && (!o.sideTexture || !*o.sideTexture);
            uint32_t sideRows=1;
            if (o.sideMode==IMAGE_MESH_SIDE::BAND)
            {
                IMAGE_MESH_OPTIONS crop=o; crop.cropWidth=field.width; crop.cropHeight=field.height;
                float maximum=0;
                if (!image_mesh::sideContour(crop,topology.contour,sideInner,maximum,topologyError))
                    return fail(errorOut,errorOutLen,topologyError.c_str());
            }
            else if (o.sideMode==IMAGE_MESH_SIDE::COLOR)
            {
                char color[10]; std::snprintf(color,sizeof(color),"#%06XFF",o.sideColor);
                sideTexture=color;
            }
            else if (o.sideMode==IMAGE_MESH_SIDE::REPEAT)
            {
                if (repeatSourceCrop) sideTexture=field.path;
                else
                {
                    bool exists=false;
                    const char *resolved=o.sideTexture?util::getFullPath(o.sideTexture,&exists):nullptr;
                    int w=0,h=0,channels=0;
                    if (!exists || !resolved || !stbi_info(resolved,&w,&h,&channels) || w<1 || h<1 ||
                        static_cast<uint64_t>(w)*h>16777216)
                        return fail(errorOut,errorOutLen,"Side texture is missing or invalid (maximum 16 million pixels)");
                    sideTexture=resolved;
                    std::unique_ptr<stbi_uc,decltype(&std::free)> decoded(stbi_load(sideTexture.c_str(),&w,&h,&channels,4),&std::free);
                    if (!decoded) return fail(errorOut,errorOutLen,"Cannot decode side texture");
                }
                image_mesh::sideSplitRepeats(o,topology);
                sideRows=static_cast<uint32_t>(std::ceil(o.sideRepeatV));
            }
            const uint32_t gridSize=static_cast<uint32_t>(topology.points.size());
            const bool compactBack=o.followImage && !o.backRelief;
            uint32_t backSize=gridSize;
            size_t backTriangles=topology.triangles.size();
            if (o.backOpen) { backSize=0; backTriangles=0; }
            else if (compactBack)
            {
                backSize=static_cast<uint32_t>(topology.boundary.size());
                backTriangles=topology.backTriangles.size();
            }
            const uint32_t vertexCount=gridSize+backSize+4*sideRows*static_cast<uint32_t>(topology.boundary.size());
            const uint32_t triangleCount=static_cast<uint32_t>(topology.triangles.size()+backTriangles+2*sideRows*topology.boundary.size());
            if (vertexCount>std::min(o.maxVertices,65535u) || triangleCount>o.maxTriangles)
            {
                const auto message="Geometry budget exceeded including side texture seams: vertices "+std::to_string(vertexCount)+
                    ", limit "+std::to_string(std::min(o.maxVertices,65535u))+"; triangles "+std::to_string(triangleCount)+
                    ", limit "+std::to_string(o.maxTriangles)+". Reduce side repeats or geometry resolution.";
                return fail(errorOut,errorOutLen,message.c_str());
            }
            const auto &path=field.path;
            const uint32_t imageWidth=field.imageWidth,imageHeight=field.imageHeight,cw=field.width,ch=field.height;
            const auto &pixels=field.pixels;
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
                const float level=field.surface(u,v,o);
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
            std::vector<uint32_t> backIndex(gridSize);
            for (uint32_t i=0;i<backSize;++i)
            {
                const uint32_t source=compactBack?topology.boundary[i]:i;
                backIndex[source]=gridSize+i;
                VERTEX back = vertices[source];
                back.position.z = o.backRelief ? -back.position.z : o.depth * 0.5f;
                const float u=o.backMirror?1.0f-topology.points[source].x:topology.points[source].x;
                if (o.backRemap)
                    back.uv=VEC2((o.backX+u*(backWidth-1)+0.5f)/imageWidth,
                                  (o.backY+topology.points[source].y*(backHeight-1)+0.5f)/imageHeight);
                else if (o.backMirror)
                    back.uv.x = (o.x + u*(cw-1) + 0.5f)/imageWidth;
                if (externalBackImage)
                    back.uv=VEC2((u*(backImageWidth-1)+.5f)/backImageWidth,
                        (topology.points[source].y*(backImageHeight-1)+.5f)/backImageHeight);
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
            }
            const uint32_t backIndexStart=static_cast<uint32_t>(indices.size());
            if (!o.backOpen)
                for (const auto &face : (compactBack?topology.backTriangles:topology.triangles))
                    triangle(backIndex[face[0]],backIndex[face[2]],backIndex[face[1]]);
            // Transparent atlas margins must not turn a closed solid into invisible walls.
            // Build a nearest-max-alpha lookup only if a side crosses transparent texels.
            // Two Manhattan-distance sweeps keep lookup work linear in crop pixels.
            std::vector<uint32_t> visibleTexel;
            const auto alpha = [&](uint32_t x, uint32_t y)
            {
                return pixels.get()[(static_cast<size_t>(y + o.y) * imageWidth + x + o.x) * 4 + 3];
            };
            const auto prepareVisibleTexels = [&]()
            {
                if (!visibleTexel.empty()) return;
                unsigned char maximum = 0;
                for (uint32_t y = 0; y < ch; ++y) for (uint32_t x = 0; x < cw; ++x)
                    maximum = std::max(maximum, alpha(x, y));
                const uint32_t missing = cw * ch;
                visibleTexel.assign(missing, missing);
                for (uint32_t y = 0; y < ch; ++y) for (uint32_t x = 0; x < cw; ++x)
                    if (alpha(x, y) == maximum) visibleTexel[y * cw + x] = y * cw + x;
                const auto consider = [&](uint32_t at, uint32_t neighbor)
                {
                    const uint32_t candidate = visibleTexel[neighbor], current = visibleTexel[at];
                    if (candidate == missing) return;
                    const auto distance = [&](uint32_t source)
                    {
                        return std::abs(static_cast<int>(at % cw) - static_cast<int>(source % cw)) +
                               std::abs(static_cast<int>(at / cw) - static_cast<int>(source / cw));
                    };
                    if (current == missing || distance(candidate) < distance(current)) visibleTexel[at] = candidate;
                };
                for (uint32_t i = 0; i < missing; ++i)
                {
                    if (i % cw) consider(i, i - 1);
                    if (i >= cw) consider(i, i - cw);
                }
                for (uint32_t i = missing; i-- > 0;)
                {
                    if (i % cw + 1 < cw) consider(i, i + 1);
                    if (i + cw < missing) consider(i, i + cw);
                }
            };
            const auto sideNeedsOpaqueSample = [&](const IMAGE_MESH_POINT &a, const IMAGE_MESH_POINT &b)
            {
                const float ax = a.x * (cw - 1), ay = a.y * (ch - 1);
                const float bx = b.x * (cw - 1), by = b.y * (ch - 1);
                const uint32_t steps = std::max(1u, static_cast<uint32_t>(std::ceil(std::max(std::abs(bx-ax), std::abs(by-ay)) * 2)));
                for (uint32_t i = 0; i <= steps; ++i)
                {
                    const float t = static_cast<float>(i) / steps;
                    const float x = ax + (bx-ax)*t, y = ay + (by-ay)*t;
                    const uint32_t x0 = static_cast<uint32_t>(x), y0 = static_cast<uint32_t>(y);
                    const uint32_t x1 = std::min(cw-1, static_cast<uint32_t>(std::ceil(x)));
                    const uint32_t y1 = std::min(ch-1, static_cast<uint32_t>(std::ceil(y)));
                    if (alpha(x0,y0) < 255 || alpha(x1,y0) < 255 || alpha(x0,y1) < 255 || alpha(x1,y1) < 255) return true;
                }
                return false;
            };
            const uint32_t sideVertexStart=static_cast<uint32_t>(vertices.size());
            const uint32_t sideIndexStart=static_cast<uint32_t>(indices.size());
            double sidePerimeter=0,sideWalked=0;
            if (o.sideMode==IMAGE_MESH_SIDE::REPEAT)
                for (size_t i=0;i<topology.boundary.size();++i)
                {
                    const auto &a=vertices[topology.boundary[i]].position,&b=vertices[topology.boundary[(i+1)%topology.boundary.size()]].position;
                    sidePerimeter+=std::hypot(b.x-a.x,b.y-a.y);
                }
            // Clockwise perimeter viewed from -Z. Duplicate side vertices for hard seams.
            const auto side = [&](uint32_t a, uint32_t b)
            {
                if (o.sideMode==IMAGE_MESH_SIDE::REPEAT)
                {
                    const auto &pa=vertices[a].position,&pb=vertices[b].position;
                    const double length=std::hypot(pb.x-pa.x,pb.y-pa.y);
                    const double u0=sideWalked/sidePerimeter*o.sideRepeatU,u1=(sideWalked+length)/sidePerimeter*o.sideRepeatU;
                    const double tile=std::floor((u0+u1)*.5);
                    const float ua=static_cast<float>(std::clamp(u0-tile,0.0,1.0)),ub=static_cast<float>(std::clamp(u1-tile,0.0,1.0));
                    for (uint32_t row=0;row<sideRows;++row)
                    {
                        const float va=static_cast<float>(row),vb=std::min(va+1,o.sideRepeatV);
                        const uint32_t start=static_cast<uint32_t>(vertices.size());
                        for (float v:{va,vb}) for (uint32_t point:{a,b})
                        {
                            VERTEX vertex=vertices[point];
                            const float backZ=o.backRelief?-vertex.position.z:o.depth*.5f;
                            vertex.position.z+=(backZ-vertex.position.z)*(v/o.sideRepeatV);
                            vertex.uv=VEC2(point==a?ua:ub,v-va);
                            if (repeatSourceCrop)
                            {
                                vertex.uv.x=(o.x+vertex.uv.x*(cw-1)+.5f)/imageWidth;
                                vertex.uv.y=(o.y+vertex.uv.y*(ch-1)+.5f)/imageHeight;
                            }
                            vertices.push_back(vertex);
                        }
                        triangle(start,start+2,start+1); triangle(start+1,start+2,start+3);
                    }
                    sideWalked+=length; return;
                }
                const uint32_t start = static_cast<uint32_t>(vertices.size());
                vertices.push_back(vertices[a]); vertices.push_back(vertices[b]);
                if (o.backOpen)
                {
                    VERTEX backA=vertices[a],backB=vertices[b];
                    backA.position.z=o.depth*0.5f; backB.position.z=o.depth*0.5f;
                    vertices.push_back(backA); vertices.push_back(backB);
                }
                else
                {
                    vertices.push_back(vertices[backIndex[a]]); vertices.push_back(vertices[backIndex[b]]);
                }
                // Back-only UV mirroring must not twist the stretched border strip.
                vertices[start+2].uv=vertices[a].uv; vertices[start+3].uv=vertices[b].uv;
                if (o.sideMode==IMAGE_MESH_SIDE::BAND)
                {
                    for (uint32_t k=0;k<2;++k)
                    {
                        const auto inner=image_mesh::sideMap(topology.points[k==0?a:b],topology.contour,sideInner);
                        vertices[start+2+k].uv=VEC2((o.x+inner.x*(cw-1)+.5f)/imageWidth,(o.y+inner.y*(ch-1)+.5f)/imageHeight);
                        if (o.sideBandInvert) std::swap(vertices[start+k].uv,vertices[start+2+k].uv);
                    }
                }
                else if (o.sideMode==IMAGE_MESH_SIDE::COLOR)
                    for (uint32_t k=0;k<4;++k) vertices[start+k].uv=VEC2(.5f,.5f);
                else if (sideNeedsOpaqueSample(topology.points[a], topology.points[b]))
                {
                    prepareVisibleTexels();
                    const uint32_t x = std::min(cw-1, static_cast<uint32_t>(std::lround((topology.points[a].x + topology.points[b].x) * 0.5f * (cw-1))));
                    const uint32_t y = std::min(ch-1, static_cast<uint32_t>(std::lround((topology.points[a].y + topology.points[b].y) * 0.5f * (ch-1))));
                    const uint32_t source = visibleTexel[y * cw + x];
                    const VEC2 uv((o.x + source % cw + 0.5f) / imageWidth, (o.y + source / cw + 0.5f) / imageHeight);
                    // Constant UV per fallback quad also avoids interpolating across another alpha gap.
                    for (uint32_t i = start; i < start + 4; ++i) vertices[i].uv = uv;
                }
                triangle(start, start + 2, start + 1); triangle(start + 1, start + 2, start + 3);
            };
            for (size_t i = 0; i < topology.boundary.size(); ++i)
                side(topology.boundary[i], topology.boundary[(i + 1) % topology.boundary.size()]);
            // Prefer plateau faces over steep ramps when computing shared front
            // normals. Keeping vertices welded preserves simplification behavior.
            const bool preservePlateaus=o.followImage && o.twoLevels && o.relief>0;
            std::vector<float> levels;
            std::vector<VEC3> plateauNormals;
            if (preservePlateaus)
            {
                levels.reserve(gridSize);
                for (const auto &p : topology.points) levels.push_back(field.surface(p.x,p.y,o));
                plateauNormals.resize(gridSize,VEC3(0,0,0));
            }
            for (size_t i = 0; i < indices.size(); i += 3)
            {
                VERTEX &a = vertices[indices[i]], &b = vertices[indices[i + 1]], &c = vertices[indices[i + 2]];
                // Double intermediates avoid cancellation on tiny groove triangles.
                const double abx = static_cast<double>(b.position.x) - a.position.x;
                const double aby = static_cast<double>(b.position.y) - a.position.y;
                const double abz = static_cast<double>(b.position.z) - a.position.z;
                const double acx = static_cast<double>(c.position.x) - a.position.x;
                const double acy = static_cast<double>(c.position.y) - a.position.y;
                const double acz = static_cast<double>(c.position.z) - a.position.z;
                const double nx = aby * acz - abz * acy;
                const double ny = abz * acx - abx * acz;
                const double nz = abx * acy - aby * acx;
                const double faceLength = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (!(faceLength > 0))
                    return fail(errorOut, errorOutLen, "Degenerate triangle after coordinate conversion");
                // Match Mesh Debug's per-subset "Recalculate all": average unit face
                // normals, so large triangles do not dominate smaller groove faces.
                const VEC3 normal(static_cast<float>(nx / faceLength),
                    static_cast<float>(ny / faceLength), static_cast<float>(nz / faceLength));
                if (preservePlateaus && i/3<topology.triangles.size())
                {
                    const uint32_t ia=indices[i],ib=indices[i+1],ic=indices[i+2];
                    const float low=std::min({levels[ia],levels[ib],levels[ic]});
                    const float high=std::max({levels[ia],levels[ib],levels[ic]});
                    if (low>=0.9999f || high<=0.0001f)
                    {
                        for (uint32_t index : {ia,ib,ic})
                        {
                            auto &sum=plateauNormals[index];
                            sum.x+=normal.x; sum.y+=normal.y; sum.z+=normal.z;
                        }
                    }
                }
                for (auto *vertex : {&a, &b, &c})
                {
                    vertex->normal.x += normal.x; vertex->normal.y += normal.y; vertex->normal.z += normal.z;
                }
            }
            for (uint32_t i=0;i<plateauNormals.size();++i)
            {
                const auto &normal=plateauNormals[i];
                if (normal.x!=0 || normal.y!=0 || normal.z!=0) vertices[i].normal=normal;
            }
            if (o.backRelief)
            {
                // Identical sampled relief on both sides must retain identical smoothing,
                // including the front's authored plateau normals.
                for (uint32_t i=0;i<gridSize;++i)
                {
                    const auto &front=vertices[i].normal;
                    vertices[backIndex[i]].normal=VEC3(front.x,front.y,-front.z);
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
            const uint32_t sideSubset=separateBack?2:1;
            uint32_t frontVertexCount=vertexCount,frontIndexCount=static_cast<uint32_t>(indices.size()),subsetCount=1;
            if (separateBack) { frontVertexCount=gridSize; frontIndexCount=backIndexStart; subsetCount=3; }
            else if (separateSides) { frontVertexCount=sideVertexStart; frontIndexCount=sideIndexStart; subsetCount=2; }
            destination.addSubset(0);
            if (!destination.addVertex(0, 0, frontVertexCount))
                return fail(errorOut, errorOutLen, "Cannot allocate mesh vertices");
            if (separateBack)
            {
                destination.addSubset(0);
                if (!destination.addVertex(0,1,backSize))
                    return fail(errorOut,errorOutLen,"Cannot allocate back mesh vertices");
            }
            if (separateSides)
            {
                destination.addSubset(0);
                if (!destination.addVertex(0,sideSubset,vertexCount-sideVertexStart))
                    return fail(errorOut,errorOutLen,"Cannot allocate side mesh vertices");
            }
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
                uvs[i] = o.backSolid && i>=gridSize && i<sideVertexStart ? VEC2(.5f,.5f) : vertex.uv;
            }
            destination.getSubset(0, 0)->texture = path;
            if (!destination.addIndex(0,0,indices.data(),frontIndexCount,errorOut,errorOutLen)) return false;
            if (separateBack)
            {
                if (o.backSolid)
                {
                    char color[10]; std::snprintf(color,sizeof(color),"#%06XFF",o.backColor);
                    backTexture=color;
                }
                destination.getSubset(0,1)->texture=backTexture;
                for (size_t i=backIndexStart;i<sideIndexStart;++i) indices[i]=static_cast<uint16_t>(indices[i]-gridSize);
                if (!destination.addIndex(0,1,indices.data()+backIndexStart,sideIndexStart-backIndexStart,errorOut,errorOutLen)) return false;
            }
            if (separateSides)
            {
                destination.getSubset(0,sideSubset)->texture=sideTexture;
                for (size_t i=sideIndexStart;i<indices.size();++i) indices[i]=static_cast<uint16_t>(indices[i]-sideVertexStart);
                if (!destination.addIndex(0,sideSubset,indices.data()+sideIndexStart,static_cast<uint32_t>(indices.size())-sideIndexStart,errorOut,errorOutLen)) return false;
            }
            // Consumers such as the simplifier use frame counts before the first save.
            auto *frame = destination.getFrameBuffer(0);
            frame->headerFrame.totalSubset = subsetCount;
            frame->headerFrame.sizeVertexBuffer = static_cast<int>(vertexCount);
            frame->headerFrame.sizeIndexBuffer = static_cast<int>(indices.size());
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
    bool getImageMeshSideContour(const IMAGE_MESH_OPTIONS &options,IMAGE_MESH_POINT *points,
                                 uint32_t &count,float &maximumInset,char *errorOut,int errorOutLen)
    {
        const uint32_t capacity=count; count=0; maximumInset=0;
        if (errorOut && errorOutLen>0) errorOut[0]=0;
        if (!points || capacity<128 || options.cropWidth<2 || options.cropHeight<2 ||
            options.cropWidth>16777216 || options.cropHeight>16777216)
            return fail(errorOut,errorOutLen,"Side contour requires a 128-point buffer and valid crop dimensions");
        try
        {
            IMAGE_MESH_OPTIONS o=options;
            o.columns=1; o.rows=1; o.width=100; o.height=100; o.followImage=false; o.maxVertices=65535; o.maxTriangles=131070;
            image_mesh::TOPOLOGY topology; std::string error;
            if (!image_mesh::buildTopology(o,topology,error)) return fail(errorOut,errorOutLen,error.c_str());
            std::vector<IMAGE_MESH_POINT> inner;
            if (!image_mesh::sideContour(o,topology.contour,inner,maximumInset,error)) return fail(errorOut,errorOutLen,error.c_str());
            count=static_cast<uint32_t>(inner.size());
            std::copy(inner.begin(),inner.end(),points); return true;
        }
        catch (const std::exception &e) { return fail(errorOut,errorOutLen,e.what()); }
    }
    bool generateImageMeshMap(const char *imagePath,const IMAGE_MESH_OPTIONS &o,const char *outputPath,
                              bool overlay,char *errorOut,int errorOutLen)
    {
        if (errorOut && errorOutLen>0) errorOut[0]=0;
        if (!outputPath || !*outputPath) return fail(errorOut,errorOutLen,"Output PNG path required");
        if (!std::isfinite(o.borderWidth) || o.borderWidth<0 || o.borderWidth>0.5f)
            return fail(errorOut,errorOutLen,"Invalid border width");
        try
        {
            image_mesh::HEIGHT_FIELD field; std::string error;
            if (!field.load(imagePath,o,error)) return fail(errorOut,errorOutLen,error.c_str());
            IMAGE_MESH_OPTIONS outline=o;
            outline.followImage=false; outline.columns=outline.rows=1; outline.maxVertices=65535; outline.maxTriangles=131070;
            image_mesh::TOPOLOGY topology;
            if (!image_mesh::buildTopology(outline,topology,error)) return fail(errorOut,errorOutLen,error.c_str());
            std::vector<unsigned char> rgba(static_cast<size_t>(field.width)*field.height*4,0);
            for (uint32_t y=0;y<field.height;++y) for (uint32_t x=0;x<field.width;++x)
            {
                const IMAGE_MESH_POINT p{static_cast<float>(x)/std::max(1u,field.width-1),static_cast<float>(y)/std::max(1u,field.height-1)};
                bool inside=false;
                for (size_t i=0,j=topology.contour.size()-1;i<topology.contour.size();j=i++)
                {
                    const auto &a=topology.contour[i],&b=topology.contour[j];
                    if ((a.y>p.y)!=(b.y>p.y) && p.x<(b.x-a.x)*(p.y-a.y)/(b.y-a.y)+a.x) inside=!inside;
                }
                const float distance=image_mesh::borderDistance(p,topology);
                if (!inside && distance>1e-6f) continue;
                const float intensity=field.sample(p.x,p.y);
                float level=field.surface(p.x,p.y,o);
                if (o.lockBorder)
                {
                    if (distance<1e-7f) level=0;
                    else if (o.borderWidth>0) level*=std::min(1.0f,distance/o.borderWidth);
                }
                auto *out=rgba.data()+(static_cast<size_t>(y)*field.width+x)*4;
                const auto *source=field.pixels.get()+(static_cast<size_t>(y+o.y)*field.imageWidth+x+o.x)*4;
                for (unsigned c=0;c<3;++c)
                    out[c]=static_cast<unsigned char>(std::lround(overlay?
                        (intensity<o.grooveThreshold?source[c]*0.35f+(c==2?255.0f:40.0f)*0.65f:source[c]):level*255));
                out[3]=overlay?source[3]:255;
            }
            const unsigned code=lodepng::encode(outputPath,rgba,field.width,field.height);
            if (code) return fail(errorOut,errorOutLen,lodepng_error_text(code));
            return true;
        }
        catch (const std::exception &e) { return fail(errorOut,errorOutLen,e.what()); }
    }

}
