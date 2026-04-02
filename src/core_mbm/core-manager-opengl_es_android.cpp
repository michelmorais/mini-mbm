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
#if defined(ANDROID)

#include <core-manager.h>
#include <mesh-manager.h>
#include <texture-manager.h>
#include <device.h>
#include <specific-opengl_es.h>
#include <core_mbm/util-interface.h>


namespace mbm
{
    void CORE_MANAGER::handleEventFromWindow()
    {
        // do nothing, events are handled from JNI
    }

    void CORE_MANAGER::ReleaseGraphics(const bool wasDeviceLost)
    {
        TEXTURE_MANAGER::getInstance()->release();
        MESH_MANAGER::getInstance()->release();
        this->device->specificContextDevice->release(wasDeviceLost);
    }

    void CORE_MANAGER::moveWindow(int , int )
    {
        // On Android, window movement is not applicable
    }

    bool CORE_MANAGER::initGraphics(const char *nameApplication, int width, int height, const int px, const int py, const bool border,const bool enable_resize)
    {
        auto* ctx = this->device->specificContextDevice;

        if (!ctx->nativeWindow)
        {
            ERROR_LOG("EGL: nativeWindow is null — cannot create EGL surface");
            return false;
        }

        // ---------------------------------------------------------------
        // EGL initialisation — must happen before any GL call
        // ---------------------------------------------------------------
        if (ctx->eglDisplay == EGL_NO_DISPLAY)
        {
            // First-time init: create display, context and window surface
            ctx->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            if (ctx->eglDisplay == EGL_NO_DISPLAY)
            {
                ERROR_LOG("EGL: eglGetDisplay failed (error 0x%x)", eglGetError());
                return false;
            }
            if (!eglInitialize(ctx->eglDisplay, nullptr, nullptr))
            {
                ERROR_LOG("EGL: eglInitialize failed (error 0x%x)", eglGetError());
                return false;
            }

            const EGLint configAttribs[] = {
                EGL_RED_SIZE,        8,
                EGL_GREEN_SIZE,      8,
                EGL_BLUE_SIZE,       8,
                EGL_DEPTH_SIZE,      24,
                EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_NONE
            };
            EGLint numConfigs = 0;
            eglChooseConfig(ctx->eglDisplay, configAttribs, &ctx->eglConfig, 1, &numConfigs);
            if (numConfigs == 0)
            {
                // Fallback: relax depth size
                INFO_LOG("EGL: depth-24 config not available, trying depth-16");
                const EGLint fallbackAttribs[] = {
                    EGL_RED_SIZE,        8,
                    EGL_GREEN_SIZE,      8,
                    EGL_BLUE_SIZE,       8,
                    EGL_DEPTH_SIZE,      16,
                    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
                    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                    EGL_NONE
                };
                eglChooseConfig(ctx->eglDisplay, fallbackAttribs, &ctx->eglConfig, 1, &numConfigs);
            }
            if (numConfigs == 0)
            {
                ERROR_LOG("EGL: eglChooseConfig found no matching config (error 0x%x)", eglGetError());
                return false;
            }
            INFO_LOG("EGL: config chosen, numConfigs=%d", numConfigs);

            EGLint format = 0;
            eglGetConfigAttrib(ctx->eglDisplay, ctx->eglConfig, EGL_NATIVE_VISUAL_ID, &format);
            ANativeWindow_setBuffersGeometry(ctx->nativeWindow, 0, 0, format);

            ctx->eglSurface = eglCreateWindowSurface(ctx->eglDisplay, ctx->eglConfig, ctx->nativeWindow, nullptr);
            if (ctx->eglSurface == EGL_NO_SURFACE)
            {
                ERROR_LOG("EGL: eglCreateWindowSurface failed (error 0x%x)", eglGetError());
                return false;
            }

            const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
            ctx->eglContext = eglCreateContext(ctx->eglDisplay, ctx->eglConfig, EGL_NO_CONTEXT, ctxAttribs);
            if (ctx->eglContext == EGL_NO_CONTEXT)
            {
                ERROR_LOG("EGL: eglCreateContext failed (error 0x%x)", eglGetError());
                return false;
            }

            if (!eglMakeCurrent(ctx->eglDisplay, ctx->eglSurface, ctx->eglSurface, ctx->eglContext))
            {
                ERROR_LOG("EGL: eglMakeCurrent failed (error 0x%x)", eglGetError());
                return false;
            }
            INFO_LOG("EGL: context created and made current");
        }
        else if (ctx->eglSurface == EGL_NO_SURFACE)
        {
            // Resume after device-lost: context is alive, only the surface needs recreation
            EGLint format = 0;
            eglGetConfigAttrib(ctx->eglDisplay, ctx->eglConfig, EGL_NATIVE_VISUAL_ID, &format);
            ANativeWindow_setBuffersGeometry(ctx->nativeWindow, 0, 0, format);
            ctx->eglSurface = eglCreateWindowSurface(ctx->eglDisplay, ctx->eglConfig, ctx->nativeWindow, nullptr);
            if (ctx->eglSurface == EGL_NO_SURFACE)
            {
                ERROR_LOG("EGL: resume eglCreateWindowSurface failed (error 0x%x)", eglGetError());
                return false;
            }
            if (!eglMakeCurrent(ctx->eglDisplay, ctx->eglSurface, ctx->eglSurface, ctx->eglContext))
            {
                ERROR_LOG("EGL: resume eglMakeCurrent failed (error 0x%x)", eglGetError());
                return false;
            }
            INFO_LOG("EGL: surface recreated for resume");
        }

        // Query actual surface dimensions and use them
        {
            EGLint surfW = 0, surfH = 0;
            eglQuerySurface(ctx->eglDisplay, ctx->eglSurface, EGL_WIDTH,  &surfW);
            eglQuerySurface(ctx->eglDisplay, ctx->eglSurface, EGL_HEIGHT, &surfH);
            INFO_LOG("EGL: surface dimensions %d x %d", surfW, surfH);
            if (surfW > 0) width  = surfW;
            if (surfH > 0) height = surfH;
        }

        // ---------------------------------------------------------------
        int x = width;
        int y = height;
        // Initialize window position
        device->windowPositionX = px;
        device->windowPositionY = py;
        this->nameApplication = nameApplication ? nameApplication : "Mini-mbm";
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
            device->backBufferWidth = static_cast<float>(x);
        if (y > 0)
            device->backBufferHeight = static_cast<float>(y);

        TEXTURE_MANAGER* texture_manager = TEXTURE_MANAGER::getInstance();
        GLint maxTextureSize = 0;
        GLGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        //const GLint MaxTextureWidth = static_cast<GLint>(std::sqrt(static_cast<float>(maxTextureSize)));
        const GLint MaxTextureWidth = maxTextureSize;
        const GLint MaxTextureHeight = MaxTextureWidth;
        texture_manager->setTextureCapabilities(static_cast<const uint32_t>(maxTextureSize), static_cast<const uint32_t>(MaxTextureWidth), static_cast<const uint32_t>(MaxTextureHeight));

        constexpr GLint index[2] = { GL_TEXTURE1, GL_TEXTURE0 };
        for (int i = 0; i < 2; i++)
        {
            GLActiveTexture(index[i]);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            GLTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        }

        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &device->specificContextDevice->filter_GL_TEXTURE_WRAP_S);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &device->specificContextDevice->filter_GL_TEXTURE_WRAP_T);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &device->specificContextDevice->filter_GL_TEXTURE_MIN_FILTER);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &device->specificContextDevice->filter_GL_TEXTURE_MAG_FILTER);

        return true;
    }
}

#endif // USE_OPENGL_ES
#endif //ANDROID
