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

#include <core-manager.h>
#include <device.h>
#include <renderizable.h>
#include <texture-manager.h>
#include <mesh-manager.h>
#include <gles-debug.h>
#include <util-interface.h>
#include <audio-interface.h>
#include <version/version.h>
#include <miniz-wrap/miniz-wrap.h>
#include <cassert>
#include <algorithm>
#include <cstring>
#include <log-util.h>

#if defined(ANDROID)
#include <platform/common-jni.h>
#elif defined(__linux__)
    #include <thread>
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <X11/XKBlib.h>
    //#include <X11/extensions/Xcomposite.h>
    //#include <X11/Xmu/WinUtil.h>
#elif defined(_WIN32)
    #include <GLES2/gl2ext.h>
#endif

#include <plugin-callback.h>
#include <dynamic-var.h>

namespace mbm
{
enum WHICH_FOR : char
{
    WFOR_INITIAL,
    WFOR_2DS,
    WFOR_2DW,
    WFOR_3D,
    WFOR_DONE
};

enum STEP_RETORE : char
{
    STEP_RES_INIT_GL,
    STEP_RES_DRAW_HOURGLASS,
    STEP_RES_OBJ,
    STEP_RES_END,
};

void printGLString(const char *name, GLenum s);
void printGLStringNewLine(const char *name, GLenum s, const char delimit);

#if !defined (ANDROID)
void printEGLStringNewLine(EGLDisplay eglDisplay,const char delimit);
#endif

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

    #if defined(ANDROID) || defined(__linux__)

    EVENTS::EVENTS() noexcept
    = default;
    EVENTS::~EVENTS() = default;

#endif

    constexpr EVENT_KEY::EVENT_KEY() noexcept : x(0), y(0), key(0), player(0), rx(0), ry(0), eventType(UNKNOWN)
    {}
        
    constexpr EVENT_KEY::EVENT_KEY(const float _x, const float _y, const int _key, const EVENT_TYPE_ACTIONS _eventName) noexcept
        : x(_x),
            y(_y),
            key(_key),
            player(0),
            rx(0.0f),
            ry(0.0f),
            eventType(_eventName)
    {}
    constexpr EVENT_KEY::EVENT_KEY(const float _lx, const float _ly, const int _key, const int _player, const float _rx,
                        const float _ry, const EVENT_TYPE_ACTIONS _eventName) noexcept : lx(_lx),
                                                                                            ly(_ly),
                                                                                            key(_key),
                                                                                            player(_player),
                                                                                            rx(_rx),
                                                                                            ry(_ry),
                                                                                            eventType(_eventName)
    {}

    INFO_JOYSTICK_INIT_PLAYER::INFO_JOYSTICK_INIT_PLAYER() : player(0), maxNumberButton(0)
    {}

    INFO_JOYSTICK_INIT_PLAYER::INFO_JOYSTICK_INIT_PLAYER(const int _player, const int _maxNumberButton, const char *_deviceName,
                                const char *_extraInfo)
        : player(_player), maxNumberButton(_maxNumberButton), deviceName(_deviceName), extraInfo(_extraInfo)
    {}

    CORE_MANAGER::CORE_MANAGER()
    {
        this->device           = DEVICE::getInstance();
        this->indexOnRestore   = 0;
        this->totalForByLoop   = 0;
        this->percentRestoreInfo = 0.0f;
        this->stepRestoreInfo  = 0.1f;
        this->stepRestore      = STEP_RES_INIT_GL;
        this->which_for        = WFOR_INITIAL;
        this->changeScene      = true;
        this->__sceneWasInit   = false;
        this->keyCapsLockState = false;
    #if defined(_WIN32)
        idIcon = 0;
    #elif defined(__linux__) && !defined (ANDROID)
        this->win      = 0;
        this->egl_surf = nullptr;
        this->egl_ctx  = nullptr;
        this->egl_dpy  = nullptr;
        this->display  = nullptr;
    #endif
    }
    
    CORE_MANAGER::~CORE_MANAGER()
    {
        DEVICE::quit();
    #if defined(__linux__) && !defined(ANDROID)
        eglDestroyContext(this->egl_dpy, this->egl_ctx);
        eglDestroySurface(this->egl_dpy, this->egl_surf);
        eglTerminate(this->egl_dpy);

        XDestroyWindow(this->display, this->win);
        XCloseDisplay(this->display);
    #endif
    }
    
    void CORE_MANAGER::setScene(SCENE *currentScene)
    {
        this->device->scene = currentScene;
    }
    
    void CORE_MANAGER::onStop()
    {
        for (auto ptr : this->device->lsObjectRender2DS)
        {
            ptr->onStop();
        }
        for (auto ptr : this->device->lsObjectRender2DW)
        {
            ptr->onStop();
        }
        for (auto ptr : this->device->lsObjectRender3D)
        {
            ptr->onStop();
        }
        TEXTURE_MANAGER::getInstance()->release();
        MESH_MANAGER::getInstance()->release();
        this->device->pauseGame();
    }

#if defined ANDROID
    bool CORE_MANAGER::onLostDevice(JNIEnv *jenv, jobject , int width, int height)
#else
    bool CORE_MANAGER::onLostDevice(int width, int height,const int px,const int py)
#endif
    {
#ifdef ANDROID
        device->jni->jenv = jenv;
#endif
        if (stepRestore == STEP_RES_INIT_GL)
        {
            #if defined _DEBUG
            ERROR_LOG("onLostDevice step %d",stepRestore);
            #endif
#ifndef ANDROID
    #define __nameAplication "Mini-mbm " MBM_VERSION " GLES"
#endif
#if defined(_WIN32)
            if (initGl(__nameAplication, width, height,px,py, false,false))
#elif defined(ANDROID)
            if (initGl(width, height))
#elif defined(__linux__)
            (void)px;
            (void)py;
            if (width && height)
#else
            #error "undefined platform"
#endif
            {
                #if defined _DEBUG
                    WARN_LOG("onLostDevice step %d function initGl sucess!",stepRestore);
                #endif
                
                this->device->__percXcam2dScale = 1.0f / this->device->camera.scale2d.x;
                this->device->__percYcam2dScale = 1.0f / this->device->camera.scale2d.y;
                this->adjustScaleScreen2d();
                stepRestore = STEP_RES_DRAW_HOURGLASS;
                return false;
            }
            else
            {
                #if defined _DEBUG
                    WARN_LOG("onLostDevice step %d function initGl failed!",stepRestore);
                #endif
                return false;
            }
        }
        else if (stepRestore == STEP_RES_DRAW_HOURGLASS)
        {
            #if defined _DEBUG
                WARN_LOG("onLostDevice step %d draw Hourglass.",stepRestore);
            #endif
            device->setProjectionMode(false, device->backBufferWidth, device->backBufferHeight);
            device->setDephtTest(false);
            GLClearColor(device->colorClearBackGround.r, device->colorClearBackGround.g, device->colorClearBackGround.b,
                         device->colorClearBackGround.a);
            GLClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear The Screen And The Depth Buffer
            GLClearDepthf(1.0f);
            if (device->scene)
                device->scene->onRestore(0); //true means: no call restore,  just to prepare the screen.
            stepRestore = STEP_RES_OBJ;
            this->which_for =  WFOR_INITIAL;
            #if defined(_WIN32) 
            eglSwapBuffers(this->eglDisplay,this->eglSurface);
            #elif defined(__linux__) && !defined(ANDROID)
                eglSwapBuffers(egl_dpy, egl_surf);
            #endif
            return false;
        }
        else if (stepRestore == STEP_RES_OBJ)
        {
            device->setProjectionMode(false, device->backBufferWidth, device->backBufferHeight);
            device->setDephtTest(false);
            GLClearColor(device->colorClearBackGround.r, device->colorClearBackGround.g, device->colorClearBackGround.b,
                         device->colorClearBackGround.a);
            GLClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear The Screen And The Depth Buffer
            GLClearDepthf(1.0f);
            switch(this->which_for)
            {
                case WFOR_INITIAL:
                {
                    #if defined _DEBUG
                        WARN_LOG("onLostDevice step %d restoring objs.",stepRestore);
                    #endif
                    const auto t = static_cast<float>(this->device->lsObjectRender2DW.size() + this->device->lsObjectRender2DS.size() + this->device->lsObjectRender3D.size());
                    if(t > 0.0f)
                    {
                        this->totalForByLoop = static_cast<uint32_t>(std::ceil(t / 60.0f));//1 seconds should be loaded all objects
                        this->stepRestoreInfo = 98.0f /  t * static_cast<float>(this->totalForByLoop);
                    }
                    else
                    {
                        this->stepRestoreInfo = 0.001f;
                        this->totalForByLoop = 1;
                    }
                    this->percentRestoreInfo = 0.0f;
                    this->which_for = WFOR_2DW;
                    this->indexOnRestore = 0;
                    return false;
                }
                break;
                case WFOR_2DW:
                {
                    for (uint32_t i = this->indexOnRestore, j = 0; 
                    i < this->device->lsObjectRender2DW.size(); 
                    ++i)
                    {
                        RENDERIZABLE *ptr             = this->device->lsObjectRender2DW[i];
                        const bool    alwaysRenderize = ptr->alwaysRenderize;
                        const bool    enableRender    = ptr->enableRender;
                        ptr->alwaysRenderize          = false;
                        ptr->enableRender             = false;
                        if (ptr->onRestoreDevice())
                        {
                            ptr->alwaysRenderize = alwaysRenderize;
                            ptr->enableRender    = enableRender;
                        }
                        if(++j >= this->totalForByLoop)
                        {
                            this->indexOnRestore = (i + 1);
                            this->percentRestoreInfo += this->stepRestoreInfo;
                            if (device->scene)
                                device->scene->onRestore(static_cast<int>(std::ceil(this->percentRestoreInfo > 98.9f ? 98.9f : this->percentRestoreInfo)));
                            #if defined(_WIN32) 
                                eglSwapBuffers(eglDisplay, eglSurface);
                            #elif defined(__linux__) && !defined(ANDROID)
                                eglSwapBuffers(egl_dpy, egl_surf);
                            #endif
                            return false;
                        }
                    }
                    this->indexOnRestore = 0;
                    this->which_for = WFOR_2DS;
                    return false;
                }
                break;
                case WFOR_2DS:
                {
                    for (uint32_t i = this->indexOnRestore, j = 0; 
                        i < this->device->lsObjectRender2DS.size(); ++i)
                    {
                        RENDERIZABLE *ptr             = this->device->lsObjectRender2DS[i];
                        const bool    alwaysRenderize = ptr->alwaysRenderize;
                        const bool    enableRender    = ptr->enableRender;
                        ptr->alwaysRenderize          = false;
                        ptr->enableRender             = false;
                        if (ptr->onRestoreDevice())
                        {
                            ptr->alwaysRenderize = alwaysRenderize;
                            ptr->enableRender    = enableRender;
                        }
                        if(++j >= this->totalForByLoop)
                        {
                            this->indexOnRestore = (i + 1);
                            this->percentRestoreInfo += this->stepRestoreInfo;
                            if (device->scene)
                                device->scene->onRestore(static_cast<int>(std::ceil(this->percentRestoreInfo > 98.9f ? 98.9f : this->percentRestoreInfo)));
                            #if defined(_WIN32) 
                                eglSwapBuffers(this->eglDisplay,this->eglSurface);
                            #elif defined(__linux__) && !defined(ANDROID)
                                eglSwapBuffers(egl_dpy, egl_surf);
                            #endif
                            return false;
                        }
                    }
                    this->indexOnRestore = 0;
                    this->which_for = WFOR_3D;
                    return false;
                }
                break;
                case WFOR_3D:
                {
                    for (uint32_t i = this->indexOnRestore, j = 0; 
                    i < this->device->lsObjectRender3D.size(); ++i)
                    {
                        RENDERIZABLE *ptr             = this->device->lsObjectRender3D[i];
                        const bool    alwaysRenderize = ptr->alwaysRenderize;
                        const bool    enableRender    = ptr->enableRender;
                        ptr->alwaysRenderize          = false;
                        ptr->enableRender             = false;
                        if (ptr->onRestoreDevice())
                        {
                            ptr->alwaysRenderize = alwaysRenderize;
                            ptr->enableRender    = enableRender;
                        }
                        if(++j >= this->totalForByLoop)
                        {
                            this->indexOnRestore = (i + 1);
                            this->percentRestoreInfo += this->stepRestoreInfo;
                            if (device->scene)
                                device->scene->onRestore(static_cast<int>(std::ceil(this->percentRestoreInfo > 98.9f ? 98.9f : this->percentRestoreInfo)));
                            #if defined(_WIN32) 
                                eglSwapBuffers(this->eglDisplay,this->eglSurface);
                            #elif defined(__linux__) && !defined(ANDROID)
                                eglSwapBuffers(egl_dpy, egl_surf);
                            #endif
                            return false;
                        }
                    }
                    this->indexOnRestore = 0;
                    this->which_for = WFOR_DONE;
                }
                break;
                default:{};
            }
            stepRestore = STEP_RES_END;
            #if defined(_WIN32) 
            eglSwapBuffers(this->eglDisplay,this->eglSurface);
            #elif defined(__linux__) && !defined(ANDROID)
            eglSwapBuffers(egl_dpy, egl_surf);
            #endif
            return false;
        }
        else if (stepRestore == STEP_RES_END)
        {
            #if defined _DEBUG
                WARN_LOG("onLostDevice step %d resumeGame",stepRestore);
            #endif
            stepRestore             = STEP_RES_INIT_GL;
            device->clearBackGround = true;
            this->device->resumeGame();
            this->device->resumeTimer();
            if (device->scene)
                device->scene->onRestore(100);
            return true;
        }
        return false;
    }

#if defined(__linux__) && !defined(ANDROID)
    
