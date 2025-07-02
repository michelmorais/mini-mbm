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

#include <lua-wrap/camera-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/vec3-lua.h>
#include <lua-wrap/vec2-lua.h>
#include <core_mbm/dynamic-var.h>
#include <core_mbm/device.h>
#include <core_mbm/primitives.h>
#include <platform/mismatch-platform.h>

namespace mbm
{
    extern void unrefTableByIdTableRef(DYNAMIC_VAR *dyVar, lua_State *lua);
    extern int onNewVec2LuaNoGC(lua_State *lua, VEC2 *vec2);
    extern int onNewVec2LuaNoGC(lua_State *lua, VEC3 *vec3);
    extern int onNewVec3Lua(lua_State *lua);
    extern int onNewVec3LuaNoGC(lua_State *lua, VEC3 *vec3);
	extern int lua_error_debug(lua_State *lua, const char *format, ...);

    int getVariableFromCam(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what)
    {
        const char * strinChar = nullptr;
        DYNAMIC_VAR *dyVar     = lsDynamicVar[what];
        if (dyVar == nullptr)
        {
            lua_pushnil(lua);
        }
        else
        {
            switch (dyVar->type)
            {
                case DYNAMIC_BOOL:
                {
                    const bool value = dyVar->getBool();
                    lua_pushboolean(lua, value ? 1 : 0);
                }
                break;
                case DYNAMIC_CHAR:
                {
                    const char value = dyVar->getChar();
                    char       str[2];
                    str[0] = value;
                    str[1] = 0;
                    lua_pushstring(lua, str);
                }
                break;
                case DYNAMIC_INT:
                {
                    const int value = dyVar->getInt();
                    lua_pushinteger(lua, value);
                }
                break;
                case DYNAMIC_FLOAT:
                {
                    const float value = dyVar->getFloat();
                    lua_pushnumber(lua, value);
                }
                break;
                case DYNAMIC_CSTRING:
                {
                    strinChar = dyVar->getString();
                    lua_pushstring(lua, strinChar);
                }
                break;
                case DYNAMIC_SHORT:
                {
                    const short value = dyVar->getShort();
                    lua_pushinteger(lua, (const int)value);
                }
                break;
                case DYNAMIC_VOID: { return lua_error_debug(lua, "variable [%s] void!", what);}
                case DYNAMIC_TABLE:
                {
                    const int tref = dyVar->getInt();
                    lua_rawgeti(lua, LUA_REGISTRYINDEX, tref);
                }
                break;
                case DYNAMIC_FUNCTION:
                {
                    const int tref = dyVar->getInt();
                    lua_rawgeti(lua, LUA_REGISTRYINDEX, tref);
                }
                break;
                default:
                {
                    return lua_error_debug(lua, "variable [%s] unknown!", what);
                }
            }
        }
        return 1;
    }

