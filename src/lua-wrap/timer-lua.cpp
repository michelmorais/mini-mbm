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

extern "C" 
{
  #include <lua.h>
  #include <lauxlib.h>
  #include <lualib.h>
}

#include <vector>
#include <map>

#include <lua-wrap/timer-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <core_mbm/device.h>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

namespace mbm
{
	extern int lua_error_debug(lua_State *lua, const char *format, ...);

TIMER_CALL_BACK::TIMER_CALL_BACK(const int functionNameCallBackRef, const float afterTimer, const int lua_LUA_NOREF)
    : timerElapsed(afterTimer)
{
    this->lastTimerElapsed   = 0.0f;
    this->ref_CallBackTimer  = functionNameCallBackRef;
    this->isPaused           = false;
    this->times              = 0;
    this->ref_MeAsTableTimer = lua_LUA_NOREF;
    this->killTimer          = false;
    DEVICE *deviceT          = DEVICE::getInstance();
    this->idTimer            = deviceT->addTimer();
    }
    
    TIMER_CALL_BACK::~TIMER_CALL_BACK()= default;
    
    void TIMER_CALL_BACK::unrefAllTableLua(lua_State *lua) // destroy all
    {
        this->unrefTableLua(lua, &this->ref_MeAsTableTimer);
        this->unrefTableLua(lua, &this->ref_CallBackTimer);
    }


