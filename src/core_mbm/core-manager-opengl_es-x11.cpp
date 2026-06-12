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
#include "specific-opengl_es-x11-context.h"
#include <miniz-wrap/miniz-wrap.h>
#include <audio-interface.h>
#include <util-interface.h>
#include <cassert>
#include <thread>
#include <unistd.h>
#include <fcntl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/XKBlib.h>

#ifdef __APPLE__
//#include <X11/extensions/Xcomposite.h>
//#include <X11/Xmu/WinUtil.h>
#endif

namespace mbm { static bool initializeWindowx11(SPECIFIC_AUX_CONTEXT_DEVICE* ctx); }

namespace mbm
{
    bool CORE_MANAGER::initGraphics(const char *nameApplication, int width, int height, const int px, const int py, const bool border,const bool enable_resize)
    {
        int x = width;
        int y = height;
        this->setNameApplication(nameApplication);
        this->setWindowOptions(border, enable_resize);
        //char * dpyName = nullptr;
        EGLint egl_major = 0;
        EGLint egl_minor = 0;
          // Initialize window position
        device->setWindowPosition(px, py);
        if(initializeWindowx11(this->device->getSpecificContextDevice()) == false)
        {
            INFO_LOG("Failed to initialize X11 window");
            return false;
        }
    #ifdef __APPLE__
        this->device->getSpecificContextDevice()->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        #pragma message("Check if this is correct for MacOS")
    #else
        this->device->getSpecificContextDevice()->eglDisplay = eglGetDisplay((EGLNativeDisplayType) this->device->getSpecificContextDevice()->display_x11);
    #endif
        if (!this->device->getSpecificContextDevice()->eglDisplay)
        {
            printf("Error: eglGetDisplay() failed\n");
            return false;
        }

        if (!eglInitialize(this->device->getSpecificContextDevice()->eglDisplay, &egl_major, &egl_minor))
        {
            printf("Error: eglInitialize() failed\n");
            return false;
        }
        Screen *screen = DefaultScreenOfDisplay(this->device->getSpecificContextDevice()->display_x11);
        if (border && (height + 60) >= screen->height)
        {
            height -= 60;
            y = height;
        }
        
        // Check if we're reusing an existing window (lost device recovery)
        const bool reusingWindow = (this->device->getSpecificContextDevice()->window_x11 != 0);
        
        this->device->getSpecificContextDevice()->make_x_window(nameApplication, px, py, static_cast<uint32_t>(width), static_cast<uint32_t>(height), this->getWindowBorder(), this->getEnableResizeWindow());

      
        
        // Wait for MapNotify event only for newly created windows
        // Existing windows are already mapped, so waiting would block forever
        if (!reusingWindow)
        {
            XEvent event;
            XIfEvent(this->device->getSpecificContextDevice()->display_x11, &event,
                    [](Display*, XEvent* ev, XPointer) -> Bool {
                        return ev->type == MapNotify;
                    }, nullptr);
        }
        
        // Sync with X server to ensure all events are processed
        XSync(this->device->getSpecificContextDevice()->display_x11, False);
        
        // Get actual window geometry after mapping
        XWindowAttributes window_attrs;
        XGetWindowAttributes(this->device->getSpecificContextDevice()->display_x11, 
                           this->device->getSpecificContextDevice()->window_x11, 
                           &window_attrs);
        
        // Update dimensions with actual window size
        x = window_attrs.width;
        y = window_attrs.height;
        device->setBackBufferSize(static_cast<float>(window_attrs.width),
                                  static_cast<float>(window_attrs.height));
        
        // Now process all pending events including ConfigureNotify
        this->handleEventFromWindow();

        if (!eglMakeCurrent(this->device->getSpecificContextDevice()->eglDisplay, this->device->getSpecificContextDevice()->eglSurface, this->device->getSpecificContextDevice()->eglSurface, this->device->getSpecificContextDevice()->eglContext))
        {
            printf("Error: eglMakeCurrent() failed\n");
            return false;
        }

        if (device->isVerbose())
        {
            printGLString("\nversion:\n", GL_VERSION);
            printGLString("vendor:\n", GL_VENDOR);
            printGLString("renderer:\n", GL_RENDERER);
            //printGLStringNewLine("Extensions:\n", GL_EXTENSIONS, ' ');
            //printEGLStringNewLine(this->device->getSpecificContextDevice()->display,' ');
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

        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &device->getSpecificContextDevice()->filter_GL_TEXTURE_WRAP_S);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &device->getSpecificContextDevice()->filter_GL_TEXTURE_WRAP_T);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &device->getSpecificContextDevice()->filter_GL_TEXTURE_MIN_FILTER);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &device->getSpecificContextDevice()->filter_GL_TEXTURE_MAG_FILTER);

        return true;
    }

    // Internal helper: returns the X11 display socket fd, or -1.
    // Defined here (not in the header) because it is X11-specific.
    int getX11DisplayFd() noexcept
    {
        DEVICE* dev = DEVICE::getInstance();
        if (dev && dev->getSpecificContextDevice() && dev->getSpecificContextDevice()->display_x11)
            return ConnectionNumber(dev->getSpecificContextDevice()->display_x11);
        return -1;
    }

    // Xlib error handler: BadWindow / BadDrawable are transient and expected when windows
    // are destroyed concurrently (e.g. after the child game process closes). Log them but
    // do NOT call exit() so the editor can survive.
    static int x11ErrorHandler(Display *dpy, XErrorEvent *e)
    {
        char msg[256] = {};
        XGetErrorText(dpy, e->error_code, msg, sizeof(msg));
        // BadWindow (3) and BadDrawable (9) are harmless transient errors
        if (e->error_code == BadWindow || e->error_code == BadDrawable)
        {
            INFO_LOG("X11 non-fatal: %s (code %d) on resource 0x%lx serial %lu",
                     msg, static_cast<int>(e->error_code),
                     static_cast<unsigned long>(e->resourceid),
                     static_cast<unsigned long>(e->serial));
            return 0;
        }
        ERROR_LOG("X11 error: %s (code %d) on resource 0x%lx serial %lu",
                  msg, static_cast<int>(e->error_code),
                  static_cast<unsigned long>(e->resourceid),
                  static_cast<unsigned long>(e->serial));
        return 0; // still don't abort; let the engine handle it
    }

    static bool initializeWindowx11(SPECIFIC_AUX_CONTEXT_DEVICE* ctx)
    {
        if(ctx->display_x11 == nullptr)
        {
            // Enable Xlib multi-threading before opening the display.
            // Required because execute_system_cmd_thread uses a background thread
            // that calls posix_spawnp while the main thread makes X11 calls.
            XInitThreads();

            // Install a custom error handler that logs BadWindow/BadDrawable without
            // aborting. These are transient and normal for long-running X11 apps.
            XSetErrorHandler(x11ErrorHandler);

            char * dpyName = nullptr;
            ctx->display_x11 = XOpenDisplay(dpyName);
            if(ctx->display_x11 == nullptr)
            {
                ERROR_LOG("Error: couldn't open display %s\n", dpyName ? dpyName : getenv("DISPLAY"));
                return false;
            }
            // Mark the X11 socket close-on-exec so child processes created via posix_spawn
            // never inherit it even during the brief fork\u2192exec window.
            fcntl(ConnectionNumber(ctx->display_x11), F_SETFD, FD_CLOEXEC);
            XFlush(ctx->display_x11);
        }
        else
        {
            //already opened display, probably lost device case
            INFO_LOG("display_x11 already opened, probably lost device case");
        }
        return true;
    }

    void CORE_MANAGER::handleEventFromWindow()
    {
        while (XPending(this->device->getSpecificContextDevice()->display_x11))
        {
            XEvent xevent;
            XNextEvent(this->device->getSpecificContextDevice()->display_x11, &xevent);
            switch (xevent.type)
            {
                case KeyPress:
                {
                    // Use XkbKeycodeToKeysym with effective group to respect NumLock
                    // For numpad keys, if NumLock is on (Mod2Mask), use group 1 to get numbers
                    unsigned int state = xevent.xkey.state;
                    bool numLockOn = (state & Mod2Mask) != 0;
                    KeySym keysym;
                    
                    // First get the base keysym to check if it's a keypad key
                    keysym = XLookupKeysym(&xevent.xkey, 0);
                    
                    // For keypad keys, NumLock inverts the keysym group
                    // Keypad keysyms are in range 0xFF80-0xFFBD
                    bool isKeypad = (keysym >= 0xFF80 && keysym <= 0xFFBD);
                    if (isKeypad && numLockOn)
                    {
                        // Use index 1 to get the number/decimal keysyms
                        keysym = XLookupKeysym(&xevent.xkey, 1);
                    }
                    
                    auto key = static_cast<int>(keysym);
                    if (key >= 'a' && key <= 'z')
                        key = toupper(key);
                    if (key == XK_Caps_Lock)
                        this->keyCapsLockState = ((xevent.xbutton.state & 2) == 0);// == 0 is on
                    this->onKeyDown(key);
                }
                break;
                case KeyRelease:
                {
                    // Same NumLock handling for key release
                    unsigned int state = xevent.xkey.state;
                    bool numLockOn = (state & Mod2Mask) != 0;
                    KeySym keysym;
                    
                    keysym = XLookupKeysym(&xevent.xkey, 0);
                    
                    bool isKeypad = (keysym >= 0xFF80 && keysym <= 0xFFBD);
                    if (isKeypad && numLockOn)
                    {
                        keysym = XLookupKeysym(&xevent.xkey, 1);
                    }
                    
                    auto key = static_cast<int>(keysym);
                    if (key >= 'a' && key <= 'z')
                        key = toupper(key);
                    this->onKeyUp(key);
                }
                break;
                case ButtonPress:
                {
                    switch (xevent.xbutton.button)
                    {
                    case Button1:
                    { 
                        this->onTouchDown(0, xevent.xbutton.x, xevent.xbutton.y); 
                    }break;
                    case Button2:
                    { 
                        this->onTouchDown(2, xevent.xbutton.x, xevent.xbutton.y); 
                    }break;
                    case Button3:
                    { 
                        this->onTouchDown(1, xevent.xbutton.x, xevent.xbutton.y); 
                    }break;
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
                    case Button1: { this->onTouchUp(0, xevent.xbutton.x, xevent.xbutton.y); } break;
                    case Button2: { this->onTouchUp(2, xevent.xbutton.x, xevent.xbutton.y); } break;
                    case Button3: { this->onTouchUp(1, xevent.xbutton.x, xevent.xbutton.y); } break;
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
                case ConfigureNotify:
                {
                    // Handle window move/resize/restack events
                    XConfigureEvent xconfig = xevent.xconfigure;
                    
                    // Get absolute window position (ConfigureNotify gives parent-relative coords)
                    Window child_return;
                    int abs_x = 0;
                    int abs_y = 0;
                    XTranslateCoordinates(this->device->getSpecificContextDevice()->display_x11,
                                         this->device->getSpecificContextDevice()->window_x11,
                                         DefaultRootWindow(this->device->getSpecificContextDevice()->display_x11),
                                         0, 0, &abs_x, &abs_y, &child_return);
                    
                    // Store absolute window position for coordinate transformations
                    bool needMoveEvent = false;
                    if(abs_x != device->getWindowPositionX() || abs_y != device->getWindowPositionY())
                    {
                        needMoveEvent = true;
                    }
                    device->setWindowPosition(abs_x, abs_y);
                    
                    // Update viewport if size changed
                    if (xconfig.width != static_cast<int>(device->getBackBufferWidth()) ||
                        xconfig.height != static_cast<int>(device->getBackBufferHeight()))
                    {
                        this->onResizeWindow(xconfig.width, xconfig.height);
                        if(needMoveEvent)
                        {
                            this->onMoveWindow(abs_x, abs_y);
                        }
                    }
                }
                break;
                case Expose:
                {
                    // Handle expose events (window redraw requests)
                    //commented out code below
                    /*XExposeEvent xexpose = xevent.xexpose;

                    // Get absolute window position (ConfigureNotify gives parent-relative coords)
                    Window child_return;
                    int abs_x = 0;
                    int abs_y = 0;
                    XTranslateCoordinates(this->device->getSpecificContextDevice()->display_x11,
                                         this->device->getSpecificContextDevice()->window_x11,
                                         DefaultRootWindow(this->device->getSpecificContextDevice()->display_x11),
                                         0, 0, &abs_x, &abs_y, &child_return);
//
                    device->setWindowPosition(abs_x, abs_y);
                    if((device->getBackBufferWidth() != static_cast<float>(xexpose.width) ||
                       device->getBackBufferHeight() != static_cast<float>(xexpose.height)) &&
                       xexpose.width > 0 && xexpose.height > 0)
                    {
                        //this->onResizeWindow(xexpose.width, xexpose.height);

                        // On X11/EGL, the EGL surface doesn't automatically resize with the window
                        // We need to recreate the surface to match the new window dimensions
                        if (!this->device->getSpecificContextDevice()->recreateEGLSurface())
                        {
                            break;  // Trigger full restore if surface recreation fails
                        }
                        
                        // Query the actual EGL surface dimensions after recreation
                        EGLint surfaceWidth = 0;
                        EGLint surfaceHeight = 0;
                        eglQuerySurface(this->device->getSpecificContextDevice()->eglDisplay, 
                                        this->device->getSpecificContextDevice()->eglSurface, 
                                        EGL_WIDTH, &surfaceWidth);
                        eglQuerySurface(this->device->getSpecificContextDevice()->eglDisplay, 
                                        this->device->getSpecificContextDevice()->eglSurface, 
                                        EGL_HEIGHT, &surfaceHeight);
                        
                        // Use actual surface dimensions
                        if (surfaceWidth > 0 && surfaceHeight > 0 && (device->getBackBufferWidth() != static_cast<float>(surfaceWidth) ||
                            device->getBackBufferHeight() != static_cast<float>(surfaceHeight)))
                        {
                            //this->device->setBackBufferSize(static_cast<float>(surfaceWidth), static_cast<float>(surfaceHeight));
                            this->onResizeWindow(surfaceWidth, surfaceHeight);
                        }
                    }*/
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
        Screen * screen = DefaultScreenOfDisplay(this->device->getSpecificContextDevice()->display_x11);
        if(screen)
        {
            *width  = screen->width;
            *height = screen->height;
        }
    }

    void CORE_MANAGER::moveWindow(int x, int y)
    {
        if(this->device->getSpecificContextDevice()->display_x11 != nullptr && 
           this->device->getSpecificContextDevice()->window_x11 != 0)
        {
            XMoveWindow(this->device->getSpecificContextDevice()->display_x11, 
                       this->device->getSpecificContextDevice()->window_x11, 
                       x, y);
            XFlush(this->device->getSpecificContextDevice()->display_x11);
        }
    }

    //void CORE_MANAGER::getWindowPosition(int *x, int *y)
    //{
    //    if(this->device->getSpecificContextDevice()->display_x11 != nullptr && 
    //       this->device->getSpecificContextDevice()->window_x11 != 0)
    //    {
    //        Window root, child;
    //        int root_x, root_y, win_x, win_y;
    //        unsigned int mask;
    //        
    //        XQueryPointer(this->device->getSpecificContextDevice()->display_x11,
    //                     this->device->getSpecificContextDevice()->window_x11,
    //                     &root, &child, &root_x, &root_y, &win_x, &win_y, &mask);
    //        
    //        XWindowAttributes attrs;
    //        XGetWindowAttributes(this->device->getSpecificContextDevice()->display_x11,
    //                           this->device->getSpecificContextDevice()->window_x11,
    //                           &attrs);
    //        
    //        *x = attrs.x;
    //        *y = attrs.y;
    //    }
    //}
   
    void SPECIFIC_AUX_CONTEXT_DEVICE::make_x_window(const char *name,const int px, const int py,const uint32_t width,const uint32_t height, const bool border, const bool enable_resize)
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

        // Only create a new X11 window if one doesn't already exist (e.g., on lost device recovery)
        const bool reuseExistingWindow = (window_x11 != 0);
        
        if (!reuseExistingWindow)
        {
            /* window attributes */
            attr.background_pixel = 0;
            attr.border_pixel     = 0;
            attr.colormap         = XCreateColormap(display_x11, root, visInfo->visual, AllocNone);
            attr.event_mask       = StructureNotifyMask | ExposureMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask;
            mask                  = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
            if(border == false)
            {
                attr.override_redirect= 1;
                mask                  = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask | CWOverrideRedirect;
            }
            

            window_x11 = static_cast<Window>(XCreateWindow(display_x11, root, px, py, width, height, 0, visInfo->depth, InputOutput,
                                visInfo->visual, mask, &attr));

            /* set hints and properties */
            {
                XSizeHints sizehints;
                sizehints.x      = px;
                sizehints.y      = py;
                sizehints.width  = static_cast<EGLint>(width);
                sizehints.height = static_cast<EGLint>(height);
                sizehints.flags  = USSize | USPosition;
                XSetNormalHints(display_x11, window_x11, &sizehints);
                XSetStandardProperties(display_x11, window_x11, name, name, None, nullptr, 0, &sizehints);
            }

            XSelectInput(this->display_x11, this->window_x11,
            (KeyPressMask | KeyReleaseMask) | (ButtonPressMask | ButtonReleaseMask) | (PointerMotionMask) | ExposureMask | StructureNotifyMask);
            XkbSetDetectableAutoRepeat(this->display_x11, true, nullptr);
            XMapWindow(this->display_x11, this->window_x11);
            XFlush(this->display_x11);


            XSizeHints xsize;
            
            if(enable_resize)
            {
                xsize.flags = PMinSize | USPosition; // only what we wish (for now not PMaxSize)
            }
            else
            {
                xsize.flags = PMaxSize | PMinSize | USPosition; // only what we wish (for now not PMaxSize)
            }
            xsize.min_width   = static_cast<int>(100);
            xsize.min_height  = static_cast<int>(100);
            xsize.max_width   = static_cast<int>(width);
            xsize.max_height  = static_cast<int>(height);
            xsize.base_width  = static_cast<int>(width);
            xsize.base_height = static_cast<int>(height);
            xsize.width       = static_cast<int>(width);
            xsize.height      = static_cast<int>(height);
            xsize.width_inc   = 0;
            xsize.height_inc  = 0;
            xsize.x           = px;
            xsize.y           = py;

            if(enable_resize == false)
            {
                xsize.min_width  = static_cast<int>(width);
                xsize.min_height = static_cast<int>(height);
                xsize.max_width  = static_cast<int>(width);
                xsize.max_height = static_cast<int>(height);
            }
            XSetWMNormalHints(this->display_x11, this->window_x11, &xsize);
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
        
        // Store config for later surface recreation on resize
        this->eglConfig = config;
        
        const EGLint *attrib_list = nullptr;
        this->eglSurface = eglCreateWindowSurface(eglDisplay, config, reinterpret_cast<EGLNativeWindowType>(window_x11), attrib_list);
        if (!this->eglSurface)
        {
            printf("Error: eglCreateWindowSurface failed\n");
            exit(1);
        }

        /* sanity checks - only check dimensions for newly created windows */
        {
            EGLint val;
            if (!reuseExistingWindow)
            {
                eglQuerySurface(eglDisplay, this->eglSurface, EGL_WIDTH, &val);
                const EGLint wEgl = val;
                eglQuerySurface(eglDisplay, this->eglSurface, EGL_HEIGHT, &val);
                const EGLint hEgl = val;
                if(wEgl != static_cast<EGLint>(width) || hEgl != static_cast<EGLint>(height))
                {
                    INFO_LOG("Warning: EGL surface size (%d x %d) differs from requested window size (%u x %u)", wEgl, hEgl, width, height);
                }
            }
            assert(eglGetConfigAttrib(eglDisplay, config, EGL_SURFACE_TYPE, &val));
            assert(val & EGL_WINDOW_BIT);
        }

        XFree(visInfo);
    }

    bool SPECIFIC_AUX_CONTEXT_DEVICE::recreateEGLSurface()
    {
        if (this->eglDisplay == EGL_NO_DISPLAY || this->window_x11 == 0 || this->eglConfig == nullptr)
        {
            return false;
        }
        
        // Destroy old surface
        if (this->eglSurface != EGL_NO_SURFACE)
        {
            eglMakeCurrent(this->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            eglDestroySurface(this->eglDisplay, this->eglSurface);
            this->eglSurface = EGL_NO_SURFACE;
        }
        
        // Create new surface with current window dimensions
        const EGLint *attrib_list = nullptr;
        this->eglSurface = eglCreateWindowSurface(this->eglDisplay, this->eglConfig, 
                                                   reinterpret_cast<EGLNativeWindowType>(this->window_x11), 
                                                   attrib_list);
        if (!this->eglSurface)
        {
            printf("Error: recreateEGLSurface - eglCreateWindowSurface failed\n");
            return false;
        }
        
        // Re-bind the context to the new surface
        if (!eglMakeCurrent(this->eglDisplay, this->eglSurface, this->eglSurface, this->eglContext))
        {
            printf("Error: recreateEGLSurface - eglMakeCurrent failed\n");
            return false;
        }
        
        return true;
    }

    void CORE_MANAGER::ReleaseGraphics(bool wasDeviceLost)
    {
        TEXTURE_MANAGER::getInstance()->release();
        MESH_MANAGER::getInstance()->release();
        this->device->getSpecificContextDevice()->release(wasDeviceLost);
    }

}

#endif // USE_OPENGL_ES
#endif //!ANDROID
#endif //(__linux__) || defined(__APPLE__)
