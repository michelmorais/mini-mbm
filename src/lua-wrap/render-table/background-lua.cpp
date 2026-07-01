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

#include <plugin-helper/user-data-lua.h>
#include <lua-wrap/common-methods-lua.h>
#include <lua-wrap/render-table/background-lua.h>
#include <render/background.h>
#include <platform/mismatch-platform.h>
#include <core_mbm/scene.h>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

namespace mbm
{

    BACKGROUND *getBackGroundFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<BACKGROUND **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_BACKGROUND));
        return *ud;
    }

    int onDestroyBackGroundLua(lua_State *lua)
    {
        BACKGROUND *          backGround = getBackGroundFromRawTable(lua, 1, 1);
        auto *userData   = static_cast<USER_DATA_RENDER_LUA *>(backGround->getUserData());
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        backGround->setUserData(nullptr);
    #if DEBUG_FREE_LUA
        const char *fileName = backGround->getFileName();
        static int  num      = 1;
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",backGround->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->getScene()->getUserData());
        userScene->remove(backGround);
        delete backGround;
        return 0;
    }

	int onDestroyNoGcBackGroundLua(lua_State *lua)
    {
        BACKGROUND *          backGround = getBackGroundFromRawTable(lua, 1, 1);
        auto *userData   = static_cast<USER_DATA_RENDER_LUA *>(backGround->getUserData());
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        backGround->setUserData(nullptr);
    #if DEBUG_FREE_LUA
        const char *fileName = backGround->getFileName();
        static int  num      = 1;
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",backGround->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->getScene()->getUserData());
        userScene->remove(backGround);
        return 0;
    }

    int onLoadBackGroundLua(lua_State *lua)
    {
        BACKGROUND *backGround = getBackGroundFromRawTable(lua, 1, 1);
        const int   top        = lua_gettop(lua);
        const char *fileName   = luaL_checkstring(lua, 2);
        if (backGround->getFileName() && strcmp(backGround->getFileName(), fileName) == 0)
        {
            lua_pushboolean(lua, 1);
            return 1;
        }
        else
        {
            backGround->release();
        }
        if (top == 3 && lua_type(lua, 3) == LUA_TSTRING)
        {
            const char *text = luaL_checkstring(lua, 3);
            if (backGround->loadFont(fileName, text))
                lua_pushboolean(lua, 1);
            else
                lua_pushboolean(lua, 0);
        }
        else if (top == 3)
        {
            const bool hasAlpha = lua_toboolean(lua, 3) ? true : false;
            if (backGround->load(fileName, hasAlpha))
                lua_pushboolean(lua, 1);
            else
                lua_pushboolean(lua, 0);
        }
        else if (backGround->load(fileName))
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    // Background-thread-friendly equivalent of the plain-load branch of "load" (mesh-v11-plan.md
    // milestone 22) - does not cover the loadFont(...) variant (see header comment on
    // BACKGROUND::loadAsync). Only the mesh-file path is genuinely deferred to a worker thread; the
    // texture path stays synchronous within this same callback-shaped API (no TEXTURE_MANAGER async
    // primitive exists anywhere in the engine). The callback is always the LAST argument, same
    // effectiveTop trick as onLoadAsyncParticleLua, since the optional hasAlpha arg can be omitted.
    namespace
    {
        struct AsyncLoadCtxBackGround
        {
            lua_State *lua;
            int        refCallback;
            int        refSelf;
        };
    }

    int onLoadAsyncBackGroundLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (lua_type(lua, top) != LUA_TFUNCTION)
            return lua_error_debug(lua, "expected [function] as the last argument (callback) for loadAsync");
        const int    effectiveTop = top - 1;
        BACKGROUND * backGround   = getBackGroundFromRawTable(lua, 1, 1);
        const char * fileName     = luaL_checkstring(lua, 2);
        const bool   hasAlpha     = effectiveTop >= 3 ? (lua_toboolean(lua, 3) ? true : false) : false;
        auto *ctx        = new AsyncLoadCtxBackGround();
        ctx->lua         = lua;
        lua_pushvalue(lua, top);
        ctx->refCallback = luaL_ref(lua, LUA_REGISTRYINDEX);
        lua_pushvalue(lua, 1);
        ctx->refSelf     = luaL_ref(lua, LUA_REGISTRYINDEX);
        backGround->loadAsync(fileName, hasAlpha, true, [ctx](bool success)
        {
            lua_State *lua = ctx->lua;
            lua_rawgeti(lua, LUA_REGISTRYINDEX, ctx->refCallback);
            if (lua_isfunction(lua, -1))
            {
                lua_rawgeti(lua, LUA_REGISTRYINDEX, ctx->refSelf);
                lua_pushboolean(lua, success);
                if (lua_pcall(lua, 2, 0, 0))
                    lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
            }
            else
            {
                lua_pop(lua, 1);
            }
            luaL_unref(lua, LUA_REGISTRYINDEX, ctx->refCallback);
            luaL_unref(lua, LUA_REGISTRYINDEX, ctx->refSelf);
            delete ctx;
        });
        return 0;
    }

    int onSetFront3dGroundLua(lua_State *lua)
    {
        BACKGROUND *backGround = getBackGroundFromRawTable(lua, 1, 1);
        const bool  isFront    = lua_toboolean(lua, 2) ? true : false;
        backGround->setFrontGround(isFront);
        return 0;
    }

    int onSetFront2dGroundLua(lua_State *lua)
    {
        BACKGROUND *backGround = getBackGroundFromRawTable(lua, 1, 1);
        const bool  isFront    = lua_toboolean(lua, 2) ? true : false;
        VEC3 &position         = backGround->getPosition();
        if (isFront)
        {
            if (position.z > 0.0f)
                position.z *= -1.0f;
        }
        else
        {
            if (position.z < 0.0f)
                position.z *= -1.0f;
        }
        return 0;
    }

	int onNewBackGroundNoGcLua(lua_State *lua,RENDERIZABLE * renderizable)
	{
		lua_settop(lua,0);
		if(renderizable == nullptr || renderizable->getUserData() != nullptr)
			return false;
		
		//table
		luaL_Reg                     regBackGroundMethods[] = {{"load", onLoadBackGroundLua},
                                           {"loadAsync", onLoadAsyncBackGroundLua},
                                           {"setFront3d", onSetFront3dGroundLua},
                                           {"setFront", onSetFront2dGroundLua},
                                           {nullptr, nullptr}};

        SELF_ADD_COMMON_METHODS selfMethods(regBackGroundMethods);
        const luaL_Reg *             regMethods = selfMethods.get();
        
		lua_createtable(lua, 0, selfMethods.getSize());//the table renderizable
		luaL_setfuncs(lua, regMethods, 0);//Registers all functions in the array l (see luaL_Reg) into the table on the top of the stack 
        

		//Metatable
		luaL_Reg regMeshMethodsMetaTable[] = {
                                           {"__newindex", onNewIndexRenderizableLua},
                                           {"__index", onIndexRenderizableLua},
                                           {"__gc", onDestroyNoGcBackGroundLua},
                                           {"__close", onDestroyRenderizable},
                                           {nullptr, nullptr}};

		lua_newtable(lua);//the metatable from renderizable
		luaL_setfuncs(lua, regMeshMethodsMetaTable, 0);//Registers all functions in the array l (see luaL_Reg) into the table on the top of the stack 
		lua_setmetatable(lua,-2);
		
		auto ** udata             = static_cast<BACKGROUND **>(lua_newuserdata(lua, sizeof(BACKGROUND *)));
        auto background           = static_cast<BACKGROUND*>(renderizable);
		auto user_data            = new USER_DATA_RENDER_LUA();
        renderizable->setUserData(user_data);
        *udata                    = background;
        
		/* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_BACKGROUND);
        luaL_getmetatable(lua,__userdata_name);
		lua_setmetatable(lua,-2);//metatable from user data
		/* end trick */

        lua_rawseti(lua, -2, 1);//userdata as raw index in the table renderizable
		
		user_data->refTableLua(lua, 1, &user_data->ref_MeAsTable);//always ref to be able to retrieve 
		return 1;
	}

    int onNewBackGroundLua(lua_State *lua)
    {
        const int top                    = lua_gettop(lua);
        luaL_Reg  regBackGroundMethods[] = {{"load", onLoadBackGroundLua},
                                           {"loadAsync", onLoadAsyncBackGroundLua},
                                           {"setFront3d", onSetFront3dGroundLua},
                                           {"setFront", onSetFront2dGroundLua},
                                           {nullptr, nullptr}};
        bool is3d = false;
        if (top == 2)
        {
            const char *type = luaL_checkstring(lua, 2);
            is3d             = strcasecmp("3d", type) == 0;
        }
        SELF_ADD_COMMON_METHODS selfMethods(regBackGroundMethods);
        const luaL_Reg *             regMethods = selfMethods.get();

        lua_settop(lua, 0);
        // luaL_newlib(lua, regMethods);
        lua_createtable(lua, 0, selfMethods.getSize());
        luaL_setfuncs(lua, regMethods, 0);
        luaL_getmetatable(lua, "_mbmBackGround");
        lua_setmetatable(lua, -2);

        auto **udata      = static_cast<BACKGROUND **>(lua_newuserdata(lua, sizeof(BACKGROUND *)));
        DEVICE *device     = DEVICE::getInstance();
        auto  backGround = new BACKGROUND(device->getScene(), is3d);
        backGround->setUserData(new USER_DATA_RENDER_LUA());
        *udata                  = backGround;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_BACKGROUND);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }

    void registerClassBackGround(lua_State *lua)
    {
        luaL_Reg regBackGroundMethods[] = {{"new", onNewBackGroundLua},
                                           {"__newindex", onNewIndexRenderizableLua},
                                           {"__index", onIndexRenderizableLua},
                                           {"__gc", onDestroyBackGroundLua},
                                           {"__close", onDestroyRenderizable},
                                           {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmBackGround");
        luaL_setfuncs(lua, regBackGroundMethods, 0);
        lua_setglobal(lua, "backGround");
        lua_settop(lua,0);
    }
};
