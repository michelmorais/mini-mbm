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
#if defined (USE_OPENGL_ES)

#include <device.h>
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

    #if defined _WIN32
    void DEVICE::setMinMaxSizeWindow(int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y)
    {
        this->window.setMinSizeAllowed(min_x,min_y);
        this->window.setMaxSizeAllowed(max_x,max_y);
    }
    #elif (defined(__linux__) || defined(__APPLE__)) && !defined(ANDROID)
    void DEVICE::setMinMaxSizeWindow(Window win,Display * display,int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y)
    {
        XSizeHints xsize;
        long min_flag = PMinSize;
        long max_flag = PMaxSize;
        if(min_x == 0 && min_y == 0)
            min_flag = 0;
        if(max_x == 0 && max_y == 0)
            max_flag = 0;

        xsize.flags         = max_flag|min_flag|USPosition;
        xsize.max_width     = static_cast<int>(max_x);
        xsize.max_height    = static_cast<int>(max_y);
        xsize.min_width     = static_cast<int>(min_x);
        xsize.min_height    = static_cast<int>(min_y);
        if(static_cast<int32_t>(this->backBufferWidth) <= max_x && static_cast<int32_t>(this->backBufferWidth) >= min_x)
        {
            xsize.base_width    = static_cast<int>(this->backBufferWidth);
            xsize.width         = static_cast<int>(this->backBufferWidth);
        }
        else
        {
            xsize.base_width    = min_x;
            xsize.width         = static_cast<int>(min_x);
        }

        if(static_cast<int32_t>(this->backBufferHeight) <= max_y && static_cast<int32_t>(this->backBufferHeight) >= min_y)
        {
            xsize.base_height   = static_cast<int>(this->backBufferHeight);
            xsize.height        = static_cast<int>(this->backBufferHeight);
        }
        else
        {
            xsize.base_height   = min_y;
            xsize.height        = static_cast<int>(min_y);
        }
        xsize.width_inc     = 0;
        xsize.height_inc    = 0;
        xsize.x             = 0;
        xsize.y             = 0;
        XSetWMNormalHints(display,win,&xsize);
    }
    #elif defined(ANDROID)
    void DEVICE::setMinMaxSizeWindow(int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y)
    {
        INFO_LOG("setMinMaxSizeWindow (%d,%d,%d,%d) has not effect on this ANDROID platform.",min_x,min_y,max_x,max_y);
    }
    #endif
    
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
#endif