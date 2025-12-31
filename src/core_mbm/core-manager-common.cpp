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
#if defined USE_EDITOR_FEATURES
#include <thread>
#endif

namespace mbm
{
    void CORE_MANAGER::setScene(SCENE *currentScene)
    {
        this->device->scene = currentScene;
    }
    
    void CORE_MANAGER::onStop()
    {
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
        this->device->pauseGame();
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
        for (auto ptrRender : lsRender3d)
        {
            if (ptrRender->render())
                ++device->totalObjectsIsRendering3D;
        }
        
        device->setProjectionMode(false, device->backBufferWidth, device->backBufferHeight);
        device->totalObjectsIsRendering2D = 0;
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
                    }
                    else if (strcmp(this->device->camera.stretch, "y") == 0)
                    {
                        this->device->camera.scaleScreen2d.x = percy;
                        this->device->camera.scaleScreen2d.y = percy;
                    }
                    else if (strcmp(this->device->camera.stretch, "xy") == 0)
                    {
                        this->device->camera.scaleScreen2d.x = percx;
                        this->device->camera.scaleScreen2d.y = percy;
                    }
                    else if (percx < percy)
                    {
                        this->device->camera.scaleScreen2d.x = percx;
                        this->device->camera.scaleScreen2d.y = percx;
                    }
                    else
                    {
                        this->device->camera.scaleScreen2d.x = percy;
                        this->device->camera.scaleScreen2d.y = percy;
                    }
                }
                else if (percx < percy)
                {
                    this->device->camera.scaleScreen2d.x = percx;
                    this->device->camera.scaleScreen2d.y = percx;
                }
                else
                {
                    this->device->camera.scaleScreen2d.x = percy;
                    this->device->camera.scaleScreen2d.y = percy;
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
                if (this->device->__swapBackBufferStep == 3)
                {
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

    #if defined USE_EDITOR_FEATURES && !defined ANDROID
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

}