    int setVariableFromCam(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what)
    {
        const int    top   = lua_gettop(lua);
        const int    type  = lua_type(lua, top);
        DYNAMIC_VAR *dyVar = lsDynamicVar[what];
        switch (type)
        {
            case LUA_TNIL:
            {
                unrefTableByIdTableRef(dyVar, lua);
                if (dyVar)
                    delete dyVar;
                dyVar              = nullptr;
                lsDynamicVar[what] = nullptr;
            }
            break;
            case LUA_TNUMBER:
            {
                float var = lua_tonumber(lua, top);
                unrefTableByIdTableRef(dyVar, lua);
                if (dyVar == nullptr || dyVar->type != DYNAMIC_FLOAT)
                {
                    if (dyVar)
                    {
                        delete dyVar;
                        dyVar = nullptr;
                    }
                    dyVar              = new DYNAMIC_VAR(DYNAMIC_FLOAT,static_cast<const void*>(&var));
                    lsDynamicVar[what] = (dyVar);
                }
                else 
                {
                    switch(dyVar->type)
                    {
                        case DYNAMIC_FLOAT:
                        {
                            dyVar->setFloat(var);
                        }
                        break;
                        case DYNAMIC_INT:
                        {
                            dyVar->setInt(static_cast<int>(var));
                        }
                        break;
                        case DYNAMIC_SHORT:
                        {
                            dyVar->setShort(static_cast<short int>(var));
                        }
                        break;
                        default:{}
                    }
                }
            }
            break;
            case LUA_TBOOLEAN:
            {
                bool var = lua_toboolean(lua, top) ? true : false;
                unrefTableByIdTableRef(dyVar, lua);
                if (dyVar == nullptr || dyVar->type != DYNAMIC_BOOL)
                {
                    if (dyVar)
                    {
                        delete dyVar;
                        dyVar = nullptr;
                    }
                    dyVar              = new DYNAMIC_VAR(DYNAMIC_BOOL, static_cast<const void*>(&var));
                    lsDynamicVar[what] = (dyVar);
                }
                else if (dyVar->type == DYNAMIC_BOOL)
                {
                    dyVar->setBool(var);
                }
            }
            break;
            case LUA_TSTRING:
            {
                const char *var = lua_tostring(lua, top);
                unrefTableByIdTableRef(dyVar, lua);
                if (dyVar == nullptr || dyVar->type != DYNAMIC_CSTRING)
                {
                    if (dyVar)
                    {
                        delete dyVar;
                        dyVar = nullptr;
                    }
                    dyVar              = new DYNAMIC_VAR(DYNAMIC_CSTRING,static_cast<const void*>(var));
                    lsDynamicVar[what] = (dyVar);
                }
                else if (dyVar->type == DYNAMIC_CSTRING)
                {
                    dyVar->setString(var);
                }
            }
            break;
            case LUA_TTABLE:
            {
                const int tref = luaL_ref(lua, LUA_REGISTRYINDEX);
                unrefTableByIdTableRef(dyVar, lua);
                if (dyVar == nullptr || dyVar->type != DYNAMIC_TABLE)
                {
                    if (dyVar)
                    {
                        delete dyVar;
                        dyVar = nullptr;
                    }
                    dyVar              = new DYNAMIC_VAR(DYNAMIC_TABLE,static_cast<const void*>(&tref));
                    lsDynamicVar[what] = dyVar;
                }
                else if (dyVar->type == DYNAMIC_TABLE)
                {
                    dyVar->setVoid(static_cast<const void*>(&tref));
                }
            }
            break;
            case LUA_TFUNCTION:
            {
                const int tref = luaL_ref(lua, LUA_REGISTRYINDEX);
                unrefTableByIdTableRef(dyVar, lua);
                if (dyVar == nullptr || dyVar->type != DYNAMIC_FUNCTION)
                {
                    if (dyVar)
                    {
                        delete dyVar;
                        dyVar = nullptr;
                    }
                    dyVar              = new DYNAMIC_VAR(DYNAMIC_FUNCTION,static_cast<const void*>(&tref));
                    lsDynamicVar[what] = dyVar;
                }
                else if (dyVar->type == DYNAMIC_FUNCTION)
                {
                    dyVar->setVoid(static_cast<const void*>(&tref));
                }
            }
            break;
            case LUA_TUSERDATA: { return lua_error_debug(lua, "variable [%s] userdata not allowed!", what);}
            case LUA_TTHREAD: { return lua_error_debug(lua, "variable [%s] thread not allowed!", what);}
            case LUA_TLIGHTUSERDATA: { return lua_error_debug(lua, "variable [%s] light userdata not allowed!", what);}
            default: { return lua_error_debug(lua, "variable [%s] unknown!", what);}
        }
        return 0;
    }

    int onScaleToScreen(lua_State *lua)
    {
        const int    top     = lua_gettop(lua);
        const auto  x       = static_cast<const float>(luaL_checknumber(lua, 2));
        const auto  y       = static_cast<const float>(luaL_checknumber(lua, 3));
        const char * stretch = top == 4 ? luaL_checkstring(lua, 4) : nullptr;
        DEVICE *device  = DEVICE::getInstance();
        device->scaleToScreen(x, y, stretch);
        return 0;
    }

