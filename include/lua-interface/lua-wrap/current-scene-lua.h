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

#ifndef CURRENT_SCENE_LUA_H
#define CURRENT_SCENE_LUA_H

#include <map>
#include <string>
#include <vector>
#include <core_mbm/primitives.h>

struct lua_State;

namespace mbm
{
    class RENDERIZABLE;
	class ANIMATION_MANAGER;
	class ANIMATION;
	class FX;
	class LINE_MESH;
    struct DYNAMIC_VAR;
    struct INFO_PHYSICS;
    enum TYPE_CLASS : char;

    void printStack(lua_State *lua, const char *fileName, const uint32_t numLine);
    void getArrayFromTablePixels(lua_State *lua, const int index, uint8_t *lsArrayOut, const uint32_t sizeBuffer);
    void getArrayFromTable(lua_State *lua, const int index, float *lsArrayOut, const uint32_t sizeBuffer);
    std::vector<VEC3> getArrayXYZ_noZ_FromTable(lua_State *lua, const int index);
    std::vector<VEC3> getArrayXYZ_FromTable(lua_State *lua, const int index);
    void getArrayFromTable(lua_State *lua, const int index, uint16_t *lsArrayOut, const uint32_t sizeBuffer);
	void getArrayFromTableWithField(lua_State *lua, const int index, float *lsArrayOut, const uint32_t sizeArray,const char * field_a,const char * field_b);
	void getArrayFromTableWithField(lua_State *lua, const int index, float *lsArrayOut, const uint32_t sizeArray,const char * field_a,const char * field_b,const char * field_c);
    RENDERIZABLE *getRenderizableFromRawTable(lua_State *lua, const int rawi, const int indexTable);
	ANIMATION_MANAGER *getAnimationManagerFromRawTable(lua_State *lua, const int rawi, const int indexTable);
	ANIMATION_MANAGER *getSafeAnimationManagerFromRenderizable(lua_State *lua,RENDERIZABLE * renderizable);
	FX *getFxFromRawTable(lua_State *lua, const int rawi, const int indexTable);
	FX *getSafeFxFromRenderizable(lua_State *lua, RENDERIZABLE * renderizable);
	ANIMATION *getSafeAnimFromRenderizable(lua_State *lua, RENDERIZABLE * renderizable);
    int getVariable(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what);
    int getVariable(lua_State *lua, RENDERIZABLE *ptr, const char *what);
    void getFieldPrimaryFromTable(lua_State *lua, const int indexTable, const char *fieldName, const int LUA_TYPE,void *ptrRet);
    void getFieldUnsignedShortFromTable(lua_State *lua, const int indexTable, const char *fieldName,uint16_t *ptrRet);
    void getFieldUnsignedFromTable(lua_State *lua, const int indexTable, const char *fieldName,uint32_t *ptrRet);
    void getFieldUnsigned8FromTable(lua_State *lua, const int indexTable, const char *fieldName,uint8_t *ptrRet);
    void getFieldSignedShortFromTable(lua_State *lua, const int indexTable, const char *fieldName, int16_t *ptrRet);
    void getFloat2FieldTableFromTable(lua_State *lua, const int indexTable, const char *fieldNameTable,const char *fieldName1, const char *fieldName2, float *out1, float *out2);
	void pushVectorArrayToTableWithField(lua_State * lua, const std::vector<float> & vec, const char* field_a, const char* field_b);
	void pushVectorArrayToTableWithField(lua_State * lua, const std::vector<float> & vec, const char* field_a, const char* field_b, const char* field_c);
    void push_uint16_arrayFromTable(lua_State *lua, const uint16_t * lsArrayIn, const unsigned int sizeBuffer,const bool one_based);
    void unrefTableByIdTableRef(DYNAMIC_VAR *dyVar, lua_State *lua);
    void setDynamicVar(const char *nameVar, DYNAMIC_VAR *    nDvar,std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar);
    int setVariable(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what);
    int setVariable(lua_State *lua, RENDERIZABLE *ptr, const char *what);
    const char *getRandomNameTexture();
    int verifyDynamicCast(lua_State *lua, void *ptr, int line, const char *__file);
    int errorLuaPushFalse(lua_State *lua,const char* msg);
    API_IMPL int onSetPhysicsFromTableLua(lua_State *lua,INFO_PHYSICS* infoPhysics,LINE_MESH * lineMesh);
    API_IMPL int onSetPhysicsFromTableLua(lua_State *lua,const int indexTable,INFO_PHYSICS* infoPhysicsOut);
	const uint32_t get_mode_draw_from_string(const char* str_mode_draw,const uint32_t default_mode_draw_ret);
	const char * get_mode_draw_from_uint(const uint32_t mode_draw,const char * default_mode_draw_ret);

	const uint32_t get_mode_cull_face_from_string(const char* str_mode_cull_face,const uint32_t default_mode_cull_face_ret);
	const char * get_mode_cull_face_from_uint(const uint32_t mode_cull_face,const char * default_mode_cull_face_ret);

	const uint32_t get_mode_front_face_direction_from_string(const char* str_mode_front_face_direction,const uint32_t default_mode_front_face_direction_ret);
	const char * get_mode_front_face_direction_from_uint(const uint32_t mode_front_face_direction,const char * default_mode_front_face_direction_ret);
};

#endif