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


#if defined(USE_OPENGL_ES) && defined(__linux__)
#include <core_mbm/mesh-manager.h>
#include <core_mbm/light.h>
#include <core_mbm/draw-compatibility.h>
#include <core_mbm/device.h>
#include <core_mbm/camera.h>
#include <specific-opengl_es.h>
#include <specific-opengl_es-buffer.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <vector>
#include <unistd.h>
namespace
{
    using namespace mbm;
    using CLOCK = std::chrono::steady_clock;
    double micros(CLOCK::time_point start) { return std::chrono::duration<double,std::micro>(CLOCK::now()-start).count(); }
    int parameter(const char *name, int fallback, int low, int high)
    {
        const char *value = std::getenv(name);
        if (!value) return fallback;
        char *end = nullptr;
        const long parsed = std::strtol(value,&end,10);
        return end != value && *end == 0 && parsed >= low && parsed <= high ? static_cast<int>(parsed) : -1;
    }
    size_t rss()
    {
        std::ifstream input("/proc/self/statm");
        size_t total = 0, resident = 0;
        return input >> total >> resident ? resident * static_cast<size_t>(sysconf(_SC_PAGESIZE)) : 0;
    }
    size_t gpuBytes(const BUFFER_GL *buffer, bool derived)
    {
        const auto *backend = buffer->getBackendBuffer();
        std::set<GLuint> ids;
        if (derived)
        {
#if USE_NORMAL_MAPPING_3D
            for (const auto &subset : backend->normalMapSubsets)
                for (const auto &batch : subset.batches)
                    for (auto id : batch.buffers) if (id) ids.insert(id);
#endif
        }
        else if (buffer->isIndexBuffer())
        {
            for (auto id : backend->vboVertNorTexIB) if (id) ids.insert(id);
            if (backend->vboIndexSubsetIB)
                for (uint32_t i=0; i<buffer->totalSubset; ++i) if (backend->vboIndexSubsetIB[i]) ids.insert(backend->vboIndexSubsetIB[i]);
        }
        else
        {
            for (uint32_t i=0; i<buffer->totalSubset; ++i)
                for (auto array : {backend->vboVertexSubsetVB,backend->vboNormalSubsetVB,backend->vboTextureSubsetVB})
                    if (array && array[i]) ids.insert(array[i]);
        }
        GLint previous = 0; glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&previous);
        size_t bytes = 0;
        for (auto id : ids)
        {
            GLint size = 0; glBindBuffer(GL_ARRAY_BUFFER,id);
            glGetBufferParameteriv(GL_ARRAY_BUFFER,GL_BUFFER_SIZE,&size);
            bytes += static_cast<size_t>(size);
        }
        glBindBuffer(GL_ARRAY_BUFFER,static_cast<GLuint>(previous));
        return bytes;
    }
}

