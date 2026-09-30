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
#include <core_mbm/core-manager.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/light.h>
#include <core_mbm/camera.h>
#include <render/mesh.h>
#include <specific-opengl_es.h>
#include <specific-opengl_es-buffer.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

int runGlesNormalMapContextTests(mbm::CORE_MANAGER &game, const mbm::SCENE &scene)
{
    using namespace mbm;
    int failures = 0;
    const auto check = [&failures](bool ok,const char *label)
    { if (!ok) { ++failures; std::printf("NORMAL MAP CONTEXT FAIL: %s\n",label); } return ok; };
    const char *directory = std::getenv("MBM_NORMAL_MAP_FIXTURE_DIR");
    if (!check(directory != nullptr,"fixture directory configured")) return -1;
    const std::filesystem::path dir(directory);
    const auto author = [&](const char *source,const char *target)
    {
        MESH_MBM_DEBUG data;
        char message[512] = {};
        if (!data.loadV11((dir/source).string().c_str())) return false;
        if (std::string(source) == "prepared.msh")
            data.getSubset(0,0)->materialTextureSlots[0].texture = "#FF8080FF";
        return data.addAnimation("context",0,0,0.1f,TYPE_ANIMATION_PAUSED,message,sizeof(message)) >= 0 &&
            data.saveV11((dir/target).string().c_str(),false,false,false,message,sizeof(message));
    };
    if (!check(author("prepared.msh","context-active.msh") && author("no-map.msh","context-plain.msh"),
               "author fixtures with production animation state")) return -1;
    std::error_code error;
    std::filesystem::copy_file(dir/"context-active.msh",dir/"context-pending.msh",
                              std::filesystem::copy_options::overwrite_existing,error);
    if (!check(!error,"create independent pending asset")) return -1;
    struct TEST_MESH : MESH
    {
        using MESH::MESH;
        using RENDERIZABLE::getMesh;
        using RENDERIZABLE::render;
    };
    TEST_MESH active(&scene,true,false), shared(&scene,true,false), pending(&scene,true,false), plain(&scene,true,false);
    if (!check(active.load((dir/"context-active.msh").string().c_str()) &&
               shared.load((dir/"context-active.msh").string().c_str()) &&
               pending.load((dir/"context-pending.msh").string().c_str()) &&
               plain.load((dir/"context-plain.msh").string().c_str()),"load restore fixtures")) return -1;
    const auto asset = [](TEST_MESH &object) { return object.getMesh(); };
    const auto buffer = [&](TEST_MESH &object) { return asset(object)->getBuffer(0)->pBufferGL; };
    const auto uploaded = [&](TEST_MESH &object)
    {
#if USE_NORMAL_MAPPING_3D
        return !buffer(object)->getBackendBuffer()->normalMapSubsets.empty();
#else
        (void)object; return false;
#endif
    };
    check(asset(active) == asset(shared),"initial shared asset identity");
    check(!uploaded(active) && !uploaded(pending),"all assets initially deferred");
    using IMAGE = std::array<unsigned char,64*64*4>;
    const auto capture = [&](TEST_MESH &object)
    {
        // Local probes never survive a context boundary; production object shaders
        // and asset resources are restored by CORE_MANAGER's real lifecycle.
        SHADER probe;
        probe.setUseReservedLightDefault(true);
        check(probe.compileShader(nullptr,nullptr,buffer(object)->fvf),"compile probe in current context");
        auto *device = DEVICE::getInstance();
        CAMERA &camera = device->getCamera();
        MatrixIdentity(&camera.matrixView);
        device->setLightTargetForRender(LIGHT_TARGET_3D);
        setLightEnabled(LIGHT_TARGET_3D,true);
        setAmbientLight(LIGHT_TARGET_3D,COLOR(0.4f,0.4f,0.4f,1));
        setDirectionalLight(LIGHT_TARGET_3D,VEC3(0,0,-1),COLOR(0.5f,0.5f,0.5f,1));
        MatrixIdentity(&SHADER::modelView); MatrixIdentity(&SHADER::mvMatrixLightSpace); MatrixIdentity(&SHADER::mvpMatrix);
        glViewport(0,0,64,64); glDisable(GL_SCISSOR_TEST); glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE); glDisable(GL_BLEND);
        glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        check(probe.render(buffer(object)),"draw probe in current context");
        GLint program = 0; glGetIntegerv(GL_CURRENT_PROGRAM,&program);
        check(glIsProgram(static_cast<GLuint>(program)),"current program is live");
        const bool mapped = &object != &plain && USE_NORMAL_MAPPING_3D;
        check((glGetAttribLocation(static_cast<GLuint>(program),"aTangent") >= 0) == mapped,"restored shader variant matches material");
        IMAGE pixels = {};
        glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        bool visible = false;
        for (size_t i=0; i<pixels.size(); i+=4) visible |= pixels[i] != 0 || pixels[i+1] != 0 || pixels[i+2] != 0;
        check(visible,"capture contains visible geometry");
        check(glGetError() == GL_NO_ERROR,"capture leaves no GL error");
        return pixels;
    };
    const IMAGE baseline = capture(active), plainBaseline = capture(plain);
    check((baseline != plainBaseline) == (USE_NORMAL_MAPPING_3D != 0),"detail map changes pixels only when enabled");
    check(!uploaded(plain),"geometric asset has no derived buffers");
    check(uploaded(active) == (USE_NORMAL_MAPPING_3D != 0),"effective draw uploads only in enabled builds");
    check(!uploaded(pending),"undrawn asset stays pending");
    check(active.render(),"draw with production object's shader before restore");
    active.setPosition(VEC3(2,3,4));
    for (int cycle=0; cycle<2; ++cycle)
    {
        // Not owned by an engine manager: its absence proves a fresh, unshared
        // GL namespace, not just asset reload. Reserve names above init's low IDs.
        GLuint sentinels[128] = {};
        glGenBuffers(128,sentinels);
        const GLuint sentinel = sentinels[127];
        glBindBuffer(GL_ARRAY_BUFFER,sentinel);
        const unsigned char marker[37] = {};
        glBufferData(GL_ARRAY_BUFFER,sizeof(marker),marker,GL_STATIC_DRAW);
        check(glIsBuffer(sentinel),"unmanaged sentinel exists in old context");
        check(glGetError() == GL_NO_ERROR && eglGetError() == EGL_SUCCESS,"clean state before restore");
        game.onStopCoreManager();
        // Deliberately bounded instead of forceRestore's unbounded loop. These
        // are the same production state-machine calls, with no manual reload.
        bool complete = false;
        for (int step=0; step<128 && !complete; ++step)
        {
            complete = game.onLostDevice(false,640,480,0,0);
            if (step == 0)
            {
                check(eglGetCurrentContext() != EGL_NO_CONTEXT,"new EGL context made current");
                check(!glIsBuffer(sentinel),"old context sentinel is gone before asset reload");
            }
        }
        if (!check(complete,"production restore state machine completes")) return -1;
        check(eglGetError() == EGL_SUCCESS,"EGL lifecycle has no error");
        check(glGetError() == GL_NO_ERROR,"GL lifecycle has no error");
        if (!check(asset(active) && asset(pending) && asset(plain),"objects reloaded assets")) return -1;
        check(asset(active) == asset(shared),"shared asset identity restored");
        check(!uploaded(active) && !uploaded(pending),"restore rebuilds staging without eager upload");
        check(active.getPosition().x == 2 && active.getPosition().y == 3 && active.getPosition().z == 4,"object transform preserved");
        check(capture(active) == baseline,"mapped pixels match pre-loss baseline");
        check(capture(plain) == plainBaseline,"geometric pixels match pre-loss baseline");
        check(uploaded(shared) == (USE_NORMAL_MAPPING_3D != 0),"shared instance reuses restored allocation");
        check(!uploaded(pending),"pending asset stays deferred while other assets draw");
        check(active.render(),"production object's restored shader draws");
        std::printf("NORMAL MAP CONTEXT cycle=%d completed\n",cycle+1);
    }
    check(capture(pending) == baseline,"late first use after two restores matches baseline");
    check(uploaded(pending) == (USE_NORMAL_MAPPING_3D != 0),"late first use uploads restored staging");
    check(glGetError() == GL_NO_ERROR,"final GL state clean");
    std::printf("NORMAL MAP CONTEXT %s (%d failures)\n",failures ? "FAIL" : "PASS",failures);
    return failures ? -1 : 0;
}
#endif