    int onSetPos2dCameraLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            DEVICE *device = DEVICE::getInstance();
            CAMERA *     camera = &device->camera;
            if(lua_type(lua, 2) == LUA_TTABLE)
            {
                VEC2 *vec = getVec2FromRawTable(lua, 1, 2);
                camera->position2d.x = vec->x  * camera->scale2d.x;
                camera->position2d.y = vec->y  * camera->scale2d.y;
            }
            else
            {
                for (int i = 2; i <= top; ++i)
                {
                    switch (i)
                    {
                        case 2: // x
                        {
                            camera->position2d.x = luaL_checknumber(lua, i) * camera->scale2d.x;
                        }
                        break;
                        case 3: // y
                        {
                            camera->position2d.y = luaL_checknumber(lua, i) * camera->scale2d.y;
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

    int onGetPos2dCameraLua(lua_State *lua)
    {
        DEVICE *device = DEVICE::getInstance();
        CAMERA *     camera = &device->camera;
        return onNewVec2LuaNoGC(lua, &camera->position2d);
    }

    int onSetPos3dCameraLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            DEVICE *device = DEVICE::getInstance();
            CAMERA *     camera = &device->camera;
            if(lua_type(lua, 2) == LUA_TTABLE)
            {
                VEC3 *vec = getVec3FromRawTable(lua, 1, 2);
                camera->position = *vec;
            }
            else
            {
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
        }
        return 0;
    }

    int onSetUp3dCameraLua(lua_State *lua)
    {
        DEVICE *device = DEVICE::getInstance();
        CAMERA *     camera = &device->camera;
        if(lua_type(lua, 2) == LUA_TTABLE)
        {
            VEC3 *vec = getVec3FromRawTable(lua, 1, 2);
            camera->up = *vec;
        }
        else
        {
            camera->up.x        = luaL_checknumber(lua, 2);
            camera->up.y        = luaL_checknumber(lua, 3);
            camera->up.z        = luaL_checknumber(lua, 4);
        }
        return 0;
    }

    int onGetPos3dCameraLua(lua_State *lua)
    {
        DEVICE *device = DEVICE::getInstance();
        CAMERA *     camera = &device->camera;
        return onNewVec3LuaNoGC(lua, &camera->position);
    }

    int onGetFocusCameraLua(lua_State *lua)
    {
        DEVICE *device = DEVICE::getInstance();
        CAMERA *     camera = &device->camera;
        return onNewVec3LuaNoGC(lua, &camera->focus);
    }

    int onGetNormalDirectionCameraLua(lua_State *lua)
    {
        DEVICE *device          = DEVICE::getInstance();
        CAMERA *     camera     = &device->camera;
        const char* direction   = lua_tostring(lua, 2);
        switch(direction[0])
        {
            case 'R':
            {
                return onNewVec3LuaNoGC(lua, &camera->normalRight);
            }
            break;
            case 'L':
            {
                return onNewVec3LuaNoGC(lua, &camera->normalLeft);
            }
            break;
            case 'F':
            {
                return onNewVec3LuaNoGC(lua, &camera->normalForward);
            }
            break;
            case 'B':
            {
                return onNewVec3LuaNoGC(lua, &camera->normalBackward);
            }
            break;
            case 'U':
            {
                return onNewVec3LuaNoGC(lua, &camera->normalUp);
            }
            break;
            case 'D':
            {
                static VEC3 down;
                down.x = -camera->normalUp.x;
                down.y = -camera->normalUp.y;
                down.z = -camera->normalUp.z;
                return onNewVec3LuaNoGC(lua, &down);
            }
            break;
            default:
            {
                const char * expected = "exepected string [R, L, U, D, B or F] number [unit] (to get the normal direction)";
                return lua_error_debug(lua, expected);
            }
        }
    }

    int onSetFocusCameraLua(lua_State *lua)
    {
        const int top = lua_gettop(lua);
        if (top > 1)
        {
            DEVICE *device = DEVICE::getInstance();
            CAMERA *     camera = &device->camera;
            if(lua_type(lua, 2) == LUA_TTABLE)
            {
                VEC3 *vec = getVec3FromRawTable(lua, 1, 2);
                camera->focus = *vec;
            }
            else
            {
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
        }
        return 0;
    }

    int onNewIndexCamera2d(lua_State *lua) // escrita
    {
        /*
        **********************************
                Estado da pilha
                -3|    table |1
                -2|   string |2
                -1|   number |3
        **********************************
        */
        DEVICE *        device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        CAMERA *             camera    = &device->camera;
        const char *         what      = luaL_checkstring(lua, 2);
        const int            len       = strlen(what);
        switch (len)
        {
            case 1:
            {
                switch (what[0])
                {
                    case 'x': { camera->position2d.x = luaL_checknumber(lua, 3) * camera->scale2d.x;}
                    break;
                    case 'y': { camera->position2d.y = luaL_checknumber(lua, 3) * camera->scale2d.y;}
                    break;
                    default: { return setVariableFromCam(lua, userScene->_lsDynamicVarCam2d, what);}
                }
            }
            break;
            case 2:
            {
                if (what[0] == 's')
                {
                    switch (what[1])
                    {
                        case 'x': { camera->scale2d.x = luaL_checknumber(lua, 3);}
                        break;
                        case 'y': { camera->scale2d.y = luaL_checknumber(lua, 3);}
                        break;
                        default: { return setVariableFromCam(lua, userScene->_lsDynamicVarCam2d, what);}
                    }
                }
                else
                {
                    return setVariableFromCam(lua, userScene->_lsDynamicVarCam2d, what);
                }
            }
            break;
            default: { return setVariableFromCam(lua, userScene->_lsDynamicVarCam2d, what);}
        }
        return 0;
    }

    int onNewIndexCamera3d(lua_State *lua) // escrita
    {
        /*
        **********************************
                Estado da pilha
                -3|    table |1
                -2|   string |2
                -1|   number |3
        **********************************
        */
        DEVICE *        device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        CAMERA *             camera    = &device->camera;
        const char *         what      = luaL_checkstring(lua, 2);
        const int            len       = strlen(what);
        switch (len)
        {
            case 1:
            {
                switch (what[0])
                {
                    case 'x': { camera->position.x = luaL_checknumber(lua, 3);}
                    break;
                    case 'y': { camera->position.y = luaL_checknumber(lua, 3);}
                    break;
                    case 'z': { camera->position.z = luaL_checknumber(lua, 3);}
                    break;
                    default: { return setVariableFromCam(lua, userScene->_lsDynamicVarCam3d, what);}
                }
            }
            break;
            case 2:
            {
                switch (what[0])
                {
                    case 'f':
                    {
                        switch (what[1])
                        {
                            case 'x': { camera->focus.x = luaL_checknumber(lua, 3);}
                            break;
                            case 'y': { camera->focus.y = luaL_checknumber(lua, 3);}
                            break;
                            case 'z': { camera->focus.z = luaL_checknumber(lua, 3);}
                            break;
                            default: { return setVariableFromCam(lua, userScene->_lsDynamicVarCam3d, what);}
                        }
                    }
                    break;
                    default: { return setVariableFromCam(lua, userScene->_lsDynamicVarCam3d, what);}
                }
            }
            break;
            default: { return setVariableFromCam(lua, userScene->_lsDynamicVarCam3d, what);}
        }
        return 0;
    }

    int onIndexCamera2d(lua_State *lua) // leitura
    {
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1|   string |2
        **********************************
        */
        DEVICE *        device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        CAMERA *             camera    = &device->camera;
        const char *         what      = luaL_checkstring(lua, 2);
        const int            len       = strlen(what);
        switch (len)
        {
            case 1:
            {
                switch (what[0])
                {
                    case 'x': { lua_pushnumber(lua, camera->position2d.x / camera->scale2d.x);}
                    break;
                    case 'y': { lua_pushnumber(lua, camera->position2d.y / camera->scale2d.y);}
                    break;
                    default: { return getVariableFromCam(lua, userScene->_lsDynamicVarCam2d, what);}
                }
            }
            break;
            case 2:
            {
                if (what[0] == 's')
                {
                    switch (what[1])
                    {
                        case 'x': { lua_pushnumber(lua, camera->scale2d.x);}
                        break;
                        case 'y': { lua_pushnumber(lua, camera->scale2d.y);}
                        break;
                        default: { return getVariableFromCam(lua, userScene->_lsDynamicVarCam2d, what);}
                    }
                }
                else
                {
                    return getVariableFromCam(lua, userScene->_lsDynamicVarCam2d, what);
                }
            }
            break;
            default: { return getVariableFromCam(lua, userScene->_lsDynamicVarCam2d, what);}
        }
        return 1;
    }

    int onIndexCamera3d(lua_State *lua) // leitura
    {
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1|   string |2
        **********************************
        */
        DEVICE *        device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        CAMERA *             camera    = &device->camera;
        const char *         what      = luaL_checkstring(lua, 2);
        const int            len       = strlen(what);
        switch (len)
        {
            case 1:
            {
                switch (what[0])
                {
                    case 'x': { lua_pushnumber(lua, camera->position.x);}
                    break;
                    case 'y': { lua_pushnumber(lua, camera->position.y);}
                    break;
                    case 'z': { lua_pushnumber(lua, camera->position.z);}
                    break;
                    default: { return getVariableFromCam(lua, userScene->_lsDynamicVarCam3d, what);}
                }
            }
            break;
            case 2:
            {
                switch (what[0])
                {
                    case 'f':
                    {
                        switch (what[1])
                        {
                            case 'x': { lua_pushnumber(lua, camera->focus.x);}
                            break;
                            case 'y': { lua_pushnumber(lua, camera->focus.y);}
                            break;
                            case 'z': { lua_pushnumber(lua, camera->focus.z);}
                            break;
                            default: { return getVariableFromCam(lua, userScene->_lsDynamicVarCam3d, what);}
                        }
                    }
                    break;
                    default: { return getVariableFromCam(lua, userScene->_lsDynamicVarCam3d, what);}
                }
            }
            break;
            default: { return getVariableFromCam(lua, userScene->_lsDynamicVarCam3d, what);}
        }
        return 1;
    }

    int onMoveCamera2d(lua_State *lua)
    {
        DEVICE *device = DEVICE::getInstance();
        CAMERA *     camera = &device->camera;
        const int    top    = lua_gettop(lua);
        switch (top)
        {
            case 2:
            {
                if(lua_type(lua, 2) == LUA_TTABLE)
                {
                    VEC2 *vec = getVec2FromRawTable(lua, 1, 2);
                    camera->position2d.x += (device->delta * vec->x);
                    camera->position2d.y += (device->delta * vec->y);
                }
                else
                {
                    const float x = luaL_checknumber(lua, 2);
                    camera->position2d.x += (device->delta * x);
                }
            }
            break;
            case 3:
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                camera->position2d.x += (device->delta * x);
                camera->position2d.y += (device->delta * y);
            }
            break;
            default: {
            }
            break;
        }
        return 0;
    }

    int onMoveCamera3d(lua_State *lua)
    {
        DEVICE *device      = DEVICE::getInstance();
        CAMERA *     camera = &device->camera;
        VEC3* position      = &camera->position;
        const float delta   = device->delta;
        const int    top    = lua_gettop(lua);
        const char * expected = "exepected [vec3] or\nstring [R, L, U, D, B or F] number [unit] (to move in some direction) or\nnumber [x] number [y] number [z]\nto move camera";
        const int type      = top > 1 ? lua_type(lua, 2) : LUA_TNONE;
        switch (top)
        {
            case 2://one arg (vec3)
            {
                if(type == LUA_TTABLE)
                {
                    VEC3 *vec   = getVec3FromRawTable(lua, 1, 2);
                    position->x += (delta * vec->x);
                    position->y += (delta * vec->y);
                    position->z += (delta * vec->z);
                }
                else
                {
                    return lua_error_debug(lua, expected);
                }
            }
            break;
            case 3://two arg (x,y) or (direction,unit)
            {
                switch(type)
                {
                    case LUA_TNUMBER:
                    {
                        const float x = luaL_checknumber(lua, 2);
                        const float y = luaL_checknumber(lua, 3);
                        position->x += (delta * x);
                        position->y += (delta * y);
                    }
                    break;
                    case LUA_TSTRING:
                    {
                        const char* direction   = lua_tostring(lua, 2);
                        const float unit        = luaL_checknumber(lua, 3);
                        switch(direction[0])
                        {
                            case 'R':
                            {
                                position->x += (camera->normalRight.x * delta * unit);
                                position->y += (camera->normalRight.y * delta * unit);
                                position->z += (camera->normalRight.z * delta * unit);
                            }
                            break;
                            case 'L':
                            {
                                position->x += (camera->normalLeft.x * delta * unit);
                                position->y += (camera->normalLeft.y * delta * unit);
                                position->z += (camera->normalLeft.z * delta * unit);
                            }
                            break;
                            case 'F':
                            {
                                position->x += (camera->normalForward.x * delta * unit);
                                position->y += (camera->normalForward.y * delta * unit);
                                position->z += (camera->normalForward.z * delta * unit);
                            }
                            break;
                            case 'B':
                            {
                                position->x += (camera->normalBackward.x * delta * unit);
                                position->y += (camera->normalBackward.y * delta * unit);
                                position->z += (camera->normalBackward.z * delta * unit);
                            }
                            break;
                            case 'U':
                            {
                                position->x += (camera->normalUp.x * delta * unit);
                                position->y += (camera->normalUp.y * delta * unit);
                                position->z += (camera->normalUp.z * delta * unit);
                            }
                            break;
                            case 'D':
                            {
                                position->x += ((-camera->normalUp.x) * delta * unit);
                                position->y += ((-camera->normalUp.y) * delta * unit);
                                position->z += ((-camera->normalUp.z) * delta * unit);
                            }
                            break;
                            default:
                            {
                                return lua_error_debug(lua, expected);
                            }
                        }
                    }
                    break;
                    default:
                    {
                        return lua_error_debug(lua, expected);
                    }
                }
            }
            break;
            case 4://three args x,y,z
            {
                const float x = luaL_checknumber(lua, 2);
                const float y = luaL_checknumber(lua, 3);
                const float z = luaL_checknumber(lua, 4);
                position->x += (delta * x);
                position->y += (delta * y);
                position->z += (delta * z);
            }
            break;
            default:
            {
                return lua_error_debug(lua, expected);
            }
        }
        return 0;
    }

    int onSetAngleOfView(lua_State *lua) // in degrees
    {
        const float  angle  = luaL_checknumber(lua, 2);
        DEVICE *device = DEVICE::getInstance();
        device->camera.angleOfView = angle;
        return 0;
    }

    int onSetFar(lua_State *lua)
    {
        const float  ffar   = luaL_checknumber(lua, 2);
        DEVICE *device      = DEVICE::getInstance();
        device->camera.zFar = ffar;
        return 0;
    }

    int onSetNear(lua_State *lua)
    {
        const float  zNear  = luaL_checknumber(lua, 2);
        DEVICE *device      = DEVICE::getInstance();
        device->zNear       = zNear;
        device->camera.zNear       = zNear;
        return 0;
    }

    
    int onGetCamera(lua_State *lua)
    {
        const int    top    = lua_gettop(lua);
        const char* type_cam= luaL_checkstring(lua, 1);
        const bool   _2d    = top == 1 ? strcasecmp(type_cam, "2d") == 0 || strcasecmp(type_cam, "2dw") == 0 : true;
        lua_settop(lua, 0);
        if (_2d)
        {
            luaL_Reg regCamera2dMethods[] = {{"setPos", onSetPos2dCameraLua},
                                             {"getPos", onGetPos2dCameraLua},
                                             {"scaleToScreen", onScaleToScreen},
                                             {"move", onMoveCamera2d},
                                             {nullptr, nullptr}};
            luaL_newlib(lua, regCamera2dMethods);
            luaL_getmetatable(lua, "_mbmCamera2d");
        }
        else
        {
            luaL_Reg regCamera3dMethods[] = {{"setPos", onSetPos3dCameraLua},
                                             {"setUp", onSetUp3dCameraLua},
                                             {"getPos", onGetPos3dCameraLua},
                                             {"getFocus", onGetFocusCameraLua},
                                             {"setFocus", onSetFocusCameraLua},
                                             {"getNormal", onGetNormalDirectionCameraLua},
                                             {"move", onMoveCamera3d},
                                             {"setAngleOfView", onSetAngleOfView},
                                             {"setFar", onSetFar},
                                             {"setNear", onSetNear},
                                             {nullptr, nullptr}};
            luaL_newlib(lua, regCamera3dMethods);
            luaL_getmetatable(lua, "_mbmCamera3d");
        }
        lua_setmetatable(lua, -2);
        return 1;
    }

    void registerClassCamera(lua_State *lua)
    {
        luaL_Reg regCamera2dMethods[] = {{"__newindex", onNewIndexCamera2d}, {"__index", onIndexCamera2d}, {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmCamera2d");
        luaL_setfuncs(lua, regCamera2dMethods, 0);
        lua_settop(lua,0);

        luaL_Reg regCamera3dMethods[] = {{"__newindex", onNewIndexCamera3d}, {"__index", onIndexCamera3d}, {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmCamera3d");
        luaL_setfuncs(lua, regCamera3dMethods, 0);
        lua_settop(lua,0);
    }
};
