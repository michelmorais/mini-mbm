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


#include <platform/common-jni.h>
#include <unistd.h>
#include <core_mbm/util-interface.h>



namespace util
{
    extern const char * getDecompressModelFileName();

    COMMON_JNI::COMMON_JNI()
    {
        this->jenv                          = nullptr;
        this->jclassFileJniEngine           = nullptr;
        this->jclassDoCommandsJniEngine     = nullptr;
        this->jclassKeyCodeJniEngine        = nullptr;
        this->jclassInstanceActivityEngine  = nullptr;
        this->jclassAudioManagerJniEngine   = nullptr;
        this->index_string_utf              = 0;

        memset(this->packageName, 0, sizeof(this->packageName));
        memset(this->packageNameMiniMBMClasses, 0, sizeof(this->packageNameMiniMBMClasses));
    }

    COMMON_JNI * COMMON_JNI::getInstance()
    {
        if (instanceComunJni == nullptr)
        {
            instanceComunJni = new COMMON_JNI();
        }
        return instanceComunJni;
    }

    void COMMON_JNI::release()
    {
        if (instanceComunJni != nullptr)
        {
            delete instanceComunJni;
        }
        instanceComunJni = nullptr;
    }

    const char * COMMON_JNI::getStrToDelete(const char *str)
    {
        retPath.clear();
        if (str)
            retPath = str;
        return retPath.c_str();
    }

    void COMMON_JNI::cacheJavaClasses(const char *_packageNameMiniMBMClasses)
    {
        strncpy(packageNameMiniMBMClasses, _packageNameMiniMBMClasses,sizeof(packageNameMiniMBMClasses)-1);
        this->jclassFileJniEngine              = this->getClass("FileJniEngine");
        this->jclassDoCommandsJniEngine        = this->getClass("DoCommandsJniEngine");
        this->jclassKeyCodeJniEngine           = this->getClass("KeyCodeJniEngine");
        this->jclassInstanceActivityEngine     = this->getClass("InstanceActivityEngine");
        this->jclassAudioManagerJniEngine      = this->getClass("AudioManagerJniEngine");
    }

    jclass COMMON_JNI::getClass(const char *nameClass)
    {
        sprintf(this->packageName, "%s/%s", this->packageNameMiniMBMClasses, nameClass);
        jclass localClass = jenv->FindClass(this->packageName);
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
    FILE * COMMON_JNI::onFailOpenFile(const int lineNumber, const char *fileName, const char *message)
    {
        ERROR_AT(lineNumber, fileName, message);
        return nullptr;
    }
#else
    FILE * COMMON_JNI::onFailOpenFile(const int, const char *, const char *)
    {
        return nullptr;
    }
#endif
    const int COMMON_JNI::onFailExistFile(const int lineNumber, const char *fileName, const char *message)
    {
        ERROR_AT(lineNumber, fileName, message);
        return -1;
    }

    void COMMON_JNI::addPathDroid(const char *fileName)
    {
        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassFileJniEngine, "addPath", "(Ljava/lang/String;)V");
        if (mid == nullptr)
        {
            ERROR_LOG( "method addPath not found!");
            return;
        }
        jstring jstr = jenv->NewStringUTF(cJni->get_safe_string_utf(fileName));//fixed issue using local std::string
        if (jstr == nullptr)
        {
            ERROR_LOG( "error on call NewStringUTF!");
            return;
        }
        jenv->CallStaticVoidMethod(cJni->jclassFileJniEngine, mid, jstr);
        jenv->DeleteLocalRef(jstr);
    }

