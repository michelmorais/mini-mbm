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

#ifndef CALL_BACK_TIMER_LUA_H
#define CALL_BACK_TIMER_LUA_H

#include <lua-wrap/user-data-lua.h>

struct lua_State;

namespace mbm
{
    class TIMER_CALL_BACK : public REF_FUNCTION_LUA
    {
      public:
        float        timerElapsed;
        float        lastTimerElapsed;
        uint32_t idTimer;
        int          ref_CallBackTimer;
        bool         isPaused;
        bool         killTimer;
        uint32_t times;
        int          ref_MeAsTableTimer;
        TIMER_CALL_BACK(const int functionNameCallBackRef, const float afterTimer, const int lua_LUA_NOREF);
        virtual ~TIMER_CALL_BACK();
        void unrefAllTableLua(lua_State *lua);
    };
    
    TIMER_CALL_BACK *getTimerFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    int onStartTimer(lua_State *lua);
    int onStopTimer(lua_State *lua);
    int onRestartTimer(lua_State *lua);
    int onPauseTimer(lua_State *lua);
    int onSetTimer(lua_State *lua);
    int onGetTimer(lua_State *lua);
    int onGetElapsedTimer(lua_State *lua);
    int onGetTimesTimer(lua_State *lua);
    int onIsRunnerTimer(lua_State *lua);
    int onSetCallBackTimer(lua_State *lua);
    int onDestroyTimerLua(lua_State *lua);
    int onGcTimerCallBack(lua_State *lua);
    int onNewTimerCallBack(lua_State *lua);

    void registerClassCallBackTimer(lua_State *lua);
};

#endif