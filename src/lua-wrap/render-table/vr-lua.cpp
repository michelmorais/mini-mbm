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

#ifdef USE_VR
    #include <lua-wrap/render-table/vr-lua.h>
#endif

#include <lua-wrap/render-table/render-2-texture-lua.h>
#include <lua-wrap/common-methods-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <render/HMD.h>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

namespace mbm
{
    extern RENDERIZABLE *getRenderizableFromRawTable(lua_State *lua, const int rawi, const int indexTable);

    HMD *getVRFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<HMD **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_VR));
        return *ud;
    }

    int onDestroyVR(lua_State *lua)
    {
        HMD *                 vr       = getVRFromRawTable(lua, 1, 1);
        auto *userData = static_cast<USER_DATA_RENDER_LUA *>(vr->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        vr->userData = nullptr;
    #if DEBUG_FREE_LUA
        static int  num      = 1;
        const char *fileName = vr->getFileName();
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",vr->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(vr);
        delete vr;
        return 0;
    }

    int onAddToVR(lua_State *lua)
    {
        HMD *         vr  = getVRFromRawTable(lua, 1, 1);
        RENDERIZABLE *ptr = getRenderizableFromRawTable(lua, 1, 2);
        if (vr->addObject2Render(ptr))
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onNewVR(lua_State *lua)
    {
        luaL_Reg regVRMethods[] = {{"add", onAddToVR}, {"getCamera", onGetCameraRender2Texture}, {nullptr, nullptr}};
        SELF_ADD_COMMON_METHODS selfMethods(regVRMethods);
        const luaL_Reg *             regMethods = selfMethods.get();
        lua_settop(lua, 0);
        
        lua_createtable(lua, 0, selfMethods.getSize());
        luaL_setfuncs(lua, regMethods, 0);
        luaL_getmetatable(lua, "_mbmVR");
        lua_setmetatable(lua, -2);
        DEVICE *device = DEVICE::getInstance();
        auto **       udata  = static_cast<HMD **>(lua_newuserdata(lua, sizeof(HMD *)));
        auto         vr     = new HMD(device->scene);
        
        *udata = vr;
        vr->load();

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_VR);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }

    void registerClassVR(lua_State *lua)
    {
        luaL_Reg regVRMethods[] = {{"new", onNewVR},
                                   {"__newindex", onNewIndexRenderizableLua},
                                   {"__index", onIndexRenderizableLua},
                                   {"__gc", onDestroyVR},
                                   {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmVR");
        luaL_setfuncs(lua, regVRMethods, 0);
        lua_setglobal(lua, "vr");
        lua_settop(lua,0);
    }
};
