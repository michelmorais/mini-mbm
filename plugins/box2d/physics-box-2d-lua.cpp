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

#include "physics-box-2d-lua.h"
#include "../plugin-helper/plugin-helper.h"
#include <core_mbm/class-identifier.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/device.h>
#include <core_mbm/animation.h>
#include <core_mbm/util-interface.h>
#include <platform/mismatch-platform.h>
#include "box-2d-wrap.h"

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

namespace mbm
{
    int onGetJointLua(lua_State *lua, b2Joint *joint);
    extern b2Joint *getJointBox2dFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    API_IMPL int onSetPhysicsFromTableLua(lua_State *lua,const int indexTable,INFO_PHYSICS* infoPhysicsOut);
	PHYSICS_BOX2D *getBox2dFromRawTable(lua_State *lua, const int rawi, const int indexTable);

    struct USER_DATA_PHYSICS_2D : public REF_FUNCTION_LUA
    {
        int ref_BeginContact;
        int ref_EndContact;
        int ref_PreSolve;
        int ref_PostSolve;

        USER_DATA_PHYSICS_2D() : REF_FUNCTION_LUA(),
            ref_BeginContact                         (LUA_NOREF),
            ref_EndContact                           (LUA_NOREF),
            ref_PreSolve                             (LUA_NOREF),
            ref_PostSolve                            (LUA_NOREF)
        {}
        
        void unrefAllTableLua(lua_State *lua) override // destroy all
        {
            this->unrefTableLua(lua, &this->ref_BeginContact);
            this->unrefTableLua(lua, &this->ref_EndContact);
            this->unrefTableLua(lua, &this->ref_PreSolve);
            this->unrefTableLua(lua, &this->ref_PostSolve);
        }
    };



    b2Body *getBodyBox2dFromRawTable(lua_State *lua,const int rawi, const int indexTable)
    {
        RENDERIZABLE *        ptr       = plugin_helper::getRenderizableFromRawTable(lua, rawi, indexTable);
        auto *userData  = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *          infoBox2d = static_cast<SHAPE_INFO *>(userData->extra);
        if(infoBox2d == nullptr || infoBox2d->body == nullptr)
        {
            plugin_helper::lua_error_debug(lua, "object [%s] doesn't have a body", ptr->getTypeClassName());
            return nullptr;
        }
        return infoBox2d->body;
    }

