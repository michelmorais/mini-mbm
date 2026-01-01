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


#if defined (__GNUC__) 
  #define PLUGIN_HELPER_API  __attribute__ ((__visibility__("default")))
#elif defined (WIN32)
  #ifdef PLUGIN_HELPER_BUILD_DLL
    #define PLUGIN_HELPER_API  __declspec(dllexport)
  #else
    #define PLUGIN_HELPER_API   __declspec(dllimport)
  #endif
#endif

extern "C"
{
    #include <lualib.h>
    #include <lauxlib.h>
    #include <lua.h>
}


#include <core_mbm/class-identifier.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/animation.h>
#include <render/line-mesh.h>
#include <vector>


enum TYPE_LOG : char;

namespace mbm
{
    extern "C" PLUGIN_HELPER_API void lua_print_line(lua_State *lua, TYPE_LOG type_log, const char *format, ...);
    extern "C" PLUGIN_HELPER_API int  lua_error_debug(lua_State *lua,  const char *format, ...);
    extern "C" PLUGIN_HELPER_API void printStack(lua_State *lua, const char *fileName, const unsigned int numLine);
    extern "C" PLUGIN_HELPER_API void *lua_check_userType (  lua_State *lua, const int rawi, const int indexTable, const L_USER_TYPE expectedType);
    extern "C" PLUGIN_HELPER_API void getFieldPrimaryFromTable(lua_State *lua, const int indexTable, const char *fieldName, const int LUA_TYPE,void *ptrRet);
    extern "C" PLUGIN_HELPER_API void getFieldUnsignedShortFromTable(lua_State *lua, const int indexTable, const char *fieldName,unsigned short int *ptrRet);
    extern "C" PLUGIN_HELPER_API void getFieldUnsigned8FromTable(lua_State *lua, const int indexTable, const char *fieldName,uint8_t *ptrRet);
    extern "C" PLUGIN_HELPER_API void getFieldSignedShortFromTable(lua_State *lua, const int indexTable, const char *fieldName, short int *ptrRet);
    extern "C" PLUGIN_HELPER_API void getFieldIntegerFromTable(lua_State *lua, const int indexTable, const char *fieldName,int *ptrRet);
    extern "C" PLUGIN_HELPER_API void getFloat2FieldTableFromTable(lua_State *lua, const int indexTable, const char *fieldNameTable,const char *fieldName1, const char *fieldName2, float *out1, float *out2);
    extern "C" PLUGIN_HELPER_API void *lua_get_userType_no_throw(lua_State *lua, const int rawi, const int indexTable, const L_USER_TYPE expectedType);
    extern "C" PLUGIN_HELPER_API RENDERIZABLE * getRenderizableFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    extern "C" PLUGIN_HELPER_API RENDERIZABLE * getRenderizableNoThrowFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    extern "C" PLUGIN_HELPER_API void lua_create_metatable_identifier(lua_State *lua,const char* _metatable_plugin,const int value);

    extern "C" PLUGIN_HELPER_API void getArrayFromTablePixels(lua_State *lua, const int index, uint8_t *lsArrayOut, const uint32_t sizeBuffer);
    extern "C" PLUGIN_HELPER_API void getArrayFloatFromTable(lua_State *lua, const int index, float *lsArrayOut, const uint32_t sizeBuffer);
    extern "C" PLUGIN_HELPER_API void getArrayXYZ_noZ_FromTable(lua_State *lua, const int index, std::vector<VEC3> & xyz_out);
    extern "C" PLUGIN_HELPER_API void getArrayXYZ_FromTable(lua_State *lua, const int index, std::vector<VEC3> & xyz_out);
    extern "C" PLUGIN_HELPER_API void getArrayUintFromTable(lua_State *lua, const int index, uint16_t *lsArrayOut, const uint32_t sizeBuffer);
    extern "C" PLUGIN_HELPER_API void get2ArrayFromTableWithField(lua_State *lua, const int index, float *lsArrayOut, const uint32_t sizeArray,const char * field_a,const char * field_b);
    extern "C" PLUGIN_HELPER_API void get3ArrayFromTableWithField(lua_State *lua, const int index, float *lsArrayOut, const uint32_t sizeArray,const char * field_a,const char * field_b,const char * field_c);
    extern "C" PLUGIN_HELPER_API ANIMATION_MANAGER *getAnimationManagerFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    extern "C" PLUGIN_HELPER_API ANIMATION_MANAGER *getSafeAnimationManagerFromRenderizable(lua_State *lua,RENDERIZABLE * renderizable);
    extern "C" PLUGIN_HELPER_API FX *getFxFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    extern "C" PLUGIN_HELPER_API FX *getSafeFxFromRenderizable(lua_State *lua, RENDERIZABLE * renderizable);
    extern "C" PLUGIN_HELPER_API ANIMATION *getSafeAnimFromRenderizable(lua_State *lua, RENDERIZABLE * renderizable);
    extern "C" PLUGIN_HELPER_API int getDynamicVariable(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what);
    extern "C" PLUGIN_HELPER_API int getVariable(lua_State *lua, RENDERIZABLE *ptr, const char *what);
    extern "C" PLUGIN_HELPER_API void getFieldUnsignedFromTable(lua_State *lua, const int indexTable, const char *fieldName,uint32_t *ptrRet);
    
    extern "C" PLUGIN_HELPER_API void push2VectorArrayToTableWithField(lua_State * lua, const std::vector<float> & vec, const char* field_a, const char* field_b);
    extern "C" PLUGIN_HELPER_API void push3VectorArrayToTableWithField(lua_State * lua, const std::vector<float> & vec, const char* field_a, const char* field_b, const char* field_c);
    extern "C" PLUGIN_HELPER_API void push_uint16_arrayFromTable(lua_State *lua, const uint16_t * lsArrayIn, const unsigned int sizeBuffer,const bool one_based);
    extern "C" PLUGIN_HELPER_API void unrefTableByIdTableRef(DYNAMIC_VAR *dyVar, lua_State *lua);
    extern "C" PLUGIN_HELPER_API void setDynamicVar(const char *nameVar, DYNAMIC_VAR *    nDvar,std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar);
    extern "C" PLUGIN_HELPER_API int setDynamicVariable(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what);
    extern "C" PLUGIN_HELPER_API int setVariable(lua_State *lua, RENDERIZABLE *ptr, const char *what);
    extern "C" PLUGIN_HELPER_API const char *getRandomNameTexture();
    extern "C" PLUGIN_HELPER_API int verifyDynamicCast(lua_State *lua, void *ptr, int line, const char *__file);
    extern "C" PLUGIN_HELPER_API int errorLuaPushFalse(lua_State *lua,const char* msg);
    extern "C" PLUGIN_HELPER_API int onSetPhysicsFromTableLuaToLineMesh(lua_State *lua,INFO_PHYSICS* infoPhysics,LINE_MESH * lineMesh);
    extern "C" PLUGIN_HELPER_API int onSetPhysicsFromTableLua(lua_State *lua,const int indexTable,INFO_PHYSICS* infoPhysicsOut);

}


#endif // !PLUGIN_HELPER
