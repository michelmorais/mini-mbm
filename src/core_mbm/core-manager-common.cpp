/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2025 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

// Must be defined before any system headers to enable GNU extensions (e.g. POSIX_SPAWN_SETSID)
#if defined(__linux__)
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#endif

#include <core-manager.h>
#include <device.h>
#include <scene.h>
#include <renderizable.h>
#include <physics.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <audio-interface.h>
#include <algorithm>
#include <plugin-callback.h>
#include <dynamic-var.h>
#include <shader-resource.h>
#include <util-interface.h>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#if (defined(__linux__) || (defined(__APPLE__) && !defined(USE_METAL))) && !defined(ANDROID)
#  include <spawn.h>
   extern char **environ;
   // Forward declaration for the X11-specific helper defined in core-manager-opengl_es-x11.cpp.
   // Not available when using the dummy backend (no X11 display is opened).
#  if !defined(USE_DUMMY_BACK_END_ENGINE)
   namespace mbm { int getX11DisplayFd() noexcept; }
#  endif
#endif

namespace mbm
{
    struct CORE_MANAGER::Impl
    {
        std::map<int, bool>                  keyPressed;
        std::list<EVENT_KEY>                 events;
        std::list<INFO_JOYSTICK_INIT_PLAYER> joystickInfoEvents;
        std::mutex                           mutexEvents;
        EVENT_KEY                            lastEvent;
        std::vector<PLUGIN*>                 plugins;
        bool                                 wasGamePausedBeforeOnStop = false;
        bool                                 loopVariablesInitialized  = false;
        WHICH_FOR                            whichFor                  = WFOR_INITIAL;
        STEP_RETORE                          stepRestore               = STEP_RES_INIT_GL;
        uint32_t                             indexOnRestore            = 0;
        uint32_t                             totalForByLoop            = 0;
        float                                stepRestoreInfo           = 0.1f;
        float                                percentRestoreInfo        = 0.0f;
        std::string                          nameApplication           = "Mini-mbm";
        bool                                 windowBorder              = true;
        bool                                 enableResizeWindow        = true;
        bool                                 changeScene               = true;
        bool                                 keyCapsLockState          = false;
        bool                                 sceneWasInitialized       = false;
    };

    void CORE_MANAGER::ImplDeleter::operator()(Impl *ptr) const
    {
        delete ptr;
    }

    void CORE_MANAGER::initializeImpl()
    {
        impl.reset(new Impl());
    }

    CORE_MANAGER::~CORE_MANAGER()
    {
        DEVICE::quit();
    }

    uint32_t CORE_MANAGER::getTotalPlugins() const noexcept
    {
        return static_cast<uint32_t>(impl->plugins.size());
    }

    PLUGIN *CORE_MANAGER::getPlugin(const uint32_t index) const noexcept
    {
        return index < impl->plugins.size() ? impl->plugins[index] : nullptr;
    }

    unsigned int CORE_MANAGER::appendPlugin(PLUGIN *plugin)
    {
        impl->plugins.push_back(plugin);
        return static_cast<unsigned int>(impl->plugins.size() - 1);
    }

    void CORE_MANAGER::clearPlugins()
    {
        impl->plugins.clear();
    }

    void CORE_MANAGER::setNameApplication(const char *nameApplication)
    {
        impl->nameApplication = nameApplication ? nameApplication : "Mini-mbm";
    }

    const char *CORE_MANAGER::getNameApplication() const noexcept
    {
        return impl->nameApplication.c_str();
    }

    void CORE_MANAGER::setWindowOptions(const bool border, const bool enableResize) noexcept
    {
        impl->windowBorder = border;
        impl->enableResizeWindow = enableResize;
    }

    bool CORE_MANAGER::getWindowBorder() const noexcept
    {
        return impl->windowBorder;
    }

    bool CORE_MANAGER::getEnableResizeWindow() const noexcept
    {
        return impl->enableResizeWindow;
    }

    void CORE_MANAGER::setChangeScene(const bool change) noexcept
    {
        impl->changeScene = change;
    }

    bool CORE_MANAGER::isChangeScene() const noexcept
    {
        return impl->changeScene;
    }

    void CORE_MANAGER::setKeyCapsLockState(const bool enabled) noexcept
    {
        impl->keyCapsLockState = enabled;
    }

    bool CORE_MANAGER::isKeyCapsLockOn() const noexcept
    {
        return impl->keyCapsLockState;
    }

    DEVICE *CORE_MANAGER::getDevice() const noexcept
    {
        return this->device;
    }

    void CORE_MANAGER::setSceneInitialized(const bool initialized) noexcept
    {
        impl->sceneWasInitialized = initialized;
    }

    bool CORE_MANAGER::isSceneInitialized() const noexcept
    {
        return impl->sceneWasInitialized;
    }

    STEP_RETORE CORE_MANAGER::getStepRestore() const noexcept
    {
        return impl->stepRestore;
    }

    void CORE_MANAGER::setUsageOfDefaultPS_VS_WhenNoShader(const bool _useDeafultPSwhenNoPsShader, const bool _useDeafultVSwhenNoVSShader) noexcept
    {
        _setUsageOfDefaultPS_VS_WhenNoShader(_useDeafultPSwhenNoPsShader, _useDeafultVSwhenNoVSShader);
    }

    void CORE_MANAGER::setScene(SCENE *currentScene)
    {
        DEVICE *device = this->getDevice();
        device->setScene(currentScene);
    }

