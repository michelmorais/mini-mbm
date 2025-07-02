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

#include <lua-wrap/vec3-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <core_mbm/primitives.h>
#include <core_mbm/device.h>
#include <cstring>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

namespace mbm
{
	extern int lua_error_debug(lua_State *lua, const char *format, ...);

    VEC3 *getVec3FromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<VEC3 **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_VEC3));
        return *ud;
    }

    int onDotVec3(lua_State *lua)
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
                VEC3 *vec3a = getVec3FromRawTable(lua, 1, 1);
                VEC3 *vec3b = getVec3FromRawTable(lua, 1, 2);
                lua_pushnumber(lua, vec3Dot(vec3a, vec3b));
                return 1;
            }
            case 3:
            {
                VEC3 *vec3a = getVec3FromRawTable(lua, 1, 2);
                VEC3 *vec3b = getVec3FromRawTable(lua, 1, 3);
                lua_pushnumber(lua, vec3Dot(vec3a, vec3b));
                return 1;
            }
            default: { return lua_error_debug(lua, "expected: vec3:dot(vec3a,vec3b) or vec3:dot(vec3b)");}
        }
    }

    int onLerpVec3(
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
            VEC3 *      vec3Out = getVec3FromRawTable(lua, 1, 1);
            VEC3 *      vec3a   = getVec3FromRawTable(lua, 1, 2);
            VEC3 *      vec3b   = getVec3FromRawTable(lua, 1, 3);
            const float s       = luaL_checknumber(lua, 4);
            vec3Out->x          = vec3a->x + s * (vec3b->x - vec3a->x);
            vec3Out->y          = vec3a->y + s * (vec3b->y - vec3a->y);
            vec3Out->z          = vec3a->z + s * (vec3b->z - vec3a->z);
        }
        else
        {
            return lua_error_debug(lua, "expected: vec3:lerp(vec3a,vec3b,number)");
        }
        return 0;
    }

    int onCrossVec3(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        /*
        expected
        **********************************
                Estado da pilha
                -3|    table |1
                -2|    table |2
                -1|    table |3
        **********************************
        */
        if (top == 3)
        {
            VEC3 *      vec3Out = getVec3FromRawTable(lua, 1, 1);
            VEC3 *      vec3a   = getVec3FromRawTable(lua, 1, 2);
            VEC3 *      vec3b   = getVec3FromRawTable(lua, 1, 3);
            const float x       = vec3a->y * vec3b->z - vec3a->z * vec3b->y;
            const float y       = vec3a->z * vec3b->x - vec3a->x * vec3b->z;
            const float z       = vec3a->x * vec3b->y - vec3a->y * vec3b->x;
            vec3Out->x          = x;
            vec3Out->y          = y;
            vec3Out->z          = z;
        }
        else
        {
            return lua_error_debug(lua, "expected: vec3:corss(vec3a,vecb)");
        }
        return 0;
    }

    int onNormalizeVec3(lua_State *lua)
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
                -1|    table |1
        **********************************
        */
        switch (top)
        {
            case 1: 
            { 
                VEC3 *vec3 = getVec3FromRawTable(lua, 1, 1);
                vec3Normalize(vec3, vec3);
            }
            break;
            case 2:
            {
                VEC3 *vec3    = getVec3FromRawTable(lua, 1, 1);
                VEC3 *vec3Out = getVec3FromRawTable(lua, 1, 2);
                vec3Normalize(vec3Out, vec3);
            }
            break;
            default: { return lua_error_debug(lua, "\nexpected: vec3:normalize(out) or vec3:normalize()");}
        }
        return 0;
    }

    int onLengthVec3(lua_State *lua)
    {
        VEC3 *vec3 = getVec3FromRawTable(lua, 1, 1);
        lua_pushnumber(lua, vec3->length());
        return 1;
    }

    int onSetVec3(lua_State *lua)
    {
        const int top  = lua_gettop(lua);
        VEC3 *    vec3 = getVec3FromRawTable(lua, 1, 1);
        switch (top)
        {
            case 2:
            {
                switch (lua_type(lua, 2))
                {
                    case LUA_TNUMBER: { vec3->x = luaL_checknumber(lua, 2);
                    }
                    break;
                    case LUA_TTABLE:
                    {
                        VEC3 *vec3a = getVec3FromRawTable(lua, 1, 2);
                        vec3->x     = vec3a->x;
                        vec3->y     = vec3a->y;
                        vec3->z     = vec3a->z;
                    }
                    break;
                    default: { return lua_error_debug(lua, "\nexpected: vec3:set(vec3a), vec3:set(x,y,z) or vec3:set(x,y)");}
                }
            }
            break;
            case 3:
            {
                vec3->x = luaL_checknumber(lua, 2);
                vec3->y = luaL_checknumber(lua, 3);
            }
            break;
            case 4:
            {
                vec3->x = luaL_checknumber(lua, 2);
                vec3->y = luaL_checknumber(lua, 3);
                vec3->z = luaL_checknumber(lua, 4);
            }
            break;
            default: { return lua_error_debug(lua, "\nexpected: vec3:set(vec3a), vec3:set(x,y,z) or vec3:set(x,y)");}
        }
        return 0;
    }

    int onGetVec3(lua_State *lua)
    {
        VEC3 *vec3 = getVec3FromRawTable(lua, 1, 1);
        lua_pushnumber(lua, vec3->x);
        lua_pushnumber(lua, vec3->y);
        lua_pushnumber(lua, vec3->z);
        return 3;
    }

    int onMulVec3(lua_State *lua)
    {
        const int top  = lua_gettop(lua);
        VEC3 *    vec3 = getVec3FromRawTable(lua, 1, 1);
        switch (top)
        {
            case 2:
            {
                const int t = lua_type(lua, 2);
                switch (t)
                {
                    case LUA_TTABLE:
                    {
                        VEC3 *vec3a = getVec3FromRawTable(lua, 1, 2);
                        vec3->x     = vec3->x * vec3a->x;
                        vec3->y     = vec3->y * vec3a->y;
                        vec3->z     = vec3->z * vec3a->z;
                    }
                    break;
                    case LUA_TNUMBER:
                    {
                        const float n = lua_tonumber(lua, 2);
                        vec3->x       = vec3->x * n;
                        vec3->y       = vec3->y * n;
                        vec3->z       = vec3->z * n;
                    }
                    break;
                    default:
                    {
                        return lua_error_debug(lua, "\nexpected: vec3:mul(vec3a), vec3:mul(x,y,z), vec3:mul(x,y) or vec3:mul(num)");
                    }
                }
            }
            break;
            case 3:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                vec3->x       = x * vec3->x;
                vec3->y       = y * vec3->y;
            }
            break;
            case 4:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                const float z = luaL_checknumber(lua, 4);
                vec3->x       = x * vec3->x;
                vec3->y       = y * vec3->y;
                vec3->z       = z * vec3->z;
            }
            break;
            default: { return lua_error_debug(lua, "\nexpected: vec3:mul(vec3a), vec3:mul(x,y,z), vec3:mul(x,y) or vec3:mul(num)");}
        }
        return 0;
    }

    int onAddVec3(lua_State *lua)
    {
        const int top  = lua_gettop(lua);
        VEC3 *    vec3 = getVec3FromRawTable(lua, 1, 1);
        switch (top)
        {
            case 2:
            {
                const int t = lua_type(lua, 2);
                switch (t)
                {
                    case LUA_TTABLE:
                    {
                        VEC3 *vec3a = getVec3FromRawTable(lua, 1, 2);
                        vec3->x     = vec3->x + vec3a->x;
                        vec3->y     = vec3->y + vec3a->y;
                        vec3->z     = vec3->z + vec3a->z;
                    }
                    break;
                    case LUA_TNUMBER:
                    {
                        const float n = lua_tonumber(lua, 2);
                        vec3->x       = vec3->x + n;
                        vec3->y       = vec3->y + n;
                        vec3->z       = vec3->z + n;
                    }
                    break;
                    default: { return lua_error_debug(lua, "\nexpected: vec3:add(vec3a), vec3:add(x,y,z), vec3:add(x,y) vec3:add(num)");}
                }
            }
            break;
            case 3: // x e y only
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                vec3->x       = x + vec3->x;
                vec3->y       = y + vec3->y;
            }
            break;
            case 4:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                const float z = luaL_checknumber(lua, 4);
                vec3->x       = x + vec3->x;
                vec3->y       = y + vec3->y;
                vec3->z       = z + vec3->z;
            }
            break;
            default: { return lua_error_debug(lua, "\nexpected: vec3:add(vec3a), vec3:add(x,y,z), vec3:add(x,y) vec3:add(num)");}
        }
        return 0;
    }

    int onDivVec3(lua_State *lua)
    {
        const int top  = lua_gettop(lua);
        VEC3 *    vec3 = getVec3FromRawTable(lua, 1, 1);
        switch (top)
        {
            case 2:
            {
                const int t = lua_type(lua, 2);
                switch (t)
                {
                    case LUA_TTABLE:
                    {
                        VEC3 *vec3b = getVec3FromRawTable(lua, 1, 2);
                        if(vec3b->x != 0.0f) //-V550
                            vec3->x     = vec3->x / vec3b->x;
                        else
                            vec3->x     = 0.0f;
                        if(vec3b->y != 0.0f) //-V550
                            vec3->y     = vec3->y / vec3b->y;
                        else
                            vec3->y     = 0.0f;
                        if(vec3b->z != 0.0f) //-V550
                            vec3->z     = vec3->z / vec3b->z;
                        else
                            vec3->z     = 0.0f;
                    }
                    break;
                    case LUA_TNUMBER:
                    {
                        const float n = lua_tonumber(lua, 2);
                        if (n != 0.0f) //-V550
                        {
                            vec3->x = vec3->x / n;
                            vec3->y = vec3->y / n;
                            vec3->z = vec3->z / n;
                        }
                        else
                        {
                            vec3->x = 0.0f;
                            vec3->y = 0.0f;
                            vec3->z = 0.0f;
                        }
                    }
                    break;
                    default:
                    {
                        return lua_error_debug(lua, "\nexpected: vec3:div(vec3a), vec3:div(x,y,z), vec3:div(x,y) or vec3:div(num)");
                    }
                }
            }
            break;
            case 3: // x e y only
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                if(x != 0.0f) //-V550
                    vec3->x       = vec3->x / x;
                else
                    vec3->x = 0.0f;
                if(y != 0.0f) //-V550
                    vec3->y       = vec3->y / y;
                else
                    vec3->y =   0.0f;
            }
            break;
            case 4:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                const float z = luaL_checknumber(lua, 4);
                if(x != 0.0f) //-V550
                    vec3->x       = vec3->x / x;
                else
                    vec3->x = 0.0f;
                if(y != 0.0f) //-V550
                    vec3->y       = vec3->y / y;
                else
                    vec3->y =   0.0f;
                if(z != 0.0f) //-V550
                    vec3->z       = vec3->z / z;
                else
                    vec3->z = 0.0f;
            }
            break;
            default: { return lua_error_debug(lua, "\nexpected: vec3:div(vec3a), vec3:div(x,y,z), vec3:div(x,y) or vec3:div(num)");}
        }
        return 0;
    }

    int onSubVec3(lua_State *lua)
    {
        const int top  = lua_gettop(lua);
        VEC3 *    vec3 = getVec3FromRawTable(lua, 1, 1);
        switch (top)
        {
            case 2:
            {
                const int t = lua_type(lua, 2);
                switch (t)
                {
                    case LUA_TTABLE:
                    {
                        VEC3 *vec3a = getVec3FromRawTable(lua, 1, 2);
                        vec3->x     = vec3->x - vec3a->x;
                        vec3->y     = vec3->y - vec3a->y;
                        vec3->z     = vec3->z - vec3a->z;
                    }
                    break;
                    case LUA_TNUMBER:
                    {
                        const float n = lua_tonumber(lua, 2);
                        vec3->x       = vec3->x - n;
                        vec3->y       = vec3->y - n;
                        vec3->z       = vec3->z - n;
                    }
                    break;
                    default:
                    {
                        return lua_error_debug(lua, "\nexpected: vec3:sub(vec3a), vec3:sub(x,y,z), vec3:sub(x,y) or vec3:sub(num)");
                    }
                }
            }
            break;
            case 3:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                vec3->x       = vec3->x - x;
                vec3->y       = vec3->y - y;
            }
            break;
            case 4:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                const float z = luaL_checknumber(lua, 4);
                vec3->x       = vec3->x - x;
                vec3->y       = vec3->y - y;
                vec3->z       = vec3->z - z;
            }
            break;
            default: { return lua_error_debug(lua, "\nexpected: vec3:sub(vec3a), vec3:sub(x,y,z), vec3:sub(x,y) or vec3:sub(num)");}
        }
        return 0;
    }

    int onDestroyVec3Lua(lua_State *lua)
    {
        VEC3 *vec3 = getVec3FromRawTable(lua, 1, 1);
    #if DEBUG_FREE_LUA
        static int v = 1;
        printf("destroying vector %d X: %f  Y: %f Z: %f\n", v++, vec3->x, vec3->y, vec3->z);
    #endif
        delete vec3;
        return 0;
    }

    int onNewIndexVec3(lua_State *lua) // escrita ok 03/10/2013
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
        VEC3 *      vec3 = getVec3FromRawTable(lua, 1, 1);
        const int   len  = strlen(what);
        if (len == 1)
        {
            switch (what[0])
            {
                case 'x': vec3->x = luaL_checknumber(lua, 3); break;
                case 'y': vec3->y = luaL_checknumber(lua, 3); break;
                case 'z': vec3->z = luaL_checknumber(lua, 3); break;
                default: { return lua_error_debug(lua, "\nproperty %s not found!!!\n", what);}
            }
        }
        else
        {
            return lua_error_debug(lua, "\nproperty %s not found!!!\n", what);
        }
        return 0;
    }

    int onIndexVec3(lua_State *lua) // leitura ok 03/10/2013
    {
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1|   string |2
        **********************************
        */
        const char *what = luaL_checkstring(lua, 2);
        VEC3 *      vec3 = getVec3FromRawTable(lua, 1, 1);
        const int   len  = strlen(what);
        if (len == 1)
        {
            switch (what[0])
            {
                case 'x': lua_pushnumber(lua, vec3->x); break;
                case 'y': lua_pushnumber(lua, vec3->y); break;
                case 'z': lua_pushnumber(lua, vec3->z); break;
                default: { lua_pushnil(lua);
                }
                break;
            }
        }
        else if (strcmp(what, "len") == 0)
        {
            const float ret = std::sqrt((vec3->x * vec3->x) + (vec3->y * vec3->y) + (vec3->z * vec3->z));
            lua_pushnumber(lua, ret);
        }
        else
        {
            lua_pushnil(lua);
        }
        return 1;
    }

    int onToStringVec3(lua_State *lua)
    {
        VEC3 *vec3 = getVec3FromRawTable(lua, 1, 1);
        lua_pushfstring(lua, "x:%f y:%f z:%f", vec3->x, vec3->y, vec3->z);
        return 1;
    }

    int onMoveVec3(lua_State *lua)
    {
        VEC3 *       vec3   = getVec3FromRawTable(lua, 1, 1);
        const int    top    = lua_gettop(lua);
        DEVICE *device = DEVICE::getInstance();
        switch (top)
        {
            case 2:
            {
                const float x = luaL_checknumber(lua, 2);
                vec3->x += (device->delta * x);
            }
            break;
            case 3:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                vec3->x += (device->delta * x);
                vec3->y += (device->delta * y);
            }
            break;
            case 4:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                const float z = luaL_checknumber(lua, 4);
                vec3->x += (device->delta * x);
                vec3->y += (device->delta * y);
                vec3->z += (device->delta * z);
            }
            break;
            default: {
            }
            break;
        }
        return 0;
    }

    int onNewVec3Lua(lua_State *lua) // ok 03/10/2013
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
        const VEC3 *vec3Copy = (top == 2 && lua_type(lua, 2) == LUA_TTABLE) ? getVec3FromRawTable(lua, 1, 2) : nullptr;
        const float x        = vec3Copy ? vec3Copy->x : (top > 1 ? luaL_checknumber(lua, 2) : 0.0f);
        const float y        = vec3Copy ? vec3Copy->y : (top > 2 ? luaL_checknumber(lua, 3) : 0.0f);
        const float z        = vec3Copy ? vec3Copy->z : (top > 3 ? luaL_checknumber(lua, 4) : 0.0f);
        lua_settop(lua, 0);
        /*
        **********************************
                Estado da pilha
        **********************************
        */
        luaL_Reg regVec3Methods[] = {{"mul", onMulVec3}, {"sub", onSubVec3},       {"add", onAddVec3},
                                     {"div", onDivVec3}, {"length", onLengthVec3}, {"normalize", onNormalizeVec3},
                                     {"dot", onDotVec3}, {"lerp", onLerpVec3},     {"cross", onCrossVec3},
                                     {"set", onSetVec3}, {"get", onGetVec3},       {"move", onMoveVec3},
                                     {nullptr, nullptr}};
        luaL_newlib(lua, regVec3Methods);
        /*
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
        luaL_getmetatable(lua, "_mbmVec3");
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
        auto **udata = static_cast<VEC3 **>(lua_newuserdata(lua, sizeof(VEC3 *)));
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1| userdata |2
        **********************************
        */
        auto vec3 = new VEC3(x, y, z);
        *udata     = vec3;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_ = getUserTypeAsString(L_USER_TYPE_VEC3);
        luaL_getmetatable(lua,__userdata_);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        /*
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
        return 1;
    }

    int onNewVec3LuaNoGC(lua_State *lua, VEC3 *vec3)
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
        luaL_Reg regVec3Methods[] = {{"mul", onMulVec3}, {"sub", onSubVec3},       {"add", onAddVec3},
                                     {"div", onDivVec3}, {"length", onLengthVec3}, {"normalize", onNormalizeVec3},
                                     {"dot", onDotVec3}, {"lerp", onLerpVec3},     {"cross", onCrossVec3},
                                     {"set", onSetVec3}, {"get", onGetVec3},       {"move", onMoveVec3},
                                     {nullptr, nullptr}};
        luaL_newlib(lua, regVec3Methods);
        /*
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
        luaL_getmetatable(lua, "_mbmVec3NoGC");
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
        auto **udata = static_cast<VEC3 **>(lua_newuserdata(lua, sizeof(VEC3 *)));
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1| userdata |2
        **********************************
        */
        *udata = vec3;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_ = getUserTypeAsString(L_USER_TYPE_VEC3);
        luaL_getmetatable(lua,__userdata_);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }

    void registerClassVec3(lua_State *lua)
    {
        luaL_Reg regVec3MMethods[] = {{"new", onNewVec3Lua},      {"__newindex", onNewIndexVec3},
                                      {"__index", onIndexVec3},   {"__tostring", onToStringVec3},
                                      {"__gc", onDestroyVec3Lua}, {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmVec3");
        /*
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
        luaL_setfuncs(lua, regVec3MMethods, 0);
        lua_setglobal(lua, "vec3");
        /*
        **********************************
                Estado da pilha
        **********************************
        */
    }

    void registerClassVec3NoGc(lua_State *lua) // sem GC pois n�o � criado e sim apontado para um vetor existente
    {
        luaL_Reg regVec3MMethods[] = {
            {"__newindex", onNewIndexVec3}, {"__index", onIndexVec3}, {"__tostring", onToStringVec3}, {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmVec3NoGC");
        /*
        **********************************
                Estado da pilha
                -1|    table |1
        **********************************
        */
        luaL_setfuncs(lua, regVec3MMethods, 0);
        lua_settop(lua,0);
        /*
        **********************************
                Estado da pilha
        **********************************
        */
    }
};
