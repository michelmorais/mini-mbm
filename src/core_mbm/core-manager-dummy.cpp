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


#if defined (USE_DUMMY_BACK_END_ENGINE)

#include "dummy-engine.h" // for compiler_message, you can remove it after implement the functions

#include <core-manager.h>
#include <device.h>
#include <renderizable.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <util-interface.h>
#include <audio-interface.h>
#include <miniz-wrap/miniz-wrap.h>
#include <plugin-callback.h>
#include <specific-dummy.h> // for specific context of dummy engine


namespace mbm
{
    void CORE_MANAGER::handleEventFromWindow()
    {
        #pragma message(REMINDER_TODO);
    }

    CORE_MANAGER::CORE_MANAGER()
    {
        this->device           = DEVICE::getInstance();
        this->indexOnRestore   = 0;
        this->totalForByLoop   = 0;
        this->percentRestoreInfo = 0.0f;
        this->stepRestoreInfo  = 0.1f;
        this->stepRestore      = STEP_RES_INIT_GL;
        this->which_for        = WFOR_INITIAL;
        this->changeScene      = true;
        this->__sceneWasInit   = false;
        this->keyCapsLockState = false;
        this->wasGamePausedBeforeOnStop = false;
        #pragma message(REMINDER_TODO);
    }
    
    CORE_MANAGER::~CORE_MANAGER()
    {
        #pragma message(REMINDER_TODO);
        DEVICE::quit();
    }
    
    void CORE_MANAGER::ReleaseGraphics(const bool wasDeviceLost)
    {
        TEXTURE_MANAGER::getInstance()->release();
        MESH_MANAGER::getInstance()->release();
        this->device->specificContextDevice->release(wasDeviceLost);
        #pragma message(REMINDER_TODO);
    }
    
    bool CORE_MANAGER::initGraphics(const char* nameAplication, int width, int height, const int px, const int py, const bool border, const bool enable_resize)
    {
        int x = width;
        int y = height;
        DEVICE* device = DEVICE::getInstance();
        this->nameAplication = nameAplication ? nameAplication : "Mini-mbm";
        #pragma message(REMINDER_TODO);
        
        //TODO: set version from your backend engine
        INFO_LOG("\nDUMMY Version: %s\n", "1");
        if (device->verbose)
        {
            MINIZ::showVersion();
            INFO_LOG("\nAudio engine: %s\n", AUDIO_ENGINE_version());
        }

        // Set texture capabilities (for NOT DUMMY engine must be real)
        TEXTURE_MANAGER* texture_manager = TEXTURE_MANAGER::getInstance();
        texture_manager->setTextureCapabilities(1024*1024, 1024*1024, 1024*1024);

        // device->setProjectionMode set viewport and other initial states for ANY
        if (x > 0)
            device->backBufferWidth = static_cast<float>(x);
        if (y > 0)
            device->backBufferHeight = static_cast<float>(y);
        return true;
    }

    bool CORE_MANAGER::resetDeviceWithNewDimensions(int newWidth, int newHeight)// need to be implemented in each backend engine
    {
        // Reset D3D device with new dimensions
        #pragma message(REMINDER_TODO);
        return true;
    }

    bool CORE_MANAGER::beginRender()
    {
        #pragma message(REMINDER_TODO);
        return true;
    }

    void CORE_MANAGER::endRender()
    {
        #pragma message(REMINDER_TODO);
    }

    void CORE_MANAGER::swapBuffers()
    {
        #pragma message(REMINDER_TODO);
    }

    bool CORE_MANAGER::renderToTargets()
    {
        bool oneRender                 = false;
        #pragma message(REMINDER_TODO);
        for (auto renderTarget : this->device->lsObjectRenderToTarget)
        {
            if (!renderTarget->isObjectOnFrustum)
                continue;
            
            
            if (!renderTarget->render2Texture())
            {
                ERROR_AT(__LINE__, __FILE__, "Error render2Texture!");
                this->device->camera.updateCam(true, static_cast<float>(device->backBufferWidth), static_cast<float>(device->backBufferHeight));
                return false;
            }
            oneRender = true;
        }
        if (oneRender)
        {
            this->device->camera.updateCam(true, static_cast<float>(device->backBufferWidth), static_cast<float>(device->backBufferHeight));
        }
        return true;
    }
    
    unsigned int CORE_MANAGER::addPlugin(PLUGIN * plugin)
    {
        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
        {
            const PLUGIN * thatPlugin = this->lsPlugins[i];
            if(plugin == thatPlugin)
            {
                return i;
            }
        }
        if(plugin != nullptr)
        {
            this->lsPlugins.push_back(plugin);
            #pragma message(REMINDER_TODO);
            void * handle = nullptr;
            plugin->onSubscribe(static_cast<int>(this->device->backBufferWidth),static_cast<int>(this->device->backBufferHeight), handle);
            return this->lsPlugins.size() - 1;
        }
        return 0xffffffff;
    }

    void CORE_MANAGER::setMinMaxSizeWindow(int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y)
    {
        #pragma message(REMINDER_TODO);
    }

    void CORE_MANAGER::getScreenSize(int *width,int *height)
    {
        #pragma message(REMINDER_TODO);
        {
            *width  = 1024;
            *height = 1024;
        }
    }

    void CORE_MANAGER::moveWindow(int x, int y)
    {
        #pragma message(REMINDER_TODO);
    }
}

namespace log_util
{
    OnScriptPrintLine onScriptPrintLine = nullptr;

    void setScriptPrintLine(OnScriptPrintLine onNewScriptPrintLine) noexcept
    {
        onScriptPrintLine = onNewScriptPrintLine;
    }

    void callScriptPrintLine() noexcept
    {
        if(onScriptPrintLine)
            onScriptPrintLine();
    }

}
#endif // USE_DUMMY_BACK_END_ENGINE
