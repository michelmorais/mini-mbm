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

#if (defined(__MINGW32__) || defined(__CYGWIN__) || defined(_WIN32))
#ifndef OPENGL_ES_WINDOWS_SPECIFIC_CONTEXT_H
#define OPENGL_ES_WINDOWS_SPECIFIC_CONTEXT_H
#if defined(USE_OPENGL_ES)

#include <specific-opengl_es.h>

#include <core-manager.h>
#include <joystick-win32/joystick-win32.h>
#include <platform/win32-platform.h>
#include <plusWindows/plusWindows.h>

namespace mbm
{
    struct SPECIFIC_AUX_CONTEXT_DEVICE
    {
        WINDOW window;
        DWORD idIcon;
        EGLDisplay eglDisplay;
        EGLSurface eglSurface;
        EGLContext eglContext;

        WIN_EVENT_BY_PASS *win32_EventByPass;
        WIN_JOYSTICK_BY_PASS *win32_joystickByPass;

        GLint filter_GL_TEXTURE_WRAP_S;
        GLint filter_GL_TEXTURE_WRAP_T;
        GLint filter_GL_TEXTURE_MIN_FILTER;
        GLint filter_GL_TEXTURE_MAG_FILTER;

        SPECIFIC_AUX_CONTEXT_DEVICE() noexcept
        {
            this->idIcon = 0;
            this->win32_joystickByPass = nullptr;
            this->win32_EventByPass = nullptr;
            this->eglDisplay = EGL_NO_DISPLAY;
            this->eglSurface = EGL_NO_SURFACE;
            this->eglContext = EGL_NO_CONTEXT;

            filter_GL_TEXTURE_WRAP_S     = GL_CLAMP_TO_EDGE;
            filter_GL_TEXTURE_WRAP_T     = GL_CLAMP_TO_EDGE;
            filter_GL_TEXTURE_MIN_FILTER = GL_NEAREST;
            filter_GL_TEXTURE_MAG_FILTER = GL_LINEAR;
        }

        ~SPECIFIC_AUX_CONTEXT_DEVICE() noexcept
        {
            constexpr bool wasDeviceLost = false;
            release(wasDeviceLost);
            // Do not release win32 events and joystick during device-loss release;
            // the core manager is still the same in that case.
            if (this->win32_EventByPass)
                delete this->win32_EventByPass;
            this->win32_EventByPass = nullptr;
            if (this->win32_joystickByPass)
                delete this->win32_joystickByPass;
            this->win32_joystickByPass = nullptr;
        }

        void initializeWi32Callbacks(CORE_MANAGER *core_manager_ptr)
        {
            if (this->win32_EventByPass)
                delete this->win32_EventByPass;
            this->win32_EventByPass = nullptr;
            if (this->win32_joystickByPass)
                delete this->win32_joystickByPass;
            this->win32_joystickByPass = nullptr;
            this->win32_EventByPass = new WIN_EVENT_BY_PASS(core_manager_ptr ? reinterpret_cast<EVENTS *>(core_manager_ptr) : nullptr);
            this->win32_joystickByPass = new WIN_JOYSTICK_BY_PASS(core_manager_ptr ? reinterpret_cast<JOYSTICK_BASE *>(core_manager_ptr) : nullptr);
        }

        void release(bool)
        {
            if (this->eglDisplay != EGL_NO_DISPLAY)
                eglTerminate(this->eglDisplay);
            if (this->eglSurface != EGL_NO_SURFACE)
                eglDestroySurface(this->eglDisplay, this->eglSurface);
            if (this->eglContext != EGL_NO_CONTEXT)
                eglDestroyContext(this->eglDisplay, this->eglContext);
            this->eglDisplay = EGL_NO_DISPLAY;
            this->eglSurface = EGL_NO_SURFACE;
            this->eglContext = EGL_NO_CONTEXT;
        }

        SPECIFIC_AUX_CONTEXT_DEVICE(const SPECIFIC_AUX_CONTEXT_DEVICE&) = delete;
        SPECIFIC_AUX_CONTEXT_DEVICE& operator=(const SPECIFIC_AUX_CONTEXT_DEVICE&) = delete;
    };
}

#endif // USE_OPENGL_ES
#endif // OPENGL_ES_WINDOWS_SPECIFIC_CONTEXT_H
#endif // Windows