    int CORE_MANAGER::onLoop(const bool singleLoop, const bool doSwapBuffers)
    {
        DEVICE *device = this->getDevice();
        //static int loopCallCount = 0;
        //loopCallCount++;
        //if (loopCallCount <= 3)
        //    INFO_LOG("CORE_MANAGER::onLoop() call #%d singleLoop=%d doSwap=%d device=%p scene=%p",
        //             loopCallCount, (int)singleLoop, (int)doSwapBuffers, (void*)device,
        //             device ? (void*)device->getScene() : nullptr);
        if (!device)
            return -1;
        if (!impl->loopVariablesInitialized)
        {
            #if defined _DEBUG || defined DEBUG
            INFO_LOG("CORE_MANAGER::onLoop() first-time init, back buffer [width=%.0f height=%.0f]", device->getBackBufferWidth(), device->getBackBufferHeight());
            #endif
            // Cfg shader from resource----
            if (!device->getShaderConfig().parserCFGFromResource())
            {
                ERROR_LOG("CORE_MANAGER::onLoop() parserCFGFromResource FAILED");
                return -1;
            }
            device->getShaderConfig().sortShader();
            device->setProjectionMode(true, device->getBackBufferWidth(), device->getBackBufferHeight());
            device->updateFps();
            initEnableRenders();
            this->_updateDimFrustum();
            impl->loopVariablesInitialized = true;
            CAMERA &camera = device->getCamera();
            camera.expectedScreen.x = device->getBackBufferWidth();
            camera.expectedScreen.y = device->getBackBufferHeight();
        }
        auto getInitializedScene = [this, device]() -> SCENE *
        {
            SCENE *scene = device->getScene();
            return scene && this->isSceneInitialized() ? scene : nullptr;
        };
        while (device->isRunning())
        {
            handleEventFromWindow();
            for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
            {
                PLUGIN* plugin = this->getPlugin(i);
                plugin->onPrepare();
            }

            INFO_JOYSTICK_INIT_PLAYER info;
            while (this->popEvent(&info))
            {
                if (SCENE *scene = getInitializedScene())
                    scene->onInfoDeviceJoystick(info.player, info.maxNumberButton, info.deviceName.c_str(),
                                                info.extraInfo.c_str());
            }
            
            EVENT_KEY event;
            while (this->popEvent(&event))
            {
                switch (event.eventType)
                {
                    case UNKNOWN: 
                    {
                        ERROR_AT(__LINE__,__FILE__, "CORE_MANAGER::onLoop() - Unknown event type %d.", event.eventType);
                    }
                    break;
                    case ONMOVEWINDOW:
                    {
                        this->moveWindow(static_cast<int>(event.x), static_cast<int>(event.y));
                    }
                    break;
                    case ONRESIZEWINDOW:
                    {
                        if( static_cast<int>(event.x) == static_cast<int>(device->getBackBufferWidth()) &&
                            static_cast<int>(event.y) == static_cast<int>(device->getBackBufferHeight()))
                        {
                            #if defined _DEBUG || defined DEBUG
                            WARN_LOG("CORE_MANAGER::onLoop() - ONRESIZEWINDOW event with same dimensions %dx%d, ignoring.", static_cast<int>(event.x), static_cast<int>(event.y));
                            #endif
                            break;
                        }
                        #if defined _DEBUG || defined DEBUG
                        WARN_LOG("CORE_MANAGER::onLoop() - ONRESIZEWINDOW event with dimensions %dx%d.", static_cast<int>(event.x), static_cast<int>(event.y));
                        #endif
                        device->setBackBufferSize(event.x, event.y);
                        if (resetDeviceWithNewDimensions(static_cast<int>(event.x), static_cast<int>(event.y)) == false)
                        {
                            // trigger full restore
                            this->forceRestore(doSwapBuffers);
                        }
                        // Update viewport
                        // Update projection and camera
                        device->setProjectionMode(true, event.x, event.y);
                        device->getCamera().updateCam(true, event.x, event.y);
                        this->_updateDimFrustum();

                        // Notify scene and plugins
                        if (SCENE *scene = device->getScene())
                            scene->onResizeWindow();
                        for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
                        {
                            PLUGIN* plugin = this->getPlugin(i);
                            plugin->onResizeWindow(static_cast<int>(event.x), static_cast<int>(event.y));
                        }
                    }
                    break;
                    case ONTOUCHDOWN:
                    {
                        if (SCENE *scene = getInitializedScene())
                            scene->onTouchDown(event.key, event.x, event.y);
                        for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
                        {
                            PLUGIN* plugin = this->getPlugin(i);
                            plugin->onTouchDown(event.key, event.x, event.y);
                        }
                    }
                    break;
                    case ONTOUCHUP:
                    {
                        if (SCENE *scene = getInitializedScene())
                            scene->onTouchUp(event.key, event.x, event.y);
                        for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
                        {
                            PLUGIN* plugin = this->getPlugin(i);
                            plugin->onTouchUp(event.key, event.x, event.y);
                        }
                    }
                    break;
                    case ONTOUCHMOVE:
                    {
                        if (SCENE *scene = getInitializedScene())
                            scene->onTouchMove(event.key, event.x, event.y);
                        for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
                        {
                            PLUGIN* plugin = this->getPlugin(i);
                            plugin->onTouchMove(event.key, event.x, event.y);
                        }
                    }
                    break;
                    case ONTOUCHZOOM:
                    {
                        if (SCENE *scene = getInitializedScene())
                            scene->onTouchZoom((float)event.key);
                        for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
                        {
                            PLUGIN* plugin = this->getPlugin(i);
                            plugin->onTouchZoom((float)event.key);
                        }
                    }
                    break;
                    case ONKEYDOWN:
                    {
                        if (SCENE *scene = getInitializedScene())
                            scene->onKeyDown(event.key);
                        #if defined(_WIN32) || defined(__MINGW32__)
                        if (event.key == VK_CAPITAL)
                        {
                            if ((GetKeyState(VK_CAPITAL) & 0x0001) != 0)
                                this->setKeyCapsLockState(true);
                            else
                                this->setKeyCapsLockState(false);
                        }
                        #elif defined(__linux__) || defined(__APPLE__)
                        //TODO: implement CapsLock state detection for linux and macOS
                        #endif
                        for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
                        {
                            PLUGIN* plugin = this->getPlugin(i);
                            plugin->onKeyDown(event.key);
                        }
                    }
                    break;
                    case ONKEYUP:
                    {
                        if (SCENE *scene = getInitializedScene())
                            scene->onKeyUp(event.key);
                        for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
                        {
                            PLUGIN* plugin = this->getPlugin(i);
                            plugin->onKeyUp(event.key);
                        }
                    }
                    break;
                    case ONDOUBLECLICK:
                    {
                        if (SCENE *scene = getInitializedScene())
                            scene->onDoubleClick(event.x, event.y, event.key);
                        for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
                        {
                            PLUGIN* plugin = this->getPlugin(i);
                            plugin->onDoubleClick(event.x, event.y, event.key);
                        }
                    }
                    break;
                    case ONSTREAMSTOPED: {
                    }
                                       break;
                    case ONCALLBACKCOMMANDS: {
                    }
                    break;
                    case ONKEYDOWNJOYSTICK:
                    {
                        if (SCENE *scene = getInitializedScene())
                            scene->onKeyDownJoystick(event.player, event.key);
                        for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
                        {
                            PLUGIN* plugin = this->getPlugin(i);
                            plugin->onKeyDownJoystick(event.player, event.key);
                        }
                    }
                    break;
                    case ONKEYUPJOYSTICK:
                    {
                        if (SCENE *scene = getInitializedScene())
                            scene->onKeyUpJoystick(event.player, event.key);
                        for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
                        {
                            PLUGIN* plugin = this->getPlugin(i);
                            plugin->onKeyUpJoystick(event.player, event.key);
                        }
                    }
                    break;
                    case ONMOVEJOYSTICK:
                    {
                        if (SCENE *scene = getInitializedScene())
                            scene->onMoveJoystick(event.player, event.lx, event.ly, event.rx, event.ry);
                        for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
                        {
                            PLUGIN* plugin = this->getPlugin(i);
                            plugin->onMoveJoystick(event.player, event.lx, event.ly, event.rx, event.ry);
                        }
                    }
                    break;
                }
                if (!device->isRunning())
                {
                    break;
                }
            }
            //if (loopCallCount <= 3)
            //    INFO_LOG("CORE_MANAGER::onLoop() about to update/render (frame %d) changeScene=%d swapStep=%d",
            //             loopCallCount, (int)this->isChangeScene(), this->device->getSwapBackBufferStep());
            this->update();
            this->render();
            if(doSwapBuffers)// some backend engines need to control when swap buffers is done
            {
                this->swapBuffers();
            }
            if(singleLoop)
            {
                // exit after single loop
                break;
            }
        }
        if(singleLoop == false)
        {
            // Cleanup plugins on exit only if not single loop
            for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
            {
                PLUGIN* plugin = this->getPlugin(i);
                plugin->onDestroy();
            }

            if (device->getAudioManagerInterface())
                device->getAudioManagerInterface()->stopAll();
        }
        return 0;
    }
    