int runGlesNormalMapBenchmark()
{
    const char *directory = std::getenv("MBM_NORMAL_BENCH_DIR"), *caseName = std::getenv("MBM_NORMAL_BENCH_CASE");
    const int draws = parameter("MBM_NORMAL_BENCH_DRAWS",32,1,4096);
    const int blocks = parameter("MBM_NORMAL_BENCH_BLOCKS",8,2,100);
    if (!directory || !caseName || draws < 0 || blocks < 0) return -1;
    const std::string mode(caseName);
    if (mode != "plain" && mode != "retained" && mode != "zero" && mode != "mapped" && mode != "mixed" && mode != "removed") return -1;
    const auto fail = [](const char *message) { std::printf("NORMAL MAP BENCH FAIL: %s\n",message); return -1; };
    glFinish();
    const size_t rssBefore = rss();
    auto start = CLOCK::now();
    const auto *mesh = MESH_MANAGER::getInstance()->load((std::filesystem::path(directory)/(mode == "plain" ? "plain.msh" : "prepared.msh")).string().c_str());
    glFinish();
    const double loadUs = micros(start);
    if (!mesh) return fail("load");
    auto *buffer = mesh->getBuffer(0)->pBufferGL;
    const size_t rssLoaded = rss(), sourceBytes = gpuBytes(buffer,false), beforeBytes = gpuBytes(buffer,true);
    start = CLOCK::now();
    if (mode != "plain" && mode != "retained")
        for (uint32_t i=0; i<(mode == "mixed" ? 1u : 2u); ++i)
            if (!mesh->setMaterialTexture(0,i,TEXTURE_ROLE_NORMAL,"#FF8080FF",false) ||
                !mesh->setNormalMapSettings(0,i,1,mode == "zero" ? 0.0f : 1.0f)) return fail("material setup");
    glFinish();
    const double materialUs = micros(start);
    auto *device = DEVICE::getInstance();
    CAMERA &camera = device->getCamera(); MatrixIdentity(&camera.matrixView);
    device->setLightTargetForRender(LIGHT_TARGET_3D);
    setLightEnabled(LIGHT_TARGET_3D,true);
    setAmbientLight(LIGHT_TARGET_3D,COLOR(0.1f,0.1f,0.1f,1));
    setDirectionalLight(LIGHT_TARGET_3D,VEC3(0,0,-1),COLOR(0.7f,0.7f,0.7f,1));
    MatrixIdentity(&SHADER::modelView); MatrixIdentity(&SHADER::mvMatrixLightSpace); MatrixIdentity(&SHADER::mvpMatrix);
    glViewport(0,0,128,128); glDisable(GL_CULL_FACE); glDisable(GL_DEPTH_TEST); glDisable(GL_BLEND); glDisable(GL_SCISSOR_TEST);
    glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
    SHADER shader; shader.setUseReservedLightDefault(true);
    start = CLOCK::now();
    if (!shader.compileShader(nullptr,nullptr,buffer->fvf)) return fail("geometric compile");
    glFinish(); const double compileUs = micros(start);
    start = CLOCK::now();
    if (!shader.render(buffer)) return fail("first draw");
    const double firstSubmitUs = micros(start);
    glFinish(); const double firstSyncUs = micros(start);
    const size_t rssFirst = rss(), afterBytes = gpuBytes(buffer,true);
    const bool effective = USE_NORMAL_MAPPING_3D && (mode == "mapped" || mode == "mixed" || mode == "removed");
    if (beforeBytes != 0 || (afterBytes > 0) != effective) return fail("derived allocation contract");
    unsigned char pixel[4] = {}; glReadPixels(64,64,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
    if (pixel[0] == 0 && pixel[1] == 0 && pixel[2] == 0) return fail("invisible geometry");
    start = CLOCK::now();
    if (mode == "removed")
        for (uint32_t i=0; i<2; ++i) if (!mesh->setMaterialTexture(0,i,TEXTURE_ROLE_NORMAL,nullptr,false)) return fail("remove map");
    const double removalUs = micros(start);
    for (int i=0; i<16; ++i) if (!shader.render(buffer)) return fail("warmup");
    glFinish();
    std::vector<double> submit, synchronized;
    submit.reserve(blocks); synchronized.reserve(blocks);
    for (int block=0; block<blocks; ++block)
    {
        start = CLOCK::now();
        for (int i=0; i<draws; ++i) if (!shader.render(buffer)) return fail("warm draw");
        submit.push_back(micros(start)/draws);
        glFinish(); synchronized.push_back(micros(start)/draws);
    }
    const size_t warmBytes = gpuBytes(buffer,true), rssWarm = rss();
    if (warmBytes != afterBytes || glGetError() != GL_NO_ERROR) return fail("warm resources or GL state");
    std::printf("NORMAL_MAP_BENCH_JSON {\"normal\":%d,\"lights\":%u,\"vertices\":%u,\"case\":\"%s\","
        "\"load_sync_us\":%.3f,\"material_sync_us\":%.3f,\"geometric_compile_sync_us\":%.3f,"
        "\"first_submit_us\":%.3f,\"first_sync_us\":%.3f,\"removal_us\":%.3f,"
        "\"rss_before\":%zu,\"rss_loaded\":%zu,\"rss_first\":%zu,\"rss_warm\":%zu,"
        "\"source_gpu_bytes\":%zu,\"derived_before\":%zu,\"derived_first\":%zu,\"derived_warm\":%zu,\"warm_submit_us\":[",
        USE_NORMAL_MAPPING_3D,getSupportedMaxLights(LIGHT_TARGET_3D),buffer->sizeOfArrayVertex,caseName,
        loadUs,materialUs,compileUs,firstSubmitUs,firstSyncUs,removalUs,rssBefore,rssLoaded,rssFirst,rssWarm,
        sourceBytes,beforeBytes,afterBytes,warmBytes);
    for (size_t i=0; i<submit.size(); ++i) std::printf("%s%.3f",i ? "," : "",submit[i]);
    std::printf("],\"warm_sync_us\":[");
    for (size_t i=0; i<synchronized.size(); ++i) std::printf("%s%.3f",i ? "," : "",synchronized[i]);
    std::printf("]}\nNORMAL MAP BENCH PASS\n");
    return 0;
}
#endif
