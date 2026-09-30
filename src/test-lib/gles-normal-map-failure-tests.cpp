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
#include "faults/gles-faults.h"
#include <core_mbm/mesh-manager.h>
#include <core_mbm/device.h>
#include <core_mbm/light.h>
#include <specific-opengl_es.h>
#include <specific-opengl_es-buffer.h>
#include <dlfcn.h>
#include <cstdio>
#include <cstdlib>
#include <string>

int runGlesNormalMapFailureTests()
{
    using namespace mbm;
    using namespace mbm_test;
    int failures = 0;
    const auto check = [&failures](bool ok,const char *label)
    { if (!ok) { ++failures; std::printf("NORMAL MAP RECOVERY FAIL: %s\n",label); } return ok; };
    const auto arm = reinterpret_cast<ARM>(dlsym(RTLD_DEFAULT,"mbm_test_arm_gles_fault"));
    const auto read = reinterpret_cast<READ>(dlsym(RTLD_DEFAULT,"mbm_test_read_gles_fault"));
    if (!check(arm && read,"test-only injection library loaded")) return -1;
    const char *dir = std::getenv("MBM_NORMAL_MAP_FIXTURE_DIR");
    if (!check(dir != nullptr,"fixture directory configured")) return -1;
    const auto *mesh = MESH_MANAGER::getInstance()->load((std::string(dir)+"/author-import.msh").c_str());
    if (!check(mesh != nullptr,"load retained basis")) return -1;
    auto *buffer = mesh->getBuffer(0)->pBufferGL;
    SHADER shader;
    shader.setUseReservedLightDefault(true);
    if (!check(shader.compileShader(nullptr,nullptr,buffer->fvf),"compile geometric shader")) return -1;
    MatrixIdentity(&SHADER::modelView); MatrixIdentity(&SHADER::mvMatrixLightSpace); MatrixIdentity(&SHADER::mvpMatrix);
    DEVICE::getInstance()->setLightTargetForRender(LIGHT_TARGET_3D);
    setLightEnabled(LIGHT_TARGET_3D,true);
    check(shader.render(buffer),"initial geometric draw");
    GLint geometric = 0; glGetIntegerv(GL_CURRENT_PROGRAM,&geometric);
    const auto geometricDraw = [&]()
    {
        check(mesh->setNormalMapSettings(0,0,1,0),"disable strength during recovery");
        check(shader.render(buffer),"geometric draw remains usable after failure");
        GLint current = 0; glGetIntegerv(GL_CURRENT_PROGRAM,&current);
        check(current == geometric,"failed mapped shader does not replace geometric shader");
        check(mesh->setNormalMapSettings(0,0,1,1),"reenable strength");
    };
    check(mesh->setMaterialTexture(0,0,TEXTURE_ROLE_NORMAL,"#FF8080FF",false),"assign normal map");
    check(mesh->setNormalMapSettings(0,0,1,1),"enable mapping");
    STATS stats = {};
#if USE_NORMAL_MAPPING_3D
    for (int fault : {VERTEX_COMPILE,FRAGMENT_COMPILE,PROGRAM_CREATE,PROGRAM_LINK})
    {
        arm(fault);
        check(!shader.render(buffer),"injected shader error rejects draw");
        read(&stats);
        check(stats.injected == 1,"shader fault actually reached driver boundary");
        check(stats.shadersCreated == stats.shadersDeleted,"failed attempt releases all temporary shaders");
        for (auto id : stats.shaderIds) if (id) check(!glIsShader(id),"driver confirms failed shader deletion");
        if (stats.programId) check(!glIsProgram(stats.programId),"driver confirms failed program deletion");
        check(stats.programsCreated == stats.programsDeleted,"failed attempt releases temporary program");
        check(stats.uploads == 0 && buffer->getBackendBuffer()->normalMapSubsets.empty(),"shader error cannot upload or publish buffers");
        geometricDraw();
        check(glGetError() == GL_NO_ERROR,"shader recovery leaves no GL error");
        std::printf("NORMAL MAP RECOVERY shader fault=%d injected=%u shaders=%u/%u programs=%u/%u\n",
                    fault,stats.injected,stats.shadersCreated,stats.shadersDeleted,stats.programsCreated,stats.programsDeleted);
    }
    for (int fault : {VERTEX_UPLOAD,INDEX_UPLOAD,INDEX_UPLOAD})
    {
        arm(fault);
        check(!shader.render(buffer),"injected upload error rejects draw");
        read(&stats);
        check(stats.injected == 1 && stats.uploads == 2,"upload fault actually reached driver boundary");
        check(stats.buffersCreated == 2 && stats.buffersDeleted == 2,"rollback deletes both temporary buffers");
        for (auto id : stats.bufferIds) if (id) check(!glIsBuffer(id),"driver confirms rolled-back buffer deletion");
        check(buffer->getBackendBuffer()->normalMapSubsets.empty(),"partial upload is never published");
        if (fault == INDEX_UPLOAD) check(stats.compiles == 0,"retry retains successfully compiled mapped variant");
        geometricDraw();
        check(glGetError() == GL_NO_ERROR,"upload failure consumed GL error");
    }
    arm(NONE);
    check(shader.render(buffer),"retry succeeds using retained staging");
    read(&stats);
    check(stats.buffersCreated == 2 && stats.uploads == 2 && stats.compiles == 0,"retry uploads once without recompiling");
    const auto &subsets = buffer->getBackendBuffer()->normalMapSubsets;
    if (!check(!subsets.empty() && !subsets[0].batches.empty(),"successful retry publishes batches")) return -1;
    const auto vertices = subsets[0].batches[0].buffers[0];
    GLint mapped = 0; glGetIntegerv(GL_CURRENT_PROGRAM,&mapped);
    check(mapped != geometric && glGetAttribLocation(static_cast<GLuint>(mapped),"aTangent") >= 0,"retry binds mapped shader");
    arm(NONE);
    for (int i=0; i<20; ++i) check(shader.render(buffer),"warm draw after recovery");
    read(&stats);
    check(stats.compiles == 0 && stats.uploads == 0 && stats.buffersCreated == 0,"recovered warm draws do no compile/upload");
    check(subsets[0].batches[0].buffers[0] == vertices,"recovered allocation reused");
#else
    for (int fault : {VERTEX_COMPILE,FRAGMENT_COMPILE,PROGRAM_CREATE,PROGRAM_LINK,VERTEX_UPLOAD,INDEX_UPLOAD})
    {
        arm(fault);
        check(shader.render(buffer),"disabled build draws geometrically");
        read(&stats);
        check(stats.injected == 0 && stats.compiles == 0 && stats.uploads == 0,"disabled feature cannot reach injection points");
    }
#endif
    arm(NONE);
    check(glGetError() == GL_NO_ERROR,"final GL state clean");
    std::printf("NORMAL MAP RECOVERY %s (%d failures)\n",failures ? "FAIL" : "PASS",failures);
    return failures ? -1 : 0;
}
#endif