    void CORE_MANAGER::make_x_window(Display *display, EGLDisplay egl_dpy, const char *name, int x, int y, uint32_t width,unsigned  int height,
                              Window *winRet, EGLContext *ctxRet, EGLSurface *surfRet,bool border)
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
        Window               win;
        XVisualInfo *        visInfo, visTemplate;
        int                  num_visuals;
        EGLContext           ctx;
        EGLConfig            config;
        EGLint               num_configs;
        EGLint               vid;

        scrnum = DefaultScreen(display);
        root   = RootWindow(display, scrnum);

        if (!eglChooseConfig(egl_dpy, attribs, &config, 1, &num_configs))
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
            if (!eglChooseConfig(egl_dpy, attribs_gl2, &config, 1, &num_configs))
            {
                printf("Error: couldn't get an EGL visual config\n");
                exit(1);
            }
        }

        assert(config);
        assert(num_configs > 0);

        if (!eglGetConfigAttrib(egl_dpy, config, EGL_NATIVE_VISUAL_ID, &vid))
        {
            printf("Error: eglGetConfigAttrib() failed\n");
            exit(1);
        }

        /* The X window visual must match the EGL config */
        visTemplate.visualid = static_cast<VisualID>(vid);
        visInfo              = XGetVisualInfo(display, VisualIDMask, &visTemplate, &num_visuals);
        if (!visInfo)
        {
            printf("Error: couldn't get X visual\n");
            exit(1);
        }

        /* window attributes */
        attr.background_pixel = 0;
        attr.border_pixel     = 0;
        attr.colormap         = XCreateColormap(display, root, visInfo->visual, AllocNone);
        attr.event_mask       = StructureNotifyMask | ExposureMask | KeyPressMask | ResizeRedirectMask;
        mask                  = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
        if(border == false)
        {
            attr.override_redirect= 1;
            mask                  = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask | CWOverrideRedirect;
        }

        win = static_cast<Window>(XCreateWindow(display, root, x < 0 ? 0 : x, y < 0 ? 0 : y, width, height, 0, visInfo->depth, InputOutput,
                            visInfo->visual, mask, &attr));

        /* set hints and properties */
        {
            XSizeHints sizehints;
            sizehints.x      = x;
            sizehints.y      = y;
            sizehints.width  = static_cast<EGLint>(width);
            sizehints.height = static_cast<EGLint>(height);
            sizehints.flags  = USSize | USPosition;
            XSetNormalHints(display, win, &sizehints);
            XSetStandardProperties(display, win, name, name, None, nullptr, 0, &sizehints);
        }

#if defined USE_FULL_GL /* XXX fix this when eglBindAPI() works */
        eglBindAPI(EGL_OPENGL_API);
#else
        eglBindAPI(EGL_OPENGL_ES_API);
#endif

        ctx = eglCreateContext(egl_dpy, config, EGL_NO_CONTEXT, es3ContextAttribs);
        if (!ctx)
        {
            ctx = eglCreateContext(egl_dpy, config, EGL_NO_CONTEXT, es2ContextAttribs);
            if (!ctx)
            {
                ctx = eglCreateContext(egl_dpy, config, EGL_NO_CONTEXT, es1ContextAttribs);
                if (!ctx)
                {
                    printf("Error: eglCreateContext failed\n");
                    exit(1);
                }
                #ifndef USE_FULL_GL /* test eglQueryContext() */
                else
                {
                    EGLint val;
                    eglQueryContext(egl_dpy, ctx, EGL_CONTEXT_CLIENT_VERSION, &val);
                    assert(val == 1);
                }
                #endif
            }
            #ifndef USE_FULL_GL /* test eglQueryContext() */
            else
            {
                EGLint val;
                eglQueryContext(egl_dpy, ctx, EGL_CONTEXT_CLIENT_VERSION, &val);
                assert(val == 2);
            }
            #endif
        }
        #ifndef USE_FULL_GL /* test eglQueryContext() */
        else
        {
            EGLint val;
            eglQueryContext(egl_dpy, ctx, EGL_CONTEXT_CLIENT_VERSION, &val);
            assert(val == 3);
        }
        #endif

        *surfRet = eglCreateWindowSurface(egl_dpy, config, win, nullptr);
        if (!*surfRet)
        {
            printf("Error: eglCreateWindowSurface failed\n");
            exit(1);
        }

        /* sanity checks */
        {
            EGLint val;
            eglQuerySurface(egl_dpy, *surfRet, EGL_WIDTH, &val);
            assert(val == static_cast<EGLint>(width));
            eglQuerySurface(egl_dpy, *surfRet, EGL_HEIGHT, &val);
            assert(val == static_cast<EGLint>(height));
            assert(eglGetConfigAttrib(egl_dpy, config, EGL_SURFACE_TYPE, &val));
            assert(val & EGL_WINDOW_BIT);
        }

        XFree(visInfo);

        *winRet = win;
        *ctxRet = ctx;
    }
#endif

#if defined(_WIN32) 
    bool CORE_MANAGER::initGl(const char *nameAplication, int width, int height, const int px, const int py, const bool border,const bool enable_resize)
#elif defined (ANDROID)
    bool CORE_MANAGER::initGl(const int width, const int height)
#elif defined __linux__
    bool CORE_MANAGER::initGl(const char *nameAplication, int width, int height, const bool border)
#else
    #error "undefined initGl"
    bool CORE_MANAGER::initGl()
