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

#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/render-table/animation-lua.h>
#include <core_mbm/animation.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/device.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/blend.h>

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

namespace mbm
{
    extern int verifyDynamicCast(lua_State *lua, void *ptr, int line, const char *__file);
    extern RENDERIZABLE *getRenderizableFromRawTable(lua_State *lua, const int rawi, const int indexTable);
	extern int lua_error_debug(lua_State *lua, const char *format, ...);

    ANIMATION_MANAGER *getAnimationManagerFromRawTable(lua_State *lua, const int rawi, const int indexTable,RENDERIZABLE **renderizable)
    {
        RENDERIZABLE *        ptr       = getRenderizableFromRawTable(lua, rawi,indexTable);
        auto *animation                 = ptr->getAnimationManager();
        if (animation && renderizable)
            *renderizable = ptr;
        else if(animation == nullptr)
            lua_error_debug(lua, "\nObject [%s] has no animation control\n",ptr->getTypeClassName());
        return animation;
    }

    int onGetAnimationsManagerLua(lua_State *lua)
    {
        const int top                 = lua_gettop(lua);
        const int indexAnim           = top > 1 ? luaL_checkinteger(lua,2) - 1 : -1;
        ANIMATION_MANAGER *animations = getAnimationManagerFromRawTable(lua, 1, 1, nullptr);
        ANIMATION *        anim       = nullptr;
        if(indexAnim > -1)
            anim       = animations->getAnimation(indexAnim);
        else
            anim       = animations->getAnimation();
        if (anim)
        {
            lua_pushstring(lua, anim->nameAnimation);
            lua_pushinteger(lua, animations->indexCurrentAnimation + 1);
        }
        else
        {
            lua_pushstring(lua, "erro");
            lua_pushnil(lua);
        }
        return 2;
    }

