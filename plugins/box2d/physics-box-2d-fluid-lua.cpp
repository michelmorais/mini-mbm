/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2022      by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include "physics-box-2d-fluid-lua.h"
#include <../plugin-helper/plugin-helper.h>
#include <Box2D/Box2D.h>
#include <core_mbm/util-interface.h>
#include <core_mbm/class-identifier.h>
#include <render/steered_particle.h>
#include <box-2d-wrap.h>

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

namespace mbm
{
    API_IMPL int onSetPhysicsFromTableLua(lua_State *lua,const int indexTable,INFO_PHYSICS* infoPhysicsOut);
    API_IMPL int onGetShaderTableRenderizableLuaNoGC(lua_State *lua);

    STEERED_PARTICLE *getRenderizableFluidBox2dFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<mbm::STEERED_PARTICLE **>(plugin_helper::lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_STEERED_PARTICLE));
        return *ud;
    }

    b2ParticleSystem* getParticleSystemBox2dFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        mbm::STEERED_PARTICLE * p_steered_particle = getRenderizableFluidBox2dFromRawTable(lua,rawi,indexTable);
        auto *userData         = static_cast<USER_DATA_RENDER_LUA *>(p_steered_particle->userData);
        mbm::INFO_FLUID*  info = static_cast<mbm::INFO_FLUID*>(userData->extra);
        return info->particleSystem;
    }

    int onGetParticleCountBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const int32 particleCount  = pSystem->GetParticleCount();
        lua_pushinteger(lua, particleCount);
        return 1;
    }

    int onGetMaxParticleCountBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const int32 particleCount  = pSystem->GetMaxParticleCount();
        lua_pushinteger(lua, particleCount);
        return 1;
    }

    int onSetMaxParticleCountBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const int32 particleCount  = luaL_checkinteger(lua,2);
        pSystem->SetMaxParticleCount(particleCount);
        return 0;
    }

    int onSetPausedParticleBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const bool pause           = lua_toboolean(lua,2);
        pSystem->SetPaused(pause);
        return 0;
    }

    int onSetDensityParticleBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const float32 density      = luaL_checknumber(lua,2);
        pSystem->SetDensity(density);
        return 0;
    }

    int onGetDensityParticleBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const float32 density      = pSystem->GetDensity();
        lua_pushnumber(lua, density);
        return 1;
    }


    int onSetGravityScaleParticleBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const float32 gravityScale = luaL_checknumber(lua,2);
        pSystem->SetGravityScale(gravityScale);
        return 0;
    }

    int onGetGravityScaleParticleBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const float32 gravityScale = pSystem->GetGravityScale();
        lua_pushnumber(lua, gravityScale);
        return 1;
    }

    
    int onGetDampingParticleBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const float32 damping      = pSystem->GetDamping();
        lua_pushnumber(lua, damping);
        return 1;
    }

    int onSetDampingParticleBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const float32 damping      = luaL_checknumber(lua,2);
        pSystem->SetDamping(damping);
        return 0;
    }
    
    int onGetStaticPressureIterationsParticleBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const int32 iterations     = pSystem->GetStaticPressureIterations();
        lua_pushinteger(lua, iterations);
        return 1;
    }

    /// Change the number of iterations when calculating the static pressure of
    /// particles. By default, 8 iterations. You can reduce the number of
    /// iterations down to 1 in some situations, but this may cause
    /// instabilities when many particles come together. If you see particles
    /// popping away from each other like popcorn, you may have to increase the
    /// number of iterations.
    /// For a description of static pressure, see
    /// http://en.wikipedia.org/wiki/Static_pressure#Static_pressure_in_fluid_dynamics
    int onSetStaticPressureIterationsParticleBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const int32 iterations     = luaL_checkinteger(lua,2);
        pSystem->SetStaticPressureIterations(iterations);
        return 0;
    }

    int onGetRadiusParticleBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const float32 radius       = pSystem->GetRadius();
        lua_pushnumber(lua, radius);
        return 1;
    }

    int onGetParticleLifetimeParticleBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const int32 index          = luaL_checkinteger(lua,2)-1;
        if(index < pSystem->GetParticleCount() && index >= 0)
        {
            float32 lifetime           = pSystem->GetParticleLifetime(index);
            lua_pushnumber(lua, lifetime);
        }
        else
        {
            lua_pushnumber(lua, 0);
        }
        return 1;
    }

    int onSetParticleLifetimeParticleBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const int32 index          = luaL_checkinteger(lua,2)-1;
        const float32 lifetime     = luaL_checknumber(lua,3);
        if(index < pSystem->GetParticleCount() && index >= 0)
        {
            pSystem->SetParticleLifetime(index,lifetime);
        }
        return 0;
    }

    int onDestroyParticleParticleBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        const int32 index          = luaL_checkinteger(lua,2)-1;
        if(index < pSystem->GetParticleCount() && index >= 0)
        {
            pSystem->DestroyParticle(index);
        }
        return 0;
    }

    int onDestroyParticlesInShapeBox2d(lua_State *lua)
    {
        mbm::STEERED_PARTICLE * p_steered_particle = getRenderizableFluidBox2dFromRawTable(lua,1,1);
        const int indexTable                       = 2;
        const int indexSubTable                    = lua_gettop(lua)+1;
        USER_DATA_RENDER_LUA * userData            = static_cast<USER_DATA_RENDER_LUA *>(p_steered_particle->userData);
        mbm::INFO_FLUID*  info                     = static_cast<mbm::INFO_FLUID*>(userData->extra);
        b2ParticleSystem* pSystem                  = info->particleSystem;
        RENDERIZABLE * the_ptr                     = plugin_helper::getRenderizableNoThrowFromRawTable(lua, 1, indexTable);
        const float scale                          = p_steered_particle->getScalePhysicsEngine();
        b2Transform bTransform;
        if(the_ptr != nullptr)
        {
            const INFO_PHYSICS*  const_info_physics = the_ptr->getInfoPhysics();
            if(const_info_physics == nullptr)
            {
                return plugin_helper::lua_error_debug(lua, "object [%s] has no physics!", the_ptr->getTypeClassName());
            }
            const b2Vec2 pos(the_ptr->position.x / scale,the_ptr->position.y / scale);
            int32 particles_destroyed = 0;
            bTransform.Set(pos,the_ptr->angle.z);
            for(const CUBE * cube :const_info_physics->lsCube)
            {
                b2PolygonShape  bBox;
                bBox.SetAsBox(cube->halfDim.x / scale,cube->halfDim.y / scale);
                particles_destroyed += pSystem->DestroyParticlesInShape(bBox,bTransform);
            }
            for(const SPHERE * sphere :const_info_physics->lsSphere)
            {
                b2CircleShape   bCircle;
                bCircle.m_radius  = sphere->ray / scale;
                particles_destroyed += pSystem->DestroyParticlesInShape(bCircle,bTransform);
            }
            for(const TRIANGLE * triangle :const_info_physics->lsTriangle)
            {
                b2PolygonShape  bTriangle;
                b2Vec2 vertices[3];
                for(int j=0; j< 3; ++j)
                {
                    vertices[j].x = triangle->point[j].x / scale;
                    vertices[j].y = triangle->point[j].y / scale;
                }
                bTriangle.Set(vertices, 3);
                particles_destroyed += pSystem->DestroyParticlesInShape(bTriangle,bTransform);
            }
            lua_pushinteger(lua,particles_destroyed);
        }
        else
        {
            float angle = 0;
            b2Shape* bShape = nullptr;
            b2CircleShape   bCircle;
            b2PolygonShape  bTriangle;
            b2PolygonShape  bBox;
            b2Vec2 pos;
            std::string strType;
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "type", LUA_TSTRING, &strType);
            if (strType.size() == 0)
                return plugin_helper::lua_error_debug(lua, "Failed to create shape from lua table");
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "x",      LUA_TNUMBER, &pos.x);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "y",      LUA_TNUMBER, &pos.y);
            plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "angle",  LUA_TNUMBER, &angle);
            pos.x = pos.x / scale;
            pos.y = pos.y / scale;
            bTransform.Set(pos,angle);
            const char* myType = strType.c_str();
            if (strcmp(myType, "rectangle") == 0 || strcmp(myType, "rect") == 0)
            {
                float width = 0;
                float height = 0;
                plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "width",  LUA_TNUMBER, &width);
                plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "height", LUA_TNUMBER, &height);
                if(width <= 0.0f)
                    return plugin_helper::lua_error_debug(lua, "Invalid [width] [%f] for [rectangle]",width);
                if(height <= 0.0f)
                    return plugin_helper::lua_error_debug(lua, "Invalid [height] [%f] for [rectangle]",height);
                
                bBox.SetAsBox(width / scale * 0.5f,height / scale * 0.5f);
                bShape = &bBox;
                
            }
            else if (strcmp(myType, "circle") == 0)
            {
                float ray = 0;
                plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "ray",  LUA_TNUMBER, &ray);
                if(ray <= 0.0f)
                    return plugin_helper::lua_error_debug(lua, "Invalid [ray] [%f] for [circle]",ray);
                bCircle.m_radius  = ray / scale;
                bCircle.m_p.Set(0,0);
                bShape = &bCircle;
            }
            else if (strcmp(myType, "triangle") == 0)
            {
                b2Vec2 vertices[3];
                for(int j=0; j< 3; ++j)
                {
                    const char l = 'a' + static_cast<char>(j);
                    const char letter[2] = {l,0};
                    lua_getfield(lua,indexTable,letter);
                    if(lua_istable(lua,indexSubTable))
                    {
                        plugin_helper::getFieldPrimaryFromTable(lua, indexSubTable, "x", LUA_TNUMBER, &vertices[j].x);
                        plugin_helper::getFieldPrimaryFromTable(lua, indexSubTable, "y", LUA_TNUMBER, &vertices[j].y);
                    }
                    else
                    {
                        return plugin_helper::lua_error_debug(lua, "Missing field[%c] for [triangle]",'a' + static_cast<char>(j));
                    }
                    lua_pop(lua, 1);
                }
                
                vertices[0].Set(vertices[0].x /  scale, vertices[0].y /  scale);
                vertices[1].Set(vertices[1].x /  scale, vertices[1].y /  scale);
                vertices[2].Set(vertices[2].x /  scale, vertices[2].y /  scale);
                bTriangle.Set(vertices, 3);
                bShape = &bTriangle;
            }
            else
            {
                return plugin_helper::lua_error_debug(lua, "Unkown type[%s] ",myType);
            }
            const int32 particles_destroyed = pSystem->DestroyParticlesInShape(*bShape,bTransform);
            lua_pushinteger(lua,particles_destroyed);
        }
        return 1;
    }

    int onParticleApplyLinearImpulseBox2d(lua_State *lua)
    {
        const int top              = lua_gettop(lua);
        const int indexTable       = top + 1;
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        int index =0;
        b2Vec2 impulse(0,0);
        if(lua_istable(lua,2))
        {
            const std::size_t lenTable = lua_rawlen(lua, 2);
            for (std::size_t i=0; i<lenTable; ++i)
            {
                lua_rawgeti(lua, 2, (i + 1));
                plugin_helper::getFieldIntegerFromTable(lua, indexTable, "index",&index);
                plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "x", LUA_TNUMBER, &impulse.x);
                plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "y", LUA_TNUMBER, &impulse.y);
                lua_pop(lua, 1);
                if(index <= pSystem->GetParticleCount() && index > 0)
                {
                    pSystem->ParticleApplyLinearImpulse(index -1,impulse);
                    index=0;
                }
            }
        }
        else
        {
            const int32 index          = luaL_checkinteger(lua,2);
            if(index <= pSystem->GetParticleCount() && index > 0)
            {
                impulse.x = luaL_checknumber(lua,3);
                impulse.y = luaL_checknumber(lua,4);
                pSystem->ParticleApplyLinearImpulse(index -1,impulse);
            }
        }
        return 0;
    }

    
    int onApplyLinearImpulseBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        b2Vec2 impulse(0,0);
        int32 iFirstIndex          = luaL_checkinteger(lua,2) - 1;
        int32 iLastIndex           = luaL_checkinteger(lua,3) - 1;
        impulse.x                  = luaL_checknumber(lua,4);
        impulse.y                  = luaL_checknumber(lua,5);
        if(iFirstIndex > iLastIndex)
            return plugin_helper::lua_error_debug(lua, "First index [%d] cannot be greater then last index [%d]",iFirstIndex+1,iLastIndex+1);
        if(iFirstIndex >= pSystem->GetParticleCount())
            iFirstIndex = pSystem->GetParticleCount() -1;
        else if(iFirstIndex < 0)
            iFirstIndex = 0;
        if(iLastIndex >= pSystem->GetParticleCount())
            iLastIndex = pSystem->GetParticleCount() -1;
        else if(iLastIndex < 0)
            iLastIndex = 0;
        pSystem->ApplyLinearImpulse(iFirstIndex,iLastIndex,impulse);
        return 0;
    }


    int onParticleApplyForceBox2d(lua_State *lua)
    {
        const int top              = lua_gettop(lua);
        const int indexTable       = top + 1;
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        int index =0;
        b2Vec2 impulse(0,0);
        if(lua_istable(lua,2))
        {
            const std::size_t lenTable = lua_rawlen(lua, 2);
            for (std::size_t i=0; i<lenTable; ++i)
            {
                lua_rawgeti(lua, 2, (i + 1));
                plugin_helper::getFieldIntegerFromTable(lua, indexTable, "index",&index);
                plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "x", LUA_TNUMBER, &impulse.x);
                plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "y", LUA_TNUMBER, &impulse.y);
                lua_pop(lua, 1);
                if(index <= pSystem->GetParticleCount() && index > 0)
                {
                    pSystem->ParticleApplyForce(index -1,impulse);
                    index=0;
                }
            }
        }
        else
        {
            const int32 index          = luaL_checkinteger(lua,2);
            if(index <= pSystem->GetParticleCount() && index > 0)
            {
                impulse.x = luaL_checknumber(lua,3);
                impulse.y = luaL_checknumber(lua,4);
                pSystem->ParticleApplyForce(index -1,impulse);
            }
        }
        return 0;
    }

    int onApplyForceParticleBox2d(lua_State *lua)
    {
        b2ParticleSystem* pSystem  = getParticleSystemBox2dFromRawTable(lua, 1, 1);
        b2Vec2 impulse(0,0);
        int32 iFirstIndex          = luaL_checkinteger(lua,2) - 1;
        int32 iLastIndex           = luaL_checkinteger(lua,3) - 1;
        impulse.x                  = luaL_checknumber(lua,4);
        impulse.y                  = luaL_checknumber(lua,5);
        if(iFirstIndex > iLastIndex)
            return plugin_helper::lua_error_debug(lua, "First index [%d] cannot be greater then last index [%d]",iFirstIndex+1,iLastIndex+1);
        if(iFirstIndex >= pSystem->GetParticleCount())
            iFirstIndex = pSystem->GetParticleCount() -1;
        else if(iFirstIndex < 0)
            iFirstIndex = 0;
        if(iLastIndex >= pSystem->GetParticleCount())
            iLastIndex = pSystem->GetParticleCount() -1;
        else if(iLastIndex < 0)
            iLastIndex = 0;
        pSystem->ApplyForce(iFirstIndex,iLastIndex,impulse);
        return 0;
    }

    struct USER_DATA_FLUID_LUA : public REF_FUNCTION_LUA
    {
        int   ref_MeAsTable;
        int   ref_CallBack;
        lua_State * lua;
        USER_DATA_FLUID_LUA(lua_State * _lua):
        ref_MeAsTable(LUA_NOREF),
        ref_CallBack(LUA_NOREF),
        lua(_lua)
        {}
        virtual ~USER_DATA_FLUID_LUA()
        {
            unrefAllTableLua(lua);
        }
        void unrefAllTableLua(lua_State *lua)
        {
            this->unrefTableLua(lua,&this->ref_MeAsTable);
            this->unrefTableLua(lua,&this->ref_CallBack);
        }
    };


    class FLUID_QUERY : public b2QueryCallback
    {
    public:
        USER_DATA_FLUID_LUA user_data_fluid_lua;
        STEERED_PARTICLE * p_steered_particle;
        b2ParticleSystem* mSystem;
        
        FLUID_QUERY(lua_State * lua,STEERED_PARTICLE * pSteered_particle, b2ParticleSystem* pSystem):
        user_data_fluid_lua(lua),
        p_steered_particle(pSteered_particle),
        mSystem(pSystem)
        {
        }
        /// Called for each fixture found in the query AABB.
	    /// @return false to terminate the query.
        virtual bool ReportFixture(b2Fixture*)
        {
            return false;
        }
        /// Called for each particle found in the query AABB.
        /// @return false to terminate the query.
        virtual bool ReportParticle(const b2ParticleSystem* particleSystem,int32 index)
        {
            lua_State * lua              = user_data_fluid_lua.lua;
            lua_rawgeti(lua, LUA_REGISTRYINDEX, user_data_fluid_lua.ref_CallBack);
            if (lua_isfunction(lua, -1))
            {
                lua_rawgeti(lua, LUA_REGISTRYINDEX, user_data_fluid_lua.ref_MeAsTable);
                lua_pushinteger(lua,index+1);
                if (lua_pcall(lua, 2, 1, 0))
                {
                    plugin_helper::lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
                }
                else
                {
                    if (lua_type(lua, -1) == LUA_TNIL)
                        return true;
                    const bool ret = (lua_toboolean(lua, -1) ? true : false);
                    return ret;
                }
            }
            return false;
        }
    };

    int onQueryAABBParticleBox2d(lua_State *lua)
    {
        const int top                                  = lua_gettop(lua);
        if(top == 6)
        {
            b2AABB b2_aabb;
            mbm::STEERED_PARTICLE*  p_steered_particle = getRenderizableFluidBox2dFromRawTable(lua,1,1);
            USER_DATA_RENDER_LUA * userData            = static_cast<USER_DATA_RENDER_LUA *>(p_steered_particle->userData);
            mbm::INFO_FLUID*  info                     = static_cast<mbm::INFO_FLUID*>(userData->extra);
            b2ParticleSystem* pSystem                  = info->particleSystem;
            const float scale                          = p_steered_particle->getScalePhysicsEngine();
            b2_aabb.lowerBound.x                       = luaL_checknumber(lua, 2) / scale;
            b2_aabb.lowerBound.y                       = luaL_checknumber(lua, 3) / scale;
            b2_aabb.upperBound.x                       = luaL_checknumber(lua, 4) / scale;
            b2_aabb.upperBound.y                       = luaL_checknumber(lua, 5) / scale;
            {
                FLUID_QUERY fluid_query(lua,p_steered_particle,pSystem);
                fluid_query.user_data_fluid_lua.refTableLua(lua, 1, &fluid_query.user_data_fluid_lua.ref_MeAsTable);
                fluid_query.user_data_fluid_lua.refFunctionLua(lua, 6, &fluid_query.user_data_fluid_lua.ref_CallBack);
                pSystem->QueryAABB(&fluid_query,b2_aabb);
            }
        }
        else
        {
            return plugin_helper::lua_error_debug(lua,"expected <lower_bound_x>, <lower_bound_y>, <upper_bound_x>, <upper_bound_y>, <call_back_function>");
        }
        return 0;
    }

    int onQueryShapeAABBParticleBox2d(lua_State *lua)
    {
        const int top                                  = lua_gettop(lua);
        if(top == 3)
        {
            b2AABB b2_aabb;
            const int indexTable                       = top + 1;
            mbm::STEERED_PARTICLE*  p_steered_particle = getRenderizableFluidBox2dFromRawTable(lua,1,1);
            RENDERIZABLE * the_ptr                     = plugin_helper::getRenderizableNoThrowFromRawTable(lua, 1, 2);
            USER_DATA_RENDER_LUA * userData            = static_cast<USER_DATA_RENDER_LUA *>(p_steered_particle->userData);
            mbm::INFO_FLUID*  info                     = static_cast<mbm::INFO_FLUID*>(userData->extra);
            b2ParticleSystem* pSystem                  = info->particleSystem;
            const float scale                          = p_steered_particle->getScalePhysicsEngine();
            b2Transform bTransform;
            FLUID_QUERY fluid_query(lua,p_steered_particle,pSystem);
            fluid_query.user_data_fluid_lua.refTableLua(lua, 1, &fluid_query.user_data_fluid_lua.ref_MeAsTable);
            fluid_query.user_data_fluid_lua.refFunctionLua(lua, 3, &fluid_query.user_data_fluid_lua.ref_CallBack);
            if(the_ptr != nullptr)
            {
                const INFO_PHYSICS*  const_info_physics = the_ptr->getInfoPhysics();
                if(const_info_physics == nullptr)
                {
                    return plugin_helper::lua_error_debug(lua, "object [%s] has no physics!", the_ptr->getTypeClassName());
                }
                const b2Vec2 pos(the_ptr->position.x / scale,the_ptr->position.y / scale);
                bTransform.Set(pos,the_ptr->angle.z);
                for(const CUBE * cube :const_info_physics->lsCube)
                {
                    b2PolygonShape  bBox;
                    bBox.SetAsBox(cube->halfDim.x / scale,cube->halfDim.y / scale);
                    pSystem->QueryShapeAABB(&fluid_query,bBox,bTransform);
                }
                for(const SPHERE * sphere :const_info_physics->lsSphere)
                {
                    b2CircleShape   bCircle;
                    bCircle.m_radius  = sphere->ray / scale;
                    pSystem->QueryShapeAABB(&fluid_query,bCircle,bTransform);
                }
                for(const TRIANGLE * triangle :const_info_physics->lsTriangle)
                {
                    b2PolygonShape  bTriangle;
                    b2Vec2 vertices[3];
                    for(int j=0; j< 3; ++j)
                    {
                        vertices[j].x = triangle->point[j].x / scale;
                        vertices[j].y = triangle->point[j].y / scale;
                    }
                    bTriangle.Set(vertices, 3);
                    pSystem->QueryShapeAABB(&fluid_query,bTriangle,bTransform);
                }
            }
            else
            {
                const int indexSubTable   = indexTable + 1;
                float angle               = 0;
                b2Shape* bShape           = nullptr;
                b2CircleShape   bCircle;
                b2PolygonShape  bTriangle;
                b2PolygonShape  bBox;
                b2Vec2 pos;
                std::string strType;
                plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "type", LUA_TSTRING, &strType);
                if (strType.size() == 0)
                    return plugin_helper::lua_error_debug(lua, "Failed to create shape from lua table");
                plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "x",      LUA_TNUMBER, &pos.x);
                plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "y",      LUA_TNUMBER, &pos.y);
                plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "angle",  LUA_TNUMBER, &angle);
                pos.x = pos.x / scale;
                pos.y = pos.y / scale;
                bTransform.Set(pos,angle);
                const char* myType = strType.c_str();
                if (strcmp(myType, "rectangle") == 0 || strcmp(myType, "rect") == 0)
                {
                    float width = 0;
                    float height = 0;
                    plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "width",  LUA_TNUMBER, &width);
                    plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "height", LUA_TNUMBER, &height);
                    if(width <= 0.0f)
                        return plugin_helper::lua_error_debug(lua, "Invalid [width] [%f] for [rectangle]",width);
                    if(height <= 0.0f)
                        return plugin_helper::lua_error_debug(lua, "Invalid [height] [%f] for [rectangle]",height);
                    
                    bBox.SetAsBox(width / scale * 0.5f,height / scale * 0.5f);
                    bShape = &bBox;
                }
                else if (strcmp(myType, "circle") == 0)
                {
                    float ray = 0;
                    plugin_helper::getFieldPrimaryFromTable(lua, indexTable, "ray",  LUA_TNUMBER, &ray);
                    if(ray <= 0.0f)
                        return plugin_helper::lua_error_debug(lua, "Invalid [ray] [%f] for [circle]",ray);
                    bCircle.m_radius  = ray / scale;
                    bCircle.m_p.Set(0,0);
                    bShape = &bCircle;
                }
                else if (strcmp(myType, "triangle") == 0)
                {
                    b2Vec2 vertices[3];
                    for(int j=0; j< 3; ++j)
                    {
                        const char l = 'a' + static_cast<char>(j);
                        const char letter[2] = {l,0};
                        lua_getfield(lua,indexTable,letter);
                        if(lua_istable(lua,indexSubTable))
                        {
                            plugin_helper::getFieldPrimaryFromTable(lua, indexSubTable, "x", LUA_TNUMBER, &vertices[j].x);
                            plugin_helper::getFieldPrimaryFromTable(lua, indexSubTable, "y", LUA_TNUMBER, &vertices[j].y);
                        }
                        else
                        {
                            return plugin_helper::lua_error_debug(lua, "Missing field[%c] for [triangle]",'a' + static_cast<char>(j));
                        }
                        lua_pop(lua, 1);
                    }
                    
                    vertices[0].Set(vertices[0].x /  scale, vertices[0].y /  scale);
                    vertices[1].Set(vertices[1].x /  scale, vertices[1].y /  scale);
                    vertices[2].Set(vertices[2].x /  scale, vertices[2].y /  scale);
                    bTriangle.Set(vertices, 3);
                    bShape = &bTriangle;
                }
                else
                {
                    return plugin_helper::lua_error_debug(lua, "Unkown type[%s] ",myType);
                }
                pSystem->QueryShapeAABB(&fluid_query,*bShape,bTransform);
            }
        }
        else
        {
            return plugin_helper::lua_error_debug(lua,"expected <body | shape>, <call_back_function>");
        }
        return 0;
    }

    int onComputeAABBParticleBox2d(lua_State *lua)
    {
        b2AABB b2_aabb;
        mbm::STEERED_PARTICLE*  p_steered_particle = getRenderizableFluidBox2dFromRawTable(lua,1,1);
        USER_DATA_RENDER_LUA * userData            = static_cast<USER_DATA_RENDER_LUA *>(p_steered_particle->userData);
        mbm::INFO_FLUID*  info                     = static_cast<mbm::INFO_FLUID*>(userData->extra);
        b2ParticleSystem* pSystem                  = info->particleSystem;
        const float scale                          = p_steered_particle->getScalePhysicsEngine();
        pSystem->ComputeAABB(&b2_aabb);
        b2_aabb.lowerBound.x                       = b2_aabb.lowerBound.x * scale;
        b2_aabb.lowerBound.y                       = b2_aabb.lowerBound.y * scale;
        b2_aabb.upperBound.x                       = b2_aabb.upperBound.x * scale;
        b2_aabb.upperBound.y                       = b2_aabb.upperBound.y * scale;
        lua_settop(lua, 0);
        lua_newtable(lua);
        lua_newtable(lua);
        lua_pushnumber(lua,b2_aabb.lowerBound.x);
        lua_setfield(lua,-2,"x");
        lua_pushnumber(lua,b2_aabb.lowerBound.y);
        lua_setfield(lua,-2,"y");
        lua_setfield(lua,-2,"lowerBound");

        lua_newtable(lua);
        lua_pushnumber(lua,b2_aabb.upperBound.x);
        lua_setfield(lua,-2,"x");
        lua_pushnumber(lua,b2_aabb.upperBound.y);
        lua_setfield(lua,-2,"y");
        lua_setfield(lua,-2,"upperBound");
        return 1;
    }

    class FLUID_RAY_CAST : public b2RayCastCallback
    {
    public:
        USER_DATA_FLUID_LUA user_data_fluid_lua;
        STEERED_PARTICLE * p_steered_particle;
        b2ParticleSystem* mSystem;
        
        FLUID_RAY_CAST(lua_State * lua,STEERED_PARTICLE * pSteered_particle, b2ParticleSystem* pSystem):
        user_data_fluid_lua(lua),
        p_steered_particle(pSteered_particle),
        mSystem(pSystem)
        {
        }

        virtual float32 ReportFixture(b2Fixture* ,const b2Vec2& ,const b2Vec2& ,float32 fraction)
        {
            return fraction;
        }
        virtual float32 ReportParticle(const b2ParticleSystem* particleSystem,int32 index, const b2Vec2& point,const b2Vec2& normal, float32 fraction)
        {
            if(particleSystem != mSystem)
                return fraction;
            const float scale = p_steered_particle->getScalePhysicsEngine();
            lua_State * lua   = user_data_fluid_lua.lua;
            lua_rawgeti(lua, LUA_REGISTRYINDEX, user_data_fluid_lua.ref_CallBack);
            if (lua_isfunction(lua, -1))
            {
                lua_rawgeti(lua, LUA_REGISTRYINDEX, user_data_fluid_lua.ref_MeAsTable);
                lua_pushinteger(lua,index+1);
                lua_pushnumber(lua, point.x * scale);
                lua_pushnumber(lua, point.y * scale);
                lua_pushnumber(lua, normal.x);
                lua_pushnumber(lua, normal.y);
                lua_pushnumber(lua, fraction);
                if (lua_pcall(lua, 7, 1, 0))
                {
                    plugin_helper::lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
                }
                else
                {
                    if (lua_type(lua, -1) != LUA_TNUMBER)
                        return fraction;
                    const float ret = lua_tonumber(lua, -1);
                    return ret;
                }
            }
            return fraction;
        }

        virtual bool ShouldQueryParticleSystem(const b2ParticleSystem*)
        {
            return true;
        }

    };

    int onRayCastParticleBox2d(lua_State *lua)
    {
        mbm::STEERED_PARTICLE*  p_steered_particle = getRenderizableFluidBox2dFromRawTable(lua,1,1);
        USER_DATA_RENDER_LUA * userData            = static_cast<USER_DATA_RENDER_LUA *>(p_steered_particle->userData);
        mbm::INFO_FLUID*  info                     = static_cast<mbm::INFO_FLUID*>(userData->extra);
        b2ParticleSystem* pSystem                  = info->particleSystem;
        const float scale                          = p_steered_particle->getScalePhysicsEngine();
        const b2Vec2 p1(luaL_checknumber(lua, 2) / scale, luaL_checknumber(lua, 3) / scale);
        const b2Vec2 p2(luaL_checknumber(lua, 4) / scale, luaL_checknumber(lua, 5) / scale);
        const b2Vec2 t(p1 - p2);
        if (t.Length() != 0.0f) //-V550
        {
            FLUID_RAY_CAST fluid_cast(lua,p_steered_particle,pSystem);
            fluid_cast.user_data_fluid_lua.refTableLua(lua, 1, &fluid_cast.user_data_fluid_lua.ref_MeAsTable);
            fluid_cast.user_data_fluid_lua.refFunctionLua(lua, 6, &fluid_cast.user_data_fluid_lua.ref_CallBack);
            pSystem->RayCast(&fluid_cast, p1, p2);
        }
    #if _DEBUG
        else
        {
            PRINT_IF_DEBUG("x1,y1 and x2,y2 must be different point");
        }
    #endif
        return 0;
    }

    int onSetColorFluidParticleBox2d(lua_State *lua)
    {
        mbm::STEERED_PARTICLE*  p_steered_particle = getRenderizableFluidBox2dFromRawTable(lua,1,1);
        COLOR c = p_steered_particle->getColor();
        if(lua_type(lua,2) == LUA_TTABLE)
        {
            plugin_helper::getFieldPrimaryFromTable(lua,2,"r",LUA_TNUMBER,&c.r);
            plugin_helper::getFieldPrimaryFromTable(lua,2,"g",LUA_TNUMBER,&c.g);
            plugin_helper::getFieldPrimaryFromTable(lua,2,"b",LUA_TNUMBER,&c.b);
            plugin_helper::getFieldPrimaryFromTable(lua,2,"a",LUA_TNUMBER,&c.a);
        }
        else
        {
            const int top = lua_gettop(lua);
            c.r = luaL_checknumber(lua,2);
            c.g = luaL_checknumber(lua,3);
            c.b = luaL_checknumber(lua,4);
            c.a = top  > 4 ? luaL_checknumber(lua,5) : c.a;
        }
        p_steered_particle->setColor(c);
        return 0;
    }

    int onGetColorFluidParticleBox2d(lua_State *lua)
    {
        mbm::STEERED_PARTICLE*  p_steered_particle = getRenderizableFluidBox2dFromRawTable(lua,1,1);
        COLOR c = p_steered_particle->getColor();
        lua_pushnumber(lua,c.r);
        lua_pushnumber(lua,c.g);
        lua_pushnumber(lua,c.b);
        lua_pushnumber(lua,c.a);
        return 4;
    }

    int onAddParticleFluidBox2d(lua_State *lua)
    {
        const int top             =  lua_gettop(lua);
        if(top != 3)
        {
            return plugin_helper::lua_error_debug(lua, "expected fluid:([renderizable | physics],table ={x,y,z,sx,sy,sz} ). args received %d",top-1);
        }
        mbm::STEERED_PARTICLE*  p_steered_particle = getRenderizableFluidBox2dFromRawTable(lua,1,1);
        RENDERIZABLE * the_ptr    = plugin_helper::getRenderizableNoThrowFromRawTable(lua, 1, 2);
        auto *userData            = static_cast<USER_DATA_RENDER_LUA *>(p_steered_particle->userData);
        mbm::INFO_FLUID*  info    = static_cast<mbm::INFO_FLUID*>(userData->extra);
        const float scalePhysics  = p_steered_particle->getScalePhysicsEngine();
        VEC3 position(p_steered_particle->position);
        VEC3 scale(1,1,1);
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
            position = the_ptr->position;
            scale    = the_ptr->scale;
        }
        else
        {
            if(onSetPhysicsFromTableLua(lua,2,local_info_physics) != 0)
            {
                return plugin_helper::lua_error_debug(lua, "Failed to create physics from lua table");
            }
        }
        if(lua_type(lua,3) != LUA_TTABLE)
        {
            return plugin_helper::lua_error_debug(lua, "expected 2 argument to be table ={x,y,z,sx,sy,sz} for position /scale. got %s",lua_typename(lua,3));
        }
        plugin_helper::getFieldPrimaryFromTable(lua,3,"x", LUA_TNUMBER,&position.x);
        plugin_helper::getFieldPrimaryFromTable(lua,3,"y", LUA_TNUMBER,&position.y);
        plugin_helper::getFieldPrimaryFromTable(lua,3,"z", LUA_TNUMBER,&position.z);
        plugin_helper::getFieldPrimaryFromTable(lua,3,"sx",LUA_TNUMBER,&scale.x);
        plugin_helper::getFieldPrimaryFromTable(lua,3,"sy",LUA_TNUMBER,&scale.y);
        plugin_helper::getFieldPrimaryFromTable(lua,3,"sz",LUA_TNUMBER,&scale.z);

        const int32 iTotalParticleAdded = PHYSICS_BOX2D::addParticleToFluid(info,local_info_physics,position,scale,scalePhysics);
        lua_pushinteger(lua,iTotalParticleAdded);
        return 1;
    }

