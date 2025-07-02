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

#include <jni.h>
#include <stdlib.h>

#include <core_mbm/device.h>
#include <core_mbm/util-interface.h>
#include <platform/common-jni.h>

#include "scene-1.h" // your scene C++

#ifndef ANDROID
    #error "Target to this main is ANDRIOD"
#endif

#ifndef PACKAGE_NAME_CLASS
    #define PACKAGE_NAME_CLASS "com/mini/mbm" // how must be (if changed the whole class path at java must be replaced)
#endif


MY_GAME *game = nullptr;

extern "C" 
{
    JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved);
};

void MiniMbmEngine_init(JNIEnv *env, jobject obj, jint width, jint height, jstring absPath, jstring apkPath);
void MiniMbmEngine_loop(JNIEnv *env, jobject obj);
void MiniMbmEngine_onTouchDown(JNIEnv *env, jobject obj, int key, float x, float y);
void MiniMbmEngine_onTouchUp(JNIEnv *env, jobject obj, int key, float x, float y);
void MiniMbmEngine_onTouchMove(JNIEnv *env, jobject obj, int key, float x, float y);
void MiniMbmEngine_onTouchZoom(JNIEnv *env, jobject obj, float zoom);
void MiniMbmEngine_onKeyDown(JNIEnv *env, jobject obj, int key);
void MiniMbmEngine_onKeyUp(JNIEnv *env, jobject obj, int key);
void MiniMbmEngine_onKeyDownJoystick(JNIEnv *env, jobject obj, int player, int key);
void MiniMbmEngine_onKeyUpJoystick(JNIEnv *env, jobject obj, int player, int key);
void MiniMbmEngine_onMoveJoystick(JNIEnv *env, jobject obj, int player, float lx, float ly, float rx, float ry);
void MiniMbmEngine_onInfoDeviceJoystick(JNIEnv *env, jobject obj, int player, int maxNumberButton, jstring jstrDeviceName,
                                  jstring jextraInfo);
void MiniMbmEngine_streamStopped(JNIEnv *env, jobject obj, int indexJNI);
bool MiniMbmEngine_onRestoreDevice(JNIEnv *env, jobject obj, jint width, jint height);
void MiniMbmEngine_onStop(JNIEnv *env, jobject obj);
void MiniMbmEngine_onCallBackCommands(JNIEnv *env, jobject obj, jstring param1, jstring param2);

void MiniMbmEngine_init(JNIEnv *env, jobject obj, jint width, jint height, jstring absPath, jstring apkPath)
{
    const char *_absPath = env->GetStringUTFChars(absPath, nullptr);
    const char *_apkPath = env->GetStringUTFChars(apkPath, nullptr);
    setenv("absPath", _absPath, 1);
    setenv("apkPath", _apkPath, 1);
    if (game != nullptr)
    {
        if (width > 0)
            game->device->backBufferWidth = static_cast<float>(width);
        if (height > 0)
            game->device->backBufferHeight = static_cast<float>(height);
        game->device->jni->absPath         = _absPath ? _absPath : "";
        game->device->jni->apkPath         = _apkPath ? _apkPath : "";
    }
    else
    {
        game = new MY_GAME(env, obj);
        if (game)
        {
            INFO_LOG("lib mini-mbm initialized\n width: %d height: %d", width, height);
			game->device->ptrManager       = game;
            game->device->backBufferWidth  = static_cast<float>(width);
            game->device->backBufferHeight = static_cast<float>(height);
            game->device->jni->absPath     = _absPath ? _absPath : "";
            game->device->jni->apkPath     = _apkPath ? _apkPath : "";
            if(game->initGl(width, height))
				game->loop(env, obj);
        }
    }
    env->ReleaseStringUTFChars(absPath, _absPath);
    env->ReleaseStringUTFChars(apkPath, _apkPath);
}

void MiniMbmEngine_loop(JNIEnv *env, jobject obj)
{
    if (game)
    {
        game->device->ptrManager = game;
        game->loop(env, obj);
    }
}

void MiniMbmEngine_onTouchDown(JNIEnv *env, jobject obj, int key, float x, float y)
{
    if (game)
        game->onTouchDown(key, x, y);
}

void MiniMbmEngine_onTouchUp(JNIEnv *env, jobject obj, int key, float x, float y)
{
    if (game)
        game->onTouchUp(key, x, y);
}

void MiniMbmEngine_onTouchMove(JNIEnv *env, jobject obj, int key, float x, float y)
{
    if (game)
        game->onTouchMove(key, x, y);
}

void MiniMbmEngine_onTouchZoom(JNIEnv *env, jobject obj, float zoom)
{
    if (game)
        game->onTouchZoom(zoom);
}

void MiniMbmEngine_onKeyDown(JNIEnv *env, jobject obj, int key)
{
    if (game)
        game->onKeyDown(key);
}