    int onSetAnimationsManagerLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top >= 2)
        {
            ANIMATION_MANAGER *animations = getAnimationManagerFromRawTable(lua, 1, 1, nullptr);
            if (lua_type(lua, 2) == LUA_TNUMBER)
            {
                const unsigned int index = lua_tointeger(lua, 2) - 1;
                animations->setAnimationByIndex(index); //-V522
            }
            else
            {
                const char *name = luaL_checkstring(lua, 2);
                animations->setAnimation(name);
            }
        }
        else
        {
            return lua_error_debug(lua, "\nexpected setAnim([number] or [string])\n");
        }
        return 0;
    }

    int onGetIndexFrameAnimationsManagerLua(lua_State *lua)
    {
        ANIMATION_MANAGER *animations = getAnimationManagerFromRawTable(lua, 1, 1, nullptr);
        ANIMATION *   anim       = animations->getAnimation(); //-V522
        if (anim)
            lua_pushinteger(lua, anim->indexCurrentFrame + 1);
        else
            lua_pushinteger(lua, 0);
        return 1;
    }

    int onRestartAnimationsManagerLua(lua_State *lua)
    {
        ANIMATION_MANAGER *animations = getAnimationManagerFromRawTable(lua, 1, 1, nullptr);
        animations->restartAnimation(); //-V522
        return 0;
    }

    int onIsEndedAnimationsManagerLua(lua_State *lua)
    {
        ANIMATION_MANAGER *animations = getAnimationManagerFromRawTable(lua, 1, 1, nullptr);
        ANIMATION *        anim       = animations->getAnimation(); //-V522
        if (anim)
            lua_pushboolean(lua, anim->isEndedThisAnimation);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    void onEndAnimationCallBackAnimationsLua(const char *fileNameAnimation, RENDERIZABLE *renderer)
    {
        if (renderer)
        {
            auto *userData  = static_cast<USER_DATA_RENDER_LUA *>(renderer->userData);
            DEVICE *         device    = DEVICE::getInstance();
            auto * sceneData = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
            lua_State *           lua       = sceneData->lua;
            lua_rawgeti(lua, LUA_REGISTRYINDEX, userData->ref_CallBackAnimation);
            if (lua_isfunction(lua, -1))
            {
                lua_rawgeti(lua, LUA_REGISTRYINDEX, userData->ref_MeAsTable);
                lua_pushstring(lua, fileNameAnimation);
                if (lua_pcall(lua, 2, 0, 0))
                    lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
            }
            else
            {
                lua_pop(lua, 1);
            }
        }
    }

    void onEndEffectCallBackAnimationsLua(const char *shaderName, RENDERIZABLE *renderizable)
    {
        if (renderizable)
        {
            auto *userData = static_cast<USER_DATA_RENDER_LUA *>(renderizable->userData);
            DEVICE *         device = DEVICE::getInstance();
            auto * sceneData = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
            lua_State *           lua = sceneData->lua;
            lua_rawgeti(lua, LUA_REGISTRYINDEX, userData->ref_CallBackEffectShader);
            if (lua_isfunction(lua, -1))
            {
                lua_rawgeti(lua, LUA_REGISTRYINDEX, userData->ref_MeAsTable);
                lua_pushstring(lua, shaderName);
                if (lua_pcall(lua, 2, 0, 0))
                    lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
            }
            else
            {
                lua_pop(lua, 1);
            }
        }
    }

    int setCallBackEndAnimationsManagerLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top == 2)
        {
            RENDERIZABLE *        renderizable = nullptr;
            ANIMATION_MANAGER *   animations   = getAnimationManagerFromRawTable(lua, 1, 1, &renderizable);
            if(renderizable && renderizable->userData)
            {
                auto *userData     = static_cast<USER_DATA_RENDER_LUA *>(renderizable->userData);
                userData->refTableLua(lua, 1, &userData->ref_MeAsTable);
                userData->refFunctionLua(lua, 2, &userData->ref_CallBackAnimation);
                if(userData->ref_CallBackAnimation == LUA_NOREF)
                    animations->onEndAnimation = nullptr;
                else
                    animations->onEndAnimation = onEndAnimationCallBackAnimationsLua; //-V522
            }
        }
        return 0;
    }

    int setCallBackEndEffectLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top == 2)
        {
            RENDERIZABLE *        renderizable = nullptr;
            ANIMATION_MANAGER *   animations = getAnimationManagerFromRawTable(lua, 1, 1, &renderizable);
            if(renderizable && renderizable->userData)
            {
                auto *userData = static_cast<USER_DATA_RENDER_LUA *>(renderizable->userData);
                userData->refTableLua(lua, 1, &userData->ref_MeAsTable);
                userData->refFunctionLua(lua, 2, &userData->ref_CallBackEffectShader);
                if(userData->ref_CallBackEffectShader == LUA_NOREF)
                    animations->onEndFx = nullptr;
                else
                    animations->onEndFx = onEndEffectCallBackAnimationsLua; //-V522
            }
        }
        return 0;
    }

    
    int onSetAnimationTypeLua(lua_State *lua)
    {
        const int          top             = lua_gettop(lua);
        RENDERIZABLE *     renderizable    = nullptr;
        ANIMATION_MANAGER *animations      = getAnimationManagerFromRawTable(lua, 1, 1, &renderizable);
        auto * animation                   = animations->getAnimation();
        unsigned int old_type              = mbm::TYPE_ANIMATION_PAUSED;
        if(animation)
        {
            old_type = animation->type;
            const unsigned int type = top >= 2 ? luaL_checkinteger(lua, 2) : 0;
            if(type >= mbm::TYPE_ANIMATION_PAUSED && type <= mbm::TYPE_ANIMATION_RECURSIVE_LOOP)
            {
                animation->type = (mbm::TYPE_ANIMATION)type;
            }
            else
            {
                lua_error_debug(lua,"expected type between %d-%d\ngot %d",mbm::TYPE_ANIMATION_PAUSED ,mbm::TYPE_ANIMATION_RECURSIVE_LOOP,type );
            }
        }
        lua_pushinteger(lua,old_type);
        return 1;
    }

    int onSetTextureAnimationLua(lua_State *lua)
    {
        const int          top             = lua_gettop(lua);
        RENDERIZABLE *     renderizable    = nullptr;
        ANIMATION_MANAGER *animations      = getAnimationManagerFromRawTable(lua, 1, 1, &renderizable);
        const int type                     = lua_type(lua,2);
        if(type == LUA_TSTRING)
        {
            const char *       fileNameTexture = lua_tostring(lua, 2);
            const bool         alpha           = top >= 3 ? (lua_toboolean(lua, 3) ? true : false) : true;
            const unsigned int stage           = top >= 4 ? luaL_checkinteger(lua, 4) : 1;

            if (stage > 2 || stage == 0)
            {
                lua_pushboolean(lua, 0);
                return lua_error_debug(lua, "stage must be 1 or 2");
            }
            else if(renderizable)
            {
                const bool ret = animations->setTexture(renderizable->getMesh(), fileNameTexture, stage-1, alpha); //-V522
                lua_pushboolean(lua, ret ? 1 : 0);
            }
        }
        else if(type == LUA_TNUMBER)
        {
            const float r = luaL_checknumber(lua,2);
            const float g = luaL_checknumber(lua,3);
            const float b = luaL_checknumber(lua,4);
            const float a = top > 4 ? luaL_checknumber(lua,5) : 1.0f;

            char color_hex[32] = {};
            const mbm::COLOR c(r,g,b,a);
            mbm::COLOR::getStringHexColorFromColor(c,color_hex,sizeof(color_hex));
            const bool ret = animations->setTexture(renderizable->getMesh(), color_hex, 0, true);
            lua_pushboolean(lua, ret ? 1 : 0);
        }
        else
        {
            return lua_error_debug(lua, "expected\n"
            "[texture_file_name], [alpha], [stage (1 or 2)]\n"
            "[red color (0-1)], [green color (0-1)], [blue color (0-1)], [alpha color (0-1)]\n"
            );
        }
        return 1;
    }

    const char *getNextNameAnim()
    {
        static char randomName[255] = "";
        static int  n               = 1;
        sprintf(randomName, "anim_%d", n++);
        return randomName;
    }
    //- addAnim(    string* name,number* type,number* StartFrame,
    //          number* FinalFrame,number intervalBetweenFrames)
    //- Retorno: index[number], name[string]
    //
    int onAddAnimationsManagerLua(lua_State *lua)
    {
        const int          top          = lua_gettop(lua);
        RENDERIZABLE *     renderizable = nullptr;
        ANIMATION_MANAGER *animations   = getAnimationManagerFromRawTable(lua, 1, 1, &renderizable);
        const char *       name         = top >= 2 ? luaL_checkstring(lua, 2) : getNextNameAnim();
        const unsigned int type         = top >= 3 ? luaL_checkinteger(lua, 3) : 0;
        const unsigned int startFrame   = top >= 4 ? luaL_checkinteger(lua, 4) - 1 : 0;
        const unsigned int finalFrame   = top >= 5 ? luaL_checkinteger(lua, 5) - 1 : 0;
        const float        interval     = top >= 6 ? luaL_checknumber(lua, 6) : 1.0f;
        const unsigned int index        = animations->addAnimation(); //-V522
        ANIMATION *   anim         = animations->getAnimation(index);
        if (anim)
        {
            if (name)
                strncpy(anim->nameAnimation, name, sizeof(anim->nameAnimation)-1);
            if (type <= 6)
                anim->type = (TYPE_ANIMATION)type;
            //------------------------------------------------------------------------------------------------------------
            const MESH_MBM *   mesh        = renderizable->getMesh();
            const unsigned int tTotalFrame = mesh ? mesh->getTotalFrame() : 0;
            if (startFrame < tTotalFrame)
                anim->indexInitialFrame = startFrame;
            if (finalFrame < tTotalFrame)
                anim->indexFinalFrame = finalFrame;
            if (interval > 0.0f)
                anim->intervalChangeFrame = interval;
            lua_pushstring(lua, anim->nameAnimation);
            lua_pushinteger(lua, index+1);
        }
        else
        {
            lua_pushnil(lua);
            lua_pushnil(lua);
        }
        return 2;
    }

    int onGetTotalFrameAnimationsManagerLua(lua_State *lua)
    {
        ANIMATION_MANAGER *animations = getAnimationManagerFromRawTable(lua, 1, 1, nullptr);
        if (animations)
        {
            int total = 0;
            const unsigned int s=animations->getTotalAnimation();
            for (unsigned int i = 0; i < s; ++i)
            {
                ANIMATION* anim = animations->getAnimation(i);
                total = total > anim->indexFinalFrame ? total : anim->indexFinalFrame;
                total = total > anim->indexInitialFrame ? total : anim->indexInitialFrame;
            }
            lua_pushinteger(lua, total+1);
        }
        else
        {
            lua_pushinteger(lua, 0);
        }
        return 1;
    }

    int onGetTotalAnimationsManagerLua(lua_State *lua)
    {
        RENDERIZABLE *          renderizable = nullptr;
        ANIMATION_MANAGER *anim         = getAnimationManagerFromRawTable(lua, 1, 1, &renderizable);
        if (anim)
            lua_pushinteger(lua, anim->getTotalAnimation());
        else
            lua_pushinteger(lua, 0);
        return 1;
    }

    int onSetRenderRenderizableState(lua_State *lua)
    {
        const char *errLog = "\nExpected:\n"
                             "[string value] or \n"
                             "[number value]\n"
                             "where:\n"
                             "value:\n"
                             "0 -  'disable'.\n"
                             "1 -  'zero'.\n"
                             "2 -  'one'.\n"
                             "3 -  'src_color'.\n"
                             "4 -  'inv_src_color'.\n"
                             "5 -  'src_alpha'.\n"
                             "6 -  'inv_src_alpha'.\n"
                             "7 -  'dest_alpha'.\n"
                             "8 -  'inv_dest_alpha'.\n"
                             "9 -  'dest_color'.\n"
                             "10 - 'inv_dest_color'.\n";
        const int top = lua_gettop(lua);
        if (top == 2)
        {
            RENDERIZABLE *          renderizable= nullptr;
            ANIMATION_MANAGER *animManager = getAnimationManagerFromRawTable(lua, 1, 1, &renderizable);
            ANIMATION* anim                = animManager->getAnimation(); //-V522
            const int          t_arg_1      = lua_type(lua, 2);
            if (t_arg_1 == LUA_TSTRING)
            {
                const char *value = luaL_checkstring(lua, 2);
                if (strcmp("disable", value) == 0)
                    anim->blendState = BLEND_DISABLE;
                else if (strcmp("zero", value) == 0)
                    anim->blendState = BLEND_ZERO;
                else if (strcmp("one", value) == 0)
                    anim->blendState = BLEND_ONE;
                else if (strcmp("src_color", value) == 0)
                    anim->blendState = BLEND_SRCCOLOR;
                else if (strcmp("inv_src_color", value) == 0)
                    anim->blendState = BLEND_INVSRCCOLOR;
                else if (strcmp("src_alpha", value) == 0)
                    anim->blendState = BLEND_SRCALPHA;
                else if (strcmp("inv_src_alpha", value) == 0)
                    anim->blendState = BLEND_INVSRCALPHA;
                else if (strcmp("dest_alpha", value) == 0)
                    anim->blendState = BLEND_DESTALPHA;
                else if (strcmp("inv_dest_alpha", value) == 0)
                    anim->blendState = BLEND_INVDESTALPHA;
                else if (strcmp("dest_color", value) == 0)
                    anim->blendState = BLEND_DESTCOLOR;
                else if (strcmp("inv_dest_color", value) == 0)
                    anim->blendState = BLEND_INVDESTCOLOR;
                else
                {
                    return lua_error_debug(lua, errLog);
                }
            }
            else
            {
                const unsigned int value = luaL_checkinteger(lua, 2);
                if (value > 10)
                {
                    return lua_error_debug(lua, errLog);
                }
                else
                {
                    anim->blendState = (BLEND_OPENGLES)value;
                }
            }
        }
        else
        {
            return lua_error_debug(lua, errLog);
        }
        return 0;
    }

    int onGetRenderRenderizableState(lua_State *lua)
    {
        RENDERIZABLE *          renderizable= nullptr;
        ANIMATION_MANAGER *animManager = getAnimationManagerFromRawTable(lua, 1, 1, &renderizable);
        ANIMATION* anim                = animManager->getAnimation(); //-V522
        const char *       descr       = renderizable->blend.getDesc(anim->blendState);
        lua_pushstring(lua, descr);
        lua_pushnumber(lua, static_cast<lua_Number>(anim->blendState));
        return 2;
    }

    int onForceEndAnimFxRenderizable(lua_State *lua)
    {
        ANIMATION_MANAGER *animManager = getAnimationManagerFromRawTable(lua, 1, 1, nullptr);
        ANIMATION* anim = animManager->getAnimation(); //-V522
        if(anim)
        {
            bool bForce_endAnim = lua_toboolean(lua, 2);
            bool bForce_endFx = lua_toboolean(lua, 3);
            if(bForce_endAnim)
            {
                anim->isEndedThisAnimation = true;
            }
            if(bForce_endFx)
            {
                anim->fx.fxPS->forceEndFx();
                anim->fx.fxVS->forceEndFx();
                anim->fx.shader.update();
            }
        }
        return 0;
    }
};
