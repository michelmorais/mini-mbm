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

#if (defined (__MINGW32__) || defined (__CYGWIN__) || defined(_WIN32))
#if defined (USE_OPENGL_ES)

#include "specific-opengl_es-windows-context.h"
#include <util-interface.h>
#include <core-manager.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <miniz-wrap/miniz-wrap.h>
#include <audio-interface.h>
#include <device.h>
#include <scene.h>
#include <skeletal-render-capability.h>

namespace mbm
{
    void CORE_MANAGER::handleEventFromWindow()
    {
        DEVICE *device = this->getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        context->window.doEvents();
        bool first_menu = true;
        while (mbm::WINDOW::isAnyMenuVisible() && context->window.run)
        {
            if (first_menu)
            {
                Sleep(50);
                mbm::WINDOW::refreshMenu();
            }
            context->window.doEvents();
            if (first_menu)
            {
                Sleep(50);
                mbm::WINDOW::refreshMenu();
            }
            first_menu = false;
        }
        if (context->window.run)
        {
            INFO_JOYSTICK_INIT_PLAYER info;
            while (this->popEvent(&info))
            {
                SCENE *scene = device->getScene();
                if (scene && this->isSceneInitialized())
                    scene->onInfoDeviceJoystick(info.player, info.maxNumberButton, info.deviceName.c_str(),
                        info.extraInfo.c_str());
            }
        }
        else
        {
            device->setRun(false);
        }
    }

    void CORE_MANAGER::moveWindow(int x, int y)
    {
        // nothing to do here for OpenGL ES on windows, because the window move is handled by the windowing system and not by the engine, so, we do not need to do anything here
    }

    void CORE_MANAGER::ReleaseGraphics(const bool wasDeviceLost)
    {
        TEXTURE_MANAGER::getInstance()->release();
        MESH_MANAGER::getInstance()->release();
        DEVICE *device = this->getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        context->window.setCallEventsManager(nullptr);
        context->win32_joystickByPass->releaseJoystick(&context->window);
        context->release(wasDeviceLost);
    }

