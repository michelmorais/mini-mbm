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

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#include <lua-wrap/render-table/sprite-lua.h>
#include <lua-wrap/common-methods-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <render/sprite.h>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

namespace mbm
{

    SPRITE *getSpriteFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<SPRITE **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_SPRITE));
        return *ud;
    }

    int onDestroySpriteLua(lua_State *lua)
    {
        SPRITE *              sprite   = getSpriteFromRawTable(lua, 1, 1);
        auto *userData = static_cast<USER_DATA_RENDER_LUA *>(sprite->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        sprite->userData = nullptr;
    #if DEBUG_FREE_LUA
        static int  num      = 1;
        const char *fileName = sprite->getFileName();
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",sprite->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(sprite);
        delete sprite;
        return 0;
    }

	int onDestroyNoGcSpriteLua(lua_State *lua)
    {
        SPRITE *              sprite   = getSpriteFromRawTable(lua, 1, 1);
        auto *userData = static_cast<USER_DATA_RENDER_LUA *>(sprite->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        sprite->userData = nullptr;
    #if DEBUG_FREE_LUA
        static int  num      = 1;
        const char *fileName = sprite->getFileName();
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",sprite->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(sprite);
        return 0;
    }

    int onLoadSpriteLua(lua_State *lua)
    {
        SPRITE *    sprite   = getSpriteFromRawTable(lua, 1, 1);
        const char *fileName = luaL_checkstring(lua, 2);
        if (sprite->getFileName() && strcmp(sprite->getFileName(), fileName) == 0)
        {
            lua_pushboolean(lua, 1);
            return 1;
        }
        else
        {
            sprite->release();
        }
        if (sprite->load(fileName))
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onNewSpriteLua(lua_State *lua)
    {
        const int top                = lua_gettop(lua);
        luaL_Reg  regSpriteMethods[] = {{"load", onLoadSpriteLua}, {nullptr, nullptr}};

        SELF_ADD_COMMON_METHODS selfMethods(regSpriteMethods);
        const luaL_Reg *             regMethods = selfMethods.get();

        VEC3 position(0, 0, 0);
        bool is2dw = true;
        bool is2ds = true;
        bool is3d = false;
        for (int i = 2; i <= top; ++i)
        {
            switch (i)
            {
                case 2:
                {
                    getTypeWordRenderizableLua(lua,i,is2dw,is2ds,is3d);
                }
                break;
                case 3: // x
                {
                    position.x = luaL_checknumber(lua, i);
                }
                break;
                case 4: // y
                {
                    position.y = luaL_checknumber(lua, i);
                }
                break;
                case 5: // z
                {
                    position.z = luaL_checknumber(lua, i);
                }
                break;
                default: {
                }
                break;
            }
        }
        lua_settop(lua, 0);
        lua_createtable(lua, 0, selfMethods.getSize());
        luaL_setfuncs(lua, regMethods, 0);
        luaL_getmetatable(lua, "_mbmSprite");
        lua_setmetatable(lua, -2);
        DEVICE *         device      = DEVICE::getInstance();
        auto **             udata       = static_cast<SPRITE **>(lua_newuserdata(lua, sizeof(SPRITE *)));
        auto               sprite      = new SPRITE(device->scene, is3d, is2ds);
        auto tableLuaMbm = new USER_DATA_RENDER_LUA();
        sprite->userData                  = tableLuaMbm;
        *udata                            = sprite;
        if (position.x != 0.0f) //-V550
            sprite->position.x = position.x;
        if (position.y != 0.0f) //-V550
            sprite->position.y = position.y;
        if (position.z != 0.0f) //-V550
            sprite->position.z = position.z;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_SPRITE);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }

	int onNewSpriteNoGcLua(lua_State *lua,RENDERIZABLE * renderizable)
	{
		lua_settop(lua,0);
		if(renderizable == nullptr || renderizable->userData != nullptr)
			return false;
		
		//table
		luaL_Reg  regSpriteMethods[] = {{"load", onLoadSpriteLua}, {nullptr, nullptr}};

        SELF_ADD_COMMON_METHODS selfMethods(regSpriteMethods);
        const luaL_Reg *             regMethods = selfMethods.get();
        
		lua_createtable(lua, 0, selfMethods.getSize());//the table renderizable
		luaL_setfuncs(lua, regMethods, 0);//Registers all functions in the array l (see luaL_Reg) into the table on the top of the stack 
        

		//Metatable
		luaL_Reg regSpriteMethodsMetaTable[] = {
                                       {"__newindex", onNewIndexRenderizableLua},
                                       {"__index", onIndexRenderizableLua},
                                       {"__gc", onDestroyNoGcSpriteLua},
                                       {"__close", onDestroyRenderizable},
                                       {nullptr, nullptr}};

		lua_newtable(lua);//the metatable from renderizable
		luaL_setfuncs(lua, regSpriteMethodsMetaTable, 0);//Registers all functions in the array l (see luaL_Reg) into the table on the top of the stack 
		lua_setmetatable(lua,-2);
		
		auto ** udata             = static_cast<SPRITE **>(lua_newuserdata(lua, sizeof(SPRITE *)));
        auto sprite               = static_cast<SPRITE*>(renderizable);
		auto user_data            = new USER_DATA_RENDER_LUA();
        renderizable->userData    = user_data;
        *udata                    = sprite;
        
		/* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_SPRITE);
        luaL_getmetatable(lua,__userdata_name);
		lua_setmetatable(lua,-2);//metatable from user data
		/* end trick */

        lua_rawseti(lua, -2, 1);//userdata as raw index in the table renderizable
		
		user_data->refTableLua(lua, 1, &user_data->ref_MeAsTable);//always ref to be able to retrieve 
		return 1;
	}

    void registerClassSprite(lua_State *lua)
    {
        luaL_Reg regSpriteMethods[] = {{"new", onNewSpriteLua},
                                       {"__newindex", onNewIndexRenderizableLua},
                                       {"__index", onIndexRenderizableLua},
                                       {"__gc", onDestroySpriteLua},
                                       {"__close", onDestroyRenderizable},
                                       {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmSprite");
        luaL_setfuncs(lua, regSpriteMethods, 0);
        lua_setglobal(lua, "sprite");
        lua_settop(lua,0);
    }
};