    void CORE_MANAGER::onStopCoreManager()
    {
        DEVICE *device = this->getDevice();
        // Stop the game and all resources, this is called when the device is lost, so we need to release all resources and stop the game, but we do not release the graphics device, so we can restore it later
        impl->wasGamePausedBeforeOnStop = device->isGamePaused();
        device->pauseGame();
        for (auto ptr : device->getRender2DSList())
        {
            ptr->onStop();
        }
        for (auto ptr : device->getRender2DWList())
        {
            ptr->onStop();
        }
        for (auto ptr : device->getRender3DList())
        {
            ptr->onStop();
        }
        TEXTURE_MANAGER::getInstance()->release();
        MESH_MANAGER::getInstance()->release();
        
    }

    void CORE_MANAGER::update()
    {
        DEVICE *device = this->getDevice();
        if (!device->isRunning())
            return;
        device->updateFps();
        const CAMERA &camera = device->getCamera();
        device->setCamera2dScaleCache(1.0f / camera.scale2d.x,
                                      1.0f / camera.scale2d.y);
        this->adjustScaleScreen2d();
        this->logic();
        this->updatePhysis();
        this->updateAudio();
        for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
        {
            PLUGIN* plugin = this->getPlugin(i);
            plugin->onLoop(device->delta);
        }
    }
    
    void CORE_MANAGER::prepareRender2d(std::vector<RENDERIZABLE *> &lsAllObjects2d,
                                std::vector<RENDERIZABLE *> &lsRenderOnFrustum2d)
    {
        const std::vector<RENDERIZABLE*>::size_type total2d = lsAllObjects2d.size();
        for (std::vector<RENDERIZABLE*>::size_type i = 0; i < total2d; ++i)
        {
            RENDERIZABLE *ptr = lsAllObjects2d[i];
            if (ptr)
            {
                ptr->updateAABB();
                if (ptr->isRender2Texture)
                {
                    ptr->isObjectOnFrustum = false;
                }
                else if (!ptr->enableRender)
                {
                    ptr->isObjectOnFrustum = false;
                }
                else if (ptr->alwaysRenderize)
                {
                    ptr->isObjectOnFrustum = true;
                }
                else if (ptr->isOnFrustum())
                {
                    ptr->isObjectOnFrustum = true;
                }
                else
                {
                    ptr->isObjectOnFrustum = false;
                }
                if (ptr->isObjectOnFrustum)
                {
                    lsRenderOnFrustum2d.push_back(ptr);
                    ptr->__distFromView = ptr->position.z;
                }
            }
        }
        std::sort(lsRenderOnFrustum2d.begin(), lsRenderOnFrustum2d.end(),
                  [](const RENDERIZABLE *a, const RENDERIZABLE *b) { return b->__distFromView < a->__distFromView; });
    }
    
    void CORE_MANAGER::prepareRender3d(std::vector<RENDERIZABLE *> &lsAllObjects3d,
                                std::vector<RENDERIZABLE *> &lsRenderOnFrustum3d)
    {
        mbm::DEVICE *      device  = mbm::DEVICE::getInstance();
        const std::vector<RENDERIZABLE*>::size_type total3d = lsAllObjects3d.size();
        for (std::vector<RENDERIZABLE*>::size_type i = 0; i < total3d; ++i)
        {
            RENDERIZABLE *ptr = lsAllObjects3d[i];
            if (ptr)
            {
                ptr->updateAABB();
                if (ptr->isRender2Texture)
                {
                    ptr->isObjectOnFrustum = false;
                }
                else if (!ptr->enableRender)
                {
                    ptr->isObjectOnFrustum = false;
                }
                else if (ptr->alwaysRenderize)
                {
                    ptr->isObjectOnFrustum = true;
                }
                else if (ptr->isOnFrustum())
                {
                    ptr->isObjectOnFrustum = true;
                }
                else
                {
                    ptr->isObjectOnFrustum = false;
                }
                if (ptr->isObjectOnFrustum)
                {
                    lsRenderOnFrustum3d.push_back(ptr);
                    const VEC3 distFromCam(ptr->position - device->getCamera().position);
                    ptr->__distFromView = distFromCam.length();
                }
            }
        }
        std::sort(lsRenderOnFrustum3d.begin(), lsRenderOnFrustum3d.end(),
                  [](const RENDERIZABLE *a, const RENDERIZABLE *b) { return b->__distFromView < a->__distFromView; });
    }

