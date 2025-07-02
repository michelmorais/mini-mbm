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


#ifndef PLUGIN_HELPER
#define PLUGIN_HELPER

extern "C"
{
    #include <lualib.h>
    #include <lauxlib.h>
    #include <lua.h>
}

#include <core_mbm/class-identifier.h>
#include <core_mbm/renderizable.h>
#include <vector>

enum TYPE_LOG : char;

namespace plugin_helper
{
    void lua_print_line(lua_State *lua, TYPE_LOG type_log, const char *format, ...);
    int  lua_error_debug(lua_State *lua,  const char *format, ...);
    void getFieldPrimaryFromTable(lua_State *lua, const int indexTable, const char *fieldName, const int LUA_TYPE,void *ptrRet);
    void getFieldUnsignedShortFromTable(lua_State *lua, const int indexTable, const char *fieldName,unsigned short int *ptrRet);
    void getFieldSignedShortFromTable(lua_State *lua, const int indexTable, const char *fieldName, short int *ptrRet);
    void getFieldIntegerFromTable(lua_State *lua, const int indexTable, const char *fieldName,int *ptrRet);
    void getFloat2FieldTableFromTable(lua_State *lua, const int indexTable, const char *fieldNameTable,const char *fieldName1, const char *fieldName2, float *out1, float *out2);
    void printStack(lua_State *lua, const char *fileName, const unsigned int numLine);
    void *lua_check_userType (  lua_State *lua,
                                const int rawi, 
                                const int indexTable,
                                const mbm::L_USER_TYPE expectedType);
    void *lua_get_userType_no_throw (  lua_State *lua,
                                const int rawi, 
                                const int indexTable,
                                const mbm::L_USER_TYPE expectedType);
    mbm::RENDERIZABLE * getRenderizableFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    mbm::RENDERIZABLE * getRenderizableNoThrowFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    void lua_create_metatable_identifier(lua_State *lua,const char* _metatable_plugin,const int value);
}

namespace mbm
{
    struct USER_DATA_AUDIO_LUA;
    class TIMER_CALL_BACK;
    
    //this is created at mbm engine side. so, we copy exactly the struct from lib side. It needs to remove this... shame on this
    struct USER_DATA_SCENE_LUA
    {
        lua_State *                        lua;
        lua_CFunction                      oldPanicFunction;
        std::vector<USER_DATA_AUDIO_LUA *> lsLuaCallBackStream;
        std::vector<RENDERIZABLE *>        lsLuaCallBackOnTouchAsynchronous;
        std::vector<RENDERIZABLE *>        lsLuaCallBackOnTouchSynchronous;
        std::vector<TIMER_CALL_BACK *>     lsTimerCallBack;
        std::map<std::string, DYNAMIC_VAR *> _lsDynamicVarCam2d;
        std::map<std::string, DYNAMIC_VAR *> _lsDynamicVarCam3d;
        USER_DATA_SCENE_LUA();
        virtual ~USER_DATA_SCENE_LUA();
        void remove(TIMER_CALL_BACK *obj);
        void remove(RENDERIZABLE *obj);
    };

    struct REF_FUNCTION_LUA
    {
        constexpr REF_FUNCTION_LUA(){}
        virtual ~REF_FUNCTION_LUA();
        void refFunctionLua(lua_State *lua, const int index, int *ref_MeAsTable);
        void refTableLua(lua_State *lua, const int index, int *ref_MeAsTable);
        void unrefTableLua(lua_State *lua, int *ref_MeAsTable);
        virtual void unrefAllTableLua(lua_State *lua) = 0; // destroy all
    };

    struct USER_DATA_RENDER_LUA : public REF_FUNCTION_LUA
    {
        float x, y, z; // where touch down
        int   key;
        int   ref_MeAsTable; // me as lua script
        int   ref_CallBackAnimation;
        int   ref_CallBackEffectShader;
        int   ref_CallBackTouchDown;
        void *extra;
        USER_DATA_RENDER_LUA();
        virtual ~USER_DATA_RENDER_LUA();
        virtual void unrefAllTableLua(lua_State *lua);
    };
}

#endif // !PLUGIN_HELPER