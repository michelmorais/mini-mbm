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
#include <thread>

namespace mbm
{
    void CORE_MANAGER::setUsageOfDefaultPS_VS_WhenNoShader(const bool _useDeafultPSwhenNoPsShader, const bool _useDeafultVSwhenNoVSShader) noexcept
    {
        _setUsageOfDefaultPS_VS_WhenNoShader(_useDeafultPSwhenNoPsShader, _useDeafultVSwhenNoVSShader);
    }

    void CORE_MANAGER::setScene(SCENE *currentScene)
    {
        this->device->scene = currentScene;
    }

    int CORE_MANAGER::loop(const bool singleLoop, const bool doSwapBuffers)
    {
        //static int loopCallCount = 0;
        //loopCallCount++;
        //if (loopCallCount <= 3)
        //    INFO_LOG("CORE_MANAGER::loop() call #%d singleLoop=%d doSwap=%d device=%p scene=%p",
        //             loopCallCount, (int)singleLoop, (int)doSwapBuffers, (void*)device,
        //             device ? (void*)device->scene : nullptr);
        if (!device)
            return -1;
        if (!this->loopVariablesInitialized)
        {
            INFO_LOG("CORE_MANAGER::loop() first-time init");
            // Cfg shader from resource----
            if (!this->device->cfg.parserCFGFromResource())
            {
                ERROR_LOG("CORE_MANAGER::loop() parserCFGFromResource FAILED");
                return -1;
            }
            this->device->cfg.sortShader();
            device->setProjectionMode(true, device->backBufferWidth, device->backBufferHeight);
            this->device->updateFps();
            initEnableRenders();
            this->_updateDimFrustum();
            this->loopVariablesInitialized = true;
            this->device->camera.expectedScreen.x = this->device->backBufferWidth;
            this->device->camera.expectedScreen.y = this->device->backBufferHeight;
        }
        while (device->run)
        {
            handleEventFromWindow();
            for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
            {
                PLUGIN* plugin = this->lsPlugins[i];
                plugin->onPrepare();
            }

            INFO_JOYSTICK_INIT_PLAYER info;
            while (this->popEvent(&info))
            {
                if (this->device->scene && this->__sceneWasInit)
                    this->device->scene->onInfoDeviceJoystick(info.player, info.maxNumberButton, info.deviceName.c_str(),
                        info.extraInfo.c_str());
            }
            
            EVENT_KEY event;
            while (this->popEvent(&event))
            {
                switch (event.eventType)
                {
                    case UNKNOWN: 
                    {
                        ERROR_AT(__LINE__,__FILE__, "CORE_MANAGER::loop() - Unknown event type %d.", event.eventType);
                    }
                    break;
                    case ONMOVEWINDOW:
                    {
                        this->moveWindow(static_cast<int>(event.x), static_cast<int>(event.y));
                    }
                    break;
                    case ONRESIZEWINDOW:
                    {
                        if( static_cast<int>(event.x) == static_cast<int>(this->device->backBufferWidth) &&
                            static_cast<int>(event.y) == static_cast<int>(this->device->backBufferHeight))
                        {
                            #if defined _DEBUG || defined DEBUG
                            WARN_LOG("CORE_MANAGER::loop() - ONRESIZEWINDOW event with same dimensions %dx%d, ignoring.", static_cast<int>(event.x), static_cast<int>(event.y));
                            #endif
                            break;
                        }
                        #if defined _DEBUG || defined DEBUG
                        WARN_LOG("CORE_MANAGER::loop() - ONRESIZEWINDOW event with dimensions %dx%d.", static_cast<int>(event.x), static_cast<int>(event.y));
                        #endif
                        this->device->backBufferWidth  = event.x;
                        this->device->backBufferHeight = event.y;
                        if (resetDeviceWithNewDimensions(static_cast<int>(event.x), static_cast<int>(event.y)) == false)
                        {
                            // trigger full restore
                            this->forceRestore(doSwapBuffers);
                        }
                        // Update viewport
                        // Update projection and camera
                        device->setProjectionMode(true, event.x, event.y);
                        this->device->camera.updateCam(true, event.x, event.y);
                        this->_updateDimFrustum();

                        // Notify scene and plugins
                        if (this->device->scene)
                            this->device->scene->onResizeWindow();
                        for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN* plugin = this->lsPlugins[i];
                            plugin->onResizeWindow(static_cast<int>(event.x), static_cast<int>(event.y));
                        }
                    }
                    break;
                    case ONTOUCHDOWN:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchDown(event.key, event.x, event.y);
                        for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN* plugin = this->lsPlugins[i];
                            plugin->onTouchDown(event.key, event.x, event.y);
                        }
                    }
                    break;
                    case ONTOUCHUP:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchUp(event.key, event.x, event.y);
                        for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN* plugin = this->lsPlugins[i];
                            plugin->onTouchUp(event.key, event.x, event.y);
                        }
                    }
                    break;
                    case ONTOUCHMOVE:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchMove(event.key, event.x, event.y);
                        for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN* plugin = this->lsPlugins[i];
                            plugin->onTouchMove(event.key, event.x, event.y);
                        }
                    }
                    break;
                    case ONTOUCHZOOM:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchZoom((float)event.key);
                        for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN* plugin = this->lsPlugins[i];
                            plugin->onTouchZoom((float)event.key);
                        }
                    }
                    break;
                    case ONKEYDOWN:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyDown(event.key);
                        #if defined(_WIN32) || defined(__MINGW32__)
                        if (event.key == VK_CAPITAL)
                        {
                            if ((GetKeyState(VK_CAPITAL) & 0x0001) != 0)
                                this->keyCapsLockState = true;
                            else
                                this->keyCapsLockState = false;
                        }
                        #elif defined(__linux__) || defined(__APPLE__)
                        //TODO: implement CapsLock state detection for linux and macOS
                        #endif
                        for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN* plugin = this->lsPlugins[i];
                            plugin->onKeyDown(event.key);
                        }
                    }
                    break;
                    case ONKEYUP:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyUp(event.key);
                        for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN* plugin = this->lsPlugins[i];
                            plugin->onKeyUp(event.key);
                        }
                    }
                    break;
                    case ONDOUBLECLICK:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onDoubleClick(event.x, event.y, event.key);
                        for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN* plugin = this->lsPlugins[i];
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
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyDownJoystick(event.player, event.key);
                        for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN* plugin = this->lsPlugins[i];
                            plugin->onKeyDownJoystick(event.player, event.key);
                        }
                    }
                    break;
                    case ONKEYUPJOYSTICK:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyUpJoystick(event.player, event.key);
                        for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN* plugin = this->lsPlugins[i];
                            plugin->onKeyUpJoystick(event.player, event.key);
                        }
                    }
                    break;
                    case ONMOVEJOYSTICK:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onMoveJoystick(event.player, event.lx, event.ly, event.rx, event.ry);
                        for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN* plugin = this->lsPlugins[i];
                            plugin->onMoveJoystick(event.player, event.lx, event.ly, event.rx, event.ry);
                        }
                    }
                    break;
                }
                if (!this->device->run)
                {
                    break;
                }
            }
            //if (loopCallCount <= 3)
            //    INFO_LOG("CORE_MANAGER::loop() about to update/render (frame %d) changeScene=%d swapStep=%d",
            //             loopCallCount, (int)this->changeScene, this->device->__swapBackBufferStep);
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
            for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
            {
                PLUGIN* plugin = this->lsPlugins[i];
                plugin->onDestroy();
            }

            if (this->device->audioInterface)
                this->device->audioInterface->stopAll();
        }
        return 0;
    }
    
    void CORE_MANAGER::onStopCoreManager()
    {
        // Stop the game and all resources, this is called when the device is lost, so we need to release all resources and stop the game, but we do not release the graphics device, so we can restore it later
        wasGamePausedBeforeOnStop = this->device->isGamePaused();
        this->device->pauseGame();
        for (auto ptr : this->device->lsObjectRender2DS)
        {
            ptr->onStop();
        }
        for (auto ptr : this->device->lsObjectRender2DW)
        {
            ptr->onStop();
        }
        for (auto ptr : this->device->lsObjectRender3D)
        {
            ptr->onStop();
        }
        TEXTURE_MANAGER::getInstance()->release();
        MESH_MANAGER::getInstance()->release();
        
    }

    void CORE_MANAGER::update()
    {
        if (!device->run)
            return;
        this->device->updateFps();
        this->device->__percXcam2dScale = 1.0f / this->device->camera.scale2d.x;
        this->device->__percYcam2dScale = 1.0f / this->device->camera.scale2d.y;
        this->adjustScaleScreen2d();
        this->logic();
        this->updatePhysis();
        this->updateAudio();
        for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
        {
            PLUGIN* plugin = this->lsPlugins[i];
            plugin->onLoop(this->device->delta);
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
                    const VEC3 distFromCam(ptr->position - device->camera.position);
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
        if (!device->run)
            return;
        std::vector<RENDERIZABLE *> lsRender2ds;
        std::vector<RENDERIZABLE *> lsRender2dw;
        std::vector<RENDERIZABLE *> lsRender3d;
        // Atualiza a camera de acordo com a
        // projeção----
        device->setProjectionMode(true, device->backBufferWidth, device->backBufferHeight);
        // prepara para renderizar os objeto --
        device->totalObjectsIsRendering3D = 0;
        device->totalObjectsOnFrustum3D   = 0;
        device->totalObjects3D            = static_cast<uint32_t>(this->device->lsObjectRender3D.size());
        device->totalObjectsIsRendering2D = 0;
        device->totalObjectsOnFrustum2D   = 0;
        const auto total2ds       = static_cast<uint32_t>(this->device->lsObjectRender2DS.size());
        const auto total2dw       = static_cast<uint32_t>(this->device->lsObjectRender2DW.size());
        device->totalObjects2D            = total2ds + total2dw;

#if defined USE_THREAD
        std::thread thread2ds(prepareRender2d, std::ref(this->device->lsObjectRender2DS), std::ref(lsRender2ds));
        std::thread thread2dw(prepareRender2d, std::ref(this->device->lsObjectRender2DW), std::ref(lsRender2dw));
        std::thread thread3d(prepareRender3d, std::ref(this->device->lsObjectRender3D), std::ref(lsRender3d));
        if (thread2ds.joinable())
            thread2ds.join();
        if (thread2dw.joinable())
            thread2dw.join();
        if (thread3d.joinable())
            thread3d.join();
#else
        prepareRender2d(std::ref(this->device->lsObjectRender2DS), std::ref(lsRender2ds)); //-V525
        prepareRender2d(std::ref(this->device->lsObjectRender2DW), std::ref(lsRender2dw));
        prepareRender3d(std::ref(this->device->lsObjectRender3D), std::ref(lsRender3d));
#endif

        device->totalObjectsOnFrustum2D = static_cast<uint32_t>(lsRender2ds.size() + lsRender2dw.size());
        device->totalObjectsOnFrustum3D = static_cast<uint32_t>(lsRender3d.size());
        
        if (!this->renderToTargets())
            return;
        
        if (device->clearBackGround)
        {
            device->clearDepthColored();
        }
        device->updateFrustum(&this->device->camera.matrixView, &this->device->camera.matrixProj);
        device->camera.updateNormalsRelativeCam();
        device->camera.calculateAzimuthFromCamera();
        this->device->camera.matrixBillboard = this->device->camera.matrixView; // Obtemos a Matrix De Vista Do Vista 3D
        MatrixInverse(&this->device->camera.matrixBillboard, nullptr, &this->device->camera.matrixBillboard);
        device->totalObjectsIsRendering3D = 0;
        if (this->beginRender())
        {
            for (auto ptrRender : lsRender3d)
            {
                if (ptrRender->render())
                    ++device->totalObjectsIsRendering3D;
            }

            device->setProjectionMode(false, device->backBufferWidth, device->backBufferHeight);
            device->totalObjectsIsRendering2D = 0;
            // Clear the depth buffer so 3D perspective depth values do not occlude 2dw
            // objects whose depth comes from the orthographic projection. On Metal this
            // ends the current command encoder and starts a new one that preserves the
            // colour attachment (3D scene) while clearing depth to 1.0.
            device->clearDepth();
            device->setDephtTest(true);
            for (auto ptrRender : lsRender2dw)
            {
                if (ptrRender->render())
                    device->totalObjectsIsRendering2D++;
            }
            device->setDephtTest(false);
            for (auto ptrRender : lsRender2ds)
            {
                if (ptrRender->render())
                    ++device->totalObjectsIsRendering2D;
            }
            device->setDephtTest(true);

            for (unsigned int i = 0; i < this->lsPlugins.size(); ++i)
            {
                PLUGIN* plugin = this->lsPlugins[i];
                plugin->onRender();
            }
            this->endRender();
        }
    }

    void CORE_MANAGER::_updateDimFrustum()
    {
        VEC3 point(0, 0, 50);
        this->device->dimNearFrustum3d = VEC3(0, 0, 20);
        this->device->dimFarFrustum3d  = VEC3(0, 0, 980);
        this->device->camera.updateCam(true, this->device->backBufferWidth, this->device->backBufferHeight);
        this->device->updateFrustum(&this->device->camera.matrixView, &this->device->camera.matrixProj);
        while (this->device->isPointAtTheFrustum(point))
        {
            point.x += 0.5f;
        }
        this->device->dimNearFrustum3d.x = point.x * 2.0f;

        point = VEC3(0, 0, 50);
        while (this->device->isPointAtTheFrustum(point))
        {
            point.y += 0.5f;
        }
        this->device->dimNearFrustum3d.y = point.y * 2.0f;

        point = VEC3(0, 0, 980);
        while (this->device->isPointAtTheFrustum(point))
        {
            point.x += 0.5f;
        }
        this->device->dimFarFrustum3d.x = point.x * 2.0f;

        point = VEC3(0, 0, 980);
        while (this->device->isPointAtTheFrustum(point))
        {
            point.y += 0.5f;
        }
        this->device->dimFarFrustum3d.y = point.y * 2.0f;
    }
    
    void CORE_MANAGER::adjustScaleScreen2d()
    {
        if (this->device->camera.expectedScreen.x != 0.0f && this->device->camera.expectedScreen.y != 0.0f) //-V550
        {
            const float percx = this->device->backBufferWidth / this->device->camera.expectedScreen.x;
            const float percy = this->device->backBufferHeight / this->device->camera.expectedScreen.y;
            if (percx != 0.0f && percy != 0.0f) //-V550
            {
                if (this->device->camera.stretch[0])
                {
                    if (strcmp(this->device->camera.stretch, "x") == 0)
                    {
                        this->device->camera.scaleScreen2d.x = percx;
                        this->device->camera.scaleScreen2d.y = percx;
                        this->device->camera.scale2d.x = percx;
                        this->device->camera.scale2d.y = percx;
                    }
                    else if (strcmp(this->device->camera.stretch, "y") == 0)
                    {
                        this->device->camera.scaleScreen2d.x = percy;
                        this->device->camera.scaleScreen2d.y = percy;
                        this->device->camera.scale2d.x = percy;
                        this->device->camera.scale2d.y = percy;
                    }
                    else if (strcmp(this->device->camera.stretch, "xy") == 0)
                    {
                        this->device->camera.scaleScreen2d.x = percx;
                        this->device->camera.scaleScreen2d.y = percy;
                        this->device->camera.scale2d.x = percx;
                        this->device->camera.scale2d.y = percy;
                    }
                    else if (percx < percy)
                    {
                        this->device->camera.scaleScreen2d.x = percx;
                        this->device->camera.scaleScreen2d.y = percx;
                        this->device->camera.scale2d.x = percx;
                        this->device->camera.scale2d.y = percx;
                    }
                    else
                    {
                        this->device->camera.scaleScreen2d.x = percy;
                        this->device->camera.scaleScreen2d.y = percy;
                        this->device->camera.scale2d.x = percy;
                        this->device->camera.scale2d.y = percy;
                    }
                }
                else if (percx < percy)
                {
                    this->device->camera.scaleScreen2d.x = percx;
                    this->device->camera.scaleScreen2d.y = percx;
                    this->device->camera.scale2d.x = percx;
                    this->device->camera.scale2d.y = percx;
                }
                else
                {
                    this->device->camera.scaleScreen2d.x = percy;
                    this->device->camera.scaleScreen2d.y = percy;
                    this->device->camera.scale2d.x = percy;
                    this->device->camera.scale2d.y = percy;
                }
            }
        }
    }
    
    void CORE_MANAGER::updateAudio()
    {
        if(this->device->audioInterface)
            this->device->audioInterface->update(this,this->device->scene->getIdScene());
    }
    
    void CORE_MANAGER::updatePhysis()
    {
        if (!this->device->scene)
            return;
        const float        fps            = this->device->delta == 0.0f ? 0.0f : this->device->fps; //-V550
        const int          idCurrentScene = this->device->scene->getIdScene();
        const std::vector<PHYSICS*>::size_type s = this->device->lsPhysics.size();
        for (std::vector<PHYSICS*>::size_type i = 0; i < s; ++i)
        {
            PHYSICS *ptr = this->device->lsPhysics[i];
            if (ptr && ptr->enablePhysics && ptr->idScene == idCurrentScene)
            {
                ptr->update(fps,this->device->delta);
            }
        }
    }
    
    void CORE_MANAGER::initEnableRenders()
    {
        for (auto ptr : this->device->lsObjectRender3D)
        {
            if (ptr != nullptr)
            {
                ptr->enableRender = false;
            }
        }
        for (auto ptr : this->device->lsObjectRender2DS)
        {
            if (ptr != nullptr)
            {
                ptr->enableRender = false;
            }
        }
        for (auto ptr : this->device->lsObjectRender2DW)
        {
            if (ptr != nullptr)
            {
                ptr->enableRender = false;
            }
        }
    }
    
    void CORE_MANAGER::logic()
    {
        if (this->device->scene != nullptr)
        {
            if (this->device->scene->endScene)
            {
                this->device->scene->onFinalizeScene();
                this->device->scene->wasUnloadedScene = true;
                disableRender(this->device->scene->getIdScene());
                for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                {
                    PLUGIN * plugin = this->lsPlugins[i];
                    plugin->onDestroy();
                }
                this->lsPlugins.clear();
                if (this->device->scene->goToNextScene && this->device->scene->nextScene == nullptr)
                {
                    this->device->run             = false;
                    this->device->clearBackGround = false;
                }
                else
                {
                    if (this->device->scene->goToNextScene)
                        this->device->scene       = this->device->scene->nextScene;
                    if(this->device->scene)
                        this->device->scene->endScene = false;
                    changeScene                   = true;
                    this->device->clearBackGround = true;
                    if(this->device->scene)
                        this->device->scene->startLoading();
                }
                this->__sceneWasInit = false;
            }
            else if (changeScene)
            {
                INFO_LOG("CORE_MANAGER::logic() changeScene=true swapStep=%d", this->device->__swapBackBufferStep);
                if (this->device->__swapBackBufferStep == 3)
                {
                    INFO_LOG("CORE_MANAGER::logic() calling scene->init()");
                    this->reinitTimers();
                    enableRender(this->device->scene->getIdScene());
                    this->device->scene->wasUnloadedScene = false;
                    this->device->orderRender.reInit();
                    this->device->scene->init();
                    this->device->setFakeFps(120,60);
                    this->device->resumeTimer();
                    this->__sceneWasInit          = true;
                    changeScene                   = false;
                    this->device->clearBackGround = true;
                    if(this->device->scene)
                        this->device->scene->endLoading();
                }
                else
                {
                    this->device->clearBackGround = false;
                    this->device->__swapBackBufferStep++;
                }
            }
            else
            {
                this->device->scene->logic();
            }
        }
    }
    
    void CORE_MANAGER::reinitTimers()
    {
        this->device->clearAdditionalTimers();
        this->device->resumeTimer();
    }

    void CORE_MANAGER::enableRender(const int idScene)
    {
        for (auto ptr : this->device->lsObjectRender3D)
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = true;
            }
        }
        for (auto ptr : this->device->lsObjectRender2DS)
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = true;
            }
        }
        for (auto ptr : this->device->lsObjectRender2DW)
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
        for (auto ptr : this->device->lsObjectRender3D)
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = false;
            }
        }
        for (auto ptr : this->device->lsObjectRender2DS)
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = false;
            }
        }
        for (auto ptr : this->device->lsObjectRender2DW)
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
        auto fExecute = [] (std::string command) -> void
        {
            system(command.c_str());
        };
        static std::string sCommand;
        sCommand                         = command;
        mbm::DEVICE* device              = mbm::DEVICE::getInstance();
        std::string name                 = fNextThreadName();
        std::thread* exec_thread         = new std::thread(fExecute, std::ref(sCommand));
        DYNAMIC_VAR* dyVar               = new DYNAMIC_VAR(DYNAMIC_REF,static_cast<const void*>(exec_thread));
        device->lsDynamicVarGlobal[name] = dyVar;
    }
    #endif

    bool CORE_MANAGER::onLostDevice(const bool doSwapBuffers, int width, int height, const int px, const int py)
    {
        if (stepRestore == STEP_RES_INIT_GL)
        {
#if defined _DEBUG
            WARN_LOG("onLostDevice step %d", stepRestore);
#endif
            // Save 2D scaling state
            const VEC2 expectedScreenBefore = this->device->camera.expectedScreen;
            char stretchBefore[sizeof(this->device->camera.stretch)] = {};
            strncpy(stretchBefore, this->device->camera.stretch, sizeof(stretchBefore) - 1);

            constexpr bool wasLostDevice = true;
            this->ReleaseGraphics(wasLostDevice);

            if (initGraphics(this->nameAplication.c_str(), width, height, px, py, this->windowBorder, this->enableResizeWindow))
            {
                // Reapply previous 2D scaling
                this->device->camera.expectedScreen = expectedScreenBefore;
                this->device->scaleToScreen(expectedScreenBefore.x, expectedScreenBefore.y, stretchBefore);

#if defined _DEBUG
                WARN_LOG("After restore - scale2d.x: %f, scale2d.y: %f, expectedScreen.x: %f, expectedScreen.y: %f",
                    this->device->camera.scale2d.x,
                    this->device->camera.scale2d.y,
                    this->device->camera.expectedScreen.x,
                    this->device->camera.expectedScreen.y);
#endif

                this->device->__percXcam2dScale = 1.0f / this->device->camera.scale2d.x;
                this->device->__percYcam2dScale = 1.0f / this->device->camera.scale2d.y;
                this->adjustScaleScreen2d();

                stepRestore = STEP_RES_DRAW_HOURGLASS;
                return false;
            }
            else
            {
#if defined _DEBUG
                WARN_LOG("onLostDevice step %d function initGraphics failed!", stepRestore);
#endif
                return false;
            }
        }
        else if (stepRestore == STEP_RES_DRAW_HOURGLASS)
        {
#if defined _DEBUG
            WARN_LOG("onLostDevice step %d draw Hourglass.", stepRestore);
#endif
            if (this->beginRender())
            {
                device->setProjectionMode(false, device->backBufferWidth, device->backBufferHeight);
                device->setDephtTest(false);
                device->clearDepthColored();
                if (device->scene)
                    device->scene->onRestore(0); //true means: no call restore,  just to prepare the screen.
                stepRestore = STEP_RES_OBJ;
                this->which_for = WFOR_INITIAL;
                this->endRender();
                if(doSwapBuffers)
                {
                    this->swapBuffers();
                }
            }
            return false;
        }
        else if (stepRestore == STEP_RES_OBJ)
        {
            device->setProjectionMode(false, device->backBufferWidth, device->backBufferHeight);
            device->setDephtTest(false);
            device->clearDepthColored();
            switch (this->which_for)
            {
            case WFOR_INITIAL:
            {
#if defined _DEBUG
                WARN_LOG("onLostDevice step %d restoring objs.", stepRestore);
#endif
                const auto t = static_cast<float>(this->device->lsObjectRender2DW.size() + this->device->lsObjectRender2DS.size() + this->device->lsObjectRender3D.size());
                if (t > 0.0f)
                {
                    this->totalForByLoop = static_cast<uint32_t>(std::ceil(t / 60.0f));//1 seconds should be loaded all objects
                    this->stepRestoreInfo = 98.0f / t * static_cast<float>(this->totalForByLoop);
                }
                else
                {
                    this->stepRestoreInfo = 0.001f;
                    this->totalForByLoop = 1;
                }
                this->percentRestoreInfo = 0.0f;
                this->which_for = WFOR_2DW;
                this->indexOnRestore = 0;
                return false;
            }
            break;
            case WFOR_2DW:
            {
                if (this->beginRender())
                {
                    for (uint32_t i = this->indexOnRestore, j = 0; i < this->device->lsObjectRender2DW.size(); ++i)
                    {
                        RENDERIZABLE* ptr = this->device->lsObjectRender2DW[i];
                        const bool    alwaysRenderize = ptr->alwaysRenderize;
                        const bool    enableRender = ptr->enableRender;
                        ptr->alwaysRenderize = false;
                        ptr->enableRender = false;
                        if (ptr->onRestoreDevice())
                        {
                            ptr->alwaysRenderize = alwaysRenderize;
                            ptr->enableRender = enableRender;
                            ptr->onRestoreAnimationsState();
                        }
                        this->indexOnRestore = (i + 1);
                        if (++j >= this->totalForByLoop)
                        {
                            this->percentRestoreInfo += this->stepRestoreInfo;
                            if (device->scene)
                                device->scene->onRestore(static_cast<int>(std::ceil(this->percentRestoreInfo > 98.9f ? 98.9f : this->percentRestoreInfo)));
                            break;
                        }
                    }
                    this->endRender();
                    if(doSwapBuffers)
                    {
                        this->swapBuffers();
                    }
                    if (this->indexOnRestore >= this->device->lsObjectRender2DW.size())
                    {
                        this->indexOnRestore = 0;
                        this->which_for = WFOR_2DS;
                    }
                }
                return false;
            }
            break;
            case WFOR_2DS:
            {
                if (this->beginRender())
                {
                    for (uint32_t i = this->indexOnRestore, j = 0; i < this->device->lsObjectRender2DS.size(); ++i)
                    {
                        RENDERIZABLE* ptr = this->device->lsObjectRender2DS[i];
                        const bool    alwaysRenderize = ptr->alwaysRenderize;
                        const bool    enableRender = ptr->enableRender;
                        ptr->alwaysRenderize = false;
                        ptr->enableRender = false;
                        if (ptr->onRestoreDevice())
                        {
                            ptr->alwaysRenderize = alwaysRenderize;
                            ptr->enableRender = enableRender;
                            ptr->onRestoreAnimationsState();
                        }
                        this->indexOnRestore = (i + 1);
                        if (++j >= this->totalForByLoop)
                        {
                            this->percentRestoreInfo += this->stepRestoreInfo;
                            if (device->scene)
                                device->scene->onRestore(static_cast<int>(std::ceil(this->percentRestoreInfo > 98.9f ? 98.9f : this->percentRestoreInfo)));
                            break;
                        }
                    }
                    this->endRender();
                    if(doSwapBuffers)
                    {
                        this->swapBuffers();
                    }
                    if (this->indexOnRestore >= this->device->lsObjectRender2DS.size())
                    {
                        this->indexOnRestore = 0;
                        this->which_for = WFOR_3D;
                    }
                }
                return false;
            }
            break;
            case WFOR_3D:
            {
                if (this->beginRender())
                {
                    for (uint32_t i = this->indexOnRestore, j = 0; i < this->device->lsObjectRender3D.size(); ++i)
                    {
                        RENDERIZABLE* ptr = this->device->lsObjectRender3D[i];
                        const bool    alwaysRenderize = ptr->alwaysRenderize;
                        const bool    enableRender = ptr->enableRender;
                        ptr->alwaysRenderize = false;
                        ptr->enableRender = false;
                        if (ptr->onRestoreDevice())
                        {
                            ptr->alwaysRenderize = alwaysRenderize;
                            ptr->enableRender = enableRender;
                            ptr->onRestoreAnimationsState();
                        }
                        this->indexOnRestore = (i + 1);
                        if (++j >= this->totalForByLoop)
                        {
                            this->percentRestoreInfo += this->stepRestoreInfo;
                            if (device->scene)
                                device->scene->onRestore(static_cast<int>(std::ceil(this->percentRestoreInfo > 98.9f ? 98.9f : this->percentRestoreInfo)));
                            break;
                        }
                    }
                    this->endRender();
                    if(doSwapBuffers)
                    {
                        this->swapBuffers();
                    }
                    if (this->indexOnRestore >= this->device->lsObjectRender3D.size())
                    {
                        this->indexOnRestore = 0;
                        this->which_for = WFOR_DONE;
                    }
                    return false;
                }
            }
            break;
            default: {};
            }
            stepRestore = STEP_RES_END;
            return false;
        }
        else if (stepRestore == STEP_RES_END)
        {
#if defined _DEBUG
            WARN_LOG("onLostDevice step %d resumeGame", stepRestore);
#endif
            stepRestore = STEP_RES_INIT_GL;
            device->clearBackGround = true;
            if(wasGamePausedBeforeOnStop == false)
                this->device->resumeGame();
            if (device->scene)
                device->scene->onRestore(100);
            return true;
        }
        return false;
    }

     void CORE_MANAGER::forceRestore(const bool doSwapBuffers)
    {
        // Call onStop and forceRestore to ensure all resources are reloaded
        this->onStopCoreManager();
        while (!this->onLostDevice(doSwapBuffers, 
            static_cast<int>(this->device->backBufferWidth),
            static_cast<int>(this->device->backBufferHeight),
            this->device->windowPositionX, 
            this->device->windowPositionY));
    }

     void CORE_MANAGER::onTouchDown(int key, float x, float y)
     {
         x /= this->device->camera.scale2d.x;
         y /= this->device->camera.scale2d.y;
         EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONTOUCHDOWN);
         this->pushEvent(&ev);
     }

     void CORE_MANAGER::onTouchUp(int key, float x, float y)
     {
         x /= this->device->camera.scale2d.x;
         y /= this->device->camera.scale2d.y;
         EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONTOUCHUP);
         this->pushEvent(&ev);
     }

     void CORE_MANAGER::onTouchMove(int key, float x, float y)
     {
         x /= this->device->camera.scale2d.x;
         y /= this->device->camera.scale2d.y;
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
         x /= this->device->camera.scale2d.x;
         y /= this->device->camera.scale2d.y;
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
         if (event && this->device->scene && this->__sceneWasInit)
         {
             mutexEvents.lock();
             if (event->eventType == this->lastEvent.eventType)
             {
                 switch (event->eventType)
                 {
                    case UNKNOWN: 
                    {
                        mutexEvents.unlock();
                        return;
                    }
                    break;
                    case ONRESIZEWINDOW:
                    {
                        EVENT_KEY* event_onresize = this->lsEvents.size() > 0 ? &this->lsEvents.back() : nullptr;
                        if (event_onresize)
                        {
                            //update resize event only
                            *event_onresize = *event;
                        }
                        mutexEvents.unlock();
                        return;
                    }
                    break;
                    case ONMOVEWINDOW:
                    {
                        EVENT_KEY* event_onmove = this->lsEvents.size() > 0 ? &this->lsEvents.back() : nullptr;
                        if (event_onmove)
                        {
                            //update move event only
                            *event_onmove = *event;
                        }
                        mutexEvents.unlock();
                        return;
                    }
                    break;
                    case ONTOUCHMOVE:
                    {
                        if (event->key == this->lastEvent.key &&
                            (static_cast<int>(event->x) == static_cast<int>(this->lastEvent.x) &&
                            static_cast<int>(event->y) == static_cast<int>(this->lastEvent.y)))
                        {
                            //ignore move event with same position only
                            mutexEvents.unlock();
                            return;
                        }
                    }
                    break;
                    case ONTOUCHDOWN:
                    case ONTOUCHUP:
                    case ONDOUBLECLICK:
                    {
                        if (event->key == this->lastEvent.key)
                        {
                            EVENT_KEY* event_touch = this->lsEvents.size() > 0 ? &this->lsEvents.back() : nullptr;
                            if (event_touch)
                            {
                                *event_touch = *event;
                            }
                            mutexEvents.unlock();
                            return;
                        }
                    }
                    break;
                    case ONKEYDOWN:
                    case ONKEYUP:
                    {
                        if (event->key == this->lastEvent.key)
                        {
                            mutexEvents.unlock();
                            return;
                        }
                    }
                    break;
                    case ONTOUCHZOOM: 
                    {
                        if (event->key == this->lastEvent.key)
                        {
                            EVENT_KEY* event_zoom = this->lsEvents.size() > 0 ? &this->lsEvents.back() : nullptr;
                            if (event_zoom)
                            {
                                *event_zoom = *event;
                            }
                            else
                            {
                                // Queue empty: coalescing would drop the event. Add it instead.
                                this->lastEvent = *event;
                                this->lsEvents.push_back(*event);
                            }
                            mutexEvents.unlock();
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
             this->lastEvent = *event;

             switch (event->eventType)
             {
                case ONKEYDOWN:
                {
                    if (this->__keyPressed[event->key] == false)
                        this->lsEvents.push_back(*event);
                    this->__keyPressed[event->key] = true;
                }
                break;
                case ONKEYUP:
                {
                    if (this->__keyPressed[event->key])
                        this->lsEvents.push_back(*event);
                    this->__keyPressed[event->key] = false;
                }
                break;
                case ONRESIZEWINDOW:
                {
                    this->lsEvents.push_back(*event);
                }
                break;
                default: 
                {
                    this->lsEvents.push_back(*event);
                }
                break;
             }
             mutexEvents.unlock();
         }
     }

     bool CORE_MANAGER::popEvent(EVENT_KEY* event)
     {
         mutexEvents.lock();
         if (this->lsEvents.size() > 0 && event)
         {
             *event = this->lsEvents.front();
             this->lsEvents.pop_front();
             mutexEvents.unlock();
             return true;
         }
         else
         {
             mutexEvents.unlock();
             return false;
         }
     }

     void CORE_MANAGER::pushEvent(INFO_JOYSTICK_INIT_PLAYER* info)
     {
         mutexEvents.lock();
         if (this->device->scene && this->__sceneWasInit)
         {
             this->lsInfoJoystick.push_back(*info);
         }
         mutexEvents.unlock();
     }

     bool CORE_MANAGER::popEvent(INFO_JOYSTICK_INIT_PLAYER* info)
     {
         mutexEvents.lock();
         if (this->lsInfoJoystick.size() > 0 && info)
         {

             *info = this->lsInfoJoystick.front();
             this->lsInfoJoystick.pop_front();
             mutexEvents.unlock();
             return true;
         }
         else
         {
             mutexEvents.unlock();
             return false;
         }
     }

}
