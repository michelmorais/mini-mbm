/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2026 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

// Android NativeActivity entry point — mirrors platform-ios/MetalViewController.mm.
// android_main() replaces the old JNI-based main-lua.cpp / main.cpp.
// All engine state lives in the heap-allocated s_game pointer.

#if defined(ANDROID)

#include <android_native_app_glue.h>
#include <android/log.h>
#include <android/input.h>
#include <jni.h>
#include <string>
#include <map>

#ifdef USE_LUA
#   include <lua-wrap/manager-lua.h>
#else
#   include "my-scene.h"
#endif

#include <core_mbm/device.h>
#include <core_mbm/specific-opengl_es.h>
#include <core_mbm/util-interface.h>

// ---------------------------------------------------------------------------
// Package name for the thin MbmActivity JNI calls (vibrate / doCommands).
// Change this if you renamed the Java package in your game project.
// ---------------------------------------------------------------------------
#ifndef PACKAGE_NAME_CLASS
#   define PACKAGE_NAME_CLASS "com/mini/mbm"
#endif

// ---------------------------------------------------------------------------
// Global engine instance — survives across window focus changes.
// ---------------------------------------------------------------------------
#ifdef USE_LUA
static mbm::LUA_MANAGER* s_game = nullptr;
#else
static MY_GAME*           s_game = nullptr;
#endif

static bool s_running        = false;
static bool s_windowReady    = false;
static bool s_isRestoring    = false; // true while onLostDevice steps are in progress

// ---------------------------------------------------------------------------
// Multi-touch tracking: map pointer-id → stable integer finger index.
// ---------------------------------------------------------------------------
static std::map<int32_t, int> s_touchMap;
static int                    s_nextTouchID = 0; // 0-based: primary finger = 0, matching desktop convention (0=left, 1=right, 2=middle)

static int touchID(int32_t pointerId)
{
    auto it = s_touchMap.find(pointerId);
    if (it != s_touchMap.end())
        return it->second;
    int id = s_nextTouchID++;
    s_touchMap[pointerId] = id;
    return id;
}

static void releaseTouch(int32_t pointerId)
{
    s_touchMap.erase(pointerId);
    if (s_touchMap.empty())
        s_nextTouchID = 0; // reset to 0 so next single touch is always key=0
}

// ---------------------------------------------------------------------------
// Input event handler (called by android_native_app_glue poll loop).
// ---------------------------------------------------------------------------
static int32_t onInputEvent(struct android_app* app, AInputEvent* event)
{
    if (!s_game)
        return 0;

    const int32_t eventType = AInputEvent_getType(event);

    if (eventType == AINPUT_EVENT_TYPE_MOTION)
    {
        const int32_t action    = AMotionEvent_getAction(event);
        const int32_t actionCode = action & AMOTION_EVENT_ACTION_MASK;
        const int32_t pointerIdx = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
                                    >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

        switch (actionCode)
        {
            case AMOTION_EVENT_ACTION_DOWN:
            case AMOTION_EVENT_ACTION_POINTER_DOWN:
            {
                const int32_t pid = AMotionEvent_getPointerId(event, pointerIdx);
                const float   x   = AMotionEvent_getX(event, pointerIdx);
                const float   y   = AMotionEvent_getY(event, pointerIdx);
                s_game->onTouchDown(touchID(pid), x, y);
                return 1;
            }
            case AMOTION_EVENT_ACTION_UP:
            case AMOTION_EVENT_ACTION_POINTER_UP:
            {
                const int32_t pid = AMotionEvent_getPointerId(event, pointerIdx);
                const float   x   = AMotionEvent_getX(event, pointerIdx);
                const float   y   = AMotionEvent_getY(event, pointerIdx);
                s_game->onTouchUp(touchID(pid), x, y);
                releaseTouch(pid);
                return 1;
            }
            case AMOTION_EVENT_ACTION_MOVE:
            {
                const size_t count = AMotionEvent_getPointerCount(event);
                if (count == 1)
                {
                    const int32_t pid = AMotionEvent_getPointerId(event, 0);
                    const float   x   = AMotionEvent_getX(event, 0);
                    const float   y   = AMotionEvent_getY(event, 0);
                    s_game->onTouchMove(touchID(pid), x, y);
                }
                else if (count >= 2)
                {
                    // Two-finger pinch → compute distance delta as zoom factor
                    const float x0 = AMotionEvent_getX(event, 0);
                    const float y0 = AMotionEvent_getY(event, 0);
                    const float x1 = AMotionEvent_getX(event, 1);
                    const float y1 = AMotionEvent_getY(event, 1);
                    const float dx = x1 - x0;
                    const float dy = y1 - y0;
                    const float dist = sqrtf(dx * dx + dy * dy);
                    s_game->onTouchZoom(dist);
                }
                return 1;
            }
            case AMOTION_EVENT_ACTION_CANCEL:
            {
                const size_t count = AMotionEvent_getPointerCount(event);
                for (size_t i = 0; i < count; ++i)
                {
                    const int32_t pid = AMotionEvent_getPointerId(event, i);
                    releaseTouch(pid);
                }
                return 1;
            }
        }
        return 0;
    }

    if (eventType == AINPUT_EVENT_TYPE_KEY)
    {
        const int32_t keyCode = AKeyEvent_getKeyCode(event);
        // Let the system handle volume keys so the hardware buttons control media volume.
        if (keyCode == AKEYCODE_VOLUME_UP || keyCode == AKEYCODE_VOLUME_DOWN)
            return 0;
        // Back button → quit cleanly; consume the event so the system doesn't handle it.
        if (keyCode == AKEYCODE_BACK)
        {
            const int32_t action = AKeyEvent_getAction(event);
            if (action == AKEY_EVENT_ACTION_UP && s_game)
                s_game->device->run = false;
            return 1;
        }
        const int32_t action  = AKeyEvent_getAction(event);
        if (action == AKEY_EVENT_ACTION_DOWN)
        {
            s_game->onKeyDown(keyCode);
            return 1;
        }
        else if (action == AKEY_EVENT_ACTION_UP)
        {
            s_game->onKeyUp(keyCode);
            return 1;
        }
    }

    return 0;
}

