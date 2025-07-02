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

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#include <lua-wrap/common-methods-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/current-scene-lua.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/device.h>
#include <core_mbm/physics.h>
#include <platform/mismatch-platform.h>
#include <cstring>

namespace mbm
{
    extern int onNewVec3LuaNoGC(lua_State *lua, VEC3 *vec3);
    extern int onGetAnimationsManagerLua(lua_State *lua);
    extern int onSetAnimationsManagerLua(lua_State *lua);
    extern int onGetIndexFrameAnimationsManagerLua(lua_State *lua);
    extern int onRestartAnimationsManagerLua(lua_State *lua);
    extern int onIsEndedAnimationsManagerLua(lua_State *lua);
    extern int setCallBackEndAnimationsManagerLua(lua_State *lua);
    extern int setCallBackEndEffectLua(lua_State *lua);
    extern int onSetTextureAnimationLua(lua_State *lua);
    extern int onAddAnimationsManagerLua(lua_State *lua);
    extern int onGetTotalFrameAnimationsManagerLua(lua_State *lua);
    extern int onGetTotalAnimationsManagerLua(lua_State *lua);
    extern int onSetRenderRenderizableState(lua_State *lua);
    extern int onGetRenderRenderizableState(lua_State *lua);
    extern int onGetShaderTableRenderizableLuaNoGC(lua_State *lua);
    extern int onForceEndAnimFxRenderizable(lua_State *lua);
    extern int onSetAnimationTypeLua(lua_State *lua);
	extern int lua_error_debug(lua_State *lua, const char *format, ...);

    void getTypeWordRenderizableLua(lua_State * lua, const int index, bool & is2dw, bool & is2ds, bool & is3d)
    {
        const char *type = luaL_checkstring(lua, index);
        const int len = strlen(type);
        switch (len)
        {
            case 0:
            {
                is2dw = true;
                is2ds = false;
                is3d = false;
            }
            break;
            case 1:
            {
                if(type[0] == '3')
                {
                    is2dw = false;
                    is2ds = false;
                    is3d = true;
                }
                else
                {
                    is2dw = true;
                    is2ds = false;
                    is3d = false;
                }
            }
            break;
            case 2:
            {
                if(strcasecmp(type,"3d") == 0)
                {
                    is2dw = false;
                    is2ds = false;
                    is3d = true;
                }
                else
                {
                    is2dw = true;
                    is2ds = false;
                    is3d = false;
                }
            }
            break;
            default:// 
            {
                if(strcasecmp(type,"2ds") == 0)
                {
                    is2dw = false;
                    is2ds = true;
                    is3d = false;
                }
                else
                {
                    is2dw = true;
                    is2ds = false;
                    is3d = false;
                }
                
            }
            break;
        }
    }