#endif
    {
        int x = width;
        int y = height;
#ifdef _WIN32
        device->window.setNameAplication(nameAplication);
        if (!this->device->window.init(nameAplication, x, y, px, py, enable_resize, enable_resize, enable_resize, false, nullptr, border == false,
                                       this->idIcon,false))
        {
            this->device->window.messageBox("error on init app ... will be closed ");
            PRINT_IF_DEBUG( "error on init app ... will be closed %s", "error on create window");
            return false;
        }
        this->device->window.setMinSizeAllowed(800,600);
        HWND mNativeWindow = this->device->window.getHwnd();
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
        this->device->window.setCallEventsManager(this);
        this->initJoystick(&this->device->window);

        HDC hdc = GetDC(device->window.getHwnd());
        // Create EGL display connection
        this->eglDisplay = eglGetDisplay(hdc);
        // Initialize EGL for this display, returns EGL version
        EGLint eglVersionMajor = 0;
        EGLint eglVersionMinor = 0;
        if(eglInitialize(this->eglDisplay, &eglVersionMajor, &eglVersionMinor) == EGL_FALSE)
        {
            ERROR_LOG(" EGL could not be initialized");
            return false;
        }
        if(device->verbose)
            INFO_LOG("EGL version %d.%d",eglVersionMajor,eglVersionMinor);
        if(eglBindAPI(EGL_OPENGL_ES_API) == EGL_FALSE)
        {
            ERROR_LOG(" EGL could not be initialized");
            return false;
        }
        EGLint numConfigs = 0;
        EGLConfig windowConfig = nullptr;
        
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

        /*if ( EGL_FALSE == eglGetConfigs(this->eglDisplay, NULL, 0, &numConfigs) )
        {
            ERROR_LOG("Could not get number of all configs");
            return false;
        }

        // collect information about the configs
        EGLConfig *configs = new EGLConfig[numConfigs];
        if ( EGL_FALSE == eglGetConfigs(this->eglDisplay,configs,numConfigs,&numConfigs) )
        {
            delete [] configs;
            ERROR_LOG("Could not get number all configs");
            return false;
        }

        struct MY_CONFIG
        {
            EGLint _red_size;
            EGLint _green_size;
            EGLint _blue_size;
            EGLint _alpha_size;
            EGLint _bind_to_texture_rgb;
            EGLint _bind_to_texture_rgba;
            EGLint _buffer_size;
            EGLint _config_caveat;
            EGLint _config_id;
            EGLint _depth_size;
            EGLint _level;
            EGLint _max_pbuffer_width;
            EGLint _max_pbuffer_height;
            EGLint _max_pbuffer_pixels;
            EGLint _max_swap_interval;
            EGLint _min_swap_interval;
            EGLint _native_renderable;
            EGLint _native_vrenderable;
            EGLint _alpha_mask_size;
            EGLint _color_buffer_type;
            EGLint _luminance_size;
            EGLint _renderable_type;
            EGLint _conformant;
            EGLint _egl_robust;
        };

        std::vector<MY_CONFIG> _bufferFormats;
        MY_CONFIG newFormat;
 
        for ( GLint c = 0 ; c < numConfigs ; ++c)
        {
            memset(&newFormat,0,sizeof(newFormat));
            EGLConfig config = configs[c];
            eglGetConfigAttrib( this->eglDisplay, config, EGL_RED_SIZE, &(newFormat._red_size));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_BLUE_SIZE, &(newFormat._blue_size));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_GREEN_SIZE, &(newFormat._green_size));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_ALPHA_SIZE, &(newFormat._alpha_size));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_BIND_TO_TEXTURE_RGB, &(newFormat._bind_to_texture_rgb));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_BIND_TO_TEXTURE_RGBA, &(newFormat._bind_to_texture_rgba));
            
            eglGetConfigAttrib( this->eglDisplay, config, EGL_BUFFER_SIZE, &(newFormat._buffer_size));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_CONFIG_CAVEAT, &(newFormat._config_caveat));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_CONFIG_ID, &(newFormat._config_id));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_DEPTH_SIZE, &(newFormat._depth_size));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_GREEN_SIZE, &(newFormat._green_size));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_LEVEL, &(newFormat._level));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_MAX_PBUFFER_WIDTH, &(newFormat._max_pbuffer_width));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_MAX_PBUFFER_HEIGHT, &(newFormat._max_pbuffer_height));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_MAX_PBUFFER_PIXELS, &(newFormat._max_pbuffer_pixels));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_MAX_SWAP_INTERVAL, &(newFormat._max_swap_interval));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_MIN_SWAP_INTERVAL, &(newFormat._min_swap_interval));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_NATIVE_RENDERABLE, &(newFormat._native_renderable));
            eglGetConfigAttrib( this->eglDisplay, config, EGL_NATIVE_VISUAL_ID, &(newFormat._native_vrenderable));
            /// etc etc etc for all those that you care about
 
            if ( eglVersionMajor >= 1 && eglVersionMinor >= 2 )
            {       
                // 1.2
                eglGetConfigAttrib( this->eglDisplay, config, EGL_ALPHA_MASK_SIZE, &(newFormat._alpha_mask_size));
                eglGetConfigAttrib( this->eglDisplay, config, EGL_COLOR_BUFFER_TYPE, &(newFormat._color_buffer_type));
                eglGetConfigAttrib( this->eglDisplay, config, EGL_LUMINANCE_SIZE, &(newFormat._luminance_size));
                eglGetConfigAttrib( this->eglDisplay, config, EGL_RENDERABLE_TYPE, &(newFormat._renderable_type));
            }
 
            if ( eglVersionMajor >= 1 && eglVersionMinor >= 3 )
            {
                // 1.3
                //const char * ext = eglQueryString(this->eglDisplay,EGL_EXTENSIONS);
                eglGetConfigAttrib( this->eglDisplay, config, EGL_CONFORMANT, &(newFormat._conformant));
                eglGetConfigAttrib( this->eglDisplay, config, EGL_CONTEXT_OPENGL_ROBUST_ACCESS, &(newFormat._egl_robust));
                
                //eglQueryString (configs[i], EGL_COLOR_COMPONENT_TYPE_EXT,
                //                   &config.colorComponentType, "EGL_EXT_pixel_format_float",
                //                   EGL_COLOR_COMPONENT_TYPE_FIXED_EXT);
            }
            _bufferFormats.push_back(newFormat);
        }

        delete [] configs;
        configs = nullptr;

        MY_CONFIG m = _bufferFormats[3];
        EGLint the_attribs[] = 
           {EGL_RED_SIZE, m._red_size,
            EGL_GREEN_SIZE, m._green_size,
            EGL_BLUE_SIZE, m._blue_size,
            EGL_ALPHA_SIZE, m._alpha_size,
            EGL_BIND_TO_TEXTURE_RGB, m._bind_to_texture_rgb,
            EGL_BIND_TO_TEXTURE_RGBA, m._bind_to_texture_rgba,
            EGL_BUFFER_SIZE, m._buffer_size,
            EGL_CONFIG_CAVEAT, m._config_caveat,
            EGL_CONFIG_ID, m._config_id,
            EGL_DEPTH_SIZE, m._depth_size,
            EGL_LEVEL, m._level,
            EGL_MAX_PBUFFER_WIDTH, m._max_pbuffer_width,
            EGL_MAX_PBUFFER_HEIGHT, m._max_pbuffer_height,
            EGL_MAX_PBUFFER_PIXELS, m._max_pbuffer_pixels,
            EGL_MAX_SWAP_INTERVAL, m._max_swap_interval,
            EGL_MIN_SWAP_INTERVAL, m._min_swap_interval,
            EGL_NATIVE_RENDERABLE, m._native_renderable,
            EGL_NATIVE_VISUAL_ID, m._native_vrenderable,
            EGL_ALPHA_MASK_SIZE, m._alpha_mask_size,
            EGL_COLOR_BUFFER_TYPE, m._color_buffer_type,
            EGL_LUMINANCE_SIZE, m._luminance_size,
            EGL_RENDERABLE_TYPE, m._renderable_type,
            EGL_CONFORMANT, m._conformant,
            EGL_CONTEXT_OPENGL_ROBUST_ACCESS, m._egl_robust,
            EGL_NONE
            };

        auto a = EGL_OPENGL_ES2_BIT;
        auto b = EGL_OPENGL_ES3_BIT;
        for(int n=0; n <numConfigs;++n)
        {
            MY_CONFIG m = _bufferFormats[n];
            if(m._red_size    == 8 &&
                m._green_size == 8 &&
                m._blue_size  == 8 &&
                m._alpha_size == 8 &&
                (m._conformant  == EGL_OPENGL_ES3_BIT || m._conformant  == (EGL_OPENGL_ES2_BIT | EGL_OPENGL_ES3_BIT) &&  // compatible with OpenGL ES 3.x., EGL_OPENGL_ES2_BIT -> compatible with OpenGL ES 2.x., EGL_OPENGL_ES_BIT -> compatible with OpenGL ES 1.x.
                m._max_swap_interval <= 4 )
                //m._egl_robust == EGL_TRUE )
                )
            {
                EGLint new_the_attribs[]= {EGL_RED_SIZE, m._red_size,
            EGL_GREEN_SIZE, m._green_size,
            EGL_BLUE_SIZE, m._blue_size,
            EGL_ALPHA_SIZE, m._alpha_size,
            EGL_BIND_TO_TEXTURE_RGB, m._bind_to_texture_rgb,
            EGL_BIND_TO_TEXTURE_RGBA, m._bind_to_texture_rgba,
            EGL_BUFFER_SIZE, m._buffer_size,
            EGL_CONFIG_CAVEAT, m._config_caveat,
            EGL_CONFIG_ID, m._config_id,
            EGL_DEPTH_SIZE, m._depth_size,
            EGL_LEVEL, m._level,
            EGL_MAX_PBUFFER_WIDTH, m._max_pbuffer_width,
            EGL_MAX_PBUFFER_HEIGHT, m._max_pbuffer_height,
            EGL_MAX_PBUFFER_PIXELS, m._max_pbuffer_pixels,
            EGL_MAX_SWAP_INTERVAL, m._max_swap_interval,
            EGL_MIN_SWAP_INTERVAL, m._min_swap_interval,
            EGL_NATIVE_RENDERABLE, m._native_renderable,
            EGL_NATIVE_VISUAL_ID, m._native_vrenderable,
            EGL_ALPHA_MASK_SIZE, m._alpha_mask_size,
            EGL_COLOR_BUFFER_TYPE, m._color_buffer_type,
            EGL_LUMINANCE_SIZE, m._luminance_size,
            EGL_RENDERABLE_TYPE, m._renderable_type,
            EGL_CONFORMANT, m._conformant,
            //EGL_CONTEXT_OPENGL_ROBUST_ACCESS, m._egl_robust,
            EGL_NONE
            };
                memcpy(the_attribs,new_the_attribs,sizeof(new_the_attribs));
                break;
            }

        }
        //auto pFn     = (PFNGLMAPBUFFEROESPROC)eglGetProcAddress("ANGLEGetDisplayPlatform"); it works
        auto pFn     = (PFNGLMAPBUFFEROESPROC)eglGetProcAddress("ANGLEGetDisplayPlatform");
        EGLBoolean result = eglChooseConfig(this->eglDisplay, the_attribs, &windowConfig, 1, &numConfigs);
        */
        EGLBoolean result = eglChooseConfig(this->eglDisplay, attribs, &windowConfig, 1, &numConfigs);
        
        //EGLBoolean result = eglChooseConfig(this->eglDisplay, attribs, &windowConfig, 1, &numConfigs);
        switch (result )
        {
            case EGL_TRUE:break;
            case EGL_FALSE:
                ERROR_LOG(" eglChooseConfig returned false");
            break;
            case EGL_BAD_DISPLAY :
                ERROR_LOG(" eglChooseConfig returned EGL_BAD_DISPLAY");
            break;
            case EGL_BAD_ATTRIBUTE :
                ERROR_LOG(" eglChooseConfig returned EGL_BAD_ATTRIBUTE");
            break;
            case EGL_NOT_INITIALIZED :
                ERROR_LOG(" eglChooseConfig returned EGL_NOT_INITIALIZED");
            break;
            case EGL_BAD_PARAMETER :
                ERROR_LOG(" eglChooseConfig returned EGL_BAD_PARAMETER");
            break;
            default:
                ERROR_LOG(" eglChooseConfig returned %d",result);
            break;
        }
        if(result != EGL_TRUE)
        {
            return false;
        }

        EGLint surfaceAttributes[] = { EGL_NONE };
        this->eglSurface = eglCreateWindowSurface(this->eglDisplay, windowConfig, device->window.getHwnd(), surfaceAttributes);
        //this->eglSurface = eglCreateWindowSurface(this->eglDisplay, windowConfig, device->window.getHwnd(), the_attribs);
        if(this->eglSurface == nullptr)
        {
            ERROR_LOG(" Could not create EGL Window surface");
            return false;
        }

        //EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
	    //this->eglContext = eglCreateContext(this->eglDisplay, windowConfig, NULL, contextAttributes);
        EGLint es3ContextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE, EGL_NONE};
        this->eglContext = eglCreateContext(this->eglDisplay, windowConfig, NULL, es3ContextAttribs);
        if(this->eglContext == nullptr)
        {
            ERROR_LOG(" Could not create EGL context");
            return false;
        }
        result = eglMakeCurrent(this->eglDisplay, this->eglSurface, this->eglSurface, this->eglContext);
        if(result != EGL_TRUE)
        {
            ERROR_LOG(" Could not make EGL context current");
            return false;
        }

        this->device->window.disableRender(mNativeWindow);
		if (device->verbose)
		{
			printGLString("\nversion:\n", GL_VERSION);
			printGLString("vendor:\n", GL_VENDOR);
			printGLString("renderer:\n", GL_RENDERER);
            //printGLStringNewLine("GL Extensions:\n", GL_EXTENSIONS, ' ');
            //printEGLStringNewLine(this->eglDisplay, ' ');
            
			MINIZ::showVersion();
            INFO_LOG("\nAudio engine: %s\n", AUDIO_ENGINE_version());
		}
