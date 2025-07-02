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

#include <vector>
#include <map>
#include <core_mbm/renderizable.h>
#include <lua-wrap/user-data-lua.h>
#include <core_mbm/dynamic-var.h>

extern "C" 
{
    #include <lauxlib.h>
}

namespace mbm
{
	extern int lua_error_debug(lua_State *lua, const char *format, ...);

    USER_DATA_SCENE_LUA::USER_DATA_SCENE_LUA()
    {
        oldPanicFunction = nullptr;
        lua              = nullptr;
    }
    USER_DATA_SCENE_LUA::~USER_DATA_SCENE_LUA()
    {
        for (const auto & it : _lsDynamicVarCam2d)
        {
            DYNAMIC_VAR *dVar = it.second;
            if (dVar)
                delete dVar;
        }
        _lsDynamicVarCam2d.clear();
        for (const auto & it : _lsDynamicVarCam3d)
        {
            DYNAMIC_VAR *dVar = it.second;
            if (dVar)
                delete dVar;
        }
        _lsDynamicVarCam3d.clear();
    }
    void USER_DATA_SCENE_LUA::remove(TIMER_CALL_BACK *obj)
    {
        const unsigned int s = lsTimerCallBack.size();
        for (unsigned int i = 0; i < s; ++i)
        {
            TIMER_CALL_BACK *obj2remove = lsTimerCallBack[i];
            if (obj2remove == obj)
            {
                lsTimerCallBack.erase(lsTimerCallBack.begin() + i);
                break;
            }
        }
    }
    void USER_DATA_SCENE_LUA::remove(RENDERIZABLE *obj)
    {
        obj->enableRender = false;
        unsigned int s = lsLuaCallBackOnTouchAsynchronous.size();
        for (unsigned int i = 0; i < s; ++i)
        {
            RENDERIZABLE *obj2remove = lsLuaCallBackOnTouchAsynchronous[i];
            if (obj2remove == obj)
            {
                lsLuaCallBackOnTouchAsynchronous.erase(lsLuaCallBackOnTouchAsynchronous.begin() + i);
                break;
            }
        }
        s = lsLuaCallBackOnTouchSynchronous.size();
        for (unsigned int i = 0; i < s; ++i)
        {
            RENDERIZABLE *obj2remove = lsLuaCallBackOnTouchSynchronous[i];
            if (obj2remove == obj)
            {
                lsLuaCallBackOnTouchSynchronous.erase(lsLuaCallBackOnTouchSynchronous.begin() + i);
                break;
            }
        }
    }


    REF_FUNCTION_LUA::~REF_FUNCTION_LUA() = default;

    void REF_FUNCTION_LUA::refFunctionLua(lua_State *lua, const int index, int *ref_MeAsTable)
    {
        if (*ref_MeAsTable != LUA_NOREF)
        {
            luaL_unref(lua, LUA_REGISTRYINDEX, *ref_MeAsTable);
            *ref_MeAsTable = LUA_NOREF;
        }
        const int tType = lua_type(lua, index);
        switch(tType)
        {
            case LUA_TNIL:
            {
                *ref_MeAsTable = LUA_NOREF;
            }
            break;
            case LUA_TSTRING:
            {
                const char *functionName = lua_tostring(lua, index);
                lua_getglobal(lua, functionName);
                *ref_MeAsTable = luaL_ref(lua, LUA_REGISTRYINDEX);
            }
            break;
            case LUA_TFUNCTION:
            {
                lua_pushvalue(lua, index);
                *ref_MeAsTable = luaL_ref(lua, LUA_REGISTRYINDEX);
            }
            break;
            default:
            {
                lua_error_debug(lua, "expected [string 'name of function', function or nil]");
            }   
            break;
        }
    }
    
    void REF_FUNCTION_LUA::refTableLua(lua_State *lua, const int index, int *ref_MeAsTable)
    {
        if (*ref_MeAsTable == LUA_NOREF)
        {
            lua_pushvalue(lua, index);
            *ref_MeAsTable = luaL_ref(lua, LUA_REGISTRYINDEX);
        }
    }
    
    void REF_FUNCTION_LUA::unrefTableLua(lua_State *lua, int *ref_MeAsTable)
    {
        if (*ref_MeAsTable != LUA_NOREF)
        {
            luaL_unref(lua, LUA_REGISTRYINDEX, *ref_MeAsTable);
            *ref_MeAsTable = LUA_NOREF;
        }
    }
    
    USER_DATA_RENDER_LUA::USER_DATA_RENDER_LUA()
    {
        this->key                   = 0;
        this->x                     = 0.0f;
        this->y                     = 0.0f;
        this->z                     = 0.0f;
        this->ref_CallBackAnimation = LUA_NOREF;
        this->ref_CallBackEffectShader = LUA_NOREF;
        this->ref_CallBackTouchDown = LUA_NOREF;
        this->ref_MeAsTable         = LUA_NOREF;
        this->extra                 = nullptr;
    }
    USER_DATA_RENDER_LUA::~USER_DATA_RENDER_LUA() = default;
    void USER_DATA_RENDER_LUA::unrefAllTableLua(lua_State *lua) // destroy all
    {
        this->unrefTableLua(lua, &this->ref_MeAsTable);
        this->unrefTableLua(lua, &this->ref_CallBackAnimation);
        this->unrefTableLua(lua, &this->ref_CallBackEffectShader);
        this->unrefTableLua(lua, &this->ref_CallBackTouchDown);
    }


    USER_DATA_AUDIO_LUA::USER_DATA_AUDIO_LUA()
    {
        this->ref_CallBackStream = LUA_NOREF;
        this->ref_MeAsTable      = LUA_NOREF;
    }
    USER_DATA_AUDIO_LUA::~USER_DATA_AUDIO_LUA() = default;
    void USER_DATA_AUDIO_LUA::unrefAllTableLua(lua_State *lua) // destroy all
    {
        this->unrefTableLua(lua, &this->ref_MeAsTable);
        this->unrefTableLua(lua, &this->ref_CallBackStream);
    }

	USER_DATA_SHAPE_LUA::USER_DATA_SHAPE_LUA():USER_DATA_RENDER_LUA()
	{
		ref_CallBackEditVertexBuffer = LUA_NOREF;
	}

	USER_DATA_SHAPE_LUA::~USER_DATA_SHAPE_LUA() = default;

	void USER_DATA_SHAPE_LUA::unrefAllTableLua(lua_State *lua) // destroy all
    {
        this->unrefTableLua(lua, &this->ref_MeAsTable);
        this->unrefTableLua(lua, &this->ref_CallBackAnimation);
        this->unrefTableLua(lua, &this->ref_CallBackEffectShader);
        this->unrefTableLua(lua, &this->ref_CallBackTouchDown);
		this->unrefTableLua(lua, &this->ref_CallBackEditVertexBuffer);
    }

};
