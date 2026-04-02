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

#if defined ANDROID

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#include <lua-wrap/manager-lua.h>
#include <core_mbm/device.h>
#include <core_mbm/util-interface.h>
#include <version/version.h>
#include <core_mbm/scene.h>

#if defined USE_OPENGL_ES
    #include <core_mbm/specific-opengl_es.h>
#elif defined USE_DUMMY_BACK_END_ENGINE && defined ANDROID
    #include <core_mbm/specific-dummy.h> // for specific context of dummy engine
#else
    #error "This file is only for OpenGL ES"
#endif

namespace mbm
{
    #if defined USE_DUMMY_BACK_END_ENGINE && defined ANDROID
    // ANDROID_AND_NOT_OPENGL_ES: For different backend engine on Android, implementation here
    LUA_MANAGER::LUA_MANAGER(JNIEnv *env, jobject obj)
    {
    }
    #else

    LUA_MANAGER::LUA_MANAGER(JNIEnv *env, jobject obj)
    {
        mbm::DEVICE* device = mbm::DEVICE::getInstance();
        LUA_MANAGER::pLuaManager = this;
        log_util::setScriptPrintLine(onScriptPrintLine);
        util::setOnAddPathScript(onAddPathScript);
        this->nameApplication = "Mini-mbm " MBM_VERSION " ";
        this->nameApplication += device->getBackendEngineName();
        this->nameApplication += "\n Compiled: " __DATE__;
        this->widthWindow        = 800;
        this->heightWindow       = 600;
        this->maximizedWindow    = false;
        this->fileNameInitialLua = "main.lua";
#if defined _DEBUG
        this->noSplash = true;
#else
        this->noSplash       = false;
#endif
        device->specificContextDevice->jenv = env;
        this->hasValueTextureLogo = false;
        INFO_LOG("%s", this->nameApplication.c_str());
    }
    #endif
};

#endif
