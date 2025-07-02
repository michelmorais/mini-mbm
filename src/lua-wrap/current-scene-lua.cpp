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

#include <core_mbm/class-identifier.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/device.h>
#include <core_mbm/physics.h>
#include <core_mbm/dynamic-var.h>
#include <core_mbm/log-util.h>
#include <core_mbm/util-interface.h>
#include <render/line-mesh.h>
#include <lua-wrap/current-scene-lua.h>
#include <lua-wrap/check-user-type-lua.h>

namespace mbm
{
    extern int lua_error_debug(lua_State *lua, const char *format, ...);
    extern void lua_print_line(lua_State *lua, TYPE_LOG type_log, const char *format, ...);

    void printStack(lua_State *lua, const char *fileName, const unsigned int numLine)
    {
        std::string stack("\n**********************************"
                          "\nState of stack at\n");
        int top = lua_gettop(lua);
        for (int i = 1, k = top; i <= top; i++, --k)
        {
            char str[255];
            int  type = lua_type(lua, i);
            sprintf(str, "\t%d| %8s |%d\n", -k, lua_typename(lua, type), i);
            stack += str;
        }
        stack += "**********************************\n\n";
        ERROR_AT(numLine, fileName, stack.c_str());
    }

#ifndef DebugLuaStack 
    #define DebugLuaStack printStack(lua,__FILE__,__LINE__);
#endif 

    static const char * informationSetPhysics =  "\nExpected table physics's information\nExample:\n"
                "{} unique table or array table as described bellow\n"
                "{[1]={type='cube',    center={x=0,y=0,z=0},half={x=0,y=0,z=0}},[2]={...}} \n"
                "{[1]={type='rect',    width=>0,height=>0 or points={[1]={x,y},[2]={x,y},[3]={x,y},[4]={x,y}} \n"
                "{[1]={type='polyline',points={[1]={x,y},[2]={x,y},[3]={x,y},[4]={x,y},...} \n"
                "{[1]={type='polygon', points={[1]={x,y},[2]={x,y},[3]={x,y},[4]={x,y},...} \n"
                "{[1]={type='circle',  width=0,height=0}\n"
                "{[1]={type='ellipse', width=0,height=0}\n"
                "{[1]={type='triangle',a={x=0,y=0,z=0},b={x=0,y=0,z=0},c={x=0,y=0,z=0}},[2]={...}} \n"
                "{[1]={type='sphere',  center={x=0,y=0,z=0},ray=1},[2]={...}} \n"
                "{[1]={type='complex', a={x=0,y=0,z=0},b={x=0,y=0,z=0},c={x=0,y=0,z=0},d={x=0,y=0,z=0},e={x=0,y=0,z=0},f={x=0,y=0,z=0},g={x=0,y=0,z=0},h={x=0,y=0,z=0}},[2]={...}} \n";