    void CORE_MANAGER::render()
    {
        if (!device)
            return;
        if (!device->isRunning())
            return;
        std::vector<RENDERIZABLE *> lsRender2ds;
        std::vector<RENDERIZABLE *> lsRender2dw;
        std::vector<RENDERIZABLE *> lsRender3d;
        // Atualiza a camera de acordo com a
        // projeção----
        device->setProjectionMode(true, device->getBackBufferWidth(), device->getBackBufferHeight());
        // prepara para renderizar os objeto --
        device->setTotalObjectsIsRendering3D(0);
        device->setTotalObjectsOnFrustum3D(0);
        auto &render3DList                = device->getRender3DList();
        device->setTotalObjects3D(static_cast<uint32_t>(render3DList.size()));
        device->setTotalObjectsIsRendering2D(0);
        device->setTotalObjectsOnFrustum2D(0);
        auto &render2DSList               = device->getRender2DSList();
        const auto total2ds       = static_cast<uint32_t>(render2DSList.size());
        auto &render2DWList               = device->getRender2DWList();
        const auto total2dw       = static_cast<uint32_t>(render2DWList.size());
        device->setTotalObjects2D(total2ds + total2dw);

#if defined USE_THREAD
        std::thread thread2ds(prepareRender2d, std::ref(render2DSList), std::ref(lsRender2ds));
        std::thread thread2dw(prepareRender2d, std::ref(render2DWList), std::ref(lsRender2dw));
        std::thread thread3d(prepareRender3d, std::ref(render3DList), std::ref(lsRender3d));
        if (thread2ds.joinable())
            thread2ds.join();
        if (thread2dw.joinable())
            thread2dw.join();
        if (thread3d.joinable())
            thread3d.join();
#else
        prepareRender2d(std::ref(render2DSList), std::ref(lsRender2ds)); //-V525
        prepareRender2d(std::ref(render2DWList), std::ref(lsRender2dw));
        prepareRender3d(std::ref(render3DList), std::ref(lsRender3d));
#endif

        device->setTotalObjectsOnFrustum2D(static_cast<uint32_t>(lsRender2ds.size() + lsRender2dw.size()));
        device->setTotalObjectsOnFrustum3D(static_cast<uint32_t>(lsRender3d.size()));
        
        if (!this->renderToTargets())
            return;
        
        if (device->isClearBackGroundEnabled())
        {
            device->clearDepthColored();
        }
        CAMERA &camera = device->getCamera();
        device->updateFrustum(&camera.matrixView, &camera.matrixProj);
        camera.updateNormalsRelativeCam();
        camera.calculateAzimuthFromCamera();
        camera.matrixBillboard = camera.matrixView; // Obtemos a Matrix De Vista Do Vista 3D
        MatrixInverse(&camera.matrixBillboard, nullptr, &camera.matrixBillboard);
        device->setTotalObjectsIsRendering3D(0);
        if (this->beginRender())
        {
            for (auto ptrRender : lsRender3d)
            {
                if (ptrRender->render())
                    device->incrementTotalObjectsIsRendering3D();
            }

            device->setProjectionMode(false, device->getBackBufferWidth(), device->getBackBufferHeight());
            device->setTotalObjectsIsRendering2D(0);
            // Clear the depth buffer so 3D perspective depth values do not occlude 2dw
            // objects whose depth comes from the orthographic projection. On Metal this
            // ends the current command encoder and starts a new one that preserves the
            // colour attachment (3D scene) while clearing depth to 1.0.
            device->clearDepth();
            device->setDepthTest(true);
            for (auto ptrRender : lsRender2dw)
            {
                if (ptrRender->render())
                    device->incrementTotalObjectsIsRendering2D();
            }
            device->setDepthTest(false);
            for (auto ptrRender : lsRender2ds)
            {
                if (ptrRender->render())
                    device->incrementTotalObjectsIsRendering2D();
            }
            device->setDepthTest(true);

            for (unsigned int i = 0; i < this->getTotalPlugins(); ++i)
            {
                PLUGIN* plugin = this->getPlugin(i);
                plugin->onRender();
            }
            this->endRender();
        }
    }

    void CORE_MANAGER::_updateDimFrustum()
    {
        DEVICE *device = this->getDevice();
        VEC3 point(0, 0, 50);
        CAMERA &camera = device->getCamera();
        device->setNearFrustumDimension(VEC3(0, 0, 20));
        device->setFarFrustumDimension(VEC3(0, 0, 980));
        camera.updateCam(true, device->getBackBufferWidth(), device->getBackBufferHeight());
        device->updateFrustum(&camera.matrixView, &camera.matrixProj);
        while (device->isPointAtTheFrustum(point))
        {
            point.x += 0.5f;
        }
        device->setNearFrustumDimensionX(point.x * 2.0f);

        point = VEC3(0, 0, 50);
        while (device->isPointAtTheFrustum(point))
        {
            point.y += 0.5f;
        }
        device->setNearFrustumDimensionY(point.y * 2.0f);

        point = VEC3(0, 0, 980);
        while (device->isPointAtTheFrustum(point))
        {
            point.x += 0.5f;
        }
        device->setFarFrustumDimensionX(point.x * 2.0f);

        point = VEC3(0, 0, 980);
        while (device->isPointAtTheFrustum(point))
        {
            point.y += 0.5f;
        }
        device->setFarFrustumDimensionY(point.y * 2.0f);
    }
    
    void CORE_MANAGER::adjustScaleScreen2d()
    {
        DEVICE *device = this->getDevice();
        CAMERA &camera = device->getCamera();
        if (camera.expectedScreen.x != 0.0f && camera.expectedScreen.y != 0.0f) //-V550
        {
            const float percx = device->getBackBufferWidth() / camera.expectedScreen.x;
            const float percy = device->getBackBufferHeight() / camera.expectedScreen.y;
            if (percx != 0.0f && percy != 0.0f) //-V550
            {
                if (camera.stretch[0])
                {
                    if (strcmp(camera.stretch, "x") == 0)
                    {
                        camera.scaleScreen2d.x = percx;
                        camera.scaleScreen2d.y = percx;
                        camera.scale2d.x = percx;
                        camera.scale2d.y = percx;
                    }
                    else if (strcmp(camera.stretch, "y") == 0)
                    {
                        camera.scaleScreen2d.x = percy;
                        camera.scaleScreen2d.y = percy;
                        camera.scale2d.x = percy;
                        camera.scale2d.y = percy;
                    }
                    else if (strcmp(camera.stretch, "xy") == 0)
                    {
                        camera.scaleScreen2d.x = percx;
                        camera.scaleScreen2d.y = percy;
                        camera.scale2d.x = percx;
                        camera.scale2d.y = percy;
                    }
                    else if (percx < percy)
                    {
                        camera.scaleScreen2d.x = percx;
                        camera.scaleScreen2d.y = percx;
                        camera.scale2d.x = percx;
                        camera.scale2d.y = percx;
                    }
                    else
                    {
                        camera.scaleScreen2d.x = percy;
                        camera.scaleScreen2d.y = percy;
                        camera.scale2d.x = percy;
                        camera.scale2d.y = percy;
                    }
                }
                else if (percx < percy)
                {
                    camera.scaleScreen2d.x = percx;
                    camera.scaleScreen2d.y = percx;
                    camera.scale2d.x = percx;
                    camera.scale2d.y = percx;
                }
                else
                {
                    camera.scaleScreen2d.x = percy;
                    camera.scaleScreen2d.y = percy;
                    camera.scale2d.x = percy;
                    camera.scale2d.y = percy;
                }
            }
        }
    }
    
    void CORE_MANAGER::updateAudio()
    {
        DEVICE *device = this->getDevice();
        if(device->getAudioManagerInterface())
            device->getAudioManagerInterface()->update(this, device->getScene()->getIdScene());
    }
    
    void CORE_MANAGER::updatePhysis()
    {
        DEVICE *device = this->getDevice();
        SCENE *scene = device->getScene();
        if (!scene)
            return;
        const float        fps            = device->delta == 0.0f ? 0.0f : device->fps; //-V550
        const int          idCurrentScene = scene->getIdScene();
        const uint32_t     s              = device->getTotalPhysics();
        for (uint32_t i = 0; i < s; ++i)
        {
            PHYSICS *ptr = device->getPhysics(i);
            if (ptr && ptr->enablePhysics && ptr->idScene == idCurrentScene)
            {
                ptr->update(fps, device->delta);
            }
        }
    }
    
