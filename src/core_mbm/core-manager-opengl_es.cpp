/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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


#if defined(USE_OPENGL_ES)
#include <core-manager.h>

#include <device.h>
#include <renderizable.h>
#include <specific-opengl_es.h>
#if defined(_WIN32)
    #include "specific-opengl_es-windows-context.h"
#elif !defined(ANDROID) && (defined(__linux__) || defined(__APPLE__))
    #include "specific-opengl_es-x11-context.h"
#endif
#include "specific-opengl_es-render-target.h"
#include <util-interface.h>
#include <cr-static-local.h>
#include <plugin-callback.h>


namespace mbm
{
    
void printGLString(const char *name, GLenum s)
{
    const auto *v = reinterpret_cast<const char *>(glGetString(s));
    INFO_LOG("\nGL %s = %s\n", name, v);
}

void printGLStringNewLine(const char *name, GLenum s, const char delimit) 
{
    const auto *v = reinterpret_cast<const char *>(glGetString(s));
    INFO_LOG("\n%s", name);
    if (v) 
    {
        std::vector<std::string> ret;
        util::split(ret, v, delimit);
        for (auto & i : ret) 
        {
            INFO_LOG("\n%s", i.c_str());
        }
    }
}

#if !defined (ANDROID)
    void printEGLStringNewLine(EGLDisplay eglDisplay,const char delimit)
    {
        const auto *v = reinterpret_cast<const char *>(eglQueryString(eglDisplay,EGL_EXTENSIONS));
        INFO_LOG("\n%s", "EGL_EXTENSIONS");
        if (v) 
        {
            std::vector<std::string> ret;
            util::split(ret, v, delimit);
            for (auto & i : ret) 
            {
                INFO_LOG("\n%s", i.c_str());
            }
        }
    }
#endif

    CORE_MANAGER::CORE_MANAGER()
    {
        this->initializeImpl();
        this->setDevice(DEVICE::getInstance());
        this->setChangeScene(true);
        this->setSceneInitialized(false);
        this->setKeyCapsLockState(false);
    #if (defined (__MINGW32__) || defined (__CYGWIN__) || defined(_WIN32))
        DEVICE *device = this->getDevice();
        device->getSpecificContextDevice()->initializeWi32Callbacks(this);
    #endif
    }
    
    void CORE_MANAGER::swapBuffers()
    {
        #if defined(ANDROID)
        androidSwapBuffers();
        #else
        DEVICE *device = this->getDevice();
        eglSwapBuffers(device->getSpecificContextDevice()->eglDisplay, device->getSpecificContextDevice()->eglSurface);
        #endif
    }

    bool CORE_MANAGER::resetDeviceWithNewDimensions(int newWidth, int newHeight)
    {
        #if (defined(__linux__) || defined(__APPLE__)) && !defined(ANDROID)
        DEVICE *device = this->getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        // On X11/EGL, the EGL surface doesn't automatically resize with the window
        // We need to recreate the surface to match the new window dimensions
        if (!context->recreateEGLSurface())
        {
            return false;  // Trigger full restore if surface recreation fails
        }
        
        // Query the actual EGL surface dimensions after recreation
        EGLint surfaceWidth = 0;
        EGLint surfaceHeight = 0;
        eglQuerySurface(context->eglDisplay,
                        context->eglSurface,
                        EGL_WIDTH, &surfaceWidth);
        eglQuerySurface(context->eglDisplay,
                        context->eglSurface,
                        EGL_HEIGHT, &surfaceHeight);
        
        // Use actual surface dimensions
        if (surfaceWidth > 0 && surfaceHeight > 0)
        {
            newWidth = surfaceWidth;
            newHeight = surfaceHeight;
            device->setBackBufferSize(static_cast<float>(newWidth), static_cast<float>(newHeight));
        }
        #endif
        
        // Update the viewport to the new dimensions
        GLViewport(0, 0, newWidth, newHeight);
        return true;
    }

    bool CORE_MANAGER::beginRender()
    {
        //nothing to do here
        return true;
    }

    void CORE_MANAGER::endRender()
    {
        //nothing to do here
    }
    
