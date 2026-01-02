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
#include <gles-debug.h>
#include <dynamic-var.h>

#if defined ANDROID
    #include <platform/common-jni.h>
#elif defined _WIN32
    #include <plusWindows/defaultThemePlusWindows.h>
#elif defined(__linux__) || defined(__APPLE__)
    #include <X11/Xutil.h>
#endif

namespace mbm
{
    struct SPECIFIC_AUX_CONTEXT_DEVICE
    {
    #if (defined __linux__ || defined(__APPLE__)) && !defined ANDROID
            
    #endif
        SPECIFIC_AUX_CONTEXT_DEVICE()
        {   
            #if (defined __linux__ || defined(__APPLE__)) && !defined ANDROID
                
            #endif
        };
        SPECIFIC_AUX_CONTEXT_DEVICE(const SPECIFIC_AUX_CONTEXT_DEVICE &) = delete;
        SPECIFIC_AUX_CONTEXT_DEVICE &operator=(const SPECIFIC_AUX_CONTEXT_DEVICE &) = delete;

        ~SPECIFIC_AUX_CONTEXT_DEVICE()
        {

        };
    };
    
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


#ifdef ANDROID
    void DEVICE::callQuitInJava()
    {
        util::COMMON_JNI *cJni  = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv   = cJni->jenv;
        jfieldID         fidRun = jenv->GetStaticFieldID(cJni->jclassInstanceActivityEngine, "run", "Z");
        if (nullptr == fidRun)
        {
            PRINT_IF_DEBUG( "wasn't found variable \"run\" class: %s", cJni->jclassInstanceActivityEngine);
            return;
        }
        jenv->SetStaticBooleanField(cJni->jclassInstanceActivityEngine, fidRun, false);
    }
#endif

    void DEVICE::quit()
    {
        TEXTURE_MANAGER::release();
        MESH_MANAGER::release();
#ifdef ANDROID
        util::COMMON_JNI::release();
#endif
		releaseAudioManager();
		if (instanceDevice)
        {
            delete instanceDevice;
        }
        instanceDevice = nullptr;
    }

#ifdef ANDROID
    
    void DEVICE::streamStopped(const int indexJNI)
    {
		if(this->audioInterface)
			this->audioInterface->streamStopped(indexJNI);
    }
#endif
    
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
        #if defined (USE_OPENGL_ES)
            GLClearDepthf(1.0f);
            GLClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        #elif defined(USE_DIRECTX)
            #error ClearDepthf is not defined if not USE_OPENGL_ES
        #else
            #error ClearDepthf is not defined if not for USE_OPENGL_ES or USE_DIRECTX
        #endif
    }
    void DEVICE::clearDepthColored()
    {
        #if defined (USE_OPENGL_ES)
            GLClearDepthf(1.0f);
            GLClearColor(this->colorClearBackGround.r,
                         this->colorClearBackGround.g,
                         this->colorClearBackGround.b,
                         this->colorClearBackGround.a);
            GLClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        #elif defined(USE_DIRECTX)
            #error ClearDepthf is not defined if not USE_OPENGL_ES
        #else
            #error ClearDepthf is not defined if not for USE_OPENGL_ES or USE_DIRECTX
        #endif
    }

    const char* DEVICE::getBackendEngineName() const noexcept
    {
        #if defined (USE_OPENGL_ES)
            return "OpenGL ES";
        #elif defined(USE_DIRECTX)
            return "DirectX";
        #else
            return "Unknown";
        #endif
    }

    const char* DEVICE::getBackendEngineVersion() const noexcept
    {
        static std::string versions(32,' ');
        #if defined (USE_OPENGL_ES)
            const char *v = (const char *)glGetString(GL_VERSION);
            versions = "\nOpengL: ";
            versions += v;
        #elif defined(USE_DIRECTX)
            versions = "\nDirectx: ";
            char tempVersion[16];
            sprintf(tempVersion, "%x", DIRECT3D_VERSION);
            versions += tempVersion;
        #else
            versions = "Unknown";
        #endif
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

}
#endif // USE_OPENGL_ES