#ifdef _DEBUG
    int onDestroyRenderizableFluidBox2dLua(lua_State *lua)
    {
        plugin_helper::lua_print_line(lua,TYPE_LOG_WARN,"you no longer have control over fluid however, to be destroyed, box2d:destroyFluid() must be called\n");
        return 0;
    }
#endif

    
    int onGetRenderizableFluidInterfaceBox2d(lua_State *lua,mbm::RENDERIZABLE * steered_particle)
    {
        lua_settop(lua, 0);
        luaL_Reg regMethods[] = {   {"add",                         onAddParticleFluidBox2d},
                                    {"getParticleCount",            onGetParticleCountBox2d},
                                    {"getMaxParticleCount",         onGetMaxParticleCountBox2d},
                                    {"setMaxParticleCount",         onSetMaxParticleCountBox2d},
                                    {"setColor",                    onSetColorFluidParticleBox2d},
                                    {"getColor",                    onGetColorFluidParticleBox2d},
                                    {"setPaused",                   onSetPausedParticleBox2d},
                                    {"getDensity",                  onGetDensityParticleBox2d},
                                    {"setDensity",                  onSetDensityParticleBox2d},
                                    {"getGravityScale",             onGetGravityScaleParticleBox2d},
                                    {"setGravityScale",             onSetGravityScaleParticleBox2d},
                                    {"getDamping",                  onGetDampingParticleBox2d},
                                    {"setDamping",                  onSetDampingParticleBox2d},
                                    {"getStaticPressureIterations", onGetStaticPressureIterationsParticleBox2d},
                                    {"setStaticPressureIterations", onSetStaticPressureIterationsParticleBox2d},
                                    {"getRadius",                   onGetRadiusParticleBox2d},
                                    {"getParticleLifetime",         onGetParticleLifetimeParticleBox2d},
                                    {"setParticleLifetime",         onSetParticleLifetimeParticleBox2d},
                                    {"destroyParticle",             onDestroyParticleParticleBox2d},
                                    {"destroyParticlesInShape",     onDestroyParticlesInShapeBox2d},
                                    {"particleApplyLinearImpulse",  onParticleApplyLinearImpulseBox2d},
                                    {"applyLinearImpulse",          onApplyLinearImpulseBox2d},
                                    {"particleApplyForce",          onParticleApplyForceBox2d},
                                    {"applyForce",                  onApplyForceParticleBox2d},
                                    {"queryAABB",                   onQueryAABBParticleBox2d},
                                    {"queryShapeAABB",              onQueryShapeAABBParticleBox2d},
                                    {"computeAABB",                 onComputeAABBParticleBox2d},
                                    {"rayCast",                     onRayCastParticleBox2d},
                                    {"getShader",                   onGetShaderTableRenderizableLuaNoGC},
                                    
#ifdef _DEBUG
                                    {"__gc", onDestroyRenderizableFluidBox2dLua},
#endif
                                    
                                    {nullptr, nullptr}};
        luaL_newlib(lua, regMethods);
        auto **udata = static_cast<mbm::STEERED_PARTICLE**>(lua_newuserdata(lua, sizeof(mbm::STEERED_PARTICLE *)));
        *udata       = static_cast<mbm::STEERED_PARTICLE*>(steered_particle);

        /* trick to ensure that we will receive the expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_STEERED_PARTICLE);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }
};
