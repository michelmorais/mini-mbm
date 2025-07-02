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

#if defined USE_EDITOR_FEATURES

extern "C" 
{
    #include <lua.h>
    #include <lauxlib.h>
    #include <lualib.h>
}

#include <map>
#include <string>

#include <lua-wrap/render-table/mesh-debug-lua.h>
#include <lua-wrap/check-user-type-lua.h>
#include <core_mbm/mesh-manager.h>
#include <core_mbm/dynamic-var.h>
#include <core_mbm/animation.h>
#include <core_mbm/shapes.h>
#include <core_mbm/texture-manager.h>
#include <core_mbm/shader-fx.h>
#include <render/particle.h>
#include <core_mbm/util-interface.h>
#include <core_mbm/shader-var-cfg.h>
#include <core_mbm/physics.h>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

namespace mbm
{
	class LINE_MESH;
    extern RENDERIZABLE * getRenderizableFromRawTable(lua_State *lua, const int rawi, const int indexTable);
    extern int getVariable(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what);
    extern int getVariable(lua_State *lua, RENDERIZABLE *ptr, const char *what);
    extern int setVariable(lua_State *lua, std::map<std::string, DYNAMIC_VAR *> &lsDynamicVar, const char *what);
    extern int setVariable(lua_State *lua, RENDERIZABLE *ptr, const char *what);
    extern ANIMATION_MANAGER *getAnimationManagerFromRawTable(lua_State *lua, const int rawi, const int indexTable,RENDERIZABLE **renderizable);
    extern void getArrayFromTablePixels(lua_State *lua, const int index, unsigned char *lsArrayOut, const unsigned int sizeBuffer);
    extern void getArrayFromTable(lua_State *lua, const int index, float *lsArrayOut, const unsigned int sizeBuffer);
    extern void getArrayFromTable(lua_State *lua, const int index, unsigned short int *lsArrayOut, const unsigned int sizeBuffer);
    extern int verifyDynamicCast(lua_State *lua, void *ptr, int line, const char *__file);
    extern void getFieldPrimaryFromTable(lua_State *lua, const int indexTable, const char *fieldName, const int LUA_TYPE,void *ptrRet);
    extern void getFieldUnsigned8FromTable(lua_State *lua, const int indexTable, const char *fieldName,uint8_t *ptrRet);
    extern void getFieldUnsignedShortFromTable(lua_State *lua, const int indexTable, const char *fieldName,unsigned short int *ptrRet);
    extern void getFieldSignedShortFromTable(lua_State *lua, const int indexTable, const char *fieldName, short int *ptrRet);
    extern int onSetPhysicsFromTableLua(lua_State *lua,INFO_PHYSICS* infoPhysics,LINE_MESH * lineMesh);
    extern int onNewMeshLua(lua_State *lua);
    extern int onNewSpriteLua(lua_State *lua);
    extern int onNewFontLua(lua_State *lua);
    extern int onNewGifViewLua(lua_State *lua);
    extern int onNewParticleLua(lua_State *lua);
    extern int onNewTextureViewLua(lua_State *lua);
	extern const unsigned int get_mode_draw_from_string(const char* str_mode_draw,const unsigned int default_mode_draw_ret);
	extern const char * get_mode_draw_from_uint(const unsigned int mode_draw,const char * default_mode_draw_ret);

	extern const unsigned int get_mode_cull_face_from_string(const char* str_mode_cull_face,const unsigned int default_mode_cull_face_ret);
	extern const char * get_mode_cull_face_from_uint(const unsigned int mode_cull_face,const char * default_mode_cull_face_ret);

	extern const unsigned int get_mode_front_face_direction_from_string(const char* str_mode_front_face_direction,const unsigned int default_mode_front_face_direction_ret);
	extern const char * get_mode_front_face_direction_from_uint(const unsigned int mode_front_face_direction,const char * default_mode_front_face_direction_ret);
	extern int lua_error_debug(lua_State *lua, const char *format, ...);
	extern void lua_print_line(lua_State *lua, TYPE_LOG type_log, const char *format, ...);

    class MESH_DEBUG_LUA
    {
      public:
        MESH_MBM_DEBUG mesh;
        std::map<std::string, DYNAMIC_VAR *> lsDynamicVar;

        MESH_DEBUG_LUA()
        = default;

        ~MESH_DEBUG_LUA()
        {
            std::map<std::string, DYNAMIC_VAR *>::const_iterator it;
            for (it = this->lsDynamicVar.cbegin(); it != this->lsDynamicVar.cend(); ++it)
            {
                DYNAMIC_VAR *dVar = it->second;
                if (dVar)
                    delete dVar;
            }
            this->lsDynamicVar.clear();
        }

        inline const char * getFileName()
        {
            return this->mesh.fileName.c_str();
        }
    };

    
    MESH_DEBUG_LUA *getMeshDebugFromRawTable(lua_State *lua, const int rawi, const int indexTable)
    {
        auto **ud = static_cast<MESH_DEBUG_LUA **>(lua_check_userType(lua,rawi,indexTable,L_USER_TYPE_MESH_DEBUG));
        return *ud;
    }

