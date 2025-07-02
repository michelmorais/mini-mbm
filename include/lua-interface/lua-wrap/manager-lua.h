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

#ifndef MANAGER_LUA_H
#define MANAGER_LUA_H

#include <string>
#include <vector>
#include <core_mbm/scene.h>
#include <core_mbm/primitives.h>
#include <core_mbm/core-manager.h>
#include <lua-wrap/user-data-lua.h>

struct lua_State;

#ifndef _DO_NATIVE_COMMANDS_FROM_LUA
#define _DO_NATIVE_COMMANDS_FROM_LUA
typedef void (*OnDoNativeCommand)(const char *command, const char *param,const char * result,const int size_result);
#endif

namespace mbm
{
    class TEXTURE_VIEW;
    const std::vector<std::string> get_globals_lua();

    class SCENE_SCRIPT : public SCENE
    {
      public:
		std::string        scriptLua; 
        std::string        fileNameScriptLuaFinal;
        static bool        logo_was_init;
        bool               wasError;
        bool               __onErrorStop__;
        TEXTURE_VIEW        *textureLogo;
        TEXTURE_VIEW        *textureRestore;
		bool               loadSceneOnFirtLoop;
        const bool         noSplash;
        std::string        strNameSceneLoadOnFirtLoop;
        
        SCENE_SCRIPT(const char *nameFileScriptLua, const bool _noSplash,RENDERIZABLE * previousSplash);
        virtual ~SCENE_SCRIPT();

        void init() override;
        void logic() override;
        void onCallBackCommands(const char *functionNameCallBack,const char *param) override;
        void onRestore(const int percent) override;
        void onTouchDown(int key, float x, float y) override;
        void onTouchUp(int key, float x, float y) override;
        void onTouchMove(int key, float x, float y) override;
        void onTouchZoom(float zoom) override;
        void * get_lua_state() override;//if we are using lua we should be able to retrieve the current state
#if !defined ANDROID
        void onDoubleClick(float x, float y,int key) override;
#endif
        void onFinalizeScene() override;
        void onKeyDown(int key) override;
        void onKeyUp(int key) override;
        void onKeyDownJoystick(int player, int key) override;
        void onKeyUpJoystick(int player, int key) override;
        void onMoveJoystick(int player, float lx, float ly, float rx,float ry) override;
        void onInfoDeviceJoystick(int player, int maxNumberButton, const char *strDeviceName, const char *extraInfo) override;
        void startLoading() override;
		void endLoading() override;
        void onResizeWindow() override;
        const char *getSceneName() noexcept override;
		void setRenderizableLoading(RENDERIZABLE * renderizable);
		lua_State* getLuaState() const;
      private:
		static void loadMainScene(void *pLua_manager, const char *nameMainScene);
		static int onNewScene(lua_State *lua);
		void removePreviousSceneOnUnload();
        bool doFileAsString(const char *newScriptPath);
        bool createSceneLua();
        void removeNullFromList(std::vector<RENDERIZABLE *> &myList);
		bool doLauncher(const std::string& sceneName);
		static int OnGetSplash(lua_State* lua);
        lua_State *         lua;
        USER_DATA_SCENE_LUA dataScene;
		RENDERIZABLE*		splashRenderizable;
        float               time_resize_window;
    };

    class LUA_MANAGER : public CORE_MANAGER
    {
      public:
        std::string                 nameAplication;
        std::string                 fileNameInitialLua;
        std::string                 string_to_execute;
        std::vector<SCENE_SCRIPT *> lsScene;
        uint32_t                    widthWindow;
        uint32_t                    heightWindow;
        uint32_t                    positionXWindow;
        uint32_t                    positionYWindow;
        bool                        maximizedWindow;
        bool                        noSplash;
		bool						noBorder;
        bool						enableResizeWindow;

		bool existScene(const int idScene)override;
    #ifdef ANDROID
        LUA_MANAGER(JNIEnv *env, jobject obj);
        
    #elif defined _WIN32 || defined __linux__
        LUA_MANAGER();
        LUA_MANAGER(const int argc,const char **argv);
        LUA_MANAGER(const std::vector<std::string> & args);
        OnDoNativeCommand onDoNativeCommand;
    #endif
        virtual ~LUA_MANAGER();
        void setExpectedSizeOfWindow(int expectedWidth,int expectedHeight,const char * stretch);
		void getExpectedSizeOfWindow(int & expectedWidth,int & expectedHeight,std::string & stretch);
        bool execute_string(lua_State *lua);
    #if defined ANDROID
        bool initializeSceneLua(int w, int h,int expectedWidth,int expectedHeight);
    #else
        bool initializeSceneLua(const bool border);
    #endif
        int run();
      private:
		bool hasValueTextureLogo;
        void parserArgs(const std::vector<std::string> &argv);
    public:
        static LUA_MANAGER* pLuaManager;
		static std::vector<std::string> globals_lua;//global that should not be erased on mbm.clearGlobals since it is information for the class
		static void onScriptPrintLine();
		static void onAddPathScript(const char * path);
    };

};

#endif