#elif defined(__linux__) && !defined(ANDROID)

        char * dpyName = nullptr;
        EGLint egl_major = 0;
        EGLint egl_minor = 0;
        this->display = XOpenDisplay(dpyName);
        if (!this->display)
        {
            printf("Error: couldn't open display %s\n", dpyName ? dpyName : getenv("DISPLAY"));
            return false;
        }

        egl_dpy = eglGetDisplay(this->display);
        if (!egl_dpy)
        {
            printf("Error: eglGetDisplay() failed\n");
            return false;
        }

        if (!eglInitialize(egl_dpy, &egl_major, &egl_minor))
        {
            printf("Error: eglInitialize() failed\n");
            return false;
        }
        Screen *screen = DefaultScreenOfDisplay(this->display);
        if ((height + 60) >= screen->height)
        {
            height -= 60;
            y = height;
        }
        const int px = screen ? (screen->width - width) / 2 : 0;
        const int py = screen ? (screen->height - height) / 2 : 0;
        make_x_window(this->display, egl_dpy, nameAplication, px, py, static_cast<uint32_t>(width), static_cast<uint32_t>(height), &win, &egl_ctx, &egl_surf,border);

        XMapWindow(this->display, win);
        if (!eglMakeCurrent(egl_dpy, egl_surf, egl_surf, egl_ctx))
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
        //printEGLStringNewLine(this->display,' ');
		MINIZ::showVersion();
        INFO_LOG("\nAudio engine: %s\n", AUDIO_ENGINE_version());
	}

#endif
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
        return true;
    }

#ifdef ANDROID
    int CORE_MANAGER::loop(JNIEnv *, jobject)
    {
        static bool variablesInitialized = false;
        if (!device)
            return -1;
        if (!variablesInitialized)
        {
            // Cfg shader from memory----
            if (!this->device->cfg.parserCFGFromResource())
            {
                PRINT_IF_DEBUG( "\nerror on Parse CFG from memory.");
                return -1;
            }
            this->device->cfg.sortShader();
            device->setProjectionMode(true, device->backBufferWidth, device->backBufferHeight);
            this->device->updateFps();
            initEnableRenders();
            this->_updateDimFrustum();
            variablesInitialized                  = true;
            this->device->camera.expectedScreen.x = this->device->backBufferWidth;
            this->device->camera.expectedScreen.y = this->device->backBufferHeight;
        }
        this->update();
        this->render();
        return 0;
    }

#elif (defined(_WIN32) || defined (__MINGW32__))

    int CORE_MANAGER::loop()
    {
        static bool variablesInitialized = false;
        if (!device)
            return -1;
        if (!variablesInitialized)
        {
             // Cfg shader from memory----
            if (!this->device->cfg.parserCFGFromResource())
            {
                PRINT_IF_DEBUG( "\nerror on Parse CFG from memory.");
                return -1;
            }
            this->device->cfg.sortShader();
            device->setProjectionMode(true, device->backBufferWidth, device->backBufferHeight);
            this->device->updateFps();
            initEnableRenders();
            this->_updateDimFrustum();
            variablesInitialized                  = true;
            this->device->camera.expectedScreen.x = this->device->backBufferWidth;
            this->device->camera.expectedScreen.y = this->device->backBufferHeight;
        }
        MSG messageMain;
        memset(&messageMain, 0, sizeof(messageMain));
        while (messageMain.message != WM_QUIT && device->run && this->device->window.run)
        {
            this->device->window.doEvents();
            bool first_menu = true;
            while (mbm::WINDOW::isAnyMenuVisible() && device->window.run)
            {
                if (first_menu)
                {
                    Sleep(50);
                    mbm::WINDOW::refreshMenu();
                }
                this->device->window.doEvents();
                if (first_menu)
                {
                    Sleep(50);
                    mbm::WINDOW::refreshMenu();
                }
                first_menu = false;
            }
            if (!this->device->window.run)
                break;

            INFO_JOYSTICK_INIT_PLAYER info;
            while (this->popEvent(&info))
            {
                if (this->device->scene && this->__sceneWasInit)
                    this->device->scene->onInfoDeviceJoystick(info.player, info.maxNumberButton, info.deviceName.c_str(),
                                                              info.extraInfo.c_str());
            }
            for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
            {
                PLUGIN * plugin = this->lsPlugins[i];
                plugin->onBeginRender();
            }
            EVENT_KEY event;
            while (this->popEvent(&event))
            {
                switch (event.eventType)
                {
                    case UNKNOWN: {
                    }
                    break;
                    case ONRESIZEWINDOW:
                    {
                        const GLsizei width  = static_cast<int>(event.x);
                        const GLsizei height = static_cast<int>(event.y);
                        if(width > 0 && height > 0 && (width != static_cast<GLsizei>(this->device->backBufferWidth) || height != static_cast<GLsizei>(this->device->backBufferHeight)))
                        {
                            glViewport(0, 0, width, height);
                            if(glIsEnabled (GL_SCISSOR_TEST))
                            {
                                glScissor(0, 0, width, height);
                            }
                            this->device->backBufferWidth  = event.x;
                            this->device->backBufferHeight = event.y;
                            if(this->device->scene)
                                this->device->scene->onResizeWindow();
                            for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                            {
                                PLUGIN * plugin = this->lsPlugins[i];
                                plugin->onResizeWindow(static_cast<int>(event.x),static_cast<int>(event.y));
                            }
                        }
                    }
                    break;
                    case ONTOUCHDOWN:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchDown(event.key, event.x, event.y);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onTouchDown(event.key, event.x, event.y);
                        }
                    }
                    break;
                    case ONTOUCHUP:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchUp(event.key, event.x, event.y);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onTouchUp(event.key, event.x, event.y);
                        }
                    }
                    break;
                    case ONTOUCHMOVE:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchMove(event.key, event.x, event.y);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onTouchMove(event.key, event.x, event.y);
                        }
                    }
                    break;
                    case ONTOUCHZOOM:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchZoom((float)event.key);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onTouchZoom((float)event.key);
                        }
                    }
                    break;
                    case ONKEYDOWN:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyDown(event.key);
                        if(event.key == VK_CAPITAL)
                        {
                            if ((GetKeyState(VK_CAPITAL) & 0x0001)!=0)
                                this->keyCapsLockState = true;
                            else
                                this->keyCapsLockState = false;
                        }
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onKeyDown(event.key);
                        }
                    }
                    break;
                    case ONKEYUP:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyUp(event.key);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onKeyUp(event.key);
                        }
                    }
                    break;
                    case ONDOUBLECLICK:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onDoubleClick(event.x, event.y, event.key);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onDoubleClick(event.x, event.y, event.key);
                        }
                    }
                    break;
                    case ONSTREAMSTOPED: {
                    }
                    break;
                    case ONCALLBACKCOMMANDS: {
                    }
                    break;
                    case ONKEYDOWNJOYSTICK:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyDownJoystick(event.player, event.key);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onKeyDownJoystick(event.player, event.key);
                        }
                    }
                    break;
                    case ONKEYUPJOYSTICK:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyUpJoystick(event.player, event.key);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onKeyUpJoystick(event.player, event.key);
                        }
                    }
                    break;
                    case ONMOVEJOYSTICK:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onMoveJoystick(event.player, event.lx, event.ly, event.rx, event.ry);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onMoveJoystick(event.player, event.lx, event.ly, event.rx, event.ry);
                        }
                    }
                    break;
                }
                if (!this->device->run)
                {
                    break;
                }
            }
            
            this->update();
            for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
            {
                PLUGIN * plugin = this->lsPlugins[i];
                plugin->onLoop(this->device->delta);
            }
            this->render();
            for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
            {
                PLUGIN * plugin = this->lsPlugins[i];
                plugin->onEndRender();
            }
            eglSwapBuffers(this->eglDisplay,this->eglSurface);
        }
        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
        {
            PLUGIN * plugin = this->lsPlugins[i];
            plugin->onDestroy();
        }
        if(this->device->audioInterface)
            this->device->audioInterface->stopAll();
        eglDestroyContext(this->eglDisplay,this->eglContext);
        eglDestroySurface(this->eglDisplay, this->eglSurface);
        return 0;
    }

