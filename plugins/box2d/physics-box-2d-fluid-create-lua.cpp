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
#include <limits>
#include <cmath>
#include <plugin-helper/plugin-helper.h>
#include <core_mbm/class-identifier.h>
#include <core_mbm/renderizable.h>
#include <render/steered_particle.h>
#include <core_mbm/device.h>
#include <core_mbm/scene.h>
#include <core_mbm/animation.h>
#include <core_mbm/util-interface.h>
#include <platform/mismatch-platform.h>
#include "box-2d-wrap.h"
#include <plugin-helper/user-data-lua.h>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

extern "C"
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#include "physics-box-2d-fluid-lua.h"
namespace mbm
{
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
            lua_error_debug(lua, "Flag group unknow [%s]",flag);
            return b2_solidParticleGroup;
        };
        lua_getfield(lua, indexTable, "groupFlags");
        if(lua_type(lua,-1) == LUA_TSTRING)
        {
            const char * flag = luaL_checkstring(lua,-1);
            groupFlags = flagFromString(lua,flag);
        }
        else if(lua_type(lua,-1) == LUA_TTABLE)
        {
            const std::size_t lenTable = lua_rawlen(lua, -1);
            for (std::size_t i=0; i<lenTable; ++i)
            {
                lua_rawgeti(lua, -1, (i + 1));
                const char * flag = luaL_checkstring(lua,-1);
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
            lua_error_debug(lua, "Flag unknow [%s]",flag);
            return b2_waterParticle;
        };
        lua_getfield(lua, indexTable, "flags");
        if(lua_type(lua,-1) == LUA_TSTRING)
        {
            const char * flag = luaL_checkstring(lua,-1);
            flags = flagFromString(lua,flag);
        }
        else if(lua_type(lua,-1) == LUA_TTABLE)
        {
            const std::size_t lenTable = lua_rawlen(lua, -1);
            for (std::size_t i=0; i<lenTable; ++i)
            {
                lua_rawgeti(lua, -1, (i + 1));
                const char * flag = luaL_checkstring(lua,-1);
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
            return lua_error_debug(lua, "expected physics:(<renderizable> | <physics_table>, <info_fluid>)");
        }
        if(top == 3 && lua_type(lua,3) != LUA_TTABLE)
        {
            return lua_error_debug(lua, "expected physics:(<renderizable> | <physics_table>, <info_fluid>)");
        }
        PHYSICS_BOX2D *       box2d       = getBox2dFromRawTable(lua, 1, 1);
        RENDERIZABLE *        the_ptr     = getRenderizableNoThrowFromRawTable(lua, 1, 2);
        INFO_PHYSICS  info_physics;
        INFO_PHYSICS*  local_info_physics = &info_physics;
        if(the_ptr != nullptr)
        {
            const INFO_PHYSICS*  const_info_physics = the_ptr->getInfoPhysics();
            if(const_info_physics == nullptr)
            {
                return lua_error_debug(lua, "object [%s] has no physics!", the_ptr->getTypeClassName());
            }
            if(local_info_physics->clone(const_info_physics) == false)
            {
                return lua_error_debug(lua, "Failed to clone phisics from [%s]", the_ptr->getTypeClassName());
            }
        }
        else if(onSetPhysicsFromTableLua(lua,2,local_info_physics) != 0)
        {
            return lua_error_debug(lua, "Failed to create physics from lua table");
        }
        VEC3 position;
        VEC3 scale(1,1,1);
        VEC2 linearVelocity(0,0);
        bool is2dScreen        = false;
        bool is3d              = false;
        uint16 fixtureMaskBits = 0xffff;
        float riseTime         = 0.0f;
        float radiusScale      = 1.0f;
        float radius           = 0.5f;
        float damping          = 0.5f;
        float lifetime         = 0.0f;
        float angularVelocity  = 0;
        float strength         = 1.0f;
        float stride           = 0;
        float angle            = the_ptr ? the_ptr->getAngle().z : 0.0f;
        bool segmented         = false;
        std::string texture("#FF00FFAA");
        COLOR * p_color = nullptr;
        COLOR color(1.0f,1.0f,1.0f,1.0f);
        std::string blend,operation;
        b2ParticleFlag flags(b2_waterParticle);
        b2ParticleGroupFlag groupFlags(b2_solidParticleGroup);
        if(top == 3)
        {
            constexpr int indexTable = 3;
            lua_getfield(lua, indexTable, "riseTime");
            if (!lua_isnil(lua, -1))
            {
                const lua_Number value = luaL_checknumber(lua, -1);
                if (!(value >= 0 && value <= std::numeric_limits<float>::max()))
                    return lua_error_debug(lua, "riseTime must be finite and non-negative");
                riseTime = static_cast<float>(value);
            }
            lua_pop(lua, 1);
            lua_getfield(lua, indexTable, "fixtureMaskBits");
            if (!lua_isnil(lua, -1))
            {
                const lua_Number mask = luaL_checknumber(lua, -1);
                if (!(mask >= 0 && mask <= 65535) || mask != static_cast<uint16>(mask))
                    return lua_error_debug(lua, "fixtureMaskBits must be an integer from 0 to 65535");
                fixtureMaskBits = static_cast<uint16>(mask);
            }
            lua_pop(lua, 1);

            getFieldPrimaryFromTable(lua, indexTable, "texture",          LUA_TSTRING,  &texture);
            getFieldPrimaryFromTable(lua, indexTable, "is2dScreen",       LUA_TBOOLEAN, &is2dScreen);
            getFieldPrimaryFromTable(lua, indexTable, "is3d",             LUA_TBOOLEAN, &is3d);
            getFieldPrimaryFromTable(lua, indexTable, "segmented",        LUA_TBOOLEAN, &segmented);
            getFieldPrimaryFromTable(lua, indexTable, "radiusScale",      LUA_TNUMBER,  &radiusScale);
            getFieldPrimaryFromTable(lua, indexTable, "radius",           LUA_TNUMBER,  &radius);
            getFieldPrimaryFromTable(lua, indexTable, "damping",          LUA_TNUMBER,  &damping);
            getFieldPrimaryFromTable(lua, indexTable, "lifetime",         LUA_TNUMBER,  &lifetime);
            getFieldPrimaryFromTable(lua, indexTable, "angularVelocity",  LUA_TNUMBER,  &angularVelocity);
            getFieldPrimaryFromTable(lua, indexTable, "strength",         LUA_TNUMBER,  &strength);
            getFieldPrimaryFromTable(lua, indexTable, "stride",           LUA_TNUMBER,  &stride);
            getFieldPrimaryFromTable(lua, indexTable, "angle",            LUA_TNUMBER,  &angle);
            getFieldPrimaryFromTable(lua, indexTable, "blend",            LUA_TSTRING,  &blend);
            getFieldPrimaryFromTable(lua, indexTable, "operation",        LUA_TSTRING,  &operation);

            lua_getfield(lua, indexTable, "scale");
            if(lua_type(lua,top + 1) == LUA_TTABLE)
            {
                getFieldPrimaryFromTable(lua, top + 1, "x", LUA_TNUMBER, &scale.x);
                getFieldPrimaryFromTable(lua, top + 1, "y", LUA_TNUMBER, &scale.y);
            }
            lua_pop(lua, 1);

            lua_getfield(lua, indexTable, "linearVelocity");
            if(lua_type(lua,top + 1) == LUA_TTABLE)
            {
                getFieldPrimaryFromTable(lua, top + 1, "x", LUA_TNUMBER, &linearVelocity.x);
                getFieldPrimaryFromTable(lua, top + 1, "y", LUA_TNUMBER, &linearVelocity.y);
            }
            lua_pop(lua, 1);

            lua_getfield(lua, indexTable, "position");
            if(lua_type(lua,top + 1) == LUA_TTABLE)
            {
                getFieldPrimaryFromTable(lua, top + 1, "x", LUA_TNUMBER, &position.x);
                getFieldPrimaryFromTable(lua, top + 1, "y", LUA_TNUMBER, &position.y);
                getFieldPrimaryFromTable(lua, top + 1, "z", LUA_TNUMBER, &position.z);
            }
            lua_pop(lua, 1);

            lua_getfield(lua, indexTable, "color");
            if(lua_type(lua,top + 1) == LUA_TTABLE)
            {
                getFieldPrimaryFromTable(lua, top + 1, "r", LUA_TNUMBER, &color.r);
                getFieldPrimaryFromTable(lua, top + 1, "g", LUA_TNUMBER, &color.g);
                getFieldPrimaryFromTable(lua, top + 1, "b", LUA_TNUMBER, &color.b);
                getFieldPrimaryFromTable(lua, top + 1, "a", LUA_TNUMBER, &color.a);
                p_color = &color;
            }
            lua_pop(lua, 1);

            flags = getb2ParticleFlagFromTable(lua,indexTable);
            groupFlags = getb2GroupFlagsParticleFromTable(lua,indexTable);
        }

        if (!(radius > 0 && std::isfinite(radius)) || !(stride >= 0 && std::isfinite(stride)))
            return luaL_error(lua, "fluid radius must be positive and stride non-negative (finite)");

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
                                                        radiusScale,
                                                        fixtureMaskBits,
                                                        riseTime);
        if (info == nullptr || info->particleSystem == nullptr || info->steered_particle == nullptr)
        {
            return lua_error_debug(lua, "Failed to create fluid. particleSystem:%p steered_particle:%p",info ? info->particleSystem : nullptr,info ? info->steered_particle : nullptr);
        }
        else
        {
            auto *userData = new USER_DATA_RENDER_LUA();
            info->steered_particle->setUserData(userData);
            userData->extra = info;
            if(blend.length() > 0)
            {
                ANIMATION_MANAGER * animManager = info->steered_particle->getAnimationManager();
                auto anim = animManager ? animManager->getAnimation() : nullptr;
                if(anim)
                {
                    const char * value = blend.c_str();
                    if (strcasecmp("disable", value) == 0)
                        anim->setBlendState(BLEND_DISABLE);
                    else if (strcasecmp("zero", value) == 0)
                        anim->setBlendState(BLEND_ZERO);
                    else if (strcasecmp("one", value) == 0)
                        anim->setBlendState(BLEND_ONE);
                    else if (strcasecmp("src_color", value) == 0)
                        anim->setBlendState(BLEND_SRCCOLOR);
                    else if (strcasecmp("inv_src_color", value) == 0)
                        anim->setBlendState(BLEND_INVSRCCOLOR);
                    else if (strcasecmp("src_alpha", value) == 0)
                        anim->setBlendState(BLEND_SRCALPHA);
                    else if (strcasecmp("inv_src_alpha", value) == 0)
                        anim->setBlendState(BLEND_INVSRCALPHA);
                    else if (strcasecmp("dest_alpha", value) == 0)
                        anim->setBlendState(BLEND_DESTALPHA);
                    else if (strcasecmp("inv_dest_alpha", value) == 0)
                        anim->setBlendState(BLEND_INVDESTALPHA);
                    else if (strcasecmp("dest_color", value) == 0)
                        anim->setBlendState(BLEND_DESTCOLOR);
                    else if (strcasecmp("inv_dest_color", value) == 0)
                        anim->setBlendState(BLEND_INVDESTCOLOR);
                    else
                    {
                        box2d->destroyFluid(info);
                        return lua_error_debug(lua, "invalid blend [%s]",value);
                    }
                }
            }
            if(operation.length() > 0)
            {
                ANIMATION_MANAGER * animManager = info->steered_particle->getAnimationManager();
                auto anim = animManager ? animManager->getAnimation() : nullptr;
                if(anim)
                {
                    FX &fx = anim->getFx();
                    const char * blendFunc = operation.c_str();
                    if(strcasecmp(blendFunc,"ADD") == 0)
                        fx.blendOperation = 1;
                    else if(strcasecmp(blendFunc,"SUBTRACT") == 0)
                        fx.blendOperation = 2;
                    else if(strcasecmp(blendFunc,"REVERSE_SUBTRACT") == 0)
                        fx.blendOperation = 3;
                    else if(strcasecmp(blendFunc,"MIN") == 0)
                        fx.blendOperation = 4;
                    else if(strcasecmp(blendFunc,"MAX") == 0)
                        fx.blendOperation = 5;
                    else
                    {
                        box2d->destroyFluid(info);
                        return lua_error_debug(lua, "expected:([blendFunc (ADD,SUBTRACT,REVERSE_SUBTRACT,MIN,MAX)])");
                    }
                }
            }
            return onGetRenderizableFluidInterfaceBox2d(lua, info->steered_particle);
        }
    }

    int onDestroyFluidBox2d(lua_State *lua)
    {
        PHYSICS_BOX2D *       box2d     = getBox2dFromRawTable(lua, 1, 1);
        RENDERIZABLE *ptr = getRenderizableFluidBox2dFromRawTable(lua, 1, 2);
        auto *userData                  = static_cast<USER_DATA_RENDER_LUA *>(ptr->getUserData());
        auto * infoFluid                = static_cast<INFO_FLUID*>(userData->extra);
        if (infoFluid->owner != box2d)
            return luaL_error(lua, "fluid belongs to a different world");
        box2d->destroyFluid(infoFluid);
        return 0;
    }

}
