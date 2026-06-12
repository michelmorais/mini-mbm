/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2015 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifdef ANDROID

#include <unistd.h>
#include <sys/stat.h>
#include <cstring>
#include <android/configuration.h>
#include <android/asset_manager.h>
#include <stb_image.h>
#include <platform/mismatch-platform.h>
#include <core_mbm/util-interface.h>
            
#if defined (USE_DUMMY_BACK_END_ENGINE)
    // ANDROID_AND_NOT_OPENGL_ES: For different backend engine on Android, implementation here
    #include <dummy-engine.h> // for REMINDER_TODO, you can remove it after implement the functions
#elif defined          ANDROID
    #include <specific-opengl_es.h>
#endif
#include "specific-opengl_es-android-context.h"
#include <device.h>
#include <audio-interface.h>

#if defined (USE_DUMMY_BACK_END_ENGINE)
namespace mbm
{
    
}

#else
namespace mbm
{
    static SPECIFIC_AUX_CONTEXT_DEVICE *getAndroidContext() noexcept
    {
        mbm::DEVICE *device = mbm::DEVICE::getInstance();
        return device ? device->getSpecificContextDevice() : nullptr;
    }

    const char *androidGetAbsPath() noexcept
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        return context ? context->absPath.c_str() : "";
    }

    bool androidAbsPathEndsWithSlash() noexcept
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        return context && context->absPath.size() > 0 && context->absPath[context->absPath.size() - 1] == '/';
    }

    void androidAddPath(const char *path)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (context)
            context->addPathDroid(path);
    }

    const char *androidCopyFileFromAsset(const char *fileName, const char *mode)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        return context ? context->copyFileFromAsset(fileName, mode) : fileName;
    }

    void *androidGetAssetManager() noexcept
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        return context ? context->assetManager : nullptr;
    }

    void androidRequestQuit()
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (context)
            context->callQuit();
    }

    int androidGetKeyCode(const char *key)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (!context || !context->jclassKeyCodeJniEngine)
            return 0;
        JNIEnv *jenv = context->jenv;
        jmethodID mid = jenv->GetStaticMethodID(context->jclassKeyCodeJniEngine, "getKeyCode",
                                                "(Ljava/lang/String;)I");
        if (mid == nullptr)
        {
            ERROR_AT(__LINE__, __FILE__, "%s", "method getKeyCode not found");
            return 0;
        }
        jstring jstr = jenv->NewStringUTF(context->get_safe_string_utf(key));
        if (jstr == nullptr)
        {
            ERROR_AT(__LINE__, __FILE__, "%s", "error on call NewStringUTF!");
            return 0;
        }
        jint ret = jenv->CallStaticIntMethod(context->jclassKeyCodeJniEngine, mid, jstr);
        jenv->DeleteLocalRef(jstr);
        return static_cast<int>(ret);
    }

    const char *androidGetKeyName(int key)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (!context || !context->jclassKeyCodeJniEngine)
            return nullptr;
        JNIEnv *jenv = context->jenv;
        jmethodID mid = jenv->GetStaticMethodID(context->jclassKeyCodeJniEngine, "getKeyName",
                                                "(I)Ljava/lang/String;");
        if (mid == nullptr)
        {
            ERROR_AT(__LINE__, __FILE__, "%s", "method getKeyName not found");
            return nullptr;
        }
        jstring ret = static_cast<jstring>(jenv->CallStaticObjectMethod(context->jclassKeyCodeJniEngine, mid, key));
        if (ret)
        {
            const char *newRet = jenv->GetStringUTFChars(ret, nullptr);
            const char *result = context->getStrToDelete(newRet);
            jenv->ReleaseStringUTFChars(ret, newRet);
            jenv->DeleteLocalRef(ret);
            return result;
        }
        return nullptr;
    }

    const char *androidGetIdiom()
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (context && context->assetManager)
        {
            AConfiguration *config = AConfiguration_new();
            AConfiguration_fromAssetManager(config, context->assetManager);
            char lang[3] = {};
            AConfiguration_getLanguage(config, lang);
            AConfiguration_delete(config);
            if (lang[0] != 0)
            {
                static std::string language;
                language = lang;
                return language.c_str();
            }
        }
        return "en";
    }

    const char *androidGetUserName()
    {
        const char *methodName = "getUserName";
        const char *signature = "()Ljava/lang/String;";
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (!context || !context->jclassDoCommandsJniEngine)
            return nullptr;
        JNIEnv *jenv = context->jenv;
        jmethodID mid = jenv->GetStaticMethodID(context->jclassDoCommandsJniEngine, methodName, signature);
        if (mid == nullptr)
            return nullptr;
        jstring ret = static_cast<jstring>(jenv->CallStaticObjectMethod(context->jclassDoCommandsJniEngine, mid));
        if (ret)
        {
            const char *newRet = jenv->GetStringUTFChars(ret, nullptr);
            const char *result = context->getStrToDelete(newRet);
            jenv->ReleaseStringUTFChars(ret, newRet);
            jenv->DeleteLocalRef(ret);
            return result;
        }
        ERROR_LOG("To get username from Android you need to add the following permission on XML manifest:\n%s",
                  "<uses-permission android:name=\"android.permission.GET_ACCOUNTS\" />");
        return nullptr;
    }

    const char *androidSaveFile(const char *defaultName)
    {
        const char *methodName = "saveFile";
        const char *signature = "(Ljava/lang/String;)Ljava/lang/String;";
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (!context || !context->jclassDoCommandsJniEngine)
            return nullptr;
        JNIEnv *jenv = context->jenv;
        jmethodID mid = jenv->GetStaticMethodID(context->jclassDoCommandsJniEngine, methodName, signature);
        if (mid == nullptr)
        {
            ERROR_LOG("method not found: %s", methodName);
            return nullptr;
        }
        jstring jstr = jenv->NewStringUTF(context->get_safe_string_utf(defaultName));
        if (jstr == nullptr)
        {
            ERROR_LOG("%s", "error on call NewStringUTF");
            return nullptr;
        }
        jstring ret = static_cast<jstring>(jenv->CallStaticObjectMethod(context->jclassDoCommandsJniEngine, mid, jstr));
        jenv->DeleteLocalRef(jstr);
        if (ret == nullptr)
            return nullptr;
        const char *newRet = jenv->GetStringUTFChars(ret, nullptr);
        const char *fileName = context->getStrToDelete(newRet);
        jenv->ReleaseStringUTFChars(ret, newRet);
        jenv->DeleteLocalRef(ret);
        return fileName;
    }

    bool androidRequestOpenFile(const char *callback, bool allowMultipleSelects)
    {
        const char *methodName = allowMultipleSelects ? "openMultFile" : "getImage";
        const char *signature = "(Ljava/lang/String;)V";
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (!context || !context->jclassDoCommandsJniEngine)
            return false;
        JNIEnv *jenv = context->jenv;
        jmethodID mid = jenv->GetStaticMethodID(context->jclassDoCommandsJniEngine, methodName, signature);
        if (mid == nullptr)
        {
            ERROR_LOG("method not found: %s", methodName);
            return false;
        }
        jstring jstr = jenv->NewStringUTF(context->get_safe_string_utf(callback));
        if (jstr == nullptr)
        {
            ERROR_LOG("%s", "error on call NewStringUTF");
            return false;
        }
        jenv->CallStaticVoidMethod(context->jclassDoCommandsJniEngine, mid, jstr);
        jenv->DeleteLocalRef(jstr);
        return true;
    }

    bool androidShowMessageBox(const char *title, const char *message, const char *dialogType)
    {
        const char *methodName = "messageBox";
        const char *signature = "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Z";
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (!context || !context->jclassFileJniEngine)
            return false;
        JNIEnv *jenv = context->jenv;
        jmethodID mid = jenv->GetStaticMethodID(context->jclassFileJniEngine, methodName, signature);
        if (mid == nullptr)
        {
            ERROR_AT(__LINE__, __FILE__, "method not found: %s", methodName);
            return false;
        }
        jstring jstrTitle = jenv->NewStringUTF(context->get_safe_string_utf(title));
        if (jstrTitle == nullptr)
        {
            ERROR_AT(__LINE__, __FILE__, "%s", "error on call NewStringUTF");
            return false;
        }
        jstring jstrMessage = jenv->NewStringUTF(context->get_safe_string_utf(message));
        if (jstrMessage == nullptr)
        {
            jenv->DeleteLocalRef(jstrTitle);
            ERROR_AT(__LINE__, __FILE__, "%s", "error on call NewStringUTF");
            return false;
        }
        jstring jstrDialogType = jenv->NewStringUTF(context->get_safe_string_utf(dialogType));
        if (jstrDialogType == nullptr)
        {
            jenv->DeleteLocalRef(jstrTitle);
            jenv->DeleteLocalRef(jstrMessage);
            ERROR_AT(__LINE__, __FILE__, "%s", "error on call NewStringUTF");
            return false;
        }
        jboolean ret = jenv->CallStaticBooleanMethod(context->jclassFileJniEngine, mid, jstrTitle, jstrMessage,
                                                     jstrDialogType);
        jenv->DeleteLocalRef(jstrTitle);
        jenv->DeleteLocalRef(jstrMessage);
        jenv->DeleteLocalRef(jstrDialogType);
        return ret;
    }

    const char *androidOpenFolder(const char *title, const char *defaultPath)
    {
        const char *methodName = "openFolder";
        const char *signature = "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;";
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (!context || !context->jclassFileJniEngine)
            return nullptr;
        JNIEnv *jenv = context->jenv;
        jmethodID mid = jenv->GetStaticMethodID(context->jclassFileJniEngine, methodName, signature);
        if (mid == nullptr)
        {
            ERROR_LOG("method not found: %s", methodName);
            return nullptr;
        }
        jstring jstrTitle = jenv->NewStringUTF(context->get_safe_string_utf(title));
        if (jstrTitle == nullptr)
        {
            ERROR_LOG("%s", "error on call NewStringUTF");
            return nullptr;
        }
        jstring jstrDefaultPath = jenv->NewStringUTF(context->get_safe_string_utf(defaultPath));
        if (jstrDefaultPath == nullptr)
        {
            jenv->DeleteLocalRef(jstrTitle);
            ERROR_LOG("%s", "error on call NewStringUTF");
            return nullptr;
        }
        jstring ret = static_cast<jstring>(jenv->CallStaticObjectMethod(context->jclassFileJniEngine, mid, jstrTitle,
                                                                        jstrDefaultPath));
        jenv->DeleteLocalRef(jstrTitle);
        jenv->DeleteLocalRef(jstrDefaultPath);
        if (ret)
        {
            const char *newRet = jenv->GetStringUTFChars(ret, nullptr);
            const char *path = context->getStrToDelete(newRet);
            jenv->ReleaseStringUTFChars(ret, newRet);
            jenv->DeleteLocalRef(ret);
            return path;
        }
        return nullptr;
    }

    void androidReleaseGraphicsContext(bool wasDeviceLost)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (context)
            context->release(wasDeviceLost);
    }

    bool androidEnsureEGLSurface(int *width, int *height)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (!context)
            return false;
        if (!context->nativeWindow)
        {
            ERROR_LOG("EGL: nativeWindow is null, cannot create EGL surface");
            return false;
        }

        if (context->eglDisplay == EGL_NO_DISPLAY)
        {
            context->eglDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
            if (context->eglDisplay == EGL_NO_DISPLAY)
            {
                ERROR_LOG("EGL: eglGetDisplay failed (error 0x%x)", eglGetError());
                return false;
            }
            if (!eglInitialize(context->eglDisplay, nullptr, nullptr))
            {
                ERROR_LOG("EGL: eglInitialize failed (error 0x%x)", eglGetError());
                return false;
            }

            const EGLint configAttribs[] = {
                EGL_RED_SIZE, 8,
                EGL_GREEN_SIZE, 8,
                EGL_BLUE_SIZE, 8,
                EGL_DEPTH_SIZE, 24,
                EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                EGL_NONE
            };
            EGLint numConfigs = 0;
            eglChooseConfig(context->eglDisplay, configAttribs, &context->eglConfig, 1, &numConfigs);
            if (numConfigs == 0)
            {
                INFO_LOG("EGL: depth-24 config not available, trying depth-16");
                const EGLint fallbackAttribs[] = {
                    EGL_RED_SIZE, 8,
                    EGL_GREEN_SIZE, 8,
                    EGL_BLUE_SIZE, 8,
                    EGL_DEPTH_SIZE, 16,
                    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
                    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                    EGL_NONE
                };
                eglChooseConfig(context->eglDisplay, fallbackAttribs, &context->eglConfig, 1, &numConfigs);
            }
            if (numConfigs == 0)
            {
                ERROR_LOG("EGL: eglChooseConfig found no matching config (error 0x%x)", eglGetError());
                return false;
            }
            INFO_LOG("EGL: config chosen, numConfigs=%d", numConfigs);

            EGLint format = 0;
            eglGetConfigAttrib(context->eglDisplay, context->eglConfig, EGL_NATIVE_VISUAL_ID, &format);
            ANativeWindow_setBuffersGeometry(context->nativeWindow, 0, 0, format);

            context->eglSurface = eglCreateWindowSurface(context->eglDisplay, context->eglConfig,
                                                         context->nativeWindow, nullptr);
            if (context->eglSurface == EGL_NO_SURFACE)
            {
                ERROR_LOG("EGL: eglCreateWindowSurface failed (error 0x%x)", eglGetError());
                return false;
            }

            const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
            context->eglContext = eglCreateContext(context->eglDisplay, context->eglConfig, EGL_NO_CONTEXT,
                                                   ctxAttribs);
            if (context->eglContext == EGL_NO_CONTEXT)
            {
                ERROR_LOG("EGL: eglCreateContext failed (error 0x%x)", eglGetError());
                return false;
            }

            if (!eglMakeCurrent(context->eglDisplay, context->eglSurface, context->eglSurface, context->eglContext))
            {
                ERROR_LOG("EGL: eglMakeCurrent failed (error 0x%x)", eglGetError());
                return false;
            }
            INFO_LOG("EGL: context created and made current");
        }
        else if (context->eglSurface == EGL_NO_SURFACE)
        {
            EGLint format = 0;
            eglGetConfigAttrib(context->eglDisplay, context->eglConfig, EGL_NATIVE_VISUAL_ID, &format);
            ANativeWindow_setBuffersGeometry(context->nativeWindow, 0, 0, format);
            context->eglSurface = eglCreateWindowSurface(context->eglDisplay, context->eglConfig,
                                                         context->nativeWindow, nullptr);
            if (context->eglSurface == EGL_NO_SURFACE)
            {
                ERROR_LOG("EGL: resume eglCreateWindowSurface failed (error 0x%x)", eglGetError());
                return false;
            }
            if (!eglMakeCurrent(context->eglDisplay, context->eglSurface, context->eglSurface, context->eglContext))
            {
                ERROR_LOG("EGL: resume eglMakeCurrent failed (error 0x%x)", eglGetError());
                return false;
            }
            INFO_LOG("EGL: surface recreated for resume");
        }

        EGLint surfaceWidth = 0;
        EGLint surfaceHeight = 0;
        eglQuerySurface(context->eglDisplay, context->eglSurface, EGL_WIDTH, &surfaceWidth);
        eglQuerySurface(context->eglDisplay, context->eglSurface, EGL_HEIGHT, &surfaceHeight);
        INFO_LOG("EGL: surface dimensions %d x %d", surfaceWidth, surfaceHeight);
        if (width && surfaceWidth > 0)
            *width = surfaceWidth;
        if (height && surfaceHeight > 0)
            *height = surfaceHeight;
        return true;
    }

    void androidSwapBuffers()
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (context)
            eglSwapBuffers(context->eglDisplay, context->eglSurface);
    }

    void androidStoreTextureFilters()
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (!context)
            return;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &context->filter_GL_TEXTURE_WRAP_S);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &context->filter_GL_TEXTURE_WRAP_T);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, &context->filter_GL_TEXTURE_MIN_FILTER);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, &context->filter_GL_TEXTURE_MAG_FILTER);
    }

    void *androidGetPluginSubscribeHandle() noexcept
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        return context ? context->jenv : nullptr;
    }

    void androidSetRuntimePaths(const char *absPath, const char *apkPath)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (!context)
            return;
        context->absPath = absPath ? absPath : "";
        context->apkPath = apkPath ? apkPath : "";
    }

    void androidSetAssetManager(void *assetManager)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (context)
            context->assetManager = static_cast<AAssetManager *>(assetManager);
    }

    void androidSetNativeWindow(void *nativeWindow)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (context)
            context->nativeWindow = static_cast<ANativeWindow *>(nativeWindow);
    }

    bool androidAttachNativeActivityThread(void *javaVm, void *activityObj, const char *packageNameClasses)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        JavaVM *vm = static_cast<JavaVM *>(javaVm);
        jobject activity = static_cast<jobject>(activityObj);
        if (!context || !vm || !activity)
            return false;
        JNIEnv *jenv = nullptr;
        vm->AttachCurrentThread(&jenv, nullptr);
        if (!jenv)
            return false;
        context->jenv = jenv;
        context->initClassLoader(activity);
        context->cacheJavaClasses(packageNameClasses);
        return true;
    }

    void *androidCreateActivityGlobalRef(void *activityObj)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        jobject activity = static_cast<jobject>(activityObj);
        if (!context || !context->jenv || !activity)
            return nullptr;
        return context->jenv->NewGlobalRef(activity);
    }

    void androidDeleteGlobalRef(void *globalRef)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (context && context->jenv && globalRef)
            context->jenv->DeleteGlobalRef(static_cast<jobject>(globalRef));
    }

    bool androidCallActivityDoCommands(void *activityObj, const char *cmd, const char *param, char *result,
                                       int maxSize)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        jobject activity = static_cast<jobject>(activityObj);
        if (!context || !context->jenv || !activity || !result || maxSize <= 0)
            return false;
        JNIEnv *jenv = context->jenv;
        jclass cls = jenv->GetObjectClass(activity);
        jmethodID mid = jenv->GetMethodID(cls, "OnDoCommands",
                                          "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
        jenv->DeleteLocalRef(cls);
        if (!mid)
            return false;
        jstring jcmd = jenv->NewStringUTF(cmd ? cmd : "");
        jstring jparam = jenv->NewStringUTF(param ? param : "");
        jstring jret = static_cast<jstring>(jenv->CallObjectMethod(activity, mid, jcmd, jparam));
        jenv->DeleteLocalRef(jcmd);
        jenv->DeleteLocalRef(jparam);
        if (jret)
        {
            const char *chars = jenv->GetStringUTFChars(jret, nullptr);
            if (chars)
            {
                strncpy(result, chars, static_cast<size_t>(maxSize) - 1);
                result[maxSize - 1] = '\0';
                jenv->ReleaseStringUTFChars(jret, chars);
            }
            jenv->DeleteLocalRef(jret);
        }
        return true;
    }

    void *androidGetJNIEnv() noexcept
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        return context ? context->jenv : nullptr;
    }

    void androidSetJNIEnv(void *jniEnv)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (context)
            context->jenv = static_cast<JNIEnv *>(jniEnv);
    }

    void androidCacheJavaClasses(const char *packageNameClasses)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        if (context)
            context->cacheJavaClasses(packageNameClasses);
    }

    uint8_t *androidGetImageDataFromDroid(const char *fileName, int *width, int *height)
    {
        SPECIFIC_AUX_CONTEXT_DEVICE *context = getAndroidContext();
        return context ? context->getImageDataFromDroid(fileName, width, height) : nullptr;
    }
    
    SPECIFIC_AUX_CONTEXT_DEVICE::SPECIFIC_AUX_CONTEXT_DEVICE():
    jenv(nullptr),
    absPath(""),
    apkPath(""),
    assetManager(nullptr),
    nativeWindow(nullptr),
    jclassDoCommandsJniEngine(nullptr),
    jclassFileJniEngine(nullptr),
    jclassKeyCodeJniEngine(nullptr),
    jclassLoaderGlobal(nullptr),
    jmethodLoadClass(nullptr),
    eglDisplay(EGL_NO_DISPLAY),
    eglSurface(EGL_NO_SURFACE),
    eglContext(EGL_NO_CONTEXT),
    eglConfig(nullptr),
    filter_GL_TEXTURE_WRAP_S(GL_CLAMP_TO_EDGE),
    filter_GL_TEXTURE_WRAP_T(GL_CLAMP_TO_EDGE),
    filter_GL_TEXTURE_MIN_FILTER(GL_NEAREST),
    filter_GL_TEXTURE_MAG_FILTER(GL_LINEAR),
    index_string_utf(0)
    {
        memset(this->packageName, 0, sizeof(this->packageName));
        memset(this->packageNameMiniMBMClasses, 0, sizeof(this->packageNameMiniMBMClasses));
    }
    
    SPECIFIC_AUX_CONTEXT_DEVICE::~SPECIFIC_AUX_CONTEXT_DEVICE()
    {
        constexpr bool wasDeviceLost = false;
        release(wasDeviceLost);
    }   
    
    void SPECIFIC_AUX_CONTEXT_DEVICE::release(const bool wasDeviceLost)
    {
        // If not lost we are quitting — clean up all references.
        if (wasDeviceLost == false)  // full clean shutdown (destructor)
        {
            this->jenv                         = nullptr;
            this->jclassDoCommandsJniEngine    = nullptr;
            this->jclassFileJniEngine          = nullptr;
            this->jclassKeyCodeJniEngine       = nullptr;
            this->jclassLoaderGlobal           = nullptr;
            this->jmethodLoadClass             = nullptr;
            this->assetManager                 = nullptr;
            this->nativeWindow                 = nullptr;
            this->index_string_utf             = 0;
            memset(this->packageName, 0, sizeof(this->packageName));
            memset(this->packageNameMiniMBMClasses, 0, sizeof(this->packageNameMiniMBMClasses));
            // destroy everything
            if (this->eglDisplay != EGL_NO_DISPLAY)
            {
                eglMakeCurrent(this->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                if (this->eglContext != EGL_NO_CONTEXT)
                {
                    eglDestroyContext(this->eglDisplay, this->eglContext);
                    this->eglContext = EGL_NO_CONTEXT;
                }
                if (this->eglSurface != EGL_NO_SURFACE)
                {
                    eglDestroySurface(this->eglDisplay, this->eglSurface);
                    this->eglSurface = EGL_NO_SURFACE;
                }
                eglTerminate(this->eglDisplay);
                this->eglDisplay = EGL_NO_DISPLAY;
            }
        }
        else  // device-lost (window destroyed) — keep EGL context alive, just destroy surface
        {
            if (this->eglDisplay != EGL_NO_DISPLAY)
            {
                eglMakeCurrent(this->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
                if (this->eglSurface != EGL_NO_SURFACE)
                {
                    eglDestroySurface(this->eglDisplay, this->eglSurface);
                    this->eglSurface = EGL_NO_SURFACE;
                }
            }
        }
    }

    const char * SPECIFIC_AUX_CONTEXT_DEVICE::getStrToDelete(const char *str)
    {
        retPath.clear();
        if (str)
            retPath = str;
        return retPath.c_str();
    }

    // Use the app's ClassLoader (set up by initClassLoader) to find a class by its
    // slash-separated name (e.g. "com/mini/mbm/MbmActivity").  Falls back to
    // FindClass when no class loader is cached — FindClass works correctly on the
    // main JVM thread (JNI_OnLoad) but NOT from attached native threads.
    static jclass findClassViaLoader(JNIEnv *env, jobject loaderObj, jmethodID loadMethod,
                                     const char *slashName)
    {
        if (loaderObj && loadMethod)
        {
            // ClassLoader.loadClass() requires dot-separated names.
            std::string dotName(slashName);
            for (char &c : dotName) if (c == '/') c = '.';
            jstring nameStr = env->NewStringUTF(dotName.c_str());
            jclass local = static_cast<jclass>(
                env->CallObjectMethod(loaderObj, loadMethod, nameStr));
            env->DeleteLocalRef(nameStr);
            if (env->ExceptionCheck()) { env->ExceptionClear(); local = nullptr; }
            return local;
        }
        return env->FindClass(slashName);
    }

    void SPECIFIC_AUX_CONTEXT_DEVICE::initClassLoader(jobject activityObj)
    {
        if (!jenv || !activityObj) return;
        // Retrieve the Activity's ClassLoader — works from any JVM-attached thread.
        jclass actClass = jenv->GetObjectClass(activityObj);
        jmethodID getLoader = jenv->GetMethodID(actClass, "getClassLoader",
                                                 "()Ljava/lang/ClassLoader;");
        jenv->DeleteLocalRef(actClass);
        if (!getLoader) return;
        jobject localLoader = jenv->CallObjectMethod(activityObj, getLoader);
        if (!localLoader) return;
        jclassLoaderGlobal = jenv->NewGlobalRef(localLoader);
        jenv->DeleteLocalRef(localLoader);
        // java/lang/ClassLoader is a system class — FindClass works here.
        jclass loaderClass = jenv->FindClass("java/lang/ClassLoader");
        jmethodLoadClass   = jenv->GetMethodID(loaderClass, "loadClass",
                                               "(Ljava/lang/String;)Ljava/lang/Class;");
        jenv->DeleteLocalRef(loaderClass);
        INFO_LOG("initClassLoader: app ClassLoader cached");
    }

    jclass SPECIFIC_AUX_CONTEXT_DEVICE::tryGetClass(const char *nameClass)
    {
        if (!jenv) return nullptr;
        snprintf(this->packageName, sizeof(this->packageName), "%s/%s",
                 this->packageNameMiniMBMClasses, nameClass);
        jclass localClass = findClassViaLoader(jenv, jclassLoaderGlobal, jmethodLoadClass,
                                               this->packageName);
        if (localClass == nullptr || jenv->ExceptionCheck())
        {
            jenv->ExceptionClear();
            INFO_LOG("tryGetClass: [%s] not found (optional — skipped)", this->packageName);
            return nullptr;
        }
        jclass globalClass = reinterpret_cast<jclass>(jenv->NewGlobalRef(localClass));
        INFO_LOG("tryGetClass: [%s] cached", this->packageName);
        return globalClass;
    }

    void SPECIFIC_AUX_CONTEXT_DEVICE::cacheJavaClasses(const char *_packageNameMiniMBMClasses)
    {
        if (!jenv) return;
        strncpy(packageNameMiniMBMClasses, _packageNameMiniMBMClasses, sizeof(packageNameMiniMBMClasses) - 1);
        // MbmActivity is always required in the APK.
        this->jclassDoCommandsJniEngine    = this->getClass("MbmActivity");
        // The following classes are part of the legacy JNI bridge.
        // They are silently skipped when the NativeActivity build omits them.
        this->jclassFileJniEngine          = this->tryGetClass("FileJniEngine");
        this->jclassKeyCodeJniEngine       = this->tryGetClass("KeyCodeJniEngine");
    }

    jclass SPECIFIC_AUX_CONTEXT_DEVICE::getClass(const char *nameClass)
    {
        snprintf(this->packageName, sizeof(this->packageName), "%s/%s", this->packageNameMiniMBMClasses, nameClass);
        jclass localClass = findClassViaLoader(jenv, jclassLoaderGlobal, jmethodLoadClass,
                                               this->packageName);
        if (localClass == nullptr)
        {
            ERROR_LOG( "FindClass -> [%s] not found!!!", this->packageName);
            _exit(0);
        }
        if (this->jenv->ExceptionCheck()) 
        {
            ERROR_LOG( "FindClass -> [%s] arised ExceptionCheck!!!", this->packageName);
            _exit(0);
        }
        jclass globalClass = reinterpret_cast<jclass>(this->jenv->NewGlobalRef(localClass));
        if (globalClass == nullptr)
        {
            ERROR_LOG( "NewGlobalRef -> [%s] nullptr!!!", this->packageName);
            _exit(0);
        }
        INFO_LOG("FindClass -> [%s] sucess!!!", this->packageName);
        return globalClass;
    }

    
#if _DEBUG
    FILE * SPECIFIC_AUX_CONTEXT_DEVICE::onFailOpenFile(const int lineNumber, const char *fileName, const char *message)
    {
        ERROR_AT(lineNumber, fileName, message);
        return nullptr;
    }
#else
    FILE * SPECIFIC_AUX_CONTEXT_DEVICE::onFailOpenFile(const int, const char *, const char *)
    {
        return nullptr;
    }
#endif
    const int SPECIFIC_AUX_CONTEXT_DEVICE::onFailExistFile(const int lineNumber, const char *fileName, const char *message)
    {
        ERROR_AT(lineNumber, fileName, message);
        return -1;
    }

    void SPECIFIC_AUX_CONTEXT_DEVICE::addPathDroid(const char *fileName)
    {
        // NativeActivity: path registration via JNI is not needed.
        // util::addPath (the only caller on Android) already stores the path in
        // lsPath after calling this function, so nothing extra is required here.
        // The old JNI implementation called a Java FileJniEngine method, which is
        // gone in the NativeActivity build.
        (void)fileName;
    }

    int SPECIFIC_AUX_CONTEXT_DEVICE::existFileOnAssets(const char *fileName)
    {
        if (fileName == nullptr)
            return -1;
        // The decompressed model temp file is always created by us — always present.
        if (strcmp(fileName, util::getDecompressModelFileName()) == 0)
            return 0;
        mbm::DEVICE *device                    = mbm::DEVICE::getInstance();
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE* cJni = device->getSpecificContextDevice();
        if (!cJni->assetManager)
            return this->onFailExistFile(__LINE__, __FILE__, "assetManager is null!");
        AAsset* asset = AAssetManager_open(cJni->assetManager, fileName, AASSET_MODE_STREAMING);
        if (asset == nullptr)
            return -1;
        AAsset_close(asset);
        return 0;
    }

    const char * SPECIFIC_AUX_CONTEXT_DEVICE::copyFileFromAsset(const char *fileName, const char * /*mode*/)
    {
        mbm::DEVICE *device                    = mbm::DEVICE::getInstance();
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE* cJni = device->getSpecificContextDevice();
        if (!cJni->assetManager || cJni->absPath.empty())
            return fileName;
        AAsset* asset = AAssetManager_open(cJni->assetManager, fileName, AASSET_MODE_BUFFER);
        if (!asset)
            return fileName;
        const off_t len = AAsset_getLength(asset);
        if (len <= 0)
        {
            AAsset_close(asset);
            return fileName;
        }
        this->retPath = cJni->absPath;
        if (this->retPath.back() != '/' && this->retPath.back() != '\\')
            this->retPath += '/';
        this->retPath += fileName;
        // Ensure all parent directories exist (e.g. absPath/logic/ for "logic/scene.lua")
        {
            const std::string dir = this->retPath.substr(0, this->retPath.rfind('/'));
            for (size_t i = 1; i < dir.size(); ++i) {
                if (dir[i] == '/') {
                    mkdir(dir.substr(0, i).c_str(), 0755);
                }
            }
            mkdir(dir.c_str(), 0755);
        }
        FILE* fp = fopen(this->retPath.c_str(), "wb");
        if (!fp)
        {
            ERROR_LOG("copyFileFromAsset: fopen failed for '%s'", this->retPath.c_str());
            AAsset_close(asset);
            return fileName;
        }
        const void* buf = AAsset_getBuffer(asset);
        fwrite(buf, 1, static_cast<size_t>(len), fp);
        fclose(fp);
        AAsset_close(asset);
        return this->retPath.c_str();
    }

    uint8_t * SPECIFIC_AUX_CONTEXT_DEVICE::getImageDataFromDroid(const char *fileName, int *width, int *height)
    {
        if (fileName == nullptr)
        {
            ERROR_LOG("fileName on getImageDataFromDroid is null!");
            return nullptr;
        }
        mbm::DEVICE *device                    = mbm::DEVICE::getInstance();
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE* cJni = device->getSpecificContextDevice();
        if (!cJni->assetManager)
        {
            ERROR_LOG("assetManager is null in getImageDataFromDroid!");
            return nullptr;
        }
        AAsset* asset = AAssetManager_open(cJni->assetManager, fileName, AASSET_MODE_BUFFER);
        if (!asset)
        {
            ERROR_LOG("Failed to open asset: %s", fileName);
            return nullptr;
        }
        const off_t    len    = AAsset_getLength(asset);
        const uint8_t* rawBuf = static_cast<const uint8_t*>(AAsset_getBuffer(asset));
        if (!rawBuf || len <= 0)
        {
            AAsset_close(asset);
            ERROR_LOG("Asset buffer empty: %s", fileName);
            return nullptr;
        }
        int w = 0, h = 0, channels = 0;
        // Decode via stb_image — returns RGB (3 channels) to match the old Java path.
        uint8_t* decoded = stbi_load_from_memory(rawBuf, static_cast<int>(len),
                                                  &w, &h, &channels, 3);
        AAsset_close(asset);
        if (!decoded)
        {
            ERROR_LOG("stbi_load_from_memory failed for: %s", fileName);
            return nullptr;
        }
        *width  = w;
        *height = h;
        // Copy into a new[]-allocated buffer so the engine can safely delete[] it.
        const size_t size   = static_cast<size_t>(w) * static_cast<size_t>(h) * 3;
        uint8_t*     result = new uint8_t[size];
        memcpy(result, decoded, size);
        stbi_image_free(decoded);
        return result;
    }

    FILE * SPECIFIC_AUX_CONTEXT_DEVICE::fopenAsset(const char *fileName, const char * /*mode*/)
    {
        mbm::DEVICE *device                    = mbm::DEVICE::getInstance();
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE* cJni = device->getSpecificContextDevice();
        if (!cJni->assetManager)
            return onFailOpenFile(__LINE__, __FILE__, "assetManager is null!");
        AAsset* asset = AAssetManager_open(cJni->assetManager, fileName, AASSET_MODE_STREAMING);
        if (!asset)
            return onFailOpenFile(__LINE__, __FILE__, "AAssetManager_open failed!");
        const off_t len = AAsset_getLength(asset);
        if (len <= 0)
        {
            AAsset_close(asset);
            return onFailOpenFile(__LINE__, __FILE__, "asset length is 0!");
        }
        auto* data = new uint8_t[static_cast<size_t>(len)];
        const off_t bytesRead = AAsset_read(asset, data, static_cast<size_t>(len));
        AAsset_close(asset);
        if (bytesRead != len)
        {
            delete[] data;
            return onFailOpenFile(__LINE__, __FILE__, "AAsset_read returned unexpected size!");
        }
        const char* currentPath = cJni->absPath.c_str();
        if (!currentPath || strlen(currentPath) == 0)
        {
            delete[] data;
            return onFailOpenFile(__LINE__, __FILE__, "absPath not set!");
        }
        std::string tempPath(currentPath);
        if (tempPath.back() != '/' && tempPath.back() != '\\')
            tempPath += '/';
        tempPath += "compatibility.temp";
        FILE* fp = fopen(tempPath.c_str(), "wb");
        if (!fp)
        {
            delete[] data;
            return onFailOpenFile(__LINE__, __FILE__, "failed to create temp file!");
        }
        if (!fwrite(data, static_cast<size_t>(len), 1, fp))
        {
            fclose(fp);
            delete[] data;
            return onFailOpenFile(__LINE__, __FILE__, "failed to write temp file!");
        }
        delete[] data;
        fflush(fp);
        fclose(fp);
        return fopen(tempPath.c_str(), "rb");
    }
 
    const char* SPECIFIC_AUX_CONTEXT_DEVICE::get_safe_string_utf(const char* string_input)//fixed issue Android keep memory to string
    {
        if(index_string_utf > 9)
            index_string_utf = 0;
        if(string_input)
            buffer_new_stringUTF[index_string_utf] = string_input;
        else
            buffer_new_stringUTF[index_string_utf].clear();
        return buffer_new_stringUTF[index_string_utf++].c_str();
    }

    void SPECIFIC_AUX_CONTEXT_DEVICE::callQuit()
    {
        // Signal the android_main loop to stop via the engine's run flag.
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        if (device)
            device->setRun(false);
    }

    

};

int access_file(const char *fileName, int)
{
    if (!fileName || fileName[0] == '\0')
        return -1;
    // Check the real filesystem first (covers files already extracted to absPath).
    if (access(fileName, F_OK) == 0)
        return 0;
    // Fall back to AAssetManager for files still inside the APK.
    mbm::DEVICE *device                    = mbm::DEVICE::getInstance();
    mbm::SPECIFIC_AUX_CONTEXT_DEVICE* cJni = device->getSpecificContextDevice();
    return cJni->existFileOnAssets(fileName);
}

#endif
#endif