    int onLoadMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const int type            = lua_type(lua,2);
        if (type == LUA_TTABLE)
        {
            RENDERIZABLE *ptr      = getRenderizableFromRawTable(lua, 1, 2);
            const MESH_MBM* mesh   = ptr->getMesh();
            if (meshDebug->mesh.loadDebugFromMemory(mesh))
                lua_pushboolean(lua, 1);
            else
                lua_pushboolean(lua, 0);
        }
        else if(type == LUA_TSTRING)
        {
            const char *    fileName  = lua_tostring(lua, 2);
            if (meshDebug->mesh.loadDebug(fileName))
                lua_pushboolean(lua, 1);
            else
                lua_pushboolean(lua, 0);
        }
        else
        {
            return lua_error_debug(lua, "Expected [string] file name or [renderizable]. Got [%s]",luaL_typename(lua,type));
        }
        return 1;
    }

    int onSaveMeshDebugLua(lua_State *lua)
    {
        const int       top           = lua_gettop(lua);
        MESH_DEBUG_LUA *meshDebug     = getMeshDebugFromRawTable(lua, 1, 1);
        const char *    fileName      = luaL_checkstring(lua, 2);
        const bool      calNormal     = top > 2 ? (lua_toboolean(lua, 3) ? true : false) : false;
        const bool      calUV         = top > 3 ? (lua_toboolean(lua, 4) ? true : false) : false;
        char            strError[255] = "";
        if (meshDebug->mesh.saveDebug(fileName, calNormal, calUV, strError,sizeof(strError)-1))
        {
            MESH_MANAGER::getInstance()->fakeRelease(fileName);
            lua_pushboolean(lua, 1);
        }
        else
        {
			lua_print_line(lua,TYPE_LOG_ERROR, "Failed to save mesh debug\n%s",strError);
            lua_pushboolean(lua, 0);
        }
        return 1;
    }

    int onGetInfoMeshDebugLua(lua_State *lua)
    {
        util::HEADER_MESH headerMeshMbmOut;
        util::TYPE_MESH   typeOut;
		util::INFO_DRAW_MODE info_mode;
        INFO_BOUND_FONT  datailFontOut;
        std::vector<util::STAGE_PARTICLE> lsParticleInfo;
        const char *          fileName = luaL_checkstring(lua, 2);
        if (!MESH_MBM_DEBUG::getInfo(fileName, headerMeshMbmOut, info_mode,typeOut, datailFontOut, lsParticleInfo))
        {
            lua_pushnil(lua);
            return 1;
        }
        /*
        table = {
        totalFrames   = number    --Total frames
        animation     = number    --Number of animations
        type          = "string"  --Type: mesh, sprite, font, texture, frame, unknown
		modeDraw      = "string"  --Mode TRIANGLES, TRIANGLE_STRIP, TRIANGLE_FAN
		modeCullFace  = "string"  --FRONT, BACK, FRONT_AND_BACK
		modeFrontFace = "string"  --CW, CCW
        hasNormal     = boolean   --has normal coordinates*
        hasTexture    = boolean   --Has textura coordinates**
        position      = {x,y,z}   --Initial position (deprected)
        angle         = {x,y,z}   --Initial angle (deprected)
        */
        lua_newtable(lua);
        lua_pushinteger(lua, headerMeshMbmOut.totalAnimation);
        lua_setfield(lua, -2, "animation");
        bool unknown = false;
        switch (typeOut)
        {
            case util::TYPE_MESH_3D:        { lua_pushstring(lua, "mesh");      }break;
            case util::TYPE_MESH_USER:      { lua_pushstring(lua, "user");      }break;
            case util::TYPE_MESH_SPRITE:    { lua_pushstring(lua, "sprite");    }break;
            case util::TYPE_MESH_FONT:      { lua_pushstring(lua, "font");      }break;
            case util::TYPE_MESH_TEXTURE:   { lua_pushstring(lua, "texture");   }break;
            case util::TYPE_MESH_SHAPE:     { lua_pushstring(lua, "shape");     }break;
            case util::TYPE_MESH_PARTICLE:  { lua_pushstring(lua, "particle");  }break;
			case util::TYPE_MESH_TILE_MAP:  { lua_pushstring(lua, "tile");      }break;
            default:                        { 
                                                lua_pushstring(lua, "unknown"); 
                                                unknown = true; 
                                            }
                                            break;
        }
        lua_setfield(lua, -2, "type");

		lua_pushstring(lua, get_mode_draw_from_uint(info_mode.mode_draw,"UNKNOWN"));
		lua_setfield(lua, -2, "modeDraw");

		lua_pushstring(lua, get_mode_cull_face_from_uint(info_mode.mode_cull_face,"UNKNOWN"));
		lua_setfield(lua, -2, "modeCullFace");

		lua_pushstring(lua, get_mode_front_face_direction_from_uint(info_mode.mode_front_face_direction,"UNKNOWN"));
		lua_setfield(lua, -2, "modeFrontFace");

        lua_pushinteger(lua, headerMeshMbmOut.totalFrames);
        lua_setfield(lua, -2, "totalFrames");

        lua_pushboolean(lua, headerMeshMbmOut.hasNorText[0]);
        lua_setfield(lua, -2, "hasNormal");

        lua_pushboolean(lua, headerMeshMbmOut.hasNorText[1]);
        lua_setfield(lua, -2, "hasTexture");

        // angle
        lua_newtable(lua);
        lua_pushnumber(lua, headerMeshMbmOut.angleX);
        lua_setfield(lua, -2, "x");

        lua_pushnumber(lua, headerMeshMbmOut.angleY);
        lua_setfield(lua, -2, "y");

        lua_pushnumber(lua, headerMeshMbmOut.angleZ);
        lua_setfield(lua, -2, "z");

        lua_setfield(lua, -2, "angle");

        lua_newtable(lua);
        lua_pushnumber(lua, headerMeshMbmOut.posX);
        lua_setfield(lua, -2, "x");

        lua_pushnumber(lua, headerMeshMbmOut.posY);
        lua_setfield(lua, -2, "y");

        lua_pushnumber(lua, headerMeshMbmOut.posZ);
        lua_setfield(lua, -2, "z");

        lua_setfield(lua, -2, "position");

        if(typeOut == util::TYPE_MESH_TEXTURE || unknown)
        {
            lua_pushstring(lua,datailFontOut.fontName.c_str());
            lua_setfield(lua, -2, "ext");
        }
        return 1;
    }

    int onGetTypeMeshDebugLua(lua_State *lua)
    {
        const int       top       = lua_gettop(lua);
        if (top > 1)
        {
            MESH_MBM_DEBUG  meshTmp;
            const char *        fileName = luaL_checkstring(lua, 2);
            util::TYPE_MESH type     = meshTmp.getType(fileName);
            switch (type)
            {
                case util::TYPE_MESH_3D: { lua_pushstring(lua, "mesh");}
                break;
                case util::TYPE_MESH_USER: { lua_pushstring(lua, "user");}
                break;
                case util::TYPE_MESH_SPRITE: { lua_pushstring(lua, "sprite");}
                break;
                case util::TYPE_MESH_FONT: { lua_pushstring(lua, "font");}
                break;
                case util::TYPE_MESH_TEXTURE: { lua_pushstring(lua, "texture");}
                break;
                case util::TYPE_MESH_SHAPE: { lua_pushstring(lua, "shape");}
                break;
                case util::TYPE_MESH_PARTICLE: { lua_pushstring(lua, "particle"); }
                break;
				case util::TYPE_MESH_TILE_MAP: { lua_pushstring(lua, "tile"); }
				break;
                default: { lua_pushstring(lua, "unknown");}
                break;
            }
        }
        else
        {
            MESH_DEBUG_LUA* meshDebug     = getMeshDebugFromRawTable(lua, 1, 1);
            util::TYPE_MESH type = meshDebug->mesh.getType();
            switch (type)
            {
                case util::TYPE_MESH_3D: { lua_pushstring(lua, "mesh");}
                break;
                case util::TYPE_MESH_USER: { lua_pushstring(lua, "user");}
                break;
                case util::TYPE_MESH_SPRITE: { lua_pushstring(lua, "sprite");}
                break;
                case util::TYPE_MESH_FONT: { lua_pushstring(lua, "font");}
                break;
                case util::TYPE_MESH_TEXTURE: { lua_pushstring(lua, "texture");}
                break;
                case util::TYPE_MESH_SHAPE: { lua_pushstring(lua, "shape");}
                break;
                case util::TYPE_MESH_PARTICLE: { lua_pushstring(lua, "particle"); }
                break;
				case util::TYPE_MESH_TILE_MAP: { lua_pushstring(lua, "tile"); }
				break;
                default: { lua_pushstring(lua, "unknown");}
                break;
            }
        }
        return 1;
    }

    int onSetPhysicsMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        return onSetPhysicsFromTableLua(lua,&meshDebug->mesh.infoPhysics,nullptr);
    }

    int onGetPhysicsMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        lua_settop(lua,0);
        lua_newtable(lua); // array
        const unsigned int sCube = meshDebug->mesh.infoPhysics.lsCube.size();
        const unsigned int sTria = meshDebug->mesh.infoPhysics.lsTriangle.size();
        const unsigned int sSphe = meshDebug->mesh.infoPhysics.lsSphere.size();
        const unsigned int sComp = meshDebug->mesh.infoPhysics.lsCubeComplex.size();
        int index_array = 1; 
        
        if (sCube)
        {
            for (unsigned int i = 0; i < sCube; ++i)
            {
                CUBE *base = meshDebug->mesh.infoPhysics.lsCube[i];
                lua_newtable(lua); // cube
                lua_pushstring(lua, "cube");
                lua_setfield(lua, -2, "type");
                lua_newtable(lua); // center
                lua_pushnumber(lua, base->absCenter.x);
                lua_setfield(lua, -2, "x");

                lua_pushnumber(lua, base->absCenter.y);
                lua_setfield(lua, -2, "y");

                lua_pushnumber(lua, base->absCenter.z);
                lua_setfield(lua, -2, "z");

                lua_setfield(lua, -2, "center");

                lua_newtable(lua); // half
                lua_pushnumber(lua, base->halfDim.x);
                lua_setfield(lua, -2, "x");

                lua_pushnumber(lua, base->halfDim.y);
                lua_setfield(lua, -2, "y");

                lua_pushnumber(lua, base->halfDim.z);
                lua_setfield(lua, -2, "z");

                lua_setfield(lua, -2, "half");

                lua_rawseti(lua,-2, index_array++);//array
            }
        }
        if(sTria)
        {
            for (unsigned int i = 0; i < sTria; ++i)
            {
                lua_newtable(lua); // raw
                lua_pushstring(lua, "triangle");
                lua_setfield(lua, -2, "type");
                TRIANGLE *triangle = meshDebug->mesh.infoPhysics.lsTriangle[i];
                for (unsigned int j = 0; j < 3; ++j)
                {
                    char c[2] = "a";
                    c[0] += (char)(j);
                    c[1] = 0;
                    lua_newtable(lua); 
                    lua_pushnumber(lua, triangle->point[j].x);
                    lua_setfield(lua, -2, "x");

                    lua_pushnumber(lua, triangle->point[j].y);
                    lua_setfield(lua, -2, "y");

                    lua_pushnumber(lua, triangle->point[j].z);
                    lua_setfield(lua, -2, "z");

                    lua_setfield(lua, -2, c);
                }
                lua_rawseti(lua,-2, index_array++);//array
            }
        }
        if(sSphe)
        {
            for (unsigned int i = 0; i < sSphe; ++i)
            {
                SPHERE *sphere = meshDebug->mesh.infoPhysics.lsSphere[i];
                lua_newtable(lua); // raw
                lua_pushstring(lua, "sphere");
                lua_setfield(lua, -2, "type");
                lua_newtable(lua); // center
                lua_pushnumber(lua, sphere->absCenter[0]);
                lua_setfield(lua, -2, "x");

                lua_pushnumber(lua, sphere->absCenter[1]);
                lua_setfield(lua, -2, "y");

                lua_pushnumber(lua, sphere->absCenter[2]);
                lua_setfield(lua, -2, "z");

                lua_setfield(lua, -2, "center");

                lua_pushnumber(lua, sphere->ray);
                lua_setfield(lua, -2, "ray");

                lua_rawseti(lua,-2, index_array++);//array
            }
        }
        if(sComp)
        {
            for (unsigned int i = 0; i < sComp; ++i)
            {
                lua_newtable(lua); // raw
                lua_pushstring(lua, "complex");
                lua_setfield(lua, -2, "type");
                CUBE_COMPLEX *complex = meshDebug->mesh.infoPhysics.lsCubeComplex[i];
                for (unsigned int j = 0; j < 8; ++j)
                {
                    char c[2] = "a";
                    c[0] += (char)(j);
                    c[1] = 0;
                    lua_newtable(lua); // a,b,c,d,e,f,g,h
                    lua_pushnumber(lua, complex->p[j].x);
                    lua_setfield(lua, -2, "x");

                    lua_pushnumber(lua, complex->p[j].y);
                    lua_setfield(lua, -2, "y");

                    lua_pushnumber(lua, complex->p[j].z);
                    lua_setfield(lua, -2, "z");

                    lua_setfield(lua, -2, c);
                }
                lua_rawseti(lua,-2, index_array++);//array
            }
        }
        return 1;
    }

    int onSetTypeMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug    = getMeshDebugFromRawTable(lua, 1, 1);
        const char *    typeAsString = luaL_checkstring(lua, 2);
        if (typeAsString)
        {
            if (strcasecmp(typeAsString, "mesh") == 0)
            {
                meshDebug->mesh.typeMe = util::TYPE_MESH_3D;
                lua_pushboolean(lua, 1);
            }
            else if (strcasecmp(typeAsString, "sprite") == 0)
            {
                meshDebug->mesh.typeMe = util::TYPE_MESH_SPRITE;
                lua_pushboolean(lua, 1);
            }
            else if (strcasecmp(typeAsString, "font") == 0)
            {
                meshDebug->mesh.typeMe = util::TYPE_MESH_FONT;
                lua_pushboolean(lua, 1);
            }
            else if (strcasecmp(typeAsString, "user") == 0)
            {
                meshDebug->mesh.typeMe = util::TYPE_MESH_USER;
                lua_pushboolean(lua, 1);
            }
            else if (strcasecmp(typeAsString, "texture") == 0)
            {
                meshDebug->mesh.typeMe = util::TYPE_MESH_TEXTURE;
                lua_pushboolean(lua, 1);
            }
            else if (strcasecmp(typeAsString, "shape") == 0)
            {
                meshDebug->mesh.typeMe = util::TYPE_MESH_SHAPE;
                lua_pushboolean(lua, 1);
            }
            else if (strcasecmp(typeAsString, "particle") == 0)
            {
                meshDebug->mesh.typeMe = util::TYPE_MESH_PARTICLE;
                lua_pushboolean(lua, 1);
            }
			else if (strcasecmp(typeAsString, "tile") == 0)
			{
				meshDebug->mesh.typeMe = util::TYPE_MESH_TILE_MAP;
				lua_pushboolean(lua, 1);
			}
            else
            {
                lua_pushboolean(lua, 0);
            }
        }
        else
        {
            lua_pushboolean(lua, 0);
        }
        return 1;
    }

    mbm::INFO_BOUND_FONT* newInfoFontFromLua(lua_State *lua,const int indexTable)
    {
        const int top  = lua_gettop(lua);
        const int type = lua_type(lua, indexTable);
		if(type == LUA_TTABLE)
        {
            lua_getfield(lua,indexTable, "letters");
            const int index_table_letter = top + 1;
            int lenTable = lua_rawlen(lua, index_table_letter);
            mbm::INFO_BOUND_FONT * infoFont = new mbm::INFO_BOUND_FONT();
            for(int i=0; i < lenTable; ++i)
            {
                lua_rawgeti(lua, index_table_letter, i + 1);
                const int i_index_table_letter = index_table_letter + 1;
                if(lua_type(lua, i_index_table_letter) == LUA_TTABLE)
                {
                    auto detailFont = new util::DETAIL_LETTER();
                    getFieldUnsigned8FromTable(lua,    i_index_table_letter,"char_id", &detailFont->letter);
                    getFieldUnsigned8FromTable(lua,    i_index_table_letter,"index",   &detailFont->indexFrame);
                    getFieldUnsignedShortFromTable(lua,i_index_table_letter,"width",   &detailFont->widthLetter);
                    getFieldUnsignedShortFromTable(lua,i_index_table_letter,"height",  &detailFont->heightLetter);
                    if(infoFont->letter[detailFont->letter].detail == nullptr)
                        infoFont->letter[detailFont->letter].detail = detailFont;
                    else
                    {
                        WARN_LOG("Letter [%c] already set\n",detailFont->letter);
                        delete detailFont;
                    }
                    
                }
                lua_pop(lua, 1);
            }
            getFieldPrimaryFromTable(lua,indexTable,      "font_name",LUA_TSTRING,&infoFont->fontName);
            getFieldUnsignedShortFromTable(lua,indexTable,"height",   &infoFont->heightLetter);
            getFieldSignedShortFromTable(lua,indexTable,  "space_x",  &infoFont->spaceXCharacter);
            getFieldSignedShortFromTable(lua,indexTable,  "space_y",  &infoFont->spaceYCharacter);
            return infoFont;
        }
        return nullptr;
    }
    
    int onSetDetailLua(lua_State *lua)
	{
        MESH_DEBUG_LUA *meshDebug     = getMeshDebugFromRawTable(lua, 1, 1);
        if (meshDebug->mesh.typeMe == util::TYPE_MESH_FONT)
        {
            meshDebug->mesh.deleteExtraInfo();
            meshDebug->mesh.extraInfo = newInfoFontFromLua(lua,2);
        }
        else
        {
            auto getTypeAsString = [] (const util::TYPE_MESH type) -> const char *
            {
                switch (type)
                {
                    case util::TYPE_MESH_3D         : return "3D";
                    case util::TYPE_MESH_USER       : return "USER";
                    case util::TYPE_MESH_SPRITE     : return "SPRITE";
                    case util::TYPE_MESH_TEXTURE    : return "TEXTURE";
                    case util::TYPE_MESH_UNKNOWN    : return "UNKNOWN";
                    case util::TYPE_MESH_SHAPE      : return "SHAPE";
                    case util::TYPE_MESH_PARTICLE   : return "PARTICLE";
                    case util::TYPE_MESH_TILE_MAP   : return "TILE_MAP";
                    default : return "UNKNOWN";
                }
            };
            return lua_error_debug(lua,"Not implemented setDetail for [%s]", getTypeAsString(meshDebug->mesh.typeMe));   
        }
        return 0;
    }

	int onSetMode_drawMeshDebugLua(lua_State *lua)
	{
        MESH_DEBUG_LUA *meshDebug     = getMeshDebugFromRawTable(lua, 1, 1);
		const char* s_mode_draw       = luaL_checkstring(lua, 2);
        const unsigned int  mode_draw = get_mode_draw_from_string(s_mode_draw,0xFFFFFFFF);
        if (mode_draw == 0xFFFFFFFF)
        {
			return lua_error_debug(lua,"Invalid mode draw: [%s] \n expected:[%s]",s_mode_draw ? s_mode_draw : "NULL",
				"TRIANGLES, TRIANGLE_STRIP, TRIANGLE_FAN, LINES, LINE_LOOP, LINE_STRIP, POINTS");
        }
        else
        {
            meshDebug->mesh.info_mode.mode_draw = mode_draw;
        }
        return 0;
    }

	int onSetMode_CullFaceMeshDebugLua(lua_State *lua)
	{
        MESH_DEBUG_LUA *meshDebug          = getMeshDebugFromRawTable(lua, 1, 1);
		const char* s_mode_cull_face       = luaL_checkstring(lua, 2);
        const unsigned int  mode_cull_face = get_mode_cull_face_from_string(s_mode_cull_face,0xFFFFFFFF);
        if (mode_cull_face == 0xFFFFFFFF)
        {
			return lua_error_debug(lua,"Invalid mode cull face: [%s] \n expected:[%s]",s_mode_cull_face ? s_mode_cull_face : "NULL",
				"FRONT, BACK, FRONT_AND_BACK");
        }
        else
        {
            meshDebug->mesh.info_mode.mode_cull_face = mode_cull_face;
        }
        return 0;
    }

	int onSetMode_FrontFaceMeshDebugLua(lua_State *lua)
	{
        MESH_DEBUG_LUA *meshDebug      = getMeshDebugFromRawTable(lua, 1, 1);
		const char* s_mode_front_face  = luaL_checkstring(lua, 2);
        const unsigned int  mode_front_face = get_mode_front_face_direction_from_string(s_mode_front_face,0xFFFFFFFF);
        if (mode_front_face == 0xFFFFFFFF)
        {
			return lua_error_debug(lua,"Invalid mode cull face: [%s] \n expected:[%s]",s_mode_front_face ? s_mode_front_face : "NULL",
				"CW, CCW");
        }
        else
        {
            meshDebug->mesh.info_mode.mode_front_face_direction = mode_front_face;
        }
        return 0;
    }

	int onGetMode_drawMeshDebugLua(lua_State *lua)
	{
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
		const char *  mode_draw   = get_mode_draw_from_uint(meshDebug->mesh.info_mode.mode_draw,"nil");
        lua_pushstring(lua,mode_draw);
        return 1;
    }
	
	int onGetMode_CullFaceMeshDebugLua(lua_State *lua)
	{
        MESH_DEBUG_LUA *meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
		const char *  mode_cull_face = get_mode_cull_face_from_uint(meshDebug->mesh.info_mode.mode_cull_face,"nil");
        lua_pushstring(lua,mode_cull_face);
        return 1;
    }

	int onGetMode_FrontFaceMeshDebugLua(lua_State *lua)
	{
        MESH_DEBUG_LUA *meshDebug     = getMeshDebugFromRawTable(lua, 1, 1);
		const char *  mode_front_face = get_mode_front_face_direction_from_uint(meshDebug->mesh.info_mode.mode_front_face_direction,"nil");
        lua_pushstring(lua,mode_front_face);
        return 1;
    }

    int onGetTotalFrameMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        lua_pushinteger(lua, meshDebug->mesh.buffer.size());
        return 1;
    }

    int onGetTotalSubsetMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *   meshDebug  = getMeshDebugFromRawTable(lua, 1, 1);
        const auto indexFrame = (unsigned int)luaL_checkinteger(lua, 2) - 1;
        if (indexFrame < (const unsigned int)meshDebug->mesh.buffer.size())
        {
            util::BUFFER_MESH_DEBUG *buffer = meshDebug->mesh.buffer[indexFrame];
            lua_pushinteger(lua, buffer->subset.size());
            return 1;
        }
        else
        {
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d)\n"
                            "indexFrame %d ",
                       meshDebug->mesh.buffer.size(), indexFrame + 1);
        }
    }

    int onGetTotalVertexMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *   meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const auto indexFrame  = (unsigned int)luaL_checkinteger(lua, 2) - 1;
        const auto indexSubset = (unsigned int)luaL_checkinteger(lua, 3) - 1;
        if (indexFrame < (const unsigned int)meshDebug->mesh.buffer.size() &&
            indexSubset < (const unsigned int)meshDebug->mesh.buffer[indexFrame]->subset.size())
        {
            util::BUFFER_MESH_DEBUG *buffer = meshDebug->mesh.buffer[indexFrame];
            util::SUBSET_DEBUG *     subset = buffer->subset[indexSubset];
            lua_pushinteger(lua, subset->vertexCount);
            return 1;
        }
        else
        {
            const int tSubset =
                indexFrame < meshDebug->mesh.buffer.size() ? meshDebug->mesh.buffer[indexFrame]->subset.size() : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       meshDebug->mesh.buffer.size(), tSubset, indexFrame + 1, indexSubset + 1);
        }
    }

    int onGetTotalIndexMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *   meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const auto indexFrame  = (unsigned int)luaL_checkinteger(lua, 2) - 1;
        const auto indexSubset = (unsigned int)luaL_checkinteger(lua, 3) - 1;
        if (indexFrame < (const unsigned int)meshDebug->mesh.buffer.size() &&
            indexSubset < (const unsigned int)meshDebug->mesh.buffer[indexFrame]->subset.size())
        {
            util::BUFFER_MESH_DEBUG *buffer = meshDebug->mesh.buffer[indexFrame];
            util::SUBSET_DEBUG *     subset = buffer->subset[indexSubset];
            lua_pushinteger(lua, subset->indexCount);
            return 1;
        }
        else
        {
            const int tSubset =
                indexFrame < meshDebug->mesh.buffer.size() ? meshDebug->mesh.buffer[indexFrame]->subset.size() : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       meshDebug->mesh.buffer.size(), tSubset, indexFrame + 1, indexSubset + 1);
        }
    }

    int onIsIndexBufferMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        if (meshDebug->mesh.buffer.size() && meshDebug->mesh.buffer[0]->indexBuffer)
            lua_pushboolean(lua, 1);
        else
            lua_pushboolean(lua, 0);
        return 1;
    }

    int onGetVertexMeshDebugLua(lua_State *lua)
    {
        const int          top            = lua_gettop(lua);
        MESH_DEBUG_LUA *   meshDebug      = getMeshDebugFromRawTable(lua, 1, 1);
        const unsigned int indexFrame     = top > 1 ? (unsigned int)luaL_checkinteger(lua, 2) - 1 : 0;
        const unsigned int indexSubset    = top > 2 ? (unsigned int)luaL_checkinteger(lua, 3) - 1 : 0;
        const unsigned int indexVertex    = top > 3 ? (unsigned int)luaL_checkinteger(lua, 4) - 1 : 0;
        unsigned int       totalVertexRet = top > 4 ? (unsigned int)luaL_checkinteger(lua, 5) : 1;
        if (indexFrame < (const unsigned int)meshDebug->mesh.buffer.size() &&
            indexSubset < (const unsigned int)meshDebug->mesh.buffer[indexFrame]->subset.size())
        {
            util::BUFFER_MESH_DEBUG *buffer    = meshDebug->mesh.buffer[indexFrame];
            util::SUBSET_DEBUG *     subset    = buffer->subset[indexSubset];
            auto *                   pPosition = reinterpret_cast<VEC3 *>(buffer->position);
            auto *                   pNormal   = reinterpret_cast<VEC3 *>(buffer->normal);
            auto *                   pUv       = reinterpret_cast<VEC2 *>(buffer->uv);
            if (indexVertex < (unsigned int)subset->vertexCount)
            {
                if (totalVertexRet > 1)
                {
                    lua_newtable(lua);
                    if ((int)totalVertexRet > subset->vertexCount)
                        totalVertexRet = subset->vertexCount;
                }
                else
                {
                    totalVertexRet = 1;
                }
                for (unsigned int ii = 0; ii < totalVertexRet; ++ii)
                {
                    lua_newtable(lua);
                    const unsigned int indexRaw = indexVertex + subset->vertexStart + ii;
                    lua_pushnumber(lua, pPosition[indexRaw].x);
                    lua_setfield(lua, -2, "x");

                    lua_pushnumber(lua, pPosition[indexRaw].y);
                    lua_setfield(lua, -2, "y");

                    lua_pushnumber(lua, pPosition[indexRaw].z);
                    lua_setfield(lua, -2, "z");

                    lua_pushnumber(lua, pNormal[indexRaw].x);
                    lua_setfield(lua, -2, "nx");

                    lua_pushnumber(lua, pNormal[indexRaw].y);
                    lua_setfield(lua, -2, "ny");

                    lua_pushnumber(lua, pNormal[indexRaw].z);
                    lua_setfield(lua, -2, "nz");

                    lua_pushnumber(lua, pUv[indexRaw].x);
                    lua_setfield(lua, -2, "u");

                    lua_pushnumber(lua, pUv[indexRaw].y);
                    lua_setfield(lua, -2, "v");

                    if (totalVertexRet > 1)
                    {
                        lua_rawseti(lua, -2, (ii + 1));
                    }
                }
                return 1;
            }
            else
            {
                return lua_error_debug(lua, "\nOut of bound[indexVertex %d/%d)\n", indexVertex, subset->vertexCount);
            }
        }
        else
        {
            const int tSubset =
                indexFrame < meshDebug->mesh.buffer.size() ? meshDebug->mesh.buffer[indexFrame]->subset.size() : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       meshDebug->mesh.buffer.size(), tSubset, indexFrame, indexSubset);
        }
    }

    int onSetVertexMeshDebugLua(lua_State *lua)
    {
        const int          top         = lua_gettop(lua);
        MESH_DEBUG_LUA *   meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const unsigned int indexFrame  = top > 1 ? (unsigned int)luaL_checkinteger(lua, 2) - 1 : 0;
        const unsigned int indexSubset = top > 2 ? (unsigned int)luaL_checkinteger(lua, 3) - 1 : 0;
        const unsigned int indexVertex = top > 3 ? (unsigned int)luaL_checkinteger(lua, 4) - 1 : 0;
        const int          hasTable    = top > 4 ? lua_type(lua, 5) : 0;
        if (hasTable == LUA_TTABLE && indexFrame < static_cast<const unsigned int>(meshDebug->mesh.buffer.size()) &&
            indexSubset < static_cast<const unsigned int>(meshDebug->mesh.buffer[indexFrame]->subset.size()))
        {
            util::BUFFER_MESH_DEBUG *buffer    = meshDebug->mesh.buffer[indexFrame];
            util::SUBSET_DEBUG *     subset    = buffer->subset[indexSubset];
            auto *                   pPosition = reinterpret_cast<VEC3 *>(buffer->position);
            auto *                   pNormal   = reinterpret_cast<VEC3 *>(buffer->normal);
            auto *                   pUv       = reinterpret_cast<VEC2 *>(buffer->uv);
            if (indexVertex < static_cast<unsigned int>(subset->vertexCount))
            {
                int lenTable = lua_rawlen(lua, 5);
                if (lenTable <= 0) // hasn't array
                {
                    constexpr int      indexTable = 5;
                    const unsigned int indexRaw   = indexVertex + subset->vertexStart;
                    getFieldPrimaryFromTable(lua, indexTable, "x", LUA_TNUMBER, &pPosition[indexRaw].x);
                    getFieldPrimaryFromTable(lua, indexTable, "y", LUA_TNUMBER, &pPosition[indexRaw].y);
                    getFieldPrimaryFromTable(lua, indexTable, "z", LUA_TNUMBER, &pPosition[indexRaw].z);

                    getFieldPrimaryFromTable(lua, indexTable, "nx", LUA_TNUMBER, &pNormal[indexRaw].x);
                    getFieldPrimaryFromTable(lua, indexTable, "ny", LUA_TNUMBER, &pNormal[indexRaw].y);
                    getFieldPrimaryFromTable(lua, indexTable, "nz", LUA_TNUMBER, &pNormal[indexRaw].z);

                    getFieldPrimaryFromTable(lua, indexTable, "u", LUA_TNUMBER, &pUv[indexRaw].x);
                    getFieldPrimaryFromTable(lua, indexTable, "v", LUA_TNUMBER, &pUv[indexRaw].y);
                }
                else
                {
                    if (lenTable > subset->vertexCount)
                        lenTable = subset->vertexCount;
                    for (int ii = 0; ii < lenTable; ++ii)
                    {
                        constexpr int      indexTable = 6;
                        const unsigned int indexRaw   = indexVertex + subset->vertexStart + ii;
                        lua_rawgeti(lua, 5, (ii + 1));
                        getFieldPrimaryFromTable(lua, indexTable, "x", LUA_TNUMBER, &pPosition[indexRaw].x);
                        getFieldPrimaryFromTable(lua, indexTable, "y", LUA_TNUMBER, &pPosition[indexRaw].y);
                        getFieldPrimaryFromTable(lua, indexTable, "z", LUA_TNUMBER, &pPosition[indexRaw].z);

                        getFieldPrimaryFromTable(lua, indexTable, "nx", LUA_TNUMBER, &pNormal[indexRaw].x);
                        getFieldPrimaryFromTable(lua, indexTable, "ny", LUA_TNUMBER, &pNormal[indexRaw].y);
                        getFieldPrimaryFromTable(lua, indexTable, "nz", LUA_TNUMBER, &pNormal[indexRaw].z);

                        getFieldPrimaryFromTable(lua, indexTable, "u", LUA_TNUMBER, &pUv[indexRaw].x);
                        getFieldPrimaryFromTable(lua, indexTable, "v", LUA_TNUMBER, &pUv[indexRaw].y);
                        lua_pop(lua, 1);
                    }
                }
                return 0;
            }
            else
            {
                return lua_error_debug(lua, "\nindexVertex [%d] > vertexCount [%d]", indexVertex, subset->vertexCount);
            }
        }
        else
        {
            const int tSubset =
                indexFrame < meshDebug->mesh.buffer.size() ? meshDebug->mesh.buffer[indexFrame]->subset.size() : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       meshDebug->mesh.buffer.size(), tSubset, indexFrame + 1, indexSubset + 1);
        }
    }

    int onAddVertexMeshDebugLua(lua_State *lua)
    {
        const int          top         = lua_gettop(lua);
        MESH_DEBUG_LUA *   meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const unsigned int indexFrame  = top > 1 ? (unsigned int)luaL_checkinteger(lua, 2) - 1 : 0;
        const unsigned int indexSubset = top > 2 ? (unsigned int)luaL_checkinteger(lua, 3) - 1 : 0;
        const int          type4       = top > 3 ? lua_type(lua, 4) : LUA_TNIL;
        if (indexFrame < (const unsigned int)meshDebug->mesh.buffer.size() &&
            indexSubset < (const unsigned int)meshDebug->mesh.buffer[indexFrame]->subset.size())
        {
            if (type4 == LUA_TNIL)
            {
                if (meshDebug->mesh.addVertex(indexFrame, indexSubset, 1))
                {
                    lua_pushboolean(lua, 1);
                    return 1;
                }
                return lua_error_debug(lua, "\nError on add vertex[indexFrame,indexSubset,vertex = {x,y,z,nx,ny,nz,u,v} | totalVertex]");
            }
            else if (type4 == LUA_TNUMBER)
            {
                const auto totalVertex = (unsigned int)luaL_checkinteger(lua, 4);
                if (meshDebug->mesh.addVertex(indexFrame, indexSubset, totalVertex))
                {
                    lua_pushboolean(lua, 1);
                    return 1;
                }
                return lua_error_debug(lua, "\nError on add vertex[indexFrame,indexSubset,vertex = {x,y,z,nx,ny,nz,u,v} | totalVertex]");
            }
            else if (type4 == LUA_TTABLE)
            {
                int                      lenTable = lua_rawlen(lua, 4);
                util::BUFFER_MESH_DEBUG *buffer   = meshDebug->mesh.buffer[indexFrame];
                util::SUBSET_DEBUG *     subset   = buffer->subset[indexSubset];

                const int vertexCount = subset->vertexCount; // before add vertex
                if (lenTable <= 0)                           // hasn't array
                {
                    if (meshDebug->mesh.addVertex(indexFrame, indexSubset, 1))
                    {
                        auto *             pPosition  = reinterpret_cast<VEC3 *>(buffer->position);
                        auto *             pNormal    = reinterpret_cast<VEC3 *>(buffer->normal);
                        auto *             pUv        = reinterpret_cast<VEC2 *>(buffer->uv);
                        constexpr int      indexTable = 4;
                        const unsigned int indexRaw   = subset->vertexStart + vertexCount;
                        getFieldPrimaryFromTable(lua, indexTable, "x", LUA_TNUMBER, &pPosition[indexRaw].x);
                        getFieldPrimaryFromTable(lua, indexTable, "y", LUA_TNUMBER, &pPosition[indexRaw].y);
                        getFieldPrimaryFromTable(lua, indexTable, "z", LUA_TNUMBER, &pPosition[indexRaw].z);

                        getFieldPrimaryFromTable(lua, indexTable, "nx", LUA_TNUMBER, &pNormal[indexRaw].x);
                        getFieldPrimaryFromTable(lua, indexTable, "ny", LUA_TNUMBER, &pNormal[indexRaw].y);
                        getFieldPrimaryFromTable(lua, indexTable, "nz", LUA_TNUMBER, &pNormal[indexRaw].z);

                        getFieldPrimaryFromTable(lua, indexTable, "u", LUA_TNUMBER, &pUv[indexRaw].x);
                        getFieldPrimaryFromTable(lua, indexTable, "v", LUA_TNUMBER, &pUv[indexRaw].y);
                        lua_pushboolean(lua, 1);
                        return 1;
                    }
                    return lua_error_debug(lua,
                               "\nError on add vertex[indexFrame,indexSubset,vertex = {x,y,z,nx,ny,nz,u,v} | totalVertex]");
                }
                else if (meshDebug->mesh.addVertex(indexFrame, indexSubset, lenTable))
                {
                    auto *pPosition = reinterpret_cast<VEC3 *>(buffer->position);
                    auto *pNormal   = reinterpret_cast<VEC3 *>(buffer->normal);
                    auto *pUv       = reinterpret_cast<VEC2 *>(buffer->uv);
                    for (int ii = 0; ii < lenTable; ++ii)
                    {
                        constexpr int indexTable = 5;
                        const unsigned int indexRaw   = subset->vertexStart + vertexCount + ii;
                        lua_rawgeti(lua, 4, (ii + 1));
                        getFieldPrimaryFromTable(lua, indexTable, "x", LUA_TNUMBER, &pPosition[indexRaw].x);
                        getFieldPrimaryFromTable(lua, indexTable, "y", LUA_TNUMBER, &pPosition[indexRaw].y);
                        getFieldPrimaryFromTable(lua, indexTable, "z", LUA_TNUMBER, &pPosition[indexRaw].z);

                        getFieldPrimaryFromTable(lua, indexTable, "nx", LUA_TNUMBER, &pNormal[indexRaw].x);
                        getFieldPrimaryFromTable(lua, indexTable, "ny", LUA_TNUMBER, &pNormal[indexRaw].y);
                        getFieldPrimaryFromTable(lua, indexTable, "nz", LUA_TNUMBER, &pNormal[indexRaw].z);

                        getFieldPrimaryFromTable(lua, indexTable, "u", LUA_TNUMBER, &pUv[indexRaw].x);
                        getFieldPrimaryFromTable(lua, indexTable, "v", LUA_TNUMBER, &pUv[indexRaw].y);
                        lua_pop(lua, 1);
                    }
                    lua_pushboolean(lua, 1);
                    return 1;
                }
            }
            return lua_error_debug(lua, "\nError on add vertex[indexFrame,indexSubset,vertex = {x,y,z,nx,ny,nz,u,v} | totalVertex]");
        }
        else
        {
            const int tSubset =
                indexFrame < meshDebug->mesh.buffer.size() ? meshDebug->mesh.buffer[indexFrame]->subset.size() : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       meshDebug->mesh.buffer.size(), tSubset, indexFrame+1, indexSubset+1);
        }
    }

    int onGetIndexMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *   meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const unsigned int indexFrame  = (unsigned int)luaL_checkinteger(lua, 2) - 1;
        const unsigned int indexSubset = (unsigned int)luaL_checkinteger(lua, 3) - 1;
        if (indexFrame < (const unsigned int)meshDebug->mesh.buffer.size() &&
            indexSubset < (const unsigned int)meshDebug->mesh.buffer[indexFrame]->subset.size())
        {
            util::BUFFER_MESH_DEBUG *buffer = meshDebug->mesh.buffer[indexFrame];
            if (buffer->indexBuffer)
            {
                util::SUBSET_DEBUG *subset = buffer->subset[indexSubset];
                lua_newtable(lua);
                const unsigned int s = (subset->indexCount + subset->indexStart);
                for (unsigned int i = subset->indexStart, j = 1; i < s; i++, ++j)
                {
                    const int indexRaw = buffer->indexBuffer[i] - subset->vertexStart;
                    lua_pushinteger(lua, indexRaw+1);
                    lua_rawseti(lua, -2, j);
                }
                return 1;
            }
            else
            {
                lua_pushnil(lua);
                return 1;
            }
        }
        else
        {
            const int tSubset =
                indexFrame < meshDebug->mesh.buffer.size() ? meshDebug->mesh.buffer[indexFrame]->subset.size() : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       meshDebug->mesh.buffer.size(), tSubset, indexFrame + 1, indexSubset + 1);
        }
    }

    int onAddIndexMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *   meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const auto indexFrame  = (unsigned int)luaL_checkinteger(lua, 2) - 1;
        const auto indexSubset = (unsigned int)luaL_checkinteger(lua, 3) - 1;
        const int          type        = lua_type(lua, 4);
        const unsigned int sTableIndex = type == LUA_TTABLE ? lua_rawlen(lua, 4) : 0;
        if (type != LUA_TTABLE)
        {
            return lua_error_debug(lua, "Expected table of index. current is not a table");
        }
        if (sTableIndex == 0)
        {
            return lua_error_debug(lua, "Expected table of full index. current len table[0] empty");
        }
        else
        {
            std::vector<unsigned short int> pIndex(sTableIndex);
            getArrayFromTable(lua, 4, const_cast<unsigned short int*>(pIndex.data()), sTableIndex);
            char strErrorOut[255] = "";
			for (unsigned int i=0; i < pIndex.size(); ++i)
			{
				pIndex[i] = pIndex[i] - 1;
			}
            if (!meshDebug->mesh.addIndex(indexFrame, indexSubset, pIndex.data(), sTableIndex, strErrorOut))
            {
                return lua_error_debug(lua, strErrorOut);
            }
            else
            {
                lua_pushboolean(lua, 1);
                return 1;
            }
        }
    }

    int onGetTextureNameMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *   meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const auto indexFrame  = (unsigned int)luaL_checkinteger(lua, 2) - 1;
        const auto indexSubset = (unsigned int)luaL_checkinteger(lua, 3) - 1;
        if (indexFrame < (const unsigned int)meshDebug->mesh.buffer.size() &&
            indexSubset < (const unsigned int)meshDebug->mesh.buffer[indexFrame]->subset.size())
        {
            util::BUFFER_MESH_DEBUG *buffer = meshDebug->mesh.buffer[indexFrame];
            util::SUBSET_DEBUG *     subset = buffer->subset[indexSubset];
            if (subset->texture.size())
                lua_pushstring(lua, subset->texture.c_str());
            else
                lua_pushnil(lua);
            return 1;
        }
        else
        {
            const int tSubset =
                indexFrame < meshDebug->mesh.buffer.size() ? meshDebug->mesh.buffer[indexFrame]->subset.size() : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       meshDebug->mesh.buffer.size(), tSubset, indexFrame +1, indexSubset +1);
        }
    }

    int onSetTextureNameMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *   meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const auto indexFrame  = (unsigned int)luaL_checkinteger(lua, 2) - 1;
        const auto indexSubset = (unsigned int)luaL_checkinteger(lua, 3) - 1;
        const char *       fileName    = lua_type(lua, 4) == LUA_TSTRING ? luaL_checkstring(lua, 4) : nullptr;
        if (indexFrame < (const unsigned int)meshDebug->mesh.buffer.size() &&
            indexSubset < (const unsigned int)meshDebug->mesh.buffer[indexFrame]->subset.size())
        {
            util::BUFFER_MESH_DEBUG *buffer = meshDebug->mesh.buffer[indexFrame];
            util::SUBSET_DEBUG *     subset = buffer->subset[indexSubset];
            if (fileName && strlen(fileName))
            {
                subset->texture = fileName;
                util::addPath(fileName);
            }
            else
            {
                subset->texture.clear();
            }
            lua_pushboolean(lua, 1);
            return 1;
        }
        else
        {
            const int tSubset =
                indexFrame < meshDebug->mesh.buffer.size() ? meshDebug->mesh.buffer[indexFrame]->subset.size() : 0;
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       meshDebug->mesh.buffer.size(), tSubset, indexFrame + 1, indexSubset + 1);
        }
    }

    int onAddFrameDebugLua(lua_State *lua)
    {
        const int          top       = lua_gettop(lua);
        MESH_DEBUG_LUA *   meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const unsigned int stride    = top > 1 ? (unsigned int)luaL_checkinteger(lua, 2) : 3;
        unsigned int       ret       = meshDebug->mesh.addBuffer(stride);
        lua_pushinteger(lua, ret);
        return 1;
    }

    int onAddSubsetDebugLua(lua_State *lua)
    {
        const int          top       = lua_gettop(lua);
        MESH_DEBUG_LUA *   meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const unsigned int indexFrame = top > 1 ? (unsigned int)luaL_checkinteger(lua, 2) - 1 : meshDebug->mesh.buffer.size() - 1;
        unsigned int ret = meshDebug->mesh.addSubset(indexFrame);
        lua_pushinteger(lua, ret);
        return 1;
    }

    int onAddAnimationDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug        = getMeshDebugFromRawTable(lua, 1, 1);
        const char *    nameAnimation    = luaL_checkstring(lua, 2);
        const int       initialFrame     = luaL_checkinteger(lua, 3)-1;
        const int       finalFrame       = luaL_checkinteger(lua, 4)-1;
        const float     timeBetweenFrame = luaL_checknumber(lua, 5);
        const int       typeAnimation    = luaL_checkinteger(lua, 6);
        char            errorOut[255]    = "";
        const int       ret              = meshDebug->mesh.addAnimation(nameAnimation, 
                                                                        initialFrame, 
                                                                        finalFrame, 
                                                                        timeBetweenFrame, 
                                                                        typeAnimation, 
                                                                        errorOut);
        if (ret == 0)
            return lua_error_debug(lua, errorOut);
        lua_pushinteger(lua, ret);
        return 1;
    }

    int onCentralizeMeshDebugLua(lua_State *lua)
    {
        const int       top         = lua_gettop(lua);
        MESH_DEBUG_LUA *meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const int       indexFrame  = top > 1 ? luaL_checkinteger(lua, 2) - 1 : -1;
        const int       indexSubset = top > 2 ? luaL_checkinteger(lua, 3) - 1 : -1;
        meshDebug->mesh.centralizeFrame(indexFrame, indexSubset);
        return 0;
    }

    int onCheckMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug     = getMeshDebugFromRawTable(lua, 1, 1);
        char            strError[255] = "";
        bool            ret           = meshDebug->mesh.check(strError,sizeof(strError)-1);
        lua_pushboolean(lua, ret ? 1 : 0);
        lua_pushstring(lua, strError);
        return 2;
    }

    int onSetStrideMeshDebugLua(lua_State *lua)
    {
        const int       top        = lua_gettop(lua);
        MESH_DEBUG_LUA *meshDebug  = getMeshDebugFromRawTable(lua, 1, 1);
        const int       stride     = luaL_checkinteger(lua, 2);
        const int       indexFrame = top > 2 ? luaL_checkinteger(lua, 3) -1 : -1;
        if (stride != 2 && stride != 3)
            return lua_error_debug(lua, "Stride must be 3 or 2");
        if (indexFrame < 0)
        {
            for (auto bufferCurrent : meshDebug->mesh.buffer)
            {
                bufferCurrent->headerFrame.stride      = stride;
            }
        }
        else if (indexFrame < (int)meshDebug->mesh.buffer.size())
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent = meshDebug->mesh.buffer[indexFrame];
            bufferCurrent->headerFrame.stride      = stride;
        }
        else
        {
            return lua_error_debug(lua, "Index frame invalid [%d/%d]",top > 2 ? indexFrame + 1 : indexFrame, meshDebug->mesh.buffer.size());
        }
        return 0;
    }

    int onEnableNormalsMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug               = getMeshDebugFromRawTable(lua, 1, 1);
        const int       enable                  = lua_toboolean(lua, 2);
        meshDebug->mesh.headerMesh.hasNorText[0] = enable ? 1 : 0;
        return 0;
    }

    int onEnableUvMeshDebugLua(lua_State *lua)
    {
        const int       top              = lua_gettop(lua);
        MESH_DEBUG_LUA *meshDebug        = getMeshDebugFromRawTable(lua, 1, 1);
        const int       enable           = lua_toboolean(lua, 2);
        const int       enableFirstFrame = top > 2 ? lua_toboolean(lua, 3) : 0;
        if (enable)
        {
            if (enableFirstFrame)
                meshDebug->mesh.headerMesh.hasNorText[1] = 2;
            else
                meshDebug->mesh.headerMesh.hasNorText[1] = 1;
        }
        else
        {
            meshDebug->mesh.headerMesh.hasNorText[1] = 0;
        }
        return 0;
    }

    void fillEffect(const EFFECT_SHADER* fx,const char* textureStage2,util::INFO_SHADER_DATA** dataInfoShader)
    {
        if(fx->ptrCurrentShader)
        {
            const unsigned int sTexStage2    = textureStage2 ? strlen(textureStage2): 0;
            const unsigned int sizeFileName  = fx->ptrCurrentShader->fileName.size();
            const unsigned int totalVar      = fx->ptrCurrentShader->getTotalVar();
            const unsigned int sizeArrayVarInBytes = totalVar * 4;
            auto dataInfo = new util::INFO_SHADER_DATA(sizeArrayVarInBytes, 
                (short)(sizeFileName ? sizeFileName + 1 : 0),
                (short)(sTexStage2   ? sTexStage2   + 1 : 0));
            *dataInfoShader     = (dataInfo);
            dataInfo->typeAnimation    = fx->typeAnim;
            dataInfo->timeAnimation    = fx->timeAnimation;
            if(sizeFileName)
                strncpy(dataInfo->fileNameShader,fx->ptrCurrentShader->fileName.c_str(),sizeFileName + 1);
            if(sTexStage2)
                strncpy(dataInfo->fileNameTextureStage2,textureStage2,sTexStage2 + 1);
            for(unsigned int k=0; k < totalVar; ++k)
            {
                const int index       = k * 4;
                VAR_SHADER* var       = fx->ptrCurrentShader->getVar(k);
                memcpy(&dataInfo->min[index],var->min,sizeof(var->min));
                memcpy(&dataInfo->max[index],var->max,sizeof(var->max));
                dataInfo->typeVars[k] = var->typeVar;
            }
        }
    }

    int onCopyAnimationsFromMeshLua(lua_State *lua)
    {
        const int       top              = lua_gettop(lua);
        MESH_DEBUG_LUA *meshDebug        = getMeshDebugFromRawTable(lua, 1, 1);
        if(top < 2)
        {
            return lua_error_debug(lua, "expected mesh with shader modified..");
        }
        else
        {
            RENDERIZABLE *renderizable          = nullptr;
            ANIMATION_MANAGER *animations       = getAnimationManagerFromRawTable(lua,1,2,&renderizable);
            auto* shaderStep                    = renderizable->getFx();
            if(verifyDynamicCast(lua, shaderStep, __LINE__, __FILE__) != 1)
                return 0;
            if(animations->getTotalAnimation() == 0)
            {
                PRINT_IF_DEBUG("there is no animation in the mesh!");
            }
            if (renderizable->typeClass == TYPE_CLASS_PARTICLE)
            {
                auto* particle = static_cast<PARTICLE*>(renderizable);
                if (particle)
                {
					auto* lsParticleInfo = new std::vector<util::STAGE_PARTICLE*>();
					meshDebug->mesh.deleteExtraInfo();
					meshDebug->mesh.extraInfo = lsParticleInfo;
                    for (unsigned int i = 0; i < particle->getTotalStage(); ++i)
                    {
                        util::STAGE_PARTICLE* stage = particle->getStageParticle(i);
                        auto  nStage = new util::STAGE_PARTICLE(stage);
                        lsParticleInfo->push_back(nStage);
                    }
                }
            }
            meshDebug->mesh.lsBlendOperation.clear();
            meshDebug->mesh.lsBlendOperation.resize(animations->getTotalAnimation());

            for (unsigned int i=0; i < animations->getTotalAnimation(); ++i)
            {
                ANIMATION* anim             = animations->getAnimation(i);
                const char* textureStage2   = anim->fx.textureOverrideStage2 ? anim->fx.textureOverrideStage2->getFileNameTexture() : nullptr;
                
                util::INFO_ANIMATION::INFO_HEADER_ANIM* infoHead = nullptr;
                if(i < meshDebug->mesh.infoAnimation.lsHeaderAnim.size())
                {
                    infoHead = meshDebug->mesh.infoAnimation.lsHeaderAnim[i];
                }
                else if(i == meshDebug->mesh.infoAnimation.lsHeaderAnim.size())
                {
                    infoHead = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
                    auto headerAnim = new util::HEADER_ANIMATION();
                    meshDebug->mesh.infoAnimation.lsHeaderAnim.push_back(infoHead);
                    infoHead->headerAnim = headerAnim;
                    strncpy(headerAnim->nameAnimation,anim->nameAnimation,sizeof(headerAnim->nameAnimation));
                    headerAnim->typeAnimation = anim->type;
                    headerAnim->timeBetweenFrame = anim->intervalChangeFrame;
                }
                if(infoHead)
                {
                    if(infoHead->effetcShader)
                        delete infoHead->effetcShader;
                    infoHead->effetcShader = nullptr;
                    util::HEADER_ANIMATION *headerAnim  = infoHead->headerAnim;
					headerAnim->hasShaderEffect = 1;
                    headerAnim->blendState        = static_cast<unsigned short int>(anim->blendState);
                    meshDebug->mesh.lsBlendOperation[i] = anim->fx.blendOperation;

                    if(anim->fx.fxPS->ptrCurrentShader)
                    {
                        infoHead->effetcShader = new util::INFO_FX();
                        infoHead->effetcShader->blendOperation = anim->fx.blendOperation;
                        fillEffect(anim->fx.fxPS,textureStage2,&infoHead->effetcShader->dataPS);
                    }
                    if(anim->fx.fxVS->ptrCurrentShader)
                    {
                        if(infoHead->effetcShader == nullptr)
                        {
                            infoHead->effetcShader = new util::INFO_FX();
                            infoHead->effetcShader->blendOperation = anim->fx.blendOperation;
                        }
                        fillEffect(anim->fx.fxVS,textureStage2,&infoHead->effetcShader->dataVS);
                    }
                }
            }
        }
        lua_pushboolean(lua,1);
        return 1;
    }


    int onUpdateAnimationDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug        = getMeshDebugFromRawTable(lua, 1, 1);
        const int       index            = luaL_checkinteger(lua, 2)-1;
        const char *    nameAnimation    = luaL_checkstring(lua, 3);
        const int       initialFrame     = luaL_checkinteger(lua, 4)-1;
        const int       finalFrame       = luaL_checkinteger(lua, 5)-1;
        const float     timeBetweenFrame = luaL_checknumber(lua, 6);
        const int       typeAnimation    = luaL_checkinteger(lua, 7);
        char            errorOut[255]    = "";

        const bool       ret              = meshDebug->mesh.updateAnimation(index,nameAnimation, 
                                                                        initialFrame, 
                                                                        finalFrame, 
                                                                        timeBetweenFrame, 
                                                                        typeAnimation, 
                                                                        errorOut,
                                                                        sizeof(errorOut)-1);
        if (ret == false)
            lua_print_line(lua,TYPE_LOG_ERROR,"%s", errorOut);
        lua_pushboolean(lua,ret ? 1 : 0);
        return 1;
    }

    
    int onGetDetailAnimationDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug        = getMeshDebugFromRawTable(lua, 1, 1);
        const int       index            = luaL_checkinteger(lua, 2)-1;

        const util::INFO_ANIMATION::INFO_HEADER_ANIM * head = meshDebug->mesh.getAnim(index);
        if (head == nullptr || head->headerAnim == nullptr)
        {
            lua_print_line(lua,TYPE_LOG_ERROR,"invalid index or no animation");
            lua_pushnil(lua);
            return 1;
        }
        lua_pushstring(lua,head->headerAnim->nameAnimation);
        lua_pushnumber(lua,static_cast<lua_Number>(head->headerAnim->initialFrame+1));
        lua_pushnumber(lua,static_cast<lua_Number>(head->headerAnim->finalFrame+1));
        lua_pushnumber(lua,head->headerAnim->timeBetweenFrame);
        lua_pushnumber(lua,static_cast<lua_Number>(head->headerAnim->typeAnimation));
        return 5;
    }

    int onNewIndexMeshDebug(lua_State *lua) // escrita
    {
        /*
        **********************************
                Estado da pilha
                -3|    table |1
                -2|   string |2
                -1|   number |3
        **********************************
        */
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const char *    what      = luaL_checkstring(lua, 2);
        return setVariable(lua, meshDebug->lsDynamicVar, what);
    }

    int onIndexMeshDebug(lua_State *lua) // leitura
    {
        /*
        **********************************
                Estado da pilha
                -2|    table |1
                -1|   string |2
        **********************************
        */
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const char *    what      = luaL_checkstring(lua, 2);
        return getVariable(lua, meshDebug->lsDynamicVar, what);
    }

	static int onGetStaticExtensionLua(lua_State *lua)
	{
		const char *    file      = luaL_checkstring(lua, 2);
		std::string ext = mbm::MESH_MBM_DEBUG::getExtension(file);
		lua_pushstring(lua,ext.c_str());
		return 1;
	}

    int onNewMeshDebugLua(lua_State *lua)
    {
        luaL_Reg regFrameMeshMethods[] = {{"load", onLoadMeshDebugLua},
                                          {"save", onSaveMeshDebugLua},
                                          {"setType", onSetTypeMeshDebugLua},
                                          {"getType", onGetTypeMeshDebugLua},
			                              {"setModeDraw", onSetMode_drawMeshDebugLua},
			                              {"getModeDraw", onGetMode_drawMeshDebugLua},
										  {"setModeCullFace", onSetMode_CullFaceMeshDebugLua},
			                              {"getModeCullFace", onGetMode_CullFaceMeshDebugLua},
										  {"setModeFrontFace", onSetMode_FrontFaceMeshDebugLua},
			                              {"getModeFrontFace", onGetMode_FrontFaceMeshDebugLua},
                                          {"setPhysics", onSetPhysicsMeshDebugLua},
                                          {"getPhysics", onGetPhysicsMeshDebugLua},
                                          {"getTotalFrame", onGetTotalFrameMeshDebugLua},
                                          {"getTotalSubset", onGetTotalSubsetMeshDebugLua},
                                          {"getTotalVertex", onGetTotalVertexMeshDebugLua},
                                          {"getTotalIndex", onGetTotalIndexMeshDebugLua},
                                          {"isIndexBuffer", onIsIndexBufferMeshDebugLua},
                                          {"getVertex", onGetVertexMeshDebugLua},
                                          {"setVertex", onSetVertexMeshDebugLua},
                                          {"addVertex", onAddVertexMeshDebugLua},
                                          {"getIndex", onGetIndexMeshDebugLua},
                                          {"addIndex", onAddIndexMeshDebugLua},
                                          {"getTexture", onGetTextureNameMeshDebugLua},
                                          {"setTexture", onSetTextureNameMeshDebugLua},
                                          {"addFrame", onAddFrameDebugLua},
                                          {"addSubSet", onAddSubsetDebugLua},
                                          {"addAnim", onAddAnimationDebugLua},
                                          {"centralize", onCentralizeMeshDebugLua},
                                          {"check", onCheckMeshDebugLua},
                                          {"setStride", onSetStrideMeshDebugLua},
                                          {"enableNormal", onEnableNormalsMeshDebugLua},
                                          {"enableUv", onEnableUvMeshDebugLua},
                                          {"copyAnimationsFromMesh", onCopyAnimationsFromMeshLua},
                                          {"updateAnim", onUpdateAnimationDebugLua},
                                          {"getAnim", onGetDetailAnimationDebugLua},
										  {"getExt", onGetStaticExtensionLua},
                                          {"setDetail", onSetDetailLua},
                                          {nullptr, nullptr}};
        lua_settop(lua, 0);
        luaL_newlib(lua, regFrameMeshMethods);
        luaL_getmetatable(lua, "_mbmMeshDebug");
        lua_setmetatable(lua, -2);

        auto **udata     = static_cast<MESH_DEBUG_LUA **>(lua_newuserdata(lua, sizeof(MESH_DEBUG_LUA *)));
        auto  meshDebug = new MESH_DEBUG_LUA();
        *udata                     = meshDebug;

        /* trick to ensure that we will receive the expected metatable type expected metatable type. */
        const char* __userdata_name = getUserTypeAsString(L_USER_TYPE_MESH_DEBUG);
        luaL_getmetatable(lua,__userdata_name);
        lua_setmetatable(lua,-2);
        /* end trick */

        lua_rawseti(lua, -2, 1);
        return 1;
    }

    int onDestroyMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
    #if DEBUG_FREE_LUA
        static int  num      = 1;
        const char *fileName = meshDebug->getFileName();
        PRINT_IF_DEBUG( "free mesh-debug [%s] [%d]\n", fileName ? fileName : "NULL", num++);
    #endif
        delete meshDebug;
        return 0;
    }

    void registerClassAuto(lua_State *lua);

    void registerClassMeshDebug(lua_State *lua)
    {
        luaL_Reg regFrameMeshMethods[] = {{"new", onNewMeshDebugLua},
                                          {"__newindex", onNewIndexMeshDebug},
                                          {"__index", onIndexMeshDebug},
                                          {"__gc", onDestroyMeshDebugLua},
                                          {"getInfo", onGetInfoMeshDebugLua},
                                          {"getType", onGetTypeMeshDebugLua},
										  {"getExt", onGetStaticExtensionLua},
                                          {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmMeshDebug");
        luaL_setfuncs(lua, regFrameMeshMethods, 0);
        lua_setglobal(lua, "meshDebug");
        lua_settop(lua,0);
        registerClassAuto(lua);
    }

    int onNewAutoLua(lua_State *lua)
    {
        MESH_MBM_DEBUG  meshTmp;
        const char* fileName            = luaL_checkstring(lua,2);//auto sFileName (2ds|2dw|3d) x,y,z
        const util::TYPE_MESH   typeOut = meshTmp.getType(fileName);
        lua_remove(lua,1);//auto sFileName (2ds|2dw|3d) x,y,z ----> sFileName (2ds|2dw|3d) x,y,z
        
        switch (typeOut)
        {
            case util::TYPE_MESH_3D:        
            { 
                onNewMeshLua(lua);
                lua_pushstring(lua,"mesh");
                return 2;
            }
            case util::TYPE_MESH_SPRITE:    
            { 
                onNewSpriteLua(lua);
                lua_pushstring(lua,"sprite");
                return 2;
            }
            case util::TYPE_MESH_FONT:      
            { 
                if(lua_gettop(lua) > 1)//sFileName (2ds|2dw|3d) x,y,z 
                    lua_remove(lua,2);//remove (2ds|2dw|3d) --->sFileName x,y,z 
                lua_pushstring(lua,fileName);//sFileName sFileName (no matter the first one)
                if(lua_gettop(lua) > 2)//sFileName
                    lua_insert(lua,2);
                onNewFontLua(lua);
                lua_pushstring(lua,"font");
                return 2;
            }
            case util::TYPE_MESH_TEXTURE: 
            { 
                bool bIsImage,bIsMesh,bIsUnknown = false;
                const char* ext = meshTmp.getValidExtension(fileName,bIsImage,bIsMesh,bIsUnknown);
                if(ext && strcmp(ext,"GIF") == 0 )
                {
                    onNewGifViewLua(lua);
                    lua_pushstring(lua,"gif");
                    
                }
                else
                {
                    onNewTextureViewLua(lua);
                    lua_pushstring(lua,"texture");
                }
                return 2;
            }
            case util::TYPE_MESH_PARTICLE: 
            { 
                onNewParticleLua(lua); 
                lua_pushstring(lua,"particle");
                return 2;
            }
			case util::TYPE_MESH_TILE_MAP:
			{
				onNewSpriteLua(lua);
				lua_pushstring(lua, "tile");
				return 2;
			}
            default:
                return lua_error_debug(lua, "Failed to get type of mesh in the file:% \nexpected extension:%s",fileName," msh, spt, fnt, ptl ");
        }
    }

    void registerClassAuto(lua_State *lua)
    {
        luaL_Reg regAutoMethods[] = {{"new", onNewAutoLua},
                                          {nullptr, nullptr}};
        luaL_newmetatable(lua, "_mbmAuto");
        luaL_setfuncs(lua, regAutoMethods, 0);
        lua_setglobal(lua, "auto");
        lua_settop(lua,0);
    }

};

#endif