    int COMMON_JNI::existFileOnAssets(const char *fileName)
    {
        if (fileName == nullptr)
            return -1;
        if (strcmp(fileName, getDecompressModelFileName()) == 0) // nao iremos perguntar ao android se existe este arquivo pois nos criamos la
            return 0;
        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassFileJniEngine, "existFileOnAssets", "(Ljava/lang/String;)Z");
        if (mid == nullptr)
            return this->onFailExistFile(__LINE__, __FILE__, "method existFileOnAssets not found!");
        jstring jstr = jenv->NewStringUTF(cJni->get_safe_string_utf(fileName));//fixed issue using local std::string
        if (jstr == nullptr)
            return this->onFailExistFile(__LINE__, __FILE__, "error on call NewStringUTF!");
        jboolean exist = (jboolean)jenv->CallStaticBooleanMethod(cJni->jclassFileJniEngine, mid, jstr);
        jenv->DeleteLocalRef(jstr);
        if (exist)
            return 0;
        return -1;
    }

    const char * COMMON_JNI::copyFileFromAsset(const char *fileName, const char *mode)
    {
        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassFileJniEngine, "copyFileFromAsset",
                                                "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;");
        if (mid == nullptr)
        {
            ERROR_LOG( "method copyFileFromAsset not found!");
            return nullptr;
        }
        jstring jFileName = jenv->NewStringUTF(cJni->get_safe_string_utf(fileName));//fixed issue using local std::string
        if (jFileName == nullptr)
        {
            ERROR_LOG( "error on call NewStringUTF!");
            return nullptr;
        }
        jstring jMode = jenv->NewStringUTF(cJni->get_safe_string_utf(mode));//fixed issue using local std::string
        if (jMode == nullptr)
        {
            jenv->DeleteLocalRef(jFileName);
            ERROR_LOG( "error on call DeleteLocalRef!");
            return nullptr;
        }
        jobject ret = jenv->CallStaticObjectMethod(cJni->jclassFileJniEngine, mid, jFileName, jMode);
        if (ret)
        {
            const char *newPath = jenv->GetStringUTFChars((jstring)ret, nullptr);
            this->retPath       = newPath;
            jenv->ReleaseStringUTFChars((jstring)ret, newPath);
            jenv->DeleteLocalRef(jFileName);
            jenv->DeleteLocalRef(jMode);
            jenv->DeleteLocalRef(ret);
            return this->retPath.c_str();
        }
        jenv->DeleteLocalRef(jFileName);
        jenv->DeleteLocalRef(jMode);
        return fileName;
    }

    uint8_t * COMMON_JNI::getImageDataFromDroid(const char *fileName, int *width, int *height)
    {
        if(fileName == nullptr)
        {
            ERROR_LOG( "fileName on getBytesImage is null!");
            return nullptr;
        }
        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid  = jenv->GetStaticMethodID(cJni->jclassFileJniEngine, "getBytesImage", "(Ljava/lang/String;)[B");
        if (mid == nullptr)
        {
            ERROR_LOG( "method getBytesImage not found!");
            return nullptr;
        }
        static std::string buffer_filename;
        buffer_filename = cJni->get_safe_string_utf(fileName);
        jstring jstr = jenv->NewStringUTF(buffer_filename.c_str());//fixed issue using local std::string
        if (jstr == nullptr)
        {
            ERROR_LOG( "error on call NewStringUTF!");
            return nullptr;
        }
        jbyteArray pixels = (jbyteArray)jenv->CallStaticObjectMethod(cJni->jclassFileJniEngine, mid, jstr);
        if (pixels == nullptr)
        {
            ERROR_LOG("Failed to load from android using getBytesImage method");
            jenv->DeleteLocalRef(jstr);
            jstr = nullptr;
            return nullptr;
        }
        jenv->DeleteLocalRef(jstr);
        jstr                   = nullptr;
        jfieldID fidwidthImage = jenv->GetStaticFieldID(cJni->jclassFileJniEngine, "widthImage", "I");
        if (nullptr == fidwidthImage)
        {
            ERROR_LOG( "Error on get image width!");
            return nullptr;
        }
        jint widthImage = jenv->GetStaticIntField(cJni->jclassFileJniEngine, fidwidthImage);
        if (widthImage == 0)
        {
            ERROR_LOG( "image with ZERO!");
            return nullptr;
        }

        jfieldID fidHeightImage = jenv->GetStaticFieldID(cJni->jclassFileJniEngine, "heightImage", "I");
        if (nullptr == fidHeightImage)
        {
            ERROR_LOG( "Error on get image height!");
            return nullptr;
        }
        jint heightImage = jenv->GetStaticIntField(cJni->jclassFileJniEngine, fidHeightImage);
        if (heightImage == 0)
        {
            ERROR_LOG( "image height ZERO!");
            return nullptr;
        }

        *width        = widthImage;
        *height       = heightImage;
        jbyte *buffer = nullptr;
        if (widthImage < 0 || heightImage < 0) // foi gerado a textura no android então só precisamos pegar o idTexture
        {
            buffer = new jbyte[4];
            this->jenv->GetByteArrayRegion(pixels, 0, 4, buffer);
        }
        else
        {
            buffer = new jbyte[widthImage * heightImage * 3];
            this->jenv->GetByteArrayRegion(pixels, 0, widthImage * heightImage * 3, buffer);
        }
        jenv->DeleteLocalRef(pixels);
        return (uint8_t *)buffer;
    }

    FILE * COMMON_JNI::fopenAsset(const char *fileName, const char *mode)
    {
        util::COMMON_JNI *cJni = util::COMMON_JNI::getInstance();
        JNIEnv *         jenv = cJni->jenv;
        jmethodID        mid =
            jenv->GetStaticMethodID(cJni->jclassFileJniEngine, "openFD", "(Ljava/lang/String;)Ljava/io/FileDescriptor;");
        if (mid == nullptr)
            return (FILE *)this->onFailOpenFile(__LINE__, __FILE__, "method openFD not found!");
        jstring jstr = jenv->NewStringUTF(cJni->get_safe_string_utf(fileName));//fixed issue using local std::string
        if (jstr == nullptr)
            return (FILE *)this->onFailOpenFile(__LINE__, __FILE__, "error on call NewStringUTF!");
        jobject fd_sys = (jobject)jenv->CallStaticObjectMethod(cJni->jclassFileJniEngine, mid, jstr);
        jenv->DeleteLocalRef(jstr);
        jstr = nullptr;
        if (fd_sys == nullptr)
            return (FILE *)this->onFailOpenFile(__LINE__, __FILE__, "error on call CallStaticObjectMethod!");
        jclass fdClass = jenv->FindClass("java/io/FileDescriptor");
        if (fdClass == nullptr)
            return (FILE *)this->onFailOpenFile(__LINE__, __FILE__, "error on call FindClass FileDescriptor!");
        jfieldID fdClassDescriptorFieldID = jenv->GetFieldID(fdClass, "descriptor", "I");
        if (fdClassDescriptorFieldID == nullptr)
            return (FILE *)this->onFailOpenFile(__LINE__, __FILE__, "error on call GetFieldID descriptor!");
        jint fd = jenv->GetIntField(fd_sys, fdClassDescriptorFieldID);
        if (fd == 0)
            return nullptr;
        jfieldID fidOffset = jenv->GetStaticFieldID(cJni->jclassFileJniEngine, "offset", "J");
        if (nullptr == fidOffset)
            return (FILE *)this->onFailOpenFile(__LINE__, __FILE__, "error on get offset from file!");
        jlong offset = jenv->GetStaticLongField(cJni->jclassFileJniEngine, fidOffset);

        jfieldID fidLen = jenv->GetStaticFieldID(cJni->jclassFileJniEngine, "len", "J");
        if (nullptr == fidLen)
            return (FILE *)this->onFailOpenFile(__LINE__, __FILE__, "error on get len from file!");
        jlong len = jenv->GetStaticLongField(cJni->jclassFileJniEngine, fidLen);
        if (len == 0)
            return (FILE *)this->onFailOpenFile(__LINE__, __FILE__, "len from file ZERO!");
        // int myfd = dup(fd);
        FILE *fp = fdopen(fd, mode);
        if (!fp)
            return (FILE *)this->onFailOpenFile(__LINE__, __FILE__, "failed fdopen method!");
        fseek(fp, offset, SEEK_CUR);
        auto *data        = new uint8_t[len];
        COMMON_JNI *    comunJni    = COMMON_JNI::getInstance();
        const char *   currentPath = comunJni->absPath.c_str();
        if (!currentPath || strlen(currentPath) == 0)
        {
            fclose(fp);
            delete[] data;
            return (FILE *)this->onFailOpenFile(__LINE__, __FILE__, "absPath not set!");
        }
        std::string newFileTemp(currentPath);
        newFileTemp += "compatibility.temp";
        if (!fread(data, len, 1, fp))
        {
            fclose(fp);
            delete[] data;
            return (FILE *)this->onFailOpenFile(__LINE__, __FILE__, "failed to read data from file asset!");
        }
        fclose(fp);
        fp = fopen(newFileTemp.c_str(), "wb");
        if (!fp)
        {
            delete[] data;
            return (FILE *)this->onFailOpenFile(__LINE__, __FILE__, "failed to create temp file asset @@@!");
        }
        if (!fwrite(data, len, 1, fp))
        {
            fclose(fp);
            delete[] data;
            return (FILE *)this->onFailOpenFile(__LINE__, __FILE__, "failed to write to file tmp!");
        }
        delete[] data;
        if (fflush(fp))
            return (FILE *)this->onFailOpenFile(__LINE__, __FILE__, "failed to flush file temp!");
        if (fclose(fp) == EOF)
            return (FILE *)this->onFailOpenFile(__LINE__, __FILE__, "failed to close file temp!");
        jenv->DeleteLocalRef(fd_sys);
        jenv->DeleteLocalRef(fdClass);
        return fopen(newFileTemp.c_str(), "rb");
    }
 
    const char* COMMON_JNI::get_safe_string_utf(const char* string_input)//fixed issue Android keep memory to string
    {
        if(index_string_utf > 9)
            index_string_utf = 0;
        if(string_input)
            buffer_new_stringUTF[index_string_utf] = string_input;
        else
            buffer_new_stringUTF[index_string_utf].clear();
        return buffer_new_stringUTF[index_string_utf++].c_str();
    }

    util::COMMON_JNI *util::COMMON_JNI::instanceComunJni = nullptr;
};

int access_file(const char *fileName, int)
{
    util::COMMON_JNI *comunJni = util::COMMON_JNI::getInstance();
    std::string fileName_buffer(fileName ? fileName : "");
    return comunJni->existFileOnAssets(fileName_buffer.c_str());
}