    SHAPE_INFO *getShapeInfoFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        RENDERIZABLE *        ptr       = plugin_helper::getRenderizableFromRawTable(lua, rawi, indexTable);
        auto *userData  = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *          infoBox2d = static_cast<SHAPE_INFO *>(userData->extra);
        if(infoBox2d == nullptr || infoBox2d->body == nullptr)
            plugin_helper::lua_error_debug(lua, "object [%s] doesn't have a body", ptr->getTypeClassName());
        return infoBox2d;
    }

    void lua_box2d_onBox2dDestroyBodyFromList(RENDERIZABLE* ptr)
    {
        DEVICE * device = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        lua_State * lua = userScene->lua;
        auto *userData  = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        //if we have animation callback we dont want to take off the reference
        if (userData->ref_CallBackAnimation == LUA_NOREF &&
            userData->ref_CallBackTouchDown == LUA_NOREF &&
            userData->ref_CallBackEffectShader == LUA_NOREF)
        {
            userData->unrefAllTableLua(lua);
        }
        userData->extra = nullptr;
    }

    int onSetGravityBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *box2d = getBox2dFromRawTable(lua, 1, 1);
        const VEC2     gravity(luaL_checknumber(lua, 2), luaL_checknumber(lua, 3));
        box2d->setGravity(&gravity);
        return 0;
    }

    int onSetGravityScaleBodyBox2d(lua_State *lua)
    {
        b2Body *    body = getBodyBox2dFromRawTable(lua,1,2);
        const float n = luaL_checknumber(lua, 3);
        body->SetGravityScale(n); //-V522
        return 0;
    }

    int onGetGravityBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *box2d = getBox2dFromRawTable(lua, 1, 1);
        const VEC2     ret(box2d->getGravity());
        lua_pushnumber(lua, ret.x);
        lua_pushnumber(lua, ret.y);
        return 2;
    }

    int onGetGravityScaleBodyBox2d(lua_State *lua)
    {
        const b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        lua_pushnumber(lua, body->GetGravityScale()); //-V522
        return 1;
    }

    int onAddStaticBodyBox2d(lua_State *lua)
    {
        const int             top       = lua_gettop(lua);
        PHYSICS_BOX2D *       box2d     = getBox2dFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr       = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData  = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *          infoBox2d = static_cast<SHAPE_INFO *>(userData->extra);
        const float           density   = top > 2 ? luaL_checknumber(lua, 3) : 0.0f;
        const float           friction  = top > 3 ? luaL_checknumber(lua, 4) : 0.3f;
        const float           reduceX   = top > 4 ? luaL_checknumber(lua, 5) : 1.0f;
        const float           reduceY   = top > 5 ? luaL_checknumber(lua, 6) : 1.0f;
        const bool            isSensor  = top > 6 ? (lua_toboolean(lua, 7) ? true : false) : false;
        if (!ptr->isLoaded())
        {
            return plugin_helper::lua_error_debug(lua, "object [%s] is not loaded!!!", ptr->getTypeClassName());
        }
        if (infoBox2d)
        {
            if (box2d->undoDestroyBody(infoBox2d))
            {
#ifdef _DEBUG
                PRINT_IF_DEBUG("object [%s] already has a body. this should be not correct but I guess you are trying to reactive this body ... then ... go on", ptr->getTypeClassName());
#endif
                lua_pushboolean(lua, 1);
            }
            else
            {
                return plugin_helper::lua_error_debug(lua, "object [%s] already has a body", ptr->getTypeClassName());
            }
            return 1;
        }
        SHAPE_INFO *info = box2d->addStaticBody(ptr, density, friction, reduceX, reduceY, isSensor);
        if (info == nullptr || info->body == nullptr)
        {
            lua_pushboolean(lua, 0);
        }
        else
        {
            userData->refTableLua(lua, 2, &userData->ref_MeAsTable);
            userData->extra = info;
            lua_pushboolean(lua, 1);
        }
        return 1;
    }

    int onAddBodyBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d = getBox2dFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr   = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData  = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *         infoBox2d  = static_cast<SHAPE_INFO*>(userData->extra);
        if (lua_type(lua, 3) != LUA_TTABLE)
        {
            return plugin_helper::lua_error_debug(lua, "expected info table physics ex.: {type='dynamic',mass=1.0,friction=0.3,sx=1.0,sy=1.0,...}");
        }
        if (!ptr->isLoaded())
        {
            return plugin_helper::lua_error_debug(lua, "object [%s] is not loaded!!!", ptr->getTypeClassName());
        }
        if (infoBox2d)
        {
            if (box2d->undoDestroyBody(infoBox2d))
            {
#ifdef _DEBUG
                PRINT_IF_DEBUG( "object [%s] already has a body. this should be not correct but I guess you are trying to reactive this body ... then ... go on", ptr->getTypeClassName());
#endif
                lua_pushboolean(lua, 1);
            }
            else
            {
                return plugin_helper::lua_error_debug(lua, "object [%s] already has a body", ptr->getTypeClassName());
            }
            return 1;
        }
        SHAPE_INFO *info = nullptr;
        constexpr int indexTable = 3;
        lua_getfield(lua, 3, "type");
        const char *type = lua_type(lua, 4) == LUA_TSTRING ? lua_tostring(lua, 4) : nullptr;
        if (type == nullptr)
        {
            return plugin_helper::lua_error_debug(lua, "expected type at infoPhysics, ex.: {type='dynamic', ...");
        }
        float  density = 1.0f;
        float  friction = strcasecmp(type, "static") == 0 ? 0.3f : 10.0f;
        float  reduceX = 1.0f;
        float  reduceY = 1.0f;
        bool   isSensor = false;
        float  restitution = 0.1f;
        plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "density",    LUA_TNUMBER, &density);
        plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "friction",   LUA_TNUMBER, &friction);
        plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "sx",         LUA_TNUMBER, &reduceX);
        plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "sy",         LUA_TNUMBER, &reduceY);
        plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "isSensor",   LUA_TBOOLEAN, &isSensor);
        if (strcasecmp(type, "static") == 0)
        {
            info = box2d->addStaticBody(ptr, density, friction, reduceX, reduceY, isSensor);
        }
        else if (strcasecmp(type, "dynamic") == 0)
        {
            bool   isBullet = false;
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "isBullet", LUA_TBOOLEAN, &isBullet);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "restitution", LUA_TNUMBER, &restitution);
            info = box2d->addDynamicBody(ptr, density, friction, restitution, reduceX, reduceY, isSensor, isBullet);
        }
        else if (strcasecmp(type, "character") == 0)
        {
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "restitution", LUA_TNUMBER, &restitution);
            info = box2d->addDynamicBody(ptr, density, friction, restitution, reduceX, reduceY, isSensor);
            box2d->setFixedRotation(info,true);
            box2d->setSleepingAllowed(info,false);
        }
        else if (strcasecmp(type, "kinematic") == 0)
        {
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "restitution", LUA_TNUMBER, &restitution);
            info = box2d->addKinematicBody(ptr, density, friction, restitution, reduceX, reduceY, isSensor);
        }
        if (info == nullptr || info->body == nullptr)
        {
            lua_pushboolean(lua, 0);
        }
        else
        {
            userData->refTableLua(lua, 2, &userData->ref_MeAsTable);
            userData->extra = info;
            lua_pushboolean(lua, 1);
        }
        return 1;
    }

    int onAddDynamicBodyBox2d(lua_State *lua)
    {
        const int             top         = lua_gettop(lua);
        PHYSICS_BOX2D *       box2d       = getBox2dFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr         = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData    = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *          infoBox2d   = static_cast<SHAPE_INFO *>(userData->extra);


        const float           density     = top > 2 ? luaL_checknumber(lua, 3) : 1.0f;
        const float           friction    = top > 3 ? luaL_checknumber(lua, 4) : 10.0f;
        const float           restitution = top > 4 ? luaL_checknumber(lua, 5) : 0.1f;
        const float           reduceX     = top > 5 ? luaL_checknumber(lua, 6) : 1.0f;
        const float           reduceY     = top > 6 ? luaL_checknumber(lua, 7) : 1.0f;
        const bool            isSensor    = top > 7 ? (lua_toboolean(lua, 8) ? true : false) : false;
        const bool            isBullet    = top > 8 ? (lua_toboolean(lua, 9) ? true : false) : false;
        if (!ptr->isLoaded())
        {
            return plugin_helper::lua_error_debug(lua, "object [%s] is not loaded!!!", ptr->getTypeClassName());
        }
        if (infoBox2d)
        {
            if (box2d->undoDestroyBody(infoBox2d))
            {
#ifdef _DEBUG
                PRINT_IF_DEBUG( "object [%s] already has a body. this should be not correct but I guess you are trying to reactive this body ... then ... go on", ptr->getTypeClassName());
#endif
                lua_pushboolean(lua, 1);
            }
            else
            {
                return plugin_helper::lua_error_debug(lua, "object [%s] already has a body", ptr->getTypeClassName());
            }
            return 1;
        }
        SHAPE_INFO *info = box2d->addDynamicBody(ptr, density, friction, restitution, reduceX, reduceY, isSensor, isBullet);
        if (info == nullptr || info->body == nullptr)
        {
            lua_pushboolean(lua, 0);
        }
        else
        {
            userData->refTableLua(lua, 2, &userData->ref_MeAsTable);
            userData->extra = info;
            lua_pushboolean(lua, 1);
        }
        return 1;
    }

    b2ParticleGroupFlag getb2GroupFlagsParticleFromTable(lua_State *lua,int indexTable)
    {
        b2ParticleGroupFlag groupFlags(b2_solidParticleGroup);
        auto flagFromString = [](lua_State *lua,const char * flag) -> b2ParticleGroupFlag
        {
            if(strcmp(flag,"solidParticleGroup") == 0)
                return b2_solidParticleGroup;
            if(strcmp(flag,"rigidParticleGroup") == 0)
                return b2_rigidParticleGroup;
            if(strcmp(flag,"particleGroupCanBeEmpty") == 0)
                return b2_particleGroupCanBeEmpty;
            if(strcmp(flag,"particleGroupWillBeDestroyed") == 0)
                return b2_particleGroupWillBeDestroyed;
            if(strcmp(flag,"particleGroupNeedsUpdateDepth") == 0)
                return b2_particleGroupNeedsUpdateDepth;
            if(strcmp(flag,"particleGroupInternalMask") == 0)
                return b2_particleGroupInternalMask;
            plugin_helper::lua_error_debug(lua, "Flag group unknow [%s]",flag);
            return b2_solidParticleGroup;
        };
        lua_getfield(lua, indexTable, "groupFlags");
        if(lua_type(lua,-1) == LUA_TSTRING)
        {
            const char * flag = lua_tostring(lua,-1);
            groupFlags = flagFromString(lua,flag);
        }
        else if(lua_type(lua,-1) == LUA_TTABLE)
        {
            const std::size_t lenTable = lua_rawlen(lua, -1);
            for (std::size_t i=0; i<lenTable; ++i)
            {
                lua_rawgeti(lua, -1, (i + 1));
                const char * flag = lua_tostring(lua,-1);
                b2ParticleGroupFlag f = flagFromString(lua,flag);
                groupFlags = static_cast<b2ParticleGroupFlag>(static_cast<int>(f) | static_cast<int>(groupFlags));
                lua_pop(lua, 1);
            }
        }
        lua_pop(lua, 1);

        return groupFlags;
    }

    b2ParticleFlag getb2ParticleFlagFromTable(lua_State *lua,int indexTable)
    {
        b2ParticleFlag flags(b2_waterParticle);
        auto flagFromString = [](lua_State *lua,const char * flag) -> b2ParticleFlag
        {
	        if(strcmp(flag,"water") == 0)
                return b2_waterParticle;
	        if(strcmp(flag,"zombie") == 0)
                return b2_zombieParticle;
	        if(strcmp(flag,"wall") == 0)
                return b2_wallParticle;
	        if(strcmp(flag,"spring") == 0)
                return b2_springParticle;
	        if(strcmp(flag,"elastic") == 0)
                return b2_elasticParticle;
	        if(strcmp(flag,"viscous") == 0)
                return b2_viscousParticle;
	        if(strcmp(flag,"powder") == 0)
                return b2_powderParticle;
	        if(strcmp(flag,"tensile") == 0)
                return b2_tensileParticle;
	        if(strcmp(flag,"colorMixing") == 0)
                return b2_colorMixingParticle;
	        if(strcmp(flag,"destructionListener") == 0)
                return b2_destructionListenerParticle;
	        if(strcmp(flag,"barrier") == 0)
                return b2_barrierParticle;
	        if(strcmp(flag,"staticPressure") == 0)
                return b2_staticPressureParticle;
	        if(strcmp(flag,"reactive") == 0)
                return b2_reactiveParticle;
	        if(strcmp(flag,"repulsive") == 0)
                return b2_repulsiveParticle;
	        if(strcmp(flag,"fixtureContactListener") == 0)
                return b2_fixtureContactListenerParticle;
	        if(strcmp(flag,"particleContactListener") == 0)
                return b2_particleContactListenerParticle;
	        if(strcmp(flag,"fixtureContactFilter") == 0)
                return b2_fixtureContactFilterParticle;
	        if(strcmp(flag,"particleContactFilter") == 0)
                return b2_particleContactFilterParticle;
            plugin_helper::lua_error_debug(lua, "Flag unknow [%s]",flag);
            return b2_waterParticle;
        };
        lua_getfield(lua, indexTable, "flags");
        if(lua_type(lua,-1) == LUA_TSTRING)
        {
            const char * flag = lua_tostring(lua,-1);
            flags = flagFromString(lua,flag);
        }
        else if(lua_type(lua,-1) == LUA_TTABLE)
        {
            const std::size_t lenTable = lua_rawlen(lua, -1);
            for (std::size_t i=0; i<lenTable; ++i)
            {
                lua_rawgeti(lua, -1, (i + 1));
                const char * flag = lua_tostring(lua,-1);
                b2ParticleFlag f = flagFromString(lua,flag);
                flags = static_cast<b2ParticleFlag>(static_cast<int>(f) | static_cast<int>(flags));
                lua_pop(lua, 1);
            }
        }
        lua_pop(lua, 1);

        return flags;
    }

    int onCreateFluidBodyBox2d(lua_State *lua)
    {
        const int     top = lua_gettop(lua);
        if(top != 2 && top != 3)
        {
            return plugin_helper::lua_error_debug(lua, "expected physics:(<renderizable> | <physics_table>, <info_fluid>)");
        }
        if(top == 3 && lua_type(lua,3) != LUA_TTABLE)
        {
            return plugin_helper::lua_error_debug(lua, "expected physics:(<renderizable> | <physics_table>, <info_fluid>)");
        }
        PHYSICS_BOX2D *       box2d       = getBox2dFromRawTable(lua, 1, 1);
        RENDERIZABLE *        the_ptr     = plugin_helper::getRenderizableNoThrowFromRawTable(lua, 1, 2);
        INFO_PHYSICS  info_physics;
        INFO_PHYSICS*  local_info_physics = &info_physics;
        if(the_ptr != nullptr)
        {
            const INFO_PHYSICS*  const_info_physics = the_ptr->getInfoPhysics();
            if(const_info_physics == nullptr)
            {
                return plugin_helper::lua_error_debug(lua, "object [%s] has no physics!", the_ptr->getTypeClassName());
            }
            if(local_info_physics->clone(const_info_physics) == false)
            {
                return plugin_helper::lua_error_debug(lua, "Failed to clone phisics from [%s]", the_ptr->getTypeClassName());
            }
        }
        else if(onSetPhysicsFromTableLua(lua,2,local_info_physics) != 0)
        {
            return plugin_helper::lua_error_debug(lua, "Failed to create physics from lua table");
        }
        VEC3 position;
        VEC3 scale(1,1,1);
        VEC2 linearVelocity(0,0);
        bool is2dScreen        = false;
        bool is3d              = false;
        float radiusScale      = 1.0f;
        float radius           = 0.5f;
        float damping          = 0.5f;
        float lifetime         = 0.0f;
        float angularVelocity  = 0;
        float strength         = 1.0f;
        float stride           = 0;
        float angle            = the_ptr ? the_ptr->angle.z : 0.0f;
        bool segmented         = false;
        std::string texture("#AAFF00FF");
        COLOR * p_color = nullptr;
        COLOR color(1.0f,1.0f,1.0f,1.0f);
        std::string blend,operation;
        b2ParticleFlag flags(b2_waterParticle);
        b2ParticleGroupFlag groupFlags(b2_solidParticleGroup);
        if(top == 3)
        {
            constexpr int indexTable = 3;
            
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "texture",          LUA_TSTRING,  &texture);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "is2dScreen",       LUA_TBOOLEAN, &is2dScreen);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "is3d",             LUA_TBOOLEAN, &is3d);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "segmented",        LUA_TBOOLEAN, &segmented);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "radiusScale",      LUA_TNUMBER,  &radiusScale);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "radius",           LUA_TNUMBER,  &radius);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "damping",          LUA_TNUMBER,  &damping);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "lifetime",         LUA_TNUMBER,  &lifetime);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "angularVelocity",  LUA_TNUMBER,  &angularVelocity);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "strength",         LUA_TNUMBER,  &strength);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "stride",           LUA_TNUMBER,  &stride);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "angle",            LUA_TNUMBER,  &angle);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "blend",            LUA_TSTRING,  &blend);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "operation",        LUA_TSTRING,  &operation);

            lua_getfield(lua, indexTable, "scale");
            if(lua_type(lua,top + 1) == LUA_TTABLE)
            {
                plugin_helper::getFieldPrimaryFromTable(lua, top + 1, "x", LUA_TNUMBER, &scale.x);
                plugin_helper::getFieldPrimaryFromTable(lua, top + 1, "y", LUA_TNUMBER, &scale.y);
            }
            lua_pop(lua, 1);

            lua_getfield(lua, indexTable, "linearVelocity");
            if(lua_type(lua,top + 1) == LUA_TTABLE)
            {
                plugin_helper::getFieldPrimaryFromTable(lua, top + 1, "x", LUA_TNUMBER, &linearVelocity.x);
                plugin_helper::getFieldPrimaryFromTable(lua, top + 1, "y", LUA_TNUMBER, &linearVelocity.y);
            }
            lua_pop(lua, 1);

            lua_getfield(lua, indexTable, "position");
            if(lua_type(lua,top + 1) == LUA_TTABLE)
            {
                plugin_helper::getFieldPrimaryFromTable(lua, top + 1, "x", LUA_TNUMBER, &position.x);
                plugin_helper::getFieldPrimaryFromTable(lua, top + 1, "y", LUA_TNUMBER, &position.y);
                plugin_helper::getFieldPrimaryFromTable(lua, top + 1, "z", LUA_TNUMBER, &position.z);
            }
            lua_pop(lua, 1);

            lua_getfield(lua, indexTable, "color");
            if(lua_type(lua,top + 1) == LUA_TTABLE)
            {
                plugin_helper::getFieldPrimaryFromTable(lua, top + 1, "r", LUA_TNUMBER, &color.r);
                plugin_helper::getFieldPrimaryFromTable(lua, top + 1, "g", LUA_TNUMBER, &color.g);
                plugin_helper::getFieldPrimaryFromTable(lua, top + 1, "b", LUA_TNUMBER, &color.b);
                plugin_helper::getFieldPrimaryFromTable(lua, top + 1, "a", LUA_TNUMBER, &color.a);
                p_color = &color;
            }
            lua_pop(lua, 1);

            flags = getb2ParticleFlagFromTable(lua,indexTable);
            groupFlags = getb2GroupFlagsParticleFromTable(lua,indexTable);
        }
        
        INFO_FLUID *info = box2d->createRenderizableFluid(local_info_physics,
                                                        position, 
                                                        scale,
                                                        linearVelocity,
                                                        angularVelocity,
                                                        angle,
                                                        texture.c_str(),
                                                        p_color,
                                                        flags,
                                                        groupFlags,
                                                        lifetime,
                                                        radius,
                                                        damping,
                                                        strength,
                                                        stride,
                                                        is3d,
                                                        is2dScreen,
                                                        segmented,
                                                        radiusScale);
        if (info == nullptr || info->particleSystem == nullptr || info->steered_particle == nullptr)
        {
            return plugin_helper::lua_error_debug(lua, "Failed to create fluid. particleSystem:%p steered_particle:%p",info->particleSystem,info->steered_particle);
        }
        else
        {
            auto *userData = new USER_DATA_RENDER_LUA();
            info->steered_particle->userData = userData;
            userData->extra = info;
            if(blend.length() > 0)
            {
                ANIMATION_MANAGER * animManager = info->steered_particle->getAnimationManager();
                auto anim = animManager ? animManager->getAnimation() : nullptr;
                if(anim)
                {
                    const char * value = blend.c_str();
                    if (strcasecmp("disable", value) == 0)
                        anim->blendState = BLEND_DISABLE;
                    else if (strcasecmp("zero", value) == 0)
                        anim->blendState = BLEND_ZERO;
                    else if (strcasecmp("one", value) == 0)
                        anim->blendState = BLEND_ONE;
                    else if (strcasecmp("src_color", value) == 0)
                        anim->blendState = BLEND_SRCCOLOR;
                    else if (strcasecmp("inv_src_color", value) == 0)
                        anim->blendState = BLEND_INVSRCCOLOR;
                    else if (strcasecmp("src_alpha", value) == 0)
                        anim->blendState = BLEND_SRCALPHA;
                    else if (strcasecmp("inv_src_alpha", value) == 0)
                        anim->blendState = BLEND_INVSRCALPHA;
                    else if (strcasecmp("dest_alpha", value) == 0)
                        anim->blendState = BLEND_DESTALPHA;
                    else if (strcasecmp("inv_dest_alpha", value) == 0)
                        anim->blendState = BLEND_INVDESTALPHA;
                    else if (strcasecmp("dest_color", value) == 0)
                        anim->blendState = BLEND_DESTCOLOR;
                    else if (strcasecmp("inv_dest_color", value) == 0)
                        anim->blendState = BLEND_INVDESTCOLOR;
                    else
                    {
                        box2d->destroyFluid(info);
                        return plugin_helper::lua_error_debug(lua, "invalid blend [%s]",value);
                    }
                }
            }
            if(operation.length() > 0)
            {
                ANIMATION_MANAGER * animManager = info->steered_particle->getAnimationManager();
                auto anim = animManager ? animManager->getAnimation() : nullptr;
                if(anim)
                {
                    const char * blendFunc = operation.c_str();
                    if(strcasecmp(blendFunc,"ADD") == 0)
                        anim->fx.blendOperation = 1;
                    else if(strcasecmp(blendFunc,"SUBTRACT") == 0)
                        anim->fx.blendOperation = 2;
                    else if(strcasecmp(blendFunc,"REVERSE_SUBTRACT") == 0)
                        anim->fx.blendOperation = 3;
                    else if(strcasecmp(blendFunc,"MIN") == 0)
                        anim->fx.blendOperation = 4;
                    else if(strcasecmp(blendFunc,"MAX") == 0)
                        anim->fx.blendOperation = 5;
                    else
                    {
                        box2d->destroyFluid(info);
                        return plugin_helper::lua_error_debug(lua, "expected:([blendFunc (ADD,SUBTRACT,REVERSE_SUBTRACT,MIN,MAX)])");
                    }
                }
            }
            return onGetRenderizableFluidInterfaceBox2d(lua, info->steered_particle);
        }
    }

    int onAddKinematicBodyBox2d(lua_State *lua)
    {
        const int             top         = lua_gettop(lua);
        PHYSICS_BOX2D *       box2d       = getBox2dFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr         = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto * userData                   = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto * infoBox2d                  = static_cast<SHAPE_INFO *>(userData->extra);
        const float           density     = top > 2 ? luaL_checknumber(lua, 3) : 1.0f;
        const float           friction    = top > 3 ? luaL_checknumber(lua, 4) : 10.0f;
        const float           restitution = top > 4 ? luaL_checknumber(lua, 5) : 0.1f;
        const float           reduceX     = top > 5 ? luaL_checknumber(lua, 6) : 1.0f;
        const float           reduceY     = top > 6 ? luaL_checknumber(lua, 7) : 1.0f;
        const bool            isSensor    = top > 7 ? (lua_toboolean(lua, 8) ? true : false) : false;
        if (!ptr->isLoaded())
        {
            return plugin_helper::lua_error_debug(lua, "object [%s] is not loaded!!!", ptr->getTypeClassName());
        }
        if (infoBox2d)
        {
            if (box2d->undoDestroyBody(infoBox2d))
            {
#ifdef _DEBUG
                PRINT_IF_DEBUG( "object [%s] already has a body. this should be not correct but I guess you are trying to reactive this body ... then ... go on", ptr->getTypeClassName());
#endif
                lua_pushboolean(lua, 1);
            }
            else
            {
                return plugin_helper::lua_error_debug(lua, "object [%s] already has a body", ptr->getTypeClassName());
            }
            return 1;
        }
        SHAPE_INFO *info = box2d->addKinematicBody(ptr, density, friction, restitution, reduceX, reduceY, isSensor);
        if (info == nullptr || info->body == nullptr)
        {
            lua_pushboolean(lua, 0);
        }
        else
        {
            userData->refTableLua(lua, 2, &userData->ref_MeAsTable);
            userData->extra = info;
            lua_pushboolean(lua, 1);
        }
        return 1;
    }

    int onApplyForceBodyBox2d(lua_State *lua)
    {
        const int             top       = lua_gettop(lua);
        b2Body *body                    = getBodyBox2dFromRawTable(lua,1,2);
        const float           x         = luaL_checknumber(lua, 3);
        const float           y         = luaL_checknumber(lua, 4);
        const b2Vec2 f(x, y);
        if (top > 4)
        {
            const float wx = luaL_checknumber(lua, 5);
            const float wy = luaL_checknumber(lua, 6);
            const b2Vec2 p(wx, wy);
            body->ApplyForce(f, p, true);
        }
        else
        {
            body->ApplyForceToCenter(f,true);
        }
        return 0;
    }

    int onApplyForceToCenterBodyBox2dFromBody(lua_State *lua)
    {
        b2Body *    body = getBodyBox2dFromRawTable(lua,1,2);
        const float x = luaL_checknumber(lua, 3);
        const float y = luaL_checknumber(lua, 4);
        const bool  awake = lua_toboolean(lua, 5) ? true : false;
        body->ApplyForceToCenter(b2Vec2(x, y), awake); //-V522
        return 0;
    }

    int onSetLinearVelocityBox2d(lua_State *lua)
    {
        b2Body *body                    = getBodyBox2dFromRawTable(lua,1,2);
        const float           x         = luaL_checknumber(lua, 3);
        const float           y         = luaL_checknumber(lua, 4);
        const b2Vec2 v(x, y);
        body->SetLinearVelocity(v); //-V522
        return 0;
    }

    int onGetLinearVelocityBox2d(lua_State *lua)
    {
        const b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        const b2Vec2  b(body->GetLinearVelocity()); //-V522
        lua_pushnumber(lua, b.x);
        lua_pushnumber(lua, b.y);
        return 2;
    }


    int onSetAngularVelocityBox2d(lua_State *lua)
    {
        b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        const float           w = luaL_checknumber(lua, 3);
        body->SetAngularVelocity(w); //-V522
        return 0;
    }

    int onGetAngularVelocityBox2d(lua_State *lua)
    {
        const b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        lua_pushnumber(lua, body->GetAngularVelocity()); //-V522
        return 1;
    }

    int onGetInertiaBox2dFromBody(lua_State *lua)
    {
        const b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        lua_pushnumber(lua, body->GetInertia()); //-V522
        return 1;
    }

    int onGetMassBox2d(lua_State *lua)
    {
        const b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        lua_pushnumber(lua,body->GetMass()); //-V522
        return 1;
    }

    
    static void push_world_manifold(lua_State *lua, const b2WorldManifold * worldManifold)
    {
        auto push_manifold_point = [&] (const int index) -> void
        {
            lua_newtable(lua);// table point
            const b2Vec2 & mPoint = worldManifold->points[index];
            lua_pushnumber(lua, mPoint.x);
            lua_setfield(lua, -2, "x");
            lua_pushnumber(lua, mPoint.y);
            lua_setfield(lua, -2, "y");
        };
        /*
        tWorldManifold = {  normal       = {x = 0, y = 0 },
                            separations  = [1] = 0, [2] = 0,
                            points       = {[1] = {x = 0, y = 0},
                                            [2] = {x = 0, y = 0},}
        */
        // table world manifold
        lua_newtable(lua);
        // table localNormal
        lua_newtable(lua);
        lua_pushnumber(lua, worldManifold->normal.x);
        lua_setfield(lua, -2, "x");
        lua_pushnumber(lua, worldManifold->normal.y);
        lua_setfield(lua, -2, "y");
        lua_setfield(lua, -2, "normal");

        // table separations
        lua_newtable(lua);
        lua_pushnumber(lua, worldManifold->separations[0]);
        lua_rawseti(lua,-2, 1);

        lua_pushnumber(lua, worldManifold->separations[1]);
        lua_rawseti(lua,-2, 2);

        lua_setfield(lua, -2, "separations");

        // table world manifold point
        lua_newtable(lua);
        constexpr size_t size_array_points = sizeof(worldManifold->points) / sizeof(b2Vec2);
        for (size_t i = 0; i < size_array_points; i++)
        {
            push_manifold_point(i);
            lua_rawseti(lua,-2, i+1);
        }
        lua_setfield(lua, -2, "points");
    }    

    static void push_manifold(lua_State *lua, const b2Manifold* manifold)
    {
        auto push_manifold_point = [&] (const int index) -> void
        {
            lua_newtable(lua);// table manifold point
            lua_newtable(lua);//localPoint
            const b2ManifoldPoint & mPoint = manifold->points[index];
            lua_pushnumber(lua, mPoint.localPoint.x);
            lua_setfield(lua, -2, "x");
            lua_pushnumber(lua, mPoint.localPoint.y);
            lua_setfield(lua, -2, "y");
            lua_setfield(lua, -2, "localPoint");

            lua_pushnumber(lua, mPoint.normalImpulse);
            lua_setfield(lua, -2, "normalImpulse");

            lua_pushnumber(lua, mPoint.tangentImpulse);
            lua_setfield(lua, -2, "tangentImpulse");
            
        };
        // table manifold
        lua_newtable(lua);
        // table localNormal
        lua_newtable(lua);
        lua_pushnumber(lua, manifold->localNormal.x);
        lua_setfield(lua, -2, "x");
        lua_pushnumber(lua, manifold->localNormal.y);
        lua_setfield(lua, -2, "y");
        lua_setfield(lua, -2, "localNormal");

        lua_newtable(lua);
        lua_pushnumber(lua, manifold->localPoint.x);
        lua_setfield(lua, -2, "x");
        lua_pushnumber(lua, manifold->localPoint.y);
        lua_setfield(lua, -2, "y");
        lua_setfield(lua, -2, "localPoint");

        lua_pushinteger(lua, manifold->pointCount);
        lua_setfield(lua, -2, "pointCount");
        
        switch (manifold->type)
        {
            case b2Manifold::e_circles : lua_pushstring(lua,"circles"); break;
            case b2Manifold::e_faceA   : lua_pushstring(lua,"face_a");  break;
            case b2Manifold::e_faceB   : lua_pushstring(lua,"face_b");  break;
            default                    : lua_pushstring(lua,"unknown"); break;
        }
        lua_setfield(lua, -2, "type");

        // table manifold point
        lua_newtable(lua);
        constexpr size_t size_array_points = sizeof(manifold->points) / sizeof(b2ManifoldPoint);
        for (size_t i = 0; i < size_array_points; i++)
        {
            push_manifold_point(i);
            lua_rawseti(lua,-2, i+1);
        }
        lua_setfield(lua, -2, "points");
    }

    int onGetManifoldBox2d(lua_State *lua)
    {
        const int  top              = lua_gettop(lua);
        const b2Body *body          = getBodyBox2dFromRawTable(lua,1,2);
        const bool checkIsTouching  = top > 2 ? lua_toboolean(lua,3) : false;
        const bool checkIsEnabled   = top > 3 ? lua_toboolean(lua,4) : false;
        lua_settop(lua,0);
        lua_newtable(lua);
        const b2ContactEdge* c = body->GetContactList();
        int index = 1;
        while(c)
        {
            const b2Manifold* manifold = c->contact->GetManifold();
            if(checkIsTouching && checkIsEnabled)
            {
                if(c->contact->IsTouching() && c->contact->IsEnabled())
                {
                    push_manifold(lua,manifold);
                    lua_rawseti(lua,-2, index++);
                }
            }
            else if(checkIsTouching)
            {
                if(c->contact->IsTouching())
                {
                    push_manifold(lua,manifold);
                    lua_rawseti(lua,-2, index++);
                }
            }
            else if(checkIsEnabled)
            {
                if(c->contact->IsEnabled())
                {
                    push_manifold(lua,manifold);
                    lua_rawseti(lua,-2, index++);
                }
            }
            else
            {
                push_manifold(lua,manifold);
                lua_rawseti(lua,-2, index++);
            }
            c = c->next;
        }
        return 1;
    }

    int onGetWorldManifoldBox2d(lua_State *lua)
    {
        const b2Body *body          = getBodyBox2dFromRawTable(lua,1,2);
        lua_settop(lua,0);
        lua_newtable(lua);
        const b2ContactEdge* c = body->GetContactList();
        int index = 1;
        
        while(c)
        {
            b2WorldManifold worldManifold;
            c->contact->GetWorldManifold(&worldManifold);
            push_world_manifold(lua,&worldManifold);
            lua_rawseti(lua,-2, index++);
            c = c->next;
        }
        return 1;
    }

    int onSetManifoldBox2d(lua_State *lua)
    {
        const int  top          = lua_gettop(lua);
        const b2Body *body      = getBodyBox2dFromRawTable(lua,1,2);
        const int indexTable    = 3;

        if(lua_type(lua,indexTable) == LUA_TTABLE)
        {
            const std::size_t lenTable = lua_rawlen(lua, indexTable);
            std::size_t lenManifolds   = 0;
            const b2ContactEdge* c     = body->GetContactList();
            while(c)
            {
                lenManifolds += 1;
                c = c->next;
            }

            if(lenTable != lenManifolds)
            {
                return plugin_helper::lua_error_debug(lua, "expected size of manifold [%d] to be same size as in the ContactList [%d]",lenManifolds,lenTable);
            }

            int index = 1;
            c         = body->GetContactList();

            while(c)
            {
                std::string strType;
                b2Manifold* manifold = c->contact->GetManifold();
                lua_rawgeti(lua, indexTable, index); // next manifold, put in top + 1

                plugin_helper::getFieldPrimaryFromTable(lua, top + 1, "type", LUA_TSTRING, &strType);
                if(strType.compare("circles") == 0 )
                {
                    manifold->type = b2Manifold::e_circles;
                }
                else if(strType.compare("face_a") == 0 )
                {
                    manifold->type = b2Manifold::e_faceA;
                }
                else if(strType.compare("face_b") == 0 )
                {
                    manifold->type = b2Manifold::e_faceB;
                }
                else if(strType.compare("unknown") == 0 )
                {
                    // Do nothing, maybe there is no point (pointCount) ???
                }
                else
                {
                    return plugin_helper::lua_error_debug(lua, "type of <manifold> index [%d] unknown [%s] \n expected 'circles', 'face_a' or 'face_b' ",index,strType.c_str());
                }

                float pointCount = static_cast<float>(manifold->pointCount);
                plugin_helper::getFieldPrimaryFromTable(lua, top + 1, "pointCount", LUA_TNUMBER, &pointCount);
                manifold->pointCount = static_cast<int32>(pointCount);

                lua_getfield(lua, top + 1, "localNormal");
                if(lua_type(lua,top + 2) == LUA_TTABLE)
                {
                    plugin_helper::getFieldPrimaryFromTable(lua, top + 2, "x", LUA_TNUMBER, &manifold->localNormal.x);
                    plugin_helper::getFieldPrimaryFromTable(lua, top + 2, "y", LUA_TNUMBER, &manifold->localNormal.y);
                }
                else
                {
                    return plugin_helper::lua_error_debug(lua, "Expected table <localNormal> in <manifold> index [%d]  ",index);
                }
                lua_pop(lua, 1);

                lua_getfield(lua, top + 1, "localPoint");
                if(lua_type(lua,top + 2) == LUA_TTABLE)
                {
                    plugin_helper::getFieldPrimaryFromTable(lua, top + 2, "x", LUA_TNUMBER, &manifold->localPoint.x);
                    plugin_helper::getFieldPrimaryFromTable(lua, top + 2, "y", LUA_TNUMBER, &manifold->localPoint.y);
                }
                else
                {
                    return plugin_helper::lua_error_debug(lua, "Expected table <localPoint> in <manifold> index [%d]  ",index);
                }
                lua_pop(lua, 1);

                lua_getfield(lua, top + 1, "points");
                if(lua_type(lua,top + 2) == LUA_TTABLE)
                {
                    constexpr size_t size_array_points = sizeof(manifold->points) / sizeof(b2ManifoldPoint);
                    for (size_t i = 0; i < size_array_points; i++)
                    {
                        
                        lua_rawgeti(lua,top + 2, i + 1);

                        if(lua_type(lua,top + 3) == LUA_TTABLE)
                        {
                            lua_getfield(lua, top + 3, "localPoint");
                            if(lua_type(lua,top + 4) == LUA_TTABLE)
                            {
                                plugin_helper::getFieldPrimaryFromTable(lua, top + 4, "x", LUA_TNUMBER, &manifold->points[i].localPoint.x);
                                plugin_helper::getFieldPrimaryFromTable(lua, top + 4, "y", LUA_TNUMBER, &manifold->points[i].localPoint.y);
                            }
                            else
                            {
                                return plugin_helper::lua_error_debug(lua, "Expected table <localPoint> in <manifold> index [%d]  ",index);
                            }
                            lua_pop(lua, 1);

                            plugin_helper::getFieldPrimaryFromTable(lua, top + 3, "normalImpulse", LUA_TNUMBER,  &manifold->points[i].normalImpulse);
                            plugin_helper::getFieldPrimaryFromTable(lua, top + 3, "tangentImpulse", LUA_TNUMBER, &manifold->points[i].tangentImpulse);
                        }
                        else
                        {
                            return plugin_helper::lua_error_debug(lua, "<points> index [1] is not a table. index manifold:[%d]",index);
                        }
                        lua_pop(lua, 1);
                    }
                }
                else
                {
                    return plugin_helper::lua_error_debug(lua, "Expected table <points> in <manifold> index [%d][1]  ",index);
                }
                lua_pop(lua, 2);//points  & previous manifold
                c = c->next;
                index++;
            }
        }
        else
        {
            return plugin_helper::lua_error_debug(lua, "expected table <mesh> <manifolds>. \n use getManifolds() function to get it.");
        }
        return 0;
    }

    int onGetPositionBox2d(lua_State *lua)
    {
        const b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        const b2Vec2  b(body->GetPosition()); //-V522
        lua_pushnumber(lua, b.x);
        lua_pushnumber(lua, b.y);
        return 2;
    }

    int onApplyTorqueBodyBox2d(lua_State *lua)
    {
        const int             top     = lua_gettop(lua);
        b2Body *body                  = getBodyBox2dFromRawTable(lua,1,2);
        const float           torque  = luaL_checknumber(lua, 3);
        const bool awake              = top > 3 ? lua_toboolean(lua,4) : true;
        body->ApplyTorque(torque,awake); //-V522
        return 0;
    }

    int onApplyLinearImpulseBodyBox2d(lua_State *lua)
    {
        const int             top   = lua_gettop(lua);
        b2Body *body                = getBodyBox2dFromRawTable(lua,1,2);
        const float           x     = luaL_checknumber(lua, 3);
        const float           y     = luaL_checknumber(lua, 4);
        if (top > 4)
        {
            const float wx = luaL_checknumber(lua, 5);
            const float wy = luaL_checknumber(lua, 6);
            body->ApplyLinearImpulse(b2Vec2(x,y), b2Vec2(wx,wy), true);
        }
        else
        {
            body->ApplyLinearImpulseToCenter(b2Vec2(x,y), true);
        }
        return 0;
    }
    
    int onApplyLinearImpulseToCenterBodyBox2d(lua_State *lua)
    {
        b2Body *body                = getBodyBox2dFromRawTable(lua,1,2);
        const float           x     = luaL_checknumber(lua, 3);
        const float           y     = luaL_checknumber(lua, 4);
        body->ApplyLinearImpulseToCenter(b2Vec2(x,y), true);
        return 0;
    }

    int onApplyAngularImpulseBodyBox2d(lua_State *lua)
    {
        const int top       = lua_gettop(lua);
        b2Body *body        = getBodyBox2dFromRawTable(lua,1,2);
        const float impulse = luaL_checknumber(lua, 3);
        const bool awake    = top > 3 ? lua_toboolean(lua,4) : true;
        body->ApplyAngularImpulse(impulse,awake); //-V522
        return 0;
    }

    int onIsOnTheGroundBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d     = getBox2dFromRawTable(lua, 1, 1);
        SHAPE_INFO *          infoBox2d = getShapeInfoFromRawTable(lua,1,2);
        lua_pushboolean(lua, box2d->isOnTheGround(infoBox2d));
        return 1;
    }

    int onSetFrictionBox2d(lua_State *lua)
    {
        const int             top       = lua_gettop(lua);
        PHYSICS_BOX2D *       box2d     = getBox2dFromRawTable(lua, 1, 1);
        SHAPE_INFO *          infoBox2d = getShapeInfoFromRawTable(lua,1,2);
        const float           friction  = luaL_checknumber(lua, 3);
        const bool update_contact_list  = top > 3 ? lua_toboolean(lua,4) : true;
        box2d->setFriction(infoBox2d, friction, update_contact_list);
        return 0;
    }

    int onSetRestitutionBox2d(lua_State *lua)
    {
        const int             top           = lua_gettop(lua);
        PHYSICS_BOX2D *       box2d         = getBox2dFromRawTable(lua, 1, 1);
        SHAPE_INFO *          infoBox2d     = getShapeInfoFromRawTable(lua,1,2);
        const float           restitution   = luaL_checknumber(lua, 3);
        const bool update_contact_list      = top > 3 ? lua_toboolean(lua,4) : true;
        box2d->setRestitution(infoBox2d, restitution, update_contact_list);
        return 0;
    }

    int onSetTypeBodyBox2d(lua_State *lua)
    {
        b2Body *  body = getBodyBox2dFromRawTable(lua,1,2);
        const int typeObj = lua_type(lua, 2);
        if (typeObj != LUA_TNUMBER && typeObj != LUA_TSTRING)
        {
            return plugin_helper::lua_error_debug(lua,"\nType expected as string \n %s ", 
                    "0 - 'static'\n"
                    "1 - 'kinematic'\n"
                    "2 - 'dynamic'\n");
        }
        if (typeObj == LUA_TNUMBER)
        {
            const unsigned int t = lua_tointeger(lua, 2);
            if (t > 2)
            {
                return plugin_helper::lua_error_debug(lua, "\nType expected as string or number  \n %s got [%d]", 
                    "0 - 'static'\n"
                    "1 - 'kinematic'\n"
                    "2 - 'dynamic'\n",t);
            }
            else
            {
                body->SetType((b2BodyType)t); //-V522
            }
        }
        else
        {
            const char *str = lua_tostring(lua, 2);
            if (str && strcasecmp(str, "static") == 0)
                body->SetType(b2_staticBody);
            else if (str && strcasecmp(str, "kinematic") == 0)
                body->SetType(b2_kinematicBody);
            else if (str && strcasecmp(str, "dynamic") == 0)
                body->SetType(b2_dynamicBody);
            else
            {
                return plugin_helper::lua_error_debug(lua, "\nType expected as string \n %s got [%s]", 
                    "0 - 'static'\n"
                    "1 - 'kinematic'\n"
                    "2 - 'dynamic'\n",str ? str : "nil");
            }
        }
        return 0;
    }

    int onSetMassBox2d(lua_State *lua)
    {
        b2Body *body            = getBodyBox2dFromRawTable(lua,1,2);
        const float   mass      = luaL_checknumber(lua, 3);
        b2MassData b2Mass;
        body->GetMassData(&b2Mass); //-V522
        b2Mass.mass = mass;
        body->SetMassData(&b2Mass);
        return 0;
    }

    int onSetDensityBox2d(lua_State *lua)
    {
        const int             top       = lua_gettop(lua);
        PHYSICS_BOX2D *       box2d     = getBox2dFromRawTable(lua, 1, 1);
        SHAPE_INFO *          infoBox2d = getShapeInfoFromRawTable(lua,1,2);
        const float           density   = luaL_checknumber(lua, 3);
        const bool           reset_mass = top > 3 ? lua_toboolean(lua,4) : true;
        box2d->setDensity(infoBox2d, density, reset_mass);
        return 0;
    }

    int onInterfereBox2d(lua_State *lua)
    {
        const int             top       = lua_gettop(lua);
        PHYSICS_BOX2D *       box2d     = getBox2dFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr       = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData                  = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *infoBox2d                 = static_cast<SHAPE_INFO *>(userData->extra);
        const float           x         = top > 2 ? luaL_checknumber(lua, 3) : ptr->position.x;
        const float           y         = top > 3 ? luaL_checknumber(lua, 4) : ptr->position.y;
        const float           z         = top > 4 ? luaL_checknumber(lua, 5) : ptr->angle.z;
        const VEC2            newPos(x, y);
        if (infoBox2d)
            box2d->interference(infoBox2d->body, &newPos, z);
        return 0;
    }

    int onIsActiveBodyBox2d(lua_State *lua)
    {
        const b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        if (body->IsActive()) //-V522
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onSetAwakeBox2d(lua_State *lua)
    {
        b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        const bool            bValue    = lua_toboolean(lua, 3) ? true : false;
        body->SetAwake(bValue); //-V522
        return 0;
    }

    int onIsAwakeBox2d(lua_State *lua)
    {
        const b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        lua_pushboolean(lua, body->IsAwake()); //-V522
        return 1;
    }

    int onSetBulletBox2d(lua_State *lua)
    {
        b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        const bool bValue    = lua_toboolean(lua, 3) ? true : false;
        body->SetBullet(bValue); //-V522
        return 0;
    }

    int onSetEnabledBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d     = getBox2dFromRawTable(lua, 1, 1);
        SHAPE_INFO *          infoBox2d = getShapeInfoFromRawTable(lua,1,2);
        const bool            bValue    = lua_toboolean(lua, 3) ? true : false;
        box2d->setEnabled(infoBox2d, bValue);
        return 0;
    }

    int onSetActiveCollisionBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d     = getBox2dFromRawTable(lua, 1, 1);
        SHAPE_INFO *          infoBox2d = getShapeInfoFromRawTable(lua,1,2);
        const bool            bValue    = lua_toboolean(lua, 3) ? true : false;
        box2d->setActive(infoBox2d, bValue);
        return 0;
    }

    /*
            Step
            Step
            BeginContact
            PreSolve
            PostSolve
            Step
            PreSolve
            PostSolve
            Step
            PreSolve
            PostSolve
            Step
            EndContact
            Step
            Step
            ...

    Outcome:
    PreSolve and PostSolve are called repeatedly
    */
    int onSetContactListenerBox2d(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            PHYSICS_BOX2D *       box2d = getBox2dFromRawTable(lua, 1, 1);
            USER_DATA_PHYSICS_2D *uData = box2d->userData;
            
            if (top > 1 && lua_type(lua,2) == LUA_TFUNCTION)
                uData->refTableLua(lua, 2, &uData->ref_BeginContact);
            else
                uData->unrefTableLua(lua,&uData->ref_BeginContact);

            if (top > 2 && lua_type(lua,3) == LUA_TFUNCTION)
                uData->refTableLua(lua, 3, &uData->ref_EndContact);
            else
                uData->unrefTableLua(lua,&uData->ref_EndContact);
            
            if (top > 3 && lua_type(lua,4) == LUA_TFUNCTION)
                uData->refTableLua(lua, 4, &uData->ref_PreSolve);
            else
                uData->unrefTableLua(lua,&uData->ref_PreSolve);

            if (top > 4 && lua_type(lua,5) == LUA_TFUNCTION)
                uData->refTableLua(lua, 5, &uData->ref_PostSolve);
            else
                uData->unrefTableLua(lua,&uData->ref_PostSolve);

            box2d->setContactListener(box2d);
        }
        else
        {
            return plugin_helper::lua_error_debug(lua, "\nExpected at least one callback function:\n%s\n", 
                                                                                "onBeginContact(tMesh_a,tMesh_b)\n"
                                                                                "onEndContact(tMesh_a,  tMesh_b)\n"
                                                                                "onPreSolve(tMesh_a,    tMesh_b,   tOldManifold)\n"
                                                                                "onPostSolve(tMesh_a,   tMesh_b,   tImpulse)");
        }
        return 0;
    }

    int onSetAngularDumpingBox2d(lua_State *lua)
    {
        b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        const float angularDamping = luaL_checknumber(lua, 3);
        body->SetAngularDamping(angularDamping); //-V522
        return 0;
    }

    int onGetScaleBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d          = getBox2dFromRawTable(lua, 1, 1);
        lua_pushnumber(lua, box2d->getScale());
        return 1;
    }

    int onSetScaleBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d = getBox2dFromRawTable(lua, 1, 1);
        const float scale = luaL_checknumber(lua, 2);
        box2d->setScale(scale);
        return 0;
    }

    int onGetMultiplyBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d          = getBox2dFromRawTable(lua, 1, 1);
        lua_pushnumber(lua, box2d->multiplyStep);
        return 1;
    }

    static int onGetVelocityIterationsBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d          = getBox2dFromRawTable(lua, 1, 1);
        lua_pushinteger(lua, box2d->velocityIterations);
        return 1;
    }

    static int onGetPositionIterationsBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d          = getBox2dFromRawTable(lua, 1, 1);
        lua_pushinteger(lua, box2d->positionIterations);
        return 1;
    }

    int onSetMultiplyBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d = getBox2dFromRawTable(lua, 1, 1);
        const float multiplyStep = luaL_checknumber(lua, 2);
        box2d->multiplyStep = multiplyStep;
        return 0;
    }

    int onGetLocalCenterBodyBox2d(lua_State *lua)
    {
        const b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        const b2Vec2  b(body->GetLocalCenter()); //-V522
        lua_pushnumber(lua, b.x);
        lua_pushnumber(lua, b.y);
        return 2;
    }

    int onGetLocalPointBodyBox2d(lua_State *lua)
    {
        const b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        const float x = luaL_checknumber(lua, 3);
        const float y = luaL_checknumber(lua, 4);
        const b2Vec2      b(body->GetLocalPoint(b2Vec2(x, y))); //-V522
        lua_pushnumber(lua, b.x);
        lua_pushnumber(lua, b.y);
        return 2;
    }

    int onGetTypeBodyBox2d(lua_State *lua)
    {
        const b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        switch (body->GetType()) //-V522
        {
        case b2_staticBody:
        {
            lua_pushstring(lua, "static");
        }
        break;
        case b2_kinematicBody:
        {
            lua_pushstring(lua, "kinematic");
        }
        break;
        case b2_dynamicBody:
        {
            lua_pushstring(lua, "dynamic");
        }
        break;
        }
        return 1;
    }

    int onGetWorldCenterBox2d(lua_State *lua)
    {
        const b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        const b2Vec2  b(body->GetWorldCenter()); //-V522
        lua_pushnumber(lua, b.x);
        lua_pushnumber(lua, b.y);
        return 2;
    }

    int onGetWorldPointBox2d(lua_State *lua)
    {
        const b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        const float x    = luaL_checknumber(lua, 3);
        const float y    = luaL_checknumber(lua, 4);
        const b2Vec2      b(body->GetWorldPoint(b2Vec2(x, y))); //-V522
        lua_pushnumber(lua, b.x);
        lua_pushnumber(lua, b.y);
        return 2;
    }

    int getVersionBox2d(lua_State *lua)
    {
        lua_pushfstring(lua,"%d.%d.%d %s",b2_version.major,b2_version.minor,b2_version.revision,b2_liquidFunVersionString);
        return 1;
    }

    int onGetWorldVectorBodyBox2d(lua_State *lua)
    {
        const b2Body *body = getBodyBox2dFromRawTable(lua,1,2);
        const float x = luaL_checknumber(lua, 3);
        const float y = luaL_checknumber(lua, 4);
        const b2Vec2      b(body->GetWorldVector(b2Vec2(x, y))); //-V522
        lua_pushnumber(lua, b.x);
        lua_pushnumber(lua, b.y);
        return 2;
    }

    struct USER_DATA_B2_CALL_BACK_LUA : public REF_FUNCTION_LUA
    {
        int   ref_CallBack;
        lua_State * lua;
        USER_DATA_B2_CALL_BACK_LUA(lua_State * _lua):
        ref_CallBack(LUA_NOREF),
        lua(_lua)
        {}
        virtual ~USER_DATA_B2_CALL_BACK_LUA()
        {
            unrefAllTableLua(lua);
        }
        void unrefAllTableLua(lua_State *lua)
        {
            this->unrefTableLua(lua,&this->ref_CallBack);
        }
    };

    class BOX2D_RAY_CAST : public b2RayCastCallback
    {
    public:
        USER_DATA_B2_CALL_BACK_LUA user_data_box2d;
        PHYSICS_BOX2D * p_box2d;
        
        BOX2D_RAY_CAST(lua_State * lua,PHYSICS_BOX2D * pbox2d):
        user_data_box2d(lua),
        p_box2d(pbox2d)
        {
        }

        virtual float32 ReportFixture(	b2Fixture* fixture, const b2Vec2& point,const b2Vec2& normal, float32 fraction)
        {
            auto* info = static_cast<SHAPE_INFO*>(fixture->GetBody()->GetUserData());
            if(info)
            {
                auto * userData              = static_cast<USER_DATA_RENDER_LUA *>(info->ptr->userData);
                lua_State * lua              = user_data_box2d.lua;
                const float scale            = p_box2d->getScale();
                lua_rawgeti(lua, LUA_REGISTRYINDEX, user_data_box2d.ref_CallBack);
                if (userData->ref_MeAsTable != LUA_NOREF && lua_isfunction(lua, -1))
                {
                    lua_rawgeti(lua, LUA_REGISTRYINDEX, userData->ref_MeAsTable);
                    lua_pushnumber(lua, point.x * scale);
                    lua_pushnumber(lua, point.y * scale);
                    lua_pushnumber(lua, normal.x);
                    lua_pushnumber(lua, normal.y);
                    lua_pushnumber(lua, fraction);
                    if (lua_pcall(lua, 6, 1, 0))
                    {
                        plugin_helper::lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
                    }
                    else
                    {
                        if (lua_type(lua, -1) != LUA_TNUMBER)
                            return fraction;
                        fraction = lua_tonumber(lua, -1);
                        return fraction;
                    }
                }
            }
            return fraction;
        }

        virtual float32 ReportParticle(const b2ParticleSystem* ,int32, const b2Vec2& ,const b2Vec2&,float32)
        {
            return 0;
        }

        virtual bool ShouldQueryParticleSystem(const b2ParticleSystem* )
        {
		    return false;
	    }
    };

    int onRayCastBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *box2d = getBox2dFromRawTable(lua, 1, 1);
        const float scale    = box2d->getScale();
        const b2Vec2 p1(luaL_checknumber(lua, 2) / scale, luaL_checknumber(lua, 3) / scale);
        const b2Vec2 p2(luaL_checknumber(lua, 4) / scale, luaL_checknumber(lua, 5) / scale);
        const b2Vec2 t(p1 - p2);
        if (t.Length() != 0.0f) //-V550
        {
            BOX2D_RAY_CAST box2d_ray_cast(lua,box2d);
            box2d_ray_cast.user_data_box2d.refFunctionLua(lua,6,&box2d_ray_cast.user_data_box2d.ref_CallBack);
            box2d->rayCast(p1, p2, &box2d_ray_cast);
        }
    #if _DEBUG
        else
        {
            PRINT_IF_DEBUG("x1,y1 and x2,y2 must be different point");
        }
    #endif
        return 0;
    }

    class BOX2D_QUERY_AABB : public b2QueryCallback
    {
    public:
        USER_DATA_B2_CALL_BACK_LUA user_data_box2d;
        PHYSICS_BOX2D * p_box2d;
        
        BOX2D_QUERY_AABB(lua_State * lua,PHYSICS_BOX2D * pbox2d):
        user_data_box2d(lua),
        p_box2d(pbox2d)
        {
        }

        virtual bool ReportFixture(b2Fixture* fixture)
        {
            auto* info = static_cast<SHAPE_INFO*>(fixture->GetBody()->GetUserData());
            if(info)
            {
                auto * userData              = static_cast<USER_DATA_RENDER_LUA *>(info->ptr->userData);
                lua_State * lua              = user_data_box2d.lua;
                lua_rawgeti(lua, LUA_REGISTRYINDEX, user_data_box2d.ref_CallBack);
                if (userData->ref_MeAsTable != LUA_NOREF && lua_isfunction(lua, -1))
                {
                    lua_rawgeti(lua, LUA_REGISTRYINDEX, userData->ref_MeAsTable);
                    if (lua_pcall(lua, 1, 1, 0))
                    {
                        plugin_helper::lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
                    }
                    else
                    {
                        if (lua_type(lua, -1) != LUA_TBOOLEAN)
                            return false;
                        const bool ret = lua_toboolean(lua, -1);
                        return ret;
                    }
                }
            }
            return false;
        }
        virtual float32 ReportParticle(const b2ParticleSystem* ,int32 , const b2Vec2& ,const b2Vec2& , float32 )
        {
            return 0;
        }

        virtual bool ShouldQueryParticleSystem(const b2ParticleSystem*)
        {
            return false;
        }
    };

    int onQueryAABBBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *box2d = getBox2dFromRawTable(lua, 1, 1);
        const float scale    = box2d->getScale();
        b2AABB  b2aabb;
        b2aabb.lowerBound.x         = luaL_checknumber(lua, 2)  / scale;
        b2aabb.lowerBound.y         = luaL_checknumber(lua, 3)  / scale;
        b2aabb.upperBound.x         = luaL_checknumber(lua, 4)  / scale;
        b2aabb.upperBound.y         = luaL_checknumber(lua, 5)  / scale;
        BOX2D_QUERY_AABB box2d_queryAABB(lua,box2d);
        box2d_queryAABB.user_data_box2d.refFunctionLua(lua,6,&box2d_queryAABB.user_data_box2d.ref_CallBack);
        box2d->queryAABB(b2aabb, &box2d_queryAABB);
        return 0;
    }

    int onGetJointBox2d(lua_State *lua)
    {
        const int     top               = lua_gettop(lua);
        PHYSICS_BOX2D *       box2d     = getBox2dFromRawTable(lua, 1, 1);
        SHAPE_INFO *          infoBox2d = getShapeInfoFromRawTable(lua,1,2);
        const unsigned int index        = top > 2 ? luaL_checkinteger(lua,3) - 1: 0;
        INFO_JOINT *          infoJoint = box2d->getInfoJoint(infoBox2d,index);
        if (infoJoint)
            return onGetJointLua(lua, infoJoint->joint);
        else
            lua_pushnil(lua);
        return 1;
    }

    int onStopSimulateBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *box2d = getBox2dFromRawTable(lua, 1, 1);
        box2d->stopSimulate  = true;
        return 0;
    }

    int onResumeSimulateBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *box2d = getBox2dFromRawTable(lua, 1, 1);
        box2d->stopSimulate = false;
        return 0;
    }

    int onDestroyBodyBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d     = getBox2dFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr       = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData                  = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *          infoBox2d       = static_cast<SHAPE_INFO *>(userData->extra);
        if(infoBox2d)
            box2d->destroyBody(infoBox2d);
        return 0;
    }

    int onDestroyJointBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d     = getBox2dFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr       = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData                  = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto *          infoBox2d       = static_cast<SHAPE_INFO *>(userData->extra);
        if(infoBox2d)
            box2d->removeJoint(infoBox2d);
        return 0;
    }

    
    int onDestroyFluidBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d     = getBox2dFromRawTable(lua, 1, 1);
        RENDERIZABLE *        ptr       = plugin_helper::getRenderizableFromRawTable(lua, 1, 2);
        auto *userData                  = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        auto * infoFluid                = static_cast<INFO_FLUID*>(userData->extra);
        if(infoFluid)
            box2d->destroyFluid(infoFluid);
        return 0;
    }

    int onTestPointBodyBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d     = getBox2dFromRawTable(lua, 1, 1);
        SHAPE_INFO *          infoBox2d = getShapeInfoFromRawTable(lua,1,2);
        b2Vec2 point(luaL_checknumber(lua, 3), luaL_checknumber(lua, 4));
        if (box2d->testPoint(infoBox2d, point))
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onSetFixedRotationBox2d  (lua_State *lua)
    {
        PHYSICS_BOX2D *box2d    = getBox2dFromRawTable(lua, 1, 1);
        SHAPE_INFO *  infoBox2d = getShapeInfoFromRawTable(lua,1,2);
        const int      value    = lua_toboolean(lua, 3);
        box2d->setFixedRotation(infoBox2d,value);
        return 0;
    }

    int onSetSleepingAllowedBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *box2d    = getBox2dFromRawTable(lua, 1, 1);
        SHAPE_INFO *  infoBox2d = getShapeInfoFromRawTable(lua,1,2);
        const int      value    = lua_toboolean(lua, 3);
        box2d->setSleepingAllowed(infoBox2d,value);
        return 0;
    }

    int onSetFilterBox2d(lua_State *lua)
    {
        const int     top         = lua_gettop(lua);
        PHYSICS_BOX2D *box2d      = getBox2dFromRawTable(lua, 1, 1);
        const char* message_error = 
        "\nExpected [body] for this case it will apply for all bodies."
        "\nExpected [table filter] box2d and [body], for this case it will apply this body. "
        "\nExpected table filter box2d like this:\n%s"
        "{ categoryBits   = 0x0001, -- I am \n"
        "  maskBits       = 0xFFFF, -- Collide with\n"
        "  groupIndex     = 0 } -- Group collision (override collision).\n"
        "\nsee documentation box2d c++\n";

        if(top > 2)
        {
            SHAPE_INFO *  infoBox2d = getShapeInfoFromRawTable(lua,1,2);
            const int      hasTable = lua_type(lua, 3);
            if (hasTable == LUA_TTABLE)
            {
                b2Filter              filter;
                plugin_helper::getFieldUnsignedShortFromTable(lua, 3, "categoryBits",  &filter.categoryBits);
                plugin_helper::getFieldUnsignedShortFromTable(lua, 3, "maskBits",      &filter.maskBits);
                plugin_helper::getFieldSignedShortFromTable(lua, 3,   "groupIndex",    &filter.groupIndex);
                box2d->setFilter(infoBox2d, filter);
            }
            else
            {
                return plugin_helper::lua_error_debug(lua, message_error);
            }
        }
        else if(top == 2)
        {
            const int      hasTable = lua_type(lua, 2);
            if (hasTable == LUA_TTABLE)
            {
                b2Filter              filter;
                plugin_helper::getFieldUnsignedShortFromTable(lua, 2, "categoryBits",  &filter.categoryBits);
                plugin_helper::getFieldUnsignedShortFromTable(lua, 2, "maskBits",      &filter.maskBits);
                plugin_helper::getFieldSignedShortFromTable(lua, 2,   "groupIndex",    &filter.groupIndex);
                box2d->setFilter(nullptr, filter);
            }
            else
            {
                return plugin_helper::lua_error_debug(lua, message_error);
            }
        }
        else
        {
            return plugin_helper::lua_error_debug(lua, message_error);
        }
        return 0;
    }

    int onCreateJointBox2d(lua_State *lua)
    {
        const int             top       = lua_gettop(lua);
        PHYSICS_BOX2D *       box2d     = getBox2dFromRawTable(lua, 1, 1);
        SHAPE_INFO *          info1     = getShapeInfoFromRawTable(lua,1,2);
        SHAPE_INFO *          info2     = getShapeInfoFromRawTable(lua,1,3);
        const int             hasTable  = top > 3 ? lua_type(lua, 4) : 0;
        
        unsigned int result = 0xffffffff;
        if (info1 == nullptr || info2 == nullptr)
        {
            return plugin_helper::lua_error_debug(lua, "\nexpected box2d body");
        }
        if (hasTable == LUA_TTABLE)
        {
            lua_getfield(lua, 4, "name");
            const char *name = lua_type(lua, 5) == LUA_TSTRING ? lua_tostring(lua, 5) : nullptr;
            if (name == nullptr)
            {
                return plugin_helper::lua_error_debug(lua, "\nExpected field 'name', where:\n%s", 
                                                                       "distance    \n"
                                                                       "friction    \n"
                                                                       "line        \n"
                                                                       "motor       \n"
                                                                       "mouse       \n"
                                                                       "prismatic   \n"
                                                                       "pulley      \n"
                                                                       "revolute    \n"
                                                                       "rope        \n"
                                                                       "weld        \n"
                                                                       "wheel       \n"
                                                                       );
            }
            if(info1 == nullptr || info1->body == nullptr)
                return plugin_helper::lua_error_debug(lua,"Renderizable 1 has no body. Create a body first!");
            if(info2 == nullptr || info2->body == nullptr)
                return plugin_helper::lua_error_debug(lua,"Renderizable 2 has no body. Create a body first!");

            if(strcasecmp(name,"gear") == 0) //done
            {
                b2GearJointDef def;
                float indexA = 1;
                float indexB = 1;
                plugin_helper::getFieldPrimaryFromTable(       lua,4,  "collideConnected",  LUA_TBOOLEAN,   &def.collideConnected);
                plugin_helper::getFieldPrimaryFromTable(       lua,4,  "ratio",             LUA_TNUMBER,    &def.ratio);
                plugin_helper::getFieldPrimaryFromTable(       lua,4,  "indexA",            LUA_TNUMBER,    &indexA);
                plugin_helper::getFieldPrimaryFromTable(       lua,4,  "indexB",            LUA_TNUMBER,    &indexB);
                plugin_helper::getFieldPrimaryFromTable(       lua,4,  "ratio",             LUA_TNUMBER,    &def.ratio);

                const unsigned int index_1 = static_cast<unsigned int>(indexA) - 1;
                const unsigned int index_2 = static_cast<unsigned int>(indexB) - 1;
                INFO_JOINT * infoJoint = box2d->getInfoJoint(info1,index_1);
                def.joint1             = infoJoint ? infoJoint->joint : nullptr;
                             infoJoint = box2d->getInfoJoint(info2,index_2);
                def.joint2             = infoJoint ? infoJoint->joint : nullptr;
                if(def.joint1 == nullptr)
                {
                    return plugin_helper::lua_error_debug(lua, "\nBody 1 has no joint with indexA [%d]",static_cast<unsigned int>(indexA));
                }
                if(def.joint2 == nullptr)
                {
                    return plugin_helper::lua_error_debug(lua, "\nBody 2 has no joint with indexB [%d]",static_cast<unsigned int>(indexB));
                }
                //has no member Initialize
                result = box2d->createJoint(info1,info2,def);
            }
            else if (strcasecmp(name, "motor") == 0) // done
            {
                b2MotorJointDef def;
                def.Initialize(info1->body, info2->body);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "collideConnected",    LUA_TBOOLEAN, &def.collideConnected);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "correctionFactor",    LUA_TNUMBER,  &def.correctionFactor);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4,  "linearOffset","x", "y",             &def.linearOffset.x, &def.linearOffset.y);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "maxForce",            LUA_TNUMBER,  &def.maxForce);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "maxTorque",           LUA_TNUMBER,  &def.maxTorque);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "angularOffset",       LUA_TNUMBER,  &def.angularOffset);
                result = box2d->createJoint(info1, info2, def);
            }
            else if (strcasecmp(name, "rope") == 0) // done
            {
                b2RopeJointDef def;
                      b2Vec2 p1 (info1->body->GetPosition());
                const b2Vec2 p2 (info2->body->GetPosition());
                //has no member Initialize
                p1 -= p2;
                def.maxLength = p1.Length();
                plugin_helper::getFieldPrimaryFromTable(lua,     4, "collideConnected",       LUA_TBOOLEAN, &def.collideConnected);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "localAnchorA", "x", "y",               &def.localAnchorA.x, &def.localAnchorA.y);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "localAnchorB", "x", "y",               &def.localAnchorB.x, &def.localAnchorB.y);
                plugin_helper::getFieldPrimaryFromTable(lua,     4, "maxLength",              LUA_TNUMBER,  &def.maxLength);
                result = box2d->createJoint(info1, info2, def);
            }
            else if (strcasecmp(name, "friction") == 0) // done
            {
                b2FrictionJointDef def;
                b2Vec2 anchor(0,0);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "anchor", "x", "y", &anchor.x, &anchor.y);
                def.Initialize(info1->body, info2->body,anchor);
                plugin_helper::getFieldPrimaryFromTable(lua,     4, "collideConnected",        LUA_TBOOLEAN,   &def.collideConnected);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "localAnchorA", "x", "y",                  &def.localAnchorA.x, &def.localAnchorA.y);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "localAnchorB", "x", "y",                  &def.localAnchorB.x, &def.localAnchorB.y);
                plugin_helper::getFieldPrimaryFromTable(lua,     4, "maxForce",                LUA_TNUMBER,    &def.maxForce);
                plugin_helper::getFieldPrimaryFromTable(lua,     4, "maxTorque",               LUA_TNUMBER,    &def.maxTorque);
                result = box2d->createJoint(info1, info2, def);
            }
            else if (strcasecmp(name, "wheel") == 0 || strcasecmp(name, "line") == 0) //old line, done
            {
                b2WheelJointDef def;
                b2Vec2 anchor(0.0f,0.0f);
                b2Vec2 axis(0.0f,1.0f);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "anchor", "x", "y", &anchor.x, &anchor.y);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "axis", "x", "y",   &axis.x,   &axis.y);
                def.Initialize(info1->body, info2->body, anchor, axis);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,     "collideConnected",            LUA_TBOOLEAN,   &def.collideConnected);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4,     "localAnchorA", "x", "y",                      &def.localAnchorA.x, &def.localAnchorA.y);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4,     "localAnchorB", "x", "y",                      &def.localAnchorB.x, &def.localAnchorB.y);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,     "dampingRatio",                LUA_TNUMBER,    &def.dampingRatio);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,     "enableMotor",                 LUA_TBOOLEAN,   &def.enableMotor);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,     "frequencyHz",                 LUA_TNUMBER,    &def.frequencyHz);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4,     "localAxisA", "x", "y",                        &def.localAxisA.x, &def.localAxisA.y);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,     "maxMotorTorque",              LUA_TNUMBER,    &def.maxMotorTorque);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,     "motorSpeed",                  LUA_TNUMBER,    &def.motorSpeed);
                result = box2d->createJoint(info1, info2, def);
            }
            else if (strcasecmp(name, "weld") == 0) // 'solda' done
            {
                b2WeldJointDef def;
                b2Vec2 anchor(info2->body->GetPosition());
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "anchor", "x", "y", &anchor.x, &anchor.y);
                def.Initialize(info1->body, info2->body,anchor);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "collideConnected",       LUA_TBOOLEAN,   &def.collideConnected);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4,  "localAnchorA", "x", "y",                 &def.localAnchorA.x, &def.localAnchorA.y);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4,  "localAnchorB", "x", "y",                 &def.localAnchorB.x, &def.localAnchorB.y);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "dampingRatio",           LUA_TNUMBER,    &def.dampingRatio);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "frequencyHz",            LUA_TNUMBER,    &def.frequencyHz);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "referenceAngle",         LUA_TNUMBER,    &def.referenceAngle);
                result = box2d->createJoint(info1, info2, def);
            }
            else if (strcasecmp(name, "mouse") == 0) // done
            {
                b2MouseJointDef def;
                //has no member Initialize
                plugin_helper::getFieldPrimaryFromTable(lua, 4,     "collideConnected",    LUA_TBOOLEAN,   &def.collideConnected);
                plugin_helper::getFieldPrimaryFromTable(lua, 4,     "dampingRatio",        LUA_TNUMBER,    &def.dampingRatio);
                plugin_helper::getFieldPrimaryFromTable(lua, 4,     "frequencyHz",         LUA_TNUMBER,    &def.frequencyHz);
                plugin_helper::getFieldPrimaryFromTable(lua, 4,     "maxForce",            LUA_TNUMBER,    &def.maxForce);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "target", "x", "y",                    &def.target.x, &def.target.y);
                result = box2d->createJoint(info1, info2, def);
            }
            else if (strcasecmp(name, "pulley") == 0) // polia, done
            {
                b2PulleyJointDef def;
                b2Vec2 anchorA(info1->body->GetPosition());
                b2Vec2 anchorB(info2->body->GetPosition());
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "anchorA", "x", "y", &anchorA.x, &anchorA.y);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "anchorB", "x", "y", &anchorB.x, &anchorB.y);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "ratio",                      LUA_TNUMBER,    &def.ratio);//default is 1
                plugin_helper::getFloat2FieldTableFromTable(lua, 4,  "groundAnchorA", "x", "y",                    &def.groundAnchorA.x, &def.groundAnchorA.y);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4,  "groundAnchorB", "x", "y",                    &def.groundAnchorB.x, &def.groundAnchorB.y);
                def.Initialize(info1->body, info2->body,def.groundAnchorA,def.groundAnchorB, anchorA,anchorB, def.ratio);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "collideConnected",           LUA_TBOOLEAN,   &def.collideConnected);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4,  "localAnchorA", "x", "y",                     &def.localAnchorA.x, &def.localAnchorA.y);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4,  "localAnchorB", "x", "y",                     &def.localAnchorB.x, &def.localAnchorB.y);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "lengthA",                    LUA_TNUMBER,    &def.lengthA);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "lengthB",                    LUA_TNUMBER,    &def.lengthB);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "ratio",                      LUA_TNUMBER,    &def.ratio);
                result = box2d->createJoint(info1, info2, def);
            }
            else if (strcasecmp(name, "distance") == 0) // done
            {
                b2DistanceJointDef def;
                b2Vec2 anchor1 (info1->body->GetPosition());
                b2Vec2 anchor2 (info2->body->GetPosition());
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "anchor1", "x", "y", &anchor1.x, &anchor1.y);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "anchor2", "x", "y", &anchor2.x, &anchor2.y);
                def.Initialize(info1->body,info2->body,anchor1,anchor2);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "collideConnected",               LUA_TBOOLEAN,   &def.collideConnected);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4,  "localAnchorA", "x", "y",                         &def.localAnchorA.x, &def.localAnchorA.y);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4,  "localAnchorB", "x", "y",                         &def.localAnchorB.x, &def.localAnchorB.y);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "dampingRatio",                   LUA_TNUMBER,    &def.dampingRatio);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "frequencyHz",                    LUA_TNUMBER,    &def.frequencyHz);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,  "length",                         LUA_TNUMBER,    &def.length);
                result = box2d->createJoint(info1, info2, def);
            }
            else if (strcasecmp(name, "prismatic") == 0) //done
            {
                b2PrismaticJointDef def;
                b2Vec2 axis(0.0f,1.0f);
                b2Vec2 anchor(0,0);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "anchor", "x", "y", &anchor.x, &anchor.y);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "axis",   "x", "y", &axis.x,   &axis.y  );
                def.Initialize(info1->body, info2->body, anchor, axis);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,    "collideConnected",       LUA_TBOOLEAN, &def.collideConnected);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,    "enableLimit",            LUA_TBOOLEAN, &def.enableLimit);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,    "enableMotor",            LUA_TBOOLEAN, &def.enableMotor);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4,    "localAnchorA", "x", "y",               &def.localAnchorA.x, &def.localAnchorA.y);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4,    "localAnchorB", "x", "y",               &def.localAnchorB.x, &def.localAnchorB.y);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4,    "localAxisA", "x", "y",                 &def.localAxisA.x, &def.localAxisA.y);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,    "lowerTranslation",       LUA_TNUMBER,  &def.lowerTranslation);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,    "maxMotorForce",          LUA_TNUMBER,  &def.maxMotorForce);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,    "motorSpeed",             LUA_TNUMBER,  &def.motorSpeed);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,    "referenceAngle",         LUA_TNUMBER,  &def.referenceAngle);
                plugin_helper::getFieldPrimaryFromTable(lua,     4,    "upperTranslation",       LUA_TNUMBER,  &def.upperTranslation);
                result = box2d->createJoint(info1, info2, def);
            }
            else if (strcasecmp(name, "revolute") == 0) // done
            {
                b2RevoluteJointDef def;
                b2Vec2 anchor(info2->body->GetPosition());
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "anchor", "x", "y", &anchor.x, &anchor.y);
                def.Initialize(info1->body, info2->body,anchor);
                plugin_helper::getFieldPrimaryFromTable(lua, 4,     "collideConnected",    LUA_TBOOLEAN,   &def.collideConnected);
                plugin_helper::getFieldPrimaryFromTable(lua, 4,     "enableLimit",         LUA_TBOOLEAN,   &def.enableLimit);
                plugin_helper::getFieldPrimaryFromTable(lua, 4,     "enableMotor",         LUA_TBOOLEAN,   &def.enableMotor);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "localAnchorA", "x", "y",              &def.localAnchorA.x, &def.localAnchorA.y);
                plugin_helper::getFloat2FieldTableFromTable(lua, 4, "localAnchorB", "x", "y",              &def.localAnchorB.x, &def.localAnchorB.y);
                plugin_helper::getFieldPrimaryFromTable(lua, 4,     "lowerAngle",          LUA_TNUMBER,    &def.lowerAngle);
                plugin_helper::getFieldPrimaryFromTable(lua, 4,     "maxMotorTorque",      LUA_TNUMBER,    &def.maxMotorTorque);
                plugin_helper::getFieldPrimaryFromTable(lua, 4,     "motorSpeed",          LUA_TNUMBER,    &def.motorSpeed);
                plugin_helper::getFieldPrimaryFromTable(lua, 4,     "referenceAngle",      LUA_TNUMBER,    &def.referenceAngle);
                plugin_helper::getFieldPrimaryFromTable(lua, 4,     "upperAngle",          LUA_TNUMBER,    &def.upperAngle);
                result = box2d->createJoint(info1, info2, def);
            }
            else
            {
                return plugin_helper::lua_error_debug(lua, "\nExpected field 'name', any of:\n%s", 
                                                                       "distance    \n"
                                                                       "friction    \n"
                                                                       "line        \n"
                                                                       "motor       \n"
                                                                       "mouse       \n"
                                                                       "prismatic   \n"
                                                                       "pulley      \n"
                                                                       "revolute    \n"
                                                                       "rope        \n"
                                                                       "weld        \n"
                                                                       "wheel       \n"
                                                                       );
            }
        }
        else
        {
            return plugin_helper::lua_error_debug(lua, "\nExpected a table joint of any of:\n%s", 
                                                                       "distance    \n"
                                                                       "friction    \n"
                                                                       "line        \n"
                                                                       "motor       \n"
                                                                       "mouse       \n"
                                                                       "prismatic   \n"
                                                                       "pulley      \n"
                                                                       "revolute    \n"
                                                                       "rope        \n"
                                                                       "weld        \n"
                                                                       "wheel       \n"
                                                                       );
        }
        if (result != 0xffffffff)
            lua_pushinteger(lua, result);
        else
            lua_pushnumber(lua, 0);
        return 1;
    }

    int onDestroyBox2dLua(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d = getBox2dFromRawTable(lua, 1, 1);
        USER_DATA_PHYSICS_2D *uData = box2d->userData;
        uData->unrefAllTableLua(lua);
        delete uData;
        box2d->userData = nullptr;
    #if DEBUG_FREE_LUA
        int v = 1;
        printf("destroying box2d %d \n", v++);
    #endif
        delete box2d;
        return 0;
    }

    int onNewBox2dLua(lua_State *lua)
    {
        const int  top = lua_gettop(lua);
        const VEC2 gravity((top > 1 ? luaL_checknumber(lua, 2) : 0.0f),
                           (top > 2 ? luaL_checknumber(lua, 3) : -90.8f));
        const auto scaleWorld         = (float)(top > 3 ? luaL_checknumber(lua,   4) : 10.0f);
        const auto   velocityIterations = (int)(top > 4 ? luaL_checkinteger(lua, 5) : 10);
        const auto   positionIterations = (int)(top > 5 ? luaL_checkinteger(lua, 6) : 3);
        const float multiplyStep        =      (top > 6 ? luaL_checknumber(lua,   7) : 1.0f);
        lua_settop(lua, 0);
        luaL_Reg regBox2dMethods[] = {  {"addStaticBody", onAddStaticBodyBox2d },
                                        {"addDynamicBody", onAddDynamicBodyBox2d },
                                        {"addKinematicBody", onAddKinematicBodyBox2d },
                                        {"createFluid", onCreateFluidBodyBox2d },
                                        {"addBody", onAddBodyBox2d },
                                        {"applyForce", onApplyForceBodyBox2d },
                                        {"applyForceToCenter", onApplyForceToCenterBodyBox2dFromBody},
                                        {"applyTorque", onApplyTorqueBodyBox2d },
                                        {"applyAngularImpulse", onApplyAngularImpulseBodyBox2d },
                                        {"applyLinearImpulse", onApplyLinearImpulseBodyBox2d },
                                        {"applyLinearImpulseToCenter", onApplyLinearImpulseToCenterBodyBox2d },
                                        {"destroyBody", onDestroyBodyBox2d},
                                        {"destroyJoint", onDestroyJointBox2d},
                                        {"destroyFluid", onDestroyFluidBox2d},
                                        {"getAngularVelocity", onGetAngularVelocityBox2d },
                                        {"getInertia", onGetInertiaBox2dFromBody},
                                        {"getJoint", onGetJointBox2d},
                                        {"getLinearVelocity", onGetLinearVelocityBox2d },
                                        {"getGravity", onGetGravityBox2d},
                                        {"getGravityScale", onGetGravityScaleBodyBox2d},
                                        {"getPosition", onGetPositionBox2d },
                                        {"getMass", onGetMassBox2d },
                                        {"getManifolds", onGetManifoldBox2d },
                                        {"getWorldManifolds", onGetWorldManifoldBox2d },
                                        {"getScale", onGetScaleBox2d},
                                        {"getLocalCenter", onGetLocalCenterBodyBox2d},
                                        {"getLocalPoint", onGetLocalPointBodyBox2d},
                                        {"getType", onGetTypeBodyBox2d},
                                        {"getWorldCenter", onGetWorldCenterBox2d },
                                        {"getWorldPoint", onGetWorldPointBox2d },
                                        {"getWorldVector", onGetWorldVectorBodyBox2d},
                                        {"setTransform", onInterfereBox2d},
                                        {"isActive", onIsActiveBodyBox2d},
                                        {"isAwake", onIsAwakeBox2d},
                                        {"isOnTheGround", onIsOnTheGroundBox2d},
                                        {"joint", onCreateJointBox2d},
                                        {"pause", onStopSimulateBox2d},
                                        {"queryAABB", onQueryAABBBox2d},
                                        {"rayCast", onRayCastBox2d},
                                        {"setActive", onSetActiveCollisionBox2d },
                                        {"setAngularVelocity", onSetAngularVelocityBox2d },
                                        {"setAngularDamping", onSetAngularDumpingBox2d},
                                        {"setAwake", onSetAwakeBox2d},
                                        {"setBullet", onSetBulletBox2d},
                                        {"setContactListener", onSetContactListenerBox2d},
                                        {"setDensity", onSetDensityBox2d},
                                        {"setEnable", onSetEnabledBox2d},
                                        {"setFilter", onSetFilterBox2d},
                                        {"setFixedRotation", onSetFixedRotationBox2d},
                                        {"setSleepingAllowed",onSetSleepingAllowedBox2d},
                                        {"setGravity", onSetGravityBox2d},
                                        {"setGravityScale", onSetGravityScaleBodyBox2d},
                                        {"setFriction", onSetFrictionBox2d},
                                        {"setMass", onSetMassBox2d},
                                        {"setManifolds", onSetManifoldBox2d },
                                        {"setLinearVelocity", onSetLinearVelocityBox2d},
                                        {"setRestitution", onSetRestitutionBox2d},
                                        {"setScale", onSetScaleBox2d },
                                        {"setMultiply", onSetMultiplyBox2d },
                                        {"getMultiply", onGetMultiplyBox2d },
                                        {"getVelocityIterations", onGetVelocityIterationsBox2d },
                                        {"getPositionIterations", onGetPositionIterationsBox2d },
                                        {"start", onResumeSimulateBox2d },
                                        {"setType", onSetTypeBodyBox2d},
                                        {"testPoint", onTestPointBodyBox2d },
                                        {nullptr, nullptr}};
        luaL_newlib(lua, regBox2dMethods);
        luaL_getmetatable(lua, "_mbmBox2d");
        lua_setmetatable(lua, -2);
        auto **udata     = static_cast<PHYSICS_BOX2D **>(lua_newuserdata(lua, sizeof(PHYSICS_BOX2D *)));
        DEVICE *   device		  = DEVICE::getInstance();
        auto  box2d				  = new PHYSICS_BOX2D(device->scene);
        box2d->userData           = new USER_DATA_PHYSICS_2D();
        *udata                    = box2d;
        box2d->setScale(scaleWorld);
        box2d->multiplyStep       = multiplyStep > 0.0f ? multiplyStep : 1.0f;
        box2d->velocityIterations = velocityIterations;
        box2d->positionIterations = positionIterations;
        box2d->init(&gravity);
        // callbacks
        box2d->on_box2d_BeginContact = lua_box2d_BeginContact;
        box2d->on_box2d_EndContact   = lua_box2d_EndContact;
        box2d->on_box2d_PostSolve    = lua_box2d_PostSolve;
        box2d->on_box2d_PreSolve     = lua_box2d_PreSolve;
        box2d->on_box2d_DestroyBodyFromList = lua_box2d_onBox2dDestroyBodyFromList;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_BOX2D);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }

    void registerClassBox2d(lua_State *lua)
    {
        luaL_Reg regBox2dMMethods[] = {{"new", onNewBox2dLua}, {"__gc", onDestroyBox2dLua}, {"getVersion", getVersionBox2d}, {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmBox2d");
        luaL_setfuncs(lua, regBox2dMMethods, 0);
        lua_setglobal(lua, "box2d");
        lua_settop(lua,0);
    }

    enum EVENT_CONTACT_B2 : short
    {

        T_B2_BeginContact = 0,
        T_B2_EndContact   = 1,
        T_B2_PreSolve     = 2,
        T_B2_PostSolve    = 3,
    };

    void lua_box2d_EventContact(PHYSICS_BOX2D *box2d, SHAPE_INFO *info1, SHAPE_INFO *info2, EVENT_CONTACT_B2 idEvent,
                                       const b2Manifold *oldManifold, const b2ContactImpulse *impulse)
    {
        if (info1 && info2)
        {
            auto *    userData1 = static_cast<USER_DATA_RENDER_LUA *>(info1->ptr->userData);
            auto *    userData2 = static_cast<USER_DATA_RENDER_LUA *>(info2->ptr->userData);
            DEVICE *             device    = DEVICE::getInstance();
            auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
            lua_State *               lua       = userScene->lua;
            USER_DATA_PHYSICS_2D *    uData     = box2d->userData;
            switch (idEvent)
            {
                case T_B2_BeginContact:
                {
                    if (uData->ref_BeginContact != LUA_NOREF)
                    {
                        lua_rawgeti(lua, LUA_REGISTRYINDEX, uData->ref_BeginContact);
                        if (userData1->ref_MeAsTable != LUA_NOREF && userData2->ref_MeAsTable != LUA_NOREF &&
                            lua_isfunction(lua, -1))
                        {
                            lua_rawgeti(lua, LUA_REGISTRYINDEX, userData1->ref_MeAsTable);
                            lua_rawgeti(lua, LUA_REGISTRYINDEX, userData2->ref_MeAsTable);
                            if (lua_pcall(lua, 2, 0, 0))
                                plugin_helper::lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
                        }
                    }
                }
                break;
                case T_B2_EndContact:
                {
                    if (uData->ref_EndContact != LUA_NOREF)
                    {
                        lua_rawgeti(lua, LUA_REGISTRYINDEX, uData->ref_EndContact);
                        if (userData1->ref_MeAsTable != LUA_NOREF && userData2->ref_MeAsTable != LUA_NOREF &&
                            lua_isfunction(lua, -1))
                        {
                            lua_rawgeti(lua, LUA_REGISTRYINDEX, userData1->ref_MeAsTable);
                            lua_rawgeti(lua, LUA_REGISTRYINDEX, userData2->ref_MeAsTable);
                            if (lua_pcall(lua, 2, 0, 0))
                                plugin_helper::lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
                        }
                    }
                }
                break;
                case T_B2_PreSolve:
                {
                    if (uData->ref_PreSolve != LUA_NOREF)
                    {
                        lua_settop(lua,0);
                        lua_rawgeti(lua, LUA_REGISTRYINDEX, uData->ref_PreSolve);
                        if (userData1->ref_MeAsTable != LUA_NOREF && userData2->ref_MeAsTable != LUA_NOREF &&
                            lua_isfunction(lua, -1))
                        { /*
                          tManifold = {
                                        type         = 'circles' or 'face_a' or 'face_b',
                                        pointCount   = 0,
                                        localNormal  = {x = 0, y = 0 },
                                        localPoint   = {x = 0, y = 0 },
                                        points       = {
                                                        [1] = {
                                                            localPoint      = {x = 0, y = 0 },
                                                            normalImpulse   = 0,
                                                            tangentImpulse  = 0
                                                         },
                                                         [2] = {
                                                            localPoint      = {x = 0, y = 0 },
                                                            normalImpulse   = 0,
                                                            tangentImpulse  = 0
                                                         },
                                        }
                          */
                            lua_rawgeti(lua, LUA_REGISTRYINDEX, userData1->ref_MeAsTable);
                            lua_rawgeti(lua, LUA_REGISTRYINDEX, userData2->ref_MeAsTable);
                            push_manifold(lua,oldManifold);
                            /*
                            **********************************
                            State of stack at
                            	-4| function |1
                            	-3|    table |2
                            	-2|    table |3
                            	-1|    table |4
                            **********************************
                            */

                            if (lua_pcall(lua, 3, 0, 0))
                                plugin_helper::lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
                        }
                    }
                }
                break;
                case T_B2_PostSolve:
                {
                    if (uData->ref_PostSolve != LUA_NOREF)
                    {
                        lua_rawgeti(lua, LUA_REGISTRYINDEX, uData->ref_PostSolve);
                        if (userData1->ref_MeAsTable != LUA_NOREF && userData2->ref_MeAsTable != LUA_NOREF && lua_isfunction(lua, -1))
                        { // tImpulse = { count  = 0,normalImpulses  = {[1] = 0, [2] = 0},tangentImpulses = {[1] = 0, [2] = 0}}
                            lua_rawgeti(lua, LUA_REGISTRYINDEX, userData1->ref_MeAsTable);
                            lua_rawgeti(lua, LUA_REGISTRYINDEX, userData2->ref_MeAsTable);

                            // table impulse
                            lua_newtable(lua);
                            // impulse->count
                            lua_pushnumber(lua,static_cast<lua_Number>(impulse->count));
                            lua_setfield(lua, -2, "count");

                            // impulse->normalImpulses
                            lua_newtable(lua);
                            lua_pushnumber(lua, impulse->normalImpulses[0]);
                            lua_rawseti(lua, -2, 1);

                            lua_pushnumber(lua, impulse->normalImpulses[1]);
                            lua_rawseti(lua, -2, 2);

                            lua_setfield(lua, -2, "normalImpulses");

                            // impulse->tangentImpulses
                            lua_newtable(lua);
                            lua_pushnumber(lua, impulse->tangentImpulses[0]);
                            lua_rawseti(lua, -2, 1);

                            lua_pushnumber(lua, impulse->tangentImpulses[1]);
                            lua_rawseti(lua, -2, 2);

                            lua_setfield(lua, -2, "tangentImpulses");
                            if (lua_pcall(lua, 3, 0, 0))
                                plugin_helper::lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
                        }
                    }
                }
                break;
            }
			lua_settop(lua,0);
        }
    }

    void lua_box2d_BeginContact(PHYSICS_BOX2D *box2d, SHAPE_INFO *info1, SHAPE_INFO *info2)
    {
        lua_box2d_EventContact(box2d, info1, info2, T_B2_BeginContact, nullptr, nullptr);
    }

    void lua_box2d_EndContact(PHYSICS_BOX2D *box2d, SHAPE_INFO *info1, SHAPE_INFO *info2)
    {
        lua_box2d_EventContact(box2d, info1, info2, T_B2_EndContact, nullptr, nullptr);
    }

    void lua_box2d_PreSolve(PHYSICS_BOX2D *box2d, SHAPE_INFO *info1, SHAPE_INFO *info2,const b2Manifold *oldManifold)
    {
        lua_box2d_EventContact(box2d, info1, info2, T_B2_PreSolve, oldManifold, nullptr);
    }

    void lua_box2d_PostSolve(PHYSICS_BOX2D *box2d, SHAPE_INFO *info1, SHAPE_INFO *info2, const b2ContactImpulse *impulse)
    {
        lua_box2d_EventContact(box2d, info1, info2, T_B2_PostSolve, nullptr, impulse);
    }

    PHYSICS_BOX2D *getBox2dFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<PHYSICS_BOX2D **>(plugin_helper::lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_BOX2D));
        return *ud;
    }
}
