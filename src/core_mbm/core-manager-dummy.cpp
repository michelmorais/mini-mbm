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
        REMINDER_TODO
    }

    CORE_MANAGER::CORE_MANAGER()
    {
        this->initializeImpl();
        this->device           = DEVICE::getInstance();
        this->changeScene               = true;
        this->__sceneWasInit            = false;
        this->keyCapsLockState          = false;
        REMINDER_TODO
    }
    
    void CORE_MANAGER::ReleaseGraphics(const bool wasDeviceLost)
    {
        TEXTURE_MANAGER::getInstance()->release();
        MESH_MANAGER::getInstance()->release();
        this->device->getSpecificContextDevice()->release(wasDeviceLost);
        REMINDER_TODO
    }
    
    bool CORE_MANAGER::initGraphics(const char* nameApplication, int width, int height, const int px, const int py, const bool border, const bool enable_resize)
    {
        int x = width;
        int y = height;
        DEVICE* device = DEVICE::getInstance();
        this->setNameApplication(nameApplication);
        REMINDER_TODO
        
        //TODO: set version from your backend engine
        INFO_LOG("\nDUMMY Version: %s\n", "1");
        if (device->isVerbose())
        {
            MINIZ::showVersion();
            INFO_LOG("\nAudio engine: %s\n", AUDIO_ENGINE_version());
        }

        // Set texture capabilities (for NOT DUMMY engine must be real)
        TEXTURE_MANAGER* texture_manager = TEXTURE_MANAGER::getInstance();
        texture_manager->setTextureCapabilities(1024*1024, 1024*1024, 1024*1024);

        // device->setProjectionMode set viewport and other initial states for ANY
        if (x > 0)
            device->setBackBufferWidth(static_cast<float>(x));
        if (y > 0)
            device->setBackBufferHeight(static_cast<float>(y));
        return true;
    }

    bool CORE_MANAGER::resetDeviceWithNewDimensions(int newWidth, int newHeight)// need to be implemented in each backend engine
    {
        // Reset D3D device with new dimensions
        REMINDER_TODO
        return true;
    }

    bool CORE_MANAGER::beginRender()
    {
        REMINDER_TODO
        return true;
    }

    void CORE_MANAGER::endRender()
    {
        REMINDER_TODO
    }

    void CORE_MANAGER::swapBuffers()
    {
        REMINDER_TODO
    }

    bool CORE_MANAGER::renderToTargets()
    {
        bool oneRender                 = false;
        REMINDER_TODO
        const uint32_t totalRenderTargets = this->device->getTotalRenderTargets();
        for (uint32_t i = 0; i < totalRenderTargets; ++i)
        {
            auto renderTarget = this->device->getRenderTarget(i);
            if (!renderTarget)
                continue;
            if (!renderTarget->isObjectOnFrustum)
                continue;
            
            
            if (!renderTarget->render2Texture())
            {
                ERROR_AT(__LINE__, __FILE__, "Error render2Texture!");
                this->device->getCamera().updateCam(true, static_cast<float>(device->getBackBufferWidth()), static_cast<float>(device->getBackBufferHeight()));
                return false;
            }
            oneRender = true;
        }
        if (oneRender)
        {
            this->device->getCamera().updateCam(true, static_cast<float>(device->getBackBufferWidth()), static_cast<float>(device->getBackBufferHeight()));
        }
        return true;
    }
    
    unsigned int CORE_MANAGER::addPlugin(PLUGIN * plugin)
    {
        for(unsigned int i=0; i < this->getTotalPlugins(); ++i)
        {
            const PLUGIN * thatPlugin = this->getPlugin(i);
            if(plugin == thatPlugin)
            {
                return i;
            }
        }
        if(plugin != nullptr)
        {
            const unsigned int indexPlugin = this->appendPlugin(plugin);
            REMINDER_TODO
            void * handle = nullptr;
            plugin->onSubscribe(static_cast<int>(this->device->getBackBufferWidth()),static_cast<int>(this->device->getBackBufferHeight()), handle, nullptr);
            return indexPlugin;
        }
        return 0xffffffff;
    }

    void CORE_MANAGER::setMinMaxSizeWindow(int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y)
    {
        REMINDER_TODO
    }

    void CORE_MANAGER::getScreenSize(int *width,int *height)
    {
        REMINDER_TODO
        {
            *width  = 1024;
            *height = 1024;
        }
    }

    void CORE_MANAGER::moveWindow(int x, int y)
    {
        REMINDER_TODO
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