#elif defined(__linux__)

    int CORE_MANAGER::loop()
    {
        static bool variablesInitialized = false;
        if (!device)
            return -1;
        if (!variablesInitialized)
        {
            // Cfg shader from memory----
            if (!this->device->cfg.parserCFGFromResource())
            {
                PRINT_IF_DEBUG( "\nerror on Parse CFG from memory.");
                return -1;
            }
            this->device->cfg.sortShader();
            device->setProjectionMode(true, device->backBufferWidth, device->backBufferHeight);
            this->device->updateFps();
            initEnableRenders();
            this->_updateDimFrustum();
            variablesInitialized                  = true;
            this->device->camera.expectedScreen.x = this->device->backBufferWidth;
            this->device->camera.expectedScreen.y = this->device->backBufferHeight;
        }
        XSelectInput(this->display, this->win,//ResizeRedirectMask ->resize (does not work properly on Linux)
                     ResizeRedirectMask |(KeyPressMask | KeyReleaseMask) | (ButtonPressMask | ButtonReleaseMask) | (PointerMotionMask) /*| ExposureMask | StructureNotifyMask*/);
        XkbSetDetectableAutoRepeat(this->display, true, nullptr);
        XMapWindow(this->display, win);
        XFlush(this->display);
        
        XSizeHints xsize;
        xsize.flags         = PMaxSize|PMinSize|USPosition; // only what we wish (for now not PMaxSize)
        xsize.min_width     = static_cast<int>(device->backBufferWidth);
        xsize.min_height    = static_cast<int>(device->backBufferHeight);
        xsize.max_width     = static_cast<int>(device->backBufferWidth);
        xsize.max_height    = static_cast<int>(device->backBufferHeight);
        xsize.base_width    = static_cast<int>(device->backBufferWidth);
        xsize.base_height   = static_cast<int>(device->backBufferHeight);
        xsize.width         = static_cast<int>(device->backBufferWidth);
        xsize.height        = static_cast<int>(device->backBufferHeight);
        xsize.width_inc     = 0;
        xsize.height_inc    = 0;
        xsize.x             = 0;
        xsize.y             = 0;
        XSetWMNormalHints(this->display,this->win,&xsize);

        while (this->device->run)
        {
            while(XPending(this->display))
            {
                XEvent xevent;
                XNextEvent(this->display, &xevent);
                switch (xevent.type)
                {
                    case KeyPress:
                    {
                        auto key = static_cast<int>(XLookupKeysym(&xevent.xkey, 0));
                        if (key >= 'a' && key <= 'z')
                            key = toupper(key);
                        if(key == XK_Caps_Lock)
                            this->keyCapsLockState =  ((xevent.xbutton.state & 2) == 0);// == 0 is on
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
                        this->onResizeWindow(xResize.width,xResize.height);
                    }
                    break;
                    default: {}
                    break;
                }
            }
            for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
            {
                PLUGIN * plugin = this->lsPlugins[i];
                plugin->onBeginRender();
            }
            EVENT_KEY event;
            while (this->popEvent(&event))
            {
                switch (event.eventType)
                {
                    case UNKNOWN: {
                    }
                    break;
                    case ONRESIZEWINDOW:
                    {
                        const GLsizei width  = static_cast<int>(event.x);
                        const GLsizei height = static_cast<int>(event.y);
                        if(width > 0 && height > 0 && (width != static_cast<GLsizei>(this->device->backBufferWidth) || height != static_cast<GLsizei>(this->device->backBufferHeight)))
                        {
                            glViewport(0, 0, width, height);
                            if(glIsEnabled (GL_SCISSOR_TEST))
                            {
                                INFO_LOG("glIsEnabled is enabled");
                                glScissor(0, 0, width, height);
                            }
                            this->device->backBufferWidth  = event.x;
                            this->device->backBufferHeight = event.y;
                            if(this->device->scene)
                            {
                                INFO_LOG("Resizing window to %dx%d not working properly on Linux",static_cast<int>(event.x),static_cast<int>(event.y));
                                this->device->scene->onResizeWindow();
                            }
                            for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                            {
                                PLUGIN * plugin = this->lsPlugins[i];
                                plugin->onResizeWindow(static_cast<int>(event.x),static_cast<int>(event.y));
                            }
                        }
                    }
                    break;
                    case ONTOUCHDOWN:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchDown(event.key, event.x, event.y);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onTouchDown(event.key, event.x, event.y);
                        }
                    }
                    break;
                    case ONTOUCHUP:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchUp(event.key, event.x, event.y);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onTouchUp(event.key, event.x, event.y);
                        }
                    }
                    break;
                    case ONTOUCHMOVE:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchMove(event.key, event.x, event.y);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onTouchMove(event.key, event.x, event.y);
                        }
                    }
                    break;
                    case ONTOUCHZOOM:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onTouchZoom(static_cast<float>(event.key));
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onTouchZoom(static_cast<float>(event.key));
                        }
                    }
                    break;
                    case ONKEYDOWN:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyDown(event.key);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onKeyDown(event.key);
                        }
                    }
                    break;
                    case ONKEYUP:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyUp(event.key);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onKeyUp(event.key);
                        }
                    }
                    break;
                    case ONDOUBLECLICK:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onDoubleClick(event.x, event.y, event.key);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onDoubleClick(event.x, event.y, event.key);
                        }
                    }
                    break;
                    case ONSTREAMSTOPED: {
                    }
                    break;
                    case ONCALLBACKCOMMANDS: {
                    }
                    break;
                    case ONKEYDOWNJOYSTICK:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyDownJoystick(event.player, event.key);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onKeyDownJoystick(event.player, event.key);
                        }
                    }
                    break;
                    case ONKEYUPJOYSTICK:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onKeyUpJoystick(event.player, event.key);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onKeyUpJoystick(event.player, event.key);
                        }
                    }
                    break;
                    case ONMOVEJOYSTICK:
                    {
                        if (this->device->scene && this->__sceneWasInit)
                            this->device->scene->onMoveJoystick(event.player, event.lx, event.ly, event.rx, event.ry);
                        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                        {
                            PLUGIN * plugin = this->lsPlugins[i];
                            plugin->onMoveJoystick(event.player, event.lx, event.ly, event.rx, event.ry);
                        }
                    }
                    break;
                }
                if (!this->device->run)
                {
                    break;
                }
            }
            this->update();
            for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
            {
                PLUGIN * plugin = this->lsPlugins[i];
                plugin->onLoop(this->device->delta);
            }
            this->render();
            for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
            {
                PLUGIN * plugin = this->lsPlugins[i];
                plugin->onEndRender();
            }
            eglSwapBuffers(egl_dpy, egl_surf);
        }
        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
        {
            PLUGIN * plugin = this->lsPlugins[i];
            plugin->onDestroy();
        }
        if(this->device->audioInterface)
            this->device->audioInterface->stopAll();
        return 0;
    }
#else
    #error "platform not suported"
#endif

//Linux thread
#if defined(__linux__) && !defined(ANDROID)

    void CORE_MANAGER::getScreenSize(int *width,int *height)
    {
        Screen * screen = DefaultScreenOfDisplay(this->display);
        if(screen)
        {
            *width  = screen->width;
            *height = screen->height;
        }
    }
