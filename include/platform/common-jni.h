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

#ifndef COMMON_JNI_GLES_H
#define COMMON_JNI_GLES_H

#if defined ANDROID

#include <string>
#include <stdio.h>
#include <jni.h>
#include <string>
#include <sstream>

namespace util
{
    class COMMON_JNI
    {
      public:
        JNIEnv *    jenv;
        std::string absPath, apkPath;
        jclass      jclassFileJniEngine;
        jclass      jclassDoCommandsJniEngine;
        jclass      jclassKeyCodeJniEngine;
        jclass      jclassInstanceActivityEngine;
        jclass      jclassAudioManagerJniEngine;
        COMMON_JNI();
        static COMMON_JNI *getInstance();
        static void release();
        const char *getStrToDelete(const char *str);
        void cacheJavaClasses(const char *_packageNameMiniMBMClasses);
      private:
        static COMMON_JNI *instanceComunJni;
        char              packageName[255];
        char              packageNameMiniMBMClasses[255];
        std::string       retPath;
        std::string       buffer_new_stringUTF[10];
        int               index_string_utf;
        jclass getClass(const char *nameClass);
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
};

int access_file(const char *fileName, int);

#endif
#endif
