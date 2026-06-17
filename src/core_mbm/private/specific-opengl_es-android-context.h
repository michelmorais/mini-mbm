/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2026      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
|                                                                                                                        |
| Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated           |
| documentation files (the "Software"), to deal in the Software without restriction, including without limitation        |
| the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and       |
| to permit persons to whom the Software is furnished to do so, subject to the following conditions:                     |
|                                                                                                                        |
| The above copyright notice and this permission notice shall be included in all copies or substantial portions of       |
| the Software.                                                                                                          |
|                                                                                                                        |
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
| IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN  |
| ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER     |
| DEALINGS IN THE SOFTWARE.                                                                                              |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef SPECIFIC_OPENGL_ES_ANDROID_CONTEXT_H
#define SPECIFIC_OPENGL_ES_ANDROID_CONTEXT_H

#if defined(ANDROID)

#include <specific-opengl_es.h>

#include <android/asset_manager.h>
#include <android/native_window.h>
#include <cstdio>
#include <jni.h>
#include <string>

namespace mbm
{
    struct SPECIFIC_AUX_CONTEXT_DEVICE
    {
      public:
        JNIEnv *         jenv;
        std::string      absPath, apkPath;
        AAssetManager*   assetManager;   // NDK asset manager - replaces FileJniEngine JNI
        ANativeWindow*   nativeWindow;   // current ANativeWindow for EGL surface creation
        jclass           jclassDoCommandsJniEngine;     // thin MbmActivity: vibrate / doCommands
        jclass           jclassFileJniEngine;           // Lua file dialogs
        jclass           jclassKeyCodeJniEngine;        // Lua key mapping
        jobject          jclassLoaderGlobal;            // app ClassLoader (for FindClass on native threads)
        jmethodID        jmethodLoadClass;              // ClassLoader.loadClass method

        // EGL context created and owned by NativeActivity C++ code
        EGLDisplay       eglDisplay;
        EGLSurface       eglSurface;
        EGLContext       eglContext;
        EGLConfig        eglConfig;

        GLint filter_GL_TEXTURE_WRAP_S;
        GLint filter_GL_TEXTURE_WRAP_T;
        GLint filter_GL_TEXTURE_MIN_FILTER;
        GLint filter_GL_TEXTURE_MAG_FILTER;

        SPECIFIC_AUX_CONTEXT_DEVICE();
        ~SPECIFIC_AUX_CONTEXT_DEVICE();
        SPECIFIC_AUX_CONTEXT_DEVICE(const SPECIFIC_AUX_CONTEXT_DEVICE &) = delete;
        SPECIFIC_AUX_CONTEXT_DEVICE &operator=(const SPECIFIC_AUX_CONTEXT_DEVICE &) = delete;

        void release(const bool wasDeviceLost);
        const char *getStrToDelete(const char *str);
        void initClassLoader(jobject activityObj);
        void cacheJavaClasses(const char *_packageNameMiniMBMClasses);
        void callQuit();

      private:
        char              packageName[255];
        char              packageNameMiniMBMClasses[255];
        std::string       retPath;
        std::string       buffer_new_stringUTF[10];
        int               index_string_utf;
        jclass getClass(const char *nameClass);
        jclass tryGetClass(const char *nameClass);

      public:
        const char* get_safe_string_utf(const char* string_input);
        #if _DEBUG
            FILE *onFailOpenFile(const int lineNumber, const char *fileName, const char *message);
        #else
            FILE *onFailOpenFile(const int, const char *, const char *);
        #endif
        const int onFailExistFile(const int lineNumber, const char *fileName, const char *message);
        void addPathDroid(const char *fileName);
        int existFileOnAssets(const char *fileName);
        const char *copyFileFromAsset(const char *fileName, const char *mode);
        uint8_t *getImageDataFromDroid(const char *fileName, int *width, int *height);
        FILE *fopenAsset(const char *fileName, const char *mode = "rb");
    };
}

#endif

#endif