#endif
    
    void CORE_MANAGER::update()
    {
        if (!device->run)
            return;
        this->device->updateFps();
        this->device->__percXcam2dScale = 1.0f / this->device->camera.scale2d.x;
        this->device->__percYcam2dScale = 1.0f / this->device->camera.scale2d.y;
        this->adjustScaleScreen2d();
        this->logic();
        this->updatePhysis();
        this->updateAudio();
    }
    
    bool CORE_MANAGER::renderToTargets()
    {
        bool oneRender = false;
        for (auto renderTarget : this->device->lsObjectRenderToTarget)
        {
            if (!renderTarget->isObjectOnFrustum)
                continue;
            GLViewport(0, 0, static_cast<GLsizei>(renderTarget->widthTexture), static_cast<GLsizei>(renderTarget->heightTexture));
            GLBindFramebuffer(GL_FRAMEBUFFER, renderTarget->idFrameBuffer);
            GLFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, static_cast<GLuint>(renderTarget->idTextureDynamic),0);
            GLFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,renderTarget->idDepthRenderbuffer);
            GLClearColor(renderTarget->colorClearBackGround.r, renderTarget->colorClearBackGround.g,
                         renderTarget->colorClearBackGround.b, renderTarget->colorClearBackGround.a);
            GLClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            GLClearDepthf(1.0f);
            const GLenum status = GLCheckFramebufferStatus(GL_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE)
            {
                GLBindFramebuffer(GL_FRAMEBUFFER, 0);
                GLViewport(0, 0, static_cast<GLsizei>(device->backBufferWidth), static_cast<GLsizei>(device->backBufferHeight));
                this->device->camera.updateCam(true, static_cast<float>(device->backBufferWidth), static_cast<float>(device->backBufferHeight));
                return false;
            }
            if (!renderTarget->render2Texture())
            {
                GLBindFramebuffer(GL_FRAMEBUFFER, 0);
                GLViewport(0, 0, static_cast<GLsizei>(device->backBufferWidth), static_cast<GLsizei>(device->backBufferHeight));
                this->device->camera.updateCam(true, static_cast<float>(device->backBufferWidth), static_cast<float>(device->backBufferHeight));
                return false;
            }
            GLBindTexture(GL_TEXTURE_2D, 0);
            GLBindFramebuffer(GL_FRAMEBUFFER, 0);
            GLBindRenderbuffer(GL_RENDERBUFFER, 0);
            oneRender = true;
        }
        if (oneRender)
        {
            GLViewport(0, 0, static_cast<GLsizei>(device->backBufferWidth), static_cast<GLsizei>(device->backBufferHeight));
            this->device->camera.updateCam(true, static_cast<float>(device->backBufferWidth), static_cast<float>(device->backBufferHeight));
        }
        return true;
    }

    
    void CORE_MANAGER::prepareRender2d(std::vector<RENDERIZABLE *> &lsAllObjects2d,
                                std::vector<RENDERIZABLE *> &lsRenderOnFrustum2d)
    {
        const std::vector<RENDERIZABLE*>::size_type total2d = lsAllObjects2d.size();
        for (std::vector<RENDERIZABLE*>::size_type i = 0; i < total2d; ++i)
        {
            RENDERIZABLE *ptr = lsAllObjects2d[i];
            if (ptr)
            {
                ptr->updateAABB();
                if (ptr->isRender2Texture)
                {
                    ptr->isObjectOnFrustum = false;
                }
                else if (!ptr->enableRender)
                {
                    ptr->isObjectOnFrustum = false;
                }
                else if (ptr->alwaysRenderize)
                {
                    ptr->isObjectOnFrustum = true;
                }
                else if (ptr->isOnFrustum())
                {
                    ptr->isObjectOnFrustum = true;
                }
                else
                {
                    ptr->isObjectOnFrustum = false;
                }
                if (ptr->isObjectOnFrustum)
                {
                    lsRenderOnFrustum2d.push_back(ptr);
                    ptr->__distFromView = ptr->position.z;
                }
            }
        }
        std::sort(lsRenderOnFrustum2d.begin(), lsRenderOnFrustum2d.end(),
                  [](const RENDERIZABLE *a, const RENDERIZABLE *b) { return b->__distFromView < a->__distFromView; });
    }
    
    void CORE_MANAGER::prepareRender3d(std::vector<RENDERIZABLE *> &lsAllObjects3d,
                                std::vector<RENDERIZABLE *> &lsRenderOnFrustum3d)
    {
        mbm::DEVICE *      device  = mbm::DEVICE::getInstance();
        const std::vector<RENDERIZABLE*>::size_type total3d = lsAllObjects3d.size();
        for (std::vector<RENDERIZABLE*>::size_type i = 0; i < total3d; ++i)
        {
            RENDERIZABLE *ptr = lsAllObjects3d[i];
            if (ptr)
            {
                ptr->updateAABB();
                if (ptr->isRender2Texture)
                {
                    ptr->isObjectOnFrustum = false;
                }
                else if (!ptr->enableRender)
                {
                    ptr->isObjectOnFrustum = false;
                }
                else if (ptr->alwaysRenderize)
                {
                    ptr->isObjectOnFrustum = true;
                }
                else if (ptr->isOnFrustum())
                {
                    ptr->isObjectOnFrustum = true;
                }
                else
                {
                    ptr->isObjectOnFrustum = false;
                }
                if (ptr->isObjectOnFrustum)
                {
                    lsRenderOnFrustum3d.push_back(ptr);
                    const VEC3 distFromCam(ptr->position - device->camera.position);
                    ptr->__distFromView = distFromCam.length();
                }
            }
        }
        std::sort(lsRenderOnFrustum3d.begin(), lsRenderOnFrustum3d.end(),
                  [](const RENDERIZABLE *a, const RENDERIZABLE *b) { return b->__distFromView < a->__distFromView; });
    }

    
    void CORE_MANAGER::render()
    {
        if (!device)
            return;
        if (!device->run)
            return;
        std::vector<RENDERIZABLE *> lsRender2ds;
        std::vector<RENDERIZABLE *> lsRender2dw;
        std::vector<RENDERIZABLE *> lsRender3d;
        // Atualiza a camera de acordo com a
        // projeção----
        device->setProjectionMode(true, device->backBufferWidth, device->backBufferHeight);
        // prepara para renderizar os objeto --
        device->totalObjectsIsRendering3D = 0;
        device->totalObjectsOnFrustum3D   = 0;
        device->totalObjects3D            = static_cast<uint32_t>(this->device->lsObjectRender3D.size());
        device->totalObjectsIsRendering2D = 0;
        device->totalObjectsOnFrustum2D   = 0;
        const auto total2ds       = static_cast<uint32_t>(this->device->lsObjectRender2DS.size());
        const auto total2dw       = static_cast<uint32_t>(this->device->lsObjectRender2DW.size());
        device->totalObjects2D            = total2ds + total2dw;

#if defined USE_THREAD
        std::thread thread2ds(prepareRender2d, std::ref(this->device->lsObjectRender2DS), std::ref(lsRender2ds));
        std::thread thread2dw(prepareRender2d, std::ref(this->device->lsObjectRender2DW), std::ref(lsRender2dw));
        std::thread thread3d(prepareRender3d, std::ref(this->device->lsObjectRender3D), std::ref(lsRender3d));
        if (thread2ds.joinable())
            thread2ds.join();
        if (thread2dw.joinable())
            thread2dw.join();
        if (thread3d.joinable())
            thread3d.join();
#else
        prepareRender2d(std::ref(this->device->lsObjectRender2DS), std::ref(lsRender2ds)); //-V525
        prepareRender2d(std::ref(this->device->lsObjectRender2DW), std::ref(lsRender2dw));
        prepareRender3d(std::ref(this->device->lsObjectRender3D), std::ref(lsRender3d));
#endif

        device->totalObjectsOnFrustum2D = static_cast<uint32_t>(lsRender2ds.size() + lsRender2dw.size());
        device->totalObjectsOnFrustum3D = static_cast<uint32_t>(lsRender3d.size());
        
        if (!this->renderToTargets())
            return;
        
        if (device->clearBackGround)
        {
            GLClearColor(device->colorClearBackGround.r, device->colorClearBackGround.g, device->colorClearBackGround.b,
                         device->colorClearBackGround.a);
            GLClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clear The Screen And The Depth Buffer
            GLClearDepthf(1.0f);
        }
        device->updateFrustum(&this->device->camera.matrixView, &this->device->camera.matrixProj);
        device->camera.updateNormalsRelativeCam();
        device->camera.calculateAzimuthFromCamera();
        this->device->camera.matrixBillboard = this->device->camera.matrixView; // Obtemos a Matrix De Vista Do Vista 3D
        MatrixInverse(&this->device->camera.matrixBillboard, nullptr, &this->device->camera.matrixBillboard);
        device->totalObjectsIsRendering3D = 0;
        for (auto ptrRender : lsRender3d)
        {
            if (ptrRender->render())
                ++device->totalObjectsIsRendering3D;
        }
        
        device->setProjectionMode(false, device->backBufferWidth, device->backBufferHeight);
        device->totalObjectsIsRendering2D = 0;
        device->setDephtTest(true);
        for (auto ptrRender : lsRender2dw)
        {
            if (ptrRender->render())
                device->totalObjectsIsRendering2D++;
        }
        device->setDephtTest(false);
        for (auto ptrRender : lsRender2ds)
        {
            if (ptrRender->render())
                ++device->totalObjectsIsRendering2D;
        }
        device->setDephtTest(true);
        
    }
    
    void CORE_MANAGER::_updateDimFrustum()
    {
        VEC3 point(0, 0, 50);
        this->device->dimNearFrustum3d = VEC3(0, 0, 20);
        this->device->dimFarFrustum3d  = VEC3(0, 0, 980);
        this->device->camera.updateCam(true, this->device->backBufferWidth, this->device->backBufferHeight);
        this->device->updateFrustum(&this->device->camera.matrixView, &this->device->camera.matrixProj);
        while (this->device->isPointAtTheFrustum(point))
        {
            point.x += 0.5f;
        }
        this->device->dimNearFrustum3d.x = point.x * 2.0f;

        point = VEC3(0, 0, 50);
        while (this->device->isPointAtTheFrustum(point))
        {
            point.y += 0.5f;
        }
        this->device->dimNearFrustum3d.y = point.y * 2.0f;

        point = VEC3(0, 0, 980);
        while (this->device->isPointAtTheFrustum(point))
        {
            point.x += 0.5f;
        }
        this->device->dimFarFrustum3d.x = point.x * 2.0f;

        point = VEC3(0, 0, 980);
        while (this->device->isPointAtTheFrustum(point))
        {
            point.y += 0.5f;
        }
        this->device->dimFarFrustum3d.y = point.y * 2.0f;
    }
    
    void CORE_MANAGER::adjustScaleScreen2d()
    {
        if (this->device->camera.expectedScreen.x != 0.0f && this->device->camera.expectedScreen.y != 0.0f) //-V550
        {
            const float percx = this->device->backBufferWidth / this->device->camera.expectedScreen.x;
            const float percy = this->device->backBufferHeight / this->device->camera.expectedScreen.y;
            if (percx != 0.0f && percy != 0.0f) //-V550
            {
                if (this->device->camera.stretch[0])
                {
                    if (strcmp(this->device->camera.stretch, "x") == 0)
                    {
                        this->device->camera.scaleScreen2d.x = percx;
                        this->device->camera.scaleScreen2d.y = percx;
                    }
                    else if (strcmp(this->device->camera.stretch, "y") == 0)
                    {
                        this->device->camera.scaleScreen2d.x = percy;
                        this->device->camera.scaleScreen2d.y = percy;
                    }
                    else if (strcmp(this->device->camera.stretch, "xy") == 0)
                    {
                        this->device->camera.scaleScreen2d.x = percx;
                        this->device->camera.scaleScreen2d.y = percy;
                    }
                    else if (percx < percy)
                    {
                        this->device->camera.scaleScreen2d.x = percx;
                        this->device->camera.scaleScreen2d.y = percx;
                    }
                    else
                    {
                        this->device->camera.scaleScreen2d.x = percy;
                        this->device->camera.scaleScreen2d.y = percy;
                    }
                }
                else if (percx < percy)
                {
                    this->device->camera.scaleScreen2d.x = percx;
                    this->device->camera.scaleScreen2d.y = percx;
                }
                else
                {
                    this->device->camera.scaleScreen2d.x = percy;
                    this->device->camera.scaleScreen2d.y = percy;
                }
            }
        }
    }
    
    void CORE_MANAGER::updateAudio()
    {
		if(this->device->audioInterface)
			this->device->audioInterface->update(this,this->device->scene->getIdScene());
    }
    
    void CORE_MANAGER::updatePhysis()
    {
        if (!this->device->scene)
            return;
        const float        fps            = this->device->delta == 0.0f ? 0.0f : this->device->fps; //-V550
        const int          idCurrentScene = this->device->scene->getIdScene();
        const std::vector<PHYSICS*>::size_type s = this->device->lsPhysics.size();
        for (std::vector<PHYSICS*>::size_type i = 0; i < s; ++i)
        {
            PHYSICS *ptr = this->device->lsPhysics[i];
            if (ptr && ptr->enablePhysics && ptr->idScene == idCurrentScene)
            {
                ptr->update(fps,this->device->delta);
            }
        }
    }
    
    void CORE_MANAGER::initEnableRenders()
    {
        for (auto ptr : this->device->lsObjectRender3D)
        {
            if (ptr != nullptr)
            {
                ptr->enableRender = false;
            }
        }
        for (auto ptr : this->device->lsObjectRender2DS)
        {
            if (ptr != nullptr)
            {
                ptr->enableRender = false;
            }
        }
        for (auto ptr : this->device->lsObjectRender2DW)
        {
            if (ptr != nullptr)
            {
                ptr->enableRender = false;
            }
        }
    }
    
    void CORE_MANAGER::logic()
    {
        if (this->device->scene != nullptr)
        {
            if (this->device->scene->endScene)
            {
                this->device->scene->onFinalizeScene();
                this->device->scene->wasUnloadedScene = true;
                disableRender(this->device->scene->getIdScene());
                for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
                {
                    PLUGIN * plugin = this->lsPlugins[i];
                    plugin->onDestroy();
                }
                this->lsPlugins.clear();
                if (this->device->scene->goToNextScene && this->device->scene->nextScene == nullptr)
                {
                    this->device->run             = false;
                    this->device->clearBackGround = false;
                }
                else
                {
                    if (this->device->scene->goToNextScene)
                        this->device->scene       = this->device->scene->nextScene;
					if(this->device->scene)
						this->device->scene->endScene = false;
                    changeScene                   = true;
                    this->device->clearBackGround = true;
					if(this->device->scene)
						this->device->scene->startLoading();
                }
                this->__sceneWasInit = false;
            }
            else if (changeScene)
            {
                if (this->device->__swapBackBufferStep == 3)
                {
                    this->reinitTimers();
                    enableRender(this->device->scene->getIdScene());
                    this->device->scene->wasUnloadedScene = false;
                    this->device->orderRender.reInit();
                    this->device->scene->init();
                    this->device->setFakeFps(120,60);
                    this->device->resumeTimer();
                    this->__sceneWasInit          = true;
                    changeScene                   = false;
                    this->device->clearBackGround = true;
					if(this->device->scene)
						this->device->scene->endLoading();
                }
                else
                {
                    this->device->clearBackGround = false;
                    this->device->__swapBackBufferStep++;
                }
            }
            else
            {
                this->device->scene->logic();
            }
        }
    }
    
    void CORE_MANAGER::reinitTimers()
    {
        this->device->clearAdditionalTimers();
        this->device->resumeTimer();
    }
    
    void CORE_MANAGER::enableRender(const int idScene)
    {
        for (auto ptr : this->device->lsObjectRender3D)
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = true;
            }
        }
        for (auto ptr : this->device->lsObjectRender2DS)
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = true;
            }
        }
        for (auto ptr : this->device->lsObjectRender2DW)
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = true;
            }
        }
    }
    
    void CORE_MANAGER::disableRender(const int idScene)
    {
        for (auto ptr : this->device->lsObjectRender3D)
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = false;
            }
        }
        for (auto ptr : this->device->lsObjectRender2DS)
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = false;
            }
        }
        for (auto ptr : this->device->lsObjectRender2DW)
        {
            if (ptr != nullptr)
            {
                if (ptr->getIdScene() == idScene)
                    ptr->enableRender = false;
            }
        }
    }
    
    void CORE_MANAGER::pushEvent(EVENT_KEY *event)
    {
        if (this->device->scene && this->__sceneWasInit)
        {
#if defined _WIN32
            mutexEvents.lock();
#endif
            if (event->eventType == this->lastEvent.eventType)
            {
                switch (event->eventType)
                {
                    case UNKNOWN: return;
                    case ONRESIZEWINDOW:
                    case ONTOUCHDOWN:
                    case ONTOUCHUP:
                    case ONTOUCHMOVE:
                    {
                        if (event->key == this->lastEvent.key &&  //-V550
							event->x == this->lastEvent.x &&
                            event->y == this->lastEvent.y) //-V550
                        {
#if defined _WIN32
                            mutexEvents.unlock();
#endif
                            return;
                        }
                    }
                    break;
                    case ONDOUBLECLICK:
                    {
                        if (event->key == this->lastEvent.key &&  //-V550
							event->x == this->lastEvent.x &&
                            event->y == this->lastEvent.y) //-V550
                        {
#if defined _WIN32
                            mutexEvents.unlock();
#endif
                            return;
                        }
                    }
                    break;
                    case ONKEYDOWN:
                    case ONKEYUP:
                    {
                        if (event->key == this->lastEvent.key)
                        {
#if defined _WIN32
                            mutexEvents.unlock();
#endif
                            return;
                        }
                    }
                    break;
                    case ONTOUCHZOOM: {
                    }
                    break;
                    default: {
                    }
                    break;
                }
            }
            this->lastEvent = *event;

            switch (event->eventType)
            {
                case ONKEYDOWN:
                {
                    if (this->__keyPressed[event->key] == false)
                        this->lsEvents.push_back(*event);
                    this->__keyPressed[event->key] = true;
                }
                break;
                case ONKEYUP:
                {
                    if (this->__keyPressed[event->key])
                        this->lsEvents.push_back(*event);
                    this->__keyPressed[event->key] = false;
                }
                break;
                default: { this->lsEvents.push_back(*event);
                }
                break;
            }
#if defined _WIN32
            mutexEvents.unlock();
#endif
        }
    }
    
    bool CORE_MANAGER::popEvent(EVENT_KEY *event)
    {
#if defined _WIN32
        mutexEvents.lock();
#endif
        if (this->lsEvents.size() > 0 && event)
        {
            *event = this->lsEvents.front();
            this->lsEvents.pop_front();
#if defined _WIN32
            mutexEvents.unlock();
#endif
            return true;
        }
        else
        {
#if defined _WIN32
            mutexEvents.unlock();
#endif
            return false;
        }
    }
    
    void CORE_MANAGER::pushEvent(INFO_JOYSTICK_INIT_PLAYER *info)
    {
        if (this->device->scene && this->__sceneWasInit)
        {
#if defined _WIN32
            mutexEvents.lock();
#endif
            this->lsInfoJoystick.push_back(*info);
#if defined _WIN32
            mutexEvents.unlock();
#endif
        }
    }
    
    bool CORE_MANAGER::popEvent(INFO_JOYSTICK_INIT_PLAYER *info)
    {
#if defined _WIN32
        mutexEvents.lock();
#endif
        if (this->lsInfoJoystick.size() > 0 && info)
        {

            *info = this->lsInfoJoystick.front();
            this->lsInfoJoystick.pop_front();
#if defined _WIN32
            mutexEvents.unlock();
#endif
            return true;
        }
        else
        {
#if defined _WIN32
            mutexEvents.unlock();
#endif
            return false;
        }
    }
