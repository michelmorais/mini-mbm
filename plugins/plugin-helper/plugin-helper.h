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


#endif // !PLUGIN_HELPER