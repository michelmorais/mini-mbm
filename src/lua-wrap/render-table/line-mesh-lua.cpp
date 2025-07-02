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

#include <lua-wrap/render-table/line-mesh-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/common-methods-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <lua-wrap/current-scene-lua.h>
#include <core_mbm/device.h>
#include <render/line-mesh.h>
#include <platform/mismatch-platform.h>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

namespace mbm
{
    struct INFO_PHYSICS;
    
    extern int onSetPhysicsFromTableLua(lua_State *lua,INFO_PHYSICS* infoPhysics,LINE_MESH * lineMesh);
	extern int lua_error_debug(lua_State *lua, const char *format, ...);

    LINE_MESH *getLineMeshFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<LINE_MESH **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_LINE));
        return *ud;
    }

    int onDestroyLineMeshLua(lua_State *lua)
    {
        LINE_MESH *           lineMesh = getLineMeshFromRawTable(lua, 1, 1);
        auto *userData = static_cast<USER_DATA_RENDER_LUA *>(lineMesh->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        lineMesh->userData = nullptr;
    #if DEBUG_FREE_LUA
        static int  num      = 1;
        const char *fileName = lineMesh->getFileName();
        PRINT_IF_DEBUG( "free lineMesh [%s] [%d]\n", fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(lineMesh);
        delete lineMesh;
        return 0;
    }

    int onAddLineMeshLua(lua_State *lua)
    {
        const int          top         = lua_gettop(lua);
        LINE_MESH *        lineMesh    = getLineMeshFromRawTable(lua, 1, 1);
        const int          hasTableXYZ = top > 1 ? lua_type(lua, 2) : LUA_TNIL;
        const unsigned int sTableXYZ   = (hasTableXYZ == LUA_TTABLE) ? lua_rawlen(lua, 2) : 0;
        unsigned int       ret         = 0xffffffff;
        if (sTableXYZ == 0)
        {
            if (lineMesh->is3D)
                return lua_error_debug(lua, "[table XYZ] empty!");
            else
                return lua_error_debug(lua, "[table XY] empty!");
        }
        if (lineMesh->is3D && sTableXYZ % 3)
        {
            return lua_error_debug(lua, "[table XYZ] must contain coordinates x,y and z (divisible by 3)! size[%d]", sTableXYZ);
        }
        if (!lineMesh->is3D && sTableXYZ % 2)
        {
            return lua_error_debug(lua, "[table XY] must contain coordinates x and y (pair)! size [%d]", sTableXYZ);
        }
        if (lineMesh->is3D)
        {
            std::vector<VEC3> xyz = getArrayXYZ_FromTable(lua,2);
            ret = lineMesh->add(std::move(xyz));
        }
        else
        {
            std::vector<VEC3> xyz = getArrayXYZ_noZ_FromTable(lua,2);
            ret = lineMesh->add(std::move(xyz));
        }
        if (ret == 0xffffffff)
            lua_pushinteger(lua, 0);
        else
            lua_pushinteger(lua, ret + 1);
        return 1;
    }

    int onSetLineMeshLua(lua_State *lua)
    {
        const int          top         = lua_gettop(lua);
        LINE_MESH *        lineMesh    = getLineMeshFromRawTable(lua, 1, 1);
        const int          hasTableXYZ = top > 1 ? lua_type(lua, 2) : LUA_TNIL;
        const unsigned int sTableXYZ   = (hasTableXYZ == LUA_TTABLE) ? lua_rawlen(lua, 2) : 0;
        const unsigned int indexLine   = top > 2 ? (luaL_checkinteger(lua, 3) - 1) : 0xffffffff;

        unsigned int ret = 0xffffffff;
        if (sTableXYZ == 0)
        {
            if (lineMesh->is3D)
                return lua_error_debug(lua, "[table XYZ] empty!");
            else
                return lua_error_debug(lua, "[table XY] empty!");
        }
        if (lineMesh->is3D && sTableXYZ % 3)
        {
            return lua_error_debug(lua, "[table XYZ] must contain coordinates x,y e z (divisible by 3)! size [%d]", sTableXYZ);
        }
        if (!lineMesh->is3D && sTableXYZ % 2)
        {
            return lua_error_debug(lua, "[table XY] must contain coordinates x e y (divisible by 2)! size [%d]", sTableXYZ);
        }
        if (indexLine == 0xffffffff)
        {
            if(lineMesh->is3D)
                return lua_error_debug(lua, "Args: [table XYZ] [index]. Index must be >0 <= size of lines!");
            else
                return lua_error_debug(lua, "Args: [table XY] [index]. Index must be >0 <= size of lines!");
        }
        if (lineMesh->is3D)
        {
            std::vector<VEC3> xyz = getArrayXYZ_FromTable(lua,2);
            ret = lineMesh->set(std::move(xyz), indexLine);
        }
        else
        {
            std::vector<VEC3> ls_xy = getArrayXYZ_noZ_FromTable(lua,2);
            ret = lineMesh->set(std::move(ls_xy), indexLine);
        }
        if (ret == 0xffffffff)
            lua_pushinteger(lua, 0);
        else
            lua_pushinteger(lua, ret + 1);
        return 1;
    }

    int onSetColorLineMeshLua(lua_State *lua)
    {
        const int   top      = lua_gettop(lua);
        LINE_MESH * lineMesh = getLineMeshFromRawTable(lua, 1, 1);
        const float r        = luaL_checknumber(lua, 2);
        const float g        = luaL_checknumber(lua, 3);
        const float b        = luaL_checknumber(lua, 4);
        const float a        = top >= 5 ? luaL_checknumber(lua, 5) : 1.0f;
        lineMesh->color = COLOR(r, g, b, 1.0f);
        float d[4];
        d[0]                      = r;
        d[1]                      = g;
        d[2]                      = b;
        d[3]                      = a;
		auto * anim = lineMesh->getAnimation();
        if(anim)
        {
            anim->fx.setVarPShader("color", d);
            anim->fx.setMaxVarPShader("color", d);
            anim->fx.setMinVarPShader("color", d);
        }
        return 0;
    }

    int onSetPhysicsLua(lua_State *lua)
    {
        LINE_MESH * lineMesh       = getLineMeshFromRawTable(lua, 1, 1);
        INFO_PHYSICS* infoPhysics  = lineMesh->getNotConstInfoPhysics();
		const int     top          = lua_gettop(lua);
		const int     type         = top > 2 ? lua_type(lua, 3) : LUA_TNIL;
		LINE_MESH * pLineMesh      = nullptr;
		if (top >= 3)
		{
			if (type == LUA_TBOOLEAN)
			{
				if(lua_toboolean(lua,3))
					pLineMesh = lineMesh;
			}
			lua_settop(lua,2);
		}
		onSetPhysicsFromTableLua(lua,infoPhysics,pLineMesh);
        if(infoPhysics->lsCube.size() || infoPhysics->lsSphere.size() || infoPhysics->lsTriangle.size() || infoPhysics->lsCubeComplex.size())
            lua_pushboolean(lua,1);
        else
            lua_pushboolean(lua,0);
        return 1;
    }

	int onDrawBoundingLua(lua_State *lua)
    {
        const int top           = lua_gettop(lua);
        LINE_MESH * lineMesh    = getLineMeshFromRawTable(lua, 1, 1);
        RENDERIZABLE *ptr       = getRenderizableFromRawTable(lua, 1, 2);
        const bool useAABB      = top > 2 ? (lua_toboolean(lua,3) ? true : false ) : true;
        lineMesh->drawBounding(ptr,useAABB);
        return 0;
    }

    int onGetSizeLineMeshLua(lua_State *lua)
    {
        LINE_MESH * lineMesh    = getLineMeshFromRawTable(lua, 1, 1);
        lua_pushinteger(lua,lineMesh->getTotalLines());
        return 1;
    }

    int onNewLineMeshLua(lua_State *lua)
    {
        const int top                  = lua_gettop(lua);
        luaL_Reg  regLineMeshMethods[] = {
            {"add", onAddLineMeshLua},
            {"set", onSetLineMeshLua},
            {"size", onGetSizeLineMeshLua},
            {"setPhysics", onSetPhysicsLua},
			{"drawBounding", onDrawBoundingLua},
            {nullptr, nullptr}};

        luaL_Reg  regLineReplaceMethods[] = {
            {"setColor", onSetColorLineMeshLua}, 
            {nullptr, nullptr}};

        SELF_ADD_COMMON_METHODS selfMethods(regLineMeshMethods);
        const luaL_Reg *             regMethods = selfMethods.get(regLineReplaceMethods);
        VEC3                    position(0, 0, 0);
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
        lua_settop(lua, 0);
        // luaL_newlib(lua, regMethods);
        lua_createtable(lua, 0, selfMethods.getSize());
        luaL_setfuncs(lua, regMethods, 0);
        luaL_getmetatable(lua, "_mbmLineMesh");
        lua_setmetatable(lua, -2);

        auto **      udata    = static_cast<void **>(lua_newuserdata(lua, sizeof(void *)));
        DEVICE *device   = DEVICE::getInstance();
        auto   lineMesh = new LINE_MESH(device->scene, is3d, is2ds);
        lineMesh->userData    = new USER_DATA_RENDER_LUA();
        *udata                = lineMesh;
        if (position.x != 0.0f) //-V550
            lineMesh->position.x = position.x;
        if (position.y != 0.0f) //-V550
            lineMesh->position.y = position.y;
        if (position.z != 0.0f) //-V550
            lineMesh->position.z = position.z;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_LINE);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);

        return 1;
    }

    void registerClassLineMesh(lua_State *lua)
    {
        luaL_Reg regLineMeshMethods[] = {{"new", onNewLineMeshLua},
                                         {"__newindex", onNewIndexRenderizableLua},
                                         {"__index", onIndexRenderizableLua},
                                         {"__gc", onDestroyLineMeshLua},
                                         {"__close", onDestroyRenderizable},
                                         {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmLineMesh");
        luaL_setfuncs(lua, regLineMeshMethods, 0);
        lua_setglobal(lua, "line");
        lua_settop(lua,0);
    }

};