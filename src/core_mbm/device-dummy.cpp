/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2025      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include <specific-dummy.h> // replace with your specific backend engine header

#include <device.h>
#include <texture-manager.h>
#include <audio-interface.h>
#include <mesh-manager.h>

namespace mbm
{
    
    
    void DEVICE::initializeSpecificContext()
    {
        this->destroySpecificContext();
        setSpecificContextDevice(new SPECIFIC_AUX_CONTEXT_DEVICE());
    }
    void DEVICE::destroySpecificContext()
    {
        auto *context = getSpecificContextDevice();
        if(context)
        {
            delete context;
            setSpecificContextDevice(nullptr);
        }
    }

    void DEVICE::quit()
    {
        TEXTURE_MANAGER::release();
        MESH_MANAGER::release();
		releaseAudioManager();
		if (instanceDevice)
        {
            delete instanceDevice;
        }
        instanceDevice = nullptr;
        REMINDER_TODO
    }

    void DEVICE::setDepthTest(const bool enable)
    {
        REMINDER_TODO
    }

    void DEVICE::clearDepth()
    {
        REMINDER_TODO
    }
    void DEVICE::clearDepthColored()
    {
        REMINDER_TODO
    }

    const char* DEVICE::getBackendEngineName() const noexcept
    {
        return "Dummy 1";
    }

    const char* DEVICE::getBackendEngineVersion() const noexcept
    {
        return "1";
    }

    void DEVICE::setProjectionMode(const bool is3D, const float width, const float height)
    {
        if (width > 0 && height > 0)
        {
            //TOD: check this
            REMINDER_TODO
        }
        if (width > 0)
            backBufferWidth = width;
        if (height > 0)
            backBufferHeight = height;
        if (width > 0 && height > 0)
            this->camera.updateCam(is3D, static_cast<float>(width), static_cast<float>(height));
        if (is3D)
        {
            
        }
        else
        {
            
        }
        
    }

    const char* DEVICE::copyFileFromAsset(const char* assetName, const char* mode)// Meant to be used in Android / Iphone (others specific implementations can just return assetName).
    {
        REMINDER_TODO
        return assetName;
    }

    void DEVICE::disableFilteringForPixelPerfect() noexcept//backend specific way to disable texture filtering for pixel perfect rendering
    {
        setPixelPerfectRenderingActive(true);
        for (int i = 0; i < 2; ++i)
        {
            REMINDER_TODO
        }
    }

    void DEVICE::enableFilteringAfterPixelPerfect() noexcept
    {
        setPixelPerfectRenderingActive(false);
        for (int i = 0; i < 2; ++i)
        {
            REMINDER_TODO
		}
    }


}
#endif // USE_DUMMY_BACK_END_ENGINE