    bool CORE_MANAGER::initGraphics(const char *nameApplication, int width, int height, const int px, const int py, const bool border,const bool enable_resize)
    {
        int x = width;
        int y = height;
        this->setNameApplication(nameApplication);
        DEVICE *device = this->getDevice();
        SPECIFIC_AUX_CONTEXT_DEVICE *context = device->getSpecificContextDevice();
        // Initialize window position
        device->setWindowPosition(px, py);
        context->window.setNameAplication(nameApplication);
        if (!context->window.init(nameApplication, x, y, px, py, enable_resize, enable_resize, enable_resize, false, nullptr, border == false,
            context->idIcon,false))
        {
            context->window.messageBox("error on init app ... will be closed ");
            PRINT_IF_DEBUG("error on init app ... will be closed %s", "error on create window");
            return false;
        }
        context->window.setMinSizeAllowed(800,600);
        context->window.askOnExit = false;
        HWND mNativeWindow = context->window.getHwnd();
        RECT rect;
        if (!GetClientRect(mNativeWindow, &rect))
        {
            MessageBoxW(mNativeWindow, L"error on get the window size!", L"DEVICE", MB_OK | MB_ICONERROR);
            rect.right  = width;
            rect.bottom = height;
            rect.left   = 0;
            rect.top    = 0;
        }
        if ((rect.right - rect.left) != width || (rect.bottom - rect.top) != height)
        {
            x = rect.right - rect.left;
            y = rect.bottom - rect.top;
            printf("BackBuffer adjusted because the width and height are different from window\n"
                   "expected X: %d Y: %d \n"
                   "real     X: %d Y: %d \n",
                   width, height, x, y);
        }
        else
        {
            x = width;
            y = height;
        }
        if(context->win32_EventByPass)
            context->window.setCallEventsManager(context->win32_EventByPass);
        if(context->win32_joystickByPass)
            context->win32_joystickByPass->initJoystick(&context->window);
        HDC hdc = GetDC(context->window.getHwnd());
        // Create EGL display connection
        context->eglDisplay = eglGetDisplay(hdc);
        // Initialize EGL for this display, returns EGL version
        EGLint eglVersionMajor = 0;
        EGLint eglVersionMinor = 0;
        if(eglInitialize(context->eglDisplay, &eglVersionMajor, &eglVersionMinor) == EGL_FALSE)
        {
            ERROR_LOG(" EGL could not be initialized");
            return false;
        }
        if (device->isVerbose())
        {
            INFO_LOG("EGL version %d.%d", eglVersionMajor, eglVersionMinor);
        }
        if(eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE)
        {
            ERROR_LOG(" EGL could not be initialized");
            return false;
        }
        EGLint numConfigs = 0;
        EGLConfig windowConfig = nullptr;
        // With ANGLE, EGL config selection can directly change how 3D looks, even when your mesh / camera code is identical.
        // 
        //     Main reason :
        // 
        // EGL config decides the actual GPU surfaces(color + depth + stencil) that ANGLE creates.
        //     ANGLE then maps that to Direct3D resources on Windows.
        //     If the chosen config is not what you think(especially depth), 3D can look wrong while 2D still seems fine.

        static const EGLint attribs[] = {
            // 32 bit color
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            // at least 24 bit depth
            EGL_DEPTH_SIZE, 24,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            // want opengl-es 3.x conformant CONTEXT
            EGL_RENDERABLE_TYPE, (EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT),
            EGL_NONE};

        EGLBoolean result = eglChooseConfig(context->eglDisplay, attribs, &windowConfig, 1, &numConfigs);
        if (result != EGL_TRUE || numConfigs <= 0)
        {
            static const EGLint attribs_gl2[] = {
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_DEPTH_SIZE, 24,
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_NONE};
            result = eglChooseConfig(context->eglDisplay, attribs_gl2, &windowConfig, 1, &numConfigs);
            if (result != EGL_TRUE || numConfigs <= 0)
            {
                ERROR_LOG("eglChooseConfig failed for GLES (ES3/ES2 fallback)");
                return false;
            }
        }

        if (device->isVerbose())
        {
            EGLint cfgId = 0;
            EGLint depthBits = 0;
            EGLint stencilBits = 0;
            EGLint renderableType = 0;
            EGLint surfaceType = 0;
            eglGetConfigAttrib(context->eglDisplay, windowConfig, EGL_CONFIG_ID, &cfgId);
            eglGetConfigAttrib(context->eglDisplay, windowConfig, EGL_DEPTH_SIZE, &depthBits);
            eglGetConfigAttrib(context->eglDisplay, windowConfig, EGL_STENCIL_SIZE, &stencilBits);
            eglGetConfigAttrib(context->eglDisplay, windowConfig, EGL_RENDERABLE_TYPE, &renderableType);
            eglGetConfigAttrib(context->eglDisplay, windowConfig, EGL_SURFACE_TYPE, &surfaceType);
            INFO_LOG("EGL config selected: id=%d depth=%d stencil=%d renderable=0x%x surface=0x%x", cfgId, depthBits, stencilBits, renderableType, surfaceType);
            printGLString("GL renderer:\n", GL_RENDERER);
            printGLString("GL version:\n", GL_VERSION);
        }

        EGLint surfaceAttributes[] = { EGL_NONE };
        context->eglSurface = eglCreateWindowSurface(context->eglDisplay, windowConfig, context->window.getHwnd(), surfaceAttributes);
        //this->device->getSpecificContextDevice()->eglSurface = eglCreateWindowSurface(this->device->getSpecificContextDevice()->eglDisplay, windowConfig, device->window.getHwnd(), the_attribs);
        if(context->eglSurface == nullptr)
        {
            ERROR_LOG(" Could not create EGL Window surface");
            return false;
        }

        //EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        //this->device->getSpecificContextDevice()->eglContext = eglCreateContext(this->device->getSpecificContextDevice()->eglDisplay, windowConfig, NULL, contextAttributes);
        EGLint es3ContextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE, EGL_NONE};
        context->eglContext = eglCreateContext(context->eglDisplay, windowConfig, NULL, es3ContextAttribs);
        if(context->eglContext == nullptr)
        {
            ERROR_LOG(" Could not create EGL context");
            return false;
        }
        result = eglMakeCurrent(context->eglDisplay, context->eglSurface, context->eglSurface, context->eglContext);
        if(result != EGL_TRUE)
        {
            ERROR_LOG(" Could not make EGL context current");
            return false;
        }

