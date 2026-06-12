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
| THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE   |
| WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR  |
| COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR       |
| OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.       |
|                                                                                                                        |
|-----------------------------------------------------------------------------------------------------------------------*/

#ifndef ANDROID_BRIDGE_H
#define ANDROID_BRIDGE_H

#include "core-exports.h"
#include <stdint.h>

namespace mbm
{

#if defined(ANDROID)

    API_IMPL const char *androidGetAbsPath() noexcept;
    API_IMPL bool androidAbsPathEndsWithSlash() noexcept;
    API_IMPL void androidAddPath(const char *path);
    API_IMPL const char *androidCopyFileFromAsset(const char *fileName, const char *mode);
    API_IMPL void *androidGetAssetManager() noexcept;
    API_IMPL void androidRequestQuit();
    API_IMPL int androidGetKeyCode(const char *key);
    API_IMPL const char *androidGetKeyName(int key);
    API_IMPL const char *androidGetIdiom();
    API_IMPL const char *androidGetUserName();
    API_IMPL const char *androidSaveFile(const char *defaultName);
    API_IMPL bool androidRequestOpenFile(const char *callback, bool allowMultipleSelects);
    API_IMPL bool androidShowMessageBox(const char *title, const char *message, const char *dialogType);
    API_IMPL const char *androidOpenFolder(const char *title, const char *defaultPath);
    API_IMPL void androidReleaseGraphicsContext(bool wasDeviceLost);
    API_IMPL bool androidEnsureEGLSurface(int *width, int *height);
    API_IMPL void androidSwapBuffers();
    API_IMPL void androidStoreTextureFilters();
    API_IMPL void *androidGetPluginSubscribeHandle() noexcept;
    API_IMPL void androidSetRuntimePaths(const char *absPath, const char *apkPath);
    API_IMPL void androidSetAssetManager(void *assetManager);
    API_IMPL void androidSetNativeWindow(void *nativeWindow);
    API_IMPL bool androidAttachNativeActivityThread(void *javaVm, void *activityObj, const char *packageNameClasses);
    API_IMPL void *androidCreateActivityGlobalRef(void *activityObj);
    API_IMPL void androidDeleteGlobalRef(void *globalRef);
    API_IMPL bool androidCallActivityDoCommands(void *activityObj, const char *cmd, const char *param, char *result,
                                                int maxSize);
    API_IMPL void *androidGetJNIEnv() noexcept;
    API_IMPL void androidSetJNIEnv(void *jniEnv);
    API_IMPL void androidCacheJavaClasses(const char *packageNameClasses);
    API_IMPL uint8_t *androidGetImageDataFromDroid(const char *fileName, int *width, int *height);

#endif

}

#endif