    int onIsOverBoundingBoxRenderizable(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top == 3)
        {
            RENDERIZABLE *ptr        = getRenderizableFromRawTable(lua, 1, 1);
            const float        x     = luaL_checknumber(lua, 2);
            const float        y     = luaL_checknumber(lua, 3);
            bool               ret   = false;
            DEVICE *      device = DEVICE::getInstance();
            if (ptr->is3D)
                ret = ptr->isOver3d(device, x, y);
            else if (ptr->is2dS)
                ret = ptr->isOver2ds(device, x, y);
            else
                ret = ptr->isOver2dw(device, x, y);
            lua_pushboolean(lua, ret ? 1 : 0);
            return 1;
        }
        return lua_error_debug(lua, "\nExpected: [renderizable],[x],[y](2ds)");
    }

    void doOffsetIfText(RENDERIZABLE *ptr,const float w,const float h)
    {
        if(ptr->typeClass == TYPE_CLASS::TYPE_CLASS_TEXT)
        {
            if(ptr->is2dS)
            {
                ptr->position.x += w * 0.5f;
                ptr->position.y += h * 0.5f;
            }
            else
            {
                ptr->position.x += w * 0.5f;
                ptr->position.y -= h * 0.5f;
            }
        }
    }

    void undoOffsetIfText(RENDERIZABLE *ptr,const float w,const float h)
    {
        if(ptr->typeClass == TYPE_CLASS::TYPE_CLASS_TEXT)
        {
            if(ptr->is2dS)
            {
                ptr->position.x -= w * 0.5f;
                ptr->position.y -= h * 0.5f;
            }
            else
            {
                ptr->position.x -= w * 0.5f;
                ptr->position.y += h * 0.5f;
            }
        }
    }

    int onCheckCollisionBoundingBoxRenderizable(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top >= 2 && lua_type(lua,2) == LUA_TTABLE)
        {
            RENDERIZABLE *ptrA      = getRenderizableFromRawTable(lua, 1, 1);
            RENDERIZABLE *ptrB      = getRenderizableFromRawTable(lua, 1, 2);
            const bool  useAABB     = top > 2 ? (lua_toboolean(lua,3) ? true : false)  : true;

            if(ptrA == ptrB)
            {
                return lua_error_debug(lua, "\nRenderizable must be different: [A == B] )");
            }

            DEVICE *      device = DEVICE::getInstance();
            bool collide    = false;
            bool goOn       = false;
            float w1 = 0.0f;
			float h1 = 0.0f;
			float w2 = 0.0f;
			float h2 = 0.0f;
            
            if(ptrA->is2dS)
            {
                if(ptrB->is2dS)//2ds
                {
                    if(useAABB)
                    {
                        ptrA->getAABB(&w1,&h1);
                        ptrB->getAABB(&w2,&h2);
                        goOn = true;
                    }
                    else
                    {
                        goOn =  ptrA->getWidthHeight(&w1,&h1) && 
                                ptrB->getWidthHeight(&w2,&h2);
                    }
                    if(goOn)
                    {
                        doOffsetIfText(ptrA,w1,h1);
                        doOffsetIfText(ptrB,w2,h2);
                        collide = device->checkBoundCollision(ptrA->position, w1,h1,ptrB->position,w2,h2);
                        undoOffsetIfText(ptrA,w1,h1);
                        undoOffsetIfText(ptrB,w2,h2);
                    }
                }
                else if(ptrB->is3D == false)//2dw
                {
                    if(useAABB)
                    {
                        ptrA->getAABB(&w1,&h1);
                        ptrB->getAABB(&w2,&h2);
                        goOn = true;
                    }
                    else
                    {
                       goOn =   ptrA->getWidthHeight(&w1,&h1) && 
                                ptrB->getWidthHeight(&w2,&h2); 
                    }
                    if(goOn)
                    {
                        VEC2 pos;
                        doOffsetIfText(ptrA,w1,h1);
                        doOffsetIfText(ptrB,w2,h2);
                        device->transformeWorld2dToScreen2d_scaled(ptrB->position.x,ptrB->position.y,pos);
                        const VEC3 p2(pos.x,pos.y,0.0f);
                        collide = device->checkBoundCollision(ptrA->position,w1,h1,p2,w2,h2);
                        undoOffsetIfText(ptrA,w1,h1);
                        undoOffsetIfText(ptrB,w2,h2);
                    }
                }
            
            }
            else if(ptrA->is3D == false)//2dw
            {
                if(ptrB->is3D == false && ptrB->is2dS == false) // 2dw
                {
                    if(useAABB)
                    {
                        ptrA->getAABB(&w1,&h1);
                        ptrB->getAABB(&w2,&h2);
                        goOn = true;
                    }
                    else
                    {
                       goOn =   ptrA->getWidthHeight(&w1,&h1) && 
                                ptrB->getWidthHeight(&w2,&h2); 
                    }
                    if(goOn)
                    {
                        doOffsetIfText(ptrA,w1,h1);
                        doOffsetIfText(ptrB,w2,h2);
                        collide = device->checkBoundCollision(ptrA->position,w1,h1,ptrB->position,w2,h2);
                        undoOffsetIfText(ptrA,w1,h1);
                        undoOffsetIfText(ptrB,w2,h2);
                    }
                }
                else if(ptrB->is2dS)
                {
                    if(useAABB)
                    {
                        ptrA->getAABB(&w1,&h1);
                        ptrB->getAABB(&w2,&h2);
                        goOn = true;
                    }
                    else
                    {
                       goOn =   ptrA->getWidthHeight(&w1,&h1) && 
                                ptrB->getWidthHeight(&w2,&h2); 
                    }
                    if(goOn)
                    {
                        VEC3 pos;
                        doOffsetIfText(ptrA,w1,h1);
                        doOffsetIfText(ptrB,w2,h2);
                        device->transformeScreen2dToWorld2d_scaled(ptrB->position.x,ptrB->position.y,pos);
                        collide = device->checkBoundCollision(ptrA->position,w1,h1,pos,w2,h2);
                        undoOffsetIfText(ptrA,w1,h1);
                        undoOffsetIfText(ptrB,w2,h2);
                    }
                }
                
            }
            else//3d
            {
                if(ptrB->is3D)//3d
                {
                    float d1 , d2 = 0.0f;
                    if(useAABB)
                    {
                        ptrA->getAABB(&w1,&h1,&d1);
                        ptrB->getAABB(&w2,&h2,&d2);
                        goOn = true;
                    }
                    else
                    {
                       goOn =   ptrA->getWidthHeight(&w1,&h1,&d1) && 
                                ptrB->getWidthHeight(&w2,&h2,&d2); 
                    }
                    if(goOn)
                    {
                        doOffsetIfText(ptrA,w1,h1);
                        doOffsetIfText(ptrB,w2,h2);
                        collide = device->checkBoundCollision(ptrA->position,w1,h1,d1,ptrB->position,w2,h2,d2);
                        undoOffsetIfText(ptrA,w1,h1);
                        undoOffsetIfText(ptrB,w2,h2);
                    }
                }
            }
            lua_pushboolean(lua, collide ? 1 : 0);
        }
		else if (top == 3)
		{
			RENDERIZABLE *ptrA  = getRenderizableFromRawTable(lua, 1, 1);
			const float x		= luaL_checknumber(lua,2);
			const float y		= luaL_checknumber(lua,3);
			const bool  useAABB     = top > 3 ? (lua_toboolean(lua,4) ? true : false)  : true;
			const VEC2 positioScreen(x,y);
			
			DEVICE *      device = DEVICE::getInstance();
			bool collide    = false;
			float w = 0.0f;
			float h = 0.0f;
			float d = 0.0f;
			if (useAABB)
				ptrA->getAABB(&w,&h,&d);
			else
				ptrA->getWidthHeight(&w,&h,&d);
			if(ptrA->is2dS)
			{
				const VEC2 rect(w*0.5f,h*0.5f);
				collide = device->isPointScreen2DOnRectangleScreen2d(positioScreen,rect,ptrA->position);
			}
			else if(ptrA->is3D == false) //2dw
			{
				const VEC2 rect(w*0.5f,h*0.5f);
				collide = device->isPointScreen2DOnRectangleWorld2d(positioScreen,rect,ptrA->position);
			}
			else//3d
			{
				VEC3 pOther(x,y,0);
				device->transformeScreen2dToWorld3d_scaled(x,y,&pOther,std::abs(ptrA->position.z));
				doOffsetIfText(ptrA,w,h);
				collide = device->checkBoundCollision(ptrA->position,w,h,d,pOther,1,1,1);
				undoOffsetIfText(ptrA,w,h);
			}
			lua_pushboolean(lua, collide ? 1 : 0);
		}
        else
        {
            return lua_error_debug(lua, "\nExpected: [renderizable],[renderizable] or\n[x,y]");
        }
        return 1;
    }

    int onGetSizeRenderizableLua(lua_State *lua)
    {
        const int          top    = lua_gettop(lua);
        RENDERIZABLE *ptr         = getRenderizableFromRawTable(lua, 1, 1);
        const bool consider_scale = top > 1 ? lua_toboolean(lua,2) : true;
        float              w   = 0;
        float              h   = 0;
        if (ptr->is3D)
        {
            float d = 0;
            if (ptr->getWidthHeight(&w, &h, &d, consider_scale))
            {
                lua_pushnumber(lua, w);
                lua_pushnumber(lua, h);
                lua_pushnumber(lua, d);
            }
            else
            {
                lua_pushnumber(lua, 0);
                lua_pushnumber(lua, 0);
                lua_pushnumber(lua, 0);
            }
            return 3;
        }
        else
        {
            if (ptr->getWidthHeight(&w, &h, consider_scale))
            {
                lua_pushnumber(lua, w);
                lua_pushnumber(lua, h);
            }
            else
            {
                lua_pushnumber(lua, 0);
                lua_pushnumber(lua, 0);
            }
            return 2;
        }
    }

    int onGetAABBRenderizableLua(lua_State *lua)
    {
        const int          top = lua_gettop(lua);
        RENDERIZABLE *ptr = getRenderizableFromRawTable(lua, 1, 1);
        float              w   = 0;
        float              h   = 0;
        if (top > 1 && lua_toboolean(lua, 2))
            ptr->updateAABB();
        if (ptr->is3D)
        {
            float d = 0;
            ptr->getAABB(&w, &h, &d);
            lua_pushnumber(lua, w);
            lua_pushnumber(lua, h);
            lua_pushnumber(lua, d);
            return 3;
        }
        else
        {
            ptr->getAABB(&w, &h);
            lua_pushnumber(lua, w);
            lua_pushnumber(lua, h);
            return 2;
        }
    }

    int onSetPosRenderizableLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            RENDERIZABLE *ptr = getRenderizableFromRawTable(lua, 1, 1);
            if(top == 2 && lua_type(lua,2) == LUA_TTABLE)
            {
                RENDERIZABLE *that = getRenderizableFromRawTable(lua, 1, 2);
                ptr->position.x = that->position.x;
                ptr->position.y = that->position.y;
                if(ptr->is3D)
                    ptr->position.z = that->position.z;
            }
            else
            {
                for (int i = 2; i <= top; ++i)
                {
                    switch (i)
                    {
                        case 2: // x
                        {
                            ptr->position.x = luaL_checknumber(lua, i);
                        }
                        break;
                        case 3: // y
                        {
                            ptr->position.y = luaL_checknumber(lua, i);
                        }
                        break;
                        case 4: // z
                        {
                            ptr->position.z = luaL_checknumber(lua, i);
                        }
                        break;
                        default: {
                        }
                        break;
                    }
                }
            }
        }
        return 0;
    }

    int onGetPosRenderizableLua(lua_State *lua)
    {
        RENDERIZABLE *ptr = getRenderizableFromRawTable(lua, 1, 1);
        return onNewVec3LuaNoGC(lua, &ptr->position);
    }

    int onSetAngleRenderizableLua(lua_State *lua)
    {
        RENDERIZABLE *ptr = getRenderizableFromRawTable(lua, 1, 1);
        const int          top = lua_gettop(lua);
        if (top > 1)
        {
            if(top == 2 && lua_type(lua,2) == LUA_TTABLE)
            {
                RENDERIZABLE *that = getRenderizableFromRawTable(lua, 1, 2);
                ptr->angle.z = that->angle.z;
                if(ptr->is3D)
                {
                    ptr->angle.x = that->angle.x;
                    ptr->angle.y = that->angle.y;
                }
            }
            else
            {
                for (int i = 2; i <= top; ++i)
                {
                    switch (i)
                    {
                        case 2: // x
                        {
                            ptr->angle.x = luaL_checknumber(lua, i);
                        }
                        break;
                        case 3: // y
                        {
                            ptr->angle.y = luaL_checknumber(lua, i);
                        }
                        break;
                        case 4: // z
                        {
                            ptr->angle.z = luaL_checknumber(lua, i);
                        }
                        break;
                        default: {
                        }
                        break;
                    }
                }
            }
        }
        return 0;
    }

    int onGetAngleRenderizableLua(lua_State *lua)
    {
        RENDERIZABLE *ptr = getRenderizableFromRawTable(lua, 1, 1);
        return onNewVec3LuaNoGC(lua, &ptr->angle);
    }

    int onSetScaleRenderizableLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top)
        {
            RENDERIZABLE *ptr = getRenderizableFromRawTable(lua, 1, 1);
            if(top == 2 && lua_type(lua,2) == LUA_TTABLE)
            {
                RENDERIZABLE *that = getRenderizableFromRawTable(lua, 1, 2);
                ptr->scale.x = that->scale.x;
                ptr->scale.y = that->scale.y;
                if(ptr->is3D)
                    ptr->scale.z = that->scale.z;
            }
            else
            {
                for (int i = 2; i <= top; ++i)
                {
                    switch (i)
                    {
                        case 2: // x
                        {
                            ptr->scale.x = luaL_checknumber(lua, i);
                        }
                        break;
                        case 3: // y
                        {
                            ptr->scale.y = luaL_checknumber(lua, i);
                        }
                        break;
                        case 4: // z
                        {
                            ptr->scale.z = luaL_checknumber(lua, i);
                        }
                        break;
                        default: {
                        }
                        break;
                    }
                }
            }
        }
        return 0;
    }

    int isOnFrustumRenderizable(lua_State *lua)
    {
        RENDERIZABLE *ptr = getRenderizableFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, ptr->isObjectOnFrustum);
        return 1;
    }

    int isLoadedRenderizable(lua_State *lua)
    {
        RENDERIZABLE *ptr = getRenderizableFromRawTable(lua, 1, 1);
        lua_pushboolean(lua, ptr->isLoaded());
        return 1;
    }

    int onDestroyRenderizable(lua_State *lua)
    {
        RENDERIZABLE *        ptr            = getRenderizableFromRawTable(lua, 1, 1);
        DEVICE *              device         = DEVICE::getInstance();
        auto * userScene      = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        auto *userDataRender = static_cast<USER_DATA_RENDER_LUA *>(ptr->userData);
        if (userDataRender)
            userDataRender->unrefAllTableLua(lua); // destroy all
        userScene->remove(ptr);
        return 0;
    }

    int onGetScaleRenderizableLua(lua_State *lua)
    {
        RENDERIZABLE *ptr = getRenderizableFromRawTable(lua, 1, 1);
        return onNewVec3LuaNoGC(lua, &ptr->scale);
    }

    int onMoveRenderizableLua(lua_State *lua)
    {
        RENDERIZABLE *ptr    = getRenderizableFromRawTable(lua, 1, 1);
        const int          top    = lua_gettop(lua);
        DEVICE *      device = DEVICE::getInstance();
        switch (top)
        {
            case 2:
            {
                const float x = luaL_checknumber(lua, 2);
                ptr->position.x += (device->delta * x);
            }
            break;
            case 3:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                ptr->position.x += (device->delta * x);
                ptr->position.y += (device->delta * y);
            }
            break;
            case 4:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                const float z = luaL_checknumber(lua, 4);
                ptr->position.x += (device->delta * x);
                ptr->position.y += (device->delta * y);
                ptr->position.z += (device->delta * z);
            }
            break;
            default: {
            }
            break;
        }
        return 0;
    }

    int onRotateRenderizableLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top == 3)
        {
            RENDERIZABLE *ptr    = getRenderizableFromRawTable(lua, 1, 1);
            const char *       angle  = luaL_checkstring(lua, 2);
            const float        radian = luaL_checknumber(lua, 3);
            DEVICE *      device = DEVICE::getInstance();
            if (angle)
            {
                switch (angle[0])
                {
                    case 'x':
                    case 'X': { ptr->angle.x += (device->delta * radian);}
                    break;
                    case 'y':
                    case 'Y': { ptr->angle.y += (device->delta * radian);}
                    break;
                    case 'z':
                    case 'Z': { ptr->angle.z += (device->delta * radian);}
                    break;
                    default: { return lua_error_debug(lua, "expected: string angle: x, y or z, number(radian)");}
                }
            }
            else
            {
                return lua_error_debug(lua, "expected: string angle: x, y or z, number(radian)");
            }
        }
        else
        {
            return lua_error_debug(lua, "expected: string angle: x, y or z, number(radian)");
        }
        return 0;
    }

    int onGetPhysicsFromRenderizable(lua_State *lua)
    {
        RENDERIZABLE * ptr                = getRenderizableFromRawTable(lua, 1, 1);
        const mbm::INFO_PHYSICS * physics = ptr->getInfoPhysics();
        lua_newtable(lua);
        if(physics)
        {
            int index_main_table = 1;   
            for(uint32_t i =0; i < physics->lsCube.size(); ++i)
            {
                const CUBE * cube = physics->lsCube[i];
                lua_newtable(lua);
                lua_pushstring(lua,"cube");
                lua_setfield(lua,-2,"type");

                lua_pushnumber(lua,cube->absCenter.x);
                lua_setfield(lua,-2,"x");

                lua_pushnumber(lua,cube->absCenter.y);
                lua_setfield(lua,-2,"y");

                lua_pushnumber(lua,cube->absCenter.z);
                lua_setfield(lua,-2,"z");

                lua_pushnumber(lua,cube->halfDim.x * 2.0f);
                lua_setfield(lua,-2,"width");

                lua_pushnumber(lua,cube->halfDim.y * 2.0f);
                lua_setfield(lua,-2,"height");

                lua_pushnumber(lua,cube->halfDim.z * 2.0f);
                lua_setfield(lua,-2,"depth");

                lua_rawseti(lua, -2, index_main_table++);
            }

            for(uint32_t i =0; i < physics->lsSphere.size(); ++i)
            {
                const SPHERE * sphere = physics->lsSphere[i];
                lua_newtable(lua);
                lua_pushstring(lua,"sphere");
                lua_setfield(lua,-2,"type");

                lua_pushnumber(lua,sphere->absCenter[0]);
                lua_setfield(lua,-2,"x");

                lua_pushnumber(lua,sphere->absCenter[1]);
                lua_setfield(lua,-2,"y");

                lua_pushnumber(lua,sphere->absCenter[2]);
                lua_setfield(lua,-2,"z");

                lua_pushnumber(lua,sphere->ray);
                lua_setfield(lua,-2,"ray");

                lua_rawseti(lua, -2, index_main_table++);
            }

            for(uint32_t i =0; i < physics->lsTriangle.size(); ++i)
            {
                const TRIANGLE * triangle = physics->lsTriangle[i];
                lua_newtable(lua);
                lua_pushstring(lua,"triangle");
                lua_setfield(lua,-2,"type");

                lua_pushnumber(lua,triangle->position.x);
                lua_setfield(lua,-2,"x");

                lua_pushnumber(lua,triangle->position.y);
                lua_setfield(lua,-2,"y");

                for (int j=0; j < 3; ++j)
                {
                    lua_newtable(lua);
                    lua_pushnumber(lua,triangle->point[j].x);
                    lua_setfield(lua,-2,"x");

                    lua_pushnumber(lua,triangle->point[j].y);
                    lua_setfield(lua,-2,"y");

                    lua_pushnumber(lua,triangle->point[j].z);
                    lua_setfield(lua,-2,"z");

                    const char l = 'a' + static_cast<char>(j);
					const char letter[2] = {l,0};
                    lua_setfield(lua,-2,letter);
                }

                lua_rawseti(lua, -2, index_main_table++);
            }

            for(uint32_t i =0; i < physics->lsCubeComplex.size(); ++i)
            {
                const CUBE_COMPLEX * cube_complex = physics->lsCubeComplex[i];
                lua_newtable(lua);
                lua_pushstring(lua,"complex");
                lua_setfield(lua,-2,"type");

                for(int j=0; j< 8; ++j)
                {
                    lua_newtable(lua);
                    
                    lua_pushnumber(lua,cube_complex->p[j].x);
                    lua_setfield(lua,-2,"x");

                    lua_pushnumber(lua,cube_complex->p[j].y);
                    lua_setfield(lua,-2,"y");

                    lua_pushnumber(lua,cube_complex->p[j].z);
                    lua_setfield(lua,-2,"z");

                    const char l = 'a' + static_cast<char>(j);
					const char letter[2] = {l,0};
                    lua_setfield(lua,-2,letter);
                }

                lua_rawseti(lua, -2, index_main_table++);
            }
            
        }
        return 1;
    }

    int onNewIndexRenderizableLua(lua_State *lua) // escrita
    {
        /*
        **********************************
                Estado da pilha
                -3|    table |1
                -2|   string |2
                -1|   number |3
        **********************************
        */
        RENDERIZABLE *ptr  = getRenderizableFromRawTable(lua, 1, 1);
        const char *       what = luaL_checkstring(lua, 2);
        const int          len  = strlen(what);
        switch (len)
        {
            case 1:
            {
                switch (what[0])
                {
                    case 'x': ptr->position.x = luaL_checknumber(lua, 3); break;
                    case 'y': ptr->position.y = luaL_checknumber(lua, 3); break;
                    case 'z': ptr->position.z = luaL_checknumber(lua, 3); break;
                    default: { return setVariable(lua, ptr, what);
                    }
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
                            case 'x': ptr->scale.x = luaL_checknumber(lua, 3); break;
                            case 'y': ptr->scale.y = luaL_checknumber(lua, 3); break;
                            case 'z': ptr->scale.z = luaL_checknumber(lua, 3); break;
                            default: { return setVariable(lua, ptr, what);
                            }
                        }
                    }
                    break;
                    case 'a':
                    {
                        switch (what[1])
                        {
                            case 'x': ptr->angle.x = luaL_checknumber(lua, 3); break;
                            case 'y': ptr->angle.y = luaL_checknumber(lua, 3); break;
                            case 'z': ptr->angle.z = luaL_checknumber(lua, 3); break;
                            default: { return setVariable(lua, ptr, what);
                            }
                        }
                    }
                    break;
                    default: { return setVariable(lua, ptr, what);
                    }
                }
            }
            break;
            case 7:
            {
                if (strcmp("visible", what) == 0)
                {
                    ptr->enableRender = lua_toboolean(lua, 3) ? true : false;
                    if (ptr->enableRender)//force on frustum at least this frame
                        ptr->isObjectOnFrustum = true;
                }
                else
                    return setVariable(lua, ptr, what);
            }
            break;
            case 12:
            {
                if (strcmp("alwaysRender", what) == 0)
                {
                    ptr->alwaysRenderize = lua_toboolean(lua, 3) ? true : false;
                    if (ptr->alwaysRenderize)//force on frustum at least this frame
                    {
                        ptr->enableRender = true;
                        ptr->isObjectOnFrustum = true;
                    }
                }
                else
                    return setVariable(lua, ptr, what);
            }
            break;
            default: { return setVariable(lua, ptr, what);
            }
        }
        return 0;
    }

    int onIndexRenderizableLua(lua_State *lua) // leitura
    {
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1|   string |2
        **********************************
        */
        RENDERIZABLE *ptr  = getRenderizableFromRawTable(lua, 1, 1);
        const char *       what = luaL_checkstring(lua, 2);
        const int          len  = strlen(what);
        switch (len)
        {
            case 1:
            {
                switch (what[0])
                {
                    case 'x': lua_pushnumber(lua, ptr->position.x); break;
                    case 'y': lua_pushnumber(lua, ptr->position.y); break;
                    case 'z': lua_pushnumber(lua, ptr->position.z); break;
                    default: { return getVariable(lua, ptr, what);
                    }
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
                            case 'x': lua_pushnumber(lua, ptr->scale.x); break;
                            case 'y': lua_pushnumber(lua, ptr->scale.y); break;
                            case 'z': lua_pushnumber(lua, ptr->scale.z); break;
                            default: { return getVariable(lua, ptr, what);
                            }
                        }
                    }
                    break;
                    case 'a':
                    {
                        switch (what[1])
                        {
                            case 'x': lua_pushnumber(lua, ptr->angle.x); break;
                            case 'y': lua_pushnumber(lua, ptr->angle.y); break;
                            case 'z': lua_pushnumber(lua, ptr->angle.z); break;
                            default: { return getVariable(lua, ptr, what);
                            }
                        }
                    }
                    break;
                    default: { return getVariable(lua, ptr, what);
                    }
                }
            }
            break;
            case 7:
            {
                if (strcmp("visible", what) == 0)
                    lua_pushboolean(lua, ptr->enableRender);
                else
                    return getVariable(lua, ptr, what);
            }
            break;
            case 12:
            {
                if (strcmp("alwaysRender", what) == 0)
                    lua_pushboolean(lua, ptr->alwaysRenderize);
                else
                    return getVariable(lua, ptr, what);
            }
            break;
            default: { return getVariable(lua, ptr, what);
            }
        }
        return 1;
    }


    SELF_ADD_COMMON_METHODS::SELF_ADD_COMMON_METHODS(luaL_Reg ptrRegMethods[])
    {
        this->regMethods                 = ptrRegMethods;
        this->prtRegAnimationsMethodsRet = nullptr;
        this->newSizeMethods             = 0;
    }

    SELF_ADD_COMMON_METHODS::~SELF_ADD_COMMON_METHODS()
    {
        if (this->prtRegAnimationsMethodsRet)
            delete[] this->prtRegAnimationsMethodsRet;
        this->prtRegAnimationsMethodsRet = nullptr;
    }


    const luaL_Reg * SELF_ADD_COMMON_METHODS::get(const luaL_Reg *regMethodsReplace)
    {
        
        static luaL_Reg regAnimationsMethods[] = {// animations
                                                    {"getAnim", onGetAnimationsManagerLua},
                                                    {"setAnim", onSetAnimationsManagerLua},
                                                    {"getIndexFrame", onGetIndexFrameAnimationsManagerLua},
                                                    {"restartAnim", onRestartAnimationsManagerLua},
                                                    {"isEndedAnim", onIsEndedAnimationsManagerLua},
                                                    {"onEndAnim", setCallBackEndAnimationsManagerLua},
                                                    {"onEndFx", setCallBackEndEffectLua },
                                                    {"setTexture", onSetTextureAnimationLua},
                                                    {"setTypeAnim", onSetAnimationTypeLua},
                                                    {"setColor", onSetTextureAnimationLua},
                                                    {"getTotalAnim", onGetTotalAnimationsManagerLua},
                                                    {"addAnim", onAddAnimationsManagerLua},
                                                    {"getTotalFrame", onGetTotalFrameAnimationsManagerLua},
                                                    // basic renderizable
                                                    {"setPos", onSetPosRenderizableLua},
                                                    {"getPos", onGetPosRenderizableLua},
                                                    {"setAngle", onSetAngleRenderizableLua},
                                                    {"getAngle", onGetAngleRenderizableLua},
                                                    {"setScale", onSetScaleRenderizableLua},
                                                    {"getScale", onGetScaleRenderizableLua},
                                                    {"isOnScreen", isOnFrustumRenderizable},
                                                    {"isLoaded", isLoadedRenderizable},
                                                    {"destroy", onDestroyRenderizable},
                                                    // util
                                                    {"move", onMoveRenderizableLua},
                                                    {"rotate", onRotateRenderizableLua},
                                                    // blend function
                                                    {"setBlend", onSetRenderRenderizableState},
                                                    {"getBlend", onGetRenderRenderizableState},
                                                    // bounding box
                                                    {"getSize", onGetSizeRenderizableLua},
                                                    {"getAABB", onGetAABBRenderizableLua},
                                                    {"isOver", onIsOverBoundingBoxRenderizable},
                                                    {"collide", onCheckCollisionBoundingBoxRenderizable},
                                                    // shader table common
                                                    {"getShader", onGetShaderTableRenderizableLuaNoGC},
													{"forceEndAnimFx", onForceEndAnimFxRenderizable },
                                                    // physics
                                                    {"getPhysics", onGetPhysicsFromRenderizable},
                                                    {nullptr, nullptr}};

        const unsigned int mysize    = (sizeof(regAnimationsMethods) / sizeof(luaL_Reg)) - 1;
        unsigned int       sizeOther = 0;
        for (unsigned int i = 0; regMethods; ++i, ++sizeOther)
        {
            luaL_Reg reg = regMethods[i];
            if (reg.func == nullptr || reg.name == nullptr)
            {
                break;
            }
        }
        this->newSizeMethods = sizeOther + mysize + 1;
        if (this->newSizeMethods)
        {
            this->prtRegAnimationsMethodsRet = new luaL_Reg[this->newSizeMethods];
            if (regMethodsReplace)
            {
                std::map<std::string, lua_CFunction> mapReg;
                for (unsigned int i = 0; regMethodsReplace[i].name; ++i)
                {
                    mapReg[regMethodsReplace[i].name] = regMethodsReplace[i].func;
                }
                for (unsigned int i = 0; i < this->newSizeMethods; ++i)
                {
                    if (i >= sizeOther)
                    {
                        unsigned int  newIndex                   = i - sizeOther;
                        luaL_Reg *    reg                        = &regAnimationsMethods[newIndex];
                        lua_CFunction newFunc                    = reg->name ? mapReg[reg->name] : nullptr;
                        this->prtRegAnimationsMethodsRet[i].func = newFunc ? newFunc : reg->func;
                        this->prtRegAnimationsMethodsRet[i].name = reg->name;
                    }
                    else
                    {

                        luaL_Reg *    reg                        = &regMethods[i]; //-V522
                        lua_CFunction newFunc                    = reg->name ? mapReg[reg->name] : nullptr; //-V522
                        this->prtRegAnimationsMethodsRet[i].func = newFunc ? newFunc : reg->func;
                        this->prtRegAnimationsMethodsRet[i].name = reg->name;
                    }
                }
            }
            else
            {
                for (unsigned int i = 0; i < this->newSizeMethods; ++i)
                {
                    if (i >= sizeOther)
                    {
                        unsigned int newIndex                    = i - sizeOther;
                        luaL_Reg *   reg                         = &regAnimationsMethods[newIndex];
                        this->prtRegAnimationsMethodsRet[i].func = reg->func;
                        this->prtRegAnimationsMethodsRet[i].name = reg->name;
                    }
                    else
                    {

                        luaL_Reg *reg                            = &regMethods[i];
                        this->prtRegAnimationsMethodsRet[i].func = reg->func; //-V522
                        this->prtRegAnimationsMethodsRet[i].name = reg->name;
                    }
                }
            }
            return this->prtRegAnimationsMethodsRet;
        }
        return nullptr;
    }

    unsigned int SELF_ADD_COMMON_METHODS::getSize() const
    {
        return this->newSizeMethods;
    }
      
};