        context->window.disableRender(mNativeWindow);
        if (device->isVerbose())
        {
            printGLString("\nversion:\n", GL_VERSION);
            printGLString("vendor:\n", GL_VENDOR);
            printGLString("renderer:\n", GL_RENDERER);
            //printGLStringNewLine("GL Extensions:\n", GL_EXTENSIONS, ' ');
            //printEGLStringNewLine(this->device->getSpecificContextDevice()->eglDisplay, ' ');
            
            MINIZ::showVersion();
            INFO_LOG("\nAudio engine: %s\n", AUDIO_ENGINE_version());
        }
        GLViewport(0, 0, x <= 0 ? 800 : x, y <= 0 ? 600 : y);
        GLClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        GLDepthRangef(0.0f, 1.0f);
        GLEnable(GL_CULL_FACE);
        GLCullFace(GL_BACK);//initial value, any mesh can decide it
        GLFrontFace(GL_CW); //initial value, any mesh can decide it
        GLEnable(GL_DEPTH_TEST);
        // GLDepthFunc(GL_GREATER);
        // GLDepthFunc(GL_LESS);
        GLDepthFunc(GL_LEQUAL);
        GLClearDepthf(1.0f);
        GLEnable(GL_BLEND);
        if (x > 0)
            device->setBackBufferWidth(static_cast<float>(x));
        if (y > 0)
            device->setBackBufferHeight(static_cast<float>(y));

        TEXTURE_MANAGER* texture_manager = TEXTURE_MANAGER::getInstance();
        GLint maxTextureSize = 0;
        GLGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        GLint maxVertexUniformVectors = 0, maxVertexAttributes = 0;
        GLGetIntegerv(GL_MAX_VERTEX_UNIFORM_VECTORS, &maxVertexUniformVectors);
        GLGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttributes);
        skeletal::setMeasuredGles2SkinningCapability(static_cast<uint32_t>(maxVertexUniformVectors),
                                                      static_cast<uint32_t>(maxVertexAttributes));
        const skeletal::GLES2_SKINNING_CAPABILITY skinning = skeletal::getMeasuredGles2SkinningCapability();
        INFO_LOG("GLES2 skeletal capability: vertexUniformVectors=%u vertexAttributes=%u LBS=%u DQS=%u",
                 skinning.maxVertexUniformVectors, skinning.maxVertexAttributes,
                 skinning.lbsMatrixPaletteBones, skinning.dqsRigidPaletteBones);
        //const GLint MaxTextureWidth = static_cast<GLint>(std::sqrt(static_cast<float>(maxTextureSize)));
        const GLint MaxTextureWidth = maxTextureSize;
        const GLint MaxTextureHeight = MaxTextureWidth;
        texture_manager->setTextureCapabilities(static_cast<const int32_t>(maxTextureSize), static_cast<const int32_t>(MaxTextureWidth), static_cast<const int32_t>(MaxTextureHeight));

        constexpr GLint index[2] = { GL_TEXTURE1, GL_TEXTURE0 };
        for (int i = 0; i < 2; i++)
        {
            GLActiveTexture(index[i]);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &context->filter_GL_TEXTURE_WRAP_S);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &context->filter_GL_TEXTURE_WRAP_T);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &context->filter_GL_TEXTURE_MIN_FILTER);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &context->filter_GL_TEXTURE_MAG_FILTER);

        return true;
    }
}

#endif // USE_OPENGL_ES
#endif // _WIN32