void MiniMbmEngine_onKeyUp(JNIEnv *env, jobject obj, int key)
{
    if (game)
        game->onKeyUp(key);
}

void MiniMbmEngine_onKeyDownJoystick(JNIEnv *env, jobject obj, int player, int key)
{
    if (game)
        game->onKeyDownJoystick(player, key);
}

void MiniMbmEngine_onKeyUpJoystick(JNIEnv *env, jobject obj, int player, int key)
{
    if (game)
        game->onKeyUpJoystick(player, key);
}

void MiniMbmEngine_onMoveJoystick(JNIEnv *env, jobject obj, int player, float lx, float ly, float rx, float ry)
{
    if (game)
        game->onMoveJoystick(player, lx, ly, rx, ry);
}

void MiniMbmEngine_onInfoDeviceJoystick(JNIEnv *env, jobject obj, int player, int maxNumberButton, jstring jstrDeviceName,
                                  jstring jextraInfo)
{
    if (game)
    {
        const char *strDeviceName = env->GetStringUTFChars(jstrDeviceName, nullptr);
        const char *extraInfo     = env->GetStringUTFChars(jextraInfo, nullptr);
        game->onInfoDeviceJoystick(player, maxNumberButton, strDeviceName, extraInfo);
        env->ReleaseStringUTFChars(jstrDeviceName, strDeviceName);
        env->ReleaseStringUTFChars(jextraInfo, extraInfo);
    }
}

void MiniMbmEngine_streamStopped(JNIEnv *env, jobject obj, int indexJNI)
{
    if (game && game->device->scene)
    {
        game->device->streamStopped(indexJNI);
    }
}

bool MiniMbmEngine_onRestoreDevice(JNIEnv *env, jobject obj, jint width, jint height)
{
    if (game)
        return game->onLostDevice(env, obj, static_cast<int>(width), static_cast<int>(height));
    return true;
}

void MiniMbmEngine_onStop(JNIEnv *env, jobject obj)
{
    util::COMMON_JNI *cJni      = util::COMMON_JNI::getInstance();
    JavaVM *         jvm        = nullptr;
    JNIEnv *         oldJenv    = cJni->jenv;
    int              getEnvStat = JNI_OK;
    int              status     = env->GetJavaVM(&jvm);
    if (status != 0)
    {
        ERROR_LOG("Failed to GetJavaVM status %d",status);
        return;
    }
    if (env != cJni->jenv)
    {
        getEnvStat = JNI_EDETACHED;
        status = jvm->AttachCurrentThread(&env, nullptr);
        if (status != 0)
        {
            ERROR_LOG("Failed to attach, status [%d]",status);
            return;
        }
        cJni->jenv = env;
    }
    if (game)
        game->onStop();
    if (getEnvStat == JNI_EDETACHED)
    {
        /*
        Não posso dar DetachCurrentThread pois no lollipop causa:
        "attempting to detach while still running code"
        jvm->DetachCurrentThread();
        */
        cJni->jenv = oldJenv;
    }
}

void MiniMbmEngine_onCallBackCommands(JNIEnv *env, jobject obj, jstring param1, jstring param2)
{
    if (env && game && game->device && game->device->scene && game->device->scene->userData)
    {
        if (param1 && param2)
        {
            const char *p1 = env->GetStringUTFChars(param1, nullptr);
            const char *p2 = env->GetStringUTFChars(param2, nullptr);
            game->device->scene->onCallBackCommands(p1, p2);
            env->ReleaseStringUTFChars(param1, p1);
            env->ReleaseStringUTFChars(param2, p2);
        }
        else if (param1)
        {
            const char *p1 = env->GetStringUTFChars(param1, nullptr);
            game->device->scene->onCallBackCommands(p1, "NULL");
            env->ReleaseStringUTFChars(param1, p1);
        }
        else if (param2)
        {
            const char *p2 = env->GetStringUTFChars(param2, nullptr);
            game->device->scene->onCallBackCommands("NULL", p2);
            env->ReleaseStringUTFChars(param2, p2);
        }
        else
        {
            ERROR_AT(__LINE__,__FILE__,"Call back expected command [%d] [%s] [%s]",__LINE__,"NULL","NULL");
            game->device->scene->onCallBackCommands("NULL", "NULL");
        }
    }
    else 
    {
        if(game == nullptr)
        {
            ERROR_AT(__LINE__,__FILE__,"%s","Engine is not ready yet!\n class game is null!");
        }
        else if(game->device == nullptr)
        {
            ERROR_AT(__LINE__,__FILE__,"%s","Engine is not ready yet!\n class game->device is null!");
        }
        else if(game->device->scene == nullptr)
        {
            ERROR_AT(__LINE__,__FILE__,"%s","Engine is not ready yet!\n class game->device->scene is null!");
        }
        else if(game->device->scene->userData == nullptr)
        {
            ERROR_AT(__LINE__,__FILE__,"%s","Engine is not ready yet!\n class game->device->scene->userData is null!");
        }
        else if(env == nullptr)
        {
            ERROR_AT(__LINE__,__FILE__,"%s","Engine is not ready yet!\n class env is null!");
        }
        
    }
}