    void CORE_MANAGER::initEnableRenders()
    {
        DEVICE *device = this->getDevice();
        for (auto ptr : device->getRender3DList())
        {
            if (ptr != nullptr)
            {
                ptr->enableRender = false;
            }
        }
        for (auto ptr : device->getRender2DSList())
        {
            if (ptr != nullptr)
            {
                ptr->enableRender = false;
            }
        }
        for (auto ptr : device->getRender2DWList())
        {
            if (ptr != nullptr)
            {
                ptr->enableRender = false;
            }
        }
    }
    
    void CORE_MANAGER::logic()
    {
        DEVICE *device = this->getDevice();
        SCENE *scene = device->getScene();
        if (scene != nullptr)
        {
            if (scene->endScene)
            {
                scene->onFinalizeScene();
                scene->wasUnloadedScene = true;
                disableRender(scene->getIdScene());
                for(unsigned int i=0; i < this->getTotalPlugins(); ++i)
                {
                    PLUGIN * plugin = this->getPlugin(i);
                    plugin->onDestroy();
                }
                this->clearPlugins();
                if (scene->goToNextScene && scene->nextScene == nullptr)
                {
                    device->setRun(false);
                    device->setClearBackGround(false);
                }
                else
                {
                    if (scene->goToNextScene)
                    {
                        device->setScene(scene->nextScene);
                        scene = device->getScene();
                    }
                    if(scene)
                        scene->endScene = false;
                    this->setChangeScene(true);
                    device->setClearBackGround(true);
                    if(scene)
                        scene->startLoading();
                }
                this->setSceneInitialized(false);
            }
            else if (this->isChangeScene())
            {
                #if defined _DEBUG || defined DEBUG
                INFO_LOG("CORE_MANAGER::logic() changeScene=true swapStep=%d", device->getSwapBackBufferStep());
                #endif
                if (device->getSwapBackBufferStep() == 3)
                {
                    #if defined _DEBUG || defined DEBUG
                    INFO_LOG("CORE_MANAGER::logic() calling scene->init()");
                    #endif
                    this->reinitTimers();
                    enableRender(scene->getIdScene());
                    scene->wasUnloadedScene = false;
                    device->getOrderRender().reInit();
                    scene->onInitScene();
                    device->setFakeFps(120,60);
                    device->resumeTimer();
                    this->setSceneInitialized(true);
                    this->setChangeScene(false);
                    device->setClearBackGround(true);
                    if(scene)
                        scene->endLoading();
                }
                else
                {
                    device->setClearBackGround(false);
                    device->incrementSwapBackBufferStep();
                }
            }
            else
            {
                scene->onLoop();
            }
        }
    }
    
    void CORE_MANAGER::reinitTimers()
    {
        DEVICE *device = this->getDevice();
        device->clearAdditionalTimers();
        device->resumeTimer();
    }

    void CORE_MANAGER::enableRender(const int idScene)
    {
        DEVICE *device = this->getDevice();
        for (auto ptr : device->getRender3DList())
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = true;
            }
        }
        for (auto ptr : device->getRender2DSList())
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = true;
            }
        }
        for (auto ptr : device->getRender2DWList())
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = true;
            }
        }
    }
    
    void CORE_MANAGER::disableRender(const int idScene)
    {
        DEVICE *device = this->getDevice();
        for (auto ptr : device->getRender3DList())
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = false;
            }
        }
        for (auto ptr : device->getRender2DSList())
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = false;
            }
        }
        for (auto ptr : device->getRender2DWList())
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = false;
            }
        }
    }

    #if !defined(ANDROID) && !defined(MBM_PLATFORM_IOS)
    void CORE_MANAGER::execute_system_cmd_thread(const char* command)//execute system command in other thread
    {
        auto fNextThreadName = []() -> std::string
        {
            static int iNumThread = 0;
            std::string name("__thread_");
            name += std::to_string(++iNumThread);
            return name;
        };
#if (defined(__linux__) || (defined(__APPLE__) && !defined(USE_METAL))) && !defined(USE_DUMMY_BACK_END_ENGINE)
        // Use posix_spawn instead of system() to avoid fork() races in multi-threaded X11 programs.
        // system() calls fork() which duplicates the X11 socket fd into the child, creating a brief
        // window where both parent and child share the same connection. On Linux, posix_spawn uses
        // clone(CLONE_VM|CLONE_VFORK) which suspends the parent until the child exec's, eliminating
        // the race. We also explicitly close the X11 fd in the child via file actions.
        const int x11_fd = getX11DisplayFd();
        auto fExecute = [](std::string cmd, int fd) -> void
        {
            posix_spawn_file_actions_t file_actions;
            posix_spawnattr_t          attr;
            posix_spawn_file_actions_init(&file_actions);
            posix_spawnattr_init(&attr);
            if (fd >= 0)
                posix_spawn_file_actions_addclose(&file_actions, fd);
#ifdef POSIX_SPAWN_SETSID
            posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETSID);
#endif
            const char* argv[] = { "sh", "-c", cmd.c_str(), nullptr };
            pid_t pid = 0;
            posix_spawnp(&pid, "sh", &file_actions, &attr,
                         const_cast<char**>(argv), environ);
            posix_spawn_file_actions_destroy(&file_actions);
            posix_spawnattr_destroy(&attr);
            // Do NOT waitpid — the shell command uses & to detach the game process
        };
        mbm::DEVICE* device              = mbm::DEVICE::getInstance();
        std::string  name                = fNextThreadName();
        // Copy command into the thread by value to avoid race with the next call
        std::thread* exec_thread         = new std::thread(fExecute, std::string(command), x11_fd);
#elif defined(_WIN32)
        // Use CreateProcessA instead of system() to bypass cmd.exe entirely.
        // system() routes through "cmd.exe /c <command>", and cmd.exe strips the
        // first and last double-quote from the command when both are present,
        // mangling quoted executable paths. CreateProcessA parses the command
        // line directly: the first (possibly quoted) token is the executable and
        // the rest are its arguments.
        auto fExecute = [] (std::string cmd) -> void
        {
            STARTUPINFOA si{};
            PROCESS_INFORMATION pi{};
            si.cb = sizeof(si);
            if (CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE,
                               CREATE_NEW_CONSOLE, nullptr, nullptr, &si, &pi))
            {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
            else
            {
                ERROR_LOG("CreateProcessA failed (error %lu) for: %s", GetLastError(), cmd.c_str());
            }
        };
        mbm::DEVICE* device              = mbm::DEVICE::getInstance();
        std::string  name                = fNextThreadName();
        std::thread* exec_thread         = new std::thread(fExecute, std::string(command));
