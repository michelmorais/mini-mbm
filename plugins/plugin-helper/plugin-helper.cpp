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


#include <core_mbm/log-util.h>
#include <core_mbm/util-interface.h>
#include <core_mbm/renderizable.h>
#include <core_mbm/dynamic-var.h>
#include "plugin-helper.h"

namespace mbm
{
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

    void lua_print_line(lua_State *lua, TYPE_LOG type_log, const char *format, ...)
	{
		va_list va_args;
		va_start(va_args, format);
		const auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, va_args));
		va_end(va_args);
		va_start(va_args, format);
		char * buffer = log_util::formatNewMessage(length, format, va_args);
		va_end(va_args);
		lua_Debug ar;
		memset(&ar, 0, sizeof(lua_Debug));
		if (lua_getstack(lua, 1, &ar))
		{
			if (lua_getinfo(lua, "nSl", &ar))
			{
				switch(type_log)
				{
					case TYPE_LOG_ERROR:
					{
						ERROR_LOG("File[%s] line[%d]\n%s", log_util::basename(ar.short_src), ar.currentline,buffer);
					};
					break;
					case TYPE_LOG_INFO:
					{
						INFO_LOG("File[%s] line[%d]\n%s", log_util::basename(ar.short_src), ar.currentline,buffer);
					};
					break;
					case TYPE_LOG_WARN:
					{
						WARN_LOG("File[%s] line[%d]\n%s", log_util::basename(ar.short_src), ar.currentline,buffer);
					};
					break;
				}
			}
			else
			{
				ERROR_AT(__LINE__,__FILE__,"Could not get the line and file\n%s",buffer);
			}
		}
		else
		{
			ERROR_AT(__LINE__,__FILE__,"Could not get stack from LUA\n%s",buffer);
		}
		delete [] buffer;
	}

    int lua_error_debug(lua_State *lua,  const char *format, ...)
	{
		va_list va_args;
		va_start(va_args, format);
		const auto length = static_cast<size_t>(vsnprintf(nullptr, 0, format, va_args));
		va_end(va_args);
		va_start(va_args, format);
		char * buffer = log_util::formatNewMessage(length, format, va_args);
		va_end(va_args);
		lua_Debug ar;
		memset(&ar, 0, sizeof(lua_Debug));
		if (lua_getstack(lua, 1, &ar))
		{
			if (lua_getinfo(lua, "nSl", &ar))
			{
				std::string buffer_2(buffer);
				delete [] buffer;
				return luaL_error(lua,"File[%s] line[%d]\n%s", log_util::basename(ar.short_src), ar.currentline,buffer_2.c_str());
			}
			else
			{
				ERROR_AT(__LINE__,__FILE__,"Could not get the line and file");
			}
		}
		else
		{
			ERROR_AT(__LINE__,__FILE__,"Could not get stack from LUA");
		}
		std::string other_buffer(buffer);
		ERROR_LOG("%s", buffer);
		delete [] buffer;
		return luaL_error(lua,"%s",other_buffer.c_str());
	}

    inline const char* getTypeMetaTableNameUserData(lua_State *lua,L_USER_TYPE* foundType)
    {
        lua_rawgeti(lua,-1, 1);
        const int p  = lua_tointeger(lua,-1);
        lua_pop(lua, 1);
        *foundType = (L_USER_TYPE)p;
        return getUserTypeAsString(p);
    }


    inline void *lua_check_MT_userData (lua_State *lua, int ud,L_USER_TYPE* foundType) 
    {
        void *p = lua_touserdata(lua, ud);
        if (p != nullptr) 
        {  /* value is a userdata? */
            if (lua_getmetatable(lua, ud)) 
            {  /* does it have a metatable? */
                const char * valid_for = getTypeMetaTableNameUserData(lua,foundType);
                luaL_getmetatable(lua, valid_for);  /* get correct metatable */
                if (!lua_rawequal(lua, -1, -2))  /* not the same? */
                    p = nullptr;  /* value is a userdata with wrong metatable */
                lua_pop(lua, 2);  /* remove both metatables */
                return p;
            }
        }
        return nullptr;  /* value is not a userdata with a metatable */
    }

    void *lua_check_userType (  lua_State *lua,
                                const int rawi, 
                                const int indexTable,
                                const mbm::L_USER_TYPE expectedType) 
    {
        mbm::L_USER_TYPE foundType = mbm::L_USER_TYPE_END;
        const int typeObj = lua_type(lua, indexTable);
        if (typeObj != LUA_TTABLE)
        {
            if(typeObj == LUA_TNONE)
                lua_error_debug(lua, "expected: [%s]. got [nil]",getUserTypeAsString(expectedType));
            else
                lua_error_debug(lua, "expected: [%s]. got [%s]",getUserTypeAsString(expectedType),lua_typename(lua, typeObj));
        }
        lua_rawgeti(lua, indexTable, rawi);

        void * user_type = lua_check_MT_userData(lua,-1,&foundType);
        if(user_type == nullptr)
        {
            if(foundType >  mbm::L_USER_TYPE_BEGIN && foundType < mbm::L_USER_TYPE_END)
                lua_error_debug(lua, "expected [%s]. got [%s]",getUserTypeAsString(expectedType),getUserTypeAsString(foundType));
            else
                lua_error_debug(lua, "expected [%s]. got [%s]",getUserTypeAsString(expectedType),lua_typename(lua, typeObj));
        }
        lua_pop(lua, 1);//remove userdata from stack
        if(foundType != expectedType)
        {
            if(expectedType == mbm::L_USER_TYPE_RENDERIZABLE)
            {
                if(mbm::isRenderizableType(foundType))//cast from specific to base class renderizable is ok
                    return user_type;
                lua_error_debug(lua, "expected [%s]. got [%s]",getUserTypeAsString(expectedType),getUserTypeAsString(foundType));
                return nullptr;
            }
            else if(expectedType == mbm::L_USER_TYPE_VEC2 && foundType == mbm::L_USER_TYPE_VEC3)//cast from vec3 to vec2 is ok
            {
                return user_type;
            }
            else if((expectedType == mbm::L_USER_TYPE_VEC2 || expectedType == mbm::L_USER_TYPE_VEC3) && mbm::isRenderizableType(foundType))//get position from renderizable to vec3 or vec2 is ok
            {
                auto **ud = static_cast<mbm::RENDERIZABLE **>(user_type);
                auto *ptr = static_cast<mbm::RENDERIZABLE *>(*ud); //-V522
                static mbm::VEC3 * p;
                p = &(ptr->position);
                return &p;
            }
            else 
            {
                lua_error_debug(lua, "expected [%s]. got [%s]",getUserTypeAsString(expectedType),getUserTypeAsString(foundType));
                return nullptr;
            }
        }
        return user_type;
    }
}

