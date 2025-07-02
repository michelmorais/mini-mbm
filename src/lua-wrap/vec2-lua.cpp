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

#include <lua-wrap/vec2-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <core_mbm/primitives.h>
#include <core_mbm/device.h>
#include <cmath>
#include <cstring>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

namespace mbm
{
	extern int lua_error_debug(lua_State *lua, const char *format, ...);

    VEC2 *getVec2FromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<VEC2 **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_VEC2));
        return *ud;
    }

    int onSetVec2(lua_State *lua)
    {
        const int top  = lua_gettop(lua);
        VEC2 *    vec2 = getVec2FromRawTable(lua, 1, 1);
        switch (top)
        {
            case 2:
            {
                switch (lua_type(lua, 2))
                {
                    case LUA_TNUMBER: 
                    { 
                        vec2->x = luaL_checknumber(lua, 2);
                    }
                    break;
                    case LUA_TTABLE:
                    {
                        VEC2 *vec2a = getVec2FromRawTable(lua, 1, 2);
                        vec2->x     = vec2a->x;
                        vec2->y     = vec2a->y;
                    }
                    break;
                    default: 
                    { 
                        return lua_error_debug(lua, "\nexpected: vec2:set(vec2a) or vec2:set(x,y)");
                    }
                }
            }
            break;
            case 3:
            {
                vec2->x = luaL_checknumber(lua, 2);
                vec2->y = luaL_checknumber(lua, 3);
            }
            break;
            default: { return lua_error_debug(lua, "\nexpected: vec2:set(vec2a) or vec2:set(x,y)");}
        }
        return 0;
    }

    int onGetVec2(lua_State *lua)
    {
        VEC2 *vec2 = getVec2FromRawTable(lua, 1, 1);
        lua_pushnumber(lua, vec2->x);
        lua_pushnumber(lua, vec2->y);
        return 2;
    }

    int onLengthVec2(lua_State *lua)
    {
        VEC2 *vec2 = getVec2FromRawTable(lua, 1, 1);
        lua_pushnumber(lua, vec2->length());
        return 1;
    }

    int onLerpVec2(
        lua_State *lua) // This function performs the linear interpolation based on the following formula: V1 + s(V2-V1).
    {
        const int top = lua_gettop(lua);
        /*
        expected
        **********************************
                Estado da pilha
                -4|    table |1
                -3|    table |2
                -2|    table |3
                -1|    number|4
        **********************************
        */
        if (top == 4)
        {
            VEC2 *      vec2Out = getVec2FromRawTable(lua, 1, 1);
            VEC2 *      vec2a   = getVec2FromRawTable(lua, 1, 2);
            VEC2 *      vec2b   = getVec2FromRawTable(lua, 1, 3);
            const float s       = luaL_checknumber(lua, 4);
            vec2Out->x          = vec2a->x + s * (vec2b->x - vec2a->x);
            vec2Out->y          = vec2a->y + s * (vec2b->y - vec2a->y);
        }
        else
        {
            return lua_error_debug(lua, "expected: vec2:lerp(vec2a,vec2b,number)");
        }
        return 0;
    }

    int onMulVec2(lua_State *lua)
    {
        const int top  = lua_gettop(lua);
        VEC2 *    vec2 = getVec2FromRawTable(lua, 1, 1);
        switch (top)
        {
            case 2:
            {
                const int t = lua_type(lua, 2);
                switch (t)
                {
                    case LUA_TTABLE:
                    {
                        VEC2 *vec2a = getVec2FromRawTable(lua, 1, 2);
                        vec2->x     = vec2->x * vec2a->x;
                        vec2->y     = vec2->y * vec2a->y;
                    }
                    break;
                    case LUA_TNUMBER:
                    {
                        const float n = lua_tonumber(lua, 2);
                        vec2->x       = vec2->x * n;
                        vec2->y       = vec2->y * n;
                    }
                    break;
                    default: { return lua_error_debug(lua, "\nexpected: vec2:mul(vec2a), vec2:mul(x,y) or vec2:mul(num)");}
                }
            }
            break;
            case 3:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                vec2->x       = x * vec2->x;
                vec2->y       = y * vec2->y;
            }
            break;
            default: { return lua_error_debug(lua, "\nexpected: vec2:mul(vec2a), vec2:mul(x,y) or vec2:mul(num)");}
        }
        return 0;
    }

    int onAddVec2(lua_State *lua)
    {
        const int top  = lua_gettop(lua);
        VEC2 *    vec2 = getVec2FromRawTable(lua, 1, 1);
        switch (top)
        {
            case 2:
            {
                const int t = lua_type(lua, 2);
                switch (t)
                {
                    case LUA_TTABLE:
                    {
                        VEC2 *vec2a = getVec2FromRawTable(lua, 1, 2);
                        vec2->x     = vec2->x + vec2a->x;
                        vec2->y     = vec2->y + vec2a->y;
                    }
                    break;
                    case LUA_TNUMBER:
                    {
                        const float n = lua_tonumber(lua, 2);
                        vec2->x       = vec2->x + n;
                        vec2->y       = vec2->y + n;
                    }
                    break;
                    default: { return lua_error_debug(lua, "\nexpected: vec2:add(vec2a), vec2:add(x,y) or vec2:add(num)");}
                }
            }
            break;
            case 3:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                vec2->x       = x + vec2->x;
                vec2->y       = y + vec2->y;
            }
            break;
            default: { return lua_error_debug(lua, "\nexpected: vec2:add(vec2a), vec2:add(x,y) or vec2:add(num)");}
        }
        return 0;
    }

    int onDivVec2(lua_State *lua)
    {
        const int top  = lua_gettop(lua);
        VEC2 *    vec2 = getVec2FromRawTable(lua, 1, 1);
        switch (top)
        {
            case 2:
            {
                const int t = lua_type(lua, 2);
                switch (t)
                {
                    case LUA_TTABLE:
                    {
                        VEC2 *vec2b = getVec2FromRawTable(lua, 1, 2);
                        if(vec2b->x != 0.0f) //-V550
                            vec2->x     = vec2->x / vec2b->x;
                        else
                            vec2->x     = 0.0f;
                        if(vec2b->y != 0.0f) //-V550
                            vec2->y     = vec2->y / vec2b->y;
                        else
                            vec2->y     = 0.0f;
                    }
                    break;
                    case LUA_TNUMBER:
                    {
                        const float n = lua_tonumber(lua, 2);
                        if (n != 0.0f) //-V550
                        {
                            vec2->x = vec2->x / n;
                            vec2->y = vec2->y / n;
                        }
                        else
                        {
                            vec2->x = 0.0f;
                            vec2->y = 0.0f;
                        }
                    }
                    break;
                    default: { return lua_error_debug(lua, "\nexpected: vec2:div(vec2a), vec2:div(x,y) or vec2:div(num)");}
                }
            }
            break;
            case 3:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                if(x != 0.0f) //-V550
                    vec2->x       = vec2->x / x;
                else
                    vec2->x = 0.0f;
                if(y != 0.0f) //-V550
                    vec2->y       = vec2->y / y;
                else
                    vec2->y = 0.0f;
            }
            break;
            default: { return lua_error_debug(lua, "\nexpected: vec2:div(vec2a), vec2:div(x,y) or vec2:div(num)");}
        }
        return 0;
    }

    int onSubVec2(lua_State *lua)
    {
        const int top  = lua_gettop(lua);
        VEC2 *    vec2 = getVec2FromRawTable(lua, 1, 1);
        switch (top)
        {
            case 2:
            {
                const int t = lua_type(lua, 2);
                switch (t)
                {
                    case LUA_TTABLE:
                    {
                        VEC2 *vec2a = getVec2FromRawTable(lua, 1, 2);
                        vec2->x     = vec2->x - vec2a->x;
                        vec2->y     = vec2->y - vec2a->y;
                    }
                    break;
                    case LUA_TNUMBER:
                    {
                        const float n = lua_tonumber(lua, 2);
                        vec2->x       = vec2->x - n;
                        vec2->y       = vec2->y - n;
                    }
                    break;
                    default: { return lua_error_debug(lua, "\nexpected: vec2:sub(vec2a), vec2:sub(x,y) or vec2:sub(num)");}
                }
            }
            break;
            case 3:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                vec2->x       = vec2->x - x;
                vec2->y       = vec2->y - y;
            }
            break;
            default: { return lua_error_debug(lua, "\nexpected: vec2:sub(vec2a), vec2:sub(x,y) or vec2:sub(num)");}
        }
        return 0;
    }

    int onDestroyVec2Lua(lua_State *lua)
    {
        VEC2 *vec2 = getVec2FromRawTable(lua, 1, 1);
    #if DEBUG_FREE_LUA
        static int v = 1;
        PRINT_IF_DEBUG("destroying vector %d X: %f  Y: %f\n", v++, vec2->x, vec2->y);
    #endif
        delete vec2;
        return 0;
    }

    int onNewIndexVec2(lua_State *lua) // escrita ok 03/10/2013
    {
        /*
        **********************************
                Estado da pilha
                -3|    table |1
                -2|   string |2
                -1|   number |3
        **********************************
        */
        const char *what = luaL_checkstring(lua, 2);
        VEC2 *      vec2 = getVec2FromRawTable(lua, 1, 1);
        const int   len  = strlen(what);
        if (len == 1)
        {
            switch (what[0])
            {
                case 'x': vec2->x = luaL_checknumber(lua, 3); break;
                case 'y': vec2->y = luaL_checknumber(lua, 3); break;
                default: { return lua_error_debug(lua, "\nproperty %s not found!!!\n", what);}
            }
        }
        else
        {
            return lua_error_debug(lua, "\nproperty %s not found!!!\n", what);
        }
        return 0;
    }

    int onIndexVec2(lua_State *lua) // leitura ok 03/10/2013
    {
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1|   string |2
        **********************************
        */
        const char *what = luaL_checkstring(lua, 2);
        VEC2 *      vec2 = getVec2FromRawTable(lua, 1, 1);
        const int   len  = strlen(what);
        if (len == 1)
        {
            switch (what[0])
            {
                case 'x': lua_pushnumber(lua, vec2->x); break;
                case 'y': lua_pushnumber(lua, vec2->y); break;
                default: { lua_pushnil(lua);
                }
                break;
            }
        }
        else if (strcmp(what, "len") == 0)
        {
            const float ret = std::sqrt((vec2->x * vec2->x) + (vec2->y * vec2->y));
            lua_pushnumber(lua, ret);
        }
        else
        {
            lua_pushnil(lua);
        }
        return 1;
    }

    int onMoveVec2(lua_State *lua)
    {
        VEC2 *    vec2 = getVec2FromRawTable(lua, 1, 1);
        const int top  = lua_gettop(lua);
        switch (top)
        {
            case 2:
            {
                DEVICE *device = DEVICE::getInstance();
                const float  x      = luaL_checknumber(lua, 2);
                vec2->x += (device->delta * x);
            }
            break;
            case 3:
            {
                DEVICE *device = DEVICE::getInstance();
                const float  x      = luaL_checknumber(lua, 2);
                const float  y      = luaL_checknumber(lua, 3);
                vec2->x += (device->delta * x);
                vec2->y += (device->delta * y);
            }
            break;
            default: {
            }
            break;
        }
        return 0;
    }

    int onDotVec2(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        /*
        expected
        **********************************
                Estado da pilha
                -2|    table |1
                -1|    table |2
        **********************************

        ou
        **********************************
                Estado da pilha
                -3|    table |1
                -2|    table |2
                -1|    table |3
        **********************************
        */
        switch (top)
        {
            case 2:
            {
                VEC2 *vec2a = getVec2FromRawTable(lua, 1, 1);
                VEC2 *vec2b = getVec2FromRawTable(lua, 1, 2);
                lua_pushnumber(lua, vec2Dot(vec2a, vec2b));
                return 1;
            }
            case 3:
            {
                VEC2 *vec2a = getVec2FromRawTable(lua, 1, 2);
                VEC2 *vec2b = getVec2FromRawTable(lua, 1, 3);
                lua_pushnumber(lua, vec2Dot(vec2a, vec2b));
                return 1;
            }
            default: { return lua_error_debug(lua, "expected: vec2:dot(vec2a,vec2b) or vec2:dot(vec2b)");}
        }
    }

    int onNormalizeVec2(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        /*
        expected
        **********************************
                Estado da pilha
                -2|    table |1
                -1|    table |2
        **********************************

        or
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
        switch (top)
        {
            case 1: 
            { 
                VEC2 *vec2 = getVec2FromRawTable(lua, 1, 1);
                vec2Normalize(vec2, vec2);
            }
            break;
            case 2:
            {
                VEC2 *vec2    = getVec2FromRawTable(lua, 1, 1);
                VEC2 *vec2Out = getVec2FromRawTable(lua, 1, 2);
                vec2Normalize(vec2Out, vec2);
            }
            break;
            default: { return lua_error_debug(lua, "\nexpected: vec2:normalize(out) or vec2:normalize()");}
        }
        return 0;
    }

    
    int onAzimuthVec2(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        switch (top)
        {
            case 1: 
            { 
                VEC2 *vec2 = getVec2FromRawTable(lua, 1, 1);
                const float azimuth = mbm::calcAzimuth(vec2->x,vec2->y);
                lua_pushnumber(lua,azimuth);
            }
            break;
            case 2:
            {
                VEC2 * va  = getVec2FromRawTable(lua, 1, 1);
                VEC2 * vb  = getVec2FromRawTable(lua, 1, 2);
                VEC2 direction(*va);
                direction.x -= vb->x;
                direction.y -= vb->y;
                const float azimuth = mbm::calcAzimuth(direction.x,direction.y);
                lua_pushnumber(lua,azimuth);
            }
            break;
            default: { return lua_error_debug(lua, "\nexpected: vec2:azimuth(in_vec) or vec2:azimuth()");}
        }
        return 1;
    }

    int onNewVec2Lua(lua_State *lua) // ok 03/10/2013
    {
        /*
        //expected
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        ou
        **********************************
                Estado da pilha
                -4|    table |1
                -3|   number |2
                -2|   number |3
                -1|   number |4
        **********************************
        ou
        **********************************
                Estado da pilha
                -2|    table |1
                -1|    table |2
        **********************************

        */
        const int   top      = lua_gettop(lua);
        const VEC2 *vec2Copy = (top == 2 && lua_type(lua, 2) == LUA_TTABLE) ? getVec2FromRawTable(lua, 1, 2) : nullptr;
        const float x        = vec2Copy ? vec2Copy->x : (top > 1 ? luaL_checknumber(lua, 2) : 0.0f);
        const float y        = vec2Copy ? vec2Copy->y : (top > 2 ? luaL_checknumber(lua, 3) : 0.0f);
        lua_settop(lua, 0);
        /*
        **********************************
                Estado da pilha
        **********************************
        */
        luaL_Reg regVec2Methods[] = {{"mul", onMulVec2},
                                     {"sub", onSubVec2},
                                     {"add", onAddVec2},
                                     {"div", onDivVec2},
                                     {"length", onLengthVec2},
                                     {"lerp", onLerpVec2},
                                     {"set", onSetVec2},
                                     {"get", onGetVec2},
                                     {"move", onMoveVec2},
                                     {"dot", onDotVec2},
                                     {"normalize", onNormalizeVec2},
                                     {"azimuth", onAzimuthVec2},
                                     {nullptr, nullptr}};
        luaL_newlib(lua, regVec2Methods);
        /*
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
        luaL_getmetatable(lua, "_mbmVec2");
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1|    table |2
        **********************************
        */
        lua_setmetatable(lua, -2);
        /*
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
        auto **udata = static_cast<VEC2 **>(lua_newuserdata(lua, sizeof(VEC2 *)));
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1| userdata |2
        **********************************
        */
        auto vec2 = new VEC2(x, y);
        *udata     = vec2;
        
        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_ = getUserTypeAsString(L_USER_TYPE_VEC2);
        luaL_getmetatable(lua,__userdata_);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);

        return 1;
    }

    int onNewVec2LuaNoGC(lua_State *lua, VEC2 *vec2)
    {
        /*
        //expected
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
        lua_settop(lua, 0);
        /*
        **********************************
                Estado da pilha
        **********************************
        */
        luaL_Reg regVec2Methods[] = {{"mul", onMulVec2},
                                     {"sub", onSubVec2},
                                     {"add", onAddVec2},
                                     {"div", onDivVec2},
                                     {"length", onLengthVec2},
                                     {"lerp", onLerpVec2},
                                     {"set", onSetVec2},
                                     {"get", onGetVec2},
                                     {"move", onMoveVec2},
                                     {"dot", onDotVec2},
                                     {"normalize", onNormalizeVec2},
                                     {"azimuth", onAzimuthVec2},
                                     {nullptr, nullptr}};
        luaL_newlib(lua, regVec2Methods);
        /*
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
        luaL_getmetatable(lua, "_mbmVec2NoGC");
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1|    table |2
        **********************************
        */
        lua_setmetatable(lua, -2);
        /*
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
        auto **udata = static_cast<VEC2 **>(lua_newuserdata(lua, sizeof(VEC2 *)));
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1| userdata |2
        **********************************
        */
        *udata = vec2;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_ = getUserTypeAsString(L_USER_TYPE_VEC2);
        luaL_getmetatable(lua,__userdata_);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        
        return 1;
    }

    int onNewVec2LuaNoGC(lua_State *lua, VEC3 *vec3)
    {
        auto *v    = static_cast<void*>(vec3);
        auto *vec2 = static_cast<VEC2 *>(v);
        return onNewVec2LuaNoGC(lua, vec2);
    }

    int onToStringVec2(lua_State *lua)
    {
        VEC2 *vec2 = getVec2FromRawTable(lua, 1, 1);
        lua_pushfstring(lua, "x:%f y:%f", vec2->x, vec2->y);
        return 1;
    }

    void registerClassVec2(lua_State *lua)
    {
        luaL_Reg regVec2MMethods[] = {{"new", onNewVec2Lua},      {"__newindex", onNewIndexVec2},
                                      {"__index", onIndexVec2},   {"__tostring", onToStringVec2},
                                      {"__gc", onDestroyVec2Lua}, {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmVec2");
        /*
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
        luaL_setfuncs(lua, regVec2MMethods, 0);
        lua_setglobal(lua, "vec2");
        /*
        **********************************
                Estado da pilha
        **********************************
        */
    }

    void registerClassVec2NoGc(lua_State *lua) // sem GC pois n�o � criado e sim apontado para um vetor existente
    {
        luaL_Reg regVec2MMethods[] = {
            {"__newindex", onNewIndexVec2}, {"__index", onIndexVec2}, {"__tostring", onToStringVec2}, {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmVec2NoGC");
        /*
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
        luaL_setfuncs(lua, regVec2MMethods, 0);
        lua_settop(lua,0);
        /*
        **********************************
                Estado da pilha
        **********************************
        */
    }

};