#else
        auto fExecute = [] (std::string cmd) -> void
        {
            system(cmd.c_str());
        };
        mbm::DEVICE* device              = mbm::DEVICE::getInstance();
        std::string  name                = fNextThreadName();
        std::thread* exec_thread         = new std::thread(fExecute, std::string(command));
#endif
        DYNAMIC_VAR* dyVar               = new DYNAMIC_VAR(DYNAMIC_REF,static_cast<const void*>(exec_thread));
        device->getDynamicVars()[name] = dyVar;
    }
    #endif

    bool CORE_MANAGER::onLostDevice(const bool doSwapBuffers, int width, int height, const int px, const int py)
    {
        DEVICE *device = this->getDevice();
        if (impl->stepRestore == STEP_RES_INIT_GL)
        {
#if defined _DEBUG
            WARN_LOG("onLostDevice step %d", impl->stepRestore);
#endif
            // Save 2D scaling state
            CAMERA &camera = device->getCamera();
            const VEC2 expectedScreenBefore = camera.expectedScreen;
            char stretchBefore[sizeof(camera.stretch)] = {};
            strncpy(stretchBefore, camera.stretch, sizeof(stretchBefore) - 1);

            constexpr bool wasLostDevice = true;
            this->ReleaseGraphics(wasLostDevice);

            if (initGraphics(this->getNameApplication(), width, height, px, py, this->getWindowBorder(), this->getEnableResizeWindow()))
            {
                // Reapply previous 2D scaling
                camera.expectedScreen = expectedScreenBefore;
                device->scaleToScreen(expectedScreenBefore.x, expectedScreenBefore.y, stretchBefore);

#if defined _DEBUG
                WARN_LOG("After restore - scale2d.x: %f, scale2d.y: %f, expectedScreen.x: %f, expectedScreen.y: %f",
                    camera.scale2d.x,
                    camera.scale2d.y,
                    camera.expectedScreen.x,
                    camera.expectedScreen.y);
#endif

                device->setCamera2dScaleCache(1.0f / camera.scale2d.x,
                                              1.0f / camera.scale2d.y);
                this->adjustScaleScreen2d();

                impl->stepRestore = STEP_RES_DRAW_HOURGLASS;
                return false;
            }
            else
            {
#if defined _DEBUG
                WARN_LOG("onLostDevice step %d function initGraphics failed!", impl->stepRestore);
#endif
                return false;
            }
        }
        else if (impl->stepRestore == STEP_RES_DRAW_HOURGLASS)
        {
#if defined _DEBUG
            WARN_LOG("onLostDevice step %d draw Hourglass.", impl->stepRestore);
#endif
            if (this->beginRender())
            {
                device->setProjectionMode(false, device->getBackBufferWidth(), device->getBackBufferHeight());
                device->setDepthTest(false);
                device->clearDepthColored();
                if (device->getScene())
                    device->getScene()->onRestore(0); //true means: no call restore,  just to prepare the screen.
                impl->stepRestore = STEP_RES_OBJ;
                impl->whichFor = WFOR_INITIAL;
                this->endRender();
                if(doSwapBuffers)
                {
                    this->swapBuffers();
                }
            }
            return false;
        }
        else if (impl->stepRestore == STEP_RES_OBJ)
        {
            device->setProjectionMode(false, device->getBackBufferWidth(), device->getBackBufferHeight());
            device->setDepthTest(false);
            device->clearDepthColored();
            switch (impl->whichFor)
            {
            case WFOR_INITIAL:
            {
#if defined _DEBUG
                WARN_LOG("onLostDevice step %d restoring objs.", impl->stepRestore);
#endif
                const auto t = static_cast<float>(device->getRender2DWList().size() + device->getRender2DSList().size() + device->getRender3DList().size());
                if (t > 0.0f)
                {
                    impl->totalForByLoop = static_cast<uint32_t>(std::ceil(t / 60.0f));//1 seconds should be loaded all objects
                    impl->stepRestoreInfo = 98.0f / t * static_cast<float>(impl->totalForByLoop);
                }
                else
                {
                    impl->stepRestoreInfo = 0.001f;
                    impl->totalForByLoop = 1;
                }
                impl->percentRestoreInfo = 0.0f;
                impl->whichFor = WFOR_2DW;
                impl->indexOnRestore = 0;
                return false;
            }
            break;
            case WFOR_2DW:
            {
                if (this->beginRender())
                {
                    auto &render2DWList = device->getRender2DWList();
                    for (uint32_t i = impl->indexOnRestore, j = 0; i < render2DWList.size(); ++i)
                    {
                        RENDERIZABLE* ptr = render2DWList[i];
                        const bool    alwaysRenderize = ptr->alwaysRenderize;
                        const bool    enableRender = ptr->enableRender;
                        ptr->alwaysRenderize = false;
                        ptr->enableRender = false;
                        //position and angle can be reloaded from MESH_MBM::loadImpl, so we need to save them before call onRestoreDevice and restore after that, to avoid objects be moved or rotated to wrong position/angle after restore
                        const VEC3 positionBefore = ptr->position;
                        const VEC3 angleBefore = ptr->angle;
                        if (ptr->onRestoreDevice())
                        {
                            ptr->alwaysRenderize = alwaysRenderize;
                            ptr->enableRender = enableRender;
                            ptr->onRestoreAnimationsState();
                        }
                        ptr->position = positionBefore;
                        ptr->angle = angleBefore;
                        impl->indexOnRestore = (i + 1);
                        if (++j >= impl->totalForByLoop)
                        {
                            impl->percentRestoreInfo += impl->stepRestoreInfo;
                            if (device->getScene())
                                device->getScene()->onRestore(static_cast<int>(std::ceil(impl->percentRestoreInfo > 98.9f ? 98.9f : impl->percentRestoreInfo)));
                            break;
                        }
                    }
                    this->endRender();
                    if(doSwapBuffers)
                    {
                        this->swapBuffers();
                    }
                    if (impl->indexOnRestore >= device->getRender2DWList().size())
                    {
                        impl->indexOnRestore = 0;
                        impl->whichFor = WFOR_2DS;
                    }
                }
                return false;
            }
            break;
            case WFOR_2DS:
            {
                if (this->beginRender())
                {
                    auto &render2DSList = device->getRender2DSList();
                    for (uint32_t i = impl->indexOnRestore, j = 0; i < render2DSList.size(); ++i)
                    {
                        RENDERIZABLE* ptr = render2DSList[i];
                        const bool    alwaysRenderize = ptr->alwaysRenderize;
                        const bool    enableRender = ptr->enableRender;
                        ptr->alwaysRenderize = false;
                        ptr->enableRender = false;
                        //position and angle can be reloaded from MESH_MBM::loadImpl, so we need to save them before call onRestoreDevice and restore after that, to avoid objects be moved or rotated to wrong position/angle after restore
                        const VEC3 positionBefore = ptr->position;
                        const VEC3 angleBefore = ptr->angle;
                        if (ptr->onRestoreDevice())
                        {
                            ptr->alwaysRenderize = alwaysRenderize;
                            ptr->enableRender = enableRender;
                            ptr->onRestoreAnimationsState();
                        }
                        ptr->position = positionBefore;
                        ptr->angle = angleBefore;
                        impl->indexOnRestore = (i + 1);
                        if (++j >= impl->totalForByLoop)
                        {
                            impl->percentRestoreInfo += impl->stepRestoreInfo;
                            if (device->getScene())
                                device->getScene()->onRestore(static_cast<int>(std::ceil(impl->percentRestoreInfo > 98.9f ? 98.9f : impl->percentRestoreInfo)));
                            break;
                        }
                    }
                    this->endRender();
                    if(doSwapBuffers)
                    {
                        this->swapBuffers();
                    }
                    if (impl->indexOnRestore >= device->getRender2DSList().size())
                    {
                        impl->indexOnRestore = 0;
                        impl->whichFor = WFOR_3D;
                    }
                }
                return false;
            }
            break;
            case WFOR_3D:
            {
                if (this->beginRender())
                {
                    auto &render3DList = device->getRender3DList();
                    for (uint32_t i = impl->indexOnRestore, j = 0; i < render3DList.size(); ++i)
                    {
                        RENDERIZABLE* ptr = render3DList[i];
                        const bool    alwaysRenderize = ptr->alwaysRenderize;
                        const bool    enableRender = ptr->enableRender;
                        ptr->alwaysRenderize = false;
                        ptr->enableRender = false;
                        //position and angle can be reloaded from MESH_MBM::loadImpl, so we need to save them before call onRestoreDevice and restore after that, to avoid objects be moved or rotated to wrong position/angle after restore
                        const VEC3 positionBefore = ptr->position;
                        const VEC3 angleBefore = ptr->angle;
                        if (ptr->onRestoreDevice())
                        {
                            ptr->alwaysRenderize = alwaysRenderize;
                            ptr->enableRender = enableRender;
                            ptr->onRestoreAnimationsState();
                        }
                        ptr->position = positionBefore;
                        ptr->angle = angleBefore;
                        impl->indexOnRestore = (i + 1);
                        if (++j >= impl->totalForByLoop)
                        {
                            impl->percentRestoreInfo += impl->stepRestoreInfo;
                            if (device->getScene())
                                device->getScene()->onRestore(static_cast<int>(std::ceil(impl->percentRestoreInfo > 98.9f ? 98.9f : impl->percentRestoreInfo)));
                            break;
                        }
                    }
                    this->endRender();
                    if(doSwapBuffers)
                    {
                        this->swapBuffers();
                    }
                    if (impl->indexOnRestore >= device->getRender3DList().size())
                    {
                        impl->indexOnRestore = 0;
                        impl->whichFor = WFOR_DONE;
                    }
                    return false;
                }
            }
            break;
            default: {};
            }
            impl->stepRestore = STEP_RES_END;
            return false;
        }
        else if (impl->stepRestore == STEP_RES_END)
        {
#if defined _DEBUG
            WARN_LOG("onLostDevice step %d resumeGame", impl->stepRestore);
#endif
            impl->stepRestore = STEP_RES_INIT_GL;
            device->setClearBackGround(true);
            if(impl->wasGamePausedBeforeOnStop == false)
                device->resumeGame();
            if (device->getScene())
                device->getScene()->onRestore(100);
            return true;
        }
        return false;
    }

     void CORE_MANAGER::forceRestore(const bool doSwapBuffers)
    {
        // Call onStop and forceRestore to ensure all resources are reloaded
        DEVICE *device = this->getDevice();
        this->onStopCoreManager();
        while (!this->onLostDevice(doSwapBuffers, 
            static_cast<int>(device->getBackBufferWidth()),
            static_cast<int>(device->getBackBufferHeight()),
            device->getWindowPositionX(),
            device->getWindowPositionY()));
    }

     void CORE_MANAGER::onTouchDown(int key, float x, float y)
     {
         DEVICE *device = this->getDevice();
         const CAMERA &camera = device->getCamera();
         x /= camera.scale2d.x;
         y /= camera.scale2d.y;
         EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONTOUCHDOWN);
         this->pushEvent(&ev);
     }

     void CORE_MANAGER::onTouchUp(int key, float x, float y)
     {
         DEVICE *device = this->getDevice();
         const CAMERA &camera = device->getCamera();
         x /= camera.scale2d.x;
         y /= camera.scale2d.y;
         EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONTOUCHUP);
         this->pushEvent(&ev);
     }

     void CORE_MANAGER::onTouchMove(int key, float x, float y)
     {
         DEVICE *device = this->getDevice();
         const CAMERA &camera = device->getCamera();
         x /= camera.scale2d.x;
         y /= camera.scale2d.y;
         EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONTOUCHMOVE);
         this->pushEvent(&ev);
     }

     void CORE_MANAGER::onTouchZoom(float zoom) // Evento chamado ao solicitar zoom. Zoom estes normalmente com movimentos dos dedos. É
         // enviados valores entre -1 e +1. No caso de mouse é o scrool do mesmo.
     {
         EVENT_KEY ev(0, 0, (int)zoom, EVENT_TYPE_ACTIONS::ONTOUCHZOOM);
         this->pushEvent(&ev);
     }

     void CORE_MANAGER::onKeyDown(int key) // Evento chamado ao pressionar uma tecla na janela ativa. key é um VK padrão da api do Windows.
     {
         EVENT_KEY ev(0, 0, key, EVENT_TYPE_ACTIONS::ONKEYDOWN);
         this->pushEvent(&ev);
     }

     void CORE_MANAGER::onKeyUp(int key) // Evento chamado ao pressionar uma tecla na janela ativa. key é um VK padrão da api do Windows.
     {
         EVENT_KEY ev(0, 0, key, EVENT_TYPE_ACTIONS::ONKEYUP);
         this->pushEvent(&ev);
     }

     void CORE_MANAGER::onDoubleClick(float x, float y, int key)
     {
         DEVICE *device = this->getDevice();
         const CAMERA &camera = device->getCamera();
         x /= camera.scale2d.x;
         y /= camera.scale2d.y;
         EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONDOUBLECLICK);
         this->pushEvent(&ev);
     }

     void CORE_MANAGER::onKeyDownJoystick(int player, int key)
     {
         EVENT_KEY ev(0.0f, 0.0f, key, player, 0.0f, 0.0f, EVENT_TYPE_ACTIONS::ONKEYDOWNJOYSTICK);
         this->pushEvent(&ev);
     }

     void CORE_MANAGER::onKeyUpJoystick(int player, int key)
     {
         EVENT_KEY ev(0.0f, 0.0f, key, player, 0.0f, 0.0f, EVENT_TYPE_ACTIONS::ONKEYUPJOYSTICK);
         this->pushEvent(&ev);
     }

     void CORE_MANAGER::onMoveJoystick(int player, float lx, float ly, float rx, float ry)
     {
         static const float pProp_128 = 1.0f / 128.f;
         static const float pProp_127 = 1.0f / 127.f;
         const float        flx = lx > 0 ? lx * pProp_127 : lx * pProp_128;
         const float        fly = ly > 0 ? ly * pProp_127 : ly * pProp_128;
         const float        frx = rx > 0 ? rx * pProp_127 : rx * pProp_128;
         const float        fry = ry > 0 ? ry * pProp_127 : ry * pProp_128;
         EVENT_KEY          ev(flx, fly, 0, player, frx, fry, EVENT_TYPE_ACTIONS::ONMOVEJOYSTICK);
         this->pushEvent(&ev);
     }

     void CORE_MANAGER::onInfoDeviceJoystick(int player, int maxNumberButton, const char* strDeviceName, const char* extraInfo)
     {
         INFO_JOYSTICK_INIT_PLAYER ev(player, maxNumberButton, strDeviceName, extraInfo);
         this->pushEvent(&ev);
     }

     void CORE_MANAGER::onResizeWindow(int width, int height)
     {
         EVENT_KEY ev(static_cast<float>(width), static_cast<float>(height), 0, EVENT_TYPE_ACTIONS::ONRESIZEWINDOW);
         this->pushEvent(&ev);
     }

     void CORE_MANAGER::onMoveWindow(int x, int y)
     {
         EVENT_KEY ev(static_cast<float>(x), static_cast<float>(y), 0, EVENT_TYPE_ACTIONS::ONMOVEWINDOW);
         this->pushEvent(&ev);
     }
     
     void CORE_MANAGER::pushEvent(EVENT_KEY* event)
     {
         DEVICE *device = this->getDevice();
         if (event && device->getScene() && this->isSceneInitialized())
         {
             impl->mutexEvents.lock();
             if (event->eventType == impl->lastEvent.eventType)
             {
                 switch (event->eventType)
                 {
                    case UNKNOWN: 
                    {
                        impl->mutexEvents.unlock();
                        return;
                    }
                    break;
                    case ONRESIZEWINDOW:
                    {
                        EVENT_KEY* event_onresize = impl->events.size() > 0 ? &impl->events.back() : nullptr;
                        if (event_onresize)
                        {
                            //update resize event only
                            *event_onresize = *event;
                        }
                        impl->mutexEvents.unlock();
                        return;
                    }
                    break;
                    case ONMOVEWINDOW:
                    {
                        EVENT_KEY* event_onmove = impl->events.size() > 0 ? &impl->events.back() : nullptr;
                        if (event_onmove)
                        {
                            //update move event only
                            *event_onmove = *event;
                        }
                        impl->mutexEvents.unlock();
                        return;
                    }
                    break;
                    case ONTOUCHMOVE:
                    {
                        if (event->key == impl->lastEvent.key &&
                            (static_cast<int>(event->x) == static_cast<int>(impl->lastEvent.x) &&
                            static_cast<int>(event->y) == static_cast<int>(impl->lastEvent.y)))
                        {
                            //ignore move event with same position only
                            impl->mutexEvents.unlock();
                            return;
                        }
                    }
                    break;
                    case ONTOUCHDOWN:
                    case ONTOUCHUP:
                    case ONDOUBLECLICK:
                    {
                        if (event->key == impl->lastEvent.key)
                        {
                            EVENT_KEY* event_touch = impl->events.size() > 0 ? &impl->events.back() : nullptr;
                            if (event_touch)
                            {
                                *event_touch = *event;
                            }
                            impl->mutexEvents.unlock();
                            return;
                        }
                    }
                    break;
                    case ONKEYDOWN:
                    case ONKEYUP:
                    {
                        if (event->key == impl->lastEvent.key)
                        {
                            impl->mutexEvents.unlock();
                            return;
                        }
                    }
                    break;
                    case ONTOUCHZOOM: 
                    {
                        if (event->key == impl->lastEvent.key)
                        {
                            EVENT_KEY* event_zoom = impl->events.size() > 0 ? &impl->events.back() : nullptr;
                            if (event_zoom)
                            {
                                *event_zoom = *event;
                            }
                            else
                            {
                                // Queue empty: coalescing would drop the event. Add it instead.
                                impl->lastEvent = *event;
                                impl->events.push_back(*event);
                            }
                            impl->mutexEvents.unlock();
                            return;
                        }
                    }
                    break;
                    default: 
                    {
                    }
                    break;
                  }
             }
             impl->lastEvent = *event;

             switch (event->eventType)
             {
                case ONKEYDOWN:
                {
                    if (impl->keyPressed[event->key] == false)
                        impl->events.push_back(*event);
                    impl->keyPressed[event->key] = true;
                }
                break;
                case ONKEYUP:
                {
                    if (impl->keyPressed[event->key])
                        impl->events.push_back(*event);
                    impl->keyPressed[event->key] = false;
                }
                break;
                case ONRESIZEWINDOW:
                {
                    impl->events.push_back(*event);
                }
                break;
                default: 
                {
                    impl->events.push_back(*event);
                }
                break;
             }
             impl->mutexEvents.unlock();
         }
     }

     bool CORE_MANAGER::popEvent(EVENT_KEY* event)
     {
         impl->mutexEvents.lock();
         if (impl->events.size() > 0 && event)
         {
             *event = impl->events.front();
             impl->events.pop_front();
             impl->mutexEvents.unlock();
             return true;
         }
         else
         {
             impl->mutexEvents.unlock();
             return false;
         }
     }

     void CORE_MANAGER::pushEvent(INFO_JOYSTICK_INIT_PLAYER* info)
     {
         impl->mutexEvents.lock();
         DEVICE *device = this->getDevice();
         if (device->getScene() && this->isSceneInitialized())
         {
             impl->joystickInfoEvents.push_back(*info);
         }
         impl->mutexEvents.unlock();
     }

     bool CORE_MANAGER::popEvent(INFO_JOYSTICK_INIT_PLAYER* info)
     {
         impl->mutexEvents.lock();
         if (impl->joystickInfoEvents.size() > 0 && info)
         {

             *info = impl->joystickInfoEvents.front();
             impl->joystickInfoEvents.pop_front();
             impl->mutexEvents.unlock();
             return true;
         }
         else
         {
             impl->mutexEvents.unlock();
             return false;
         }
     }

}
