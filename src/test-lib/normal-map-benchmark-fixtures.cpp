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


#if (defined(USE_OPENGL_ES) && defined(__linux__)) || defined(USE_METAL)
#include <core_mbm/mesh-manager.h>
#include <core_mbm/draw-compatibility.h>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
namespace
{
    using namespace mbm;
    int parameter(const char *name, int fallback, int low, int high)
    {
        const char *value = std::getenv(name);
        if (!value) return fallback;
        char *end = nullptr;
        const long parsed = std::strtol(value,&end,10);
        return end != value && *end == 0 && parsed >= low && parsed <= high ? static_cast<int>(parsed) : -1;
    }
}

using namespace mbm;

int createNormalMapBenchmarkFixtures()
{
    const char *directory = std::getenv("MBM_NORMAL_BENCH_DIR");
    const int grid = parameter("MBM_NORMAL_BENCH_GRID",32,2,256);
    if (!directory || grid < 0 || grid % 2) return -1;
    std::filesystem::create_directories(directory);
    MESH_MBM_DEBUG mesh;
    mesh.addBuffer(); mesh.addSubset(0); mesh.addSubset(0);
    const uint32_t count = static_cast<uint32_t>(grid*grid*6);
    auto *frame = mesh.getFrameBuffer(0);
    auto *positions = new VEC3[count]; auto *normals = new VEC3[count]; auto *uv = new VEC2[count];
    frame->position = reinterpret_cast<float *>(positions);
    frame->normal = reinterpret_cast<float *>(normals);
    frame->uv = reinterpret_cast<float *>(uv);
    frame->headerFrame.sizeVertexBuffer = count; frame->headerFrame.totalSubset = 2;
    const int dx[6] = {0,1,0,1,1,0}, dy[6] = {0,0,1,0,1,1};
    uint32_t index = 0;
    for (int y=0; y<grid; ++y) for (int x=0; x<grid; ++x) for (int c=0; c<6; ++c,++index)
    {
        const float u = static_cast<float>(x+dx[c])/grid, v = static_cast<float>(y+dy[c])/grid;
        positions[index] = VEC3(u*1.8f-0.9f,v*1.8f-0.9f,0);
        normals[index] = VEC3(0,0,1); uv[index] = VEC2(u,v);
    }
    for (uint32_t i=0; i<2; ++i)
    {
        auto *subset = mesh.getSubset(0,i);
        subset->vertexStart = i*count/2; subset->vertexCount = count/2;
        subset->indexCount = 0; subset->indexStart = 0; subset->texture = "#FFFFFFFF";
    }
    mesh.setHasNormal(HAS_NOR_IN_FILE); mesh.setHasTexture(HAS_TEX_EACH_FRAME);
    mesh.setMeshType(util::TYPE_MESH_3D); mesh.setModeDraw(util::MODE_DRAW_TRIANGLES);
    char error[512] = {};
    const auto save = [&](const char *name)
    { return mesh.saveV11((std::filesystem::path(directory)/name).string().c_str(),false,false,false,error,sizeof(error)); };
    if (!save("plain.msh")) return -1;
    for (uint32_t i=0; i<2; ++i)
    {
        NORMAL_MAP_REPORT report;
        if (!mesh.prepareNormalMap(0,i,NORMAL_MAP_POLICY::GENERATE,report,error,sizeof(error))) return -1;
    }
    if (!save("prepared.msh")) return -1;
    std::printf("NORMAL MAP BENCH FIXTURES PASS vertices=%u\n",count);
    return 0;
}

#endif
