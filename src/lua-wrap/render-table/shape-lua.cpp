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

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#include <lua-wrap/render-table/shape-lua.h>
#include <lua-wrap/user-data-lua.h>
#include <lua-wrap/common-methods-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <render/shape-mesh.h>
#include <platform/mismatch-platform.h>
#include <core_mbm/header-mesh.h>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif
#include <core_mbm/util-interface.h>
namespace mbm
{
    extern void getArrayFromTable(lua_State *lua, const int index, float *lsArrayOut, const unsigned int sizeBuffer);
    extern void getArrayFromTable(lua_State *lua, const int index, unsigned short int *lsArrayOut, const unsigned int sizeBuffer);
	extern void printStack(lua_State *lua, const char *fileName, const unsigned int numLine);
	extern void pushVectorArrayToTableWithField(lua_State * lua, const std::vector<float> & vec, const char* field_a, const char* field_b);
	extern void pushVectorArrayToTableWithField(lua_State * lua, const std::vector<float> & vec, const char* field_a, const char* field_b, const char* field_c);
    extern void push_uint16_arrayFromTable(lua_State *lua, const uint16_t * lsArrayIn, const unsigned int sizeBuffer,const bool one_based);
	extern void getArrayFromTableWithField(lua_State *lua, const int index, float *lsArrayOut, const unsigned int sizeArray,const char * field_a,const char * field_b);
	extern void getArrayFromTableWithField(lua_State *lua, const int index, float *lsArrayOut, const unsigned int sizeArray,const char * field_a,const char * field_b,const char * field_c);
	extern const unsigned int get_mode_draw_from_string(const char* str_mode_draw,const unsigned int default_mode_draw_ret);
	extern const unsigned int get_mode_cull_face_from_string(const char* str_mode_cull_face,const unsigned int default_mode_cull_face_ret);
	extern const unsigned int get_mode_front_face_direction_from_string(const char* str_mode_front_face_direction,const unsigned int default_mode_front_face_direction_ret);
	extern int lua_error_debug(lua_State *lua, const char *format, ...);


	const char* options_shape = 
				"create (type_of_shape, width, height, dynamic_mode, nick_name) \n"
				"        type_of_shape: 'CIRCLE', 'RECTANGLE', 'TRIANGLE' \n"
				"        ps.: for 'CIRCLE' there are 5 options: \n"
				"              create (type_of_shape = 'CIRCLE', width = >0 , height >0 , total_triangle = 18, dynamic_mode = false, nick_name = 'generated_automatic') \n"
				"             for 'RECTANGLE', 'TRIANGLE' there are 4 options: \n"
				"              create (type_of_shape = 'RECTANGLE' or 'TRIANGLE', width = >0 , height >0 , dynamic_mode = false, nick_name = 'generated_automatic') \n\n"
				
				"create ([table XYZ], [table NX_NY_NZ], [table UV]) <-(is NOT dynamic) \n"
				"createIndexed ([table XYZ], [table index], [table NX_NY_NZ], [table UV]) <-(is NOT dynamic) \n"
				"createDynamicIndexed ([table XYZ], [table index], [table NX_NY_NZ], [table UV]) <-(is dynamic) \n\n"
				" * All of them in case of success return the 'NICK_NAME' *"
		;

