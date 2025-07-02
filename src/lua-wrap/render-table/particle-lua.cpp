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

#include <lua-wrap/render-table/particle-lua.h>
#include <lua-wrap/common-methods-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <core_mbm/dynamic-var.h>
#include <core_mbm/header-mesh.h>
#include <core_mbm/util-interface.h>
#include <render/particle.h>
#include <lua-wrap/vec3-lua.h>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

namespace mbm
{
    extern int getVariable(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what);
    extern int getVariable(lua_State *lua, RENDERIZABLE *ptr, const char *what);
    extern int setVariable(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what);
    extern int setVariable(lua_State *lua, RENDERIZABLE *ptr, const char *what);
	extern int lua_error_debug(lua_State *lua, const char *format, ...);

    PARTICLE *getParticleFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<PARTICLE **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_PARTICLE));
        return *ud;
    }

    int onDestroyParticleLua(lua_State *lua)
    {
        PARTICLE *            particle = getParticleFromRawTable(lua, 1, 1);
        auto *userData = static_cast<USER_DATA_RENDER_LUA *>(particle->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        particle->userData = nullptr;
    #if DEBUG_FREE_LUA
        static int  num      = 1;
        const char *fileName = particle->getFileName();
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",particle->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(particle);
        delete particle;
        return 0;
    }

	int onDestroyParticleNoGcLua(lua_State *lua)
    {
        PARTICLE *            particle = getParticleFromRawTable(lua, 1, 1);
        auto *userData = static_cast<USER_DATA_RENDER_LUA *>(particle->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        particle->userData = nullptr;
    #if DEBUG_FREE_LUA
        static int  num      = 1;
        const char *fileName = particle->getFileName();
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",particle->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(particle);
        return 0;
    }

    util::STAGE_PARTICLE* getSelectedStageLua(lua_State *lua)
    {
        PARTICLE *particle      = getParticleFromRawTable(lua, 1, 1);
        unsigned int indexStage = lua_type(lua,2) == LUA_TNUMBER ? lua_tointeger(lua, 2) - 1 : 0;
        util::STAGE_PARTICLE* stage = particle->getStageParticle(indexStage);
        return stage;
    }

    int onNewIndexParticle(lua_State *lua) // escrita
    {
        /*
        **********************************
                Estado da pilha
                -3|    table |1
                -2|   string |2
                -1|   number |3
        **********************************
        */
        PARTICLE *  particle = getParticleFromRawTable(lua, 1, 1);
        const char *what     = luaL_checkstring(lua, 2);
        const int   len      = strlen(what);
        switch (len)
        {
            case 1:
            {
                switch (what[0])
                {
                    case 'x': particle->position.x = luaL_checknumber(lua, 3); break;
                    case 'y': particle->position.y = luaL_checknumber(lua, 3); break;
                    case 'z': particle->position.z = luaL_checknumber(lua, 3); break;
                    default: { return setVariable(lua, particle, what);}
                }
            }
            break;
            case 2:
            {
                switch (what[0])
                {
                    case 's':
                    {
                        switch (what[1])
                        {
                            case 'x': particle->scale.x = luaL_checknumber(lua, 3); break;
                            case 'y': particle->scale.y = luaL_checknumber(lua, 3); break;
                            case 'z': particle->scale.z = luaL_checknumber(lua, 3); break;
                            default: { return setVariable(lua, particle, what);}
                        }
                    }
                    break;
                    case 'a':
                    {
                        switch (what[1])
                        {
                            case 'x': particle->angle.x = luaL_checknumber(lua, 3); break;
                            case 'y': particle->angle.y = luaL_checknumber(lua, 3); break;
                            case 'z': particle->angle.z = luaL_checknumber(lua, 3); break;
                            default: { return setVariable(lua, particle, what);}
                        }
                    }
                    break;
                    default: { return setVariable(lua, particle, what);}
                }
            }
            break;
            case 4:
            {
                if (strcmp("grow", what) == 0)
                {
                    util::STAGE_PARTICLE* stage = particle->getStageParticle();
                    if(stage)
                        stage->sizeMin2Max = lua_toboolean(lua, 3) ? 1 : 0;
                }
                else
                    return setVariable(lua, particle, what);
            }
            break;
            case 5:
            {
                if (strcmp("alpha", what) == 0)
                {
					auto *  anim = particle->getAnimation();
                    if (anim)
                    {
                        float      data[4] = { 0, 0, 0, 0 };
                        data[0] = lua_toboolean(lua, 3) ? 1.0f : 0.0f;
                        anim->fx.setVarPShader("enableAlphaFromColor", data);
                        anim->fx.setMaxVarPShader("enableAlphaFromColor", data);
                        anim->fx.setMinVarPShader("enableAlphaFromColor", data);
                    }
                }
                else
                    return setVariable(lua, particle, what);
            }
            break;
            case 6:
            {
                if (strcmp("revive", what) == 0)
                {
                    util::STAGE_PARTICLE* stage = particle->getStageParticle();
                    if (stage)
                        stage->revive = lua_toboolean(lua, 3) ? 1 : 0;
                }
                else
                    return setVariable(lua, particle, what);
            }
            break;
            case 7:
            {
                if (strcmp("visible", what) == 0)
                    particle->enableRender = lua_toboolean(lua, 3) ? true : false;
                else
                    return setVariable(lua, particle, what);
            }
            break;
            case 8:
            {
                if (strcmp("operator", what) == 0)
                {
                    util::STAGE_PARTICLE* stage = particle->getStageParticle();
                    if (stage)
                    {
                        const char* _operator = luaL_checkstring(lua, 3);
                        if (_operator)
                        {
                            if (_operator[0] == '+' || _operator[0] == '-' || _operator[0] == '*' || _operator[0] == '/')
                                stage->_operator = _operator[0];
                        }
                    }
                }
                else
                    return setVariable(lua, particle, what);
            }
            break;
            case 9:
            {
                if (strcmp("segmented", what) == 0)
                {
                    util::STAGE_PARTICLE* stage = particle->getStageParticle();
                    if (stage)
                        stage->segmented = lua_toboolean(lua, 3) ? 1 : 0;
                }
                else
                    return setVariable(lua, particle, what);
            }
            break;
            default: { return setVariable(lua, particle, what);}
        }
        return 0;
    }

    int onIndexParticle(lua_State *lua) // leitura
    {
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1|   string |2
        **********************************
        */
        PARTICLE *  particle = getParticleFromRawTable(lua, 1, 1);
        const char *what     = luaL_checkstring(lua, 2);
        const int   len      = strlen(what);
        switch (len)
        {
            case 1:
            {
                switch (what[0])
                {
                    case 'x': lua_pushnumber(lua, particle->position.x); break;
                    case 'y': lua_pushnumber(lua, particle->position.y); break;
                    case 'z': lua_pushnumber(lua, particle->position.z); break;
                    default: { return getVariable(lua, particle, what);}
                }
            }
            break;
            case 2:
            {
                switch (what[0])
                {
                    case 's':
                    {
                        switch (what[1])
                        {
                            case 'x': lua_pushnumber(lua, particle->scale.x); break;
                            case 'y': lua_pushnumber(lua, particle->scale.y); break;
                            case 'z': lua_pushnumber(lua, particle->scale.z); break;
                            default: { return getVariable(lua, particle, what);}
                        }
                    }
                    break;
                    case 'a':
                    {
                        switch (what[1])
                        {
                            case 'x': lua_pushnumber(lua, particle->angle.x); break;
                            case 'y': lua_pushnumber(lua, particle->angle.y); break;
                            case 'z': lua_pushnumber(lua, particle->angle.z); break;
                            default: { return getVariable(lua, particle, what);}
                        }
                    }
                    break;
                    default: { return getVariable(lua, particle, what);}
                }
            }
            break;
            case 4:
            {
                if (strcmp("grow", what) == 0)
                {
                    const util::STAGE_PARTICLE* stage = particle->getStageParticle();
                    lua_pushboolean(lua, stage ? (stage->sizeMin2Max ? 1 : 0): 0);
                }
                else
                    return getVariable(lua, particle, what);
            }
            break;
            case 5:
            {
                if (strcmp("alpha", what) == 0)
                {
					auto *  anim = particle->getAnimation();
                    if (anim)
                    {
                        float      data[4] = { 0, 0, 0, 0 };
						if (anim->fx.getVarPShader("enableAlphaFromColor", data))
                            lua_pushboolean(lua, data[0] > 0.5f ? 1 : 0);
                        else
                            lua_pushboolean(lua,0);
                        return 1;
                    }
                    else
                    {
                        lua_pushboolean(lua, 0);
                        return 1;
                    }
                }
                else
                    return getVariable(lua, particle, what);
            }
            break;
            case 6:
            {
                if (strcmp("revive", what) == 0)
                {
                    const util::STAGE_PARTICLE* stage = particle->getStageParticle();
                    lua_pushboolean(lua, stage ? stage->revive : 0);
                }
                else
                    return getVariable(lua, particle, what);
            }
            break;
            case 7:
            {
                if (strcmp("visible", what) == 0)
                    lua_pushboolean(lua, particle->enableRender);
                else
                    return getVariable(lua, particle, what);
            }
            break;
            case 8:
            {
                if (strcmp("operator", what) == 0)
                {
                    util::STAGE_PARTICLE* stage = particle->getStageParticle();
                    if (stage)
                    {
                        char _operator[2] = { 0,0 };
                        _operator[0] = stage->_operator;
                        lua_pushstring(lua, _operator);
                    }
                    else
                    {
                        lua_pushnil(lua);
                    }
                }
                else
                    return getVariable(lua, particle, what);
            }
            break;
            case 9:
            {
                if (strcmp("segmented", what) == 0)
                {
                    const util::STAGE_PARTICLE* stage = particle->getStageParticle();
                    lua_pushboolean(lua, stage ? stage->segmented : 0);
                }
                else
                    return getVariable(lua, particle, what);
            }
            break;
            default: { return getVariable(lua, particle, what);
            }
        }
        return 1;
    }

    int onAddParticleLua(lua_State *lua)
    {
        const int          top          = lua_gettop(lua);
        PARTICLE *         particle     = getParticleFromRawTable(lua, 1, 1);
        const unsigned int numParticle  = luaL_checkinteger(lua, 2);
        const bool      forceNow        = top > 2 ? (lua_toboolean(lua, 3) ? true : false ): false;
        if (particle->addParticle(numParticle, forceNow))
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onLoadParticleLua(lua_State *lua)
    {
        const int          top            = lua_gettop(lua);
        PARTICLE *         particle       = getParticleFromRawTable(lua, 1, 1);
        const char *       fileName       = luaL_checkstring(lua, 2);
        const unsigned int numParticle    = top >= 3 ? luaL_checkinteger(lua, 3) : 0;
        const char *       operatorShader = top >= 4 && lua_type(lua,4) == LUA_TSTRING ? lua_tostring(lua, 4) : "*";
        const char *       newCodeLine    = top >= 5 && lua_type(lua,5) == LUA_TSTRING ? lua_tostring(lua, 5) : nullptr;
        if (particle->load(fileName, operatorShader, newCodeLine, numParticle))
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    static bool setVec3ToMemberParticle(lua_State* lua,const int top, VEC3* member,VEC3* min_value,VEC3* max_value)
    {
        int index_table = 0;
        if(top == 2 && lua_type(lua,2) == LUA_TTABLE)//just vec3 exepcted
            index_table = 2;
        else if(top == 3 && lua_type(lua,3) == LUA_TTABLE)//vec3 exepcted
            index_table = 3;
        if(index_table != 0)
        {
            VEC3 * value  = getVec3FromRawTable(lua, 1, index_table);
            member->x = value->x;
            member->y = value->y;
            member->z = value->z;
            if (member->x > max_value->x)
                max_value->x = member->x;
            if (member->y > max_value->y)
                max_value->y = member->y;
            if (member->z > max_value->z)
                max_value->z = member->z;

            if (member->x < min_value->x)
                min_value->x = member->x;
            if (member->y < min_value->y)
                min_value->y = member->y;
            if (member->z < min_value->z)
                min_value->z = member->z;
            return true;
        }
        return false;
    }

    int onSetMinOffsetParticle(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
            if (stage == nullptr)
                return lua_error_debug(lua,"invalid particle's stage's index");
            if(setVec3ToMemberParticle(lua,top,
                   &stage->minOffsetPosition,
                   &stage->minOffsetPosition,
                   &stage->maxOffsetPosition))
                return 0;
            for (int i = 3; i <= top; ++i)
            {
                switch (i)
                {
                    case 3: // x
                    {
                        stage->minOffsetPosition.x = luaL_checknumber(lua, i);
                        if (stage->minOffsetPosition.x > stage->maxOffsetPosition.x)
                            stage->maxOffsetPosition.x = stage->minOffsetPosition.x;
                    }
                    break;
                    case 4: // y
                    {
                        stage->minOffsetPosition.y = luaL_checknumber(lua, i);
                        if (stage->minOffsetPosition.y > stage->maxOffsetPosition.y)
                            stage->maxOffsetPosition.y = stage->minOffsetPosition.y;
                    }
                    break;
                    case 5: // z
                    {
                        stage->minOffsetPosition.z = luaL_checknumber(lua, i);
                        if (stage->minOffsetPosition.z > stage->maxOffsetPosition.z)
                            stage->maxOffsetPosition.z = stage->minOffsetPosition.z;
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

    int onSetMaxOffsetParticle(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
            if (stage == nullptr)
            {
                return lua_error_debug(lua,"invalid particle's stage's index");
            }
            if(setVec3ToMemberParticle(lua,top,
                   &stage->maxOffsetPosition,
                   &stage->minOffsetPosition,
                   &stage->maxOffsetPosition))
                return 0;
            for (int i = 3; i <= top; ++i)
            {
                switch (i)
                {
                    case 3: // x
                    {
                        stage->maxOffsetPosition.x = luaL_checknumber(lua, i);
                        if (stage->maxOffsetPosition.x < stage->minOffsetPosition.x)
                            stage->minOffsetPosition.x = stage->maxOffsetPosition.x;
                    }
                    break;
                    case 4: // y
                    {
                        stage->maxOffsetPosition.y = luaL_checknumber(lua, i);
                        if (stage->maxOffsetPosition.y < stage->minOffsetPosition.y)
                            stage->minOffsetPosition.y = stage->maxOffsetPosition.y;
                    }
                    break;
                    case 5: // z
                    {
                        stage->maxOffsetPosition.z = luaL_checknumber(lua, i);
                        if (stage->maxOffsetPosition.z < stage->minOffsetPosition.z)
                            stage->minOffsetPosition.z = stage->maxOffsetPosition.z;
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

    int onGetMinOffsetParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
        {
            lua_pushnumber(lua, stage->minOffsetPosition.x);
            lua_pushnumber(lua, stage->minOffsetPosition.y);
            lua_pushnumber(lua, stage->minOffsetPosition.z);
        }
        else
        {
            lua_pushnumber(lua, 0);
            lua_pushnumber(lua, 0);
            lua_pushnumber(lua, 0);
        }
        return 3;
    }

    int onGetMaxOffsetParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
        {
            lua_pushnumber(lua, stage->maxOffsetPosition.x);
            lua_pushnumber(lua, stage->maxOffsetPosition.y);
            lua_pushnumber(lua, stage->maxOffsetPosition.z);
        }
        else
        {
            lua_pushnumber(lua, 0);
            lua_pushnumber(lua, 0);
            lua_pushnumber(lua, 0);
        }
        return 3;
    }

    int onSetMinDirectionParticle(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
            if (stage == nullptr)
            {
                return lua_error_debug(lua,"invalid particle's stage's index");
            }
            if(setVec3ToMemberParticle(lua,top,
                   &stage->minDirection,
                   &stage->minDirection,
                   &stage->maxDirection))
                return 0;
            for (int i = 3; i <= top; ++i)
            {
                switch (i)
                {
                    case 3: // x
                    {
                        stage->minDirection.x = luaL_checknumber(lua, i);
                    }
                    break;
                    case 4: // y
                    {
                        stage->minDirection.y = luaL_checknumber(lua, i);
                    }
                    break;
                    case 5: // z
                    {
                        stage->minDirection.z = luaL_checknumber(lua, i);
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

    int onSetMaxDirectionParticle(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
            if (stage == nullptr)
            {
                return lua_error_debug(lua,"invalid particle's stage's index");
            }
            if(setVec3ToMemberParticle(lua,top,
                   &stage->maxDirection,
                   &stage->minDirection,
                   &stage->maxDirection))
                return 0;
            for (int i = 3; i <= top; ++i)
            {
                switch (i)
                {
                    case 3: // x
                    {
                        stage->maxDirection.x = luaL_checknumber(lua, i);
                    }
                    break;
                    case 4: // y
                    {
                        stage->maxDirection.y = luaL_checknumber(lua, i);
                    }
                    break;
                    case 5: // z
                    {
                        stage->maxDirection.z = luaL_checknumber(lua, i);
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

    int onGetMinDirectionParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
        {
            lua_pushnumber(lua, stage->minDirection.x);
            lua_pushnumber(lua, stage->minDirection.y);
            lua_pushnumber(lua, stage->minDirection.z);
        }
        else
        {
            lua_pushnumber(lua, 0);
            lua_pushnumber(lua, 0);
            lua_pushnumber(lua, 0);
        }
        return 3;
    }

    int onGetMaxDirectionParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
        {
            lua_pushnumber(lua, stage->maxDirection.x);
            lua_pushnumber(lua, stage->maxDirection.y);
            lua_pushnumber(lua, stage->maxDirection.z);
        }
        else
        {
            lua_pushnumber(lua, 0);
            lua_pushnumber(lua, 0);
            lua_pushnumber(lua, 0);
        }
        return 3;
    }

    int onSetMinColorParticle(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
            if (stage == nullptr)
            {
                return lua_error_debug(lua,"invalid particle's stage's index");
            }
            if(setVec3ToMemberParticle(lua,top,
                   &stage->minColor,
                   &stage->minColor,
                   &stage->maxColor))
                return 0;
            for (int i = 3; i <= top; ++i)
            {
                switch (i)
                {
                    case 3: // r
                    {
                        stage->minColor.x = luaL_checknumber(lua, i);
                        if (stage->minColor.x > stage->maxColor.x)
                            stage->maxColor.x = stage->minColor.x;
                    }
                    break;
                    case 4: // g
                    {
                        stage->minColor.y = luaL_checknumber(lua, i);
                        if (stage->minColor.y > stage->maxColor.y)
                            stage->maxColor.y = stage->minColor.y;
                    }
                    break;
                    case 5: // b
                    {
                        stage->minColor.z = luaL_checknumber(lua, i);
                        if (stage->minColor.z > stage->maxColor.z)
                            stage->maxColor.z = stage->minColor.z;
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

    int onGetMinColorParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
        {
            lua_pushnumber(lua, stage->minColor.x);
            lua_pushnumber(lua, stage->minColor.y);
            lua_pushnumber(lua, stage->minColor.z);
        }
        else
        {
            lua_pushnumber(lua, 0);
            lua_pushnumber(lua, 0);
            lua_pushnumber(lua, 0);
        }
        return 3;
    }

    int onGetMaxColorParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
        {
            lua_pushnumber(lua, stage->maxColor.x);
            lua_pushnumber(lua, stage->maxColor.y);
            lua_pushnumber(lua, stage->maxColor.z);
        }
        else
        {
            lua_pushnumber(lua, 0);
            lua_pushnumber(lua, 0);
            lua_pushnumber(lua, 0);
        }
        return 3;
    }

    int onSetMaxColorParticle(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
            if (stage == nullptr)
            {
                return lua_error_debug(lua,"invalid particle's stage's index");
            }
            if(setVec3ToMemberParticle(lua,top,
                   &stage->maxColor,
                   &stage->minColor,
                   &stage->maxColor))
                return 0;
            for (int i = 3; i <= top; ++i)
            {
                switch (i)
                {
                    case 3: // r
                    {
                        stage->maxColor.x = luaL_checknumber(lua, i);
                        if (stage->maxColor.x < stage->minColor.x)
                            stage->minColor.x = stage->maxColor.x;
                    }
                    break;
                    case 4: // g
                    {
                        stage->maxColor.y = luaL_checknumber(lua, i);
                        if (stage->maxColor.y < stage->minColor.y)
                            stage->minColor.y = stage->maxColor.y;
                    }
                    break;
                    case 5: // b
                    {
                        stage->maxColor.z = luaL_checknumber(lua, i);
                        if (stage->maxColor.z < stage->minColor.z)
                            stage->minColor.z = stage->maxColor.z;
                    }
                    break;
                    default: {}
                    break;
                }
            }
        }
        return 0;
    }

    int onGetMinSizeParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage == nullptr)
            lua_pushnumber(lua, 0);
        else
            lua_pushnumber(lua, stage->minSizeParticle);
        return 1;
    }

    int onSetMinSizeParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
        {
            stage->minSizeParticle = luaL_checknumber(lua, 3);
            if (stage->minSizeParticle > stage->maxSizeParticle)
                stage->maxSizeParticle = stage->minSizeParticle + 1.0f;
            return 0;
        }
        else
        {
            return lua_error_debug(lua,"invalid particle's stage's index");
        }
    }

    int onGetMaxSizeParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage == nullptr)
            lua_pushnumber(lua, 0);
        else
            lua_pushnumber(lua, stage->maxSizeParticle);
        return 1;
    }

    int onSetMaxSizeParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
        {
            stage->maxSizeParticle = luaL_checknumber(lua, 3);
            if (stage->maxSizeParticle < stage->minSizeParticle)
                stage->minSizeParticle = stage->maxSizeParticle - 1.0f;
            return 0;
        }
        else
        {
            return lua_error_debug(lua,"invalid particle's stage's index");
        }
    }

    int onGetMinSpeedParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage == nullptr)
            lua_pushnumber(lua, 0);
        else
            lua_pushnumber(lua, stage->minSpeed);
        return 1;
    }

    int onGetMaxSpeedParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage == nullptr)
            lua_pushnumber(lua, 0);
        else
            lua_pushnumber(lua, stage->maxSpeed);
        return 1;
    }

    int onSetMinSpeedParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
        {
            stage->minSpeed = luaL_checknumber(lua, 3);
            if (stage->minSpeed > stage->maxSpeed)
                stage->maxSpeed = stage->minSpeed;
            return 0;
        }
        else
        {
            return lua_error_debug(lua,"invalid particle's stage's index");
        }
    }

    int onSetMaxSpeedParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
        {
            stage->maxSpeed = luaL_checknumber(lua, 3);
            if (stage->maxSpeed < stage->minSpeed)
                stage->minSpeed = stage->maxSpeed;
        }
        return 0;
    }

    int onGetMinLifeTimeParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage == nullptr)
            lua_pushnumber(lua, 0);
        else
            lua_pushnumber(lua, stage->minTimeLife);
        return 1;
    }

    int onSetMinLifeTimeParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
        {
            stage->minTimeLife = luaL_checknumber(lua, 3);
            if (stage->minTimeLife > stage->maxTimeLife)
                stage->maxTimeLife = stage->minTimeLife;
            return 0;
        }
        else
        {
            return lua_error_debug(lua,"invalid particle's stage's index");
        }
    }

    int onGetMaxLifeTimeParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage == nullptr)
            lua_pushnumber(lua, 0);
        else
            lua_pushnumber(lua, stage->maxTimeLife);
        return 1;
    }

    int onSetMaxLifeTimeParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
        {
            stage->maxTimeLife = luaL_checknumber(lua, 3);
            if (stage->maxTimeLife < stage->minTimeLife)
                stage->minTimeLife = stage->maxTimeLife;
            return 0;
        }
        else
        {
            return lua_error_debug(lua,"invalid particle's stage's index");
        }
    }

    int onGetTotalAliveParticle(lua_State *lua)
    {
        PARTICLE *particle = getParticleFromRawTable(lua, 1, 1);
        lua_pushnumber(lua, static_cast<lua_Number>(particle->getTotalParticleAlive()));
        return 1;
    }

    int onGetTotalParticle(lua_State *lua)
    {
        const int          top          = lua_gettop(lua);
        PARTICLE *particle = getParticleFromRawTable(lua, 1, 1);
        if(top > 1)
        {
            const unsigned int index = luaL_checkinteger(lua, 2)-1;
            lua_pushnumber(lua, static_cast<lua_Number>(particle->getTotalParticleByStage(index)));
        }
        else
        {
            lua_pushnumber(lua, static_cast<lua_Number>(particle->getTotalParticle()));
        }
        return 1;
    }

    int onSetTotalParticle(lua_State *lua)
    {
        PARTICLE *particle = getParticleFromRawTable(lua, 1, 1);
        const unsigned int index = luaL_checkinteger(lua, 2)-1;
        const unsigned int total = luaL_checkinteger(lua, 3);
        particle->setTotalParticleByStage(index,total);
        return 0;
    }

    int onGetTimeAriseParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if(stage)
            lua_pushnumber(lua, stage->ariseTime);
        else
            lua_pushnumber(lua, 0);
        return 1;
    }

    int onSetTimeAriseParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
        {
            stage->ariseTime = luaL_checknumber(lua, 3);
            return 0;
        }
        else
        {
            return lua_error_debug(lua,"invalid particle's stage's index");
        }
    }

    int onGetStageTimeParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
            lua_pushnumber(lua, stage->stageTime);
        else
            lua_pushnumber(lua, 0);
        return 1;
    }

    int onSetStageTimeParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
            stage->stageTime = luaL_checknumber(lua, 3);
        return 0;
    }

    int onRestartAnimParticle(lua_State *lua)
    {
        PARTICLE *particle = getParticleFromRawTable(lua, 1, 1);
        particle->restartAnimationParticle();
        return 0;
    }

    int onAddAnimationParticle(lua_State *)
    {
        WARN_LOG("You can not add animation to particle.\nuse [addStage] instead of");
        return 0;
    }

    int onSetInvertedColorParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
        {
            const auto r = (char)lua_toboolean(lua, 3);
            const auto g = (char)lua_toboolean(lua, 4);
            const auto b = (char)lua_toboolean(lua, 5);
            const auto a = (char)lua_toboolean(lua, 6);
            stage->invert_red   = r;
            stage->invert_green = g;
            stage->invert_blue  = b;
            stage->invert_alpha = a;
        }
        return 0;
    }

    int onGetInvertedColorParticle(lua_State *lua)
    {
        util::STAGE_PARTICLE* stage = getSelectedStageLua(lua);
        if (stage)
        {
            lua_pushboolean(lua, stage->invert_red);
            lua_pushboolean(lua, stage->invert_green);
            lua_pushboolean(lua, stage->invert_blue);
            lua_pushboolean(lua, stage->invert_alpha);
        }
        else
        {
            lua_pushboolean(lua, 0);
            lua_pushboolean(lua, 0);
            lua_pushboolean(lua, 0);
            lua_pushboolean(lua, 0);
        }
        return 4;
    }

    int onGetTextureParticle(lua_State *lua)
    {
        PARTICLE * particle = getParticleFromRawTable(lua, 1, 1);
        const char* texture = particle->getTextureFileName();
        if (texture)
            lua_pushstring(lua, texture);
        else
            lua_pushnil(lua);
        return 1;
    }

    int onGetStageParticle(lua_State *lua)
    {
        PARTICLE * particle = getParticleFromRawTable(lua, 1, 1);
        lua_pushnumber(lua, static_cast<lua_Number>(particle->getIndexStageParticle() + 1));
        lua_pushnumber(lua, static_cast<lua_Number>(particle->getTotalStage()));
        return 2;
    }

    int onSetStageParticle(lua_State *lua)
    {
        PARTICLE * particle = getParticleFromRawTable(lua, 1, 1);
        const unsigned int index = luaL_checkinteger(lua, 2)-1;
        particle->setIndexStageParticle(index);
        return 0;
    }
     
    int onAddStageParticle(lua_State *lua)
    {
        PARTICLE * particle = getParticleFromRawTable(lua, 1, 1);
        const auto total = static_cast<lua_Number>(particle->addStage());
        lua_pushnumber(lua, total);
        return 1;
    }

    int onGetTotalStageParticle(lua_State *lua)
    {
        PARTICLE * particle = getParticleFromRawTable(lua, 1, 1);
        const auto total = static_cast<lua_Number>(particle->getTotalStage());
        lua_pushnumber(lua, total);
        return 1;
    }

	int onNewParticleNoGcLua(lua_State *lua,RENDERIZABLE * renderizable)
	{
		lua_settop(lua,0);
		if(renderizable == nullptr || renderizable->userData != nullptr)
			return false;
		
		//table
		luaL_Reg  regParticleMethods[] = {{"load", onLoadParticleLua},
                                         {"add", onAddParticleLua},
                                         {"setMinOffset", onSetMinOffsetParticle},
                                         {"setMaxOffset", onSetMaxOffsetParticle},
                                         {"getMinOffset", onGetMinOffsetParticle},
                                         {"getMaxOffset", onGetMaxOffsetParticle},
                                         {"setMinDirection", onSetMinDirectionParticle},
                                         {"setMaxDirection", onSetMaxDirectionParticle},
                                         {"getMinDirection", onGetMinDirectionParticle},
                                         {"getMaxDirection", onGetMaxDirectionParticle},
                                         {"setMinColor", onSetMinColorParticle},
                                         {"setMaxColor", onSetMaxColorParticle},
                                         {"getMinColor", onGetMinColorParticle},
                                         {"getMaxColor", onGetMaxColorParticle},
                                         {"getMinSize", onGetMinSizeParticle},
                                         {"setMinSize", onSetMinSizeParticle},
                                         {"getMaxSize", onGetMaxSizeParticle},
                                         {"setMaxSize", onSetMaxSizeParticle},
                                         {"getMinSpeed", onGetMinSpeedParticle},
                                         {"getMaxSpeed", onGetMaxSpeedParticle},
                                         {"setMinSpeed", onSetMinSpeedParticle},
                                         {"setMaxSpeed", onSetMaxSpeedParticle},
                                         {"getMinLifeTime", onGetMinLifeTimeParticle},
                                         {"setMinLifeTime", onSetMinLifeTimeParticle},
                                         {"getMaxLifeTime", onGetMaxLifeTimeParticle},
                                         {"setMaxLifeTime", onSetMaxLifeTimeParticle},
                                         {"getTotalAlive", onGetTotalAliveParticle},
                                         {"getTotalParticle", onGetTotalParticle },
                                         {"setTotalParticle", onSetTotalParticle },
                                         {"getStage", onGetStageParticle },
                                         {"setStage", onSetStageParticle },
                                         {"addStage", onAddStageParticle },
                                         {"getTotalStage", onGetTotalStageParticle },
                                         {"getAriseTime", onGetTimeAriseParticle },
                                         {"setAriseTime", onSetTimeAriseParticle },
                                         {"getStageTime", onGetStageTimeParticle },
                                         {"setStageTime", onSetStageTimeParticle },
                                         {"setInvertedColor", onSetInvertedColorParticle },
                                         {"getInvertedColor", onGetInvertedColorParticle },
                                         {"getTexture", onGetTextureParticle},
                                         {nullptr, nullptr}};

		luaL_Reg regParticleReplaceMethods[] = {
                                                { "restartAnim",    onRestartAnimParticle },
                                                { "addAnim",        onAddAnimationParticle },
                                                { nullptr, nullptr } };
        SELF_ADD_COMMON_METHODS selfMethods(regParticleMethods);
        const luaL_Reg *             regMethods = selfMethods.get(regParticleReplaceMethods);

		lua_createtable(lua, 0, selfMethods.getSize());//the table renderizable
		luaL_setfuncs(lua, regMethods, 0);//Registers all functions in the array l (see luaL_Reg) into the table on the top of the stack 
        

		//Metatable
		luaL_Reg regParticleMethodsMetaTable[] = {
                                         {"__newindex", onNewIndexParticle},
                                         {"__index", onIndexParticle},
                                         {"__gc", onDestroyParticleNoGcLua},
                                         {"__close", onDestroyRenderizable},
                                         {nullptr, nullptr}};

		lua_newtable(lua);//the metatable from renderizable
		luaL_setfuncs(lua, regParticleMethodsMetaTable, 0);//Registers all functions in the array l (see luaL_Reg) into the table on the top of the stack 
		lua_setmetatable(lua,-2);
		
		auto ** udata             = static_cast<PARTICLE **>(lua_newuserdata(lua, sizeof(PARTICLE *)));
        auto particle             = static_cast<PARTICLE*>(renderizable);
		auto user_data            = new USER_DATA_RENDER_LUA();
        renderizable->userData    = user_data;
        *udata                    = particle;
        
		/* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_PARTICLE);
        luaL_getmetatable(lua,__userdata_name);
		lua_setmetatable(lua,-2);//metatable from user data
		/* end trick */

        lua_rawseti(lua, -2, 1);//userdata as raw index in the table renderizable
		
		user_data->refTableLua(lua, 1, &user_data->ref_MeAsTable);//always ref to be able to retrieve 
		return 1;
	}

    int onNewParticleLua(lua_State *lua)
    {
        const int top                  = lua_gettop(lua);
        luaL_Reg  regParticleMethods[] = {{"load", onLoadParticleLua},
                                         {"add", onAddParticleLua},
                                         {"setMinOffset", onSetMinOffsetParticle},
                                         {"setMaxOffset", onSetMaxOffsetParticle},
                                         {"getMinOffset", onGetMinOffsetParticle},
                                         {"getMaxOffset", onGetMaxOffsetParticle},
                                         {"setMinDirection", onSetMinDirectionParticle},
                                         {"setMaxDirection", onSetMaxDirectionParticle},
                                         {"getMinDirection", onGetMinDirectionParticle},
                                         {"getMaxDirection", onGetMaxDirectionParticle},
                                         {"setMinColor", onSetMinColorParticle},
                                         {"setMaxColor", onSetMaxColorParticle},
                                         {"getMinColor", onGetMinColorParticle},
                                         {"getMaxColor", onGetMaxColorParticle},
                                         {"getMinSize", onGetMinSizeParticle},
                                         {"setMinSize", onSetMinSizeParticle},
                                         {"getMaxSize", onGetMaxSizeParticle},
                                         {"setMaxSize", onSetMaxSizeParticle},
                                         {"getMinSpeed", onGetMinSpeedParticle},
                                         {"getMaxSpeed", onGetMaxSpeedParticle},
                                         {"setMinSpeed", onSetMinSpeedParticle},
                                         {"setMaxSpeed", onSetMaxSpeedParticle},
                                         {"getMinLifeTime", onGetMinLifeTimeParticle},
                                         {"setMinLifeTime", onSetMinLifeTimeParticle},
                                         {"getMaxLifeTime", onGetMaxLifeTimeParticle},
                                         {"setMaxLifeTime", onSetMaxLifeTimeParticle},
                                         {"getTotalAlive", onGetTotalAliveParticle},
                                         {"getTotalParticle", onGetTotalParticle },
                                         {"setTotalParticle", onSetTotalParticle },
                                         {"getStage", onGetStageParticle },
                                         {"setStage", onSetStageParticle },
                                         {"addStage", onAddStageParticle },
                                         {"getTotalStage", onGetTotalStageParticle },
                                         {"getAriseTime", onGetTimeAriseParticle },
                                         {"setAriseTime", onSetTimeAriseParticle },
                                         {"getStageTime", onGetStageTimeParticle },
                                         {"setStageTime", onSetStageTimeParticle },
                                         {"setInvertedColor", onSetInvertedColorParticle },
                                         {"getInvertedColor", onGetInvertedColorParticle },
                                         {"getTexture", onGetTextureParticle},
                                         {nullptr, nullptr}};
        luaL_Reg regParticleReplaceMethods[] = {
                                                { "restartAnim",    onRestartAnimParticle },
                                                { "addAnim",        onAddAnimationParticle },
                                                { nullptr, nullptr } };
        SELF_ADD_COMMON_METHODS selfMethods(regParticleMethods);
        const luaL_Reg *             regMethods = selfMethods.get(regParticleReplaceMethods);
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
        lua_createtable(lua, 0, selfMethods.getSize());
        luaL_setfuncs(lua, regMethods, 0);

        luaL_getmetatable(lua, "_mbmParticle");
        lua_setmetatable(lua, -2);

		auto **      udata    = static_cast<PARTICLE **>(lua_newuserdata(lua, sizeof(PARTICLE *)));
        DEVICE *device   = DEVICE::getInstance();
        auto    particle = new PARTICLE(device->scene, is3d, is2ds);
        particle->userData    = new USER_DATA_RENDER_LUA();
        *udata                = particle;
        if (position.x != 0.0f) //-V550
            particle->position.x = position.x;
        if (position.y != 0.0f) //-V550
            particle->position.y = position.y;
        if (position.z != 0.0f) //-V550
            particle->position.z = position.z;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_PARTICLE);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);

        return 1;
    }

    void registerClassParticle(lua_State *lua)
    {
        luaL_Reg regParticleMethods[] = {{"new", onNewParticleLua},
                                         {"__newindex", onNewIndexParticle},
                                         {"__index", onIndexParticle},
                                         {"__gc", onDestroyParticleLua},
                                         {"__close", onDestroyRenderizable},
                                         {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmParticle");
        luaL_setfuncs(lua, regParticleMethods, 0);
        lua_setglobal(lua, "particle");
        lua_settop(lua,0);
    }
};
