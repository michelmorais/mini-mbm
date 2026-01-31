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
#include <scene.h>
#include <renderizable.h>
#include <texture-manager.h>
#include <specific-opengl_es.h>
#include <util-interface.h>
#include <audio-interface.h>
#include <version/version.h>
#include <miniz-wrap/miniz-wrap.h>
#include <cr-static-local.h>
#include <mesh-manager.h>
#include <plugin-callback.h>


namespace mbm
{
    INFO_JOYSTICK_INIT_PLAYER::INFO_JOYSTICK_INIT_PLAYER() : player(0), maxNumberButton(0)
    {}

    INFO_JOYSTICK_INIT_PLAYER::INFO_JOYSTICK_INIT_PLAYER(const int _player, const int _maxNumberButton, const char *_deviceName,
                                const char *_extraInfo)
        : player(_player), maxNumberButton(_maxNumberButton), deviceName(_deviceName), extraInfo(_extraInfo)
    {}
    
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
    #if (defined (__MINGW32__) || defined (__CYGWIN__) || defined(_WIN32))
        this->device->specificContextDevice->initializeWi32Callbacks(this);
    #endif
    }
    
    CORE_MANAGER::~CORE_MANAGER()
    {
        DEVICE::quit();
    }

    void CORE_MANAGER::swapBuffers()
    {
        #if !defined (ANDROID)
        eglSwapBuffers(this->device->specificContextDevice->eglDisplay, this->device->specificContextDevice->eglSurface);
        #endif
    }

    void CORE_MANAGER::ReleaseGraphics()
    {
        TEXTURE_MANAGER::getInstance()->release();
        MESH_MANAGER::getInstance()->release();
        this->device->specificContextDevice->window.setCallEventsManager(nullptr);
        this->device->specificContextDevice->win32_joystickByPass->releaseJoystick(&this->device->specificContextDevice->window);
        this->device->specificContextDevice->release();
    }
    

    bool CORE_MANAGER::initGraphics(const char *nameAplication, int width, int height, const int px, const int py, const bool border,const bool enable_resize)
    {
        int x = width;
        int y = height;
        this->nameAplication = nameAplication ? nameAplication : "Mini-mbm";
#if (defined (__MINGW32__) || defined (__CYGWIN__) || defined(_WIN32))
        DEVICE* device = DEVICE::getInstance();
        device->specificContextDevice->window.setNameAplication(nameAplication);
        if (!device->specificContextDevice->window.init(nameAplication, x, y, px, py, enable_resize, enable_resize, enable_resize, false, nullptr, border == false,
            this->device->specificContextDevice->idIcon,false))
        {
            device->specificContextDevice->window.messageBox("error on init app ... will be closed ");
            PRINT_IF_DEBUG( "error on init app ... will be closed %s", "error on create window");
            return false;
        }
        device->specificContextDevice->window.setMinSizeAllowed(800,600);
        device->specificContextDevice->window.askOnExit = false;
        device->specificContextDevice->window.exitOnEsc = false;
        HWND mNativeWindow = device->specificContextDevice->window.getHwnd();
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
        if(this->device->specificContextDevice->win32_EventByPass)
            device->specificContextDevice->window.setCallEventsManager(this->device->specificContextDevice->win32_EventByPass);
        if(this->device->specificContextDevice->win32_joystickByPass)
            this->device->specificContextDevice->win32_joystickByPass->initJoystick(&device->specificContextDevice->window);
        HDC hdc = GetDC(device->specificContextDevice->window.getHwnd());
        // Create EGL display connection
        this->device->specificContextDevice->eglDisplay = eglGetDisplay(hdc);
        // Initialize EGL for this display, returns EGL version
        EGLint eglVersionMajor = 0;
        EGLint eglVersionMinor = 0;
        if(eglInitialize(this->device->specificContextDevice->eglDisplay, &eglVersionMajor, &eglVersionMinor) == EGL_FALSE)
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

        /*if ( EGL_FALSE == eglGetConfigs(this->device->specificContextDevice->eglDisplay, NULL, 0, &numConfigs) )
        {
            ERROR_LOG("Could not get number of all configs");
            return false;
        }

        // collect information about the configs
        EGLConfig *configs = new EGLConfig[numConfigs];
        if ( EGL_FALSE == eglGetConfigs(this->device->specificContextDevice->eglDisplay,configs,numConfigs,&numConfigs) )
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
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_RED_SIZE, &(newFormat._red_size));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_BLUE_SIZE, &(newFormat._blue_size));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_GREEN_SIZE, &(newFormat._green_size));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_ALPHA_SIZE, &(newFormat._alpha_size));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_BIND_TO_TEXTURE_RGB, &(newFormat._bind_to_texture_rgb));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_BIND_TO_TEXTURE_RGBA, &(newFormat._bind_to_texture_rgba));
            
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_BUFFER_SIZE, &(newFormat._buffer_size));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_CONFIG_CAVEAT, &(newFormat._config_caveat));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_CONFIG_ID, &(newFormat._config_id));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_DEPTH_SIZE, &(newFormat._depth_size));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_GREEN_SIZE, &(newFormat._green_size));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_LEVEL, &(newFormat._level));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_MAX_PBUFFER_WIDTH, &(newFormat._max_pbuffer_width));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_MAX_PBUFFER_HEIGHT, &(newFormat._max_pbuffer_height));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_MAX_PBUFFER_PIXELS, &(newFormat._max_pbuffer_pixels));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_MAX_SWAP_INTERVAL, &(newFormat._max_swap_interval));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_MIN_SWAP_INTERVAL, &(newFormat._min_swap_interval));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_NATIVE_RENDERABLE, &(newFormat._native_renderable));
            eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_NATIVE_VISUAL_ID, &(newFormat._native_vrenderable));
            /// etc etc etc for all those that you care about
 
            if ( eglVersionMajor >= 1 && eglVersionMinor >= 2 )
            {       
                // 1.2
                eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_ALPHA_MASK_SIZE, &(newFormat._alpha_mask_size));
                eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_COLOR_BUFFER_TYPE, &(newFormat._color_buffer_type));
                eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_LUMINANCE_SIZE, &(newFormat._luminance_size));
                eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_RENDERABLE_TYPE, &(newFormat._renderable_type));
            }
 
            if ( eglVersionMajor >= 1 && eglVersionMinor >= 3 )
            {
                // 1.3
                //const char * ext = eglQueryString(this->device->specificContextDevice->eglDisplay,EGL_EXTENSIONS);
                eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_CONFORMANT, &(newFormat._conformant));
                eglGetConfigAttrib( this->device->specificContextDevice->eglDisplay, config, EGL_CONTEXT_OPENGL_ROBUST_ACCESS, &(newFormat._egl_robust));
                
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
        EGLBoolean result = eglChooseConfig(this->device->specificContextDevice->eglDisplay, the_attribs, &windowConfig, 1, &numConfigs);
        */
        EGLBoolean result = eglChooseConfig(this->device->specificContextDevice->eglDisplay, attribs, &windowConfig, 1, &numConfigs);
        
        //EGLBoolean result = eglChooseConfig(this->device->specificContextDevice->eglDisplay, attribs, &windowConfig, 1, &numConfigs);
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
        this->device->specificContextDevice->eglSurface = eglCreateWindowSurface(this->device->specificContextDevice->eglDisplay, windowConfig, device->specificContextDevice->window.getHwnd(), surfaceAttributes);
        //this->device->specificContextDevice->eglSurface = eglCreateWindowSurface(this->device->specificContextDevice->eglDisplay, windowConfig, device->window.getHwnd(), the_attribs);
        if(this->device->specificContextDevice->eglSurface == nullptr)
        {
            ERROR_LOG(" Could not create EGL Window surface");
            return false;
        }

        //EGLint contextAttributes[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
        //this->device->specificContextDevice->eglContext = eglCreateContext(this->device->specificContextDevice->eglDisplay, windowConfig, NULL, contextAttributes);
        EGLint es3ContextAttribs[] = {EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE, EGL_NONE};
        this->device->specificContextDevice->eglContext = eglCreateContext(this->device->specificContextDevice->eglDisplay, windowConfig, NULL, es3ContextAttribs);
        if(this->device->specificContextDevice->eglContext == nullptr)
        {
            ERROR_LOG(" Could not create EGL context");
            return false;
        }
        result = eglMakeCurrent(this->device->specificContextDevice->eglDisplay, this->device->specificContextDevice->eglSurface, this->device->specificContextDevice->eglSurface, this->device->specificContextDevice->eglContext);
        if(result != EGL_TRUE)
        {
            ERROR_LOG(" Could not make EGL context current");
            return false;
        }

        device->specificContextDevice->window.disableRender(mNativeWindow);
        if (device->verbose)
        {
            printGLString("\nversion:\n", GL_VERSION);
            printGLString("vendor:\n", GL_VENDOR);
            printGLString("renderer:\n", GL_RENDERER);
            //printGLStringNewLine("GL Extensions:\n", GL_EXTENSIONS, ' ');
            //printEGLStringNewLine(this->device->specificContextDevice->eglDisplay, ' ');
            
            MINIZ::showVersion();
            INFO_LOG("\nAudio engine: %s\n", AUDIO_ENGINE_version());
        }
#elif (defined(__linux__) || defined(__APPLE__)) && !defined(ANDROID)

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

        TEXTURE_MANAGER* texture_manager = TEXTURE_MANAGER::getInstance();
        GLint maxTextureSize = 0;
        GLGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
        //const GLint MaxTextureWidth = static_cast<GLint>(std::sqrt(static_cast<float>(maxTextureSize)));
        const GLint MaxTextureWidth = maxTextureSize;
        const GLint MaxTextureHeight = MaxTextureWidth;
        texture_manager->setTextureCapabilities(static_cast<const int32_t>(maxTextureSize), static_cast<const int32_t>(MaxTextureWidth), static_cast<const int32_t>(MaxTextureHeight));
        return true;
    }

    //int CORE_MANAGER::loop(JNIEnv *, jobject)

    bool CORE_MANAGER::resetDeviceWithNewDimensions(int newWidth, int newHeight)
    {
        return false;
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
        bool oneRender = false;
        for (auto renderTarget : this->device->lsObjectRenderToTarget)
        {
            if (!renderTarget->isObjectOnFrustum)
                continue;
            const RENDER2TARGET_GLES* sf = static_cast<const RENDER2TARGET_GLES*>(renderTarget->specificConfig);
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
                handle = device->specificContextDevice->window.getHwnd();
            #elif (defined(__linux__) || defined(__APPLE__)) && !defined (ANDROID)
                handle = this->device->specificContextDevice->display_x11;
            #elif defined(ANDROID)
                SPECIFIC_AUX_CONTEXT_DEVICE* cJni = device->specificContextDevice;
                JNIEnv *     jenv                 = cJni->jenv;
                handle                            = jenv;
            #endif
            plugin->onSubscribe(static_cast<int>(this->device->backBufferWidth),static_cast<int>(this->device->backBufferHeight),handle);
            return this->lsPlugins.size() - 1;
        }
        return 0xffffffff;
    }

    #if defined _WIN32
    void CORE_MANAGER::setMinMaxSizeWindow(int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y)
    {
        device->specificContextDevice->window.setMinSizeAllowed(min_x,min_y);
        device->specificContextDevice->window.setMaxSizeAllowed(max_x,max_y);
    }
    #elif (defined(__linux__) || defined(__APPLE__)) && !defined(ANDROID)
    void CORE_MANAGER::setMinMaxSizeWindow(int32_t min_x,int32_t min_y,int32_t max_x,int32_t max_y)
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
        if(static_cast<int32_t>(this->device->backBufferWidth) <= max_x && static_cast<int32_t>(this->device->backBufferWidth) >= min_x)
        {
            xsize.base_width    = static_cast<int>(this->device->backBufferWidth);
            xsize.width         = static_cast<int>(this->device->backBufferWidth);
        }
        else
        {
            xsize.base_width    = min_x;
            xsize.width         = static_cast<int>(min_x);
        }

        if(static_cast<int32_t>(this->device->backBufferHeight) <= max_y && static_cast<int32_t>(this->device->backBufferHeight) >= min_y)
        {
            xsize.base_height   = static_cast<int>(this->device->backBufferHeight);
            xsize.height        = static_cast<int>(this->device->backBufferHeight);
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
        XSetWMNormalHints(this->device->specificContextDevice->display_x11,this->device->specificContextDevice->window_x11,&xsize);
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
                sprintf(errStr, "Unknown error gl: decimal:[%d] hexadecimal [0x%x] ", (int)error, (int)error);
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