/*
        B = byte
        C = char
        D = double
        F = float
        I = int
        J = long
        S = short
        V = void
        Z = boolean
        Lfully-qualified-class = fully qualified class
        [type = array of type
        (argument types)return type = method type. If no arguments, use empty argument types: ().
        If return type is void (or constructor) use (argument types)V.
*/

static JNINativeMethod methodTableJNI[] = {
    {"init", "(IILjava/lang/String;Ljava/lang/String;)V",					(void *)MiniMbmEngine_init},
    {"loop", "()V",															(void *)MiniMbmEngine_loop},
    {"onTouchDown", "(IFF)V",												(void *)MiniMbmEngine_onTouchDown},
    {"onTouchUp", "(IFF)V",													(void *)MiniMbmEngine_onTouchUp},
    {"onTouchMove", "(IFF)V",												(void *)MiniMbmEngine_onTouchMove},
    {"onTouchZoom", "(F)V",													(void *)MiniMbmEngine_onTouchZoom},
    {"onKeyDown", "(I)V",													(void *)MiniMbmEngine_onKeyDown},
    {"onKeyUp", "(I)V",														(void *)MiniMbmEngine_onKeyUp},
    {"onKeyDownJoystick", "(II)V",											(void *)MiniMbmEngine_onKeyDownJoystick},
    {"onKeyUpJoystick", "(II)V",											(void *)MiniMbmEngine_onKeyUpJoystick},
    {"onMoveJoystick", "(IFFFF)V",											(void *)MiniMbmEngine_onMoveJoystick},
    {"onInfoDeviceJoystick", "(IILjava/lang/String;Ljava/lang/String;)V",	(void *)MiniMbmEngine_onInfoDeviceJoystick},
    {"streamStopped", "(I)V",						                 		(void *)MiniMbmEngine_streamStopped},
    {"onRestoreDevice", "(II)Z",											(void *)MiniMbmEngine_onRestoreDevice},
    {"onStop", "()V",														(void *)MiniMbmEngine_onStop},
    {"onCallBackCommands", "(Ljava/lang/String;Ljava/lang/String;)V",		(void *)MiniMbmEngine_onCallBackCommands}};

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void * /*reserved*/)
{
    JNIEnv *   env                = nullptr;
    char       MiniMbmEngine_class[255] = "";
    const jint result             = -1;
    int status = vm->GetEnv((void **)&env, JNI_VERSION_1_6);
    if (status != JNI_OK)
    {
        ERROR_LOG("GetEnv failed, status [%d]",status);
        return result;
    }
    if (env == nullptr)
    {
        ERROR_LOG("%s","GetEnv failed. env == NULL");
        return result;
    }
    /*
    //Tentativa de pegar o pakage dinamico mas nao deu :(
    //Tenho que pegar a instancia da classe atual; sem o package nao me parece possivel.
    //log_util::err(__LINE__,__FILE__,"android_content_Context:[%p,%s]",reserved,(char*)reserved);//android_content_Context:[0x0,(null)]
    jclass android_content_Context	=	env->FindClass("android/app.Activity");
    if(android_content_Context)
    {
        log_util::err(__LINE__,__FILE__,"android_content_Context");
        jmethodID midGetPackageName		=	
        env->GetMethodID( android_content_Context, "getPackageName","()Ljava/lang/String;");
        if(midGetPackageName)
        {
            log_util::err(__LINE__,__FILE__,"android_content_Context");
            jstring packageName				=	(jstring)env->CallObjectMethod(android_content_Context,
    midGetPackageName);
            if(packageName)
            {
                log_util::err(__LINE__,__FILE__,"android_content_Context");
                const char* packageNameRec		=	env->GetStringUTFChars(packageName, NULL);
                log_util::err(__LINE__,__FILE__,"PackageName native'%s'", packageNameRec);
                env->ReleaseStringUTFChars(packageName,packageNameRec);
            }
            else
            {
                log_util::err(__LINE__,__FILE__,"android_content_Context");
            }
        }
    }
    */
    sprintf(MiniMbmEngine_class, "%s/MiniMbmEngine", PACKAGE_NAME_CLASS);
    jclass clazz = env->FindClass(MiniMbmEngine_class);
    if (clazz == nullptr)
    {
        ERROR_LOG("Native registration unable to find class '%s'", MiniMbmEngine_class);
        return JNI_FALSE;
    }
    env->RegisterNatives(clazz, methodTableJNI, sizeof(methodTableJNI) / sizeof(methodTableJNI[0]));
    util::COMMON_JNI *jniInstance = util::COMMON_JNI::getInstance();
    jniInstance->jenv            = env;
    jniInstance->cacheJavaClasses(PACKAGE_NAME_CLASS);
    return JNI_VERSION_1_6;
}
