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
#if !defined(ANDROID)
#if defined(__linux__) || defined(__APPLE__)

#include <core-manager.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <device.h>
#include <specific-opengl_es.h>
#include <miniz-wrap/miniz-wrap.h>
#include <audio-interface.h>
#include <cassert>
#include <thread>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

#ifdef __APPLE__
//#include <X11/extensions/Xcomposite.h>
//#include <X11/Xmu/WinUtil.h>
#endif


namespace mbm
{
    bool CORE_MANAGER::initGraphics(const char *nameAplication, int width, int height, const int px, const int py, const bool border,const bool enable_resize)
    {
        int x = width;
        int y = height;
        this->nameAplication = nameAplication ? nameAplication : "Mini-mbm";
        char * dpyName = nullptr;
        EGLint egl_major = 0;
        EGLint egl_minor = 0;
        this->device->specificContextDevice->display_x11 = XOpenDisplay(dpyName);
        if (!this->device->specificContextDevice->display_x11)
        {
            printf("Error: couldn't open display %s\n", dpyName ? dpyName : getenv("DISPLAY"));
            return false;
        }
    #ifdef __APPLE__
        this->device->specificContextDevice->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        #pragma message("Check if this is correct for MacOS")
    #else
        this->device->specificContextDevice->eglDisplay = eglGetDisplay((EGLNativeDisplayType) this->device->specificContextDevice->display_x11);
    #endif
        if (!this->device->specificContextDevice->eglDisplay)
        {
            printf("Error: eglGetDisplay() failed\n");
            return false;
        }

        if (!eglInitialize(this->device->specificContextDevice->eglDisplay, &egl_major, &egl_minor))
        {
            printf("Error: eglInitialize() failed\n");
            return false;
        }
        Screen *screen = DefaultScreenOfDisplay(this->device->specificContextDevice->display_x11);
        if ((height + 60) >= screen->height)
        {
            height -= 60;
            y = height;
        }
        const int cx = screen ? (screen->width - width) / 2 : 0;
        const int cy = screen ? (screen->height - height) / 2 : 0;
        this->device->specificContextDevice->make_x_window(nameAplication, cx, cy, static_cast<uint32_t>(width), static_cast<uint32_t>(height), border);

        XMapWindow(this->device->specificContextDevice->display_x11, this->device->specificContextDevice->window_x11);
        if (!eglMakeCurrent(this->device->specificContextDevice->eglDisplay, this->device->specificContextDevice->eglSurface, this->device->specificContextDevice->eglSurface, this->device->specificContextDevice->eglContext))
        {
            printf("Error: eglMakeCurrent() failed\n");
            return false;
        }

        if (device->verbose)
        {
            printGLString("\nversion:\n", GL_VERSION);
            printGLString("vendor:\n", GL_VENDOR);
            printGLString("renderer:\n", GL_RENDERER);
            //printGLStringNewLine("Extensions:\n", GL_EXTENSIONS, ' ');
            //printEGLStringNewLine(this->device->specificContextDevice->display,' ');
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
            device->backBufferWidth = static_cast<float>(x);
        if (y > 0)
            device->backBufferHeight = static_cast<float>(y);

        TEXTURE_MANAGER* texture_manager = TEXTURE_MANAGER::getInstance();
        GLint maxTextureSize = 0;
        GLGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
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

        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &device->specificContextDevice->filter_GL_TEXTURE_WRAP_S);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &device->specificContextDevice->filter_GL_TEXTURE_WRAP_T);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &device->specificContextDevice->filter_GL_TEXTURE_MIN_FILTER);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &device->specificContextDevice->filter_GL_TEXTURE_MAG_FILTER);

        return true;
    }

    void CORE_MANAGER::initializeWindowx11()
    {
        XSelectInput(this->device->specificContextDevice->display_x11, this->device->specificContextDevice->window_x11,//ResizeRedirectMask ->resize (does not work properly on Linux)
            ResizeRedirectMask | (KeyPressMask | KeyReleaseMask) | (ButtonPressMask | ButtonReleaseMask) | (PointerMotionMask) /*| ExposureMask | StructureNotifyMask*/);
        XkbSetDetectableAutoRepeat(this->device->specificContextDevice->display_x11, true, nullptr);
        XMapWindow(this->device->specificContextDevice->display_x11, this->device->specificContextDevice->window_x11);
        XFlush(this->device->specificContextDevice->display_x11);

        XSizeHints xsize;
        xsize.flags = PMaxSize | PMinSize | USPosition; // only what we wish (for now not PMaxSize)
        xsize.min_width = static_cast<int>(device->backBufferWidth);
        xsize.min_height = static_cast<int>(device->backBufferHeight);
        xsize.max_width = static_cast<int>(device->backBufferWidth);
        xsize.max_height = static_cast<int>(device->backBufferHeight);
        xsize.base_width = static_cast<int>(device->backBufferWidth);
        xsize.base_height = static_cast<int>(device->backBufferHeight);
        xsize.width = static_cast<int>(device->backBufferWidth);
        xsize.height = static_cast<int>(device->backBufferHeight);
        xsize.width_inc = 0;
        xsize.height_inc = 0;
        xsize.x = 0;
        xsize.y = 0;
        XSetWMNormalHints(this->device->specificContextDevice->display_x11, this->device->specificContextDevice->window_x11, &xsize);
    }

    void CORE_MANAGER::handleEventFromWindow()
    {
        while (XPending(this->device->specificContextDevice->display_x11))
        {
            XEvent xevent;
            XNextEvent(this->device->specificContextDevice->display_x11, &xevent);
            switch (xevent.type)
            {
            case KeyPress:
            {
                auto key = static_cast<int>(XLookupKeysym(&xevent.xkey, 0));
                if (key >= 'a' && key <= 'z')
                    key = toupper(key);
                if (key == XK_Caps_Lock)
                    this->keyCapsLockState = ((xevent.xbutton.state & 2) == 0);// == 0 is on
                this->onKeyDown(key);
            }
            break;
            case KeyRelease:
            {
                auto key = static_cast<int>(XLookupKeysym(&xevent.xkey, 0));
                if (key >= 'a' && key <= 'z')
                    key = toupper(key);
                this->onKeyUp(key);
            }
            break;
            case ButtonPress:
            {
                switch (xevent.xbutton.button)
                {
                case Button1: this->onTouchDown(0, xevent.xbutton.x, xevent.xbutton.y); break;
                case Button2: this->onTouchDown(2, xevent.xbutton.x, xevent.xbutton.y); break;
                case Button3: this->onTouchDown(1, xevent.xbutton.x, xevent.xbutton.y); break;
                case 4: // zomm in
                    this->onTouchZoom(1.0f);
                    break;
                case 5: // zomm out
                    this->onTouchZoom(-1.0f);
                    break;
                }
            }
            break;
            case ButtonRelease:
            {
                switch (xevent.xbutton.button)
                {
                case Button1: this->onTouchUp(0, xevent.xbutton.x, xevent.xbutton.y); break;
                case Button2: this->onTouchUp(2, xevent.xbutton.x, xevent.xbutton.y); break;
                case Button3: this->onTouchUp(1, xevent.xbutton.x, xevent.xbutton.y); break;
                }
            }
            break;
            case MotionNotify:
            {
                this->onTouchMove(0, xevent.xmotion.x, xevent.xmotion.y);
            }
            break;
            case ResizeRequest:
            {
                XResizeRequestEvent xResize = xevent.xresizerequest;
                this->onResizeWindow(xResize.width, xResize.height);
            }
            break;
            default: 
            {
                //printf("Event: %d\n", xevent.type);
            }
                   break;
            }
        }
    }

    void CORE_MANAGER::getScreenSize(int *width,int *height)
    {
        Screen * screen = DefaultScreenOfDisplay(this->device->specificContextDevice->display_x11);
        if(screen)
        {
            *width  = screen->width;
            *height = screen->height;
        }
    }
   
    void SPECIFIC_AUX_CONTEXT_DEVICE::make_x_window(const char *name, int x, int y, uint32_t width,unsigned  int height, bool border)
    {
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

        static const EGLint es1ContextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 1, EGL_NONE};
        static const EGLint es2ContextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 2, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE, EGL_NONE};
        static const EGLint es3ContextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE, EGL_NONE};

        int                  scrnum;
        XSetWindowAttributes attr;
        unsigned long        mask;
        Window               root;
        XVisualInfo *        visInfo, visTemplate;
        int                  num_visuals;
        EGLConfig            config;
        EGLint               num_configs;
        EGLint               vid;

        scrnum = DefaultScreen(display_x11);
        root   = RootWindow(display_x11, scrnum);

        if (!eglChooseConfig(eglDisplay, attribs, &config, 1, &num_configs))
        {
            static const EGLint attribs_gl2[] = {
            // 32 bit color
            EGL_RED_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_BLUE_SIZE, 8,
            // at least 24 bit depth
            EGL_DEPTH_SIZE, 24,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            // want opengl-es 2.x conformant CONTEXT
            EGL_RENDERABLE_TYPE, (EGL_OPENGL_ES2_BIT),
            EGL_NONE};
            if (!eglChooseConfig(eglDisplay, attribs_gl2, &config, 1, &num_configs))
            {
                printf("Error: couldn't get an EGL visual config\n");
                exit(1);
            }
        }

        assert(config);
        assert(num_configs > 0);

        if (!eglGetConfigAttrib(eglDisplay, config, EGL_NATIVE_VISUAL_ID, &vid))
        {
            printf("Error: eglGetConfigAttrib() failed\n");
            exit(1);
        }

        /* The X window visual must match the EGL config */
        visTemplate.visualid = static_cast<VisualID>(vid);
        visInfo              = XGetVisualInfo(display_x11, VisualIDMask, &visTemplate, &num_visuals);
        if (!visInfo)
        {
            printf("Error: couldn't get X visual\n");
            exit(1);
        }

        /* window attributes */
        attr.background_pixel = 0;
        attr.border_pixel     = 0;
        attr.colormap         = XCreateColormap(display_x11, root, visInfo->visual, AllocNone);
        attr.event_mask       = StructureNotifyMask | ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask | ResizeRedirectMask;
        mask                  = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
        if(border == false)
        {
            attr.override_redirect= 1;
            mask                  = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask | CWOverrideRedirect;
        }

        window_x11 = static_cast<Window>(XCreateWindow(display_x11, root, x < 0 ? 0 : x, y < 0 ? 0 : y, width, height, 0, visInfo->depth, InputOutput,
                            visInfo->visual, mask, &attr));

        /* set hints and properties */
        {
            XSizeHints sizehints;
            sizehints.x      = x;
            sizehints.y      = y;
            sizehints.width  = static_cast<EGLint>(width);
            sizehints.height = static_cast<EGLint>(height);
            sizehints.flags  = USSize | USPosition;
            XSetNormalHints(display_x11, window_x11, &sizehints);
            XSetStandardProperties(display_x11, window_x11, name, name, None, nullptr, 0, &sizehints);
        }

