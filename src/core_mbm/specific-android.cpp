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
#include <cstring>
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
#include <device.h>
#include <audio-interface.h>

#if defined (USE_DUMMY_BACK_END_ENGINE)
namespace mbm
{
    
}

#else
namespace mbm
{
    
    SPECIFIC_AUX_CONTEXT_DEVICE::SPECIFIC_AUX_CONTEXT_DEVICE():
    jenv(nullptr),
    absPath(""),
    apkPath(""),
    assetManager(nullptr),
    nativeWindow(nullptr),
    jclassDoCommandsJniEngine(nullptr),
    jclassAudioManagerJniEngine(nullptr),
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
            this->jclassAudioManagerJniEngine  = nullptr;
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
        this->jclassAudioManagerJniEngine  = this->tryGetClass("AudioManagerJniEngine");
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
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE* cJni = device->specificContextDevice;
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
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE* cJni = device->specificContextDevice;
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
        FILE* fp = fopen(this->retPath.c_str(), "wb");
        if (!fp)
        {
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
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE* cJni = device->specificContextDevice;
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
        mbm::SPECIFIC_AUX_CONTEXT_DEVICE* cJni = device->specificContextDevice;
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
            device->run = false;
    }

    
    void SPECIFIC_AUX_CONTEXT_DEVICE::streamStopped(const int indexJNI)
    {
        mbm::DEVICE *device                    = mbm::DEVICE::getInstance();
        AUDIO_MANAGER_INTERFACE* audioManager = device->getAudioManagerInterface();
        if(audioManager)
            audioManager->streamStopped(indexJNI);
    }
};

int access_file(const char *fileName, int)
{
    mbm::DEVICE *device                    = mbm::DEVICE::getInstance();
    mbm::SPECIFIC_AUX_CONTEXT_DEVICE* cJni = device->specificContextDevice;
    std::string fileName_buffer(fileName ? fileName : "");
    return cJni->existFileOnAssets(fileName_buffer.c_str());
}

#endif
#endif
