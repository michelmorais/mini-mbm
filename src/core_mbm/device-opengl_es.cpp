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

#include <device.h>

#if defined (USE_OPENGL_ES)

#include <scene.h>
#include <texture-manager.h>
#include <audio-interface.h>
#include <shapes.h>
#include <physics.h>
#include <renderizable.h>
#include <mesh-manager.h>
#include <util-interface.h>
#include <specific-opengl_es.h>
#include <dynamic-var.h>

#if defined ANDROID
    // no inlucdes here
#elif defined _WIN32
    #include <plusWindows/defaultThemePlusWindows.h>
#elif defined(__linux__) || defined(__APPLE__)
    #include <X11/Xutil.h>
#endif

namespace mbm
{
    void DEVICE::initializeSpecificContext()
    {
        this->destroySpecificContext();
        this->specificContextDevice = new SPECIFIC_AUX_CONTEXT_DEVICE();
        
    }
    void DEVICE::destroySpecificContext()
    {
        if(this->specificContextDevice)
        {
            delete this->specificContextDevice;
            this->specificContextDevice = nullptr;
        }
    }

    void DEVICE::quit()
    {
        TEXTURE_MANAGER::release();
        MESH_MANAGER::release();
        releaseAudioManager();
        if (instanceDevice)
        {
            constexpr bool wasDeviceLost = false; // we are not in lost device, because we are in the destructor, so we can release all resources   
            instanceDevice->specificContextDevice->release(wasDeviceLost);
            delete instanceDevice;
        }
        instanceDevice = nullptr;
    }
    
    void DEVICE::setDephtTest(const bool enable)
    {
        if (enable)
        {
            GLEnable(GL_DEPTH_TEST);
        }
        else
        {
            GLDisable(GL_DEPTH_TEST);
        }
    }

    void DEVICE::clearDepth()
    {
        GLClearDepthf(1.0f);
        GLClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }
    void DEVICE::clearDepthColored()
    {
        GLClearDepthf(1.0f);
        GLClearColor(this->colorClearBackGround.r,
                        this->colorClearBackGround.g,
                        this->colorClearBackGround.b,
                        this->colorClearBackGround.a);
        GLClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    const char* DEVICE::getBackendEngineName() const noexcept
    {
        return "OpenGL ES";
    }

    const char* DEVICE::getBackendEngineVersion() const noexcept
    {
        static std::string versions(32,' ');
        const char *v = (const char *)glGetString(GL_VERSION);
        versions = "\nOpengL: ";
        versions += v;
        return versions.c_str();
    }

    void DEVICE::setProjectionMode(const bool is3D, const float width, const float height)
    {
        if (width > 0 && height > 0)
        {
            GLViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
        }
        if (width > 0)
            backBufferWidth = width;
        if (height > 0)
            backBufferHeight = height;
        if (width > 0 && height > 0)
            this->camera.updateCam(is3D, static_cast<float>(width), static_cast<float>(height));
    }

    const char* DEVICE::copyFileFromAsset(const char* assetName, const char* mode)// Meant to be used in Android / Iphone (others specific implementations can just return assetName).
    {
        #if defined ANDROID
        SPECIFIC_AUX_CONTEXT_DEVICE * cJni = this->specificContextDevice;
        return cJni->copyFileFromAsset(assetName, mode);
        #else
        return assetName;
        #endif
    }

    void DEVICE::disableFilteringForPixelPerfect() noexcept//backend specific way to disable texture filtering for pixel perfect rendering
    {
		// Store current texture filtering
		// In OPENGL this does not seem to be affecting the texture. what is affecting is to set those GLTexParameteri while loading the texture.
        // TODO: maybe investigate??
        constexpr GLint index[2] = { GL_TEXTURE1 , GL_TEXTURE0 };
        GLActiveTexture(GL_TEXTURE0);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &this->specificContextDevice->filter_GL_TEXTURE_WRAP_S);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &this->specificContextDevice->filter_GL_TEXTURE_WRAP_T);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &this->specificContextDevice->filter_GL_TEXTURE_MIN_FILTER);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &this->specificContextDevice->filter_GL_TEXTURE_MAG_FILTER);

        for (int i = 0; i < 2; i++)
        {
            GLActiveTexture(index[i]);
            // 'filter' now holds the current minification filter value   
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        
    }
    void DEVICE::enableFilteringAfterPixelPerfect() noexcept//backend specific way to restore texture filtering
    {
        constexpr GLint index[2] = { GL_TEXTURE1, GL_TEXTURE0 };
        for (int i = 0; i < 2; i++)
        {
            GLActiveTexture(index[i]);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }
    }
}
#endif // USE_OPENGL_ES