#if defined USE_FULL_GL /* XXX fix this when eglBindAPI() works */
        eglBindAPI(EGL_OPENGL_API);
#else
        eglBindAPI(EGL_OPENGL_ES_API);
#endif

        this->eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, es3ContextAttribs);
        if (!this->eglContext)
        {
            this->eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, es2ContextAttribs);
            if (!this->eglContext)
            {
                this->eglContext = eglCreateContext(eglDisplay, config, EGL_NO_CONTEXT, es1ContextAttribs);
                if (!this->eglContext)
                {
                    printf("Error: eglCreateContext failed\n");
                    exit(1);
                }
                #ifndef USE_FULL_GL /* test eglQueryContext() */
                else
                {
                    EGLint val;
                    eglQueryContext(eglDisplay, this->eglContext, EGL_CONTEXT_CLIENT_VERSION, &val);
                    assert(val == 1);
                }
                #endif
            }
            #ifndef USE_FULL_GL /* test eglQueryContext() */
            else
            {
                EGLint val;
                eglQueryContext(eglDisplay, this->eglContext, EGL_CONTEXT_CLIENT_VERSION, &val);
                assert(val == 2);
            }
            #endif
        }
        #ifndef USE_FULL_GL /* test eglQueryContext() */
        else
        {
            EGLint val;
            eglQueryContext(eglDisplay, this->eglContext, EGL_CONTEXT_CLIENT_VERSION, &val);
            assert(val == 3);
        }
        #endif
        const EGLint *attrib_list = nullptr;
        this->eglSurface = eglCreateWindowSurface(eglDisplay, config, reinterpret_cast<EGLNativeWindowType>(window_x11), attrib_list);
        if (!this->eglSurface)
        {
            printf("Error: eglCreateWindowSurface failed\n");
            exit(1);
        }

        /* sanity checks */
        {
            EGLint val;
            eglQuerySurface(eglDisplay, this->eglSurface, EGL_WIDTH, &val);
            assert(val == static_cast<EGLint>(width));
            eglQuerySurface(eglDisplay, this->eglSurface, EGL_HEIGHT, &val);
            assert(val == static_cast<EGLint>(height));
            assert(eglGetConfigAttrib(eglDisplay, config, EGL_SURFACE_TYPE, &val));
            assert(val & EGL_WINDOW_BIT);
        }

        XFree(visInfo);
    }

    void CORE_MANAGER::ReleaseGraphics()
    {
        TEXTURE_MANAGER::getInstance()->release();
        MESH_MANAGER::getInstance()->release();
        this->device->specificContextDevice->release();
    }

}

#endif // USE_OPENGL_ES
#endif //!ANDROID
#endif //(__linux__) || defined(__APPLE__)