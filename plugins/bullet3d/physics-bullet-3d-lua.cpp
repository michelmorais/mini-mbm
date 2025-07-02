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

#include <cstring>
#include "../plugin-helper/plugin-helper.h"
#include "physics-bullet-3d-lua.h"
#include "physics-bullet-3d-wrap.h"
#include <core_mbm/device.h>
#include <core_mbm/renderizable.h>
#include <LinearMath/btVector3.h>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

extern "C" {
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
}


namespace mbm
{

struct USER_DATA_PHYSICS_3D
{
    char szBeginContact[255];
    char szEndContact[255];
    char szKeepContact[255];
    USER_DATA_PHYSICS_3D()
    {
        memset(this->szBeginContact, 0, sizeof(this->szBeginContact));
        memset(this->szEndContact, 0, sizeof(this->szEndContact));
        memset(this->szKeepContact, 0, sizeof(this->szKeepContact));
    }
    };

    PHYSICS_BULLET *getBulletFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<PHYSICS_BULLET **>(plugin_helper::lua_check_userType(lua,rawi,indexTable,mbm::L_USER_TYPE_BULLET3D));
        return *ud;
    }

    int onAddDynamicBodyBullet3d(lua_State *lua)
    {
        const int             top         = lua_gettop(lua);
        PHYSICS_BULLET *      bullet      = getBulletFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr         = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        const float           mass        = top > 2 ? luaL_checknumber(lua, 3) : 1.0f;
        const float           friction    = top > 3 ? luaL_checknumber(lua, 4) : 0.3f;
        const float           reduceX     = top > 4 ? luaL_checknumber(lua, 5) : 1.0f;
        const float           reduceY     = top > 5 ? luaL_checknumber(lua, 6) : 1.0f;
        const float           reduceZ     = top > 6 ? luaL_checknumber(lua, 7) : 1.0f;
        const bool            isKinematic = false;
        const bool            isCharacter = false;
        if (ptr)
        {
            auto *userData    = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
            if (!ptr->isLoaded())
            {
                return plugin_helper::lua_error_debug(lua, "object [%s] isnot loaded!!!", ptr->getTypeClassName());
            }
            if (userData->extra)
            {
                return plugin_helper::lua_error_debug(lua, "object [%s] already has a body", ptr->getTypeClassName());
            }
            btRigidBody *info = bullet->addBody(ptr, mass, friction, reduceX, reduceY, reduceZ, isKinematic, isCharacter);
            if (info == nullptr)
            {
                lua_pushboolean(lua, 0);
            }
            else
            {
                userData->refTableLua(lua, 2, &userData->ref_MeAsTable);
                lua_pushboolean(lua, 1);
            }
        }
        else
        {
            lua_pushboolean(lua, 0);
        }
        return 1;
    }

    int onAddDynamicCharacterBodyBullet3d(lua_State *lua)
    {
        const int             top         = lua_gettop(lua);
        PHYSICS_BULLET *      bullet      = getBulletFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr         = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        const float           mass        = top > 2 ? luaL_checknumber(lua, 3) : 1.0f;
        const float           friction    = top > 3 ? luaL_checknumber(lua, 4) : 0.3f;
        const float           reduceX     = top > 4 ? luaL_checknumber(lua, 5) : 1.0f;
        const float           reduceY     = top > 5 ? luaL_checknumber(lua, 6) : 1.0f;
        const float           reduceZ     = top > 6 ? luaL_checknumber(lua, 7) : 1.0f;
        const bool            isKinematic = false;
        const bool            isCharacter = true;
        if (ptr)
        {
            auto *userData    = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
            if (!ptr->isLoaded())
            {
                return plugin_helper::lua_error_debug(lua, "object [%s] isnot loaded!!!", ptr->getTypeClassName());
            }
            if (userData->extra)
            {
                return plugin_helper::lua_error_debug(lua, "object [%s] already has a body", ptr->getTypeClassName());
            }
            btRigidBody *info = bullet->addBody(ptr, mass, friction, reduceX, reduceY, reduceZ, isKinematic, isCharacter);
            if (info == nullptr )
            {
                lua_pushboolean(lua, 0);
            }
            else
            {
                userData->refTableLua(lua, 2, &userData->ref_MeAsTable);
                lua_pushboolean(lua, 1);
            }
        }
        else
        {
            lua_pushboolean(lua, 0);
        }
        return 1;
    }

    int onAddStaticBodyBullet3d(lua_State *lua)
    {
        const int             top         = lua_gettop(lua);
        PHYSICS_BULLET *      bullet      = getBulletFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr         = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        const float           friction    = top > 2 ? luaL_checknumber(lua, 3) : 0.3f;
        const float           reduceX     = top > 3 ? luaL_checknumber(lua, 4) : 1.0f;
        const float           reduceY     = top > 4 ? luaL_checknumber(lua, 5) : 1.0f;
        const float           reduceZ     = top > 5 ? luaL_checknumber(lua, 6) : 1.0f;
        const bool            isKinematic = false;
        const bool            isCharacter = false;
        if (ptr)
        {
            auto *userData    = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
            if (!ptr->isLoaded())
            {
                return plugin_helper::lua_error_debug(lua, "object [%s] isnot loaded!!!", ptr->getTypeClassName());
            }
            if (userData->extra)
            {
                return plugin_helper::lua_error_debug(lua, "object [%s] already has a body", ptr->getTypeClassName());
            }
            btRigidBody *info = bullet->addBody(ptr, 0.0f, friction, reduceX, reduceY, reduceZ, isKinematic, isCharacter);
            if (info == nullptr)
            {
                lua_pushboolean(lua, 0);
            }
            else
            {
                userData->refTableLua(lua, 2, &userData->ref_MeAsTable);
                lua_pushboolean(lua, 1);
            }
        }
        else
        {
            lua_pushboolean(lua, 0);
        }
        return 1;
    }

    int onAddKinematicBodyBullet3d(lua_State *lua)
    {
        const int             top         = lua_gettop(lua);
        PHYSICS_BULLET *      bullet      = getBulletFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr         = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        const float           mass        = top > 2 ? luaL_checknumber(lua, 3) : 1.0f;
        const float           friction    = top > 3 ? luaL_checknumber(lua, 4) : 0.3f;
        const float           reduceX     = top > 4 ? luaL_checknumber(lua, 5) : 1.0f;
        const float           reduceY     = top > 5 ? luaL_checknumber(lua, 6) : 1.0f;
        const float           reduceZ     = top > 6 ? luaL_checknumber(lua, 7) : 1.0f;
        const bool            isKinematic = true;
        if (ptr)
        {
            auto *userData    = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
            if (!ptr->isLoaded())
            {
                return plugin_helper::lua_error_debug(lua, "object [%s] isnot loaded!!!", ptr->getTypeClassName());
            }
            if (userData->extra)
            {
                return plugin_helper::lua_error_debug(lua, "object [%s] already has a body", ptr->getTypeClassName());
            }
            btRigidBody *info = bullet->addBody(ptr, mass, friction, reduceX, reduceY, reduceZ, isKinematic);
            if (info == nullptr)
            {
                lua_pushboolean(lua, 0);
            }
            else
            {
                userData->refTableLua(lua, 2, &userData->ref_MeAsTable);
                lua_pushboolean(lua, 1);
            }
        }
        else
        {
            lua_pushboolean(lua, 0);
        }
        return 1;
    }

    int onSetGravityBullet3d(lua_State *lua)
    {
        PHYSICS_BULLET *bullet = getBulletFromRawTable(lua, 1, 1);
        const VEC3 gravity(luaL_checknumber(lua, 2), luaL_checknumber(lua, 3), luaL_checknumber(lua, 4));
        bullet->setGravity(gravity);
        return 0;
    }

    int onGetGravityBullet3d(lua_State *lua)
    {
        PHYSICS_BULLET *bullet = getBulletFromRawTable(lua, 1, 1);
        const VEC3      ret(bullet->getGravity());
        lua_pushnumber(lua, ret.x);
        lua_pushnumber(lua, ret.y);
        lua_pushnumber(lua, ret.z);
        return 3;
    }

    int onApplyForceBodyBullet3d(lua_State *lua)
    {
        PHYSICS_BULLET *      bullet     = getBulletFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr        = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData   = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *       infoBullet = static_cast<SHAPE_INFO_3D *>(userData->extra);
        const float           x          = luaL_checknumber(lua, 3);
        const float           y          = luaL_checknumber(lua, 4);
        const float           z          = luaL_checknumber(lua, 5);
        bullet->applyForce(infoBullet, x, y, z);
        return 0;
    }

    int onApplyTorqueBodyBullet3d(lua_State *lua)
    {
        PHYSICS_BULLET *      bullet     = getBulletFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr        = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData   = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *       infoBullet = static_cast<SHAPE_INFO_3D *>(userData->extra);
        const float           x          = luaL_checknumber(lua, 3);
        const float           y          = luaL_checknumber(lua, 4);
        const float           z          = luaL_checknumber(lua, 5);
        bullet->applyTorque(infoBullet, x, y, z);
        return 0;
    }

    int onApplyImpulseBodyBullet3d(lua_State *lua)
    {
        PHYSICS_BULLET *      bullet     = getBulletFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr        = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData   = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *       infoBullet = static_cast<SHAPE_INFO_3D *>(userData->extra);
        const float           x          = luaL_checknumber(lua, 3);
        const float           y          = luaL_checknumber(lua, 4);
        const float           z          = luaL_checknumber(lua, 5);
        bullet->applyImpulse(infoBullet, x, y, z);
        return 0;
    }

    int onSetLinearVelocityBullet3d(lua_State *lua)
    {
        PHYSICS_BULLET *      bullet     = getBulletFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr        = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData   = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *       infoBullet = static_cast<SHAPE_INFO_3D *>(userData->extra);
        const float           x          = luaL_checknumber(lua, 3);
        const float           y          = luaL_checknumber(lua, 4);
        const float           z          = luaL_checknumber(lua, 5);
        bullet->setLinearVelocity(infoBullet, x, y, z);
        return 0;
    }

    int onSetFrictionBullet3d(lua_State *lua)
    {
        PHYSICS_BULLET *      bullet     = getBulletFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr        = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData   = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *       infoBullet = static_cast<SHAPE_INFO_3D *>(userData->extra);
        const float           f          = luaL_checknumber(lua, 3);
        bullet->setFriction(infoBullet, f);
        return 0;
    }

    int onSetRestituitionBullet3d(lua_State *lua)
    {
        PHYSICS_BULLET *      bullet     = getBulletFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr        = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData   = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *       infoBullet = static_cast<SHAPE_INFO_3D *>(userData->extra);
        const float           r          = luaL_checknumber(lua, 3);
        bullet->setRestituition(infoBullet, r);
        return 0;
    }

    int onSetMassBullet3d(lua_State *lua)
    {
        PHYSICS_BULLET *      bullet     = getBulletFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr        = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData   = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *       infoBullet = static_cast<SHAPE_INFO_3D *>(userData->extra);
        const float           mass       = luaL_checknumber(lua, 3);
        const float           x          = luaL_checknumber(lua, 4);
        const float           y          = luaL_checknumber(lua, 5);
        const float           z          = luaL_checknumber(lua, 6);
        bullet->setMass(infoBullet, mass, x, y, z);
        return 0;
    }

    int onInterfereBullet3d(lua_State *lua)
    {
        const int             top        = lua_gettop(lua);
        PHYSICS_BULLET *      bullet     = getBulletFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr        = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData   = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData); //-V595
        const float           x          = top >= 3 ? luaL_checknumber(lua, 3) : ptr->position.x;
        const float           y          = top >= 4 ? luaL_checknumber(lua, 4) : ptr->position.y;
        const float           z          = top >= 5 ? luaL_checknumber(lua, 5) : ptr->position.z;
        const float           ax         = top >= 6 ? luaL_checknumber(lua, 6) : ptr->angle.x;
        const float           ay         = top >= 7 ? luaL_checknumber(lua, 7) : ptr->angle.y;
        const float           az         = top >= 8 ? luaL_checknumber(lua, 8) : ptr->angle.z;
        if (ptr)
        {
            auto *       infoBullet = static_cast<SHAPE_INFO_3D *>(userData->extra);
            ptr->position.x = x;
            ptr->position.y = y;
            ptr->position.z = z;
            ptr->angle.x    = ax;
            ptr->angle.y    = ay;
            ptr->angle.z    = az;
            bullet->interference(infoBullet);
        }
        return 0;
    }

    int onSetAwakeBullet3d(lua_State *lua)
    {
        PHYSICS_BULLET *      bullet     = getBulletFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr        = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData   = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *       infoBullet = static_cast<SHAPE_INFO_3D *>(userData->extra);
        const bool            bValue     = lua_toboolean(lua, 3) ? true : false;
        bullet->setAwake(infoBullet, bValue);
        return 0;
    }

    int onIsAwakeBullet3d(lua_State *lua)
    {
        PHYSICS_BULLET *      bullet     = getBulletFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr        = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData   = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *       infoBullet = static_cast<SHAPE_INFO_3D *>(userData->extra);
        const bool            bValue     = bullet->isAwake(infoBullet);
        lua_pushboolean(lua, bValue ? 1 : 0);
        return 1;
    }

    void lua_bullet3d_BeginContact(PHYSICS_BULLET *bullet, RENDERIZABLE *info1, RENDERIZABLE *info2)
    {
        auto *uData = static_cast<USER_DATA_PHYSICS_3D *>(bullet->userData3d);
        if (uData->szBeginContact[0])
        {
            DEVICE *             device    = DEVICE::getInstance();
            auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
            lua_State *               lua       = userScene->lua;
            auto *    userData1 = static_cast<USER_DATA_RENDER_LUA *>(info1->userData);
            auto *    userData2 = static_cast<USER_DATA_RENDER_LUA *>(info2->userData);
            lua_getglobal(lua, uData->szBeginContact);
            if (userData1->ref_MeAsTable != LUA_NOREF && userData2->ref_MeAsTable != LUA_NOREF && lua_isfunction(lua, -1))
            {
                lua_rawgeti(lua, LUA_REGISTRYINDEX, userData1->ref_MeAsTable);
                lua_rawgeti(lua, LUA_REGISTRYINDEX, userData2->ref_MeAsTable);
                if (lua_pcall(lua, 2, 0, 0))
                    plugin_helper::lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
            }
        }
    }

    void lua_bullet3d_EndContact(PHYSICS_BULLET *bullet, RENDERIZABLE *info1, RENDERIZABLE *info2)
    {
        auto *uData = static_cast<USER_DATA_PHYSICS_3D *>(bullet->userData3d);
        if (uData->szEndContact[0])
        {
            DEVICE *             device    = DEVICE::getInstance();
            auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
            lua_State *               lua       = userScene->lua;
            auto *    userData1 = static_cast<USER_DATA_RENDER_LUA *>(info1->userData);
            auto *    userData2 = static_cast<USER_DATA_RENDER_LUA *>(info2->userData);
            lua_getglobal(lua, uData->szEndContact);
            if (userData1->ref_MeAsTable != LUA_NOREF && userData2->ref_MeAsTable != LUA_NOREF && lua_isfunction(lua, -1))
            {
                lua_rawgeti(lua, LUA_REGISTRYINDEX, userData1->ref_MeAsTable);
                lua_rawgeti(lua, LUA_REGISTRYINDEX, userData2->ref_MeAsTable);
                if (lua_pcall(lua, 2, 0, 0))
                    plugin_helper::lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
            }
        }
    }

    void lua_bullet3d_KeepContact(PHYSICS_BULLET *bullet, RENDERIZABLE *info1, RENDERIZABLE *info2)
    {
        auto *uData = static_cast<USER_DATA_PHYSICS_3D *>(bullet->userData3d);
        if (uData->szKeepContact[0])
        {
            DEVICE *             device    = DEVICE::getInstance();
            auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
            lua_State *               lua       = userScene->lua;
            auto *    userData1 = static_cast<USER_DATA_RENDER_LUA *>(info1->userData);
            auto *    userData2 = static_cast<USER_DATA_RENDER_LUA *>(info2->userData);
            lua_getglobal(lua, uData->szKeepContact);
            if (userData1->ref_MeAsTable != LUA_NOREF && userData2->ref_MeAsTable != LUA_NOREF && lua_isfunction(lua, -1))
            {
                lua_rawgeti(lua, LUA_REGISTRYINDEX, userData1->ref_MeAsTable);
                lua_rawgeti(lua, LUA_REGISTRYINDEX, userData2->ref_MeAsTable);
                if (lua_pcall(lua, 2, 0, 0))
                    plugin_helper::lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
            }
        }
    }

    int onSetContactListenerBullet3d(lua_State *lua)
    {
        const int       top            = lua_gettop(lua);
        PHYSICS_BULLET *bullet         = getBulletFromRawTable(lua, 1, 1);
        const char *    szBeginContact = (top > 1) ? (lua_type(lua, 2) == LUA_TNIL ? nullptr : luaL_checkstring(lua, 2)) : nullptr;
        const char *    szKeepContact  = (top > 2) ? (lua_type(lua, 3) == LUA_TNIL ? nullptr : luaL_checkstring(lua, 3)) : nullptr;
        const char *    szEndContact   = (top > 3) ? (lua_type(lua, 4) == LUA_TNIL ? nullptr : luaL_checkstring(lua, 4)) : nullptr;
        auto *uData    = static_cast<USER_DATA_PHYSICS_3D *>(bullet->userData3d);
        if (szBeginContact)
        {
            bullet->on_bullet3d_BeginContact = lua_bullet3d_BeginContact;
            strncpy(uData->szBeginContact, szBeginContact,sizeof(uData->szBeginContact)-1);
        }
        if (szKeepContact)
        {
            bullet->on_bullet3d_KeepContact = lua_bullet3d_KeepContact;
            strncpy(uData->szKeepContact, szKeepContact,sizeof(uData->szKeepContact)-1);
        }
        if (szEndContact)
        {
            bullet->on_bullet3d_EndContact = lua_bullet3d_EndContact;
            strncpy(uData->szEndContact, szEndContact,sizeof(uData->szEndContact)-1);
        }
        return 0;
    }

    int onIsOnTheGroundBullet3d(lua_State *lua)
    {
        PHYSICS_BULLET *      bullet     = getBulletFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr        = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData   = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *       infoBullet = static_cast<SHAPE_INFO_3D *>(userData->extra);
        lua_pushboolean(lua, bullet->isOnTheGround(infoBullet) ? 1 : 0);
        return 1;
    }

    int onRayCastBullet3d(lua_State *lua)
    {
        PHYSICS_BULLET *bullet = getBulletFromRawTable(lua, 1, 1);
        const float     x      = luaL_checknumber(lua, 2);
        const float     y      = luaL_checknumber(lua, 3);
        const float     z      = luaL_checknumber(lua, 4);
        const float     xEnd   = luaL_checknumber(lua, 5);
        const float     yEnd   = luaL_checknumber(lua, 6);
        const float     zEnd   = luaL_checknumber(lua, 7);
        const btVector3 startPoint(x, y, z);
        const btVector3 endPoint(xEnd, yEnd, zEnd);
        btVector3       hit(0, 0, 0);
        btVector3       normal(0, 0, 0);
        RENDERIZABLE *  ptr = bullet->rayCast(startPoint, endPoint, hit, normal);
        if (ptr)
        {
            auto *userData = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
            if (userData->ref_MeAsTable != LUA_NOREF)
            {
                lua_rawgeti(lua, LUA_REGISTRYINDEX, userData->ref_MeAsTable);
                lua_pushnumber(lua, x);
                lua_pushnumber(lua, y);
                lua_pushnumber(lua, z);
                lua_pushnumber(lua, xEnd);
                lua_pushnumber(lua, yEnd);
                lua_pushnumber(lua, zEnd);
                return 7;
            }
        }
        lua_pushnil(lua);
        return 1;
    }

    int onNewBullet3dLua(lua_State *lua)
    {
        const int       top = lua_gettop(lua);
        const btVector3 gravity((top > 1 ? luaL_checknumber(lua, 2) : 0.0f),
                                (top > 2 ? luaL_checknumber(lua, 3) : -90.8f),
                                (top > 3 ? luaL_checknumber(lua, 4) : 0.0f));
        const auto scale         = (float)(top > 4 ? luaL_checknumber(lua, 5) : 0.2f);
        const auto   maxSubSteps = (int)  (top > 5 ? luaL_checkinteger(lua, 6) : 10);
        const auto fixedTimeStep = (float)(top > 6 ? luaL_checknumber(lua, 7) : btScalar(1.) / btScalar(60.));
        lua_settop(lua, 0);
        luaL_Reg regBullet3dMethods[] = {{"getGravity", onGetGravityBullet3d},
                                         {"setGravity", onSetGravityBullet3d},
                                         {"addStaticBody", onAddStaticBodyBullet3d},
                                         {"addDynamicBody", onAddDynamicBodyBullet3d},
                                         {"addKinematicBody", onAddKinematicBodyBullet3d},
                                         {"addDynamicCharacterBody", onAddDynamicCharacterBodyBullet3d},
                                         {"applyForce", onApplyForceBodyBullet3d},
                                         {"applyTorque", onApplyTorqueBodyBullet3d},
                                         {"applyImpulse", onApplyImpulseBodyBullet3d},
                                         {"setLinearVelocity", onSetLinearVelocityBullet3d},
                                         {"isOnTheGround", onIsOnTheGroundBullet3d},
                                         {"setFriction", onSetFrictionBullet3d},
                                         {"setRestituition", onSetRestituitionBullet3d},
                                         {"setMass", onSetMassBullet3d},
                                         //{"setDensity",                       onSetDensityBullet3d},
                                         {"interfere", onInterfereBullet3d},
                                         {"setAwake", onSetAwakeBullet3d},
                                         {"isAwake", onIsAwakeBullet3d},
                                         //{"setBullet",                        onSetBulletBullet3d},
                                         //{"setEnabled",                       onSetEnabledBullet3d},
                                         //{"enableCollision",                  onEnableDisableCollisionBullet3d},
                                         {"setContactListener", onSetContactListenerBullet3d},
                                         //{"joint",                            onCreateJointBullet3d},
                                         //{"setFilter",                        onSetFilterBullet3d},
                                         //{"setAngularDamping",                onSetAngularDumpingBullet3d},
                                         //{"getScale",                     onGetScaleBullet3d},
                                         //{"queryAABB",                        onQueryAABBBullet3d},
                                         {"rayCast", onRayCastBullet3d},
                                         //{"getJoint",                     onGetJointBullet3d},
                                         //{"getBody",                          onGetBodyBullet3d},
                                         //{"destroyBody",                      onDestroyBodyBullet3d},
                                         //{"pause",                            onStopSimulateBullet3d},
                                         {nullptr, nullptr}};
        luaL_newlib(lua, regBullet3dMethods);
        luaL_getmetatable(lua, "_mbmBullet3d");
        lua_setmetatable(lua, -2);
        auto **udata    = static_cast<PHYSICS_BULLET **>(lua_newuserdata(lua, sizeof(PHYSICS_BULLET *)));
        DEVICE *    device        = DEVICE::getInstance();
        auto  bullet3d            = new PHYSICS_BULLET(device->scene);
        bullet3d->userData3d      = new USER_DATA_PHYSICS_3D();
        *udata                    = bullet3d;
        bullet3d->scale           = scale;
        bullet3d->maxSubSteps     = maxSubSteps;
        bullet3d->fixedTimeStep   = fixedTimeStep;
        bullet3d->init(gravity);

        /* trick to ensure that we will receive a expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_BULLET3D);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }

    int onDestroyBullet3dLua(lua_State *lua)
    {
        PHYSICS_BULLET *      bullet3d = getBulletFromRawTable(lua, 1, 1);
        auto *uData    = static_cast<USER_DATA_PHYSICS_3D *>(bullet3d->userData3d);
        delete uData;
        bullet3d->userData = nullptr;
    #if DEBUG_FREE_LUA
        static int v = 1;
        printf("destroying bullet3d %d \n", v++);
    #endif
        delete bullet3d;
        return 0;
    }

    void registerClassBullet3d(lua_State *lua)
    {
        luaL_Reg regBullet3dMMethods[] = {{"new", onNewBullet3dLua}, {"__gc", onDestroyBullet3dLua}, {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmBullet3d");
        luaL_setfuncs(lua, regBullet3dMMethods, 0);
        lua_setglobal(lua, "bullet3d");
    }

    const char* getVersionBullet()
    {
        return BULLET_VERSION;
    }
}