    TIMER_CALL_BACK *getTimerFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<TIMER_CALL_BACK **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_TIMER));
        return *ud;
    }

    int onStartTimer(lua_State *lua)
    {
        TIMER_CALL_BACK *timer = getTimerFromRawTable(lua, 1, 1);
        timer->isPaused        = false;
        return 0;
    }

    int onStopTimer(lua_State *lua)
    {
        TIMER_CALL_BACK *timer  = getTimerFromRawTable(lua, 1, 1);
        DEVICE *    device = DEVICE::getInstance();
        timer->isPaused         = true;
        timer->lastTimerElapsed = 0.0f;
        timer->times            = 0;
        device->setTimer(timer->idTimer, 0.0f);
        return 0;
    }

    int onRestartTimer(lua_State *lua)
    {
        TIMER_CALL_BACK *timer  = getTimerFromRawTable(lua, 1, 1);
        DEVICE *    device = DEVICE::getInstance();
        timer->isPaused         = false;
        timer->lastTimerElapsed = 0.0f;
        timer->times            = 0;
        device->setTimer(timer->idTimer, 0.0f);
        return 0;
    }

    int onPauseTimer(lua_State *lua)
    {
        TIMER_CALL_BACK *timer  = getTimerFromRawTable(lua, 1, 1);
        DEVICE *    device = DEVICE::getInstance();
        timer->isPaused         = true;
        timer->lastTimerElapsed = device->getTimer(timer->idTimer);
        return 0;
    }

    int onSetTimer(lua_State *lua)
    {
        TIMER_CALL_BACK *timer        = getTimerFromRawTable(lua, 1, 1);
        const float      timerElapsed = luaL_checknumber(lua, 2);
        timer->timerElapsed           = timerElapsed;
        return 0;
    }

    int onGetTimer(lua_State *lua)
    {
        TIMER_CALL_BACK *timer = getTimerFromRawTable(lua, 1, 1);
        lua_pushnumber(lua, timer->timerElapsed);
        return 1;
    }

    int onGetElapsedTimer(lua_State *lua)
    {
        TIMER_CALL_BACK *timer  = getTimerFromRawTable(lua, 1, 1);
        DEVICE *    device = DEVICE::getInstance();
        lua_pushnumber(lua, device->getTimer(timer->idTimer));
        return 1;
    }

    int onGetTimesTimer(lua_State *lua)
    {
        TIMER_CALL_BACK *timer = getTimerFromRawTable(lua, 1, 1);
        lua_pushinteger(lua, timer->times);
        return 1;
    }

    int onIsRunnerTimer(lua_State *lua)
    {
        TIMER_CALL_BACK *timer = getTimerFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, timer->isPaused ? 0 : 1);
        return 1;
    }

    int onSetCallBackTimer(lua_State *lua)
    {
        TIMER_CALL_BACK *timer = getTimerFromRawTable(lua, 1, 1);
        timer->refFunctionLua(lua, 2, &timer->ref_CallBackTimer);
        return 0;
    }

    int onDestroyTimerLua(lua_State *lua)
    {
        TIMER_CALL_BACK *         timer     = getTimerFromRawTable(lua, 1, 1);
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        timer->unrefAllTableLua(lua);
        userScene->remove(timer);
        timer->isPaused         = true;
        timer->lastTimerElapsed = 0.0f;
        timer->killTimer        = true;
        return 0;
    }

    int onGcTimerCallBack(lua_State *lua)
    {
        TIMER_CALL_BACK *         timer     = getTimerFromRawTable(lua, 1, 1);
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        timer->unrefAllTableLua(lua);
        userScene->remove(timer);
        timer->isPaused         = true;
        timer->lastTimerElapsed = 0.0f;
        timer->killTimer        = true;
        delete timer;
    #if DEBUG_FREE_LUA
        static int num = 1;
        PRINT_IF_DEBUG("free [timer] [%d]\n", num++);
    #endif

        return 0;
    }

    int onNewTimerCallBack(lua_State *lua)
    {
        const int top   = lua_gettop(lua);
        const int tType = lua_type(lua, 2);
        if (tType != LUA_TSTRING && tType != LUA_TFUNCTION && tType != LUA_TNIL)
            return lua_error_debug(lua, "expected [string 'name of function' or function]");
        const float timer       = (top == 3) ? luaL_checknumber(lua, 3) : 1.0f;
        int         refCallBack = LUA_NOREF;
        if (tType == LUA_TSTRING)
        {
            const char *functionName = lua_tostring(lua, 2);
            lua_getglobal(lua, functionName);
            refCallBack = luaL_ref(lua, LUA_REGISTRYINDEX);
			if(refCallBack == LUA_REFNIL)
				return lua_error_debug(lua, "expected [string 'name of function' or function], got nil\n did you define the function?");
        }
        else if (tType == LUA_TFUNCTION)
        {
            lua_pushvalue(lua, 2);
            refCallBack = luaL_ref(lua, LUA_REGISTRYINDEX);
			if(refCallBack == LUA_REFNIL)
				return lua_error_debug(lua, "expected [string 'name of function' or function], got nil\n did you define the function?");
        }
        lua_settop(lua, 0);
        luaL_Reg regVec3Methods[] = {{"start", onStartTimer},
                                     {"stop", onStopTimer},
                                     {"restart", onRestartTimer},
                                     {"pause", onPauseTimer},
                                     {"isRunning", onIsRunnerTimer},
                                     {"set", onSetTimer},
                                     {"get", onGetTimer},
                                     {"elapsed", onGetElapsedTimer},
                                     {"times", onGetTimesTimer},
                                     {"setCallBack", onSetCallBackTimer},
                                     {"destroy", onDestroyTimerLua},
                                     {nullptr, nullptr}};
        DEVICE *device = DEVICE::getInstance();
        luaL_newlib(lua, regVec3Methods);
        luaL_getmetatable(lua, "_mbmTimerCallBack");
        lua_setmetatable(lua, -2);
        auto **udata         = static_cast<TIMER_CALL_BACK **>(lua_newuserdata(lua, sizeof(TIMER_CALL_BACK *)));
        auto  timerCallBack = new TIMER_CALL_BACK(refCallBack, timer >= 0.0f ? timer : 1.0f, LUA_NOREF);
        *udata                          = timerCallBack;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_TIMER);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->lsTimerCallBack.push_back(timerCallBack);
        lua_pushvalue(lua, 1);
        if (timerCallBack->ref_MeAsTableTimer != LUA_NOREF)
        {
            luaL_unref(lua, LUA_REGISTRYINDEX, timerCallBack->ref_MeAsTableTimer);
            timerCallBack->ref_MeAsTableTimer = LUA_NOREF;
        }
        timerCallBack->ref_MeAsTableTimer = luaL_ref(lua, LUA_REGISTRYINDEX);
        return 1;
    }

    void registerClassCallBackTimer(lua_State *lua)
    {
        luaL_Reg regTimerMethods[] = {{"new", onNewTimerCallBack}, {"__gc", onGcTimerCallBack}, {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmTimerCallBack");
        luaL_setfuncs(lua, regTimerMethods, 0);
        lua_setglobal(lua, "timer");
        lua_settop(lua,0);
    }

};
