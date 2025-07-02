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

#ifndef FRAMEWORK_LUA_H
#define FRAMEWORK_LUA_H

#include <vector>
#include <map>
#include <string>

extern "C" 
{
    #include <lua.h>
}

#if defined __linux__ && !defined ANDROID
    #include <pwd.h>
    #include <langinfo.h>
#endif

#if (defined __linux__ || defined _WIN32) && !defined ANDROID
    #include <tinyfiledialogs/tinyfiledialogs.h>
#endif

struct lua_State;

enum TYPE_LOG : char;



namespace mbm
{
    struct VAR_CFG;
    class SHADER_CFG;
    class SCENE;

	extern const std::vector<std::string> get_globals_lua();

    enum TYPE_VAR_PRINT : char
    {
        VAR_PRINT_DEFAULT = 1,
        VAR_PRINT_MIN     = 2,
        VAR_PRINT_MAX     = 3,
    };

    const char *__std_p();
    void showConsoleWindowLua();
    void hideConsoleWindowLua();
    int onGetRealSizeBackBuffer(lua_State *lua);
    int onGetDisplayMetrics(lua_State *lua);
    int onGetSizeBackBuffer(lua_State *lua);
    int onGetFps(lua_State *lua);
    int onSetFps(lua_State *lua);
    int onQuitEngine(lua_State *);
    int onSetColorBackground(lua_State *lua);
    int onShowConsoleMbm(lua_State *lua);
    int onAddPathSourceMbm(lua_State *lua);
    const char *getPathAtLevel(const int level, const char *path, const char *filename);
    int onGetPathSourceMbm(lua_State *lua);
    int onGetFullPath(lua_State *lua);
    int onGetAllPath(lua_State *lua);
    int ontransform2dS2dWMbm(lua_State *lua);
    int ontransform2dW2dSMbm(lua_State *lua);
    int ontransform2dsto3dmbm(lua_State *lua);
    int onGetTotalObjectsRender(lua_State *lua);
    int addOnTouchMeshLua(lua_State *lua);
    int onSetGlobal(lua_State *lua);
    int onGetGlobal(lua_State *lua);
    int onGetAzimute(lua_State *lua);
    int onIs(lua_State *lua);
    int onGet(lua_State *lua);
    int onDoCommands(lua_State *lua);
    int onGetTimeRun(lua_State *lua);
    int onEnableClearBackGround(lua_State *lua);
    int onIncludeFile(lua_State *lua);
    int onPauseGameLua(lua_State *);
    int onResumeGameLua(lua_State *);
    int onCreateTextureLua(lua_State *lua);
    int onExistTextureLua(lua_State *lua);
    int onCompressFile(lua_State *lua);
    int onDecompressFile(lua_State *lua);
    int onExistFile(lua_State *lua);
    int onStopFlag(lua_State *lua);
    int getKeyCode(const char *key);
    const char *getKeyName(const int key);
    int onGetKeyCode(lua_State *lua);
    int onGetKeyName(lua_State *lua);
    int onGetIdiom(lua_State *lua);
    int onGetUserName(lua_State *lua);
    int onClearGlobals(lua_State *);
    int onEncryptFile(lua_State *lua);
    int onDecryptFile(lua_State *lua);
    int onGetSceneName(lua_State *lua);
    int onSaveFile(lua_State *lua);
    int openMultiSingleFile(lua_State *lua, int allowMultipleSelects);
    #if defined ANDROID
    bool onShowMessageBoxAndroid(const char *const title, const char *const message, const char *dialogType);
    #endif
    int onShowMessageBox(lua_State *lua);
    int onOpenFolder(lua_State *lua);
    int onOpenFile(lua_State *lua);
    int onOpenMultiFile(lua_State *lua);
    int onInputDialogBox(lua_State *lua);
    int onInputPasswordBox(lua_State *lua);
    int onColorFromDialogBox(lua_State *lua);
    void fillVarTableShaderList(lua_State *lua, const std::vector<VAR_CFG *> &lsVar, const TYPE_VAR_PRINT typeVarPrint);
    void fillTableShaderList(lua_State *lua, const std::vector<SHADER_CFG *> &lsShader, const bool bMin, const bool bMax,const bool bCode);
    int onGetShaderList(lua_State *lua);
    int onExistShader(lua_State *lua);
    bool fillVarShadersFromMap(std::map<std::string, std::vector<float> *> &lsMapVars, const char *minMaxDefault,std::vector<std::string> &out);
    int onSortShader(lua_State *);
    int onAddShader(lua_State *lua);
    int onPanic(lua_State *lua);
    void registerNamespaceMBM(lua_State *lua, SCENE *scene, lua_CFunction OnNewScene, lua_CFunction OnGetSplash);
	int lua_error_debug(lua_State *lua, const char *format, ...);
	void lua_print_line(lua_State *lua,TYPE_LOG type_log,const char *format, ...);
	
};

#endif
