/*-----------------------------------------------------------------------------------------------------------------------|
| MIT License (MIT)                                                                                                      |
| Copyright (C) 2004-2017 by Michel Braz de Morais  <michel.braz.morais@gmail.com>                                       |
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

#include <lua-wrap/shader-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <core_mbm/device.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/shader-var-cfg.h>
#include <core_mbm/shader-fx.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/animation.h>
#include <core_mbm/texture-manager.h>
#include <platform/mismatch-platform.h>

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

namespace mbm
{
    extern RENDERIZABLE *getRenderizableFromRawTable(lua_State *lua, const int rawi, const int indexTable);
	extern ANIMATION_MANAGER *getAnimationManagerFromRawTable(lua_State *lua, const int rawi, const int indexTable);
	extern ANIMATION_MANAGER *getSafeAnimationManagerFromRenderizable(lua_State *lua, RENDERIZABLE * renderizable);
	extern ANIMATION *getSafeAnimFromRenderizable(lua_State *lua, RENDERIZABLE * renderizable);
	extern FX *getFxFromRawTable(lua_State *lua, const int rawi, const int indexTable);
	extern FX *getSafeFxFromRenderizable(lua_State *lua, RENDERIZABLE * renderizable);
    extern int verifyDynamicCast(lua_State *lua, void *ptr, int line, const char *__file);
	extern int lua_error_debug(lua_State *lua, const char *format, ...);

	int onLoadNewShaderLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
		RENDERIZABLE *renderizable	= getRenderizableFromRawTable(lua, 1, 1);
        if (top >= 2)
        {
            const int            typePshader          = lua_type(lua, 2);
            const int            typeVshader          = top > 2 ? lua_type(lua, 3) : LUA_TNIL;
            const char *         pixelShaderFileName  = typePshader == LUA_TNIL ? nullptr : luaL_checkstring(lua, 2);
            const char *         vertexShaderFileName = typeVshader == LUA_TNIL ? nullptr : luaL_checkstring(lua, 3);
            SHADER_CFG *         pShaderCfg           = nullptr;
            SHADER_CFG *         vShaderCfg           = nullptr;
            const unsigned int   param3               = top > 3 ? luaL_checkinteger(lua, 4) : 1;
            const auto          param4                = (float)(top > 4 ? luaL_checknumber(lua, 5) : 0.0f);
            const unsigned int   param5               = top > 5 ? luaL_checkinteger(lua, 6) : 1;
            const auto          param6                = (float)(top > 6 ? luaL_checknumber(lua, 7) : 0.0f);
            const auto typePs                         = (TYPE_ANIMATION)(param3 <= 6 ? param3 : 1);
            const float          timePs               = param4 > 0.0f ? param4 : 0.0f;
            const auto typeVs                         = (TYPE_ANIMATION)(param5 <= 6 ? param5 : 1);
            const float          timeVs               = param6 > 0.0f ? param6 : 0.0f;
            DEVICE *             device               = DEVICE::getInstance();
			FX * fx                                   = getSafeFxFromRenderizable(lua, renderizable);
            if (pixelShaderFileName)
            {
                pShaderCfg = device->cfg.getShader(pixelShaderFileName);
                if (pShaderCfg == nullptr)
                {
                    return lua_error_debug(lua, "pixel shader not found: %s", pixelShaderFileName);
                }
            }
            if (vertexShaderFileName)
            {
                vShaderCfg = device->cfg.getShader(vertexShaderFileName);
                if (vShaderCfg == nullptr)
                {
                    return lua_error_debug(lua, "vertex shader not found: %s", vertexShaderFileName);
                }
            }
			const bool ret = fx->loadNewShader(pShaderCfg, vShaderCfg, typePs, timePs, typeVs, timeVs);
            if (ret)
                lua_pushboolean(lua, 1);
            else
                lua_pushboolean(lua, 0);
            return 1;
        }
        else
        {
            return lua_error_debug(lua, "expected:[nameVarShader], [value],...");
        }
    }

    int onSetPixelShaderLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        FX* fx =  getFxFromRawTable(lua,1,1);
        if (top >= 3)
        {
            const char *       varName    = luaL_checkstring(lua, 2);
            float              data[4] = {
                                           lua_type(lua,3) == LUA_TNUMBER ? (float)lua_tonumber(lua, 3) : 0.0f, 
                                top > 3 && lua_type(lua,4) == LUA_TNUMBER ? (float)lua_tonumber(lua, 4) : 0.0f,
                                top > 4 && lua_type(lua,5) == LUA_TNUMBER ? (float)lua_tonumber(lua, 5) : 0.0f,
                                top > 5 && lua_type(lua,6) == LUA_TNUMBER ? (float)lua_tonumber(lua, 6) : 0.0f};
            if (fx->setVarPShader(varName, data))
            {
                lua_pushboolean(lua, 1);
                return 1;
            }
            else
            {
				const char * shaderName = fx->fxPS->ptrCurrentShader ? fx->fxPS->ptrCurrentShader->fileName.c_str() : "NULL"; //-V522
                return lua_error_debug(lua, "variable [%s] not found for current shader [%s]", varName,shaderName);
            }
        }
        return lua_error_debug(lua, "expected:[nameVarShader], [value],...");
    }

    int onSetAllPixelShaderLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        FX* fx =  getFxFromRawTable(lua,1,1);
		if (top >= 3)
        {
            const char *       varName    = luaL_checkstring(lua, 2);
            float              data[4] = {
                                           lua_type(lua,3) == LUA_TNUMBER ? (float)lua_tonumber(lua, 3) : 0.0f, 
                                top > 3 && lua_type(lua,4) == LUA_TNUMBER ? (float)lua_tonumber(lua, 4) : 0.0f,
                                top > 4 && lua_type(lua,5) == LUA_TNUMBER ? (float)lua_tonumber(lua, 5) : 0.0f,
                                top > 5 && lua_type(lua,6) == LUA_TNUMBER ? (float)lua_tonumber(lua, 6) : 0.0f};
			if (fx->setVarPShader(varName, data) && 
                fx->setMinVarPShader(varName, data) &&
                fx->setMaxVarPShader(varName, data))
            {
                lua_pushboolean(lua, 1);
                return 1;
            }
            else
            {
				const char * shaderName = fx->fxPS->ptrCurrentShader ? fx->fxPS->ptrCurrentShader->fileName.c_str() : "NULL"; //-V522
                return lua_error_debug(lua, "variable [%s] not found for current shader [%s]", varName,shaderName);
            }
        }
        return lua_error_debug(lua, "expected:[nameVarShader], [value],...");
    }

    int onSetPixelShaderMaxLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        FX* fx =  getFxFromRawTable(lua,1,1);
        if (top >= 3)
        {
            const char *       varName    = luaL_checkstring(lua, 2);
            float              data[4] = {
                                           lua_type(lua,3) == LUA_TNUMBER ? (float)lua_tonumber(lua, 3) : 0.0f, 
                                top > 3 && lua_type(lua,4) == LUA_TNUMBER ? (float)lua_tonumber(lua, 4) : 0.0f,
                                top > 4 && lua_type(lua,5) == LUA_TNUMBER ? (float)lua_tonumber(lua, 5) : 0.0f,
                                top > 5 && lua_type(lua,6) == LUA_TNUMBER ? (float)lua_tonumber(lua, 6) : 0.0f};
			if (fx->setMaxVarPShader(varName, data))
            {
                lua_pushboolean(lua, 1);
                return 1;
            }
            else
            {
				const char * shaderName = fx->fxPS->ptrCurrentShader ? fx->fxPS->ptrCurrentShader->fileName.c_str() : "NULL"; //-V522
                return lua_error_debug(lua, "variable [%s] not found for current shader [%s]", varName,shaderName);
            }
        }
        return lua_error_debug(lua, "expected:[nameVarShader], [value],...");
    }

    int onSetPixelShaderMinLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        FX* fx =  getFxFromRawTable(lua,1,1);
        if (top >= 3)
        {
            const char *       varName    = luaL_checkstring(lua, 2);
            float              data[4] = {
                                           lua_type(lua,3) == LUA_TNUMBER ? (float)lua_tonumber(lua, 3) : 0.0f, 
                                top > 3 && lua_type(lua,4) == LUA_TNUMBER ? (float)lua_tonumber(lua, 4) : 0.0f,
                                top > 4 && lua_type(lua,5) == LUA_TNUMBER ? (float)lua_tonumber(lua, 5) : 0.0f,
                                top > 5 && lua_type(lua,6) == LUA_TNUMBER ? (float)lua_tonumber(lua, 6) : 0.0f};
            if (fx->setMinVarPShader(varName, data))
            {
                lua_pushboolean(lua, 1);
                return 1;
            }
            else
            {
				const char * shaderName = fx->fxPS->ptrCurrentShader ? fx->fxPS->ptrCurrentShader->fileName.c_str() : "NULL"; //-V522
                return lua_error_debug(lua, "variable [%s] not found for current shader [%s]", varName,shaderName);
            }
        }
        return lua_error_debug(lua, "expected:[nameVarShader], [value],...");
    }


    int onGetPixelShaderMinLua(lua_State *lua)
    {
        FX* fx =  getFxFromRawTable(lua,1,1);
        const char* varName = luaL_checkstring(lua,2);
        float data[4] = {0,0,0,0};
        const int t = fx->getMinVarPShader(varName,data);
        if(t == 0)
        {
			const char * shaderName = fx->fxPS->ptrCurrentShader ? fx->fxPS->ptrCurrentShader->fileName.c_str() : "NULL"; //-V522
            return lua_error_debug(lua, "variable [%s] not found for current shader [%s]", varName,shaderName);
        }
        for (int i = 0; i < 4 && i < t; ++i)
        {
            lua_pushnumber(lua,data[i]);
        }
        return t;
    }

    int onGetVertexShaderMinLua(lua_State *lua)
    {
        FX* fx =  getFxFromRawTable(lua,1,1);
        const char* varName = luaL_checkstring(lua,2);
        float data[4] = {0,0,0,0};
        const int t = fx->getMinVarVShader(varName,data);
        if(t == 0)
        {
			const char * shaderName = fx->fxVS->ptrCurrentShader ? fx->fxVS->ptrCurrentShader->fileName.c_str() : "NULL"; //-V522
            return lua_error_debug(lua, "variable [%s] not found for current shader [%s]", varName,shaderName);
        }
        for (int i = 0; i < 4 && i < t; ++i)
        {
            lua_pushnumber(lua,data[i]);
        }
        return t;
    }

    int onGetPixelShaderMaxLua(lua_State *lua)
    {
        FX* fx =  getFxFromRawTable(lua,1,1);
        const char* varName = luaL_checkstring(lua,2);
        float data[4] = {0,0,0,0};
        const int t = fx->getMaxVarPShader(varName,data);
        if(t == 0)
        {
			const char * shaderName = fx->fxPS->ptrCurrentShader ? fx->fxPS->ptrCurrentShader->fileName.c_str() : "NULL"; //-V522
            return lua_error_debug(lua, "variable [%s] not found for current shader [%s]", varName,shaderName);
        }
        for (int i = 0; i < 4 && i < t; ++i)
        {
            lua_pushnumber(lua,data[i]);
        }
        return t;
    }

    int onGetVertexShaderMaxLua(lua_State *lua)
    {
        FX* fx =  getFxFromRawTable(lua,1,1);
        const char* varName = luaL_checkstring(lua,2);
        float data[4] = {0,0,0,0};
        const int t = fx->getMaxVarVShader(varName,data);
        if(t == 0)
        {
			const char * shaderName = fx->fxVS->ptrCurrentShader ? fx->fxVS->ptrCurrentShader->fileName.c_str() : "NULL"; //-V522
            return lua_error_debug(lua, "variable [%s] not found for current shader [%s]", varName,shaderName);
        }
        for (int i = 0; i < 4 && i < t; ++i)
        {
            lua_pushnumber(lua,data[i]);
        }
        return t;
    }

    int onSetVertexShaderLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        FX* fx =  getFxFromRawTable(lua,1,1);
        if (top >= 3)
        {
            const char *       varName    = luaL_checkstring(lua, 2);
            float              data[4] = {
                                           lua_type(lua,3) == LUA_TNUMBER ? (float)lua_tonumber(lua, 3) : 0.0f, 
                                top > 3 && lua_type(lua,4) == LUA_TNUMBER ? (float)lua_tonumber(lua, 4) : 0.0f,
                                top > 4 && lua_type(lua,5) == LUA_TNUMBER ? (float)lua_tonumber(lua, 5) : 0.0f,
                                top > 5 && lua_type(lua,6) == LUA_TNUMBER ? (float)lua_tonumber(lua, 6) : 0.0f};
            if (fx->setVarVShader(varName, data))
            {
                lua_pushboolean(lua, 1);
                return 1;
            }
            else
            {
				const char * shaderName = fx->fxVS->ptrCurrentShader ? fx->fxVS->ptrCurrentShader->fileName.c_str() : "NULL"; //-V522
                return lua_error_debug(lua, "variable [%s] not found for current shader [%s]", varName,shaderName);
            }
        }
        return lua_error_debug(lua, "expected:[nameVarShader], [value],...");
    }

    int onSetAllVertexShaderLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        FX* fx =  getFxFromRawTable(lua,1,1);
        if (top >= 3)
        {
            const char *       varName    = luaL_checkstring(lua, 2);
            float              data[4] = {
                                           lua_type(lua,3) == LUA_TNUMBER ? (float)lua_tonumber(lua, 3) : 0.0f, 
                                top > 3 && lua_type(lua,4) == LUA_TNUMBER ? (float)lua_tonumber(lua, 4) : 0.0f,
                                top > 4 && lua_type(lua,5) == LUA_TNUMBER ? (float)lua_tonumber(lua, 5) : 0.0f,
                                top > 5 && lua_type(lua,6) == LUA_TNUMBER ? (float)lua_tonumber(lua, 6) : 0.0f};
            if (fx->setVarVShader(varName, data) && 
                fx->setMinVarVShader(varName, data) &&
                fx->setMaxVarVShader(varName, data))
            {
                lua_pushboolean(lua, 1);
                return 1;
            }
            else
            {
				const char * shaderName = fx->fxVS->ptrCurrentShader ? fx->fxVS->ptrCurrentShader->fileName.c_str() : "NULL"; //-V522
                return lua_error_debug(lua, "variable [%s] not found for current shader [%s]", varName,shaderName);
            }
        }
        return lua_error_debug(lua, "expected:[nameVarShader], [value],...");
    }

    int onSetVertexShaderMaxLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        FX* fx =  getFxFromRawTable(lua,1,1);
        if (top >= 3)
        {
            const char *       varName    = luaL_checkstring(lua, 2);
            float              data[4] = {
                                           lua_type(lua,3) == LUA_TNUMBER ? (float)lua_tonumber(lua, 3) : 0.0f, 
                                top > 3 && lua_type(lua,4) == LUA_TNUMBER ? (float)lua_tonumber(lua, 4) : 0.0f,
                                top > 4 && lua_type(lua,5) == LUA_TNUMBER ? (float)lua_tonumber(lua, 5) : 0.0f,
                                top > 5 && lua_type(lua,6) == LUA_TNUMBER ? (float)lua_tonumber(lua, 6) : 0.0f};
            if (fx->setMaxVarVShader( varName, data))
            {
                lua_pushboolean(lua, 1);
                return 1;
            }
            else
            {
				const char * shaderName = fx->fxVS->ptrCurrentShader ? fx->fxVS->ptrCurrentShader->fileName.c_str() : "NULL"; //-V522
                return lua_error_debug(lua, "variable [%s] not found for current shader [%s]", varName,shaderName);
            }
        }
        return lua_error_debug(lua, "expected:[nameVarShader], [value],...");
    }

    int onSetVertexShaderMinLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        FX* fx =  getFxFromRawTable(lua,1,1);
        if (top >= 3)
        {
            const char *       varName    = luaL_checkstring(lua, 2);
            float              data[4] = {
                                           lua_type(lua,3) == LUA_TNUMBER ? (float)lua_tonumber(lua, 3) : 0.0f, 
                                top > 3 && lua_type(lua,4) == LUA_TNUMBER ? (float)lua_tonumber(lua, 4) : 0.0f,
                                top > 4 && lua_type(lua,5) == LUA_TNUMBER ? (float)lua_tonumber(lua, 5) : 0.0f,
                                top > 5 && lua_type(lua,6) == LUA_TNUMBER ? (float)lua_tonumber(lua, 6) : 0.0f};
            if (fx->setMinVarVShader(varName, data))
            {
                lua_pushboolean(lua, 1);
                return 1;
            }
            else
            {
				const char * shaderName = fx->fxVS->ptrCurrentShader ? fx->fxVS->ptrCurrentShader->fileName.c_str() : "NULL"; //-V522
                return lua_error_debug(lua, "variable [%s] not found for current shader [%s]", varName,shaderName);
            }
        }
        return lua_error_debug(lua, "expected:[nameVarShader], [value],...");
    }

    int onGetPixelShaderLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        FX* fx =  getFxFromRawTable(lua,1,1);
        if (top >= 2)
        {
            const char *       varName    = luaL_checkstring(lua, 2);
            float              data[4]    = {0.0f, 0.0f, 0.0f, 0.0f};
			const int          ret        = fx->getVarPShader(varName, data);
            if (ret > 0 && ret <= 4)
            {
                for (int i = 0; i < ret; ++i)
                {
                    lua_pushnumber(lua, data[i]);
                }
                return ret;
            }
            else
            {
				const char * shaderName = fx->fxPS->ptrCurrentShader ? fx->fxPS->ptrCurrentShader->fileName.c_str() : "NULL"; //-V522
                return lua_error_debug(lua, "variable [%s] not found for current shader [%s]", varName,shaderName);
            }
        }
        return lua_error_debug(lua, "expected:[nameVarShader], [value],...");
    }

    int onGetVertexShaderLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        FX* fx =  getFxFromRawTable(lua,1,1);
        if (top >= 2)
        {
            const char *       varName    = luaL_checkstring(lua, 2);
            float              data[4]    = {0.0f, 0.0f, 0.0f, 0.0f};
            const int          ret        = fx->getVarVShader(varName, data);
            if (ret > 0 && ret <= 4)
            {
                for (int i = 0; i < ret; ++i)
                {
                    lua_pushnumber(lua, data[i]);
                }
                return ret;
            }
            else
            {
				const char * shaderName = fx->fxVS->ptrCurrentShader ? fx->fxVS->ptrCurrentShader->fileName.c_str() : "NULL"; //-V522
                return lua_error_debug(lua, "variable [%s] not found for current shader [%s]", varName,shaderName);
            }
        }
        return lua_error_debug(lua, "expected:[nameVarShader], [value],...");
    }

    int onSetBlendOperationLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top >= 2)
        {
            FX* fx =  getFxFromRawTable(lua,1,1);
            const int type = lua_type(lua,2);
            if(type == LUA_TSTRING)
            {
                const char*        blendFunc  = lua_tostring(lua, 2);
                if(strcasecmp(blendFunc,"ADD") == 0)
                    fx->blendOperation = 1;
                else if(strcasecmp(blendFunc,"SUBTRACT") == 0)
                    fx->blendOperation = 2;
                else if(strcasecmp(blendFunc,"REVERSE_SUBTRACT") == 0)
                    fx->blendOperation = 3;
                else if(strcasecmp(blendFunc,"MIN") == 0)
                    fx->blendOperation = 4;
                else if(strcasecmp(blendFunc,"MAX") == 0)
                    fx->blendOperation = 5;
                else
                    return lua_error_debug(lua, "expected:([blendFunc (ADD,SUBTRACT,REVERSE_SUBTRACT,MIN,MAX)])");
            }
            else
            {
                const unsigned int value = lua_tointeger(lua,2);
                if(value > 5)
                    return lua_error_debug(lua, "expected:([blendFunc (ADD,SUBTRACT,REVERSE_SUBTRACT,MIN,MAX)])");
                fx->blendOperation = static_cast<int>(value);
            }
        }
        else
        {
            return lua_error_debug(lua, "expected:([blendFunc (ADD,SUBTRACT,REVERSE_SUBTRACT,MIN,MAX)])");
        }
        return 0;
    }

    int onGetBlendOperationLua(lua_State *lua)
    {
        FX* fx =  getFxFromRawTable(lua,1,1);
        switch(fx->blendOperation)
        {
            case 1: lua_pushstring(lua,"ADD");break;
            case 2: lua_pushstring(lua,"SUBTRACT");break;
            case 3: lua_pushstring(lua,"REVERSE_SUBTRACT");break;
            case 4: lua_pushstring(lua,"MIN");break;
            case 5: lua_pushstring(lua,"MAX");break;
            default:lua_pushstring(lua,"ADD");break;
        }
        return 1;
    }

    void pushVarShader(lua_State *lua, std::vector<VAR_SHADER *> *lsVars)
    {
        lua_newtable(lua); // table ret (1)
        if (lsVars)
        {
            for (unsigned int i = 0; i < lsVars->size(); ++i)
            {
                /*
                 table = { [1] = { name=color, type=rgba, value={[1]=1,[2]=1,[3]=1}, min={[1]=1,[2]=1,[3]=1},
                 max={[1]=1,[2]=1,[3]=1} }, ... }
                */
                unsigned int s = 0;
                lua_newtable(lua); // index [1] (2)
                VAR_SHADER *localVar = lsVars->at(i);
                lua_pushstring(lua, localVar->name.c_str()); //(3)
                lua_setfield(lua, -2, "name");               //(2)
                switch (localVar->typeVar)
                {
                    case VAR_FLOAT:
                    {
                        lua_pushstring(lua, "number");
                        lua_setfield(lua, -2, "type");
                        s = 1;
                    }
                    break;
                    case VAR_VECTOR:
                    {
                        lua_pushstring(lua, "vec3");
                        lua_setfield(lua, -2, "type");
                        s = 3;
                    }
                    break;
                    case VAR_VECTOR2:
                    {
                        lua_pushstring(lua, "vec2");
                        lua_setfield(lua, -2, "type");
                        s = 2;
                    }
                    break;
                    case VAR_COLOR_RGB:
                    {
                        lua_pushstring(lua, "rgb");
                        lua_setfield(lua, -2, "type");
                        s = 3;
                    }
                    break;
                    case VAR_COLOR_RGBA:
                    {
                        lua_pushstring(lua, "rgba");
                        lua_setfield(lua, -2, "type");
                        s = 4;
                    }
                    break;
                }
                lua_newtable(lua); // value
                for (unsigned int j = 0; j < s; ++j)
                {
                    lua_pushnumber(lua, localVar->current[j]);
                    lua_rawseti(lua, -2, j + 1);
                }
                lua_setfield(lua, -2, "value");

                lua_newtable(lua); // min
                for (unsigned int j = 0; j < s; ++j)
                {
                    lua_pushnumber(lua, localVar->min[j]);
                    lua_rawseti(lua, -2, j + 1);
                }
                lua_setfield(lua, -2, "min");

                lua_newtable(lua); // max
                for (unsigned int j = 0; j < s; ++j)
                {
                    lua_pushnumber(lua, localVar->max[j]);
                    lua_rawseti(lua, -2, j + 1);
                }
                lua_setfield(lua, -2, "max");

                lua_rawseti(lua, -2, i + 1); // index //(1)
            }
        }
    }

    int onGetVarsShaderLua(lua_State *lua)
    {
        FX* fx =  getFxFromRawTable(lua,1,1);
        lua_settop(lua, 0);
		std::vector<VAR_SHADER *> *lsVarsPs = fx->getVarsPS();
		std::vector<VAR_SHADER *> *lsVarsVs = fx->getVarsVS();
		pushVarShader(lua, lsVarsPs);
		pushVarShader(lua, lsVarsVs);
		return 2;
    }

    int onGetCodeShaderLua(lua_State *lua)
    {
        FX* fx =  getFxFromRawTable(lua,1,1);
        const char *       codePS   = fx->getCodePS();
        const char *       codeVS   = fx->getCodeVS();
        if (codePS)
            lua_pushstring(lua, codePS);
        else
            lua_pushnil(lua);
        if(codeVS)
            lua_pushstring(lua, codeVS);
        else
            lua_pushnil(lua);
        return 2;
    }

    int onGetNamesShaderLua(lua_State *lua)
    {
        FX* fx =  getFxFromRawTable(lua,1,1);
        const char *       namePS   = fx->fxPS->ptrCurrentShader ? fx->fxPS->ptrCurrentShader->fileName.c_str() : nullptr;
        const char *       nameVS   = fx->fxVS->ptrCurrentShader ? fx->fxVS->ptrCurrentShader->fileName.c_str() : nullptr;
        if(namePS)
            lua_pushstring(lua, namePS);
        else
            lua_pushnil(lua);
        if(nameVS)
            lua_pushstring(lua, nameVS);
        else
            lua_pushnil(lua);
        return 2;
    }

    int onGetTextureStage2AnimationLua(lua_State *lua)
    {
	RENDERIZABLE* renderizable  = getRenderizableFromRawTable(lua, 1,1);
        FX* fx						= getSafeFxFromRenderizable(lua,renderizable);
	if (fx->textureOverrideStage2)
		lua_pushstring(lua, fx->textureOverrideStage2->getFileNameTexture());
	else
		lua_pushnil(lua);
	return 1;
    }

    int onSetPixelShaderTimeLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top >= 2)
        {
			FX* fx      = getFxFromRawTable(lua,1,1);
            float  time = luaL_checknumber(lua, 2);
            if (fx->setTimePS(time))
                lua_pushboolean(lua, 1);
            else
                lua_pushboolean(lua, 0);
            return 1;
        }
        return lua_error_debug(lua, "expected:[time]");
    }

    int onSetVertexShaderTimeLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top >= 2)
        {
			FX* fx     = getFxFromRawTable(lua,1,1);
            float time = luaL_checknumber(lua, 2);
            if (fx->setTimeVS(time))
                lua_pushboolean(lua, 1);
            else
                lua_pushboolean(lua, 0);
            return 1;
        }
        return lua_error_debug(lua, "expected:[time]");
    }

    const int getNumberFromAnimationTypeShader(lua_State *lua,const int index)
    {
        const char* animName = luaL_checkstring(lua,index);
        if(animName)
        {
            if(strcasecmp(animName,"paused") == 0)
                return 0;
            if(strcasecmp(animName,"growing") == 0)
                return 1;
            if(strcasecmp(animName,"growing loop") == 0)
                return 2;
            if(strcasecmp(animName,"decreasing") == 0)
                return 3;
            if(strcasecmp(animName,"decreasing loop") == 0)
                return 4;
            if(strcasecmp(animName,"recursive") == 0)
                return 5;
            if(strcasecmp(animName,"recursive loop") == 0)
                return 6;
        }
        return 0xff;
    }

    int errorAnimType(lua_State *lua)
    {
        return lua_error_debug(lua, "name of type Animation Shader should be:\n"
                    "0: 'paused'\n"
                    "1: 'growing'\n"
                    "2: 'growing loop'\n"
                    "3: 'decreasing'\n"
                    "4: 'decreasing loop'\n"
                    "5: 'recursive'\n"
                    "6: 'recursive loop'\n");
    }

    int onSetTypeAnimationPsShaderLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top >= 2)
        {
			FX* fx =  getFxFromRawTable(lua,1,1);
            const int      typeAnimation  = lua_type(lua,2) == LUA_TNUMBER ? lua_tointeger(lua, 2) : getNumberFromAnimationTypeShader(lua,2);
            if(typeAnimation < 0 || typeAnimation > 6)
                return errorAnimType(lua);
            lua_pushboolean(lua, fx->setTypePS((TYPE_ANIMATION)typeAnimation) ? 1 : 0);
            return 1;
        }
        return lua_error_debug(lua, "expected:[time]");
    }

    int onSetTypeAnimationVsShaderLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top >= 2)
        {
			FX* fx =  getFxFromRawTable(lua,1,1);
            const int      typeAnimation  = lua_type(lua,2) == LUA_TNUMBER ? lua_tointeger(lua, 2) : getNumberFromAnimationTypeShader(lua,2);
            if(typeAnimation < 0 || typeAnimation > 6)
                return errorAnimType(lua);
            lua_pushboolean(lua, fx->setTypeVS((TYPE_ANIMATION)typeAnimation) ? 1 : 0);
            return 1;
        }
        return lua_error_debug(lua, "expected:[time]");
    }

    int onGetTypeAnimationPsShaderLua(lua_State *lua)
    {
        FX* fx =  getFxFromRawTable(lua,1,1);
        TYPE_ANIMATION type  = fx->getTypePS();
        switch(type)
        {
            case TYPE_ANIMATION_PAUSED          :lua_pushstring(lua,"PAUSED");  break;
            case TYPE_ANIMATION_GROWING         :lua_pushstring(lua,"GROWING");  break;
            case TYPE_ANIMATION_GROWING_LOOP    :lua_pushstring(lua,"GROWING_LOOP");  break;
            case TYPE_ANIMATION_DECREASING      :lua_pushstring(lua,"DECREASING");  break;
            case TYPE_ANIMATION_DECREASING_LOOP :lua_pushstring(lua,"DECREASING_LOOP");  break;
            case TYPE_ANIMATION_RECURSIVE       :lua_pushstring(lua,"RECURSIVE");  break;
            case TYPE_ANIMATION_RECURSIVE_LOOP  :lua_pushstring(lua,"RECURSIVE_LOOP");  break;
            default:lua_pushstring(lua,"UNKNOWN");  break;
        }
        lua_pushinteger(lua,(int)type);
        return 2;
    }

    int onGetTypeAnimationVsShaderLua(lua_State *lua)
    {
        FX* fx                      = getFxFromRawTable(lua,1,1);
        TYPE_ANIMATION type         = fx->getTypeVS();
        switch(type)
        {
            case TYPE_ANIMATION_PAUSED          :lua_pushstring(lua,"PAUSED");  break;
            case TYPE_ANIMATION_GROWING         :lua_pushstring(lua,"GROWING");  break;
            case TYPE_ANIMATION_GROWING_LOOP    :lua_pushstring(lua,"GROWING_LOOP");  break;
            case TYPE_ANIMATION_DECREASING      :lua_pushstring(lua,"DECREASING");  break;
            case TYPE_ANIMATION_DECREASING_LOOP :lua_pushstring(lua,"DECREASING_LOOP");  break;
            case TYPE_ANIMATION_RECURSIVE       :lua_pushstring(lua,"RECURSIVE");  break;
            case TYPE_ANIMATION_RECURSIVE_LOOP  :lua_pushstring(lua,"RECURSIVE_LOOP");  break;
            default:lua_pushstring(lua,"UNKNOWN");  break;
        }
        lua_pushinteger(lua,(int)type);
        return 2;
    }

    const float getDataPsVsTime(lua_State *lua,const bool pixelShader,const bool original)
    {
        if(original)
        {
            RENDERIZABLE * renderizable = getRenderizableFromRawTable(lua,1,1);
            ANIMATION_MANAGER* animMan  = getSafeAnimationManagerFromRenderizable(lua, renderizable);
            const MESH_MBM* mesh        = renderizable->getMesh();
            if(mesh)
            {
                const unsigned int index = animMan->getIndexAnimation();
                if(index < mesh->infoAnimation.lsHeaderAnim.size())
                {
                    util::INFO_ANIMATION::INFO_HEADER_ANIM* infoHead = mesh->infoAnimation.lsHeaderAnim[index];
                    if(infoHead && infoHead->effetcShader)
                    {
                        if(pixelShader)
                        {
                            const util::INFO_SHADER_DATA* data = infoHead->effetcShader->dataPS;
                            if(data)
                                return data->timeAnimation;
                        }
                        else
                        {
                            const util::INFO_SHADER_DATA* data = infoHead->effetcShader->dataVS;
                            if(data)
                                return data->timeAnimation;
                        }
                    }
                }
            }
        }
        else
        {
            FX* fx      = getFxFromRawTable(lua,1,1);
            if(pixelShader)
                return fx->getTimePS();
            else
                return fx->getTimeVS();
        }
        return 0.0f;
    }

    int onGetTimeAnimationVsShaderLua(lua_State *lua)
    {
        const int top       = lua_gettop(lua);
        const bool original = top > 1 ? lua_toboolean(lua,2) : false;
        const float myTime  = getDataPsVsTime(lua,false,original);
        lua_pushnumber(lua,myTime);
        return 1;
        
    }

    int onGetTimeAnimationPsShaderLua(lua_State *lua)
    {
        const int top       = lua_gettop(lua);
        const bool original = top > 1 ? lua_toboolean(lua,2) : false;
        const float myTime  = getDataPsVsTime(lua,true,original);
        lua_pushnumber(lua,myTime);
        return 1;
    }

    int onGetShaderTableRenderizableLuaNoGC(lua_State *lua)
    {
        RENDERIZABLE *renderizable	= getRenderizableFromRawTable(lua, 1, 1);
        
		lua_settop(lua, 0);
        luaL_Reg regShaderMethods[] = {{"load", onLoadNewShaderLua},
                                       {"setPS", onSetPixelShaderLua},
                                       {"setVS", onSetVertexShaderLua},
                                       {"setPStype", onSetTypeAnimationPsShaderLua},
                                       {"setVStype", onSetTypeAnimationVsShaderLua},
                                       {"getPStype", onGetTypeAnimationPsShaderLua},
                                       {"getVStype", onGetTypeAnimationVsShaderLua},
                                       {"getPStime", onGetTimeAnimationPsShaderLua},
                                       {"getVStime", onGetTimeAnimationVsShaderLua},
                                       {"setPSall", onSetAllPixelShaderLua},
                                       {"setVSall", onSetAllVertexShaderLua},
                                       {"getPS", onGetPixelShaderLua},
                                       {"getVS", onGetVertexShaderLua},
                                       {"getPSmin", onGetPixelShaderMinLua},
                                       {"getVSmin", onGetVertexShaderMinLua},
                                       {"getPSmax", onGetPixelShaderMaxLua},
                                       {"getVSmax", onGetVertexShaderMaxLua},
                                       {"setPSmin", onSetPixelShaderMinLua},
                                       {"setVSmin", onSetVertexShaderMinLua},
                                       {"setPSmax", onSetPixelShaderMaxLua},
                                       {"setVSmax", onSetVertexShaderMaxLua},
                                       {"setPStime", onSetPixelShaderTimeLua},
                                       {"setVStime", onSetVertexShaderTimeLua},
                                       {"setBlendOp", onSetBlendOperationLua},
                                       {"getBlendOp", onGetBlendOperationLua},
                                       {"getVars", onGetVarsShaderLua},
                                       {"getCode", onGetCodeShaderLua},
                                       {"getNames", onGetNamesShaderLua},
                                       {"getTextureStage2", onGetTextureStage2AnimationLua},
			                           {nullptr, nullptr}};
        luaL_newlib(lua, regShaderMethods);

		//luaL_getmetatable(lua, "_mbmANIM");
        //lua_setmetatable(lua, -2);

        auto **udata = static_cast<RENDERIZABLE **>(lua_newuserdata(lua, sizeof(RENDERIZABLE *)));
		(*udata) = renderizable;
		
		/* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_SHADER);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }
};
