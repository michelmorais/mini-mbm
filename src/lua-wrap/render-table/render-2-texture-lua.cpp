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

#include <lua-wrap/render-table/render-2-texture-lua.h>
#include <lua-wrap/common-methods-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <render/render-2-texture.h>
#include <platform/mismatch-platform.h>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

namespace mbm
{
    struct VEC3;

    extern const char *getRandomNameTexture();
    extern RENDERIZABLE *getRenderizableFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    extern int onNewVec3LuaNoGC(lua_State *lua, VEC3 *vec3);

    RENDER_2_TEXTURE *getRender2TextureTargetFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<RENDER_2_TEXTURE **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_RENDER_2_TEXTURE));
        return *ud;
    }

    CAMERA_TARGET *getCameraRender2TextureTargetFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<CAMERA_TARGET **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_CAMERA_TARGET));
        return *ud;
    }

    int onDestroyRender2Texture(lua_State *lua)
    {
        RENDER_2_TEXTURE *    render2texture = getRender2TextureTargetFromRawTable(lua, 1, 1);
        auto *userData       = static_cast<USER_DATA_RENDER_LUA *>(render2texture->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        render2texture->userData = nullptr;
    #if DEBUG_FREE_LUA
        static int  num      = 1;
        const char *fileName = render2texture->getFileName();
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",render2texture->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(render2texture);
        delete render2texture;
        return 0;
    }

    int onCreateRender2Texture(lua_State *lua)
    {
        const int          top            = lua_gettop(lua);
        RENDER_2_TEXTURE * render2texture = getRender2TextureTargetFromRawTable(lua, 1, 1);
        DEVICE *      device         = DEVICE::getInstance();
        const unsigned int w        = top > 1 ? luaL_checkinteger(lua, 2) : (unsigned int)device->getScaleBackBufferWidth();
        const unsigned int h        = top > 2 ? luaL_checkinteger(lua, 3) : (unsigned int)device->getScaleBackBufferHeight();
        const bool         hasAlpha = top > 3 ? (lua_toboolean(lua, 4) ? true : false) : true;
        const char *       fileName = top > 4 ? luaL_checkstring(lua, 5) : getRandomNameTexture();
        int texture_id              = 0;
        if (render2texture->load(w, h, w, h, fileName, hasAlpha,&texture_id))
        {
            lua_pushboolean(lua, 1);
            lua_pushstring(lua, fileName);
            lua_pushinteger(lua,texture_id);
        }
        else
        {
            lua_pushboolean(lua, 0);
            lua_pushnil(lua);
            lua_pushinteger(lua,0);
        }
        return 3;
    }

    int onAddRender2Texture(lua_State *lua)
    {
        RENDER_2_TEXTURE *render2texture = getRender2TextureTargetFromRawTable(lua, 1, 1);
        RENDERIZABLE *    ptr            = getRenderizableFromRawTable(lua, 1, 2);
        if (render2texture->addObject2Render(ptr))
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onRemoveRender2Texture(lua_State *lua)
    {
        RENDER_2_TEXTURE *render2texture = getRender2TextureTargetFromRawTable(lua, 1, 1);
        RENDERIZABLE *    ptr            = getRenderizableFromRawTable(lua, 1, 2);
        if (render2texture->removeObject2Render(ptr))
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onSetPosCameraRender2TextureLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            CAMERA_TARGET *camera = getCameraRender2TextureTargetFromRawTable(lua, 1, 1);
            for (int i = 2; i <= top; ++i)
            {
                switch (i)
                {
                    case 2: // x
                    {
                        camera->position.x = luaL_checknumber(lua, i);
                    }
                    break;
                    case 3: // y
                    {
                        camera->position.y = luaL_checknumber(lua, i);
                    }
                    break;
                    case 4: // z
                    {
                        camera->position.z = luaL_checknumber(lua, i);
                    }
                    break;
                    default: {
                    }
                    break;
                }
            }
        }
        return 0;
    }

    int onGetPosCameraRender2TextureLua(lua_State *lua)
    {
        CAMERA_TARGET *camera = getCameraRender2TextureTargetFromRawTable(lua, 1, 1);
        return onNewVec3LuaNoGC(lua, &camera->position);
    }

    int onGetFocusCameraRender2TextureLua(lua_State *lua)
    {
        CAMERA_TARGET *camera = getCameraRender2TextureTargetFromRawTable(lua, 1, 1);
        return onNewVec3LuaNoGC(lua, &camera->focus);
    }

    int onSetFocusCameraRender2TextureLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            CAMERA_TARGET *camera = getCameraRender2TextureTargetFromRawTable(lua, 1, 1);
            for (int i = 2; i <= top; ++i)
            {
                switch (i)
                {
                    case 2: // x
                    {
                        camera->focus.x = luaL_checknumber(lua, i);
                    }
                    break;
                    case 3: // y
                    {
                        camera->focus.y = luaL_checknumber(lua, i);
                    }
                    break;
                    case 4: // z
                    {
                        camera->focus.z = luaL_checknumber(lua, i);
                    }
                    break;
                    default: {
                    }
                    break;
                }
            }
        }
        return 0;
    }

    int onMoveCameraRender2TextureLua(lua_State *lua)
    {
        DEVICE *  device = DEVICE::getInstance();
        CAMERA_TARGET *camera = getCameraRender2TextureTargetFromRawTable(lua, 1, 1);
        const int      top    = lua_gettop(lua);
        switch (top)
        {
            case 2:
            {
                const float x = luaL_checknumber(lua, 2);
                camera->position.x += (device->delta * x);
            }
            break;
            case 3:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                camera->position.x += (device->delta * x);
                camera->position.y += (device->delta * y);
            }
            break;
            case 4:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                const float z = luaL_checknumber(lua, 4);
                camera->position.x += (device->delta * x);
                camera->position.y += (device->delta * y);
                camera->position.z += (device->delta * z);
            }
            break;
            default: {
            }
            break;
        }
        return 0;
    }

    int onSetScaleCameraRender2TextureLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            CAMERA_TARGET *camera = getCameraRender2TextureTargetFromRawTable(lua, 1, 1);
            for (int i = 2; i <= top; ++i)
            {
                switch (i)
                {
                    case 2: // x
                    {
                        camera->scale.x = luaL_checknumber(lua, i);
                    }
                    break;
                    case 3: // y
                    {
                        camera->scale.y = luaL_checknumber(lua, i);
                    }
                    break;
                    case 4: // z
                    {
                        camera->scale.z = luaL_checknumber(lua, i);
                    }
                    break;
                    default: {
                    }
                    break;
                }
            }
        }
        return 0;
    }

    int onSetAngleCameraRender2TextureLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            CAMERA_TARGET *camera = getCameraRender2TextureTargetFromRawTable(lua, 1, 1);
            for (int i = 2; i <= top; ++i)
            {
                switch (i)
                {
                    case 2: // x
                    {
                        camera->angle.x = luaL_checknumber(lua, i);
                    }
                    break;
                    case 3: // y
                    {
                        camera->angle.y = luaL_checknumber(lua, i);
                    }
                    break;
                    case 4: // z
                    {
                        camera->angle.z = luaL_checknumber(lua, i);
                    }
                    break;
                    default: {
                    }
                    break;
                }
            }
        }
        return 0;
    }

    int onGetScaleCameraRender2TextureLua(lua_State *lua)
    {
        CAMERA_TARGET *camera = getCameraRender2TextureTargetFromRawTable(lua, 1, 1);
        return onNewVec3LuaNoGC(lua, &camera->scale);
    }

    int onGetAngleCameraRender2TextureLua(lua_State *lua)
    {
        CAMERA_TARGET *camera = getCameraRender2TextureTargetFromRawTable(lua, 1, 1);
        return onNewVec3LuaNoGC(lua, &camera->angle);
    }

    int onGetUpCameraRender2TextureLua(lua_State *lua)
    {
        CAMERA_TARGET *camera = getCameraRender2TextureTargetFromRawTable(lua, 1, 1);
        return onNewVec3LuaNoGC(lua, &camera->up);
    }

    int onSetUpCameraRender2TextureLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            CAMERA_TARGET *camera = getCameraRender2TextureTargetFromRawTable(lua, 1, 1);
            for (int i = 2; i <= top; ++i)
            {
                switch (i)
                {
                    case 2: // x
                    {
                        camera->up.x = luaL_checknumber(lua, i);
                    }
                    break;
                    case 3: // y
                    {
                        camera->up.y = luaL_checknumber(lua, i);
                    }
                    break;
                    case 4: // z
                    {
                        camera->up.z = luaL_checknumber(lua, i);
                    }
                    break;
                    default: {
                    }
                    break;
                }
            }
        }
        return 0;
    }

    int onSetColorBackgroundRender2TextureLua(lua_State *lua)
    {
        const int         top            = lua_gettop(lua);
        RENDER_2_TEXTURE *render2texture = getRender2TextureTargetFromRawTable(lua, 1, 1);
        const float       r              = luaL_checknumber(lua, 2);
        const float       g              = luaL_checknumber(lua, 3);
        const float       b              = luaL_checknumber(lua, 4);
        const float       a              = top > 4 ? luaL_checknumber(lua, 5) : 1.0f;

        COLOR color(r, g, b, a);
        render2texture->colorClearBackGround = (unsigned int)color;
        return 0;
    }

    int onEnableFrameRender2TextureLua(lua_State *lua)
    {
        RENDER_2_TEXTURE *render2texture = getRender2TextureTargetFromRawTable(lua, 1, 1);
        const bool        mode           = lua_toboolean(lua, 2) == 0;
        render2texture->modeTextureOnly  = mode;
        return 0;
    }

    int onClearRender2TextureLua(lua_State *lua)
    {
        RENDER_2_TEXTURE *render2texture = getRender2TextureTargetFromRawTable(lua, 1, 1);
        render2texture->clear();
        return 0;
    }

    int onReleaseRender2TextureLua(lua_State *lua)
    {
        RENDER_2_TEXTURE *render2texture = getRender2TextureTargetFromRawTable(lua, 1, 1);
        render2texture->release();
        return 0;
    }
    
    int onSaveRender2Texture(lua_State *lua)
    {
        const int         top               = lua_gettop(lua);
        RENDER_2_TEXTURE *render2texture    = getRender2TextureTargetFromRawTable(lua, 1, 1);
        const char*        fileName         = luaL_checkstring(lua, 2);
        const int x                         = top > 2 ? luaL_checkinteger(lua,3) : 0;
        const int y                         = top > 3 ? luaL_checkinteger(lua,4) : 0;
        const int w                         = top > 4 ? luaL_checkinteger(lua,5) : render2texture->widthTexture;
        const int h                         = top > 5 ? luaL_checkinteger(lua,6) : render2texture->heightTexture;
        if(render2texture->saveAsPNG(fileName,x,y,w,h))
            lua_pushboolean(lua,1);
        else
            lua_pushboolean(lua,0);
        return 1;
    }
    
    int onGetCameraRender2Texture(lua_State *lua)
    {
        RENDER_2_TEXTURE *render2texture = getRender2TextureTargetFromRawTable(lua, 1, 1);
        const char *      type           = luaL_checkstring(lua, 2);
        const bool        is3d           = strcmp("3d", type) == 0;
        lua_settop(lua, 0);
        luaL_Reg regCamera3dMethods[] = {
            {"setPos", onSetPosCameraRender2TextureLua},     {"getPos", onGetPosCameraRender2TextureLua},
            {"setFocus", onSetFocusCameraRender2TextureLua}, {"getFocus", onGetFocusCameraRender2TextureLua},
            {"setScale", onSetScaleCameraRender2TextureLua}, {"getScale", onGetScaleCameraRender2TextureLua},
            {"setAngle", onSetAngleCameraRender2TextureLua}, {"getAngle", onGetAngleCameraRender2TextureLua},
            {"setUp", onSetUpCameraRender2TextureLua},       {"getUp", onGetUpCameraRender2TextureLua},
            {"move", onMoveCameraRender2TextureLua},         {nullptr, nullptr}};
        luaL_newlib(lua, regCamera3dMethods);
        auto **udata = static_cast<CAMERA_TARGET **>(lua_newuserdata(lua, sizeof(CAMERA_TARGET *)));
        if (is3d)
            *udata = &render2texture->camera3d;
        else
            *udata = &render2texture->camera2d;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_CAMERA_TARGET);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }

    int onNewRender2Texture(lua_State *lua)
    {
        const int top                        = lua_gettop(lua);
        luaL_Reg  regRender2TextureMethods[] = {
                                               {"enableFrame", onEnableFrameRender2TextureLua},
                                               {"create", onCreateRender2Texture},
                                               {"add", onAddRender2Texture},
                                               {"remove", onRemoveRender2Texture},
                                               {"clear", onClearRender2TextureLua},
                                               {"release", onReleaseRender2TextureLua},
                                               {"getCamera", onGetCameraRender2Texture},
                                               {"save", onSaveRender2Texture},
                                               {nullptr, nullptr}};

        luaL_Reg  regRender2TextureReplaceMethods[] = {{"setColor", onSetColorBackgroundRender2TextureLua},{nullptr, nullptr}};
        SELF_ADD_COMMON_METHODS selfMethods(regRender2TextureMethods);
        const luaL_Reg *             regMethods = selfMethods.get(regRender2TextureReplaceMethods);
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
        luaL_getmetatable(lua, "_mbmRender2TextureTarget");
        lua_setmetatable(lua, -2);
        DEVICE *      device              = DEVICE::getInstance();
        auto **udata                      = static_cast<RENDER_2_TEXTURE **>(lua_newuserdata(lua, sizeof(RENDER_2_TEXTURE *)));
        auto  render2texture              = new RENDER_2_TEXTURE(device->scene, is3d, is2ds);
        render2texture->userData          = new USER_DATA_RENDER_LUA();
        *udata                            = render2texture;
        if (position.x != 0.0f) //-V550
            render2texture->position.x = position.x;
        if (position.y != 0.0f) //-V550
            render2texture->position.y = position.y;
        if (position.z != 0.0f) //-V550
            render2texture->position.z = position.z;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_RENDER_2_TEXTURE);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }

    void registerClassRender2TextureTarget(lua_State *lua)
    {
        luaL_Reg regRender2TextureMethods[] = {{"new", onNewRender2Texture},
                                               {"__newindex", onNewIndexRenderizableLua},
                                               {"__index", onIndexRenderizableLua},
                                               {"__gc", onDestroyRender2Texture},
                                               {"__close", onDestroyRenderizable},
                                               {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmRender2TextureTarget");
        luaL_setfuncs(lua, regRender2TextureMethods, 0);
        lua_setglobal(lua, "render2texture");
        lua_settop(lua,0);
    }
};