    RENDERIZABLE * getRenderizableFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<RENDERIZABLE **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_RENDERIZABLE));
        return *ud;
    }

    ANIMATION_MANAGER *getAnimationManagerFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<RENDERIZABLE **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_RENDERIZABLE));
        RENDERIZABLE* renderizable = *ud;
        ANIMATION_MANAGER * animManager = renderizable->getAnimationManager();
        if (animManager == nullptr)
            lua_error_debug(lua, "type of class [%d][%s] not implemented for animation_manager!!!", (int)renderizable->typeClass,renderizable->getTypeClassName());
        return animManager;
    }

    ANIMATION_MANAGER *getSafeAnimationManagerFromRenderizable(lua_State *lua,RENDERIZABLE * renderizable)
    {
        ANIMATION_MANAGER * animManager = renderizable->getAnimationManager();
        if (animManager == nullptr)
            lua_error_debug(lua, "type of class [%d][%s] not implemented for animation_manager!!!", (int)renderizable->typeClass,renderizable->getTypeClassName());
        return animManager;
    }

    FX *getFxFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<RENDERIZABLE **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_RENDERIZABLE));
        RENDERIZABLE* renderizable = *ud;
        FX * fx = renderizable->getFx();
        if (fx == nullptr)
            lua_error_debug(lua, "type of class [%d][%s] not implemented for FX!!!", (int)renderizable->typeClass,renderizable->getTypeClassName());
        return fx;
    }

    FX *getSafeFxFromRenderizable(lua_State *lua, RENDERIZABLE * renderizable)
    {
        FX * fx = renderizable->getFx();
        if (fx == nullptr)
            lua_error_debug(lua, "type of class [%d][%s] not implemented for FX!!!", (int)renderizable->typeClass,renderizable->getTypeClassName());
        return fx;
    }

    ANIMATION *getSafeAnimFromRenderizable(lua_State *lua, RENDERIZABLE * renderizable)
    {
        ANIMATION* anim = nullptr;
        ANIMATION_MANAGER * animManager = renderizable->getAnimationManager();
        if (animManager == nullptr)
        {
            lua_error_debug(lua, "type of class [%d][%s] not implemented for ANIMATION_MANAGER!!!", (int)renderizable->typeClass,renderizable->getTypeClassName());
        }
        else
        {
            anim = animManager->getAnimation();
            if (anim == nullptr)
                lua_error_debug(lua, "type of class [%d][%s] not implemented for ANIMATION_MANAGER!!!", (int)renderizable->typeClass,renderizable->getTypeClassName());
        }
        return anim;
    }

    void getArrayFromTablePixels(lua_State *lua, const int index, unsigned char *lsArrayOut, const unsigned int sizeBuffer)
    {
        const size_t rawlen = lua_rawlen(lua,index);
        if(sizeBuffer == rawlen)
        {
            for (size_t i = 1; i <= sizeBuffer; ++i)
            {
                lua_rawgeti(lua,index, i);
                lsArrayOut[i-1]  = (unsigned char)lua_tointeger(lua,-1);
                lua_pop(lua,1);
            }
        }
        else
        {
            lua_error_debug(lua,"Error table has different size as expected:\nexpected[%d] table[%d]\n[%s:%d]\n ",sizeBuffer,rawlen,__FILE__,__LINE__);
        }
    }
    std::vector<VEC3> getArrayXYZ_noZ_FromTable(lua_State *lua, const int index)
    {
        const size_t rawlen = lua_rawlen(lua,index);
        std::vector<VEC3> xyz(rawlen/2);

        for (size_t i = 1, j =0; i <= rawlen; i+=2, ++j)
        {
            lua_rawgeti(lua,index, i);
            lua_rawgeti(lua,index, i+1);
            xyz[j]   = VEC3(lua_tonumber(lua, -2),lua_tonumber(lua, -1),0.0f);
            lua_pop(lua, 2);
        }
        return xyz;
    }

    std::vector<VEC3> getArrayXYZ_FromTable(lua_State *lua, const int index)
    {
        const size_t rawlen = lua_rawlen(lua,index);
        std::vector<VEC3> xyz(rawlen/3);

        for (size_t i = 1, j =0; i <= rawlen; i+=3, ++j)
        {
            lua_rawgeti(lua,index, i);
            lua_rawgeti(lua,index, i+1);
            lua_rawgeti(lua,index, i+2);
            xyz[j]   = VEC3(lua_tonumber(lua, -3),lua_tonumber(lua, -2),lua_tonumber(lua, -1));
            lua_pop(lua, 3);
        }
        return xyz;
    }

    void getArrayFromTable(lua_State *lua, const int index, float *lsArrayOut, const unsigned int sizeBuffer)
    {
        const size_t rawlen = lua_rawlen(lua,index);
        if(sizeBuffer == rawlen)
        {
            for (size_t i = 1; i <= sizeBuffer; ++i)
            {
                lua_rawgeti(lua,index, i);
                lsArrayOut[i-1] = lua_tonumber(lua,-1);
                lua_pop(lua,1);
            }
        }
        else
        {
            lua_error_debug(lua,"Error table has different size as expected:\nexpected[%d] table[%d]\n[%s:%d]\n ",sizeBuffer,rawlen,__FILE__,__LINE__);
        }
    }

    void getArrayFromTableWithField(lua_State *lua, const int index, float *lsArrayOut, const unsigned int sizeArray,const char * field_a,const char * field_b)
    {
        const size_t rawlen = lua_rawlen(lua,index);
        if(sizeArray == (rawlen * 2))
        {
            for (size_t i = 1,j=0; i <= rawlen; i++, j+=2)
            {
                lua_rawgeti(lua,index, i);
                lua_getfield(lua, -1, field_a);
                lua_getfield(lua, -2, field_b);
                lsArrayOut[j]   = lua_tonumber(lua, -2);
                lsArrayOut[j+1] = lua_tonumber(lua, -1);
                lua_pop(lua, 3);
            }
        }
        else
        {
            lua_error_debug(lua,"Error table has different size as expected:\nexpected[%d] table[%d]\n[%s:%d]\n ",sizeArray,rawlen,__FILE__,__LINE__);
        }
    }

    void getArrayFromTableWithField(lua_State *lua, const int index, float *lsArrayOut, const unsigned int sizeArray,const char * field_a,const char * field_b,const char * field_c)
    {
        const size_t rawlen = lua_rawlen(lua,index);
        if(sizeArray == (rawlen * 3))
        {
            for (size_t i = 1,j=0; i <= rawlen; i++, j+=3)
            {
                lua_rawgeti(lua,index, i);
                lua_getfield(lua, -1, field_a);
                lua_getfield(lua, -2, field_b);
                lua_getfield(lua, -3, field_c);
                lsArrayOut[j]   = lua_tonumber(lua, -3);
                lsArrayOut[j+1] = lua_tonumber(lua, -2);
                lsArrayOut[j+2] = lua_tonumber(lua, -1);
                lua_pop(lua, 4);
            }
        }
        else
        {
            lua_error_debug(lua,"Error table has different size as expected:\nexpected[%d] table[%d]\n[%s:%d]\n ",sizeArray,rawlen,__FILE__,__LINE__);
        }
    }

    void getArrayFromTable(lua_State *lua, const int index, unsigned short int *lsArrayOut, const unsigned int sizeBuffer)
    {
        const size_t rawlen = lua_rawlen(lua,index);
        if(sizeBuffer == rawlen)
        {
            for (size_t i = 1; i <= sizeBuffer; ++i)
            {
                lua_rawgeti(lua,index, i);
                lsArrayOut[i-1] = (unsigned short int)lua_tointeger(lua, -1);
                lua_pop(lua,1);
            }
        }
        else
        {
            lua_error_debug(lua,"Error table has different size as expected:\nexpected[%d] table[%d]\n[%s:%d]\n ",sizeBuffer,rawlen,__FILE__,__LINE__);
        }
    }

    int getVariable(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what)
    {
        const char *      strinChar = nullptr;
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
                    lua_pushinteger(lua,static_cast<lua_Integer>(value));
                }
                break;
                case DYNAMIC_VOID: { return lua_error_debug(lua, "variable [%s] type void!", what);}
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
                default: { return lua_error_debug(lua, "variable [%s] unknown!", what);}
            }
        }
        return 1;
    }

    int getVariable(lua_State *lua, RENDERIZABLE *ptr, const char *what)
    {
        return getVariable(lua, ptr->lsDynamicVar, what);
    }

    void getFieldPrimaryFromTable(lua_State *lua, const int indexTable, const char *fieldName, const int LUA_TYPE,void *ptrRet)
    {
        lua_getfield(lua, indexTable, fieldName);
        switch (LUA_TYPE)
        {
            case LUA_TBOOLEAN:
            {
                auto *b = static_cast<bool *>(ptrRet);
                if (lua_type(lua, -1) == LUA_TBOOLEAN)
                    *b = lua_toboolean(lua, -1) ? true : false;
            }
            break;
            case LUA_TNUMBER:
            {
                auto *n = static_cast<float *>(ptrRet);
                if (lua_type(lua, -1) == LUA_TNUMBER)
                    *n = lua_tonumber(lua, -1);
            }
            break;
            case LUA_TSTRING:
            {
                auto *s = static_cast<std::string *>(ptrRet);
                if (lua_type(lua, -1) == LUA_TSTRING)
                    *s = lua_tostring(lua, -1);
            }
            break;
            default: {
            }
        }
        lua_pop(lua, 1);
    }

    void getFieldUnsignedShortFromTable(lua_State *lua, const int indexTable, const char *fieldName,unsigned short int *ptrRet)
    {
        lua_getfield(lua, indexTable, fieldName);
        if (lua_type(lua, -1) == LUA_TNUMBER)
            *ptrRet = (unsigned short int)lua_tointeger(lua, -1);
        lua_pop(lua, 1);
    }

    void getFieldUnsignedFromTable(lua_State *lua, const int indexTable, const char *fieldName,uint32_t *ptrRet)
    {
        lua_getfield(lua, indexTable, fieldName);
        if (lua_type(lua, -1) == LUA_TNUMBER)
            *ptrRet = lua_tointeger(lua, -1);
        lua_pop(lua, 1);
    }

    void getFieldUnsigned8FromTable(lua_State *lua, const int indexTable, const char *fieldName,uint8_t *ptrRet)
    {
        lua_getfield(lua, indexTable, fieldName);
        if (lua_type(lua, -1) == LUA_TNUMBER)
            *ptrRet = (uint8_t)lua_tointeger(lua, -1);
        lua_pop(lua, 1);
    }

    void getFieldSignedShortFromTable(lua_State *lua, const int indexTable, const char *fieldName, short int *ptrRet)
    {
        lua_getfield(lua, indexTable, fieldName);
        if (lua_type(lua, -1) == LUA_TNUMBER)
            *ptrRet = (short int)lua_tointeger(lua, -1);
        lua_pop(lua, 1);
    }

    void getFloat2FieldTableFromTable(lua_State *lua, const int indexTable, const char *fieldNameTable,const char *fieldName1, const char *fieldName2, float *out1, float *out2)
    {
        lua_getfield(lua, indexTable, fieldNameTable);
        if (lua_type(lua, -1) == LUA_TTABLE)
        {
            const int top = lua_gettop(lua);
            getFieldPrimaryFromTable(lua, top, fieldName1, LUA_TNUMBER, out1);
            getFieldPrimaryFromTable(lua, top, fieldName2, LUA_TNUMBER, out2);
        }
        lua_pop(lua, 1);
    }

    void push_uint16_arrayFromTable(lua_State *lua, const uint16_t * lsArrayIn, const unsigned int sizeBuffer,const bool one_based)
    {
        lua_newtable(lua);
        if(one_based)
        {
            for(unsigned int i=0; i < sizeBuffer; ++i )
            {
                lua_pushinteger(lua,lsArrayIn[i] + 1);
                lua_rawseti(lua, -2, i+1);
            }
        }
        else
        {
            for(unsigned int i=0; i < sizeBuffer; ++i )
            {
                lua_pushinteger(lua,lsArrayIn[i]);
                lua_rawseti(lua, -2, i+1);
            }
        }
    }

    void pushVectorArrayToTableWithField(lua_State * lua, const std::vector<float> & vec, const char* field_a, const char* field_b)
    {
        lua_newtable(lua);
        const unsigned int s = vec.size();
        const float * pData  = vec.data();
        for(unsigned int i=0,j = 1; i < s; i += 2, ++j)
        {
            lua_newtable(lua);

            lua_pushnumber(lua, pData[i]);
            lua_setfield(lua, -2, field_a);

            lua_pushnumber(lua, pData[i+1]);
            lua_setfield(lua, -2, field_b);

            lua_rawseti(lua, -2, j);
        }
    }

    void pushVectorArrayToTableWithField(lua_State * lua, const std::vector<float> & vec, const char* field_a, const char* field_b, const char* field_c)
    {
        lua_newtable(lua);
        const unsigned int s = vec.size();
        const float * pData  = vec.data();
        for(unsigned int i=0,j = 1; i < s; i += 3, ++j)
        {
            lua_newtable(lua);

            lua_pushnumber(lua, pData[i]);
            lua_setfield(lua, -2, field_a);

            lua_pushnumber(lua, pData[i+1]);
            lua_setfield(lua, -2, field_b);

            lua_pushnumber(lua, pData[i+2]);
            lua_setfield(lua, -2, field_c);

            lua_rawseti(lua, -2, j);
        }
    }

    void unrefTableByIdTableRef(DYNAMIC_VAR *dyVar, lua_State *lua)
    {
        if (dyVar && (dyVar->type == DYNAMIC_TABLE || dyVar->type == DYNAMIC_FUNCTION))
        {
            const int tref = dyVar->getInt();
            if (tref != LUA_NOREF)
            {
                luaL_unref(lua, LUA_REGISTRYINDEX, tref);
                dyVar->setInt(LUA_NOREF);
            }
        }
    }

    void setDynamicVar(const char *nameVar, DYNAMIC_VAR *  nDvar,std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar)
    {
        DYNAMIC_VAR *oldVar = lsDynamicVar[nameVar];
        if (oldVar)
            delete oldVar;
        oldVar                = nullptr;
        lsDynamicVar[nameVar] = nDvar;
    }

    int setVariable(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what)
    {
        const int         top   = lua_gettop(lua);
        const int         type  = lua_type(lua, top);
        DYNAMIC_VAR *dyVar = lsDynamicVar[what];
        switch (type)
        {
            case LUA_TNIL:
            {
                unrefTableByIdTableRef(dyVar, lua);
                setDynamicVar(what, nullptr, lsDynamicVar);
            }
            break;
            case LUA_TNUMBER:
            {
                float var = lua_tonumber(lua, top);
                unrefTableByIdTableRef(dyVar, lua);
                if (dyVar == nullptr || dyVar->type != DYNAMIC_FLOAT)
                {
                    setDynamicVar(what, nullptr, lsDynamicVar);
                    dyVar              = new DYNAMIC_VAR(DYNAMIC_FLOAT, &var);
                    lsDynamicVar[what] = dyVar;
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
                    setDynamicVar(what, nullptr, lsDynamicVar);
                    dyVar              = new DYNAMIC_VAR(DYNAMIC_BOOL, &var);
                    lsDynamicVar[what] = dyVar;
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
                    setDynamicVar(what, nullptr, lsDynamicVar);
                    dyVar              = new DYNAMIC_VAR(DYNAMIC_CSTRING,var);
                    lsDynamicVar[what] = dyVar;
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
                    setDynamicVar(what, nullptr, lsDynamicVar);
                    dyVar              = new DYNAMIC_VAR(DYNAMIC_TABLE,&tref);
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
                    setDynamicVar(what, nullptr, lsDynamicVar);
                    dyVar              = new DYNAMIC_VAR(DYNAMIC_FUNCTION,&tref);
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

    int setVariable(lua_State *lua, RENDERIZABLE *ptr, const char *what)
    {
        return setVariable(lua, ptr->lsDynamicVar, what);
    }

    const char *getRandomNameTexture()
    {
        static char seqTextureName[255] = "";
        static int  n                   = 1;
        sprintf(seqTextureName, "__Texture_%d.png", n++);
        return seqTextureName;
    }

    int verifyDynamicCast(lua_State *lua, void *ptr, int line, const char *__file)
    {
        if (ptr == nullptr)
        {
            return lua_error_debug(lua, "error on casting [%s] [%d]", __file, line);
        }
        return 1;
    }

    int errorLuaPushFalse(lua_State *,const char* msg)
    {
        if(msg)
            INFO_LOG(msg);
        return 1;
    }

    static VEC2 getPositionFromTableLuaToLine(lua_State *lua,const int indexTable)
    {
        VEC2 pos;
        getFieldPrimaryFromTable(lua, indexTable, "x", LUA_TNUMBER, &pos.x);
        getFieldPrimaryFromTable(lua, indexTable, "y", LUA_TNUMBER, &pos.y);
        return pos;
    }

    static int setPhysicsFromRenderizable(lua_State *lua, INFO_PHYSICS* infoPhysics, const int indexTable, LINE_MESH* lineMesh)
    {
        if (lua_isuserdata(lua, indexTable) && indexTable > 2)
        {
            lua_settop(lua,indexTable-1);//remove userdata from stack
            const int rawi = 1;
            auto** pptr = static_cast<RENDERIZABLE**>(lua_check_userType(lua,rawi,indexTable-1,L_USER_TYPE_RENDERIZABLE));
            if (pptr)
            {
                auto* ptr = static_cast<RENDERIZABLE*>(*pptr);
                if (ptr == nullptr)
                {
                    return verifyDynamicCast(lua,ptr,__LINE__,__FILE__);
                }
                const auto pInfoPhysics = ptr->getInfoPhysics();
                if(pInfoPhysics == nullptr)
                {
                    return lua_error_debug(lua, "Unexpected object [%s] has no physics table",ptr->getTypeClassName());
                }
                for (auto pCube : pInfoPhysics->lsCube)
                {
                    auto * cube = new CUBE(pCube->halfDim,pCube->absCenter);
                    infoPhysics->lsCube.push_back(cube);
                }

                for (auto pComplex : pInfoPhysics->lsCubeComplex)
                {
                    auto * complex = new CUBE_COMPLEX();
                    for (unsigned int j = 0; j < 8; ++j)
                    {
                        complex->p[j] = pComplex->p[j];
                    }
                    infoPhysics->lsCubeComplex.push_back(complex);
                }

                for (auto pSphere : pInfoPhysics->lsSphere)
                {
                    auto * sphere = new SPHERE();
                    for (unsigned int j = 0; j < 3; ++j)
                    {
                        sphere->absCenter[j] = pSphere->absCenter[j];
                    }
                    sphere->ray = pSphere->ray;
                    infoPhysics->lsSphere.push_back(sphere);
                }

                for (auto pTriangle : pInfoPhysics->lsTriangle)
                {
                    auto * triangle = new TRIANGLE();
                    for (unsigned int j = 0; j < 3; ++j)
                    {
                        triangle->point[j] = pTriangle->point[j];
                    }
                    infoPhysics->lsTriangle.push_back(triangle);
                }
                if (lineMesh)
                {
                    lineMesh->position = ptr->position;

                    for (auto cube : pInfoPhysics->lsCube)
                    {
                        std::vector<VEC3> vertex(5);
                        vertex[0].x = -cube->halfDim.x + cube->absCenter.x;
                        vertex[0].y = -cube->halfDim.y + cube->absCenter.y;
                        vertex[1].x = -cube->halfDim.x + cube->absCenter.x;
                        vertex[1].y =  cube->halfDim.y + cube->absCenter.y; //-V525
                        vertex[2].x =  cube->halfDim.x + cube->absCenter.x;
                        vertex[2].y =  cube->halfDim.y + cube->absCenter.y;
                        vertex[3].x =  cube->halfDim.x + cube->absCenter.x;
                        vertex[3].y = -cube->halfDim.y + cube->absCenter.y;
                        vertex[4].x = -cube->halfDim.x + cube->absCenter.x;
                        vertex[4].y = -cube->halfDim.y + cube->absCenter.y;
                        lineMesh->add(std::move(vertex));
                    }

                    for (auto complex : pInfoPhysics->lsCubeComplex)
                    {
                        std::vector<VEC3> box(16);
                        box[1 -1]  = complex->a; // --a 1
                        box[2 -1]  = complex->b; // --b 2
                        box[3 -1]  = complex->a; // --c 3 
                        box[4 -1]  = complex->d; // --d 4
                        box[5 -1]  = complex->a; // --a 1
                        box[6 -1]  = complex->e; // --e 5
                        box[7 -1]  = complex->f; // --f 6
                        box[8 -1]  = complex->b; // --b 2
                        box[9 -1]  = complex->f; // --f 6
                        box[10-1]  = complex->g; // --g 7
                        box[11-1]  = complex->h; // --h 8
                        box[12-1]  = complex->d; // --d 4
                        box[13-1]  = complex->c; // --c 3 
                        box[14-1]  = complex->g; // --g 7
                        box[15-1]  = complex->h; // --h 8
                        box[16-1]  = complex->e; // --e 5
                        lineMesh->add(std::move(box));
                    }

                    for (auto sphere : pInfoPhysics->lsSphere)
                    {
                        std::vector<VEC3> circleLine(361);
                        for (unsigned int i = 0; i < 361; i++)
                        {
                            circleLine[i].x = (sinf(util::degreeToRadian(static_cast<float>(i))) * sphere->ray)  + sphere->absCenter[0];
                            circleLine[i].y = (cosf(util::degreeToRadian(static_cast<float>(i))) * sphere->ray)  + sphere->absCenter[1];
                        }
                        lineMesh->add(std::move(circleLine));
                    }

                    for (auto triangle : pInfoPhysics->lsTriangle)
                    {
                        std::vector<VEC3> vertex(4);
                        vertex[0].x = triangle->point[0].x;
                        vertex[0].y = triangle->point[0].y;
                        vertex[1].x = triangle->point[1].x;
                        vertex[1].y = triangle->point[1].y;
                        vertex[2].x = triangle->point[2].x;
                        vertex[2].y = triangle->point[2].y;
                        vertex[3].x = triangle->point[0].x;
                        vertex[3].y = triangle->point[0].y;
                        lineMesh->add(std::move(vertex));
                    }
                }
            }
        }
        return 0;
    }

    static int setPhysicsFromTablePureLua(lua_State *lua, INFO_PHYSICS* infoPhysics,const int indexTable,LINE_MESH* lineMesh,const bool singleObj)
    {
        std::string strType;
        const int indexSubTable     = lua_gettop(lua)+1;
        const int iindexNextSubTable= indexSubTable+1;
        if(lua_istable(lua,indexTable))
        {
            getFieldPrimaryFromTable(lua, indexTable, "type", LUA_TSTRING, &strType);
            if (strType.size() > 0)
            {
                const char* myType = strType.c_str();
                if (strcmp(myType, "rectangle") == 0)//from Tile
                {
                    float width = 0;
                    float height = 0;
                    bool hasAngle = false;
                    lua_getfield(lua,indexTable,"width");
                    if(lua_isnumber(lua,indexSubTable))
                        width = lua_tonumber(lua,indexSubTable);
                    lua_pop(lua, 1);
                    lua_getfield(lua,indexTable,"height");
                    if(lua_isnumber(lua,indexSubTable))
                        height = lua_tonumber(lua,indexSubTable);
                    lua_pop(lua, 1);
                    lua_getfield(lua,indexTable,"radian");
                    if(lua_isnumber(lua,indexSubTable))
                        hasAngle = true;
                    lua_pop(lua, 1);
                    if ((width <= 0.0f && height <= 0.0f) || hasAngle)//rotated
                    {
                        lua_getfield(lua,indexTable,"points");
                        if (lua_istable(lua, indexSubTable))
                        {
                            VEC2 points[4];
                            const int len = lua_rawlen(lua,indexSubTable);
                            for (int j = 1; j <= len; ++j)
                            {
                                lua_rawgeti(lua,indexSubTable,j);
                                if (lua_istable(lua, iindexNextSubTable))
                                {
                                    getFieldPrimaryFromTable(lua, iindexNextSubTable, "x", LUA_TNUMBER, &points[j-1].x);
                                    getFieldPrimaryFromTable(lua, iindexNextSubTable, "y", LUA_TNUMBER, &points[j-1].y);
                                }
                                lua_pop(lua,1);
                            }
                            auto* triangleA = new TRIANGLE();
                            auto* triangleB = new TRIANGLE();
                            triangleA->point[0].x = points[0].x;
                            triangleA->point[0].y = points[0].y;
                            triangleA->point[1].x = points[1].x;
                            triangleA->point[1].y = points[1].y;
                            triangleA->point[2].x = points[2].x;
                            triangleA->point[2].y = points[2].y;

                            triangleB->point[0].x = points[2].x;
                            triangleB->point[0].y = points[2].y;
                            triangleB->point[1].x = points[3].x;
                            triangleB->point[1].y = points[3].y;
                            triangleB->point[2].x = points[0].x;
                            triangleB->point[2].y = points[0].y;

                            infoPhysics->lsTriangle.push_back(triangleA);
                            infoPhysics->lsTriangle.push_back(triangleB);

                            if (lineMesh)
                            {
                                std::vector<VEC3> vertex_1(4);
                                const VEC2 pos = getPositionFromTableLuaToLine(lua,indexTable);

                                auto moveIfNotSingle = [&pos](std::vector<VEC3> & wich_vertex,const bool IsingleObj) -> bool
                                {
                                    if (IsingleObj == false)
                                    {
                                        for (auto & i : wich_vertex)
                                        {
                                            i.x += pos.x;
                                            i.y += pos.y;
                                        }
                                    }
                                    return IsingleObj;
                                };
                                vertex_1[0].x = triangleA->point[0].x;
                                vertex_1[0].y = triangleA->point[0].y;
                                vertex_1[1].x = triangleA->point[1].x;
                                vertex_1[1].y = triangleA->point[1].y;
                                vertex_1[2].x = triangleA->point[2].x;
                                vertex_1[2].y = triangleA->point[2].y;
                                vertex_1[3].x = triangleA->point[0].x;
                                vertex_1[3].y = triangleA->point[0].y;

                                moveIfNotSingle(vertex_1,singleObj);

                                lineMesh->add(std::move(vertex_1));

                                std::vector<VEC3> vertex(4);
                                
                                vertex[0].x = triangleB->point[0].x;
                                vertex[0].y = triangleB->point[0].y;
                                vertex[1].x = triangleB->point[1].x;
                                vertex[1].y = triangleB->point[1].y;
                                vertex[2].x = triangleB->point[2].x;
                                vertex[2].y = triangleB->point[2].y;
                                vertex[3].x = triangleB->point[0].x;
                                vertex[3].y = triangleB->point[0].y;

                                if (moveIfNotSingle(vertex,singleObj) == true)
                                {
                                    lineMesh->position.x = pos.x;
                                    lineMesh->position.y = pos.y;
                                }
                                
                                lineMesh->add(std::move(vertex));
                            }
                        }
                        lua_pop(lua,1);
                    }
                    else
                    {
                        auto  cube = new CUBE(width,height,0.0f);
                        infoPhysics->lsCube.push_back(cube);
                        const VEC2 _p(getPositionFromTableLuaToLine(lua,indexTable));
                        VEC3 pos(_p.x,_p.y,0);

                        lua_getfield(lua,indexTable,"center");
                        if(lua_istable(lua,indexSubTable))
                        {
                            getFieldPrimaryFromTable(lua, indexSubTable, "x", LUA_TNUMBER, &pos.x);
                            getFieldPrimaryFromTable(lua, indexSubTable, "y", LUA_TNUMBER, &pos.y);
                            getFieldPrimaryFromTable(lua, indexSubTable, "z", LUA_TNUMBER, &pos.z);
                        }
                        lua_pop(lua, 1);

                        cube->absCenter.x = pos.x;
                        cube->absCenter.y = pos.y;
                        
                        if(lineMesh)
                        {
                            std::vector<VEC3> vertex(5);
                            vertex[0].x = -cube->halfDim.x;
                            vertex[0].y = -cube->halfDim.y;
                            vertex[1].x = -cube->halfDim.x;
                            vertex[1].y =  cube->halfDim.y; //-V525
                            vertex[2].x =  cube->halfDim.x;
                            vertex[2].y =  cube->halfDim.y;
                            vertex[3].x =  cube->halfDim.x;
                            vertex[3].y = -cube->halfDim.y;
                            vertex[4].x = -cube->halfDim.x;
                            vertex[4].y = -cube->halfDim.y;
                            
                            if (singleObj)
                            {
                                lineMesh->position.x = pos.x;
                                lineMesh->position.y = pos.y;
                            }
                            else
                            {
                                vertex[0].x += pos.x;
                                vertex[0].y += pos.y;
                                vertex[1].x += pos.x;
                                vertex[1].y += pos.y;
                                vertex[2].x += pos.x;
                                vertex[2].y += pos.y;
                                vertex[3].x += pos.x;
                                vertex[3].y += pos.y;
                                vertex[4].x += pos.x;
                                vertex[4].y += pos.y; 
                            }
                            lineMesh->add(std::move(vertex));
                        }
                    }
                }
                else if(strcmp(myType,"cube") == 0 || strcmp(myType,"rect") == 0)
                {
                    auto  cube = new CUBE();
                    infoPhysics->lsCube.push_back(cube);
                    lua_getfield(lua,indexTable,"center");
                    if(lua_istable(lua,indexSubTable))
                    {
                        getFieldPrimaryFromTable(lua, indexSubTable, "x", LUA_TNUMBER, &cube->absCenter.x);
                        getFieldPrimaryFromTable(lua, indexSubTable, "y", LUA_TNUMBER, &cube->absCenter.y);
                        getFieldPrimaryFromTable(lua, indexSubTable, "z", LUA_TNUMBER, &cube->absCenter.z);
                        lua_pop(lua, 1);
                    }
                    else
                    {
                        lua_pop(lua, 1);
                        getFieldPrimaryFromTable(lua, indexTable, "x", LUA_TNUMBER, &cube->absCenter.x);
                        getFieldPrimaryFromTable(lua, indexTable, "y", LUA_TNUMBER, &cube->absCenter.y);
                        getFieldPrimaryFromTable(lua, indexTable, "z", LUA_TNUMBER, &cube->absCenter.z);
                    }

                    lua_getfield(lua,indexTable,"half");
                    if(lua_istable(lua,indexSubTable))
                    {
                        getFieldPrimaryFromTable(lua, indexSubTable, "x", LUA_TNUMBER, &cube->halfDim.x);
                        getFieldPrimaryFromTable(lua, indexSubTable, "y", LUA_TNUMBER, &cube->halfDim.y);
                        getFieldPrimaryFromTable(lua, indexSubTable, "z", LUA_TNUMBER, &cube->halfDim.z);
                        if(cube->halfDim.x <= 0.0f)
                            return errorLuaPushFalse(lua,"error: half.x must be > 0.0");
                        if(cube->halfDim.y <= 0.0f)
                            return errorLuaPushFalse(lua,"error: half.y must be > 0.0");
                        if(cube->halfDim.z <  0.0f)
                            return errorLuaPushFalse(lua,"error: half.z must be >= 0.0");
                        lua_pop(lua, 1);
                    }
                    else
                    {
                        lua_pop(lua, 1);
                        getFieldPrimaryFromTable(lua, indexTable, "width", LUA_TNUMBER, &cube->halfDim.x);
                        getFieldPrimaryFromTable(lua, indexTable, "height", LUA_TNUMBER, &cube->halfDim.y);
                        getFieldPrimaryFromTable(lua, indexTable, "depth", LUA_TNUMBER, &cube->halfDim.z);
                        cube->halfDim.x *= 0.5f;
                        cube->halfDim.y *= 0.5f;
                        cube->halfDim.z *= 0.5f;
                    }
                    
                    if(lineMesh)
                    {
                        std::vector<VEC3> vertex(5);
                        if (singleObj)
                        {
                            vertex[0].x = -cube->halfDim.x;
                            vertex[0].y = -cube->halfDim.y;
                            vertex[1].x = -cube->halfDim.x;
                            vertex[1].y =  cube->halfDim.y; //-V525
                            vertex[2].x =  cube->halfDim.x;
                            vertex[2].y =  cube->halfDim.y;
                            vertex[3].x =  cube->halfDim.x;
                            vertex[3].y = -cube->halfDim.y;
                            vertex[4].x = -cube->halfDim.x;
                            vertex[4].y = -cube->halfDim.y;
                            lineMesh->position.x = cube->absCenter.x;
                            lineMesh->position.y = cube->absCenter.y;
                        }
                        else
                        {
                            vertex[0].x = -cube->halfDim.x + cube->absCenter.x;
                            vertex[0].y = -cube->halfDim.y + cube->absCenter.y;
                            vertex[1].x = -cube->halfDim.x + cube->absCenter.x;
                            vertex[1].y =  cube->halfDim.y + cube->absCenter.y; //-V525
                            vertex[2].x =  cube->halfDim.x + cube->absCenter.x;
                            vertex[2].y =  cube->halfDim.y + cube->absCenter.y;
                            vertex[3].x =  cube->halfDim.x + cube->absCenter.x;
                            vertex[3].y = -cube->halfDim.y + cube->absCenter.y;
                            vertex[4].x = -cube->halfDim.x + cube->absCenter.x;
                            vertex[4].y = -cube->halfDim.y + cube->absCenter.y;
                        }
                        lineMesh->add(std::move(vertex));
                    }
                }
                else if(strcmp(myType,"triangle") == 0)
                {
                    auto  triangle = new TRIANGLE();
                    float z = 0;
                    infoPhysics->lsTriangle.push_back(triangle);
                    getFieldPrimaryFromTable(lua, indexTable, "x", LUA_TNUMBER, &triangle->position.x);
                    getFieldPrimaryFromTable(lua, indexTable, "y", LUA_TNUMBER, &triangle->position.y);

                    lua_getfield(lua,indexTable,"center");
                    if(lua_istable(lua,indexSubTable))
                    {
                        getFieldPrimaryFromTable(lua, indexSubTable, "x", LUA_TNUMBER, &triangle->position.x);
                        getFieldPrimaryFromTable(lua, indexSubTable, "y", LUA_TNUMBER, &triangle->position.y);
                        getFieldPrimaryFromTable(lua, indexSubTable, "z", LUA_TNUMBER, &z);
                    }
                    lua_pop(lua, 1);

                    for(int j=0; j< 3; ++j)
                    {
                        const char l = 'a' + static_cast<char>(j);
                        const char letter[2] = {l,0};
                        lua_getfield(lua,indexTable,letter);
                        if(lua_istable(lua,indexSubTable))
                        {
                            getFieldPrimaryFromTable(lua, indexSubTable, "x", LUA_TNUMBER, &triangle->point[j].x);
                            getFieldPrimaryFromTable(lua, indexSubTable, "y", LUA_TNUMBER, &triangle->point[j].y);
                            getFieldPrimaryFromTable(lua, indexSubTable, "z", LUA_TNUMBER, &triangle->point[j].z);
                        }
                        else
                        {
                            lua_pop(lua, 1);
                            int len = lua_rawlen(lua,indexSubTable);
                            if(len == 0)
                            {
                                lua_getfield(lua,indexTable,"points");
                                if (lua_istable(lua, indexSubTable))
                                    len = lua_rawlen(lua,indexSubTable);
                                else
                                    break;
                            }
                            if(len > 0)
                            {
                                int indexNoPoint = 0;
                                VEC3 points[3];
                                lua_rawgeti(lua,indexSubTable,1);
                                if (lua_istable(lua, iindexNextSubTable))
                                {
                                    getFieldPrimaryFromTable(lua, iindexNextSubTable, "x", LUA_TNUMBER, &points[0].x);
                                    getFieldPrimaryFromTable(lua, iindexNextSubTable, "y", LUA_TNUMBER, &points[0].y);
                                    getFieldPrimaryFromTable(lua, iindexNextSubTable, "z", LUA_TNUMBER, &points[0].z);
                                }
                                lua_pop(lua,1);

                                if(len >= 2)
                                {
                                    lua_rawgeti(lua,indexSubTable,2);
                                    if (lua_istable(lua, iindexNextSubTable))
                                    {
                                        getFieldPrimaryFromTable(lua, iindexNextSubTable, "x", LUA_TNUMBER, &points[1].x);
                                        getFieldPrimaryFromTable(lua, iindexNextSubTable, "y", LUA_TNUMBER, &points[1].y);
                                        getFieldPrimaryFromTable(lua, iindexNextSubTable, "z", LUA_TNUMBER, &points[1].z);
                                    }
                                    lua_pop(lua,1);
                                }
                                else
                                {
                                    points[1].x = points[indexNoPoint].x;
                                    points[1].y = points[indexNoPoint].y;
                                    indexNoPoint = 1;
                                }

                                if(len >= 3)
                                {
                                    lua_rawgeti(lua,indexSubTable,3);
                                    if (lua_istable(lua, iindexNextSubTable))
                                    {
                                        getFieldPrimaryFromTable(lua, iindexNextSubTable, "x", LUA_TNUMBER, &points[2].x);
                                        getFieldPrimaryFromTable(lua, iindexNextSubTable, "y", LUA_TNUMBER, &points[2].y);
                                        getFieldPrimaryFromTable(lua, iindexNextSubTable, "z", LUA_TNUMBER, &points[2].z);
                                    }
                                    lua_pop(lua,1);
                                }
                                else
                                {
                                    points[2].x = points[indexNoPoint].x;
                                    points[2].y = points[indexNoPoint].y;
                                }
                                triangle->point[0].x = points[0].x;
                                triangle->point[0].y = points[0].y;
                                triangle->point[0].z += z;
                                triangle->point[1].x = points[1].x;
                                triangle->point[1].y = points[1].y;
                                triangle->point[1].z += z;
                                triangle->point[2].x = points[2].x;
                                triangle->point[2].y = points[2].y;
                                triangle->point[2].z += z;
                            }
                            break;
                        }
                        lua_pop(lua, 1);
                    }
                    if (lineMesh)
                    {
                        std::vector<VEC3> vertex(4);
                        vertex[0].x = triangle->point[0].x;
                        vertex[0].y = triangle->point[0].y;
                        vertex[0].z = triangle->point[0].z;

                        vertex[1].x = triangle->point[1].x;
                        vertex[1].y = triangle->point[1].y;
                        vertex[1].z = triangle->point[1].z;

                        vertex[2].x = triangle->point[2].x;
                        vertex[2].y = triangle->point[2].y;
                        vertex[2].z = triangle->point[2].z;
                        
                        vertex[3].x = triangle->point[0].x;
                        vertex[3].y = triangle->point[0].y;
                        vertex[3].z = triangle->point[0].z;
                        lineMesh->add(std::move(vertex));
                    }
                }
                else if (strcmp(myType, "circle") == 0 || strcmp(myType, "ellipse") == 0)//circle and ellipse from tile initially
                {
                    float width = 0;
                    float height = 0;
                    float ray = 0.0f;
                    lua_getfield(lua,indexTable,"width");
                    if(lua_isnumber(lua,indexSubTable))
                        width = lua_tonumber(lua,indexSubTable);
                    lua_pop(lua, 1);

                    lua_getfield(lua,indexTable,"height");
                    if(lua_isnumber(lua,indexSubTable))
                        height = lua_tonumber(lua,indexSubTable);
                    lua_pop(lua, 1);

                    lua_getfield(lua,indexTable,"ray");
                    if(lua_isnumber(lua,indexSubTable))
                        ray = lua_tonumber(lua,indexSubTable);
                    lua_pop(lua, 1);

                    auto  sphere = new SPHERE();
                    if(ray > 0.0f)
                        sphere->ray = ray;
                    else
                        sphere->ray = (width > height ? width : height) * 0.5f;
                    const VEC2 _p(getPositionFromTableLuaToLine(lua,indexTable));
                    VEC3 pos(_p.x,_p.y,0);
                    

                    lua_getfield(lua,indexTable,"center");
                    if(lua_istable(lua,indexSubTable))
                    {
                        getFieldPrimaryFromTable(lua, indexSubTable, "x", LUA_TNUMBER, &pos.x);
                        getFieldPrimaryFromTable(lua, indexSubTable, "y", LUA_TNUMBER, &pos.y);
                        getFieldPrimaryFromTable(lua, indexSubTable, "z", LUA_TNUMBER, &pos.z);
                    }
                    lua_pop(lua, 1);

                    sphere->absCenter[0] = pos.x;
                    sphere->absCenter[1] = pos.y;

                    
                    infoPhysics->lsSphere.push_back(sphere);
                    if (lineMesh)
                    {
                        VEC2 halfDim(ray > 0.0f ? ray * 2 : width,ray > 0.0f ? ray * 2: height);
                        halfDim.x *= 0.5f;
                        halfDim.y *= 0.5f;
                        std::vector<VEC3> circleLine(361);
                        if (singleObj)
                        {
                            for (unsigned int i = 0; i < 361; i++)
                            {
                                circleLine[i].x = sinf(util::degreeToRadian(static_cast<float>(i))) * halfDim.x ;
                                circleLine[i].y = cosf(util::degreeToRadian(static_cast<float>(i))) * halfDim.y ;
                            }
                            lineMesh->position.x = pos.x;
                            lineMesh->position.y = pos.y;
                        }
                        else
                        {
                            for (unsigned int i = 0; i < 361; i++)
                            {
                                circleLine[i].x = sinf(util::degreeToRadian(static_cast<float>(i))) * halfDim.x + pos.x;
                                circleLine[i].y = cosf(util::degreeToRadian(static_cast<float>(i))) * halfDim.y + pos.y;
                            }
                        }
                        
                        lineMesh->add(std::move(circleLine));
                    }
                }
                else if(strcmp(myType,"sphere") == 0)
                {
                    auto  sphere = new SPHERE();
                    infoPhysics->lsSphere.push_back(sphere);
                    lua_getfield(lua,indexTable,"center");
                    if(lua_istable(lua,indexSubTable))
                    {
                        getFieldPrimaryFromTable(lua, indexSubTable, "x", LUA_TNUMBER, &sphere->absCenter[0]);
                        getFieldPrimaryFromTable(lua, indexSubTable, "y", LUA_TNUMBER, &sphere->absCenter[1]);
                        getFieldPrimaryFromTable(lua, indexSubTable, "z", LUA_TNUMBER, &sphere->absCenter[2]);
                        lua_pop(lua, 1);
                    }
                    else
                    {
                        lua_pop(lua, 1);
                        getFieldPrimaryFromTable(lua, indexTable, "x", LUA_TNUMBER, &sphere->absCenter[0]);
                        getFieldPrimaryFromTable(lua, indexTable, "y", LUA_TNUMBER, &sphere->absCenter[1]);
                        getFieldPrimaryFromTable(lua, indexTable, "z", LUA_TNUMBER, &sphere->absCenter[2]);
                    }
                    
                    getFieldPrimaryFromTable(lua, indexTable, "ray", LUA_TNUMBER, &sphere->ray);
                    if(sphere->ray <= 0.0f)
                        return errorLuaPushFalse(lua,"error: ray must be > 0.0");
                    if (lineMesh)
                    {
                        std::vector<VEC3> circleLine(361);
                        if (singleObj)
                        {
                            for (unsigned int i = 0; i < 361; i++)
                            {
                                circleLine[i].x = sinf(util::degreeToRadian(static_cast<float>(i))) * sphere->ray ;
                                circleLine[i].y = cosf(util::degreeToRadian(static_cast<float>(i))) * sphere->ray ;
                            }
                            lineMesh->position.x = sphere->absCenter[0];
                            lineMesh->position.y = sphere->absCenter[1];
                            lineMesh->position.z = sphere->absCenter[2];
                        }
                        else
                        {
                            for (unsigned int i = 0; i < 361; i++)
                            {
                                circleLine[i].x = sinf(util::degreeToRadian(static_cast<float>(i))) * sphere->ray + sphere->absCenter[0];
                                circleLine[i].y = cosf(util::degreeToRadian(static_cast<float>(i))) * sphere->ray + sphere->absCenter[1];
                            }
                        }
                        lineMesh->add(std::move(circleLine));
                    }
                }
                else if(strcmp(myType,"polyline") == 0 || strcmp(myType,"polygon") == 0)//from tile
                {
                    lua_getfield(lua,indexTable,"points");
                    if (lua_istable(lua, indexSubTable))
                    {
                        const int len = lua_rawlen(lua,indexSubTable);
                        for (int j = 1; j <= len; j+=3)
                        {
                            auto* triangle = new TRIANGLE();
                            int indexNoPoint = 0;
                            VEC2 points[3];
                            lua_rawgeti(lua,indexSubTable,j);
                            if (lua_istable(lua, iindexNextSubTable))
                            {
                                getFieldPrimaryFromTable(lua, iindexNextSubTable, "x", LUA_TNUMBER, &points[0].x);
                                getFieldPrimaryFromTable(lua, iindexNextSubTable, "y", LUA_TNUMBER, &points[0].y);
                            }
                            lua_pop(lua,1);

                            if((j+1) <= len)
                            {
                                lua_rawgeti(lua,indexSubTable,j+1);
                                if (lua_istable(lua, iindexNextSubTable))
                                {
                                    getFieldPrimaryFromTable(lua, iindexNextSubTable, "x", LUA_TNUMBER, &points[1].x);
                                    getFieldPrimaryFromTable(lua, iindexNextSubTable, "y", LUA_TNUMBER, &points[1].y);
                                }
                                lua_pop(lua,1);
                            }
                            else
                            {
                                points[1].x = points[indexNoPoint].x;
                                points[1].y = points[indexNoPoint].y;
                                indexNoPoint = 1;
                            }

                            if((j+2) <= len)
                            {
                                lua_rawgeti(lua,indexSubTable,j+2);
                                if (lua_istable(lua, iindexNextSubTable))
                                {
                                    getFieldPrimaryFromTable(lua, iindexNextSubTable, "x", LUA_TNUMBER, &points[2].x);
                                    getFieldPrimaryFromTable(lua, iindexNextSubTable, "y", LUA_TNUMBER, &points[2].y);
                                }
                                lua_pop(lua,1);
                            }
                            else
                            {
                                points[2].x = points[indexNoPoint].x;
                                points[2].y = points[indexNoPoint].y;
                            }

                            triangle->point[0].x = points[0].x;
                            triangle->point[0].y = points[0].y;
                            triangle->point[1].x = points[1].x;
                            triangle->point[1].y = points[1].y;
                            triangle->point[2].x = points[2].x;
                            triangle->point[2].y = points[2].y;

                            infoPhysics->lsTriangle.push_back(triangle);

                            if (lineMesh)
                            {
                                std::vector<VEC3> vertex(4);
                                const VEC2 pos = getPositionFromTableLuaToLine(lua,indexTable);
                                if (singleObj)
                                {
                                    vertex[0].x = triangle->point[0].x;
                                    vertex[0].y = triangle->point[0].y;
                                    vertex[1].x = triangle->point[1].x;
                                    vertex[1].y = triangle->point[1].y;
                                    vertex[2].x = triangle->point[2].x;
                                    vertex[2].y = triangle->point[2].y;
                                    vertex[3].x = triangle->point[0].x;
                                    vertex[3].y = triangle->point[0].y;
                                    lineMesh->position.x = pos.x;
                                    lineMesh->position.y = pos.y;
                                }
                                else
                                {
                                    vertex[0].x = triangle->point[0].x + pos.x;
                                    vertex[0].y = triangle->point[0].y + pos.y;
                                    vertex[1].x = triangle->point[1].x + pos.x;
                                    vertex[1].y = triangle->point[1].y + pos.y;
                                    vertex[2].x = triangle->point[2].x + pos.x;
                                    vertex[2].y = triangle->point[2].y + pos.y;
                                    vertex[3].x = triangle->point[0].x + pos.x;
                                    vertex[3].y = triangle->point[0].y + pos.y;
                                }
                                lineMesh->add(std::move(vertex));
                            }
                        }
                    }
                    lua_pop(lua,1);
                }
                else if(strcmp(myType,"complex") == 0)
                {
                    auto  complex = new CUBE_COMPLEX();
                    infoPhysics->lsCubeComplex.push_back(complex);
                    float xPos = 0;
                    float yPos = 0;
                    float zPos = 0;
                    getFieldPrimaryFromTable(lua, indexTable, "x", LUA_TNUMBER, &xPos);
                    getFieldPrimaryFromTable(lua, indexTable, "y", LUA_TNUMBER, &yPos);
                    getFieldPrimaryFromTable(lua, indexTable, "z", LUA_TNUMBER, &yPos);

                    lua_getfield(lua,indexTable,"center");
                    if(lua_istable(lua,indexSubTable))
                    {
                        getFieldPrimaryFromTable(lua, indexSubTable, "x", LUA_TNUMBER, &xPos);
                        getFieldPrimaryFromTable(lua, indexSubTable, "y", LUA_TNUMBER, &yPos);
                        getFieldPrimaryFromTable(lua, indexSubTable, "z", LUA_TNUMBER, &zPos);
                    }
                    lua_pop(lua, 1);

                    for(int j=0; j< 8; ++j)
                    {
                        const char l = 'a' + static_cast<char>(j);
                        const char letter[2] = {l, 0};
                        lua_getfield(lua,indexTable,letter);
                        if(lua_istable(lua,indexSubTable))
                        {
                            getFieldPrimaryFromTable(lua, indexSubTable, "x", LUA_TNUMBER, &complex->p[j].x);
                            getFieldPrimaryFromTable(lua, indexSubTable, "y", LUA_TNUMBER, &complex->p[j].y);
                            getFieldPrimaryFromTable(lua, indexSubTable, "z", LUA_TNUMBER, &complex->p[j].z);
                            complex->p[j].x += xPos;
                            complex->p[j].y += yPos;
                            complex->p[j].z += zPos;
                        }
                        lua_pop(lua, 1);
                    }
                    if (lineMesh)
                    {
                        std::vector<VEC3> box(16);
                        box[1 -1]  = complex->a; // --a 1
                        box[2 -1]  = complex->b; // --b 2
                        box[3 -1]  = complex->a; // --c 3 
                        box[4 -1]  = complex->d; // --d 4
                        box[5 -1]  = complex->a; // --a 1
                        box[6 -1]  = complex->e; // --e 5
                        box[7 -1]  = complex->f; // --f 6
                        box[8 -1]  = complex->b; // --b 2
                        box[9 -1]  = complex->f; // --f 6
                        box[10-1]  = complex->g; // --g 7
                        box[11-1]  = complex->h; // --h 8
                        box[12-1]  = complex->d; // --d 4
                        box[13-1]  = complex->c; // --c 3 
                        box[14-1]  = complex->g; // --g 7
                        box[15-1]  = complex->h; // --h 8
                        box[16-1]  = complex->e; // --e 5
                        lineMesh->add(std::move(box));
                    }
                }
                else
                {
                    //printStack(lua,__FILE__,__LINE__);
                    return lua_error_debug(lua, "%s\n%s",informationSetPhysics,myType);
                }
            }
            else
            {
                //printStack(lua,__FILE__,__LINE__);
                return lua_error_debug(lua,informationSetPhysics);
            }
        }
        else
        {
            return setPhysicsFromRenderizable(lua, infoPhysics, indexTable, lineMesh);
        }
        return 0;
    }

    int onSetPhysicsFromTableLua(lua_State *lua,INFO_PHYSICS* infoPhysics,LINE_MESH* lineMesh)
    {
        const int top        = lua_gettop(lua);
        const int type       = top > 1 ? lua_type(lua, 2) : LUA_TNIL;
        const int indexTable = top;
        if(type == LUA_TTABLE)
        {
            infoPhysics->release();
            const std::size_t lenTable = lua_rawlen(lua, 2);
            if (lenTable == 0)
            {
                return setPhysicsFromTablePureLua(lua, infoPhysics,indexTable,lineMesh,true);
            }
            for (std::size_t i=0; i<lenTable; ++i)
            {
                lua_rawgeti(lua, indexTable, (i + 1));
                const int res = setPhysicsFromTablePureLua(lua, infoPhysics,top+1,lineMesh,false);
                lua_pop(lua, 1);
                if(res != 0)
                {
                    return res;
                }
            }
        }
        else
        {
            return lua_error_debug(lua, informationSetPhysics);
        }
        return 0;
    }

    int onSetPhysicsFromTableLua(lua_State *lua,const int indexTable,INFO_PHYSICS* infoPhysicsOut)
    {
        const int       type     = lua_type(lua, indexTable);
        const int       top      = lua_gettop(lua);
        if(type == LUA_TTABLE)
        {
            infoPhysicsOut->release();
            const std::size_t lenTable = lua_rawlen(lua, indexTable);
            if (lenTable == 0)
            {
                return setPhysicsFromTablePureLua(lua, infoPhysicsOut,indexTable,nullptr,true);
            }
            for (std::size_t i=0; i<lenTable; ++i)
            {
                lua_rawgeti(lua, indexTable, (i + 1));
                const int res = setPhysicsFromTablePureLua(lua, infoPhysicsOut,top+1,nullptr,false);
                lua_pop(lua, 1);
                if(res != 0)
                {
                    return res;
                }
            }
        }
        else
        {
            return lua_error_debug(lua, informationSetPhysics);
        }
        return 0;
    }

    const unsigned int get_mode_draw_from_string(const char* str_mode_draw,const unsigned int default_mode_draw_ret)
    {
        if(str_mode_draw == nullptr)
            return default_mode_draw_ret;
        if(strcmp(str_mode_draw,"TRIANGLES") == 0 )
            return GL_TRIANGLES;
        if(strcmp(str_mode_draw,"TRIANGLE_STRIP") == 0 )
            return GL_TRIANGLE_STRIP;
        if(strcmp(str_mode_draw,"TRIANGLE_FAN") == 0 )
            return GL_TRIANGLE_FAN;
        if(strcmp(str_mode_draw,"LINES") == 0 )
            return GL_LINES;
        if(strcmp(str_mode_draw,"LINE_LOOP") == 0 )
            return GL_LINE_LOOP;
        if(strcmp(str_mode_draw,"LINE_STRIP") == 0 )
            return GL_LINE_STRIP;
        if(strcmp(str_mode_draw,"POINTS") == 0 )
            return GL_POINTS;
        return default_mode_draw_ret;
    }

    const char * get_mode_draw_from_uint(const unsigned int mode_draw,const char * default_mode_draw_ret)
    {
        switch(mode_draw)
        {
            case GL_POINTS         : return "POINTS";
            case GL_LINES          : return "LINES";
            case GL_LINE_LOOP      : return "LINE_LOOP";
            case GL_LINE_STRIP     : return "LINE_STRIP";
            case GL_TRIANGLES      : return "TRIANGLES";
            case GL_TRIANGLE_STRIP : return "TRIANGLE_STRIP";
            case GL_TRIANGLE_FAN   : return "TRIANGLE_FAN";
            default                : return default_mode_draw_ret;
        }
    }

    const unsigned int get_mode_cull_face_from_string(const char* str_mode_cull_face,const unsigned int default_mode_cull_face_ret)
    {
        if(str_mode_cull_face == nullptr)
            return default_mode_cull_face_ret;
        if(strcmp(str_mode_cull_face,"FRONT") == 0 )
            return GL_FRONT;
        if(strcmp(str_mode_cull_face,"BACK") == 0 )
            return GL_BACK;
        if(strcmp(str_mode_cull_face,"FRONT_AND_BACK") == 0 )
            return GL_FRONT_AND_BACK;
        return default_mode_cull_face_ret;
    }
    const char * get_mode_cull_face_from_uint(const unsigned int mode_cull_face,const char * default_mode_cull_face_ret)
    {
        switch(mode_cull_face)
        {
            case GL_FRONT          : return "FRONT";
            case GL_BACK           : return "BACK";
            case GL_FRONT_AND_BACK : return "FRONT_AND_BACK";
            default                : return default_mode_cull_face_ret;
        }
    }
    const unsigned int get_mode_front_face_direction_from_string(const char* str_mode_front_face_direction,const unsigned int default_mode_front_face_direction_ret)
    {
        if(str_mode_front_face_direction == nullptr)
            return default_mode_front_face_direction_ret;
        if(strcmp(str_mode_front_face_direction,"CW") == 0 )
            return GL_CW;
        if(strcmp(str_mode_front_face_direction,"CCW") == 0 )
            return GL_CCW;
        return default_mode_front_face_direction_ret;
    }
    const char * get_mode_front_face_direction_from_uint(const unsigned int mode_front_face_direction,const char * default_mode_front_face_direction_ret)
    {
        switch(mode_front_face_direction)
        {
            case GL_CW          : return "CW";
            case GL_CCW         : return "CCW";
            default             : return default_mode_front_face_direction_ret;
        }
    }
};