#if defined(ANDROID)
    
    void CORE_MANAGER::onTouchDown(int key, float x, float y)
    {
        x /= this->device->camera.scale2d.x;
        y /= this->device->camera.scale2d.y;
        if (this->device->scene && this->__sceneWasInit)
            this->device->scene->onTouchDown(key, x, y);
    }
    
    void CORE_MANAGER::onTouchUp(int key, float x, float y)
    {
        x /= this->device->camera.scale2d.x;
        y /= this->device->camera.scale2d.y;
        if (this->device->scene && this->__sceneWasInit)
            this->device->scene->onTouchUp(key, x, y);
    }
    
    void CORE_MANAGER::onTouchMove(int key, float x, float y)
    {
        x /= this->device->camera.scale2d.x;
        y /= this->device->camera.scale2d.y;
        if (this->device->scene && this->__sceneWasInit)
            this->device->scene->onTouchMove(key, x, y);
    }
    
    void CORE_MANAGER::onTouchZoom(float zoom) // Evento chamado ao solicitar zoom. Zoom estes normalmente com movimentos dos dedos. É
                                 // enviados valores entre -1 e +1. No caso de mouse é o scrool do mesmo.
    {
        if (this->device->scene && this->__sceneWasInit)
            this->device->scene->onTouchZoom(zoom);
    }
    
    void CORE_MANAGER::onKeyDown(int key) // Evento chamado ao pressionar uma tecla na janela ativa. key é um VK padrão da api do Windows.
    {
        if (this->device->scene && this->__sceneWasInit)
            this->device->scene->onKeyDown(key);
    }
    
    void CORE_MANAGER::onKeyUp(int key) // Evento chamado ao pressionar uma tecla na janela ativa. key é um VK padrão da api do Windows.
    {
        if (this->device->scene && this->__sceneWasInit)
            this->device->scene->onKeyUp(key);
    }
    
    void CORE_MANAGER::onDoubleClick(float x, float y, int key)
    {
        if (this->device->scene && this->__sceneWasInit)
            this->device->scene->onDoubleClick(x, y, key);
    }
    
    void CORE_MANAGER::onKeyDownJoystick(int player, int key)
    {
        if (this->device->scene && this->__sceneWasInit)
            this->device->scene->onKeyDownJoystick(player, key);
    }
    
    void CORE_MANAGER::onKeyUpJoystick(int player, int key)
    {
        if (this->device->scene && this->__sceneWasInit)
            this->device->scene->onKeyUpJoystick(player, key);
    }
    
    void CORE_MANAGER::onMoveJoystick(int player, float lx, float ly, float rx, float ry)
    {
        if (this->device->scene && this->__sceneWasInit)
            this->device->scene->onMoveJoystick(player, lx, ly, rx, ry);
    }
    
    void CORE_MANAGER::onInfoDeviceJoystick(int player, int maxNumberButton, const char *strDeviceName, const char *extraInfo)
    {
        if (this->device->scene && this->__sceneWasInit)
            this->device->scene->onInfoDeviceJoystick(player, maxNumberButton, strDeviceName, extraInfo);
    }

    void CORE_MANAGER::onResizeWindow(int, int)
    {
        if (this->device->scene && this->__sceneWasInit)
            this->device->scene->onResizeWindow();
    }