    bool CORE_MANAGER::renderToTargets()
    {
        DEVICE *device = this->getDevice();
        CAMERA &camera = device->getCamera();
        bool oneRender = false;
        const uint32_t totalRenderTargets = device->getTotalRenderTargets();
        for (uint32_t i = 0; i < totalRenderTargets; ++i)
        {
            auto renderTarget = device->getRenderTarget(i);
            if (!renderTarget)
                continue;
            if (!renderTarget->isObjectOnFrustum)
                continue;
            void *renderTargetSpecificConfig = renderTarget->getRenderTargetSpecificConfig();
            const RENDER2TARGET_GLES* sf = static_cast<const RENDER2TARGET_GLES*>(renderTargetSpecificConfig);
            GLViewport(0, 0, static_cast<GLsizei>(renderTarget->widthTexture), static_cast<GLsizei>(renderTarget->heightTexture));
            GLBindFramebuffer(GL_FRAMEBUFFER, sf->idFrameBuffer);
            GLFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, sf->idTextureDynamic,0);
            GLFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, sf->idDepthRenderbuffer);
            GLClearColor(renderTarget->colorClearBackGround.r, renderTarget->colorClearBackGround.g,
                         renderTarget->colorClearBackGround.b, renderTarget->colorClearBackGround.a);
            GLClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            GLClearDepthf(1.0f);
            const GLenum status = GLCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE)
            {
                GLBindFramebuffer(GL_FRAMEBUFFER, 0);
                GLViewport(0, 0, static_cast<GLsizei>(device->getBackBufferWidth()), static_cast<GLsizei>(device->getBackBufferHeight()));
                camera.updateCam(true, static_cast<float>(device->getBackBufferWidth()), static_cast<float>(device->getBackBufferHeight()));
                return false;
            }
            if (!renderTarget->render2Texture())
            {
                GLBindFramebuffer(GL_FRAMEBUFFER, 0);
                GLViewport(0, 0, static_cast<GLsizei>(device->getBackBufferWidth()), static_cast<GLsizei>(device->getBackBufferHeight()));
                camera.updateCam(true, static_cast<float>(device->getBackBufferWidth()), static_cast<float>(device->getBackBufferHeight()));
                return false;
            }
            GLBindTexture(GL_TEXTURE_2D, 0);
            GLBindFramebuffer(GL_FRAMEBUFFER, 0);
            GLBindRenderbuffer(GL_RENDERBUFFER, 0);
            oneRender = true;
        }
        if (oneRender)
        {
            GLViewport(0, 0, static_cast<GLsizei>(device->getBackBufferWidth()), static_cast<GLsizei>(device->getBackBufferHeight()));
            camera.updateCam(true, static_cast<float>(device->getBackBufferWidth()), static_cast<float>(device->getBackBufferHeight()));
        }
        return true;
    }
    
    unsigned int CORE_MANAGER::addPlugin(PLUGIN * plugin)
    {
        DEVICE *device = this->getDevice();
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
            void * handle = nullptr;
            #if defined _WIN32
                handle = device->getSpecificContextDevice()->window.getHwnd();
            #elif (defined(__linux__) || defined(__APPLE__)) && !defined (ANDROID)
                handle = device->getSpecificContextDevice()->display_x11;
            #elif defined(ANDROID)
                handle = androidGetPluginSubscribeHandle();
            #else
                #error "Platform not supported"
            #endif
            plugin->onSubscribe(static_cast<int>(device->getBackBufferWidth()),static_cast<int>(device->getBackBufferHeight()),handle, nullptr);
            return indexPlugin;
        }
        return 0xffffffff;
    }

    #if defined _WIN32
    void CORE_MANAGER::setMinMaxSizeWindow(int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y)
    {
        DEVICE *device = this->getDevice();
        device->getSpecificContextDevice()->window.setMinSizeAllowed(min_x,min_y);
        device->getSpecificContextDevice()->window.setMaxSizeAllowed(max_x,max_y);
    }
    #elif (defined(__linux__) || defined(__APPLE__)) && !defined(ANDROID)
    void CORE_MANAGER::setMinMaxSizeWindow(int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y)
    {
        DEVICE *device = this->getDevice();
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
        const float backBufferWidth = device->getBackBufferWidth();
        const float backBufferHeight = device->getBackBufferHeight();
        if(static_cast<int32_t>(backBufferWidth) <= max_x && static_cast<int32_t>(backBufferWidth) >= min_x)
        {
            xsize.base_width    = static_cast<int>(backBufferWidth);
            xsize.width         = static_cast<int>(backBufferWidth);
        }
        else
        {
            xsize.base_width    = min_x;
            xsize.width         = static_cast<int>(min_x);
        }

        if(static_cast<int32_t>(backBufferHeight) <= max_y && static_cast<int32_t>(backBufferHeight) >= min_y)
        {
            xsize.base_height   = static_cast<int>(backBufferHeight);
            xsize.height        = static_cast<int>(backBufferHeight);
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
        XSetWMNormalHints(device->getSpecificContextDevice()->display_x11,device->getSpecificContextDevice()->window_x11,&xsize);
    }
    #elif defined(ANDROID)
    void CORE_MANAGER::setMinMaxSizeWindow(int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y)
    {
        INFO_LOG("setMinMaxSizeWindow (%d,%d,%d,%d) has not effect on ANDROID platform.",min_x,min_y,max_x,max_y);
    }
    #endif
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

    const char *getDescriptionError(const unsigned int error)
    {
        switch (error)
        {
            case 0x0500: // GL_INVALID_ENUM:
            {
                return ("\nAn unacceptable value is specified for an enumerated argument.\n"
                        "The offending command is ignored\n"
                        "and has no other side effect than to set the error flag.\n");
            }
            case 0x0501: // GL_INVALID_VALUE:
            {
                return ("\nA numeric argument is out of range.\n"
                        "The offending command is ignored\n"
                        "and has no other side effect than to set the error flag.\n");
            }
            case 0x0502: // GL_INVALID_OPERATION:
            {
                return ("\nThe specified operation is not allowed in the current state.\n"
                        "The offending command is ignored\n"
                        "and has no other side effect than to set the error flag.\n");
            }
            case 0x0506: // GL_INVALID_FRAMEBUFFER_OPERATION:
            {
                return ("\nThe framebuffer object is not complete. The offending command\n"
                        "is ignored and has no other side effect than to set the error flag.\n");
            }
            case 0x0505: // GL_OUT_OF_MEMORY:
            {
                return ("\nThere is not enough memory left to execute the command.\n"
                        "The state of the GL is undefined,\n"
                        "except for the state of the error flags,\n"
                        "after this error is recorded.\n");
            }
            default:
            {
                static char errStr[255];
                snprintf(errStr, sizeof(errStr), "Unknown error gl: decimal:[%d] hexadecimal [0x%x] ", (int)error, (int)error);
                return errStr;
            }
        }
    }

    void checkGlError(const char *fileName, const int numLine, const char *message)
    {
        for (GLenum error = glGetError(); error; error = glGetError())
        {
            CR_DEFINE_STATIC_LOCAL(std::vector<GLenum>, lsErrors);
            bool mustContinue = false;
            for (uint32_t lsError : lsErrors)
            {
                if (lsError == error)
                {
                    mustContinue = true;
                    break;
                }
            }
            if (mustContinue)
                continue;
            lsErrors.push_back(error);
            const char *errorAsString = getDescriptionError(error);
            callScriptPrintLine();
            INFO_LOG("File [%s] Line[%d] %s()\n%s", basename(fileName), numLine, message ? message : "[message]",errorAsString);
        }
    }

    void checkGlError(const char *fileName, const int numLine)
    {
        for (GLenum error = glGetError(); error; error = glGetError())
        {
            CR_DEFINE_STATIC_LOCAL(std::vector<GLenum>, lsErrors);
            bool mustContinue = false;
            for (uint32_t lsError : lsErrors)
            {
                if (lsError == error)
                {
                    mustContinue = true;
                    break;
                }
            }
            if (mustContinue)
                continue;
            lsErrors.push_back(error);
            const char *errorAsString = getDescriptionError(error);
            callScriptPrintLine();
            INFO_LOG("\nFile [%s] Line[%d] \n%s", basename(fileName), numLine, errorAsString);
        }
    }
}

#endif // USE_OPENGL_ES
