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

#include <lua-wrap/render-table/texture-view-lua.h>
#include <lua-wrap/common-methods-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <core_mbm/texture-manager.h>
#include <render/texture-view.h>
#include <platform/mismatch-platform.h>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

namespace mbm
{

    TEXTURE_VIEW *getTextureViewFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<TEXTURE_VIEW **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_TEXTURE));
        return *ud;
    }

    int onDestroyTextureViewLua(lua_State *lua)
    {
        TEXTURE_VIEW *        textureView = getTextureViewFromRawTable(lua, 1, 1);
        auto *userData    = static_cast<USER_DATA_RENDER_LUA *>(textureView->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        textureView->userData = nullptr;
    #if DEBUG_FREE_LUA
        static int  num      = 1;
        const char *fileName = textureView->getFileName();
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",textureView->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(textureView);
        delete textureView;
        return 0;
    }

    int onLoadTextureViewLua(lua_State *lua)
    {
        const int     top         = lua_gettop(lua);
        TEXTURE_VIEW *textureView = getTextureViewFromRawTable(lua, 1, 1);
        const char *  fileName    = luaL_checkstring(lua, 2);
        const float width         = top > 2 ? luaL_checknumber(lua,3) : 0.0f;
        const float height        = top > 3 ? luaL_checknumber(lua,4) : 0.0f;
        const bool hasAlpha       = top > 4 ? lua_toboolean(lua,5) : true;
        TEXTURE *tex              = textureView->getTexture();
        if (tex && strcmp(tex->getFileNameTexture(), fileName) == 0)
        {
            lua_pushboolean(lua, 1);
            return 1;
        }
        else
        {
            textureView->release();
        }
        if (textureView->load(fileName, width, height, hasAlpha))
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onSetSizeTextureViewLua(lua_State *lua)
    {
        TEXTURE_VIEW *textureView = getTextureViewFromRawTable(lua, 1, 1);
        const float   w           = luaL_checknumber(lua, 2);
        const float   h           = luaL_checknumber(lua, 3);
        textureView->setFrame(w, h);
        return 0;
    }

    int onNewTextureViewLua(lua_State *lua)
    {
        const int top                     = lua_gettop(lua);
        luaL_Reg  regTextureViewMethods[] = {
            {"load", onLoadTextureViewLua}, {"setSize", onSetSizeTextureViewLua}, {nullptr, nullptr}};
        SELF_ADD_COMMON_METHODS selfMethods(regTextureViewMethods);
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
        lua_createtable(lua, 0, selfMethods.getSize());
		luaL_setfuncs(lua, regMethods, 0);
		luaL_getmetatable(lua, "_mbmTextureView");
		lua_setmetatable(lua, -2);
		auto ** udata     = static_cast<TEXTURE_VIEW **>(lua_newuserdata(lua, sizeof(TEXTURE_VIEW *)));
        auto textureView          = new TEXTURE_VIEW(device->scene, is3d, is2ds);
        textureView->userData     = new USER_DATA_RENDER_LUA();
        *udata                    = textureView;
        if (position.x != 0.0f) //-V550
            textureView->position.x = position.x;
        if (position.y != 0.0f) //-V550
            textureView->position.y = position.y;
        if (position.z != 0.0f) //-V550
            textureView->position.z = position.z;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_TEXTURE);
        luaL_getmetatable(lua,__userdata_name);
		lua_setmetatable(lua,-2);
        /* end trick */
		lua_rawseti(lua, -2, 1);
		return 1;
    }

	int onDestroyNoGCTextureViewLua(lua_State *lua)
    {
        TEXTURE_VIEW *        textureView = getTextureViewFromRawTable(lua, 1, 1);
        auto *userData    = static_cast<USER_DATA_RENDER_LUA *>(textureView->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        textureView->userData = nullptr;
    #if DEBUG_FREE_LUA
        static int  num      = 1;
        const char *fileName = textureView->getFileName();
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",textureView->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(textureView);
        return 0;
    }


	int onNewTextureViewNoGcLua(lua_State *lua,RENDERIZABLE * renderizable)
	{
		lua_settop(lua,0);
		if(renderizable == nullptr || renderizable->userData != nullptr)
			return false;
		
		//table
		luaL_Reg  regTextureViewMethodsNormal[] = {{"load", onLoadTextureViewLua}, 
											 {"setSize", onSetSizeTextureViewLua},
											 {nullptr, nullptr}};

        

        SELF_ADD_COMMON_METHODS selfMethods(regTextureViewMethodsNormal);
        const luaL_Reg *             regMethods = selfMethods.get();
        
		lua_createtable(lua, 0, selfMethods.getSize());//the table renderizable
		luaL_setfuncs(lua, regMethods, 0);//Registers all functions in the array l (see luaL_Reg) into the table on the top of the stack 
        

		//Metatable
		luaL_Reg  regTextureViewMethodsMetaTable[] = {
											 {"__newindex", onNewIndexRenderizableLua},
                                             {"__index", onIndexRenderizableLua},
                                             {"__gc", onDestroyNoGCTextureViewLua},
                                             {"__close", onDestroyRenderizable},
											 {nullptr, nullptr}};

		lua_newtable(lua);//the metatable from renderizable
		luaL_setfuncs(lua, regTextureViewMethodsMetaTable, 0);//Registers all functions in the array l (see luaL_Reg) into the table on the top of the stack 
		lua_setmetatable(lua,-2);
		
		auto ** udata             = static_cast<TEXTURE_VIEW **>(lua_newuserdata(lua, sizeof(TEXTURE_VIEW *)));
        auto textureView          = static_cast<TEXTURE_VIEW*>(renderizable);
		auto user_data            = new USER_DATA_RENDER_LUA();
        renderizable->userData    = user_data;
        *udata                    = textureView;
        
		/* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_TEXTURE);
        luaL_getmetatable(lua,__userdata_name);
		lua_setmetatable(lua,-2);//metatable from user data
		/* end trick */

        lua_rawseti(lua, -2, 1);//userdata as raw index in the table renderizable
		
		user_data->refTableLua(lua, 1, &user_data->ref_MeAsTable);//always ref to be able to retrieve 
		return 1;
	}

    void registerClassTextureView(lua_State *lua)
    {
        luaL_Reg regTextureViewMethods[] = {{"new", onNewTextureViewLua},
                                            {"__newindex", onNewIndexRenderizableLua},
                                            {"__index", onIndexRenderizableLua},
                                            {"__gc", onDestroyTextureViewLua},
                                            {"__close", onDestroyRenderizable},
                                            {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmTextureView");
        luaL_setfuncs(lua, regTextureViewMethods, 0);
        lua_setglobal(lua, "texture");
        lua_settop(lua,0);
    }
};
