/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2015      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#ifndef USERDATA_LUA_H
#define USERDATA_LUA_H

#include <string>
#include <vector>
#include <map>
#include <core-exports.h>


extern "C" 
{
    #include <lua.h>
}

namespace mbm
{
struct USER_DATA_AUDIO_LUA;
struct DYNAMIC_VAR;
class TIMER_CALL_BACK;
class RENDERIZABLE;

    struct USER_DATA_SCENE_LUA
    {
        lua_State *                        lua;
        lua_CFunction                      oldPanicFunction;
        std::vector<USER_DATA_AUDIO_LUA *> lsLuaCallBackStream; // lista de objetos callBack (LUA) (stream)
        std::vector<RENDERIZABLE *>        lsLuaCallBackOnTouchAsynchronous; // lista de objetos callBack (LUA) (onTouch) - RENDERIZABLE
        std::vector<RENDERIZABLE *>        lsLuaCallBackOnTouchSynchronous; // sincrono - lista de objetos callBack (LUA) (onTouch) - RENDERIZABLE
        std::vector<TIMER_CALL_BACK *>     lsTimerCallBack;                 // CallBack para timers Lua
        std::map<std::string, DYNAMIC_VAR *> _lsDynamicVarCam2d;
        std::map<std::string, DYNAMIC_VAR *> _lsDynamicVarCam3d;
        API_IMPL USER_DATA_SCENE_LUA();
        API_IMPL virtual ~USER_DATA_SCENE_LUA();
        API_IMPL void remove(TIMER_CALL_BACK *obj);
        API_IMPL void remove(RENDERIZABLE *obj);
    };

    struct REF_FUNCTION_LUA
    {
        API_IMPL constexpr REF_FUNCTION_LUA(){}
        API_IMPL virtual ~REF_FUNCTION_LUA();
        API_IMPL void refFunctionLua(lua_State *lua, const int index, int *ref_MeAsTable);
        API_IMPL void refTableLua(lua_State *lua, const int index, int *ref_MeAsTable);
        API_IMPL void unrefTableLua(lua_State *lua, int *ref_MeAsTable);
        API_IMPL virtual void unrefAllTableLua(lua_State *lua) = 0; // destroy all
    };

    struct USER_DATA_RENDER_LUA : public REF_FUNCTION_LUA
    {
        float x, y, z; // where touch down
        int   key;
        int   ref_MeAsTable; // me as lua script
        int   ref_CallBackAnimation;
        int   ref_CallBackEffectShader;
        int   ref_CallBackTouchDown;
        void *extra;
        API_IMPL USER_DATA_RENDER_LUA();
        API_IMPL virtual ~USER_DATA_RENDER_LUA();
        API_IMPL virtual void unrefAllTableLua(lua_State *lua);
    };

    struct USER_DATA_AUDIO_LUA : public REF_FUNCTION_LUA
    {
        int         ref_MeAsTable; // me as lua script
        int         ref_CallBackStream;
        std::string fileNameStream;
        API_IMPL USER_DATA_AUDIO_LUA();
        API_IMPL virtual ~USER_DATA_AUDIO_LUA();
        API_IMPL void unrefAllTableLua(lua_State *lua);
    };

	struct USER_DATA_SHAPE_LUA : public USER_DATA_RENDER_LUA
    {
        int         ref_CallBackEditVertexBuffer;
        API_IMPL USER_DATA_SHAPE_LUA();
        API_IMPL virtual ~USER_DATA_SHAPE_LUA();
		API_IMPL virtual void unrefAllTableLua(lua_State *lua);
    };

};

#endif

