/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                      |
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

#if defined(USE_OPENGL_ES) && !defined(ANDROID) && (defined(__linux__) || defined(__APPLE__))
#ifndef OPENGL_ES_X11_SPECIFIC_CONTEXT_H
#define OPENGL_ES_X11_SPECIFIC_CONTEXT_H

#include <specific-opengl_es.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

namespace mbm
{
    struct SPECIFIC_AUX_CONTEXT_DEVICE
    {
        EGLDisplay eglDisplay;
        EGLSurface eglSurface;
        EGLContext eglContext;
        EGLConfig eglConfig;
        Window window_x11;
        Display *display_x11;

        GLint filter_GL_TEXTURE_WRAP_S;
        GLint filter_GL_TEXTURE_WRAP_T;
        GLint filter_GL_TEXTURE_MIN_FILTER;
        GLint filter_GL_TEXTURE_MAG_FILTER;

        void make_x_window(const char *name, const int px, const int py, const uint32_t width, const uint32_t height, const bool border, const bool enable_resize);
        bool recreateEGLSurface();

        SPECIFIC_AUX_CONTEXT_DEVICE() noexcept
        {
            this->eglDisplay = EGL_NO_DISPLAY;
            this->eglSurface = EGL_NO_SURFACE;
            this->eglContext = EGL_NO_CONTEXT;
            this->eglConfig = nullptr;
            this->window_x11 = 0;
            this->display_x11 = nullptr;

            filter_GL_TEXTURE_WRAP_S = GL_CLAMP_TO_EDGE;
            filter_GL_TEXTURE_WRAP_T = GL_CLAMP_TO_EDGE;
            filter_GL_TEXTURE_MIN_FILTER = GL_NEAREST;
            filter_GL_TEXTURE_MAG_FILTER = GL_LINEAR;
        }

        ~SPECIFIC_AUX_CONTEXT_DEVICE() noexcept
        {
            constexpr bool wasDeviceLost = false;
            release(wasDeviceLost);
        }

        void release(bool wasDeviceLost)
        {
            if (this->eglDisplay != EGL_NO_DISPLAY)
                eglTerminate(this->eglDisplay);
            if (this->eglSurface != EGL_NO_SURFACE)
                eglDestroySurface(this->eglDisplay, this->eglSurface);
            if (this->eglContext != EGL_NO_CONTEXT)
                eglDestroyContext(this->eglDisplay, this->eglContext);
            if (wasDeviceLost == false)
            {
                if (this->display_x11 != nullptr && this->window_x11 != 0)
                {
                    XDestroyWindow(this->display_x11, this->window_x11);
                    XCloseDisplay(this->display_x11);
                    this->display_x11 = nullptr;
                    this->window_x11 = 0;
                }
            }
            this->eglDisplay = EGL_NO_DISPLAY;
            this->eglSurface = EGL_NO_SURFACE;
            this->eglContext = EGL_NO_CONTEXT;
            this->eglConfig = nullptr;
        }

        SPECIFIC_AUX_CONTEXT_DEVICE(const SPECIFIC_AUX_CONTEXT_DEVICE&) = delete;
        SPECIFIC_AUX_CONTEXT_DEVICE& operator=(const SPECIFIC_AUX_CONTEXT_DEVICE&) = delete;
    };
}

#endif // OPENGL_ES_X11_SPECIFIC_CONTEXT_H
#endif // OpenGL ES X11
