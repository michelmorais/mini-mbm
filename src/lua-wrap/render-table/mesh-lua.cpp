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

#include <lua-wrap/render-table/mesh-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/common-methods-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <render/mesh.h>
#include <platform/mismatch-platform.h>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

namespace mbm
{

    MESH *getMeshFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<MESH **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_MESH));
        return *ud;
    }

    int onDestroyMeshLua(lua_State *lua)
    {
        MESH *                mesh     = getMeshFromRawTable(lua, 1, 1);
        auto *userData = static_cast<USER_DATA_RENDER_LUA *>(mesh->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        mesh->userData = nullptr;
    #if DEBUG_FREE_LUA
        const char *fileName = mesh->getFileName();
        static int  num      = 1;
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",mesh->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(mesh);
        delete mesh;
        return 0;
    }

	int onDestroyNoGcMeshLua(lua_State *lua)
    {
        MESH *                mesh     = getMeshFromRawTable(lua, 1, 1);
        auto *userData = static_cast<USER_DATA_RENDER_LUA *>(mesh->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        mesh->userData = nullptr;
    #if DEBUG_FREE_LUA
        const char *fileName = mesh->getFileName();
        static int  num      = 1;
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",mesh->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(mesh);
        return 0;
    }

    int onLoadMeshLua(lua_State *lua)
    {
        MESH *      mesh     = getMeshFromRawTable(lua, 1, 1);
        const char *fileName = luaL_checkstring(lua, 2);
        if (mesh->getFileName() && strcmp(mesh->getFileName(), fileName) == 0)
        {
            lua_pushboolean(lua, 1);
            return 1;
        }
        else
        {
            mesh->release();
        }
        if (mesh->load(fileName))
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

	int onNewMeshNoGcLua(lua_State *lua,RENDERIZABLE * renderizable)
	{
		lua_settop(lua,0);
		if(renderizable == nullptr || renderizable->userData != nullptr)
			return false;
		
		//table
		luaL_Reg                     regMeshMethods[] = {{"load", onLoadMeshLua}, {nullptr, nullptr}};

        SELF_ADD_COMMON_METHODS selfMethods(regMeshMethods);
        const luaL_Reg *             regMethods = selfMethods.get();
        
		lua_createtable(lua, 0, selfMethods.getSize());//the table renderizable
		luaL_setfuncs(lua, regMethods, 0);//Registers all functions in the array l (see luaL_Reg) into the table on the top of the stack 
        

		//Metatable
		luaL_Reg regMeshMethodsMetaTable[] = {{"__newindex", onNewIndexRenderizableLua},
                                     {"__index", onIndexRenderizableLua},
                                     {"__gc", onDestroyNoGcMeshLua},
                                     {"__close", onDestroyRenderizable},
                                     {nullptr, nullptr}};

		lua_newtable(lua);//the metatable from renderizable
		luaL_setfuncs(lua, regMeshMethodsMetaTable, 0);//Registers all functions in the array l (see luaL_Reg) into the table on the top of the stack 
		lua_setmetatable(lua,-2);
		
		auto ** udata             = static_cast<MESH **>(lua_newuserdata(lua, sizeof(MESH *)));
        auto mesh                 = static_cast<MESH*>(renderizable);
		auto user_data            = new USER_DATA_RENDER_LUA();
        renderizable->userData    = user_data;
        *udata                    = mesh;
        
		/* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_MESH);
        luaL_getmetatable(lua,__userdata_name);
		lua_setmetatable(lua,-2);//metatable from user data
		/* end trick */

        lua_rawseti(lua, -2, 1);//userdata as raw index in the table renderizable
		
		user_data->refTableLua(lua, 1, &user_data->ref_MeAsTable);//always ref to be able to retrieve 
		return 1;
	}

    int onNewMeshLua(lua_State *lua)
    {
        const int                    top              = lua_gettop(lua);
        luaL_Reg                     regMeshMethods[] = {{"load", onLoadMeshLua}, {nullptr, nullptr}};
        SELF_ADD_COMMON_METHODS selfMethods(regMeshMethods);
        const luaL_Reg *             regMethods = selfMethods.get();

        VEC3 position(0, 0, 0);
        bool is3d  = true;
        bool is2ds = false;
        bool is2dw = false;
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
        // luaL_newlib(lua, regMethods);
        lua_createtable(lua, 0, selfMethods.getSize());
        luaL_setfuncs(lua, regMethods, 0);
        luaL_getmetatable(lua, "_mbmMesh");
        lua_setmetatable(lua, -2);

        auto **      udata  = static_cast<MESH **>(lua_newuserdata(lua, sizeof(MESH *)));
        DEVICE *device = DEVICE::getInstance();
        auto        mesh   = new MESH(device->scene, is3d, is2ds);
        mesh->userData      = new USER_DATA_RENDER_LUA();
        *udata              = mesh;
        if (position.x != 0.0f) //-V550
            mesh->position.x = position.x;
        if (position.y != 0.0f) //-V550
            mesh->position.y = position.y;
        if (position.z != 0.0f) //-V550
            mesh->position.z = position.z;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_MESH);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }

    void registerClassMesh(lua_State *lua)
    {
        luaL_Reg regMeshMethods[] = {{"new", onNewMeshLua},
                                     {"__newindex", onNewIndexRenderizableLua},
                                     {"__index", onIndexRenderizableLua},
                                     {"__gc", onDestroyMeshLua},
                                     {"__close", onDestroyRenderizable},
                                     {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmMesh");
        luaL_setfuncs(lua, regMeshMethods, 0);
        lua_setglobal(lua, "mesh");
        lua_settop(lua,0);
    }
};