// ---------------------------------------------------------------------------
// App command handler (called by android_native_app_glue).
// ---------------------------------------------------------------------------
static void onAppCmd(struct android_app* app, int32_t cmd)
{
    switch (cmd)
    {
        case APP_CMD_INIT_WINDOW:
        {
            if (app->window == nullptr)
                break;

            // Derive paths from the native activity.
            ANativeActivity* activity = app->activity;
            const std::string absPath = activity->internalDataPath
                                         ? activity->internalDataPath
                                         : "";
            const std::string apkPath = activity->obbPath
                                         ? activity->obbPath
                                         : "";

            setenv("absPath", absPath.c_str(), 1);
            setenv("apkPath", apkPath.c_str(),  1);

            const int w = ANativeWindow_getWidth(app->window);
            const int h = ANativeWindow_getHeight(app->window);

            if (s_game != nullptr)
            {
                // Window was re-created after resume — restore device.
                INFO_LOG("mini-mbm: window re-created %d x %d", w, h);
                mbm::SPECIFIC_AUX_CONTEXT_DEVICE* ctx = s_game->device->specificContextDevice;
                ctx->absPath = absPath;
                ctx->apkPath = apkPath;
                ctx->nativeWindow = app->window;
                // Kick off the restore state machine — each step runs as one frame
                // in the main loop (see s_isRestoring below), so Android can render
                // the hourglass / loading progress while textures reload.
                s_isRestoring = true;
                s_windowReady = true;
                s_running     = true;
            }
            else
            {
                INFO_LOG("mini-mbm: APP_CMD_INIT_WINDOW %d x %d", w, h);

                // Attach to the JVM — required by LUA_MANAGER(JNIEnv*, jobject) on Android.
                JNIEnv* jenv = nullptr;
                app->activity->vm->AttachCurrentThread(&jenv, nullptr);

#ifdef USE_LUA
                s_game = new mbm::LUA_MANAGER(jenv, app->activity->clazz);
#else
                // Pure C++ path — MY_GAME constructor calls setScene().
                s_game = new MY_GAME();
#endif
                if (!s_game)
                {
                    ERROR_LOG("mini-mbm: failed to create engine instance");
                    break;
                }

                s_game->device->backBufferWidth  = static_cast<float>(w);
                s_game->device->backBufferHeight = static_cast<float>(h);
                s_game->device->ptrManager       = s_game;

                // Store the AAssetManager and native window in the platform context.
                mbm::SPECIFIC_AUX_CONTEXT_DEVICE* ctx = s_game->device->specificContextDevice;
                ctx->absPath     = absPath;
                ctx->apkPath     = apkPath;
                ctx->assetManager = app->activity->assetManager;
                ctx->nativeWindow = app->window;

                // Cache the thin MbmActivity JNI class for vibrate / doCommands.
                if (jenv)
                {
                    ctx->jenv = jenv;
                    // android_native_app_glue runs android_main on a new native thread.
                    // FindClass on a non-main thread uses the bootstrap class loader and
                    // cannot see app classes.  initClassLoader captures the Activity's
                    // ClassLoader so that getClass/tryGetClass can use it instead.
                    ctx->initClassLoader(app->activity->clazz);
                    ctx->cacheJavaClasses(PACKAGE_NAME_CLASS);
                }

#ifdef USE_LUA
                constexpr bool border = false;
                if (!s_game->initializeSceneLua(w, h, w, h, border))
                {
                    ERROR_LOG("mini-mbm: initializeSceneLua failed");
                    delete s_game;
                    s_game = nullptr;
                    break;
                }
                s_game->run();
#else
                constexpr bool border = false;
                if (!s_game->initGraphics("mini_mbm", w, h, 0, 0, border, false))
                {
                    ERROR_LOG("mini-mbm: initGraphics failed");
                    delete s_game;
                    s_game = nullptr;
                    break;
                }
#endif
                s_windowReady = true;
                s_running     = true;
            }
            break;
        }

        case APP_CMD_TERM_WINDOW:
        {
            INFO_LOG("mini-mbm: APP_CMD_TERM_WINDOW");
            s_windowReady = false;
            if (s_game)
            {
                // Release GL resources — device is lost.
                mbm::SPECIFIC_AUX_CONTEXT_DEVICE* ctx = s_game->device->specificContextDevice;
                ctx->nativeWindow = nullptr;
                constexpr bool wasDeviceLost = true;
                ctx->release(wasDeviceLost);
            }
            break;
        }

        case APP_CMD_GAINED_FOCUS:
            INFO_LOG("mini-mbm: APP_CMD_GAINED_FOCUS");
            s_running = (s_game != nullptr && s_windowReady);
            break;

        case APP_CMD_LOST_FOCUS:
            INFO_LOG("mini-mbm: APP_CMD_LOST_FOCUS");
            s_running = false;
            break;

        case APP_CMD_STOP:
            INFO_LOG("mini-mbm: APP_CMD_STOP");
            if (s_game)
                s_game->onStopCoreManager();
            break;

        case APP_CMD_DESTROY:
            INFO_LOG("mini-mbm: APP_CMD_DESTROY");
            s_running = false;
            break;

        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// android_main — NativeActivity entry point (called by android_native_app_glue).
// ---------------------------------------------------------------------------
void android_main(struct android_app* app)
{
    // Reset all file-scope statics so a second launch into the same process
    // (Android keeps the process alive after ANativeActivity_finish) starts
    // from a clean state.
    s_game        = nullptr;
    s_running     = false;
    s_windowReady = false;
    s_isRestoring = false;
    s_touchMap.clear();
    s_nextTouchID = 0;

    app->onAppCmd     = onAppCmd;
    app->onInputEvent = onInputEvent;

    INFO_LOG("mini-mbm: android_main started");

    while (true)
    {
        int                         ident;
        int                         events;
        struct android_poll_source* source;

        // Poll for events. When rendering, poll without blocking (0);
        // when idle, block until an event arrives (-1).
        while ((ident = ALooper_pollOnce(
                    (s_running && s_windowReady) ? 0 : -1,
                    nullptr, &events,
                    reinterpret_cast<void**>(&source))) >= 0)
        {
            if (source != nullptr)
                source->process(app, source);

            if (app->destroyRequested)
            {
                INFO_LOG("mini-mbm: destroyRequested — cleaning up");
                if (s_game)
                {
                    delete s_game;
                    s_game = nullptr;
                }
                return;
            }
        }

        // Render one frame when the window is ready and the engine is running.
        if (s_game && s_windowReady && s_running)
        {
            if (!s_game->device->run)
            {
                // mbm.quit() was called from Lua or C++.
                INFO_LOG("mini-mbm: device->run is false — exiting");
                ANativeActivity_finish(app->activity);
                s_running = false;
                continue;
            }

            if (s_isRestoring)
            {
                // Advance onLostDevice one step per frame so Android stays
                // responsive and the hourglass / progress screen is presented.
                constexpr bool doSwapBuffers = true;
                const int w = ANativeWindow_getWidth(app->window);
                const int h = ANativeWindow_getHeight(app->window);
                if (s_game->onLostDevice(doSwapBuffers, w, h, 0, 0))
                    s_isRestoring = false; // all steps done, resume normal loop
            }
            else
            {
                constexpr bool singleLoop    = true;
                constexpr bool doSwapBuffers = true;
                s_game->loop(singleLoop, doSwapBuffers);
            }
        }
    }
}

#else
#   error "main-native-activity.cpp is for ANDROID only"
#endif
