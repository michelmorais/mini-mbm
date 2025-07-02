/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                            |
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

#include <core_mbm/renderizable.h>
#include <core_mbm/class-identifier.h>
#include <lua-wrap/check-user-type-lua.h>



namespace mbm
{
	extern int lua_error_debug(lua_State *lua, const char *format, ...);

    void lua_userdata_register(lua_State *lua,const int value)
    {
        const char* __userdata_ = getUserTypeAsString(value);
        luaL_newmetatable(lua, __userdata_);
        lua_pushinteger(lua,value);
        lua_rawseti(lua,-2,1);
        lua_settop(lua,0);
    }

    void registerClassUsersData(lua_State *lua)
    {
        lua_settop(lua,0);
        for(int i= L_USER_TYPE_BEGIN + 1; i < L_USER_TYPE_END; ++i)
        {
            lua_userdata_register(lua,i);
        }
    }

    inline const char* getTypeMetaTableNameUserData(lua_State *lua,L_USER_TYPE* foundType)
    {
        lua_rawgeti(lua,-1, 1);
        const int p  = lua_tointeger(lua,-1);
        lua_pop(lua, 1);
        *foundType = (L_USER_TYPE)p;
        return getUserTypeAsString(p);
    }


    inline void *lua_check_MT_userData (lua_State *lua, int ud,L_USER_TYPE* foundType) 
    {
        void *p = lua_touserdata(lua, ud);
        if (p != nullptr) 
        {  /* value is a userdata? */
            if (lua_getmetatable(lua, ud)) 
            {  /* does it have a metatable? */
                const char * valid_for = getTypeMetaTableNameUserData(lua,foundType);
                luaL_getmetatable(lua, valid_for);  /* get correct metatable */
                if (!lua_rawequal(lua, -1, -2))  /* not the same? */
                    p = nullptr;  /* value is a userdata with wrong metatable */
                lua_pop(lua, 2);  /* remove both metatables */
                return p;
            }
        }
        return nullptr;  /* value is not a userdata with a metatable */
    }

    void *lua_check_userType (  lua_State *lua,
                                const int rawi, 
                                const int indexTable,
                                const L_USER_TYPE expectedType) 
    {
        L_USER_TYPE foundType = L_USER_TYPE_END;
        const int typeObj = lua_type(lua, indexTable);
        if (typeObj != LUA_TTABLE)
        {
            if(typeObj == LUA_TNONE)
                lua_error_debug(lua, "expected: [%s]. got [nil]",getUserTypeAsString(expectedType));
            else
                lua_error_debug(lua, "expected: [%s]. got [%s]",getUserTypeAsString(expectedType),lua_typename(lua, typeObj));
        }
        lua_rawgeti(lua, indexTable, rawi);

        void * user_type = lua_check_MT_userData(lua,-1,&foundType);
        if(user_type == nullptr)
        {
            if(foundType >  L_USER_TYPE_BEGIN && foundType < L_USER_TYPE_END)
                lua_error_debug(lua, "expected [%s]. got [%s]",getUserTypeAsString(expectedType),getUserTypeAsString(foundType));
            else
                lua_error_debug(lua, "expected [%s]. got [%s]",getUserTypeAsString(expectedType),lua_typename(lua, typeObj));
        }
        lua_pop(lua, 1);//remove userdata from stack
        if(foundType != expectedType)
        {
            if(expectedType == L_USER_TYPE_RENDERIZABLE)
            {
                if(isRenderizableType(foundType))//cast from specific to base class renderizable is ok
                    return user_type;
                lua_error_debug(lua, "expected [%s]. got [%s]",getUserTypeAsString(expectedType),getUserTypeAsString(foundType));
                return nullptr;
            }
            else if(expectedType == L_USER_TYPE_VEC2 && foundType == L_USER_TYPE_VEC3)//cast from vec3 to vec2 is ok
            {
                return user_type;
            }
            else if((expectedType == L_USER_TYPE_VEC2 || expectedType == L_USER_TYPE_VEC3) && isRenderizableType(foundType))//get position from renderizable to vec3 or vec2 is ok
            {
                auto **ud = static_cast<RENDERIZABLE **>(user_type);
                auto *ptr = static_cast<RENDERIZABLE *>(*ud); //-V522
                static VEC3 * p;
                p = &(ptr->position);
                return &p;
            }
            else 
            {
                lua_error_debug(lua, "expected [%s]. got [%s]",getUserTypeAsString(expectedType),getUserTypeAsString(foundType));
                return nullptr;
            }
        }
        return user_type;
    }
}