    SHAPE_MESH *getShapeMeshFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<SHAPE_MESH **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_SHAPE_MESH));
        return *ud;
    }

    int onDestroyShapeMeshLua(lua_State *lua)
    {
        SHAPE_MESH *          shape    = getShapeMeshFromRawTable(lua, 1, 1);
        auto *userData = static_cast<USER_DATA_SHAPE_LUA *>(shape->userData);
        if (userData)
        {
            userData->unrefAllTableLua(lua);
            delete userData;
        }
        shape->userData = nullptr;
    #if DEBUG_FREE_LUA
        static int  num      = 1;
        const char *fileName = shape->getFileName();
        PRINT_IF_DEBUG("free [%s] [%s] [%d]\n",shape->getTypeClassName(), fileName ? fileName : "NULL", num++);
    #endif
        DEVICE *             device    = DEVICE::getInstance();
        auto *userScene = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        userScene->remove(shape);
        delete shape;
        return 0;
    }

    const char *getRandomNameMesh()
    {
        static char randomName[255] = "";
        static int  n               = 1;
        sprintf(randomName, "__ShapeMesh_%d_", n++);
        return randomName;
    }
    static int onCreateTriangleShapeMeshLua(lua_State *lua,SHAPE_MESH *shape,const int top)
    {
        const int index_triangles_point = 3;
        const int len = lua_rawlen(lua,index_triangles_point);
        if(len != 6)
        {
            return lua_error_debug(lua,"expected table for triangle {x1,y1,x2,y2,x3,y3} as raw table. Size of table %d",len);
        }
        float points[6];
        for(int i=1; i <= 6; ++i)
        {
            lua_rawgeti(lua,index_triangles_point, i);
            points[i-1] = lua_tonumber(lua, -1);
            lua_pop(lua, 1);
        }
        const bool dynamic    = top > 3 ? (lua_toboolean(lua,4) ? true : false ): false;
        const char* nickName  = top > 4 ? lua_tostring(lua,5)  : nullptr;

        std::string newNickName;
		auto getNickName = [&](const bool dynamic) -> const char*
		{
			newNickName = "triangle_points";
            for(int i=0;i < 6; ++i)
            {
                newNickName += ':';
                newNickName += std::to_string(points[i]);
            }
            if(dynamic)
				newNickName += ":dynamic";
			else
				newNickName += ":static";
            return newNickName.c_str();
		};

        if(nickName == nullptr)
            nickName = getNickName(dynamic);
        if(shape->loadTriangle(nickName,points,dynamic))
            lua_pushstring(lua,nickName);
        else
            lua_pushboolean(lua,0);
        return 1;
    }

    static int onCreateDefinedShapeMeshLua(lua_State *lua,SHAPE_MESH *shape,const int top)
    {
        const char* typeShape = lua_tostring(lua,2);
        if(strcasecmp(typeShape,"triangle") == 0 && top > 2)
        {
            if(lua_type(lua,3) == LUA_TTABLE)
            {
                return onCreateTriangleShapeMeshLua(lua,shape,top);
            }
        }
        const float width     = top > 2 ? luaL_checknumber(lua,3) : 100.0f;
        const float height    = top > 3 ? luaL_checknumber(lua,4) : 0.0f;
        int numTriangle       = top > 4 ? luaL_checkinteger(lua,5) : 0;
        const bool dynamic    = top > 5 ? (lua_toboolean(lua,6) ? true : false ): false;
        const char* nickName  = top > 6 ? lua_tostring(lua,7)  : nullptr;
		std::string newNickName;
		auto getNickName = [&](const int numTriangles,const bool dynamic) -> const char*
		{
			newNickName = (typeShape ? typeShape : "");
            newNickName += ':';
            newNickName += std::to_string(width);
            newNickName += ':';
            newNickName += std::to_string(height);
            newNickName += ':';
            newNickName += std::to_string(numTriangles);
			if(dynamic)
				newNickName += ":dynamic";
			else
				newNickName += ":static";
            return newNickName.c_str();
		};

        if(nickName == nullptr)
            nickName = getNickName(numTriangle,dynamic);

        if(strcasecmp(typeShape,"circle") == 0)
		{
            if(numTriangle <= 0)
                numTriangle = 18;
            else if(numTriangle < 4)
                numTriangle = 4;
			if(shape->loadCircle(nickName,width,height,dynamic,numTriangle))
				lua_pushstring(lua,nickName);
			else
				lua_pushboolean(lua,0);
		}
		else if(strcasecmp(typeShape,"rectangle") == 0 || strcasecmp(typeShape,"quad") == 0 || strcasecmp(typeShape,"square") == 0 || strcasecmp(typeShape,"rect") == 0)
		{
            if(numTriangle < 2)
                numTriangle = 2;
			if(shape->loadRectangle(nickName,width,height,dynamic,numTriangle))
				lua_pushstring(lua,nickName);
			else
				lua_pushboolean(lua,0);
		}
		else if(strcasecmp(typeShape,"triangle") == 0)
		{
            if(numTriangle <= 0)
                numTriangle = 1;
			if(shape->loadTriangle(nickName,width,height,dynamic,numTriangle))
				lua_pushstring(lua,nickName);
			else
				lua_pushboolean(lua,0);
		}
		else
		{
			lua_pushboolean(lua,0);
		}
        return 1;
    }

    int onCreateShapeMeshLua(lua_State *lua)
    {
        const int   top            = lua_gettop(lua);
        SHAPE_MESH *shape          = getShapeMeshFromRawTable(lua, 1, 1);
        const int   hasTableXYZ    = top > 1 ? lua_type(lua, 2) : LUA_TNIL;

        if(hasTableXYZ == LUA_TSTRING)
            return onCreateDefinedShapeMeshLua(lua,shape,top);
		util::INFO_DRAW_MODE info_draw_mode;
		
        const int   hasTableUV          = top > 2 ? lua_type(lua, 3) : LUA_TNIL;
        const int   hasTableNormal      = top > 3 ? lua_type(lua, 4) : LUA_TNIL;
		const char *fileName            = (top > 4 && (lua_type(lua, 5) == LUA_TSTRING)) ? lua_tostring(lua, 5) : getRandomNameMesh();
		info_draw_mode.mode_draw                 = top > 5 ? (lua_type(lua, 6) == LUA_TSTRING ? get_mode_draw_from_string(lua_tostring(lua,6),info_draw_mode.mode_draw)                                 : info_draw_mode.mode_draw)                 : info_draw_mode.mode_draw;
		info_draw_mode.mode_cull_face            = top > 6 ? (lua_type(lua, 7) == LUA_TSTRING ? get_mode_cull_face_from_string(lua_tostring(lua,7),info_draw_mode.mode_cull_face)                       : info_draw_mode.mode_cull_face)            : info_draw_mode.mode_cull_face;
		info_draw_mode.mode_front_face_direction = top > 7 ? (lua_type(lua, 8) == LUA_TSTRING ? get_mode_front_face_direction_from_string(lua_tostring(lua,8),info_draw_mode.mode_front_face_direction) : info_draw_mode.mode_front_face_direction) : info_draw_mode.mode_front_face_direction;
        const unsigned int sTableXYZ    = (hasTableXYZ == LUA_TTABLE) ? lua_rawlen(lua, 2) : 0;
        const unsigned int sTableUV     = (hasTableUV == LUA_TTABLE) ? lua_rawlen(lua, 3) : 0;
        const unsigned int sTableNormal = (hasTableNormal == LUA_TTABLE) ? lua_rawlen(lua, 4) : 0;

        AUTO_VERTEX vertex;
        vertex.sizeArray = sTableXYZ;
        if (shape->getFileName() && strcmp(shape->getFileName(), fileName) == 0)
        {
            lua_pushstring(lua, fileName);
            return 1;
        }
        else
        {
            shape->release();
        }
        if (sTableXYZ == 0)
        {
            return lua_error_debug(lua, "[table XYZ] empty!\n Funtions available:\n%s",options_shape);
        }

        if (shape->is3D)
        {
            if (sTableXYZ % 3)
            {
                return lua_error_debug(lua, "[table XYZ] must has x,y e z (divisible by 3)! size [%d] \n\n options:\n%s", sTableXYZ,options_shape);
            }
            if (sTableNormal)
            {
                if (sTableNormal != sTableXYZ)
                {
                    return lua_error_debug(lua, "[table Normal] must has the same size of table XYZ! \n\n options:\n%s", options_shape);
                }
            }
            if (sTableUV)
            {
                if ((sTableXYZ / 3) != (sTableUV / 2))
                {
                    return lua_error_debug(lua, "[table UV] should has the size [%d]!\n\n options:\n%s", (sTableXYZ / 3) * 2, options_shape );
                }
            }
            vertex.ls_xyz = new float[sTableXYZ];
            getArrayFromTable(lua, 2, vertex.ls_xyz, sTableXYZ);
            if (sTableUV)
            {
                vertex.ls_uv = new float[sTableUV];
                getArrayFromTable(lua, 3, vertex.ls_uv, sTableUV);
            }
            if (sTableNormal)
            {
                vertex.ls_normal = new float[sTableNormal];
                getArrayFromTable(lua, 4, vertex.ls_normal, sTableNormal);
            }
        }
        else
        {
            if (sTableXYZ % 2)
            {
                return lua_error_debug(lua, "[table XY] must contain coordinates x and y (pair)! size [%d]\n\n options:\n%s", sTableXYZ, options_shape);
            }
            if (sTableNormal)
            {
                return lua_error_debug(lua, "[table Normal] only must exists with table XYZ!\n\n options:\n%s", options_shape);
            }
            if (sTableUV)
            {
                if (sTableXYZ != sTableUV)
                {
                    return lua_error_debug(lua, "[table UV] It must be the same size as the table XY! size [%d]\n\n options:\n%s", sTableUV, options_shape);
                }
            }
            vertex.ls_xyz = new float[sTableXYZ];
            getArrayFromTable(lua, 2, vertex.ls_xyz, sTableXYZ);

            if (sTableUV)
            {
                vertex.ls_uv = new float[sTableUV];
                getArrayFromTable(lua, 3, vertex.ls_uv, sTableUV);
            }
        }

        if (shape->load(fileName, &vertex,&info_draw_mode))
            lua_pushstring(lua, fileName);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onCreateDynamicIndexedShapeMeshLua(lua_State *lua)
    {
        const int   top            = lua_gettop(lua);
        SHAPE_MESH *shape          = getShapeMeshFromRawTable(lua, 1, 1);
		util::INFO_DRAW_MODE info_draw_mode;
        const int   hasTableXYZ    = top > 1 ? lua_type(lua, 2) : LUA_TNIL;
        const int   hasTableIndex  = top > 2 ? lua_type(lua, 3) : LUA_TNIL;
        const int   hasTableUV     = top > 3 ? lua_type(lua, 4) : LUA_TNIL;
        const int   hasTableNormal = top > 4 ? lua_type(lua, 5) : LUA_TNIL;
        const char *fileName       =(top > 5 && (lua_type(lua, 6) == LUA_TSTRING)) ? luaL_checkstring(lua, 6) : getRandomNameMesh();
		info_draw_mode.mode_draw                 = top > 6 ? (lua_type(lua, 7) == LUA_TSTRING ? get_mode_draw_from_string(lua_tostring(lua,7),info_draw_mode.mode_draw)                                 : info_draw_mode.mode_draw)                 : info_draw_mode.mode_draw;
		info_draw_mode.mode_cull_face            = top > 7 ? (lua_type(lua, 8) == LUA_TSTRING ? get_mode_cull_face_from_string(lua_tostring(lua,8),info_draw_mode.mode_cull_face)                       : info_draw_mode.mode_cull_face)            : info_draw_mode.mode_cull_face;
		info_draw_mode.mode_front_face_direction = top > 8 ? (lua_type(lua, 9) == LUA_TSTRING ? get_mode_front_face_direction_from_string(lua_tostring(lua,9),info_draw_mode.mode_front_face_direction) : info_draw_mode.mode_front_face_direction) : info_draw_mode.mode_front_face_direction;

        const unsigned int sTableXYZ    = (hasTableXYZ == LUA_TTABLE) ? lua_rawlen(lua, 2) : 0;
        const unsigned int sTableIndex  = (hasTableIndex == LUA_TTABLE) ? lua_rawlen(lua, 3) : 0;
        const unsigned int sTableUV     = (hasTableUV == LUA_TTABLE) ? lua_rawlen(lua, 4) : 0;
        const unsigned int sTableNormal = (hasTableNormal == LUA_TTABLE) ? lua_rawlen(lua, 5) : 0;

        unsigned int                          sizeArray = sTableXYZ;
        unsigned int                          sizeIndex = sTableIndex;
        std::vector<float>              ls_xyz;
        std::vector<float>              ls_normal;
        std::vector<float>              ls_uv;
        std::unique_ptr<unsigned short int[],DeleteArrayUnShortInt> ls_index;
        if (sTableXYZ == 0)
        {
            return lua_error_debug(lua, "[table XYZ] empty!\n\n options:\n%s", options_shape);
        }
        if (sTableIndex == 0)
        {
            return lua_error_debug(lua, "[table index] empty!\n\n options:\n%s", options_shape);
        }

        if (shape->is3D)
        {
            if (sTableXYZ % 3)
            {
                return lua_error_debug(lua, "[table XYZ] must contain coordinates x,y and z! \n\n options:\n%s", options_shape);
            }
            if (sTableIndex % 3)
            {
                return lua_error_debug(lua, "[table index] must contain indices for triangle list (1 triang == 3 index)! \n\n options:\n%s", options_shape);
            }
            if (sTableNormal)
            {
                if (sTableNormal != sTableXYZ)
                {
                    return lua_error_debug(lua, "[table Normal] It must be the same size as the table XYZ! \n\n options:\n%s", options_shape);
                }
            }
            if (sTableUV)
            {
                if ((sTableXYZ / 3) != (sTableUV / 2))
                {
                    return lua_error_debug(lua, "[table UV] size should be [%d]! \n\n options:\n%s", (sTableXYZ / 3) * 2, options_shape);
                }
            }
            ls_xyz.resize(sTableXYZ);
            getArrayFromTable(lua, 2, ls_xyz.data(), sTableXYZ);
            ls_index.reset(new unsigned short int[sTableIndex]);
            getArrayFromTable(lua, 3, ls_index.get(), sTableIndex);
            if (sTableUV)
            {
                ls_uv.resize(sTableUV);
                getArrayFromTable(lua, 4, ls_uv.data(), sTableUV);
            }
            if (sTableNormal)
            {
                ls_normal.resize(sTableNormal);
                getArrayFromTable(lua, 5, ls_normal.data(), sTableNormal);
            }
            const auto maxVertex = (const unsigned short int)(sTableXYZ / 3);
            for (unsigned short int i = 0; i < sizeArray; ++i)
            {
                if (ls_index[i] == 0 || ls_index[i] > maxVertex)
                {
                    return lua_error_debug(
                        lua,
                        "[table index] -> index -> [%d] -> the value [%d] is out of bound [table XYZ] -> max vertex [%d] index one based[min is 1]! \n\n options:\n%s", i,
                        ls_index[i], maxVertex, options_shape);
                }
				else
				{
					ls_index[i] -= 1;//index one based
				}
            }
        }
        else
        {
            if (sTableIndex % 3)
            {
                return lua_error_debug(lua, "[table index] must contain indices for triangle list (1 triangle == 3 index)! \n\n options:\n%s", options_shape);
            }
            if (sTableXYZ % 2)
            {
                return lua_error_debug(lua, "[table XY] must contain coordinates x and y!\n\n options:\n%s", options_shape);
            }
            if (sTableNormal)
            {
                return lua_error_debug(lua, "[table Normal] should only exist with table XYZ! \n\n options:\n%s", options_shape);
            }
            if (sTableUV)
            {
                if (sTableXYZ != sTableUV)
                {
                    return lua_error_debug(lua, "[table UV] It must be the same size as the table XY! \n\n options:\n%s", options_shape);
                }
            }
            ls_xyz.resize(sTableXYZ);
            getArrayFromTable(lua, 2, ls_xyz.data(), sTableXYZ);
            ls_index.reset(new unsigned short int[sTableIndex]);
            getArrayFromTable(lua, 3, ls_index.get(), sTableIndex);
            if (sTableUV)
            {
                ls_uv.resize(sTableUV);
                getArrayFromTable(lua, 4, ls_uv.data(), sTableUV);
            }
            const auto maxVertex = (const unsigned short int)(sTableXYZ / 2);
            for (unsigned short int i = 0; i < sizeIndex; ++i)
            {
                if (ls_index[i] == 0 || ls_index[i] > maxVertex)
                {
                    return lua_error_debug(lua,
                               "[table index] -> index -> [%d] -> value [%d] is out of bound [table XYZ] -> max vertex [%d] index one based[min is 1]! \n\n options:\n%s",
                               i, ls_index[i], maxVertex, options_shape);
                }
				else
				{
					ls_index[i] -= 1;//one based
				}
            }
        }

        if (shape->loadIndexedDynamic(fileName, std::move(ls_xyz), std::move(ls_normal), std::move(ls_uv),std::move(ls_index), sizeArray, sizeIndex,&info_draw_mode))
            lua_pushstring(lua, fileName);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onCreateIndexedShapeMeshLua(lua_State *lua)
    {
        const int   top            = lua_gettop(lua);
		util::INFO_DRAW_MODE info_draw_mode;
        SHAPE_MESH *shape          = getShapeMeshFromRawTable(lua, 1, 1);
        const int   hasTableXYZ    = top > 1 ? lua_type(lua, 2) : LUA_TNIL;
        const int   hasTableIndex  = top > 2 ? lua_type(lua, 3) : LUA_TNIL;
        const int   hasTableUV     = top > 3 ? lua_type(lua, 4) : LUA_TNIL;
        const int   hasTableNormal = top > 4 ? lua_type(lua, 5) : LUA_TNIL;
        const char *fileName       = (top > 5 && (lua_type(lua, 6) == LUA_TSTRING)) ? luaL_checkstring(lua, 6) : getRandomNameMesh();
		
		info_draw_mode.mode_draw                 = top > 6 ? (lua_type(lua, 7) == LUA_TSTRING ? get_mode_draw_from_string(lua_tostring(lua,7),info_draw_mode.mode_draw)                                 : info_draw_mode.mode_draw)                 : info_draw_mode.mode_draw;
		info_draw_mode.mode_cull_face            = top > 7 ? (lua_type(lua, 8) == LUA_TSTRING ? get_mode_cull_face_from_string(lua_tostring(lua,8),info_draw_mode.mode_cull_face)                       : info_draw_mode.mode_cull_face)            : info_draw_mode.mode_cull_face;
		info_draw_mode.mode_front_face_direction = top > 8 ? (lua_type(lua, 9) == LUA_TSTRING ? get_mode_front_face_direction_from_string(lua_tostring(lua,9),info_draw_mode.mode_front_face_direction) : info_draw_mode.mode_front_face_direction) : info_draw_mode.mode_front_face_direction;

        const unsigned int sTableXYZ    = (hasTableXYZ == LUA_TTABLE) ? lua_rawlen(lua, 2) : 0;
        const unsigned int sTableIndex  = (hasTableIndex == LUA_TTABLE) ? lua_rawlen(lua, 3) : 0;
        const unsigned int sTableUV     = (hasTableUV == LUA_TTABLE) ? lua_rawlen(lua, 4) : 0;
        const unsigned int sTableNormal = (hasTableNormal == LUA_TTABLE) ? lua_rawlen(lua, 5) : 0;

        AUTO_VERTEX vertex;
        vertex.sizeArray = sTableXYZ;
        vertex.sizeIndex = sTableIndex;
        if (shape->getFileName() && strcmp(shape->getFileName(), fileName) == 0)
        {
            lua_pushstring(lua, fileName);
            return 1;
        }
        else
        {
            shape->release();
        }
        if (sTableXYZ == 0)
        {
            return lua_error_debug(lua, "[table XYZ] empty! \n\n options:\n%s", options_shape);
        }
        if (sTableIndex == 0)
        {
            return lua_error_debug(lua, "[table index] empty! \n\n options:\n%s", options_shape);
        }

        if (shape->is3D)
        {
            if (sTableXYZ % 3)
            {
                return lua_error_debug(lua, "[table XYZ] must contain coordinates x,y and z! \n\n options:\n%s", options_shape);
            }
            if (sTableIndex % 3)
            {
                return lua_error_debug(lua, "[table index] must contain indices for triangle list (1 triang == 3 index)! \n\n options:\n%s", options_shape);
            }
            if (sTableNormal)
            {
                if (sTableNormal != sTableXYZ)
                {
                    return lua_error_debug(lua, "[table Normal] It must be the same size as the table XYZ! \n\n options:\n%s", options_shape);
                }
            }
            if (sTableUV)
            {
                if ((sTableXYZ / 3) != (sTableUV / 2))
                {
                    return lua_error_debug(lua, "[table UV] size should be [%d]! \n\n options:\n%s", (sTableXYZ / 3) * 2, options_shape);
                }
            }
            vertex.ls_xyz = new float[sTableXYZ];
            getArrayFromTable(lua, 2, vertex.ls_xyz, sTableXYZ);
            vertex.ls_index = new unsigned short int[sTableIndex];
            getArrayFromTable(lua, 3, vertex.ls_index, sTableIndex);
            if (sTableUV)
            {
                vertex.ls_uv = new float[sTableUV];
                getArrayFromTable(lua, 4, vertex.ls_uv, sTableUV);
            }
            if (sTableNormal)
            {
                vertex.ls_normal = new float[sTableNormal];
                getArrayFromTable(lua, 5, vertex.ls_normal, sTableNormal);
            }
            const auto maxVertex = (const unsigned short int)(sTableXYZ / 3);
            for (unsigned short int i = 0; i < vertex.sizeIndex; ++i)
            {
                if (vertex.ls_index[i] == 0 || vertex.ls_index[i] > maxVertex)
                {
                    return lua_error_debug(
                        lua,
                        "[table index] -> index -> [%d] -> the value [%d] is out of bound [table XYZ] -> max vertex [%d] index one based[min is 1]! \n\n options:\n%s", i,
                        vertex.ls_index[i], maxVertex, options_shape);
                }
				else
				{
					vertex.ls_index[i] -= 1;//index one based
				}
            }
        }
        else
        {
            if (sTableIndex % 3)
            {
                return lua_error_debug(lua, "[table index] must contain indices for triangle list (1 triangle == 3 index)! \n\n options:\n%s", options_shape);
            }
            if (sTableXYZ % 2)
            {
                return lua_error_debug(lua, "[table XY] must contain coordinates x and y! \n\n options:\n%s", options_shape);
            }
            if (sTableNormal)
            {
                return lua_error_debug(lua, "[table Normal] should only exist with table XYZ! \n\n options:\n%s", options_shape);
            }
            if (sTableUV)
            {
                if (sTableXYZ != sTableUV)
                {
                    return lua_error_debug(lua, "[table UV] It must be the same size as the table XY! \n\n options:\n%s", options_shape);
                }
            }
            vertex.ls_xyz = new float[sTableXYZ];
            getArrayFromTable(lua, 2, vertex.ls_xyz, sTableXYZ);
            vertex.ls_index = new unsigned short int[sTableIndex];
            getArrayFromTable(lua, 3, vertex.ls_index, sTableIndex);
            if (sTableUV)
            {
                vertex.ls_uv = new float[sTableUV];
                getArrayFromTable(lua, 4, vertex.ls_uv, sTableUV);
            }
            const auto maxVertex = (const unsigned short int)(sTableXYZ / 2);
            for (unsigned short int i = 0; i < vertex.sizeIndex; ++i)
            {
                if (vertex.ls_index[i] == 0 || vertex.ls_index[i] > maxVertex)
                {
                    return lua_error_debug(lua,
                               "[table index] -> index -> [%d] -> value [%d] is out of bound [table XYZ] -> max vertex [%d] index one based[min is 1]! \n\n options:\n%s",
                               i, vertex.ls_index[i], maxVertex, options_shape);
                }
				else
				{
					vertex.ls_index[i] -= 1;//index one based
				}
            }
        }

		if (shape->loadIndexed(fileName, &vertex,&info_draw_mode))
            lua_pushstring(lua, fileName);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

	/*
	example LUA code
	function onRender(tShape,vertex,normal,uv,index_buffer)
		tShape.x = 0 -- that is the shape
		print("--------------------")
		print(" vertex (" .. #vertex .. ')')

		print(" name table 1: (" .. vertex.name .. ')')
		print(" name table 2: (" .. normal.name .. ')')
		print(" name table 3: ("     .. uv.name .. ')')

		for i=1,#vertex do
			print('x:' .. vertex[i].x .. ' y:' .. vertex[i].y .. ' z:' .. vertex[i].z)
		end
    
		print(" normal (" .. #normal .. ')')
		for i=1,#normal do
			print('nx:' .. normal[i].nx .. ' ny:' .. normal[i].ny .. ' nz:' .. normal[i].nz)
		end

		print(" uv (" .. #uv .. ')')
		for i=1,#uv do
			print('u:' .. uv[i].u .. ' v:' .. uv[i].v )
		end

		print("--------------------")

		return vertex,normal,uv --overwrite all
		
		-- return vertex,uv --overwrite only vertex and uv

		-- return uv --overwrite only uv
	end

	tShape:onRender(onRender) -- set the callback

	*/

	static void onRenderDynamicBufferCallBackLua(SHAPE_MESH * shape, std::vector<float> & dynamicVertex,std::vector<float> & dynamicNormal,std::vector<float> & dynamicUV,const std::vector<uint16_t> & index_read_only)
	{
        auto *userData   = static_cast<USER_DATA_SHAPE_LUA *>(shape->userData);
		DEVICE * device  = DEVICE::getInstance();
        auto * sceneData = static_cast<USER_DATA_SCENE_LUA *>(device->scene->userData);
        lua_State * lua  = sceneData->lua;
        lua_rawgeti(lua, LUA_REGISTRYINDEX, userData->ref_CallBackEditVertexBuffer);
        if (lua_isfunction(lua, -1))
        {
			constexpr int nargs    = 1 + 4;
			constexpr int nresults = 3;
			lua_rawgeti(lua, LUA_REGISTRYINDEX, userData->ref_MeAsTable);

			pushVectorArrayToTableWithField(lua,dynamicVertex,"x","y","z");
			lua_pushstring(lua,"vertex");
			lua_setfield(lua,-2,"name");

			pushVectorArrayToTableWithField(lua,dynamicNormal,"nx","ny","nz");
			lua_pushstring(lua,"normal");
			lua_setfield(lua,-2,"name");

			pushVectorArrayToTableWithField(lua,dynamicUV,"u","v");
			lua_pushstring(lua,"uv");
			lua_setfield(lua,-2,"name");

            push_uint16_arrayFromTable(lua,index_read_only.data(),index_read_only.size(),true);

			if (lua_pcall(lua, nargs, nresults, 0))
                lua_error_debug(lua, "\n%s", luaL_checkstring(lua, -1));
            auto get_exepected_name_table = [](const int & index_iteration) -> const char *
			{
				switch(index_iteration)
				{
					case 0: return "vertex";
					case 1: return "normal";
					case 2: return "uv";
					default: return "unknown";
				}
			};

			const int top       = lua_gettop(lua);
			int index_iteration = 0;
            for(int index = 1; index <= top; ++index)
			{
				if(lua_type(lua,index) == LUA_TTABLE)
				{
					lua_getfield(lua,index,"name");
					const char * name = lua_type(lua,-1) == LUA_TSTRING ? lua_tostring(lua,-1) : get_exepected_name_table(index_iteration);
                    lua_pop(lua,1);
					if(strcasecmp(name,"vertex") == 0)
					{
						getArrayFromTableWithField(lua,index,dynamicVertex.data(),dynamicVertex.size(),"x","y","z");
						++index_iteration;
					}
					else if(strcasecmp(name,"normal") == 0)
					{
						getArrayFromTableWithField(lua,index,dynamicNormal.data(),dynamicNormal.size(),"nx","ny","nz");
						++index_iteration;
					}
					else if(strcasecmp(name,"uv") == 0)
					{
						getArrayFromTableWithField(lua,index,dynamicUV.data(),dynamicUV.size(),"u","v");
						++index_iteration;
					}
				}
			}
			
			lua_settop(lua,0);
			if(index_iteration == 0)
			{
				shape->setOnRenderDynamicBuffer(nullptr);
                userData->unrefTableLua(lua,&userData->ref_CallBackEditVertexBuffer);
			}
        }
        else
        {
            lua_pop(lua, 1);
        }
	}

	int onRenderDynamicBufferShapeMeshLua(lua_State *lua)
	{
		const int top = lua_gettop(lua);
        if (top >= 2)
        {
            SHAPE_MESH *shape          = getShapeMeshFromRawTable(lua, 1, 1);
			if(shape->isDynamicBufferMode())
			{
				auto *userData = static_cast<USER_DATA_SHAPE_LUA *>(shape->userData);
				if(userData)
				{
                    if(lua_type(lua, 2) == LUA_TNIL)
                    {
                        userData->unrefTableLua(lua,&userData->ref_CallBackEditVertexBuffer);
                        shape->setOnRenderDynamicBuffer(nullptr);
                    }
                    else
                    {
                        userData->refTableLua(lua, 1, &userData->ref_MeAsTable);
                        userData->refFunctionLua(lua, 2, &userData->ref_CallBackEditVertexBuffer);
                        shape->setOnRenderDynamicBuffer(onRenderDynamicBufferCallBackLua);
                    }
				}
			}
			else
			{
				return lua_error_debug(lua,"To be able to edit the Shape, it must be loaded in dynamic buffer mode.\n"
				"Funtions available for that:\n%s",
				options_shape);
			}
        }
		else
		{
			return lua_error_debug(lua,"Expected callback function");
		}
        return 0;
	}

    int onNewShapeMeshLua(lua_State *lua)
    {
        const int top                   = lua_gettop(lua);
        luaL_Reg  regShapeMeshMethods[] = {
                                          {"create", onCreateShapeMeshLua},
                                          {"createIndexed", onCreateIndexedShapeMeshLua},
                                          {"createDynamicIndexed", onCreateDynamicIndexedShapeMeshLua},
										  {"onRender", onRenderDynamicBufferShapeMeshLua},
                                          {nullptr, nullptr}};
        SELF_ADD_COMMON_METHODS selfMethods(regShapeMeshMethods);
        const luaL_Reg *             regMethods = selfMethods.get();
        VEC3                    position(0, 0, 0);
        bool                         is2ds = true;
        bool                         is3d  = false;
        bool                         is2dw = false;
        for (int i = 2; i <= top; ++i)
        {
            switch (i)
            {
                case 2:
                {
                    getTypeWordRenderizableLua(lua,i,is2dw,is2ds,is3d);
                }
                break;
                case 3: // x
                {
                    position.x = luaL_checknumber(lua, i);
                }
                break;
                case 4: // y
                {
                    position.y = luaL_checknumber(lua, i);
                }
                break;
                case 5: // z
                {
                    position.z = luaL_checknumber(lua, i);
                }
                break;
                default: {
                }
                break;
            }
        }
        lua_settop(lua, 0);
        // luaL_newlib(lua, regMethods);
        lua_createtable(lua, 0, selfMethods.getSize());
        luaL_setfuncs(lua, regMethods, 0);
        luaL_getmetatable(lua, "_mbmShape");
        lua_setmetatable(lua, -2);

        auto **udata  = static_cast<SHAPE_MESH **>(lua_newuserdata(lua, sizeof(SHAPE_MESH *)));
        DEVICE *device = DEVICE::getInstance();
        auto  shape  = new SHAPE_MESH(device->scene, is3d, is2ds);
		auto userDataShape  = new USER_DATA_SHAPE_LUA();
        shape->userData     = userDataShape;
        *udata              = shape;
        if (position.x != 0.0f) //-V550
            shape->position.x = position.x;
        if (position.y != 0.0f) //-V550
            shape->position.y = position.y;
        if (position.z != 0.0f) //-V550
            shape->position.z = position.z;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_SHAPE_MESH);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }

    void registerClassShapeMesh(lua_State *lua)
    {
        luaL_Reg regShapeMeshMethods[] = {{"new", onNewShapeMeshLua},
                                          {"__newindex", onNewIndexRenderizableLua},
                                          {"__index", onIndexRenderizableLua},
                                          {"__gc", onDestroyShapeMeshLua},
                                          {"__close", onDestroyRenderizable},
                                          {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmShape");
        luaL_setfuncs(lua, regShapeMeshMethods, 0);
        lua_setglobal(lua, "shape");
        lua_settop(lua,0);
    }
};