#elif defined __linux__

    void CORE_MANAGER::onTouchDown(int key, float x, float y)
    {
        x /= this->device->camera.scale2d.x;
        y /= this->device->camera.scale2d.y;
        EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONTOUCHDOWN);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onTouchUp(int key, float x, float y)
    {
        x /= this->device->camera.scale2d.x;
        y /= this->device->camera.scale2d.y;
        EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONTOUCHUP);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onTouchMove(int key, float x, float y)
    {
        x /= this->device->camera.scale2d.x;
        y /= this->device->camera.scale2d.y;
        EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONTOUCHMOVE);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onTouchZoom(float zoom) // Evento chamado ao solicitar zoom. Zoom estes normalmente com movimentos dos dedos. É
                                 // enviados valores entre -1 e +1. No caso de mouse é o scrool do mesmo.
    {
        EVENT_KEY ev(0, 0, (int)zoom, EVENT_TYPE_ACTIONS::ONTOUCHZOOM);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onKeyDown(int key) // Evento chamado ao pressionar uma tecla na janela ativa. key é um VK padrão da api do Windows.
    {
        EVENT_KEY ev(0, 0, key, EVENT_TYPE_ACTIONS::ONKEYDOWN);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onKeyUp(int key) // Evento chamado ao pressionar uma tecla na janela ativa. key é um VK padrão da api do Windows.
    {
        EVENT_KEY ev(0, 0, key, EVENT_TYPE_ACTIONS::ONKEYUP);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onDoubleClick(float x, float y, int key)
    {
        x /= this->device->camera.scale2d.x;
        y /= this->device->camera.scale2d.y;
        EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONDOUBLECLICK);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onKeyDownJoystick(int player, int key)
    {
        EVENT_KEY ev(0.0f, 0.0f, key, player, 0.0f, 0.0f, EVENT_TYPE_ACTIONS::ONKEYDOWNJOYSTICK);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onKeyUpJoystick(int player, int key)
    {
        EVENT_KEY ev(0.0f, 0.0f, key, player, 0.0f, 0.0f, EVENT_TYPE_ACTIONS::ONKEYUPJOYSTICK);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onMoveJoystick(int player, float lx, float ly, float rx, float ry)
    {
        static const float pProp_128 = 1.0f / 128.f;
        static const float pProp_127 = 1.0f / 127.f;
        const float        flx       = lx > 0 ? lx * pProp_127 : lx * pProp_128;
        const float        fly       = ly > 0 ? ly * pProp_127 : ly * pProp_128;
        const float        frx       = rx > 0 ? rx * pProp_127 : rx * pProp_128;
        const float        fry       = ry > 0 ? ry * pProp_127 : ry * pProp_128;
        EVENT_KEY          ev(flx, fly, 0, player, frx, fry, EVENT_TYPE_ACTIONS::ONMOVEJOYSTICK);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onInfoDeviceJoystick(int player, int maxNumberButton, const char *strDeviceName, const char *extraInfo)
    {
        INFO_JOYSTICK_INIT_PLAYER ev(player, maxNumberButton, strDeviceName, extraInfo);
        this->pushEvent(&ev);
    }

    void CORE_MANAGER::onResizeWindow(int width, int height)
    {
        EVENT_KEY ev(static_cast<float>(width),static_cast<float>(height),0,EVENT_TYPE_ACTIONS::ONRESIZEWINDOW);
        this->pushEvent(&ev);
    }
#elif _WIN32
    
    void CORE_MANAGER::onTouchDown(HWND, int key, float x, float y)
    {
        x /= this->device->camera.scale2d.x;
        y /= this->device->camera.scale2d.y;
        EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONTOUCHDOWN);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onTouchUp(HWND, int key, float x, float y)
    {
        x /= this->device->camera.scale2d.x;
        y /= this->device->camera.scale2d.y;
        EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONTOUCHUP);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onTouchMove(HWND, float x, float y)
    {
        x /= this->device->camera.scale2d.x;
        y /= this->device->camera.scale2d.y;
        EVENT_KEY ev(x, y, 0, EVENT_TYPE_ACTIONS::ONTOUCHMOVE);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onTouchZoom(HWND, float zoom) // Evento chamado ao solicitar zoom. Zoom estes normalmente com movimentos dos
                                       // dedos. É enviados valores entre -1 e +1. No caso de mouse é o scrool do mesmo.
    {
        EVENT_KEY ev(0, 0, (int)zoom, EVENT_TYPE_ACTIONS::ONTOUCHZOOM);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onKeyDown(HWND, int key) // Evento chamado ao pressionar uma tecla na janela ativa. key é um VK padrão da api do Windows.
    {
        EVENT_KEY ev(0, 0, key, EVENT_TYPE_ACTIONS::ONKEYDOWN);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onKeyUp(HWND,int key) // Evento chamado ao pressionar uma tecla na janela ativa. key é um VK padrão da api do Windows.
    {
        EVENT_KEY ev(0, 0, key, EVENT_TYPE_ACTIONS::ONKEYUP);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onDoubleClick(HWND, float x, float y, int key)
    {
        x /= this->device->camera.scale2d.x;
        y /= this->device->camera.scale2d.y;
        EVENT_KEY ev(x, y, key, EVENT_TYPE_ACTIONS::ONDOUBLECLICK);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onKeyDownJoystick(int player, int key)
    {
        EVENT_KEY ev(0.0f, 0.0f, key, player, 0.0f, 0.0f, EVENT_TYPE_ACTIONS::ONKEYDOWNJOYSTICK);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onKeyUpJoystick(int player, int key)
    {
        EVENT_KEY ev(0.0f, 0.0f, key, player, 0.0f, 0.0f, EVENT_TYPE_ACTIONS::ONKEYUPJOYSTICK);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onMoveJoystick(int player, float lx, float ly, float rx, float ry)
    {
        constexpr float pProp_128 = 1.0f / 128.f;
        constexpr float pProp_127 = 1.0f / 127.f;
        const float        flx       = lx > 0 ? lx * pProp_127 : lx * pProp_128;
        const float        fly       = ly > 0 ? ly * pProp_127 : ly * pProp_128;
        const float        frx       = rx > 0 ? rx * pProp_127 : rx * pProp_128;
        const float        fry       = ry > 0 ? ry * pProp_127 : ry * pProp_128;
        EVENT_KEY          ev(flx, fly, 0, player, frx, fry, EVENT_TYPE_ACTIONS::ONMOVEJOYSTICK);
        this->pushEvent(&ev);
    }
    
    void CORE_MANAGER::onInfoDeviceJoystick(int player, int maxNumberButton, const char *strDeviceName, const char *extraInfo)
    {
        INFO_JOYSTICK_INIT_PLAYER ev(player, maxNumberButton, strDeviceName, extraInfo);
        this->pushEvent(&ev);
    }

    void CORE_MANAGER::onResizeWindow(HWND, int width, int height)
    {
        EVENT_KEY ev(static_cast<float>(width),static_cast<float>(height),0,EVENT_TYPE_ACTIONS::ONRESIZEWINDOW);
        this->pushEvent(&ev);
    }
    #endif

    void CORE_MANAGER::forceRestore()
    {
        this->onStop();
        #if defined ANDROID
        while (!this->onLostDevice(this->device->jni->jenv,nullptr,static_cast<int>(this->device->backBufferWidth),static_cast<int>(this->device->backBufferHeight)));
        #else
        while (!this->onLostDevice(static_cast<int>(this->device->backBufferWidth),static_cast<int>(this->device->backBufferHeight),0,0));
        #endif
    }

    unsigned int CORE_MANAGER::addPlugin(PLUGIN * plugin)
    {
        for(unsigned int i=0; i < this->lsPlugins.size(); ++i)
        {
            const PLUGIN * thatPlugin = this->lsPlugins[i];
            if(plugin == thatPlugin)
            {
                return i;
            }
        }
        if(plugin != nullptr)
        {
            this->lsPlugins.push_back(plugin);
            void * handle = nullptr;
            #if defined _WIN32
                handle = this->device->window.getHwnd();
            #elif defined(__linux__) && !defined (ANDROID)
                handle = this->display;
            #elif defined(ANDROID)
                handle = this->device->jni->jenv;
            #endif
            plugin->onSubscribe(static_cast<int>(this->device->backBufferWidth),static_cast<int>(this->device->backBufferHeight),handle);
            return this->lsPlugins.size() - 1;
        }
        return 0xffffffff;
    }

    #if defined USE_EDITOR_FEATURES && !defined ANDROID
    void CORE_MANAGER::execute_system_cmd_thread(const char* command)//execute system command in other thread
    {
        auto fNextThreadName = []() -> std::string
        {
            static int iNumThread = 0;
            std::string name("__thread_");
            name += std::to_string(++iNumThread);
            return name;
        };
        auto fExecute = [] (std::string command) -> void
        {
            system(command.c_str());
        };
        static std::string sCommand;
        sCommand                         = command;
        mbm::DEVICE* device              = mbm::DEVICE::getInstance();
        std::string name                 = fNextThreadName();
        std::thread* exec_thread         = new std::thread(fExecute, std::ref(sCommand));
        DYNAMIC_VAR* dyVar               = new DYNAMIC_VAR(DYNAMIC_REF,static_cast<const void*>(exec_thread));
        device->lsDynamicVarGlobal[name] = dyVar;
    }
    #endif
}

#if defined(_WIN32)

namespace util
{
    void getDisplayMetrics(int * width, int * height)
    {
        mbm::MONITOR_MANAGER manMonit;
        mbm::MONITOR    monitor;
        manMonit.updateMonitors();
        for (DWORD iMon = 0; iMon < manMonit.getTotalMonitor(); ++iMon)
        {
            if (manMonit.isMainMonitor(iMon) && manMonit.getMonitor(iMon, &monitor))
            {
                *width = monitor.width;
                *height = monitor.height;
                break;
            }
        }
    }
}
#endif
