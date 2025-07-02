/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2017      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include <lua-wrap/render-table/gif-view-lua.h>
#include <lua-wrap/common-methods-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <core_mbm/texture-manager.h>
#include <render/gif-view.h>
#include <platform/mismatch-platform.h>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

namespace mbm
{

    GIF_VIEW *getGifViewFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<GIF_VIEW **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_GIF));
        return *ud;
    }

    int onDestroyGifViewLua(lua_State *lua)
    {
        GIF_VIEW *        gifView = getGifViewFromRawTable(lua, 1, 1);
        auto *userData    = static_cast<USER_DATA_RENDER_LUA *>(gifView->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        gifView->userData = nullptr;
    #if DEBUG_FREE_LUA
        static int  num      = 1;
        const char *fileName = gifView->getFileName();
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",gifView->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(gifView);
        delete gifView;
        return 0;
    }

	int onDestroyGifViewNoGcLua(lua_State *lua)
    {
        GIF_VIEW *        gifView = getGifViewFromRawTable(lua, 1, 1);
        auto *userData    = static_cast<USER_DATA_RENDER_LUA *>(gifView->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        gifView->userData = nullptr;
    #if DEBUG_FREE_LUA
        static int  num      = 1;
        const char *fileName = gifView->getFileName();
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",gifView->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(gifView);
        return 0;
    }

    int onLoadGifViewLua(lua_State *lua)
    {
        GIF_VIEW *gifView       = getGifViewFromRawTable(lua, 1, 1);
        const int   top         = lua_gettop(lua);
        const char *  fileName  = luaL_checkstring(lua, 2);
        TEXTURE *tex            = gifView->getTexture();
        if (tex && strcmp(tex->getFileNameTexture(), fileName) == 0)
        {
            lua_pushboolean(lua, 1);
            return 1;
        }
        else
        {
            gifView->release();
        }
        const float w = top > 2 ? luaL_checknumber(lua,3) : 0.0f;
        const float h = top > 3 ? luaL_checknumber(lua,4) : 0.0f;
        if (gifView->load(fileName, w, h))
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onSetSizeGifViewLua(lua_State *lua)
    {
        GIF_VIEW *gifView = getGifViewFromRawTable(lua, 1, 1);
        const float   w           = luaL_checknumber(lua, 2);
        const float   h           = luaL_checknumber(lua, 3);
        gifView->setFrame(w, h);
        return 0;
    }

	int onNewGifViewNoGcLua(lua_State *lua,RENDERIZABLE * renderizable)
	{
		lua_settop(lua,0);
		if(renderizable == nullptr || renderizable->userData != nullptr)
			return false;
		
		//table
		luaL_Reg  regGifViewMethods[] = {{"load", onLoadGifViewLua}, 
                                        {"setSize", onSetSizeGifViewLua}, 
                                        {nullptr, nullptr}};

        SELF_ADD_COMMON_METHODS selfMethods(regGifViewMethods);
        const luaL_Reg *             regMethods = selfMethods.get();
        
		lua_createtable(lua, 0, selfMethods.getSize());//the table renderizable
		luaL_setfuncs(lua, regMethods, 0);//Registers all functions in the array l (see luaL_Reg) into the table on the top of the stack 
        

		//Metatable
		luaL_Reg regMethodsMetaTable[] = {
                                            {"__newindex", onNewIndexRenderizableLua},
                                            {"__index", onIndexRenderizableLua},
                                            {"__gc", onDestroyGifViewNoGcLua},
                                            {"__close", onDestroyRenderizable},
                                            {nullptr, nullptr}};

		lua_newtable(lua);//the metatable from renderizable
		luaL_setfuncs(lua, regMethodsMetaTable, 0);//Registers all functions in the array l (see luaL_Reg) into the table on the top of the stack 
		lua_setmetatable(lua,-2);
		
		auto ** udata             = static_cast<GIF_VIEW **>(lua_newuserdata(lua, sizeof(GIF_VIEW *)));
        auto gif                  = static_cast<GIF_VIEW*>(renderizable);
		auto user_data            = new USER_DATA_RENDER_LUA();
        renderizable->userData    = user_data;
        *udata                    = gif;
        
		/* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_GIF);
        luaL_getmetatable(lua,__userdata_name);
		lua_setmetatable(lua,-2);//metatable from user data
		/* end trick */

        lua_rawseti(lua, -2, 1);//userdata as raw index in the table renderizable
		
		user_data->refTableLua(lua, 1, &user_data->ref_MeAsTable);//always ref to be able to retrieve 
		return 1;
	}

    int onNewGifViewLua(lua_State *lua)
    {
        const int top                     = lua_gettop(lua);
        luaL_Reg  regGifViewMethods[] = {{"load", onLoadGifViewLua}, 
                                        {"setSize", onSetSizeGifViewLua}, 
                                        {nullptr, nullptr}};
        SELF_ADD_COMMON_METHODS selfMethods(regGifViewMethods);
        const luaL_Reg *             regMethods = selfMethods.get();
        VEC3                         position(0, 0, 0);
        bool                         is3d  = false;
        bool                         is2ds = false;
        bool                         is2dw = false;
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
        DEVICE *device = DEVICE::getInstance();
        lua_settop(lua, 0);
        // luaL_newlib(lua, regMethods);
        lua_createtable(lua, 0, selfMethods.getSize());
        luaL_setfuncs(lua, regMethods, 0);
        luaL_getmetatable(lua, "_mbmGifView");
        lua_setmetatable(lua, -2);

		auto **   udata     = static_cast<GIF_VIEW **>(lua_newuserdata(lua, sizeof(void *)));
        auto gifView			= new GIF_VIEW(device->scene, is3d, is2ds);
        gifView->userData       = new USER_DATA_RENDER_LUA();
        *udata                    = gifView;
        if (position.x != 0.0f) //-V550
            gifView->position.x = position.x;
        if (position.y != 0.0f) //-V550
            gifView->position.y = position.y;
        if (position.z != 0.0f) //-V550
            gifView->position.z = position.z;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_GIF);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);

        return 1;
    }

    void registerClassGifView(lua_State *lua)
    {
        luaL_Reg regGifViewMethods[] = {{"new", onNewGifViewLua},
                                            {"__newindex", onNewIndexRenderizableLua},
                                            {"__index", onIndexRenderizableLua},
                                            {"__gc", onDestroyGifViewLua},
                                            {"__close", onDestroyRenderizable},
                                            {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmGifView");
        luaL_setfuncs(lua, regGifViewMethods, 0);
        lua_setglobal(lua, "gif");
        lua_settop(lua,0);
    }
};
