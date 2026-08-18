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

#include <cstring>
#include <cstdlib>
#include <vector>
#include <plugin-helper/user-data-lua.h>
#include <lua-wrap/common-methods-lua.h>
#include <render/mesh.h>
#include <platform/mismatch-platform.h>
#include <core_mbm/scene.h>

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
        auto *userData = static_cast<USER_DATA_RENDER_LUA *>(mesh->getUserData());
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        mesh->setUserData(nullptr);
    #if DEBUG_FREE_LUA
        const char *fileName = mesh->getFileName();
        static int  num      = 1;
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",mesh->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->getScene()->getUserData());
        userScene->remove(mesh);
        delete mesh;
        return 0;
    }

	int onDestroyNoGcMeshLua(lua_State *lua)
    {
        MESH *                mesh     = getMeshFromRawTable(lua, 1, 1);
        auto *userData = static_cast<USER_DATA_RENDER_LUA *>(mesh->getUserData());
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        mesh->setUserData(nullptr);
    #if DEBUG_FREE_LUA
        const char *fileName = mesh->getFileName();
        static int  num      = 1;
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",mesh->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->getScene()->getUserData());
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

    int onPlayArticulatedAnimationLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        const char *name = luaL_checkstring(lua, 2);
        const int priority = static_cast<int>(luaL_optinteger(lua, 3, 0));
        const float blendDuration = static_cast<float>(luaL_optnumber(lua, 4, 0.0));
        const float weight = static_cast<float>(luaL_optnumber(lua, 5, 1.0));
        lua_pushboolean(lua, mesh->playArticulatedAnimation(
            name, priority, blendDuration, weight) ? 1 : 0);
        return 1;
    }

    int onGetTotalArticulatedAnimationsLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushinteger(lua, static_cast<lua_Integer>(mesh->getTotalArticulatedAnimations()));
        return 1;
    }

    int onGetArticulatedAnimationNameLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        const lua_Integer index = luaL_checkinteger(lua, 2);
        const char *name = index > 0
            ? mesh->getArticulatedAnimationName(static_cast<uint32_t>(index - 1)) : nullptr;
        if (name)
            lua_pushstring(lua, name);
        else
            lua_pushnil(lua);
        return 1;
    }

    int onPauseArticulatedAnimationLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->pauseArticulatedAnimation(luaL_checkstring(lua, 2)) ? 1 : 0);
        return 1;
    }

    int onResumeArticulatedAnimationLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->resumeArticulatedAnimation(luaL_checkstring(lua, 2)) ? 1 : 0);
        return 1;
    }

    int onDisableArticulatedAnimationLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->disableArticulatedAnimation(luaL_checkstring(lua, 2)) ? 1 : 0);
        return 1;
    }

    int onSeekArticulatedAnimationLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        const char *name = luaL_checkstring(lua, 2);
        const float time = static_cast<float>(luaL_checknumber(lua, 3));
        lua_pushboolean(lua, mesh->seekArticulatedAnimation(name, time) ? 1 : 0);
        return 1;
    }

    int onGetArticulatedAnimationTimeLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        const char *name = luaL_checkstring(lua, 2);
        float time = 0.0f;
        if (mesh->getArticulatedAnimationTime(name, &time))
            lua_pushnumber(lua, time);
        else
            lua_pushnil(lua);
        return 1;
    }

    int onGetTotalSkeletalAnimationsLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushinteger(lua, static_cast<lua_Integer>(mesh->getTotalSkeletalAnimations()));
        return 1;
    }

    int onGetSkeletalAnimationNameLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        const lua_Integer index = luaL_checkinteger(lua, 2);
        const char *name = index > 0
            ? mesh->getSkeletalAnimationName(static_cast<uint32_t>(index - 1)) : nullptr;
        if (name) lua_pushstring(lua, name); else lua_pushnil(lua);
        return 1;
    }

    int onGetSkeletalAnimationDurationLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        const lua_Integer index = luaL_checkinteger(lua, 2);
        float duration = 0.0f;
        if (index > 0 && mesh->getSkeletalAnimationDuration(static_cast<uint32_t>(index - 1), &duration))
            lua_pushnumber(lua, duration);
        else
            lua_pushnil(lua);
        return 1;
    }

    int onSetSkeletalSkinningMethodLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        const char *method = luaL_checkstring(lua, 2);
        SKELETAL_SHADER_METHOD selected;
        if (std::strcmp(method, "lbs") == 0)
            selected = SKELETAL_SHADER_METHOD::LBS;
        else if (std::strcmp(method, "dqs") == 0)
            selected = SKELETAL_SHADER_METHOD::DQS_RIGID;
        else if (std::strcmp(method, "auto") == 0)
            selected = SKELETAL_SHADER_METHOD::AUTO;
        else
            return luaL_error(lua, "skeletal skinning method must be 'lbs', 'dqs', or 'auto'");
        lua_pushboolean(lua, mesh->setSkeletalSkinningMethod(selected) ? 1 : 0);
        return 1;
    }

    int onGetSkeletalSkinningMethodLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        const SKELETAL_SHADER_METHOD method = mesh->getSkeletalSkinningMethod();
        lua_pushstring(lua, method == SKELETAL_SHADER_METHOD::AUTO ? "auto" :
            method == SKELETAL_SHADER_METHOD::DQS_RIGID ? "dqs" : "lbs");
        return 1;
    }

    int onGetResolvedSkeletalSkinningMethodLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        const SKELETAL_SHADER_METHOD method = mesh->getResolvedSkeletalSkinningMethod();
        lua_pushstring(lua, method == SKELETAL_SHADER_METHOD::NONE ? "unresolved" :
            method == SKELETAL_SHADER_METHOD::DQS_RIGID ? "dqs" : "lbs");
        return 1;
    }

    int onGetSkeletalSkinningReportLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        const char *status = nullptr, *resolutionReason = nullptr;
        uint32_t requiredBoneCount = 0, effectiveBoneCapacity = 0;
        mesh->getSkeletalSkinningReport(&status, &resolutionReason, &requiredBoneCount,
                                        &effectiveBoneCapacity);
        lua_createtable(lua, 0, 6);
        lua_pushstring(lua, status ? status : "unknown");
        lua_setfield(lua, -2, "status");
        lua_pushinteger(lua, static_cast<lua_Integer>(requiredBoneCount));
        lua_setfield(lua, -2, "requiredBoneCount");
        lua_pushinteger(lua, static_cast<lua_Integer>(effectiveBoneCapacity));
        lua_setfield(lua, -2, "effectiveBoneCapacity");
        const SKELETAL_SHADER_METHOD requested = mesh->getSkeletalSkinningMethod();
        lua_pushstring(lua, requested == SKELETAL_SHADER_METHOD::AUTO ? "auto" :
            requested == SKELETAL_SHADER_METHOD::DQS_RIGID ? "dqs" : "lbs");
        lua_setfield(lua, -2, "requestedMethod");
        const SKELETAL_SHADER_METHOD resolved = mesh->getResolvedSkeletalSkinningMethod();
        lua_pushstring(lua, resolved == SKELETAL_SHADER_METHOD::NONE ? "unresolved" :
            resolved == SKELETAL_SHADER_METHOD::DQS_RIGID ? "dqs" : "lbs");
        lua_setfield(lua, -2, "resolvedMethod");
        lua_pushstring(lua, resolutionReason ? resolutionReason : "unknown");
        lua_setfield(lua, -2, "resolutionReason");
        return 1;
    }

    int onSetSkeletalAuthoringPaletteLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        const char *methodName = luaL_checkstring(lua, 2);
        const SKELETAL_SHADER_METHOD method = std::strcmp(methodName, "dqs") == 0
            ? SKELETAL_SHADER_METHOD::DQS_RIGID : std::strcmp(methodName, "lbs") == 0
            ? SKELETAL_SHADER_METHOD::LBS : SKELETAL_SHADER_METHOD::NONE;
        luaL_checktype(lua, 3, LUA_TTABLE);
        const size_t count = lua_rawlen(lua, 3);
        std::vector<float> rows(count);
        for (size_t index = 0; index < count; ++index)
        {
            lua_rawgeti(lua, 3, static_cast<lua_Integer>(index + 1));
            rows[index] = static_cast<float>(luaL_checknumber(lua, -1));
            lua_pop(lua, 1);
        }
        const float time = static_cast<float>(luaL_checknumber(lua, 4));
        luaL_checktype(lua, 5, LUA_TTABLE);
        const size_t boneCount=lua_rawlen(lua,5);
        std::vector<uint64_t> boneIds(boneCount);
        for (size_t index=0; index<boneCount; ++index)
        {
            lua_rawgeti(lua,5,static_cast<lua_Integer>(index+1));
            const char *boneId=luaL_checkstring(lua,-1);
            char *end=nullptr;
            boneIds[index]=static_cast<uint64_t>(std::strtoull(boneId,&end,16));
            if (!end || end==boneId || *end!='\0')
                return luaL_error(lua,"ordered bone identity %d is not hexadecimal",static_cast<int>(index+1));
            lua_pop(lua,1);
        }
        char errorOut[255]="";
        const bool applied=mesh->setSkeletalAuthoringPalette(method, rows.data(),
            static_cast<uint32_t>(rows.size()),boneIds.data(),
            static_cast<uint32_t>(boneIds.size()),time,errorOut,static_cast<int>(sizeof(errorOut)));
        lua_pushboolean(lua,applied);
        if (applied) lua_pushnil(lua); else lua_pushstring(lua,errorOut);
        return 2;
    }

    int onPlaySkeletalAnimationLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->playSkeletalAnimation(luaL_checkstring(lua, 2)) ? 1 : 0);
        return 1;
    }

    int onCrossFadeSkeletalAnimationLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->crossFadeSkeletalAnimation(
            luaL_checkstring(lua, 2), static_cast<float>(luaL_checknumber(lua, 3))) ? 1 : 0);
        return 1;
    }

    int onPauseSkeletalAnimationLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->pauseSkeletalAnimation() ? 1 : 0);
        return 1;
    }

    int onResumeSkeletalAnimationLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->resumeSkeletalAnimation() ? 1 : 0);
        return 1;
    }

    int onStopSkeletalAnimationLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->stopSkeletalAnimation() ? 1 : 0);
        return 1;
    }

    int onSeekSkeletalAnimationLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->seekSkeletalAnimation(
            static_cast<float>(luaL_checknumber(lua, 2))) ? 1 : 0);
        return 1;
    }

    int onGetSkeletalAnimationTimeLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        float time = 0.0f;
        if (mesh->getSkeletalAnimationTime(&time)) lua_pushnumber(lua, time); else lua_pushnil(lua);
        return 1;
    }

    int onSetSkeletalAnimationPlaybackSpeedLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->setSkeletalAnimationPlaybackSpeed(
            static_cast<float>(luaL_checknumber(lua, 2))) ? 1 : 0);
        return 1;
    }

    int onGetSkeletalAnimationPlaybackSpeedLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushnumber(lua, mesh->getSkeletalAnimationPlaybackSpeed());
        return 1;
    }

    int onPlaySkeletalAnimationAbsoluteLayerLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->playSkeletalAnimationAbsoluteLayer(
            luaL_checkstring(lua, 2), static_cast<float>(luaL_checknumber(lua, 3))) ? 1 : 0);
        return 1;
    }

    int onPlaySkeletalAnimationAdditiveLayerLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->playSkeletalAnimationAdditiveLayer(
            luaL_checkstring(lua, 2), static_cast<float>(luaL_checknumber(lua, 3))) ? 1 : 0);
        return 1;
    }

    int onPauseSkeletalAnimationLayerLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->pauseSkeletalAnimationLayer() ? 1 : 0);
        return 1;
    }

    int onResumeSkeletalAnimationLayerLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->resumeSkeletalAnimationLayer() ? 1 : 0);
        return 1;
    }

    int onIsSkeletalAnimationLayerPausedLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->isSkeletalAnimationLayerPaused() ? 1 : 0);
        return 1;
    }

    int onStopSkeletalAnimationAbsoluteLayerLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->stopSkeletalAnimationAbsoluteLayer() ? 1 : 0);
        return 1;
    }

    int onSeekSkeletalAnimationAbsoluteLayerLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->seekSkeletalAnimationAbsoluteLayer(
            static_cast<float>(luaL_checknumber(lua, 2))) ? 1 : 0);
        return 1;
    }

    int onSetSkeletalAnimationAbsoluteLayerWeightLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->setSkeletalAnimationAbsoluteLayerWeight(
            static_cast<float>(luaL_checknumber(lua, 2))) ? 1 : 0);
        return 1;
    }

    int onFadeSkeletalAnimationAbsoluteLayerLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->fadeSkeletalAnimationAbsoluteLayer(
            static_cast<float>(luaL_checknumber(lua, 2)),
            static_cast<float>(luaL_checknumber(lua, 3))) ? 1 : 0);
        return 1;
    }

    int onGetSkeletalAnimationAbsoluteLayerWeightLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        float weight = 0.0f;
        if (mesh->getSkeletalAnimationAbsoluteLayerWeight(&weight)) lua_pushnumber(lua, weight);
        else lua_pushnil(lua);
        return 1;
    }

    int onGetSkeletalAnimationAbsoluteLayerTimeLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        float time = 0.0f;
        if (mesh->getSkeletalAnimationAbsoluteLayerTime(&time)) lua_pushnumber(lua, time);
        else lua_pushnil(lua);
        return 1;
    }

    uint64_t checkSkeletalBoneId(lua_State *lua, const int index)
    {
        const char *text = luaL_checkstring(lua, index);
        char *end = nullptr;
        const uint64_t id = static_cast<uint64_t>(std::strtoull(text, &end, 16));
        if (!text[0] || !end || end[0] != '\0' || id == 0)
            luaL_error(lua, "skeletal bone id must be a nonzero hexadecimal string");
        return id;
    }

    int onSetSkeletalAnimationLayerBoneWeightLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->setSkeletalAnimationLayerBoneWeight(
            checkSkeletalBoneId(lua, 2), static_cast<float>(luaL_checknumber(lua, 3))) ? 1 : 0);
        return 1;
    }

    int onSetSkeletalAnimationLayerBoneWeightsLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        luaL_checktype(lua, 2, LUA_TTABLE);
        const size_t count = lua_rawlen(lua, 2);
        if (count == 0 || count > UINT32_MAX)
            return luaL_error(lua, "skeletal layer mask batch must be a nonempty array");
        std::vector<uint64_t> boneIds(count);
        std::vector<float> weights(count);
        for (size_t index = 0; index < count; ++index)
        {
            lua_rawgeti(lua, 2, static_cast<lua_Integer>(index + 1));
            luaL_checktype(lua, -1, LUA_TTABLE);
            lua_getfield(lua, -1, "boneId");
            boneIds[index] = checkSkeletalBoneId(lua, -1);
            lua_pop(lua, 1);
            lua_getfield(lua, -1, "weight");
            weights[index] = static_cast<float>(luaL_checknumber(lua, -1));
            lua_pop(lua, 2);
        }
        lua_pushboolean(lua, mesh->setSkeletalAnimationLayerBoneWeights(
            boneIds.data(), weights.data(), static_cast<uint32_t>(count)) ? 1 : 0);
        return 1;
    }

    int onGetSkeletalAnimationLayerBoneWeightLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        float weight = 0.0f;
        if (!mesh->getSkeletalAnimationLayerBoneWeight(checkSkeletalBoneId(lua, 2), &weight))
            return 0;
        lua_pushnumber(lua, weight);
        return 1;
    }

    int onClearSkeletalAnimationLayerMaskLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, mesh->clearSkeletalAnimationLayerMask() ? 1 : 0);
        return 1;
    }

    int onGetSkeletalAnimationPoseLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        const uint32_t count = mesh->getSkeletalAnimationPoseBoneCount();
        if (count == 0)
        {
            lua_pushnil(lua);
            return 1;
        }
        lua_createtable(lua, static_cast<int>(count), 0);
        for (uint32_t index = 0; index < count; ++index)
        {
            uint64_t stableBoneId = 0;
            int32_t parentIndex = -1;
            MATRIX globalMatrix;
            if (!mesh->getSkeletalAnimationPoseBone(index, &stableBoneId, &parentIndex,
                                                     &globalMatrix))
            {
                lua_pop(lua, 1);
                lua_pushnil(lua);
                return 1;
            }
            lua_createtable(lua, 0, 3);
            char boneId[17] = "";
            snprintf(boneId, sizeof(boneId), "%016llx",
                     static_cast<unsigned long long>(stableBoneId));
            lua_pushstring(lua, boneId); lua_setfield(lua, -2, "boneId");
            lua_pushinteger(lua, parentIndex + 1); lua_setfield(lua, -2, "parentIndex");
            lua_createtable(lua, 16, 0);
            for (int matrixIndex = 0; matrixIndex < 16; ++matrixIndex)
            {
                lua_pushnumber(lua, globalMatrix.p[matrixIndex]);
                lua_rawseti(lua, -2, matrixIndex + 1);
            }
            lua_setfield(lua, -2, "globalMatrix");
            lua_rawseti(lua, -2, index + 1);
        }
        return 1;
    }

    int onGetSkeletalBoneTransformLua(lua_State *lua)
    {
        MESH *mesh = getMeshFromRawTable(lua, 1, 1);
        const char *boneName = luaL_checkstring(lua, 2);
        const char *space = luaL_optstring(lua, 3, "model");
        const bool worldSpace = strcmp(space, "world") == 0;
        if (!worldSpace && strcmp(space, "model") != 0)
            return luaL_error(lua, "space must be 'model' or 'world'");

        uint64_t stableBoneId = 0;
        MATRIX matrix;
        VEC3 position;
        float rotation[4] = {};
        VEC3 angle;
        VEC3 scale;
        if (!mesh->getSkeletalBoneTransform(boneName, worldSpace, &stableBoneId, &matrix,
                                            &position, rotation, &angle, &scale))
        {
            lua_pushnil(lua);
            return 1;
        }

        lua_createtable(lua, 0, 7);
        char boneId[17] = "";
        snprintf(boneId, sizeof(boneId), "%016llx",
                 static_cast<unsigned long long>(stableBoneId));
        lua_pushstring(lua, boneId); lua_setfield(lua, -2, "boneId");
        lua_pushstring(lua, space); lua_setfield(lua, -2, "space");

        lua_createtable(lua, 0, 3);
        lua_pushnumber(lua, position.x); lua_setfield(lua, -2, "x");
        lua_pushnumber(lua, position.y); lua_setfield(lua, -2, "y");
        lua_pushnumber(lua, position.z); lua_setfield(lua, -2, "z");
        lua_setfield(lua, -2, "position");

        lua_createtable(lua, 0, 4);
        lua_pushnumber(lua, rotation[0]); lua_setfield(lua, -2, "x");
        lua_pushnumber(lua, rotation[1]); lua_setfield(lua, -2, "y");
        lua_pushnumber(lua, rotation[2]); lua_setfield(lua, -2, "z");
        lua_pushnumber(lua, rotation[3]); lua_setfield(lua, -2, "w");
        lua_setfield(lua, -2, "rotation");

        lua_createtable(lua, 0, 3);
        lua_pushnumber(lua, angle.x); lua_setfield(lua, -2, "x");
        lua_pushnumber(lua, angle.y); lua_setfield(lua, -2, "y");
        lua_pushnumber(lua, angle.z); lua_setfield(lua, -2, "z");
        lua_setfield(lua, -2, "angle");

        lua_createtable(lua, 0, 3);
        lua_pushnumber(lua, scale.x); lua_setfield(lua, -2, "x");
        lua_pushnumber(lua, scale.y); lua_setfield(lua, -2, "y");
        lua_pushnumber(lua, scale.z); lua_setfield(lua, -2, "z");
        lua_setfield(lua, -2, "scale");

        lua_createtable(lua, 16, 0);
        for (int index = 0; index < 16; ++index)
        {
            lua_pushnumber(lua, matrix.p[index]);
            lua_rawseti(lua, -2, index + 1);
        }
        lua_setfield(lua, -2, "matrix");
        return 1;
    }

    // Background-thread-friendly equivalent of "load":
    // mesh:loadAsync(fileName, function(tmesh, success) ... end). The callback's refs (and a ref to
    // `self`) are held in the registry for the pending load's duration - this both lets the callback
    // receive `self` back and keeps the Lua object (and therefore the underlying C++ object, since
    // its __gc won't fire while referenced) alive even if the script drops every other reference to
    // it before the load completes.
    namespace
    {
        struct AsyncLoadCtxMesh
        {
            lua_State *lua;
            int        refCallback;
            int        refSelf;
        };
    }

    int onLoadAsyncMeshLua(lua_State *lua)
    {
        MESH *      mesh     = getMeshFromRawTable(lua, 1, 1);
        const char *fileName = luaL_checkstring(lua, 2);
        if (lua_type(lua, 3) != LUA_TFUNCTION)
            return lua_error_debug(lua, "expected [function] as callback for loadAsync");
        auto *ctx        = new AsyncLoadCtxMesh();
        ctx->lua         = lua;
        lua_pushvalue(lua, 3);
        ctx->refCallback = luaL_ref(lua, LUA_REGISTRYINDEX);
        lua_pushvalue(lua, 1);
        ctx->refSelf     = luaL_ref(lua, LUA_REGISTRYINDEX);
        mesh->loadAsync(fileName, [ctx](bool success)
        {
            lua_State *lua = ctx->lua;
            lua_rawgeti(lua, LUA_REGISTRYINDEX, ctx->refCallback);
            if (lua_isfunction(lua, -1))
            {
                lua_rawgeti(lua, LUA_REGISTRYINDEX, ctx->refSelf);
                lua_pushboolean(lua, success);
                if (lua_pcall(lua, 2, 0, 0))
                    lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
            }
            else
            {
                lua_pop(lua, 1);
            }
            luaL_unref(lua, LUA_REGISTRYINDEX, ctx->refCallback);
            luaL_unref(lua, LUA_REGISTRYINDEX, ctx->refSelf);
            delete ctx;
        });
        return 0;
    }

	int onNewMeshNoGcLua(lua_State *lua,RENDERIZABLE * renderizable)
	{
		lua_settop(lua,0);
		if(renderizable == nullptr || renderizable->getUserData() != nullptr)
			return false;
		
		//table
		luaL_Reg                     regMeshMethods[] = {{"load", onLoadMeshLua}, {"loadAsync", onLoadAsyncMeshLua},
                                                     {"playArticulatedAnimation", onPlayArticulatedAnimationLua},
                                                     {"getTotalArticulatedAnimations", onGetTotalArticulatedAnimationsLua},
                                                     {"getArticulatedAnimationName", onGetArticulatedAnimationNameLua},
                                                     {"pauseArticulatedAnimation", onPauseArticulatedAnimationLua},
                                                     {"resumeArticulatedAnimation", onResumeArticulatedAnimationLua},
                                                     {"disableArticulatedAnimation", onDisableArticulatedAnimationLua},
                                                     {"seekArticulatedAnimation", onSeekArticulatedAnimationLua},
                                                     {"getArticulatedAnimationTime", onGetArticulatedAnimationTimeLua},
                                                     {"getTotalSkeletalAnimations", onGetTotalSkeletalAnimationsLua},
                                                     {"getSkeletalAnimationName", onGetSkeletalAnimationNameLua},
                                                     {"getSkeletalAnimationDuration", onGetSkeletalAnimationDurationLua},
                                                     {"setSkeletalSkinningMethod", onSetSkeletalSkinningMethodLua},
                                                     {"getSkeletalSkinningMethod", onGetSkeletalSkinningMethodLua},
                                                     {"getResolvedSkeletalSkinningMethod", onGetResolvedSkeletalSkinningMethodLua},
                                                     {"getSkeletalSkinningReport", onGetSkeletalSkinningReportLua},
                                                     {"playSkeletalAnimation", onPlaySkeletalAnimationLua},
                                                     {"crossFadeSkeletalAnimation", onCrossFadeSkeletalAnimationLua},
                                                     {"pauseSkeletalAnimation", onPauseSkeletalAnimationLua},
                                                     {"resumeSkeletalAnimation", onResumeSkeletalAnimationLua},
                                                     {"stopSkeletalAnimation", onStopSkeletalAnimationLua},
                                                     {"seekSkeletalAnimation", onSeekSkeletalAnimationLua},
                                                     {"getSkeletalAnimationTime", onGetSkeletalAnimationTimeLua},
                                                     {"setSkeletalAnimationPlaybackSpeed", onSetSkeletalAnimationPlaybackSpeedLua},
                                                     {"getSkeletalAnimationPlaybackSpeed", onGetSkeletalAnimationPlaybackSpeedLua},
                                                     {"playSkeletalAnimationAbsoluteLayer", onPlaySkeletalAnimationAbsoluteLayerLua},
                                                     {"playSkeletalAnimationAdditiveLayer", onPlaySkeletalAnimationAdditiveLayerLua},
                                                     {"pauseSkeletalAnimationLayer", onPauseSkeletalAnimationLayerLua},
                                                     {"resumeSkeletalAnimationLayer", onResumeSkeletalAnimationLayerLua},
                                                     {"isSkeletalAnimationLayerPaused", onIsSkeletalAnimationLayerPausedLua},
                                                     {"stopSkeletalAnimationAbsoluteLayer", onStopSkeletalAnimationAbsoluteLayerLua},
                                                     {"seekSkeletalAnimationAbsoluteLayer", onSeekSkeletalAnimationAbsoluteLayerLua},
                                                     {"setSkeletalAnimationAbsoluteLayerWeight", onSetSkeletalAnimationAbsoluteLayerWeightLua},
                                                     {"fadeSkeletalAnimationAbsoluteLayer", onFadeSkeletalAnimationAbsoluteLayerLua},
                                                     {"getSkeletalAnimationAbsoluteLayerWeight", onGetSkeletalAnimationAbsoluteLayerWeightLua},
                                                     {"getSkeletalAnimationAbsoluteLayerTime", onGetSkeletalAnimationAbsoluteLayerTimeLua},
                                                     {"setSkeletalAnimationLayerBoneWeight", onSetSkeletalAnimationLayerBoneWeightLua},
                                                     {"setSkeletalAnimationLayerBoneWeights", onSetSkeletalAnimationLayerBoneWeightsLua},
                                                     {"getSkeletalAnimationLayerBoneWeight", onGetSkeletalAnimationLayerBoneWeightLua},
                                                     {"clearSkeletalAnimationLayerMask", onClearSkeletalAnimationLayerMaskLua},
                                                     {"getSkeletalAnimationPose", onGetSkeletalAnimationPoseLua},
                                                     {"getSkeletalBoneTransform", onGetSkeletalBoneTransformLua},
                                                     {"setSkeletalAuthoringPalette", onSetSkeletalAuthoringPaletteLua},
                                                     {nullptr, nullptr}};

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
        renderizable->setUserData(user_data);
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
        luaL_Reg                     regMeshMethods[] = {{"load", onLoadMeshLua}, {"loadAsync", onLoadAsyncMeshLua},
                                                         {"playArticulatedAnimation", onPlayArticulatedAnimationLua},
                                                         {"getTotalArticulatedAnimations", onGetTotalArticulatedAnimationsLua},
                                                         {"getArticulatedAnimationName", onGetArticulatedAnimationNameLua},
                                                         {"pauseArticulatedAnimation", onPauseArticulatedAnimationLua},
                                                         {"resumeArticulatedAnimation", onResumeArticulatedAnimationLua},
                                                         {"disableArticulatedAnimation", onDisableArticulatedAnimationLua},
                                                         {"seekArticulatedAnimation", onSeekArticulatedAnimationLua},
                                                         {"getArticulatedAnimationTime", onGetArticulatedAnimationTimeLua},
                                                         {"getTotalSkeletalAnimations", onGetTotalSkeletalAnimationsLua},
                                                         {"getSkeletalAnimationName", onGetSkeletalAnimationNameLua},
                                                         {"getSkeletalAnimationDuration", onGetSkeletalAnimationDurationLua},
                                                         {"setSkeletalSkinningMethod", onSetSkeletalSkinningMethodLua},
                                                         {"getSkeletalSkinningMethod", onGetSkeletalSkinningMethodLua},
                                                         {"getResolvedSkeletalSkinningMethod", onGetResolvedSkeletalSkinningMethodLua},
                                                         {"getSkeletalSkinningReport", onGetSkeletalSkinningReportLua},
                                                         {"playSkeletalAnimation", onPlaySkeletalAnimationLua},
                                                         {"crossFadeSkeletalAnimation", onCrossFadeSkeletalAnimationLua},
                                                         {"pauseSkeletalAnimation", onPauseSkeletalAnimationLua},
                                                         {"resumeSkeletalAnimation", onResumeSkeletalAnimationLua},
                                                         {"stopSkeletalAnimation", onStopSkeletalAnimationLua},
                                                         {"seekSkeletalAnimation", onSeekSkeletalAnimationLua},
                                                         {"getSkeletalAnimationTime", onGetSkeletalAnimationTimeLua},
                                                         {"setSkeletalAnimationPlaybackSpeed", onSetSkeletalAnimationPlaybackSpeedLua},
                                                         {"getSkeletalAnimationPlaybackSpeed", onGetSkeletalAnimationPlaybackSpeedLua},
                                                         {"playSkeletalAnimationAbsoluteLayer", onPlaySkeletalAnimationAbsoluteLayerLua},
                                                         {"playSkeletalAnimationAdditiveLayer", onPlaySkeletalAnimationAdditiveLayerLua},
                                                         {"pauseSkeletalAnimationLayer", onPauseSkeletalAnimationLayerLua},
                                                         {"resumeSkeletalAnimationLayer", onResumeSkeletalAnimationLayerLua},
                                                         {"isSkeletalAnimationLayerPaused", onIsSkeletalAnimationLayerPausedLua},
                                                         {"stopSkeletalAnimationAbsoluteLayer", onStopSkeletalAnimationAbsoluteLayerLua},
                                                         {"seekSkeletalAnimationAbsoluteLayer", onSeekSkeletalAnimationAbsoluteLayerLua},
                                                         {"setSkeletalAnimationAbsoluteLayerWeight", onSetSkeletalAnimationAbsoluteLayerWeightLua},
                                                         {"fadeSkeletalAnimationAbsoluteLayer", onFadeSkeletalAnimationAbsoluteLayerLua},
                                                         {"getSkeletalAnimationAbsoluteLayerWeight", onGetSkeletalAnimationAbsoluteLayerWeightLua},
                                                         {"getSkeletalAnimationAbsoluteLayerTime", onGetSkeletalAnimationAbsoluteLayerTimeLua},
                                                         {"setSkeletalAnimationLayerBoneWeight", onSetSkeletalAnimationLayerBoneWeightLua},
                                                         {"setSkeletalAnimationLayerBoneWeights", onSetSkeletalAnimationLayerBoneWeightsLua},
                                                         {"getSkeletalAnimationLayerBoneWeight", onGetSkeletalAnimationLayerBoneWeightLua},
                                                         {"clearSkeletalAnimationLayerMask", onClearSkeletalAnimationLayerMaskLua},
                                                         {"getSkeletalAnimationPose", onGetSkeletalAnimationPoseLua},
                                                         {"getSkeletalBoneTransform", onGetSkeletalBoneTransformLua},
                                                         {"setSkeletalAuthoringPalette", onSetSkeletalAuthoringPaletteLua},
                                                         {nullptr, nullptr}};
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
        auto        mesh   = new MESH(device->getScene(), is3d, is2ds);
        mesh->setUserData(new USER_DATA_RENDER_LUA());
        *udata              = mesh;
        VEC3 &meshPosition  = mesh->getPosition();
        if (position.x != 0.0f) //-V550
            meshPosition.x = position.x;
        if (position.y != 0.0f) //-V550
            meshPosition.y = position.y;
        if (position.z != 0.0f) //-V550
            meshPosition.z = position.z;

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