namespace plugin_helper
{

	using namespace mbm;

	

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

    void getFieldSignedShortFromTable(lua_State *lua, const int indexTable, const char *fieldName, short int *ptrRet)
    {
        lua_getfield(lua, indexTable, fieldName);
        if (lua_type(lua, -1) == LUA_TNUMBER)
            *ptrRet = (short int)lua_tointeger(lua, -1);
        lua_pop(lua, 1);
    }

    void getFieldIntegerFromTable(lua_State *lua, const int indexTable, const char *fieldName,int *ptrRet)
    {
        lua_getfield(lua, indexTable, fieldName);
        if (lua_type(lua, -1) == LUA_TNUMBER)
            *ptrRet = (unsigned short int)lua_tointeger(lua, -1);
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

    inline const char* getTypeMetaTableNameUserData(lua_State *lua, mbm::L_USER_TYPE* foundType)
    {
        lua_rawgeti(lua,-1, 1);
        const int p  = lua_tointeger(lua,-1);
        lua_pop(lua, 1);
        *foundType = (mbm::L_USER_TYPE)p;
        return mbm::getUserTypeAsString(p);
    }

    

    

    void *lua_get_userType_no_throw (  lua_State *lua,
                                const int rawi, 
                                const int indexTable,
                                const mbm::L_USER_TYPE expectedType)
    {
        mbm::L_USER_TYPE foundType = mbm::L_USER_TYPE_END;
        const int typeObj = lua_type(lua, indexTable);
        if (typeObj != LUA_TTABLE)
        {
            return nullptr;
        }
        lua_rawgeti(lua, indexTable, rawi);

        void * user_type = lua_check_MT_userData(lua,-1,&foundType);
        lua_pop(lua, 1);//remove userdata from stack
        if(user_type == nullptr)
        {
            return nullptr;
        }
        if(foundType != expectedType)
        {
            if(expectedType == mbm::L_USER_TYPE_RENDERIZABLE)
            {
                if(mbm::isRenderizableType(foundType))//cast from specific to base class renderizable is ok
                    return user_type;
                return nullptr;
            }
            else if(expectedType == mbm::L_USER_TYPE_VEC2 && foundType == mbm::L_USER_TYPE_VEC3)//cast from vec3 to vec2 is ok
            {
                return user_type;
            }
            else if((expectedType == mbm::L_USER_TYPE_VEC2 || expectedType == mbm::L_USER_TYPE_VEC3) && mbm::isRenderizableType(foundType))//get position from renderizable to vec3 or vec2 is ok
            {
                auto **ud = static_cast<mbm::RENDERIZABLE **>(user_type);
                auto *ptr = static_cast<mbm::RENDERIZABLE *>(*ud); //-V522
                static mbm::VEC3 * p;
                p = &(ptr->position);
                return &p;
            }
            else 
            {
                return nullptr;
            }
        }
        return user_type;
    }

    mbm::RENDERIZABLE * getRenderizableFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<mbm::RENDERIZABLE **>(lua_check_userType(lua,rawi,indexTable,mbm::L_USER_TYPE_RENDERIZABLE));
        return *ud;        
    }

    
    mbm::RENDERIZABLE * getRenderizableNoThrowFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<mbm::RENDERIZABLE **>(plugin_helper::lua_get_userType_no_throw(lua,rawi,indexTable,mbm::L_USER_TYPE_RENDERIZABLE));
        if(ud == nullptr)
            return nullptr;
        return *ud;        
    }

    void lua_create_metatable_identifier(lua_State *lua,const char* _metatable_plugin,const int value)
    {
        luaL_newmetatable(lua, _metatable_plugin);
        lua_pushinteger(lua,value);
        lua_rawseti(lua,-2,1);
    }
}
