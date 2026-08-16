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

#include <map>
#include <string>

#include <lua-wrap/render-table/mesh-debug-lua.h>
#include <lua-wrap/render-table/animation-lua.h>
#include <lua-wrap/render-table/mesh-lua.h>
#include <lua-wrap/render-table/sprite-lua.h>
#include <lua-wrap/render-table/font-lua.h>
#include <lua-wrap/render-table/gif-view-lua.h>
#include <lua-wrap/render-table/texture-view-lua.h>
#include <lua-wrap/render-table/particle-lua.h>
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
#include <core_mbm/scene.h>
#include <plugin-helper/plugin-helper.h>
#include <draw-compatibility.h>

#if DEBUG_FREE_LUA
	#include <core_mbm/util-interface.h>
#endif

namespace mbm
{
	class LINE_MESH;

    namespace
    {
        bool parseMaterialTextureSlotType(lua_State *lua, const int indexArg, uint16_t &slotType)
        {
            const int type = lua_type(lua, indexArg);
            if (type == LUA_TNUMBER)
            {
                const auto raw = static_cast<uint16_t>(luaL_checkinteger(lua, indexArg));
                switch (raw)
                {
                    case util::MATERIAL_TEXTURE_SLOT_NORMAL:
                    case util::MATERIAL_TEXTURE_SLOT_SPECULAR:
                    case util::MATERIAL_TEXTURE_SLOT_EMISSIVE:
                    case util::MATERIAL_TEXTURE_SLOT_MASK:
                    {
                        slotType = raw;
                        return true;
                    }
                    default:
                    {
                        return false;
                    }
                }
            }
            if (type == LUA_TSTRING)
            {
                const char *name = lua_tostring(lua, indexArg);
                if (strcasecmp(name, "normal") == 0)
                {
                    slotType = util::MATERIAL_TEXTURE_SLOT_NORMAL;
                    return true;
                }
                if (strcasecmp(name, "specular") == 0)
                {
                    slotType = util::MATERIAL_TEXTURE_SLOT_SPECULAR;
                    return true;
                }
                if (strcasecmp(name, "emissive") == 0)
                {
                    slotType = util::MATERIAL_TEXTURE_SLOT_EMISSIVE;
                    return true;
                }
                if (strcasecmp(name, "mask") == 0)
                {
                    slotType = util::MATERIAL_TEXTURE_SLOT_MASK;
                    return true;
                }
            }
            return false;
        }

        util::MATERIAL_TEXTURE_SLOT_DEBUG *findMaterialTextureSlot(util::SUBSET_DEBUG *subset,
                                                                   const uint16_t slotType) noexcept
        {
            if (subset == nullptr)
                return nullptr;
            for (auto &slot : subset->materialTextureSlots)
            {
                if (slot.type == slotType)
                    return &slot;
            }
            return nullptr;
        }

    }

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
            return this->mesh.getFilenameMesh();
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
            if (meshDebug->mesh.loadV11(fileName))
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
        const bool      compress      = top > 4 ? (lua_toboolean(lua, 5) ? true : false) : false;
        char            strError[255] = "";
        if (meshDebug->mesh.saveV11(fileName, calNormal, calUV, compress, strError,sizeof(strError)-1))
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

    int onFakeReleaseMeshManagerLua(lua_State *lua)
    {
        const char *fileName = luaL_checkstring(lua, 2);
        MESH_MANAGER::getInstance()->fakeRelease(fileName);
        return 0;
    }

    int onGetInfoMeshDebugLua(lua_State *lua)
    {
        util::HEADER_MESH headerMeshMbmOut;
        util::TYPE_MESH   typeOut;
		util::INFO_DRAW_MODE info_mode;
        INFO_BOUND_FONT  datailFontOut;
        std::vector<util::STAGE_PARTICLE> lsParticleInfo;
        int version = 0;
        const char *          fileName = luaL_checkstring(lua, 2);
        if (!MESH_MBM_DEBUG::getInfo(fileName, headerMeshMbmOut, info_mode,typeOut, datailFontOut, lsParticleInfo, &version))
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
        lua_pushinteger(lua, version);
        lua_setfield(lua, -2, "version");
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

		lua_pushstring(lua, util::get_mode_draw_from_uint(info_mode.mode_draw,"UNKNOWN"));
		lua_setfield(lua, -2, "modeDraw");

		lua_pushstring(lua, util::get_mode_cull_face_from_uint(info_mode.mode_cull_face,"UNKNOWN"));
		lua_setfield(lua, -2, "modeCullFace");

		lua_pushstring(lua, util::get_mode_front_face_direction_from_uint(info_mode.mode_front_face_direction,"UNKNOWN"));
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
        if (typeOut == util::TYPE_MESH_PARTICLE)
        {
            lua_pushinteger(lua, static_cast<lua_Integer>(lsParticleInfo.size()));
            lua_setfield(lua, -2, "stages");
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
        return onSetPhysicsFromTableLuaToLineMesh(lua,&meshDebug->mesh.getPhysicsInfo(),nullptr);
    }

    int onGetPhysicsMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        lua_settop(lua,0);
        lua_newtable(lua); // array
        const unsigned int sCube = static_cast<unsigned int>(meshDebug->mesh.getPhysicsInfo().lsCube.size());
        const unsigned int sTria = static_cast<unsigned int>(meshDebug->mesh.getPhysicsInfo().lsTriangle.size());
        const unsigned int sSphe = static_cast<unsigned int>(meshDebug->mesh.getPhysicsInfo().lsSphere.size());
        const unsigned int sComp = static_cast<unsigned int>(meshDebug->mesh.getPhysicsInfo().lsCubeComplex.size());
        int index_array = 1;

        if (sCube)
        {
            for (unsigned int i = 0; i < sCube; ++i)
            {
                CUBE *base = meshDebug->mesh.getPhysicsInfo().lsCube[i];
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
                TRIANGLE *triangle = meshDebug->mesh.getPhysicsInfo().lsTriangle[i];
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
                SPHERE *sphere = meshDebug->mesh.getPhysicsInfo().lsSphere[i];
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
                CUBE_COMPLEX *complex = meshDebug->mesh.getPhysicsInfo().lsCubeComplex[i];
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
                meshDebug->mesh.setMeshType(util::TYPE_MESH_3D);
                lua_pushboolean(lua, 1);
            }
            else if (strcasecmp(typeAsString, "sprite") == 0)
            {
                meshDebug->mesh.setMeshType(util::TYPE_MESH_SPRITE);
                lua_pushboolean(lua, 1);
            }
            else if (strcasecmp(typeAsString, "font") == 0)
            {
                meshDebug->mesh.setMeshType(util::TYPE_MESH_FONT);
                lua_pushboolean(lua, 1);
            }
            else if (strcasecmp(typeAsString, "user") == 0)
            {
                meshDebug->mesh.setMeshType(util::TYPE_MESH_USER);
                lua_pushboolean(lua, 1);
            }
            else if (strcasecmp(typeAsString, "texture") == 0)
            {
                meshDebug->mesh.setMeshType(util::TYPE_MESH_TEXTURE);
                lua_pushboolean(lua, 1);
            }
            else if (strcasecmp(typeAsString, "shape") == 0)
            {
                meshDebug->mesh.setMeshType(util::TYPE_MESH_SHAPE);
                lua_pushboolean(lua, 1);
            }
            else if (strcasecmp(typeAsString, "particle") == 0)
            {
                meshDebug->mesh.setMeshType(util::TYPE_MESH_PARTICLE);
                lua_pushboolean(lua, 1);
            }
			else if (strcasecmp(typeAsString, "tile") == 0)
			{
				meshDebug->mesh.setMeshType(util::TYPE_MESH_TILE_MAP);
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
        if (meshDebug->mesh.getMeshType() == util::TYPE_MESH_FONT)
        {
            meshDebug->mesh.replaceDetailInfo(newInfoFontFromLua(lua,2));
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
            return lua_error_debug(lua,"Not implemented setDetail for [%s]", getTypeAsString(meshDebug->mesh.getMeshType()));
        }
        return 0;
    }

	int onSetMode_drawMeshDebugLua(lua_State *lua)
	{
        MESH_DEBUG_LUA *meshDebug     = getMeshDebugFromRawTable(lua, 1, 1);
		const char* s_mode_draw       = luaL_checkstring(lua, 2);
        const unsigned int  mode_draw = util::get_mode_draw_from_string(s_mode_draw);
        if (mode_draw == std::numeric_limits<unsigned int>::max())
        {
			return lua_error_debug(lua,"Invalid mode draw: [%s] \n expected:[%s]",s_mode_draw ? s_mode_draw : "NULL",
				"TRIANGLES, TRIANGLE_STRIP, TRIANGLE_FAN, LINES, LINE_LOOP, LINE_STRIP, POINTS");
        }
        else
        {
            meshDebug->mesh.setModeDraw(mode_draw);
        }
        return 0;
    }

	int onSetMode_CullFaceMeshDebugLua(lua_State *lua)
	{
        MESH_DEBUG_LUA *meshDebug          = getMeshDebugFromRawTable(lua, 1, 1);
		const char* s_mode_cull_face       = luaL_checkstring(lua, 2);
        const unsigned int  mode_cull_face = util::get_mode_cull_face_from_string(s_mode_cull_face);
        if (mode_cull_face == std::numeric_limits<unsigned int>::max())
        {
			return lua_error_debug(lua,"Invalid mode cull face: [%s] \n expected:[%s]",s_mode_cull_face ? s_mode_cull_face : "NULL",
				"FRONT, BACK, FRONT_AND_BACK");
        }
        else
        {
            meshDebug->mesh.setModeCullFace(mode_cull_face);
        }
        return 0;
    }

	int onSetMode_FrontFaceMeshDebugLua(lua_State *lua)
	{
        MESH_DEBUG_LUA *meshDebug      = getMeshDebugFromRawTable(lua, 1, 1);
		const char* s_mode_front_face  = luaL_checkstring(lua, 2);
        const unsigned int  mode_front_face = util::get_mode_front_face_direction_from_string(s_mode_front_face);
        if (mode_front_face == std::numeric_limits<unsigned int>::max())
        {
			return lua_error_debug(lua,"Invalid mode cull face: [%s] \n expected:[%s]",s_mode_front_face ? s_mode_front_face : "NULL",
				"CW, CCW");
        }
        else
        {
            meshDebug->mesh.setModeFrontFaceDirection(mode_front_face);
        }
        return 0;
    }

	int onGetMode_drawMeshDebugLua(lua_State *lua)
	{
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
		const char *  mode_draw   = util::get_mode_draw_from_uint(meshDebug->mesh.getModeDraw(),"nil");
        lua_pushstring(lua,mode_draw);
        return 1;
    }

	int onGetMode_CullFaceMeshDebugLua(lua_State *lua)
	{
        MESH_DEBUG_LUA *meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
		const char *  mode_cull_face = util::get_mode_cull_face_from_uint(meshDebug->mesh.getModeCullFace(),"nil");
        lua_pushstring(lua,mode_cull_face);
        return 1;
    }

	int onGetMode_FrontFaceMeshDebugLua(lua_State *lua)
	{
        MESH_DEBUG_LUA *meshDebug     = getMeshDebugFromRawTable(lua, 1, 1);
		const char *  mode_front_face = util::get_mode_front_face_direction_from_uint(meshDebug->mesh.getModeFrontFaceDirection(),"nil");
        lua_pushstring(lua,mode_front_face);
        return 1;
    }

    int onGetVersionMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        lua_pushinteger(lua, meshDebug->mesh.getFileVersion());
        return 1;
    }

    int onGetMaterialMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const util::MATERIAL &m = meshDebug->mesh.getMaterial();
        lua_newtable(lua);
        lua_newtable(lua);
        lua_pushnumber(lua, m.Diffuse.r);
        lua_setfield(lua, -2, "r");
        lua_pushnumber(lua, m.Diffuse.g);
        lua_setfield(lua, -2, "g");
        lua_pushnumber(lua, m.Diffuse.b);
        lua_setfield(lua, -2, "b");
        lua_pushnumber(lua, m.Diffuse.a);
        lua_setfield(lua, -2, "a");
        lua_setfield(lua, -2, "Diffuse");
        lua_newtable(lua);
        lua_pushnumber(lua, m.Ambient.r);
        lua_setfield(lua, -2, "r");
        lua_pushnumber(lua, m.Ambient.g);
        lua_setfield(lua, -2, "g");
        lua_pushnumber(lua, m.Ambient.b);
        lua_setfield(lua, -2, "b");
        lua_pushnumber(lua, m.Ambient.a);
        lua_setfield(lua, -2, "a");
        lua_setfield(lua, -2, "Ambient");
        lua_newtable(lua);
        lua_pushnumber(lua, m.Specular.r);
        lua_setfield(lua, -2, "r");
        lua_pushnumber(lua, m.Specular.g);
        lua_setfield(lua, -2, "g");
        lua_pushnumber(lua, m.Specular.b);
        lua_setfield(lua, -2, "b");
        lua_pushnumber(lua, m.Specular.a);
        lua_setfield(lua, -2, "a");
        lua_setfield(lua, -2, "Specular");
        lua_newtable(lua);
        lua_pushnumber(lua, m.Emissive.r);
        lua_setfield(lua, -2, "r");
        lua_pushnumber(lua, m.Emissive.g);
        lua_setfield(lua, -2, "g");
        lua_pushnumber(lua, m.Emissive.b);
        lua_setfield(lua, -2, "b");
        lua_pushnumber(lua, m.Emissive.a);
        lua_setfield(lua, -2, "a");
        lua_setfield(lua, -2, "Emissive");
        lua_pushnumber(lua, m.Power);
        lua_setfield(lua, -2, "Power");
        return 1;
    }

    int onSetMaterialMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        util::MATERIAL &m = meshDebug->mesh.getMaterial();
        luaL_checktype(lua, 2, LUA_TTABLE);
        auto getColor = [lua](const char *key, float *r, float *g, float *b, float *a) {
            *r = *g = *b = *a = 1.0f;
            lua_getfield(lua, 2, key);
            if (lua_istable(lua, -1)) {
                lua_getfield(lua, -1, "r");
                if (lua_isnumber(lua, -1)) *r = static_cast<float>(lua_tonumber(lua, -1));
                lua_pop(lua, 1);
                lua_getfield(lua, -1, "g");
                if (lua_isnumber(lua, -1)) *g = static_cast<float>(lua_tonumber(lua, -1));
                lua_pop(lua, 1);
                lua_getfield(lua, -1, "b");
                if (lua_isnumber(lua, -1)) *b = static_cast<float>(lua_tonumber(lua, -1));
                lua_pop(lua, 1);
                lua_getfield(lua, -1, "a");
                if (lua_isnumber(lua, -1)) *a = static_cast<float>(lua_tonumber(lua, -1));
                lua_pop(lua, 1);
            }
            lua_pop(lua, 1);
        };
        float r, g, b, a;
        getColor("Diffuse", &r, &g, &b, &a);
        m.Diffuse = mbm::COLOR(r, g, b, a);
        getColor("Ambient", &r, &g, &b, &a);
        m.Ambient = mbm::COLOR(r, g, b, a);
        getColor("Specular", &r, &g, &b, &a);
        m.Specular = mbm::COLOR(r, g, b, a);
        getColor("Emissive", &r, &g, &b, &a);
        m.Emissive = mbm::COLOR(r, g, b, a);
        lua_getfield(lua, 2, "Power");
        if (lua_isnumber(lua, -1))
            m.Power = static_cast<float>(lua_tonumber(lua, -1));
        lua_pop(lua, 1);
        return 0;
    }

    int onGetTotalFrameMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        lua_pushinteger(lua, static_cast<lua_Integer>(meshDebug->mesh.getTotalFrames()));
        return 1;
    }

    int onGetTotalSubsetMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *   meshDebug  = getMeshDebugFromRawTable(lua, 1, 1);
        const auto indexFrame = (unsigned int)luaL_checkinteger(lua, 2) - 1;
        const uint32_t totalFrames = meshDebug->mesh.getTotalFrames();
        if (indexFrame < totalFrames)
        {
            lua_pushinteger(lua, static_cast<lua_Integer>(meshDebug->mesh.getTotalSubsets(indexFrame)));
            return 1;
        }
        else
        {
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d)\n"
                            "indexFrame %d ",
                       totalFrames, indexFrame + 1);
        }
    }

    int onGetTotalVertexMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *   meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const auto indexFrame  = (unsigned int)luaL_checkinteger(lua, 2) - 1;
        const auto indexSubset = (unsigned int)luaL_checkinteger(lua, 3) - 1;
        util::SUBSET_DEBUG *subset = meshDebug->mesh.getSubset(indexFrame, indexSubset);
        if (subset)
        {
            lua_pushinteger(lua, subset->vertexCount);
            return 1;
        }
        else
        {
            const int tSubset = static_cast<int>(meshDebug->mesh.getTotalSubsets(indexFrame));
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.getTotalFrames()), tSubset, indexFrame + 1, indexSubset + 1);
        }
    }

    int onGetTotalIndexMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *   meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const auto indexFrame  = (unsigned int)luaL_checkinteger(lua, 2) - 1;
        const auto indexSubset = (unsigned int)luaL_checkinteger(lua, 3) - 1;
        util::SUBSET_DEBUG *subset = meshDebug->mesh.getSubset(indexFrame, indexSubset);
        if (subset)
        {
            lua_pushinteger(lua, subset->indexCount);
            return 1;
        }
        else
        {
            const int tSubset = static_cast<int>(meshDebug->mesh.getTotalSubsets(indexFrame));
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.getTotalFrames()), tSubset, indexFrame + 1, indexSubset + 1);
        }
    }

    int onIsIndexBufferMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        if (meshDebug->mesh.hasIndexBuffer(0))
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
        util::BUFFER_MESH_DEBUG *buffer = meshDebug->mesh.getFrameBuffer(indexFrame);
        util::SUBSET_DEBUG *subset = meshDebug->mesh.getSubset(indexFrame, indexSubset);
        if (buffer && subset)
        {
            VEC3 *pPosition = meshDebug->mesh.getPositionArray(indexFrame);
            VEC3 *pNormal   = meshDebug->mesh.getNormalArray(indexFrame);
            VEC2 *pUv       = meshDebug->mesh.getUvArray(indexFrame);
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

                    if (pNormal)
                    {
                        lua_pushnumber(lua, pNormal[indexRaw].x);
                        lua_setfield(lua, -2, "nx");
                        lua_pushnumber(lua, pNormal[indexRaw].y);
                        lua_setfield(lua, -2, "ny");
                        lua_pushnumber(lua, pNormal[indexRaw].z);
                        lua_setfield(lua, -2, "nz");
                    }
                    else
                    {
                        lua_pushnumber(lua, 0.0);
                        lua_setfield(lua, -2, "nx");
                        lua_pushnumber(lua, 0.0);
                        lua_setfield(lua, -2, "ny");
                        lua_pushnumber(lua, 0.0);
                        lua_setfield(lua, -2, "nz");
                        #if (DEBUG || _DEBUG)
                        static bool s_warnNormalNull = true;
                        if (s_warnNormalNull)
                        {
                            s_warnNormalNull = false;
                            WARN_LOG("%s:%d Normal array is null for indexFrame %d, returning (0,0,0)\n", __FILE__, __LINE__, indexFrame);
                        }
                        #endif
                    }

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
            const int tSubset = static_cast<int>(meshDebug->mesh.getTotalSubsets(indexFrame));
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.getTotalFrames()), tSubset, indexFrame, indexSubset);
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
        util::BUFFER_MESH_DEBUG *buffer = meshDebug->mesh.getFrameBuffer(indexFrame);
        util::SUBSET_DEBUG *subset = meshDebug->mesh.getSubset(indexFrame, indexSubset);
        if (hasTable == LUA_TTABLE && buffer && subset)
        {
            VEC3 *pPosition = meshDebug->mesh.getPositionArray(indexFrame);
            VEC3 *pNormal   = meshDebug->mesh.getNormalArray(indexFrame);
            VEC2 *pUv       = meshDebug->mesh.getUvArray(indexFrame);
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

                    if (pNormal)
                    {
                        getFieldPrimaryFromTable(lua, indexTable, "nx", LUA_TNUMBER, &pNormal[indexRaw].x);
                        getFieldPrimaryFromTable(lua, indexTable, "ny", LUA_TNUMBER, &pNormal[indexRaw].y);
                        getFieldPrimaryFromTable(lua, indexTable, "nz", LUA_TNUMBER, &pNormal[indexRaw].z);
                    }

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

                        if (pNormal)
                        {
                            getFieldPrimaryFromTable(lua, indexTable, "nx", LUA_TNUMBER, &pNormal[indexRaw].x);
                            getFieldPrimaryFromTable(lua, indexTable, "ny", LUA_TNUMBER, &pNormal[indexRaw].y);
                            getFieldPrimaryFromTable(lua, indexTable, "nz", LUA_TNUMBER, &pNormal[indexRaw].z);
                        }

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
            const int tSubset = static_cast<int>(meshDebug->mesh.getTotalSubsets(indexFrame));
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.getTotalFrames()), tSubset, indexFrame + 1, indexSubset + 1);
        }
    }

    int onAddVertexMeshDebugLua(lua_State *lua)
    {
        const int          top         = lua_gettop(lua);
        MESH_DEBUG_LUA *   meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const unsigned int indexFrame  = top > 1 ? (unsigned int)luaL_checkinteger(lua, 2) - 1 : 0;
        const unsigned int indexSubset = top > 2 ? (unsigned int)luaL_checkinteger(lua, 3) - 1 : 0;
        const int          type4       = top > 3 ? lua_type(lua, 4) : LUA_TNIL;
        util::BUFFER_MESH_DEBUG *buffer = meshDebug->mesh.getFrameBuffer(indexFrame);
        util::SUBSET_DEBUG *subset = meshDebug->mesh.getSubset(indexFrame, indexSubset);
        if (buffer && subset)
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
                int lenTable = lua_rawlen(lua, 4);
                const int vertexCount = subset->vertexCount; // before add vertex
                if (lenTable <= 0)                           // hasn't array
                {
                    if (meshDebug->mesh.addVertex(indexFrame, indexSubset, 1))
                    {
                        buffer = meshDebug->mesh.getFrameBuffer(indexFrame); // re-fetch after addVertex (may reallocate)
                        subset = meshDebug->mesh.getSubset(indexFrame, indexSubset);
                        VEC3 *             pPosition  = meshDebug->mesh.getPositionArray(indexFrame);
                        VEC3 *             pNormal    = meshDebug->mesh.getNormalArray(indexFrame);
                        VEC2 *             pUv        = meshDebug->mesh.getUvArray(indexFrame);
                        constexpr int      indexTable = 4;
                        const unsigned int indexRaw   = subset->vertexStart + vertexCount;
                        getFieldPrimaryFromTable(lua, indexTable, "x", LUA_TNUMBER, &pPosition[indexRaw].x);
                        getFieldPrimaryFromTable(lua, indexTable, "y", LUA_TNUMBER, &pPosition[indexRaw].y);
                        getFieldPrimaryFromTable(lua, indexTable, "z", LUA_TNUMBER, &pPosition[indexRaw].z);

                        if (pNormal)
                        {
                            getFieldPrimaryFromTable(lua, indexTable, "nx", LUA_TNUMBER, &pNormal[indexRaw].x);
                            getFieldPrimaryFromTable(lua, indexTable, "ny", LUA_TNUMBER, &pNormal[indexRaw].y);
                            getFieldPrimaryFromTable(lua, indexTable, "nz", LUA_TNUMBER, &pNormal[indexRaw].z);
                        }

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
                    buffer = meshDebug->mesh.getFrameBuffer(indexFrame); // re-fetch after addVertex (may reallocate)
                    subset = meshDebug->mesh.getSubset(indexFrame, indexSubset);
                    VEC3 *pPosition = meshDebug->mesh.getPositionArray(indexFrame);
                    VEC3 *pNormal   = meshDebug->mesh.getNormalArray(indexFrame);
                    VEC2 *pUv       = meshDebug->mesh.getUvArray(indexFrame);
                    for (int ii = 0; ii < lenTable; ++ii)
                    {
                        constexpr int indexTable = 5;
                        const unsigned int indexRaw   = subset->vertexStart + vertexCount + ii;
                        lua_rawgeti(lua, 4, (ii + 1));
                        getFieldPrimaryFromTable(lua, indexTable, "x", LUA_TNUMBER, &pPosition[indexRaw].x);
                        getFieldPrimaryFromTable(lua, indexTable, "y", LUA_TNUMBER, &pPosition[indexRaw].y);
                        getFieldPrimaryFromTable(lua, indexTable, "z", LUA_TNUMBER, &pPosition[indexRaw].z);

                        if (pNormal)
                        {
                            getFieldPrimaryFromTable(lua, indexTable, "nx", LUA_TNUMBER, &pNormal[indexRaw].x);
                            getFieldPrimaryFromTable(lua, indexTable, "ny", LUA_TNUMBER, &pNormal[indexRaw].y);
                            getFieldPrimaryFromTable(lua, indexTable, "nz", LUA_TNUMBER, &pNormal[indexRaw].z);
                        }

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
            const int tSubset = static_cast<int>(meshDebug->mesh.getTotalSubsets(indexFrame));
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.getTotalFrames()), tSubset, indexFrame+1, indexSubset+1);
        }
    }

    int onGetIndexMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *   meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const unsigned int indexFrame  = (unsigned int)luaL_checkinteger(lua, 2) - 1;
        const unsigned int indexSubset = (unsigned int)luaL_checkinteger(lua, 3) - 1;
        util::BUFFER_MESH_DEBUG *buffer = meshDebug->mesh.getFrameBuffer(indexFrame);
        util::SUBSET_DEBUG *subset = meshDebug->mesh.getSubset(indexFrame, indexSubset);
        if (buffer && subset)
        {
            uint16_t *indexArray = meshDebug->mesh.getIndexArray(indexFrame);
            if (indexArray)
            {
                lua_newtable(lua);
                const unsigned int s = (subset->indexCount + subset->indexStart);
                for (unsigned int i = subset->indexStart, j = 1; i < s; i++, ++j)
                {
                    const int indexRaw = indexArray[i] - subset->vertexStart;
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
            const int tSubset = static_cast<int>(meshDebug->mesh.getTotalSubsets(indexFrame));
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.getTotalFrames()), tSubset, indexFrame + 1, indexSubset + 1);
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
            getArrayUintFromTable(lua, 4, const_cast<unsigned short int*>(pIndex.data()), sTableIndex);
            char strErrorOut[255] = "";
			for (unsigned int i=0; i < pIndex.size(); ++i)
			{
				pIndex[i] = pIndex[i] - 1;
			}
            if (!meshDebug->mesh.addIndex(indexFrame, indexSubset, pIndex.data(), sTableIndex, strErrorOut, (int)sizeof(strErrorOut)))
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
        util::SUBSET_DEBUG *subset = meshDebug->mesh.getSubset(indexFrame, indexSubset);
        if (subset)
        {
            if (subset->texture.size())
                lua_pushstring(lua, subset->texture.c_str());
            else
                lua_pushnil(lua);
            return 1;
        }
        else
        {
            const int tSubset = static_cast<int>(meshDebug->mesh.getTotalSubsets(indexFrame));
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.getTotalFrames()), tSubset, indexFrame +1, indexSubset +1);
        }
    }

    int onSetTextureNameMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *   meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const auto indexFrame  = (unsigned int)luaL_checkinteger(lua, 2) - 1;
        const auto indexSubset = (unsigned int)luaL_checkinteger(lua, 3) - 1;
        const char *       fileName    = lua_type(lua, 4) == LUA_TSTRING ? luaL_checkstring(lua, 4) : nullptr;
        util::SUBSET_DEBUG *subset = meshDebug->mesh.getSubset(indexFrame, indexSubset);
        if (subset)
        {
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
            const int tSubset = static_cast<int>(meshDebug->mesh.getTotalSubsets(indexFrame));
            return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                            "indexFrame %d indexSubset %d",
                       static_cast<int>(meshDebug->mesh.getTotalFrames()), tSubset, indexFrame + 1, indexSubset + 1);
        }
    }

    int onGetMaterialTextureNameMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const auto indexFrame = static_cast<unsigned int>(luaL_checkinteger(lua, 2) - 1);
        const auto indexSubset = static_cast<unsigned int>(luaL_checkinteger(lua, 3) - 1);
        uint16_t slotType = 0;
        if (!parseMaterialTextureSlotType(lua, 4, slotType))
            return lua_error_debug(lua, "Expected material texture slot type [normal|specular|emissive|mask]");
        util::SUBSET_DEBUG *subset = meshDebug->mesh.getSubset(indexFrame, indexSubset);
        if (subset)
        {
            const auto *slot = findMaterialTextureSlot(subset, slotType);
            if (slot && slot->texture.size())
                lua_pushstring(lua, slot->texture.c_str());
            else
                lua_pushnil(lua);
            return 1;
        }
        const int tSubset = static_cast<int>(meshDebug->mesh.getTotalSubsets(indexFrame));
        return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                        "indexFrame %d indexSubset %d",
                   static_cast<int>(meshDebug->mesh.getTotalFrames()), tSubset, indexFrame + 1, indexSubset + 1);
    }

    int onSetMaterialTextureNameMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const auto indexFrame = static_cast<unsigned int>(luaL_checkinteger(lua, 2) - 1);
        const auto indexSubset = static_cast<unsigned int>(luaL_checkinteger(lua, 3) - 1);
        uint16_t slotType = 0;
        if (!parseMaterialTextureSlotType(lua, 4, slotType))
            return lua_error_debug(lua, "Expected material texture slot type [normal|specular|emissive|mask]");
        const char *fileName = lua_type(lua, 5) == LUA_TSTRING ? luaL_checkstring(lua, 5) : nullptr;
        util::SUBSET_DEBUG *subset = meshDebug->mesh.getSubset(indexFrame, indexSubset);
        if (subset)
        {
            auto *slot = findMaterialTextureSlot(subset, slotType);
            if (fileName && strlen(fileName))
            {
                if (slot == nullptr)
                {
                    util::MATERIAL_TEXTURE_SLOT_DEBUG newSlot;
                    newSlot.type = slotType;
                    newSlot.texture = fileName;
                    subset->materialTextureSlots.push_back(newSlot);
                }
                else
                {
                    slot->texture = fileName;
                }
                util::addPath(fileName);
            }
            else if (slot)
            {
                auto &slots = subset->materialTextureSlots;
                for (auto it = slots.begin(); it != slots.end(); ++it)
                {
                    if (it->type == slotType)
                    {
                        slots.erase(it);
                        break;
                    }
                }
            }
            lua_pushboolean(lua, 1);
            return 1;
        }
        const int tSubset = static_cast<int>(meshDebug->mesh.getTotalSubsets(indexFrame));
        return lua_error_debug(lua, "\nOut of bound[indexFrame(total %d),indexSubset(total %d)\n"
                        "indexFrame %d indexSubset %d",
                   static_cast<int>(meshDebug->mesh.getTotalFrames()), tSubset, indexFrame + 1, indexSubset + 1);
    }

    int onGetFxTextureMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const auto      indexAnim = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const char *    fileName  = meshDebug->mesh.getAnimationEffectTexture(indexAnim);
        if (fileName && fileName[0])
            lua_pushstring(lua, fileName);
        else
            lua_pushnil(lua);
        return 1;
    }

    int onSetFxTextureMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const auto      indexAnim = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const char *    fileName  = lua_type(lua, 3) == LUA_TSTRING ? luaL_checkstring(lua, 3) : nullptr;
        const bool      ret       = meshDebug->mesh.setAnimationEffectTexture(indexAnim, fileName);
        if (ret && fileName && strlen(fileName))
            util::addPath(fileName);
        if (!ret)
            return lua_error_debug(lua, "invalid animation index [%u]", indexAnim + 1);
        lua_pushboolean(lua, 1);
        return 1;
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
        const unsigned int indexFrame = top > 1 ? (unsigned int)luaL_checkinteger(lua, 2) - 1 : meshDebug->mesh.getTotalFrames() - 1;
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
                                                                        errorOut,
                                                                        (int)sizeof(errorOut));
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

    int onCentralizeItselfMeshDebugLua(lua_State *lua)
    {
        const int       top         = lua_gettop(lua);
        MESH_DEBUG_LUA *meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const int       indexFrame  = top > 1 ? luaL_checkinteger(lua, 2) - 1 : -1;
        const int       indexSubset = top > 2 ? luaL_checkinteger(lua, 3) - 1 : -1;
        meshDebug->mesh.centralizeFrameItself(indexFrame, indexSubset);
        return 0;
    }

    // rotateFrame(frame, ax, ay, az [,subset])  -- frame=0 means all; subset=0 means all; angles in degrees
    int onRotateFrameDebugLua(lua_State *lua)
    {
        const int       top         = lua_gettop(lua);
        MESH_DEBUG_LUA *meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const int       frameArg    = top > 1 ? luaL_checkinteger(lua, 2) : 0;
        const int       indexFrame  = frameArg <= 0 ? -1 : frameArg - 1;
        const float     angleX      = top > 2 ? static_cast<float>(luaL_optnumber(lua, 3, 0.0)) : 0.0f;
        const float     angleY      = top > 3 ? static_cast<float>(luaL_optnumber(lua, 4, 0.0)) : 0.0f;
        const float     angleZ      = top > 4 ? static_cast<float>(luaL_optnumber(lua, 5, 0.0)) : 0.0f;
        const int       subsetArg   = top > 5 ? luaL_optinteger(lua, 6, 0) : 0;
        const int       indexSubset = subsetArg <= 0 ? -1 : subsetArg - 1;
        meshDebug->mesh.rotateFrame(indexFrame, indexSubset, angleX, angleY, angleZ);
        return 0;
    }

    // scaleFrame(frame, sx, sy, sz [,subset]) -- frame/subset=0 means all.
    int onScaleFrameDebugLua(lua_State *lua)
    {
        const int       top         = lua_gettop(lua);
        MESH_DEBUG_LUA *meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const int       frameArg    = top > 1 ? luaL_checkinteger(lua, 2) : 0;
        const int       indexFrame  = frameArg <= 0 ? -1 : frameArg - 1;
        const float     sx          = top > 2 ? static_cast<float>(luaL_optnumber(lua, 3, 1.0)) : 1.0f;
        const float     sy          = top > 3 ? static_cast<float>(luaL_optnumber(lua, 4, 1.0)) : 1.0f;
        const float     sz          = top > 4 ? static_cast<float>(luaL_optnumber(lua, 5, 1.0)) : 1.0f;
        const int       subsetArg   = top > 5 ? luaL_optinteger(lua, 6, 0) : 0;
        const int       indexSubset = subsetArg <= 0 ? -1 : subsetArg - 1;
        meshDebug->mesh.scaleFrame(indexFrame, indexSubset, sx, sy, sz);
        return 0;
    }

    // scaleSkeletalAsset(scale) -- atomically scales geometry, canonical bind/clip translations,
    // bone display metadata, and physics bounds. Only a finite positive uniform factor is valid.
    int onScaleSkeletalAssetDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const float scale = static_cast<float>(luaL_checknumber(lua, 2));
        char errorOut[255] = "";
        if (!meshDebug->mesh.scaleSkeletalAsset(scale, errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        return 0;
    }

    // translateFrame(frame, dx, dy, dz [,subset])  -- frame=0 means all; subset=0 means all; values added to each vertex position
    int onTranslateFrameDebugLua(lua_State *lua)
    {
        const int       top         = lua_gettop(lua);
        MESH_DEBUG_LUA *meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const int       frameArg    = top > 1 ? luaL_checkinteger(lua, 2) : 0;
        const int       indexFrame  = frameArg <= 0 ? -1 : frameArg - 1;
        const float     dx          = top > 2 ? static_cast<float>(luaL_optnumber(lua, 3, 0.0)) : 0.0f;
        const float     dy          = top > 3 ? static_cast<float>(luaL_optnumber(lua, 4, 0.0)) : 0.0f;
        const float     dz          = top > 4 ? static_cast<float>(luaL_optnumber(lua, 5, 0.0)) : 0.0f;
        const int       subsetArg   = top > 5 ? luaL_optinteger(lua, 6, 0) : 0;
        const int       indexSubset = subsetArg <= 0 ? -1 : subsetArg - 1;
        meshDebug->mesh.translateFrame(indexFrame, indexSubset, dx, dy, dz);
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
        const int totalFrames      = static_cast<int>(meshDebug->mesh.getTotalFrames());
        if (stride != 2 && stride != 3)
            return lua_error_debug(lua, "Stride must be 3 or 2");
        if (indexFrame < 0)
        {
            for (int i = 0; i < totalFrames; ++i)
            {
                util::BUFFER_MESH_DEBUG *bufferCurrent = meshDebug->mesh.getFrameBuffer(static_cast<uint32_t>(i));
                bufferCurrent->headerFrame.stride      = stride;
            }
        }
        else if (indexFrame < totalFrames)
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent = meshDebug->mesh.getFrameBuffer(static_cast<uint32_t>(indexFrame));
            bufferCurrent->headerFrame.stride      = stride;
        }
        else
        {
            return lua_error_debug(lua, "Index frame invalid [%d/%d]",top > 2 ? indexFrame + 1 : indexFrame, totalFrames);
        }
        return 0;
    }

    int onGetStrideMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug  = getMeshDebugFromRawTable(lua, 1, 1);
        const int       top        = lua_gettop(lua);
        const int       indexFrame = top > 1 ? luaL_checkinteger(lua, 2) - 1 : 0;
        const int totalFrames      = static_cast<int>(meshDebug->mesh.getTotalFrames());
        if (indexFrame >= 0 && indexFrame < totalFrames)
        {
            util::BUFFER_MESH_DEBUG *bufferCurrent = meshDebug->mesh.getFrameBuffer(static_cast<uint32_t>(indexFrame));
            lua_pushinteger(lua, bufferCurrent->headerFrame.stride);
            return 1;
        }
        return lua_error_debug(lua, "Index frame invalid [%d/%d]", top > 1 ? indexFrame + 1 : indexFrame, totalFrames);
    }

    int onEnableNormalsMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug               = getMeshDebugFromRawTable(lua, 1, 1);
        const int       enable                  = lua_toboolean(lua, 2);
        meshDebug->mesh.setHasNormal(enable ? HAS_NOR_IN_FILE : HAS_NOR_NO);
        return 0;
    }

    int onRemoveNormalsMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        meshDebug->mesh.removeNormals();
        return 0;
    }

    int onRemoveFrameDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *  meshDebug  = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t    indexFrame = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        meshDebug->mesh.removeBuffer(indexFrame);
        return 0;
    }

    int onRemoveSubsetDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug    = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t  indexFrame   = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const uint32_t  indexSubset  = static_cast<uint32_t>(luaL_checkinteger(lua, 3) - 1);
        meshDebug->mesh.removeSubset(indexFrame, indexSubset);
        return 0;
    }

    int onMoveSubsetUpDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug   = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer frame     = luaL_checkinteger(lua, 2);
        const lua_Integer subset    = luaL_checkinteger(lua, 3);
        const bool moved = frame > 0 && subset > 1 &&
                           meshDebug->mesh.moveSubsetUp(static_cast<uint32_t>(frame - 1),
                                                        static_cast<uint32_t>(subset - 1));
        lua_pushboolean(lua, moved ? 1 : 0);
        return 1;
    }

    int onCopyFrameFromDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug    = getMeshDebugFromRawTable(lua, 1, 1);
        MESH_DEBUG_LUA *srcDebug     = getMeshDebugFromRawTable(lua, 1, 2);
        const uint32_t  srcFrameIdx  = static_cast<uint32_t>(luaL_checkinteger(lua, 3) - 1);
        const uint32_t  ret          = meshDebug->mesh.copyBufferFrom(srcDebug->mesh, srcFrameIdx);
        lua_pushinteger(lua, static_cast<lua_Integer>(ret));
        return 1;
    }

    int onCopySubsetFromDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug    = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t  targetFrame  = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        MESH_DEBUG_LUA *srcDebug     = getMeshDebugFromRawTable(lua, 1, 3);
        const uint32_t  srcFrame     = static_cast<uint32_t>(luaL_checkinteger(lua, 4) - 1);
        const uint32_t  srcSubset    = static_cast<uint32_t>(luaL_checkinteger(lua, 5) - 1);
        const uint32_t  ret          = meshDebug->mesh.copySubsetFrom(targetFrame, srcDebug->mesh, srcFrame, srcSubset);
        lua_pushinteger(lua, static_cast<lua_Integer>(ret));
        return 1;
    }

    int onRemoveAnimationDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t  index     = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        meshDebug->mesh.removeAnimation(index);
        return 0;
    }

    int onAddNormalsMeshDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        meshDebug->mesh.addNormals();
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
                meshDebug->mesh.setHasTexture(HAS_TEX_FIRST_FRAME);
            else
                meshDebug->mesh.setHasTexture(HAS_TEX_EACH_FRAME);
        }
        else
        {
            meshDebug->mesh.setHasTexture(HAS_TEX_NO);
        }
        return 0;
    }

    void fillEffect(const EFFECT_SHADER* fx,const char* textureAnimationEffect,util::INFO_SHADER_DATA** dataInfoShader)
    {
        if(fx->getCurrentShader())
        {
            const unsigned int sTexStage2    = textureAnimationEffect ? static_cast<unsigned int>(strlen(textureAnimationEffect)): 0;
            const unsigned int sizeFileName  = static_cast<unsigned int>(fx->getCurrentShader()->fileName.size());
            const unsigned int totalVar      = fx->getCurrentShader()->getTotalVar();
            const unsigned int sizeArrayVarInBytes = totalVar * 4;
            auto dataInfo = new util::INFO_SHADER_DATA(sizeArrayVarInBytes,
                (short)(sizeFileName ? sizeFileName + 1 : 0),
                (short)(sTexStage2   ? sTexStage2   + 1 : 0));
            *dataInfoShader     = (dataInfo);
            dataInfo->typeAnimation    = fx->getTypeAnim();
            dataInfo->timeAnimation    = fx->getTimeAnimation();
            if(sizeFileName)
                strncpy(dataInfo->fileNameShader,fx->getCurrentShader()->fileName.c_str(),sizeFileName + 1);
            if(sTexStage2)
                strncpy(dataInfo->fileNameTextureStage2,textureAnimationEffect,sTexStage2 + 1);
            for(unsigned int k=0; k < totalVar; ++k)
            {
                const int index       = k * 4;
                VAR_SHADER* var       = fx->getCurrentShader()->getVar(k);
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
            if (renderizable->getTypeClass() == TYPE_CLASS_PARTICLE)
            {
                auto* particle = static_cast<PARTICLE*>(renderizable);
                if (particle)
                {
					auto* lsParticleInfo = new std::vector<util::STAGE_PARTICLE*>();
					meshDebug->mesh.replaceDetailInfo(lsParticleInfo);
                    for (unsigned int i = 0; i < particle->getTotalStage(); ++i)
                    {
                        util::STAGE_PARTICLE* stage = particle->getStageParticle(i);
                        auto  nStage = new util::STAGE_PARTICLE(stage);
                        lsParticleInfo->push_back(nStage);
                    }
                }
            }
            meshDebug->mesh.clearBlendOperations();
            meshDebug->mesh.resizeBlendOperations(animations->getTotalAnimation());

            for (unsigned int i=0; i < animations->getTotalAnimation(); ++i)
            {
                ANIMATION* anim             = animations->getAnimation(i);
                FX &fx                      = anim->getFx();
                const char* textureAnimationEffect = fx.textureAnimationEffect ? fx.textureAnimationEffect->getFileNameTexture() : nullptr;

                util::INFO_ANIMATION::INFO_HEADER_ANIM* infoHead = meshDebug->mesh.getAnimationHeader(i);
                if(infoHead == nullptr && i == meshDebug->mesh.getTotalAnimationHeaders())
                {
                    infoHead = new util::INFO_ANIMATION::INFO_HEADER_ANIM();
                    auto headerAnim = new util::HEADER_ANIMATION();
                    meshDebug->mesh.appendAnimationHeader(infoHead);
                    infoHead->headerAnim = headerAnim;
                    strncpy(headerAnim->nameAnimation,anim->getNameAnimation(),sizeof(headerAnim->nameAnimation));
                    headerAnim->typeAnimation = anim->getType();
                    headerAnim->timeBetweenFrame = anim->getIntervalChangeFrame();
                }
                if(infoHead)
                {
                    if(infoHead->effectShader)
                        delete infoHead->effectShader;
                    infoHead->effectShader = nullptr;
                    util::HEADER_ANIMATION *headerAnim  = infoHead->headerAnim;
					headerAnim->hasShaderEffect = 1;
                    headerAnim->blendState        = static_cast<unsigned short int>(anim->getBlendState());
                    meshDebug->mesh.setBlendOperation(i, fx.blendOperation);

                    if(fx.fxPS->getCurrentShader())
                    {
                        infoHead->effectShader = new util::INFO_FX();
                        infoHead->effectShader->blendOperation = fx.blendOperation;
                        infoHead->effectShader->setTextureAnimationEffectFileName(textureAnimationEffect);
                        fillEffect(fx.fxPS,textureAnimationEffect,&infoHead->effectShader->dataPS);
                    }
                    if(fx.fxVS->getCurrentShader())
                    {
                        if(infoHead->effectShader == nullptr)
                        {
                            infoHead->effectShader = new util::INFO_FX();
                            infoHead->effectShader->blendOperation = fx.blendOperation;
                        }
                        infoHead->effectShader->setTextureAnimationEffectFileName(textureAnimationEffect);
                        fillEffect(fx.fxVS,textureAnimationEffect,&infoHead->effectShader->dataVS);
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
        lua_pushinteger(lua,static_cast<lua_Integer>(head->headerAnim->initialFrame+1));
        lua_pushinteger(lua,static_cast<lua_Integer>(head->headerAnim->finalFrame+1));
        lua_pushnumber(lua,head->headerAnim->timeBetweenFrame);
        lua_pushinteger(lua,static_cast<lua_Integer>(head->headerAnim->typeAnimation));
        return 5;
    }

    int onGetTotalArticulatedPartsDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        lua_pushinteger(lua, static_cast<lua_Integer>(meshDebug->mesh.getTotalArticulatedParts()));
        return 1;
    }

    int onGetArticulatedPartDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const int index = static_cast<int>(luaL_checkinteger(lua, 2)) - 1;
        const util::ARTICULATED_PART_V11 *part = index >= 0
            ? meshDebug->mesh.getArticulatedPart(static_cast<uint32_t>(index)) : nullptr;
        if (!part)
        {
            lua_pushnil(lua);
            return 1;
        }
        lua_pushinteger(lua, static_cast<lua_Integer>(part->partId));
        lua_pushinteger(lua, static_cast<lua_Integer>(part->frameIndex + 1));
        lua_pushinteger(lua, static_cast<lua_Integer>(part->subsetIndex + 1));
        lua_pushstring(lua, part->name.c_str());
        lua_pushnumber(lua, part->pivotX); lua_pushnumber(lua, part->pivotY); lua_pushnumber(lua, part->pivotZ);
        lua_pushnumber(lua, part->pivotQX); lua_pushnumber(lua, part->pivotQY);
        lua_pushnumber(lua, part->pivotQZ); lua_pushnumber(lua, part->pivotQW);
        lua_pushinteger(lua, static_cast<lua_Integer>(part->parentPartId));
        return 12;
    }

    int onInitializeArticulatedPartsDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        lua_pushinteger(lua, static_cast<lua_Integer>(meshDebug->mesh.initializeArticulatedParts()));
        return 1;
    }

    int onRemoveArticulatedPartsDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        lua_pushinteger(lua, static_cast<lua_Integer>(meshDebug->mesh.removeArticulatedParts()));
        return 1;
    }

    int onAddArticulatedPartDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint64_t partId = static_cast<uint64_t>(luaL_checkinteger(lua, 2));
        const uint32_t frame = static_cast<uint32_t>(luaL_checkinteger(lua, 3) - 1);
        const uint32_t subset = static_cast<uint32_t>(luaL_checkinteger(lua, 4) - 1);
        const char *name = luaL_optstring(lua, 5, "");
        const float px = static_cast<float>(luaL_optnumber(lua, 6, 0.0));
        const float py = static_cast<float>(luaL_optnumber(lua, 7, 0.0));
        const float pz = static_cast<float>(luaL_optnumber(lua, 8, 0.0));
        const float qx = static_cast<float>(luaL_optnumber(lua, 9, 0.0));
        const float qy = static_cast<float>(luaL_optnumber(lua, 10, 0.0));
        const float qz = static_cast<float>(luaL_optnumber(lua, 11, 0.0));
        const float qw = static_cast<float>(luaL_optnumber(lua, 12, 1.0));
        const uint64_t parent = static_cast<uint64_t>(luaL_optinteger(lua, 13, 0));
        char errorOut[255] = "";
        const int ret = meshDebug->mesh.addArticulatedPart(partId, frame, subset, name, px, py, pz,
                                                            qx, qy, qz, qw, parent, errorOut, sizeof(errorOut));
        if (ret == 0)
            return lua_error_debug(lua, errorOut);
        lua_pushinteger(lua, ret);
        return 1;
    }

    int onRemoveArticulatedAnimationDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t index = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        char errorOut[255] = "";
        if (!meshDebug->mesh.removeArticulatedAnimation(index, errorOut, sizeof(errorOut)))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, 1);
        return 1;
    }

    int onUpdateArticulatedPartDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t index = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const char *name = luaL_optstring(lua, 3, "");
        const float px = static_cast<float>(luaL_optnumber(lua, 4, 0.0));
        const float py = static_cast<float>(luaL_optnumber(lua, 5, 0.0));
        const float pz = static_cast<float>(luaL_optnumber(lua, 6, 0.0));
        const float qx = static_cast<float>(luaL_optnumber(lua, 7, 0.0));
        const float qy = static_cast<float>(luaL_optnumber(lua, 8, 0.0));
        const float qz = static_cast<float>(luaL_optnumber(lua, 9, 0.0));
        const float qw = static_cast<float>(luaL_optnumber(lua, 10, 1.0));
        const uint64_t parent = static_cast<uint64_t>(luaL_optinteger(lua, 11, 0));
        char errorOut[255] = "";
        if (!meshDebug->mesh.updateArticulatedPart(index, name, px, py, pz, qx, qy, qz, qw,
                                                   parent, errorOut, sizeof(errorOut)))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, 1);
        return 1;
    }

    int onGetTotalArticulatedAnimationsDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        lua_pushinteger(lua, static_cast<lua_Integer>(meshDebug->mesh.getTotalArticulatedAnimations()));
        return 1;
    }

    int onGetArticulatedAnimationNameDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t index = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const char *name = meshDebug->mesh.getArticulatedAnimationName(index);
        if (name) lua_pushstring(lua, name); else lua_pushnil(lua);
        return 1;
    }

    int onGetArticulatedAnimationDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t index = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const char *name = nullptr;
        float duration = 0.0f, speed = 1.0f;
        int priority = 0;
        bool loop = false;
        uint8_t blendMode = util::ARTICULATED_BLEND_ABSOLUTE;
        if (!meshDebug->mesh.getArticulatedAnimation(
                index, &name, &duration, &speed, &priority, &loop, &blendMode))
        {
            lua_pushnil(lua);
            return 1;
        }
        lua_pushstring(lua, name);
        lua_pushnumber(lua, duration);
        lua_pushnumber(lua, speed);
        lua_pushinteger(lua, priority);
        lua_pushboolean(lua, loop ? 1 : 0);
        lua_pushinteger(lua, static_cast<lua_Integer>(blendMode));
        return 6;
    }

    int onGetTotalArticulatedTracksDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t animation = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        lua_pushinteger(lua, static_cast<lua_Integer>(meshDebug->mesh.getTotalArticulatedTracks(animation)));
        return 1;
    }

    int onUpdateArticulatedAnimationDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t index = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const char *name = luaL_checkstring(lua, 3);
        const float duration = static_cast<float>(luaL_optnumber(lua, 4, 0.0));
        const float speed = static_cast<float>(luaL_optnumber(lua, 5, 1.0));
        const int priority = static_cast<int>(luaL_optinteger(lua, 6, 0));
        const bool loop = lua_isnoneornil(lua, 7) ? true : lua_toboolean(lua, 7) != 0;
        const uint8_t blendMode = static_cast<uint8_t>(
            luaL_optinteger(lua, 8, util::ARTICULATED_BLEND_ABSOLUTE));
        char errorOut[255] = "";
        if (!meshDebug->mesh.updateArticulatedAnimation(
                index, name, duration, speed, priority, loop, blendMode,
                                                        errorOut, sizeof(errorOut)))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, 1);
        return 1;
    }

    int onGetArticulatedTrackDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t animation = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const uint32_t track = static_cast<uint32_t>(luaL_checkinteger(lua, 3) - 1);
        uint64_t partId = 0; uint8_t mask = 0; uint32_t keyCount = 0;
        if (!meshDebug->mesh.getArticulatedTrack(animation, track, &partId, &mask, &keyCount))
        {
            lua_pushnil(lua);
            return 1;
        }
        lua_pushinteger(lua, static_cast<lua_Integer>(partId));
        lua_pushinteger(lua, static_cast<lua_Integer>(mask));
        lua_pushinteger(lua, static_cast<lua_Integer>(keyCount));
        return 3;
    }

    int onGetArticulatedKeyDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t animation = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const uint32_t track = static_cast<uint32_t>(luaL_checkinteger(lua, 3) - 1);
        const uint32_t keyIndex = static_cast<uint32_t>(luaL_checkinteger(lua, 4) - 1);
        float time = 0.0f, px = 0.0f, py = 0.0f, pz = 0.0f;
        float qx = 0.0f, qy = 0.0f, qz = 0.0f, qw = 1.0f;
        float sx = 1.0f, sy = 1.0f, sz = 1.0f;
        uint8_t easing = util::ARTICULATED_EASING_LINEAR;
        float bezierX1 = 0.25f, bezierY1 = 0.25f;
        float bezierX2 = 0.75f, bezierY2 = 0.75f;
        float rotationEulerX = 0.0f, rotationEulerY = 0.0f, rotationEulerZ = 0.0f;
        bool hasRotationEuler = false;
        if (!meshDebug->mesh.getArticulatedKey(animation, track, keyIndex, &time, &px, &py, &pz,
                                               &qx, &qy, &qz, &qw, &sx, &sy, &sz, &easing,
                                               &bezierX1, &bezierY1, &bezierX2, &bezierY2,
                                               &rotationEulerX, &rotationEulerY, &rotationEulerZ,
                                               &hasRotationEuler))
        {
            lua_pushnil(lua);
            return 1;
        }
        lua_pushnumber(lua, time);
        lua_pushnumber(lua, px); lua_pushnumber(lua, py); lua_pushnumber(lua, pz);
        lua_pushnumber(lua, qx); lua_pushnumber(lua, qy); lua_pushnumber(lua, qz); lua_pushnumber(lua, qw);
        lua_pushnumber(lua, sx); lua_pushnumber(lua, sy); lua_pushnumber(lua, sz);
        lua_pushinteger(lua, static_cast<lua_Integer>(easing));
        lua_pushnumber(lua, bezierX1); lua_pushnumber(lua, bezierY1);
        lua_pushnumber(lua, bezierX2); lua_pushnumber(lua, bezierY2);
        lua_pushnumber(lua, rotationEulerX);
        lua_pushnumber(lua, rotationEulerY);
        lua_pushnumber(lua, rotationEulerZ);
        lua_pushboolean(lua, hasRotationEuler ? 1 : 0);
        return 20;
    }

    int onAddArticulatedAnimationDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const char *name = luaL_checkstring(lua, 2);
        const float duration = static_cast<float>(luaL_optnumber(lua, 3, 0.0));
        const float speed = static_cast<float>(luaL_optnumber(lua, 4, 1.0));
        const int priority = static_cast<int>(luaL_optinteger(lua, 5, 0));
        const bool loop = lua_isnoneornil(lua, 6) ? true : lua_toboolean(lua, 6) != 0;
        const uint8_t blendMode = static_cast<uint8_t>(
            luaL_optinteger(lua, 7, util::ARTICULATED_BLEND_ABSOLUTE));
        char errorOut[255] = "";
        const int ret = meshDebug->mesh.addArticulatedAnimation(
            name, duration, speed, priority, loop, blendMode, errorOut, sizeof(errorOut));
        if (ret == 0) return lua_error_debug(lua, errorOut);
        lua_pushinteger(lua, ret);
        return 1;
    }

    int onAddArticulatedTrackDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t animation = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const uint64_t partId = static_cast<uint64_t>(luaL_checkinteger(lua, 3));
        const uint8_t mask = static_cast<uint8_t>(luaL_checkinteger(lua, 4));
        char errorOut[255] = "";
        const int ret = meshDebug->mesh.addArticulatedTrack(animation, partId, mask, errorOut, sizeof(errorOut));
        if (ret == 0) return lua_error_debug(lua, errorOut);
        lua_pushinteger(lua, ret);
        return 1;
    }

    int onAddArticulatedKeyDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t animation = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const uint32_t track = static_cast<uint32_t>(luaL_checkinteger(lua, 3) - 1);
        const float time = static_cast<float>(luaL_checknumber(lua, 4));
        const float px = static_cast<float>(luaL_optnumber(lua, 5, 0.0));
        const float py = static_cast<float>(luaL_optnumber(lua, 6, 0.0));
        const float pz = static_cast<float>(luaL_optnumber(lua, 7, 0.0));
        const float qx = static_cast<float>(luaL_optnumber(lua, 8, 0.0));
        const float qy = static_cast<float>(luaL_optnumber(lua, 9, 0.0));
        const float qz = static_cast<float>(luaL_optnumber(lua, 10, 0.0));
        const float qw = static_cast<float>(luaL_optnumber(lua, 11, 1.0));
        const float sx = static_cast<float>(luaL_optnumber(lua, 12, 1.0));
        const float sy = static_cast<float>(luaL_optnumber(lua, 13, 1.0));
        const float sz = static_cast<float>(luaL_optnumber(lua, 14, 1.0));
        char errorOut[255] = "";
        if (!meshDebug->mesh.addArticulatedKey(animation, track, time, px, py, pz, qx, qy, qz, qw,
                                               sx, sy, sz, errorOut, sizeof(errorOut)))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, 1);
        return 1;
    }

    int onSetArticulatedTrackChannelsDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t animation = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const uint32_t track = static_cast<uint32_t>(luaL_checkinteger(lua, 3) - 1);
        const uint8_t channelMask = static_cast<uint8_t>(luaL_checkinteger(lua, 4));
        char errorOut[255] = "";
        if (!meshDebug->mesh.setArticulatedTrackChannels(
                animation, track, channelMask, errorOut, sizeof(errorOut)))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, 1);
        return 1;
    }

    int onSetArticulatedKeyEulerDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t animation = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const uint32_t track = static_cast<uint32_t>(luaL_checkinteger(lua, 3) - 1);
        const float time = static_cast<float>(luaL_checknumber(lua, 4));
        const float eulerX = static_cast<float>(luaL_checknumber(lua, 5));
        const float eulerY = static_cast<float>(luaL_checknumber(lua, 6));
        const float eulerZ = static_cast<float>(luaL_checknumber(lua, 7));
        char errorOut[255] = "";
        if (!meshDebug->mesh.setArticulatedKeyEuler(animation, track, time, eulerX, eulerY, eulerZ,
                                                    errorOut, sizeof(errorOut)))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, 1);
        return 1;
    }

    int onSetArticulatedKeyEasingDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t animation = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const uint32_t track = static_cast<uint32_t>(luaL_checkinteger(lua, 3) - 1);
        const uint32_t key = static_cast<uint32_t>(luaL_checkinteger(lua, 4) - 1);
        const uint8_t easing = static_cast<uint8_t>(luaL_checkinteger(lua, 5));
        char errorOut[255] = "";
        if (!meshDebug->mesh.setArticulatedKeyEasing(animation, track, key, easing,
                                                     errorOut, sizeof(errorOut)))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, 1);
        return 1;
    }

    int onSetArticulatedKeyBezierDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t animation = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const uint32_t track = static_cast<uint32_t>(luaL_checkinteger(lua, 3) - 1);
        const uint32_t key = static_cast<uint32_t>(luaL_checkinteger(lua, 4) - 1);
        const float x1 = static_cast<float>(luaL_checknumber(lua, 5));
        const float y1 = static_cast<float>(luaL_checknumber(lua, 6));
        const float x2 = static_cast<float>(luaL_checknumber(lua, 7));
        const float y2 = static_cast<float>(luaL_checknumber(lua, 8));
        char errorOut[255] = "";
        if (!meshDebug->mesh.setArticulatedKeyBezier(animation, track, key, x1, y1, x2, y2,
                                                     errorOut, sizeof(errorOut)))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, 1);
        return 1;
    }

    int onUpdateArticulatedKeyDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t animation = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const uint32_t track = static_cast<uint32_t>(luaL_checkinteger(lua, 3) - 1);
        const uint32_t key = static_cast<uint32_t>(luaL_checkinteger(lua, 4) - 1);
        const float time = static_cast<float>(luaL_checknumber(lua, 5));
        const float px = static_cast<float>(luaL_optnumber(lua, 6, 0.0));
        const float py = static_cast<float>(luaL_optnumber(lua, 7, 0.0));
        const float pz = static_cast<float>(luaL_optnumber(lua, 8, 0.0));
        const float qx = static_cast<float>(luaL_optnumber(lua, 9, 0.0));
        const float qy = static_cast<float>(luaL_optnumber(lua, 10, 0.0));
        const float qz = static_cast<float>(luaL_optnumber(lua, 11, 0.0));
        const float qw = static_cast<float>(luaL_optnumber(lua, 12, 1.0));
        const float sx = static_cast<float>(luaL_optnumber(lua, 13, 1.0));
        const float sy = static_cast<float>(luaL_optnumber(lua, 14, 1.0));
        const float sz = static_cast<float>(luaL_optnumber(lua, 15, 1.0));
        char errorOut[255] = "";
        if (!meshDebug->mesh.updateArticulatedKey(animation, track, key, time, px, py, pz,
                                                  qx, qy, qz, qw, sx, sy, sz,
                                                  errorOut, sizeof(errorOut)))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, 1);
        return 1;
    }

    int onRemoveArticulatedKeyDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t animation = static_cast<uint32_t>(luaL_checkinteger(lua, 2) - 1);
        const uint32_t track = static_cast<uint32_t>(luaL_checkinteger(lua, 3) - 1);
        const uint32_t key = static_cast<uint32_t>(luaL_checkinteger(lua, 4) - 1);
        char errorOut[255] = "";
        if (!meshDebug->mesh.removeArticulatedKey(animation, track, key, errorOut, sizeof(errorOut)))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, 1);
        return 1;
    }


    namespace
    {
        void pushSkeletonBindMatrix(lua_State *lua, const MATRIX &matrix)
        {
            lua_createtable(lua, 16, 0);
            for (int index = 0; index < 16; ++index)
            {
                lua_pushnumber(lua, matrix.p[index]);
                lua_rawseti(lua, -2, index + 1);
            }
        }

        void pushSkeletonBindVector(lua_State *lua, const VEC3 &value)
        {
            lua_createtable(lua, 0, 3);
            lua_pushnumber(lua, value.x); lua_setfield(lua, -2, "x");
            lua_pushnumber(lua, value.y); lua_setfield(lua, -2, "y");
            lua_pushnumber(lua, value.z); lua_setfield(lua, -2, "z");
        }

        void pushSkeletonBindId(lua_State *lua, const uint64_t id, const char *field)
        {
            char text[17] = "";
            snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(id));
            lua_pushstring(lua, text);
            lua_setfield(lua, -2, field);
        }
    }

    int onGetSkeletonBindReportDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const bool includeDependencyImpact = lua_gettop(lua) < 2 || lua_toboolean(lua, 2) != 0;
        SKELETON_BIND_SUMMARY summary;
        if (!meshDebug->mesh.getSkeletonBindSummary(summary))
        {
            lua_pushnil(lua);
            return 1;
        }

        lua_createtable(lua, 0, 7);
        lua_pushboolean(lua, summary.valid); lua_setfield(lua, -2, "valid");
        lua_pushinteger(lua, summary.boneCount); lua_setfield(lua, -2, "boneCount");
        lua_pushinteger(lua, summary.diagnosticCount); lua_setfield(lua, -2, "diagnosticCount");
        lua_pushinteger(lua, summary.animationClipCount); lua_setfield(lua, -2, "animationClipCount");
        lua_pushboolean(lua, summary.canonical);
        lua_setfield(lua, -2, "canonical");
        lua_pushnumber(lua, summary.maximumReconstructionError);
        lua_setfield(lua, -2, "maximumReconstructionError");
        lua_pushnumber(lua, summary.maximumBindIdentityError);
        lua_setfield(lua, -2, "maximumBindIdentityError");

        lua_createtable(lua, static_cast<int>(summary.boneCount), 0);
        for (uint32_t index = 0; index < summary.boneCount; ++index)
        {
            SKELETON_BIND_BONE_INFO bone;
            if (!meshDebug->mesh.getSkeletonBindBone(index, bone, includeDependencyImpact))
                continue;
            lua_createtable(lua, 0, 18);
            lua_pushinteger(lua, bone.sourceIndex + 1); lua_setfield(lua, -2, "sourceIndex");
            const char *boneName = meshDebug->mesh.getSkeletonBindBoneName(index);
            lua_pushstring(lua, boneName ? boneName : ""); lua_setfield(lua, -2, "name");
            pushSkeletonBindId(lua, bone.boneId, "boneId");
            pushSkeletonBindId(lua, bone.parentBoneId, "parentBoneId");
            lua_pushinteger(lua, bone.parentIndex + 1); lua_setfield(lua, -2, "parentIndex");
            pushSkeletonBindVector(lua, bone.localTranslation); lua_setfield(lua, -2, "localTranslation");
            lua_createtable(lua, 0, 4);
            lua_pushnumber(lua, bone.localRotationX); lua_setfield(lua, -2, "x");
            lua_pushnumber(lua, bone.localRotationY); lua_setfield(lua, -2, "y");
            lua_pushnumber(lua, bone.localRotationZ); lua_setfield(lua, -2, "z");
            lua_pushnumber(lua, bone.localRotationW); lua_setfield(lua, -2, "w");
            lua_setfield(lua, -2, "localRotation");
            pushSkeletonBindVector(lua, bone.localScale); lua_setfield(lua, -2, "localScale");
            pushSkeletonBindMatrix(lua, bone.localBindMatrix); lua_setfield(lua, -2, "localBindMatrix");
            pushSkeletonBindMatrix(lua, bone.globalBindMatrix); lua_setfield(lua, -2, "globalBindMatrix");
            pushSkeletonBindMatrix(lua, bone.inverseGlobalBindMatrix);
            lua_setfield(lua, -2, "inverseGlobalBindMatrix");
            lua_pushnumber(lua, bone.radius); lua_setfield(lua, -2, "radius");
            lua_pushnumber(lua, bone.length); lua_setfield(lua, -2, "length");
            pushSkeletonBindVector(lua, bone.tailOffset); lua_setfield(lua, -2, "tailOffset");
            lua_pushboolean(lua, bone.hasExplicitTail); lua_setfield(lua, -2, "hasExplicitTail");
            lua_pushboolean(lua, bone.connectedToParent); lua_setfield(lua, -2, "connectedToParent");
            lua_pushinteger(lua, bone.childCount); lua_setfield(lua, -2, "childCount");
            lua_pushinteger(lua, bone.weightedVertexCount); lua_setfield(lua, -2, "weightedVertexCount");
            lua_pushinteger(lua, bone.animationTrackCount); lua_setfield(lua, -2, "animationTrackCount");
            lua_pushboolean(lua, bone.weightPaletteReferenced);
            lua_setfield(lua, -2, "weightPaletteReferenced");
            lua_pushboolean(lua, bone.hasNegativeScale); lua_setfield(lua, -2, "hasNegativeScale");
            lua_pushboolean(lua, bone.hasShear); lua_setfield(lua, -2, "hasShear");
            lua_rawseti(lua, -2, index + 1);
        }
        lua_setfield(lua, -2, "bones");

        lua_createtable(lua, static_cast<int>(summary.diagnosticCount), 0);
        for (uint32_t index = 0; index < summary.diagnosticCount; ++index)
        {
            SKELETON_BIND_DIAGNOSTIC_INFO diagnostic;
            if (!meshDebug->mesh.getSkeletonBindDiagnostic(index, diagnostic))
                continue;
            lua_createtable(lua, 0, 5);
            lua_pushstring(lua, diagnostic.code ? diagnostic.code : "unknown");
            lua_setfield(lua, -2, "code");
            lua_pushinteger(lua, diagnostic.sourceIndex + 1); lua_setfield(lua, -2, "sourceIndex");
            const char *boneName = meshDebug->mesh.getSkeletonBindBoneName(diagnostic.sourceIndex);
            lua_pushstring(lua, boneName ? boneName : ""); lua_setfield(lua, -2, "boneName");
            lua_pushnumber(lua, diagnostic.observedError); lua_setfield(lua, -2, "observedError");
            lua_pushboolean(lua, diagnostic.fatal); lua_setfield(lua, -2, "fatal");
            lua_rawseti(lua, -2, index + 1);
        }
        lua_setfield(lua, -2, "diagnostics");
        return 1;
    }

    int onRenameSkeletalBoneDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer index = luaL_checkinteger(lua, 2);
        if (index <= 0) return luaL_error(lua, "canonical bone index must be one-based");
        const char *name = luaL_checkstring(lua, 3);
        char errorOut[255] = "";
        if (!meshDebug->mesh.renameSkeletalBone(static_cast<uint32_t>(index - 1), name,
                                                errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        return 0;
    }

    int onReparentSkeletalBoneDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer index = luaL_checkinteger(lua, 2);
        const lua_Integer parent = luaL_checkinteger(lua, 3);
        if (index <= 0) return luaL_error(lua, "canonical bone index must be one-based");
        if (parent < 0) return luaL_error(lua, "canonical parent index must be zero (root) or one-based");
        const bool preserveGlobal = lua_gettop(lua) < 4 || lua_toboolean(lua, 4) != 0;
        char errorOut[255] = "";
        if (!meshDebug->mesh.reparentSkeletalBone(static_cast<uint32_t>(index - 1),
                                                  parent == 0 ? -1 : static_cast<int32_t>(parent - 1),
                                                  preserveGlobal, errorOut,
                                                  static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        return 0;
    }

    int onSetSkeletalBoneBindDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer index = luaL_checkinteger(lua, 2);
        if (index <= 0) return luaL_error(lua, "canonical bone index must be one-based");
        const VEC3 translation(static_cast<float>(luaL_checknumber(lua, 3)),
                               static_cast<float>(luaL_checknumber(lua, 4)),
                               static_cast<float>(luaL_checknumber(lua, 5)));
        const float rotationX = static_cast<float>(luaL_checknumber(lua, 6));
        const float rotationY = static_cast<float>(luaL_checknumber(lua, 7));
        const float rotationZ = static_cast<float>(luaL_checknumber(lua, 8));
        const float rotationW = static_cast<float>(luaL_checknumber(lua, 9));
        const VEC3 scale(static_cast<float>(luaL_checknumber(lua, 10)),
                         static_cast<float>(luaL_checknumber(lua, 11)),
                         static_cast<float>(luaL_checknumber(lua, 12)));
        const float radius = static_cast<float>(luaL_checknumber(lua, 13));
        const float length = static_cast<float>(luaL_checknumber(lua, 14));
        char errorOut[255] = "";
        if (!meshDebug->mesh.setSkeletalBoneBind(static_cast<uint32_t>(index - 1), translation,
                rotationX, rotationY, rotationZ, rotationW, scale, radius, length,
                errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        return 0;
    }

    int onAddSkeletalBoneDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer parent = luaL_checkinteger(lua, 2);
        if (parent < 0) return luaL_error(lua, "canonical parent index must be zero (root) or one-based");
        const char *name = luaL_checkstring(lua, 3);
        const VEC3 translation(static_cast<float>(luaL_checknumber(lua, 4)),
                               static_cast<float>(luaL_checknumber(lua, 5)),
                               static_cast<float>(luaL_checknumber(lua, 6)));
        const float radius = static_cast<float>(luaL_checknumber(lua, 7));
        const float length = static_cast<float>(luaL_checknumber(lua, 8));
        const bool hasExplicitTail = lua_gettop(lua) < 9 || lua_toboolean(lua, 9) != 0;
        const bool connectedToParent = lua_gettop(lua) >= 10 && lua_toboolean(lua, 10) != 0;
        uint32_t newIndex = 0;
        char errorOut[255] = "";
        if (!meshDebug->mesh.addSkeletalBone(parent == 0 ? -1 : static_cast<int32_t>(parent - 1),
                name, translation, radius, length, hasExplicitTail, connectedToParent, &newIndex,
                errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        lua_pushinteger(lua, static_cast<lua_Integer>(newIndex + 1));
        return 1;
    }

    int onSetSkeletalBoneTailDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer index = luaL_checkinteger(lua, 2);
        if (index <= 0) return luaL_error(lua, "canonical bone index must be one-based");
        const VEC3 tailOffset(static_cast<float>(luaL_checknumber(lua, 3)),
                              static_cast<float>(luaL_checknumber(lua, 4)),
                              static_cast<float>(luaL_checknumber(lua, 5)));
        const bool hasExplicitTail = lua_gettop(lua) < 6 || lua_toboolean(lua, 6) != 0;
        const bool preserveOtherJoints = lua_gettop(lua) < 7 || lua_toboolean(lua, 7) != 0;
        char errorOut[255] = "";
        if (!meshDebug->mesh.setSkeletalBoneTail(static_cast<uint32_t>(index - 1), tailOffset,
                hasExplicitTail, preserveOtherJoints, errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        return 0;
    }

    int onSetSkeletalBoneHeadDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug=getMeshDebugFromRawTable(lua,1,1);
        const lua_Integer index=luaL_checkinteger(lua,2);
        if(index<=0) return luaL_error(lua,"canonical bone index must be one-based");
        const VEC3 translation(static_cast<float>(luaL_checknumber(lua,3)),
                               static_cast<float>(luaL_checknumber(lua,4)),
                               static_cast<float>(luaL_checknumber(lua,5)));
        const bool preserveOtherJoints=lua_gettop(lua)<6||lua_toboolean(lua,6)!=0;
        char errorOut[255]="";
        if(!meshDebug->mesh.setSkeletalBoneHead(static_cast<uint32_t>(index-1),translation,
                preserveOtherJoints,
                errorOut,static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua,errorOut);
        return 0;
    }

    int onTranslateSkeletalBoneSegmentDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug=getMeshDebugFromRawTable(lua,1,1);
        const lua_Integer index=luaL_checkinteger(lua,2);
        if(index<=0) return luaL_error(lua,"canonical bone index must be one-based");
        const VEC3 translation(static_cast<float>(luaL_checknumber(lua,3)),
                               static_cast<float>(luaL_checknumber(lua,4)),
                               static_cast<float>(luaL_checknumber(lua,5)));
        const bool preserveOtherJoints=lua_gettop(lua)<6||lua_toboolean(lua,6)!=0;
        char errorOut[255]="";
        if(!meshDebug->mesh.translateSkeletalBoneSegment(static_cast<uint32_t>(index-1),translation,
                preserveOtherJoints,errorOut,static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua,errorOut);
        return 0;
    }

    int onSetSkeletalBoneConnectedToParentDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug=getMeshDebugFromRawTable(lua,1,1);
        const lua_Integer index=luaL_checkinteger(lua,2);
        if(index<=0) return luaL_error(lua,"canonical bone index must be one-based");
        const bool connected=lua_toboolean(lua,3)!=0;
        const bool preserveOtherJoints=lua_gettop(lua)<4||lua_toboolean(lua,4)!=0;
        char errorOut[255]="";
        if(!meshDebug->mesh.setSkeletalBoneConnectedToParent(static_cast<uint32_t>(index-1),
                connected,preserveOtherJoints,errorOut,static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua,errorOut);
        return 0;
    }

    int onSetSkeletalBoneRadiusDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug=getMeshDebugFromRawTable(lua,1,1);
        const lua_Integer index=luaL_checkinteger(lua,2);
        if(index<=0) return luaL_error(lua,"canonical bone index must be one-based");
        const float radius=static_cast<float>(luaL_checknumber(lua,3));
        const bool includeDescendants=lua_gettop(lua)>=4&&lua_toboolean(lua,4)!=0;
        char errorOut[255]="";
        if(!meshDebug->mesh.setSkeletalBoneRadius(static_cast<uint32_t>(index-1),radius,
                includeDescendants,errorOut,static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua,errorOut);
        return 0;
    }

    int onInitializeSkeletalSkeletonDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const char *name = luaL_checkstring(lua, 2);
        const VEC3 translation(static_cast<float>(luaL_checknumber(lua, 3)),
                               static_cast<float>(luaL_checknumber(lua, 4)),
                               static_cast<float>(luaL_checknumber(lua, 5)));
        const float radius = static_cast<float>(luaL_checknumber(lua, 6));
        const float length = static_cast<float>(luaL_checknumber(lua, 7));
        const bool hasExplicitTail = lua_gettop(lua) < 8 || lua_toboolean(lua, 8) != 0;
        char errorOut[255] = "";
        if (!meshDebug->mesh.initializeSkeletalSkeleton(name, translation, radius, length,
                hasExplicitTail,
                errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        return 0;
    }

    int onAddSkeletalBoneChainDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer parent = luaL_checkinteger(lua, 2);
        if (parent < 0) return luaL_error(lua, "canonical parent index must be zero (root) or one-based");
        const char *prefix = luaL_checkstring(lua, 3);
        const lua_Integer count = luaL_checkinteger(lua, 4);
        if (count <= 0) return luaL_error(lua, "canonical chain count must be positive");
        const VEC3 translation(static_cast<float>(luaL_checknumber(lua, 5)),
                               static_cast<float>(luaL_checknumber(lua, 6)),
                               static_cast<float>(luaL_checknumber(lua, 7)));
        const float radius = static_cast<float>(luaL_checknumber(lua, 8));
        const float length = static_cast<float>(luaL_checknumber(lua, 9));
        uint32_t lastIndex = 0;
        char errorOut[255] = "";
        if (!meshDebug->mesh.addSkeletalBoneChain(parent == 0 ? -1 : static_cast<int32_t>(parent - 1),
                prefix, static_cast<uint32_t>(count), translation, radius, length, &lastIndex,
                errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        lua_pushinteger(lua, static_cast<lua_Integer>(lastIndex + 1));
        return 1;
    }

    int onExtendSkeletalBoneTailDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug=getMeshDebugFromRawTable(lua,1,1);
        const lua_Integer index=luaL_checkinteger(lua,2);
        const lua_Integer count=luaL_checkinteger(lua,3);
        if(index<=0) return luaL_error(lua,"canonical bone index must be one-based");
        if(count<=0) return luaL_error(lua,"canonical extension count must be positive");
        const float radius=static_cast<float>(luaL_checknumber(lua,4));
        const float length=static_cast<float>(luaL_checknumber(lua,5));
        uint32_t lastIndex=0;
        char errorOut[255]="";
        if(!meshDebug->mesh.extendSkeletalBoneTail(static_cast<uint32_t>(index-1),
                static_cast<uint32_t>(count),radius,length,&lastIndex,errorOut,
                static_cast<int>(sizeof(errorOut)))) return lua_error_debug(lua,errorOut);
        lua_pushinteger(lua,static_cast<lua_Integer>(lastIndex+1));
        return 1;
    }

    int onMirrorSkeletalBoneSubtreeDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer index = luaL_checkinteger(lua, 2);
        const lua_Integer axis = luaL_checkinteger(lua, 3);
        if (index <= 0) return luaL_error(lua, "canonical bone index must be one-based");
        if (axis < 1 || axis > 3) return luaL_error(lua, "canonical mirror axis must be 1 (X), 2 (Y), or 3 (Z)");
        const char *prefix = luaL_checkstring(lua, 4);
        uint32_t newRootIndex = 0;
        char errorOut[255] = "";
        if (!meshDebug->mesh.mirrorSkeletalBoneSubtree(static_cast<uint32_t>(index - 1),
                static_cast<uint32_t>(axis - 1), prefix, &newRootIndex,
                errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        lua_pushinteger(lua, static_cast<lua_Integer>(newRootIndex + 1));
        return 1;
    }

    int onRemoveSkeletalBoneDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer index = luaL_checkinteger(lua, 2);
        if (index <= 0) return luaL_error(lua, "canonical bone index must be one-based");
        char errorOut[255] = "";
        if (!meshDebug->mesh.removeSkeletalBone(static_cast<uint32_t>(index - 1),
                                                errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        return 0;
    }

    int onRemoveSkeletalBoneRemappedDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer index = luaL_checkinteger(lua, 2);
        const lua_Integer replacement = luaL_checkinteger(lua, 3);
        if (index <= 0 || replacement <= 0)
            return luaL_error(lua, "canonical bone indices must be one-based");
        const bool discardTracks = lua_toboolean(lua, 4) != 0;
        const bool reparentChildren = lua_toboolean(lua, 5) != 0;
        char errorOut[255] = "";
        if (!meshDebug->mesh.removeSkeletalBoneRemapped(static_cast<uint32_t>(index - 1),
                static_cast<uint32_t>(replacement - 1), discardTracks, reparentChildren,
                errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        return 0;
    }


    int onSetSkeletalVertexWeightDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug=getMeshDebugFromRawTable(lua,1,1);
        const uint32_t vertexIndex=static_cast<uint32_t>(luaL_checkinteger(lua,2)-1);
        const char *names[4];
        float weights[4];
        for (int slot=0; slot<4; ++slot)
        {
            const int nameIndex=3+slot*2;
            names[slot]=lua_isnil(lua,nameIndex) ? nullptr : luaL_checkstring(lua,nameIndex);
            weights[slot]=static_cast<float>(luaL_optnumber(lua,nameIndex+1,0.0));
        }
        char errorOut[255]="";
        if (!meshDebug->mesh.setSkeletalVertexWeight(vertexIndex,
                names[0],weights[0],names[1],weights[1],names[2],weights[2],names[3],weights[3],
                errorOut,static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua,errorOut);
        lua_pushboolean(lua,1);
        return 1;
    }

    int onGetSkeletalVertexWeightDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug=getMeshDebugFromRawTable(lua,1,1);
        const int index=static_cast<int>(luaL_checkinteger(lua,2))-1;
        if (index<0) { lua_pushnil(lua); return 1; }
        const char *names[4]={nullptr,nullptr,nullptr,nullptr};
        float weights[4]={0,0,0,0};
        if (!meshDebug->mesh.getSkeletalVertexWeight(static_cast<uint32_t>(index),
                &names[0],&weights[0],&names[1],&weights[1],&names[2],&weights[2],&names[3],&weights[3]))
        { lua_pushnil(lua); return 1; }
        for (int slot=0; slot<4; ++slot)
        {
            if (names[slot]) lua_pushstring(lua,names[slot]); else lua_pushnil(lua);
            lua_pushnumber(lua,weights[slot]);
        }
        return 8;
    }

    int onHasSkeletalVertexWeightsDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug=getMeshDebugFromRawTable(lua,1,1);
        lua_pushboolean(lua,meshDebug->mesh.hasSkeletalVertexWeights());
        return 1;
    }

    int onInitializeSkeletalVertexWeightsDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer boneIndex = luaL_checkinteger(lua, 2);
        if (boneIndex <= 0) return luaL_error(lua, "canonical bone index must be one-based");
        uint32_t vertexCount = 0;
        char errorOut[255] = "";
        if (!meshDebug->mesh.initializeSkeletalVertexWeights(static_cast<uint32_t>(boneIndex - 1),
                &vertexCount, errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        lua_pushinteger(lua, static_cast<lua_Integer>(vertexCount));
        return 1;
    }

    int onGetSkeletalAnimationReportDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const uint32_t clipCount = meshDebug->mesh.getTotalSkeletalClips();
        lua_createtable(lua, static_cast<int>(clipCount), 0);
        for (uint32_t clipIndex = 0; clipIndex < clipCount; ++clipIndex)
        {
            SKELETAL_CLIP_INFO clip;
            if (!meshDebug->mesh.getSkeletalClip(clipIndex, clip)) continue;
            lua_createtable(lua, 0, 7);
            pushSkeletonBindId(lua, clip.clipId, "clipId");
            lua_pushstring(lua, meshDebug->mesh.getSkeletalClipName(clipIndex)); lua_setfield(lua, -2, "name");
            lua_pushnumber(lua, clip.duration); lua_setfield(lua, -2, "duration");
            lua_pushboolean(lua, clip.loop); lua_setfield(lua, -2, "loop");
            lua_createtable(lua, static_cast<int>(clip.trackCount), 0);
            for (uint32_t trackIndex = 0; trackIndex < clip.trackCount; ++trackIndex)
            {
                SKELETAL_TRACK_INFO track;
                if (!meshDebug->mesh.getSkeletalTrack(clipIndex, trackIndex, track)) continue;
                lua_createtable(lua, 0, 6);
                pushSkeletonBindId(lua, track.boneId, "boneId");
                lua_pushinteger(lua, track.boneIndex + 1); lua_setfield(lua, -2, "boneIndex");
                const char *boneName = meshDebug->mesh.getSkeletonBindBoneName(track.boneIndex);
                lua_pushstring(lua, boneName ? boneName : ""); lua_setfield(lua, -2, "boneName");
                lua_pushinteger(lua, track.channelMask); lua_setfield(lua, -2, "channelMask");
                lua_createtable(lua, static_cast<int>(track.keyCount), 0);
                for (uint32_t keyIndex = 0; keyIndex < track.keyCount; ++keyIndex)
                {
                    SKELETAL_KEY_INFO key;
                    if (!meshDebug->mesh.getSkeletalKey(clipIndex, trackIndex, keyIndex, key)) continue;
                    lua_createtable(lua, 0, 8);
                    lua_pushnumber(lua, key.time); lua_setfield(lua, -2, "time");
                    pushSkeletonBindVector(lua, key.localTranslation); lua_setfield(lua, -2, "translation");
                    lua_createtable(lua, 0, 4);
                    lua_pushnumber(lua, key.localRotationX); lua_setfield(lua, -2, "x");
                    lua_pushnumber(lua, key.localRotationY); lua_setfield(lua, -2, "y");
                    lua_pushnumber(lua, key.localRotationZ); lua_setfield(lua, -2, "z");
                    lua_pushnumber(lua, key.localRotationW); lua_setfield(lua, -2, "w");
                    lua_setfield(lua, -2, "rotation");
                    pushSkeletonBindVector(lua, key.localScale); lua_setfield(lua, -2, "scale");
                    lua_pushinteger(lua, key.easing); lua_setfield(lua, -2, "easing");
                    lua_createtable(lua, 0, 4);
                    lua_pushnumber(lua, key.bezierX1); lua_setfield(lua, -2, "x1");
                    lua_pushnumber(lua, key.bezierY1); lua_setfield(lua, -2, "y1");
                    lua_pushnumber(lua, key.bezierX2); lua_setfield(lua, -2, "x2");
                    lua_pushnumber(lua, key.bezierY2); lua_setfield(lua, -2, "y2");
                    lua_setfield(lua, -2, "bezier");
                    lua_rawseti(lua, -2, keyIndex + 1);
                }
                lua_setfield(lua, -2, "keys");
                lua_rawseti(lua, -2, trackIndex + 1);
            }
            lua_setfield(lua, -2, "tracks");
            lua_rawseti(lua, -2, clipIndex + 1);
        }
        return 1;
    }

    int onAddSkeletalClipDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const char *name = luaL_checkstring(lua, 2);
        const float duration = static_cast<float>(luaL_checknumber(lua, 3));
        const bool loop = lua_toboolean(lua, 4) != 0;
        uint32_t index = 0;
        char errorOut[255] = "";
        if (!meshDebug->mesh.addSkeletalClip(name, duration, loop, &index,
                errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        lua_pushinteger(lua, static_cast<lua_Integer>(index + 1));
        return 1;
    }

    int onUpdateSkeletalClipDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer index = luaL_checkinteger(lua, 2);
        if (index <= 0) return luaL_error(lua, "canonical clip index must be one-based");
        const char *name = luaL_checkstring(lua, 3);
        const float duration = static_cast<float>(luaL_checknumber(lua, 4));
        const bool loop = lua_toboolean(lua, 5) != 0;
        char errorOut[255] = "";
        if (!meshDebug->mesh.updateSkeletalClip(static_cast<uint32_t>(index - 1), name, duration, loop,
                errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, true);
        return 1;
    }

    int onRemoveSkeletalClipDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer index = luaL_checkinteger(lua, 2);
        if (index <= 0) return luaL_error(lua, "canonical clip index must be one-based");
        char errorOut[255] = "";
        if (!meshDebug->mesh.removeSkeletalClip(static_cast<uint32_t>(index - 1), errorOut,
                static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, true);
        return 1;
    }

    int onAddSkeletalTrackDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer clip = luaL_checkinteger(lua, 2);
        const lua_Integer bone = luaL_checkinteger(lua, 3);
        const lua_Integer mask = luaL_checkinteger(lua, 4);
        if (clip <= 0 || bone <= 0) return luaL_error(lua, "canonical clip and bone indices must be one-based");
        uint32_t index = 0;
        char errorOut[255] = "";
        if (!meshDebug->mesh.addSkeletalTrack(static_cast<uint32_t>(clip - 1),
                static_cast<uint32_t>(bone - 1), static_cast<uint8_t>(mask), &index,
                errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        lua_pushinteger(lua, static_cast<lua_Integer>(index + 1));
        return 1;
    }

    int onUpdateSkeletalTrackChannelsDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer clip = luaL_checkinteger(lua, 2);
        const lua_Integer track = luaL_checkinteger(lua, 3);
        const lua_Integer mask = luaL_checkinteger(lua, 4);
        if (clip <= 0 || track <= 0) return luaL_error(lua, "canonical clip and track indices must be one-based");
        char errorOut[255] = "";
        if (!meshDebug->mesh.updateSkeletalTrackChannels(static_cast<uint32_t>(clip - 1),
                static_cast<uint32_t>(track - 1), static_cast<uint8_t>(mask), errorOut,
                static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, true);
        return 1;
    }

    int onRemoveSkeletalTrackDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer clip = luaL_checkinteger(lua, 2);
        const lua_Integer track = luaL_checkinteger(lua, 3);
        if (clip <= 0 || track <= 0) return luaL_error(lua, "canonical clip and track indices must be one-based");
        char errorOut[255] = "";
        if (!meshDebug->mesh.removeSkeletalTrack(static_cast<uint32_t>(clip - 1),
                static_cast<uint32_t>(track - 1), errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, true);
        return 1;
    }

    int onAddSkeletalKeyDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer clip = luaL_checkinteger(lua, 2);
        const lua_Integer track = luaL_checkinteger(lua, 3);
        const float time = static_cast<float>(luaL_checknumber(lua, 4));
        if (clip <= 0 || track <= 0) return luaL_error(lua, "canonical clip and track indices must be one-based");
        uint32_t index = 0;
        char errorOut[255] = "";
        if (!meshDebug->mesh.addSkeletalKey(static_cast<uint32_t>(clip - 1),
                static_cast<uint32_t>(track - 1), time, &index, errorOut,
                static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        lua_pushinteger(lua, static_cast<lua_Integer>(index + 1));
        return 1;
    }

    int onUpdateSkeletalKeyDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer clip = luaL_checkinteger(lua, 2);
        const lua_Integer track = luaL_checkinteger(lua, 3);
        const lua_Integer key = luaL_checkinteger(lua, 4);
        if (clip <= 0 || track <= 0 || key <= 0)
            return luaL_error(lua, "canonical clip, track, and key indices must be one-based");
        const float time = static_cast<float>(luaL_checknumber(lua, 5));
        const VEC3 translation(static_cast<float>(luaL_checknumber(lua, 6)),
                               static_cast<float>(luaL_checknumber(lua, 7)),
                               static_cast<float>(luaL_checknumber(lua, 8)));
        const float qx = static_cast<float>(luaL_checknumber(lua, 9));
        const float qy = static_cast<float>(luaL_checknumber(lua, 10));
        const float qz = static_cast<float>(luaL_checknumber(lua, 11));
        const float qw = static_cast<float>(luaL_checknumber(lua, 12));
        const VEC3 scale(static_cast<float>(luaL_checknumber(lua, 13)),
                         static_cast<float>(luaL_checknumber(lua, 14)),
                         static_cast<float>(luaL_checknumber(lua, 15)));
        const lua_Integer easing = luaL_checkinteger(lua, 16);
        const float x1 = static_cast<float>(luaL_checknumber(lua, 17));
        const float y1 = static_cast<float>(luaL_checknumber(lua, 18));
        const float x2 = static_cast<float>(luaL_checknumber(lua, 19));
        const float y2 = static_cast<float>(luaL_checknumber(lua, 20));
        char errorOut[255] = "";
        if (!meshDebug->mesh.updateSkeletalKey(static_cast<uint32_t>(clip - 1),
                static_cast<uint32_t>(track - 1), static_cast<uint32_t>(key - 1), time,
                translation, qx, qy, qz, qw, scale, static_cast<uint8_t>(easing),
                x1, y1, x2, y2, errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, true);
        return 1;
    }

    int onRemoveSkeletalKeyDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer clip = luaL_checkinteger(lua, 2);
        const lua_Integer track = luaL_checkinteger(lua, 3);
        const lua_Integer key = luaL_checkinteger(lua, 4);
        if (clip <= 0 || track <= 0 || key <= 0)
            return luaL_error(lua, "canonical clip, track, and key indices must be one-based");
        char errorOut[255] = "";
        if (!meshDebug->mesh.removeSkeletalKey(static_cast<uint32_t>(clip - 1),
                static_cast<uint32_t>(track - 1), static_cast<uint32_t>(key - 1), errorOut,
                static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        lua_pushboolean(lua, true);
        return 1;
    }

    int onMoveSkeletalKeysDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug=getMeshDebugFromRawTable(lua,1,1);
        const lua_Integer clip=luaL_checkinteger(lua,2);
        if (clip<=0) return luaL_error(lua,"canonical clip index must be one-based");
        luaL_checktype(lua,3,LUA_TTABLE);
        const lua_Integer itemCount=static_cast<lua_Integer>(lua_rawlen(lua,3));
        if (itemCount<=0 || (itemCount%2)!=0)
            return luaL_error(lua,"canonical key move references must contain track/key pairs");
        std::vector<uint32_t> tracks;
        std::vector<uint32_t> keys;
        tracks.reserve(static_cast<size_t>(itemCount/2));
        keys.reserve(static_cast<size_t>(itemCount/2));
        for (lua_Integer item=1;item<=itemCount;item+=2)
        {
            lua_rawgeti(lua,3,item);
            const lua_Integer track=luaL_checkinteger(lua,-1);
            lua_pop(lua,1);
            lua_rawgeti(lua,3,item+1);
            const lua_Integer key=luaL_checkinteger(lua,-1);
            lua_pop(lua,1);
            if (track<=0 || key<=0)
                return luaL_error(lua,"canonical track and key indices must be one-based");
            tracks.push_back(static_cast<uint32_t>(track-1));
            keys.push_back(static_cast<uint32_t>(key-1));
        }
        const float delta=static_cast<float>(luaL_checknumber(lua,4));
        char errorOut[255]="";
        if (!meshDebug->mesh.moveSkeletalKeys(static_cast<uint32_t>(clip-1),tracks.data(),
                keys.data(),static_cast<uint32_t>(tracks.size()),delta,errorOut,
                static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua,errorOut);
        lua_pushboolean(lua,true);
        return 1;
    }

    int onDuplicateSkeletalKeysDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug=getMeshDebugFromRawTable(lua,1,1);
        const lua_Integer clip=luaL_checkinteger(lua,2);
        if (clip<=0) return luaL_error(lua,"canonical clip index must be one-based");
        luaL_checktype(lua,3,LUA_TTABLE);
        const lua_Integer itemCount=static_cast<lua_Integer>(lua_rawlen(lua,3));
        if (itemCount<=0 || (itemCount%2)!=0)
            return luaL_error(lua,"canonical key duplicate references must contain track/key pairs");
        std::vector<uint32_t> tracks;
        std::vector<uint32_t> keys;
        tracks.reserve(static_cast<size_t>(itemCount/2));
        keys.reserve(static_cast<size_t>(itemCount/2));
        for (lua_Integer item=1;item<=itemCount;item+=2)
        {
            lua_rawgeti(lua,3,item);
            const lua_Integer track=luaL_checkinteger(lua,-1);
            lua_pop(lua,1);
            lua_rawgeti(lua,3,item+1);
            const lua_Integer key=luaL_checkinteger(lua,-1);
            lua_pop(lua,1);
            if (track<=0 || key<=0)
                return luaL_error(lua,"canonical track and key indices must be one-based");
            tracks.push_back(static_cast<uint32_t>(track-1));
            keys.push_back(static_cast<uint32_t>(key-1));
        }
        const float delta=static_cast<float>(luaL_checknumber(lua,4));
        char errorOut[255]="";
        if (!meshDebug->mesh.duplicateSkeletalKeys(static_cast<uint32_t>(clip-1),tracks.data(),
                keys.data(),static_cast<uint32_t>(tracks.size()),delta,errorOut,
                static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua,errorOut);
        lua_pushboolean(lua,true);
        return 1;
    }

    int onInsertSkeletalKeysRippleDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug=getMeshDebugFromRawTable(lua,1,1);
        const lua_Integer clip=luaL_checkinteger(lua,2);
        if (clip<=0) return luaL_error(lua,"canonical clip index must be one-based");
        luaL_checktype(lua,3,LUA_TTABLE);
        const lua_Integer itemCount=static_cast<lua_Integer>(lua_rawlen(lua,3));
        if (itemCount<=0 || (itemCount%2)!=0)
            return luaL_error(lua,"canonical ripple references must contain track/key pairs");
        std::vector<uint32_t> tracks;
        std::vector<uint32_t> keys;
        tracks.reserve(static_cast<size_t>(itemCount/2));
        keys.reserve(static_cast<size_t>(itemCount/2));
        for (lua_Integer item=1;item<=itemCount;item+=2)
        {
            lua_rawgeti(lua,3,item);
            const lua_Integer track=luaL_checkinteger(lua,-1);
            lua_pop(lua,1);
            lua_rawgeti(lua,3,item+1);
            const lua_Integer key=luaL_checkinteger(lua,-1);
            lua_pop(lua,1);
            if (track<=0 || key<=0)
                return luaL_error(lua,"canonical track and key indices must be one-based");
            tracks.push_back(static_cast<uint32_t>(track-1));
            keys.push_back(static_cast<uint32_t>(key-1));
        }
        const float insertionTime=static_cast<float>(luaL_checknumber(lua,4));
        char errorOut[255]="";
        if (!meshDebug->mesh.insertSkeletalKeysRipple(static_cast<uint32_t>(clip-1),tracks.data(),
                keys.data(),static_cast<uint32_t>(tracks.size()),insertionTime,errorOut,
                static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua,errorOut);
        lua_pushboolean(lua,true);
        return 1;
    }

    int onInsertSkeletalEmptyTimeDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug=getMeshDebugFromRawTable(lua,1,1);
        const lua_Integer clip=luaL_checkinteger(lua,2);
        if (clip<=0) return luaL_error(lua,"canonical clip index must be one-based");
        const float insertionTime=static_cast<float>(luaL_checknumber(lua,3));
        const float duration=static_cast<float>(luaL_checknumber(lua,4));
        char errorOut[255]="";
        if (!meshDebug->mesh.insertSkeletalEmptyTime(static_cast<uint32_t>(clip-1),insertionTime,
                duration,errorOut,static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua,errorOut);
        lua_pushboolean(lua,true);
        return 1;
    }

    int onRemoveSkeletalTimeRangeDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug=getMeshDebugFromRawTable(lua,1,1);
        const lua_Integer clip=luaL_checkinteger(lua,2);
        if (clip<=0) return luaL_error(lua,"canonical clip index must be one-based");
        const float startTime=static_cast<float>(luaL_checknumber(lua,3));
        const float duration=static_cast<float>(luaL_checknumber(lua,4));
        uint32_t removedKeyCount=0;
        char errorOut[255]="";
        if (!meshDebug->mesh.removeSkeletalTimeRange(static_cast<uint32_t>(clip-1),startTime,
                duration,&removedKeyCount,errorOut,static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua,errorOut);
        lua_pushinteger(lua,static_cast<lua_Integer>(removedKeyCount));
        return 1;
    }

    int onEvaluateSkeletalAuthoringPoseDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug = getMeshDebugFromRawTable(lua, 1, 1);
        const lua_Integer clip = luaL_checkinteger(lua, 2);
        if (clip <= 0) return luaL_error(lua, "canonical clip index must be one-based");
        const float time = static_cast<float>(luaL_checknumber(lua, 3));
        const char *methodName = luaL_checkstring(lua, 4);
        const SKELETAL_SHADER_METHOD method = strcmp(methodName, "dqs") == 0
            ? SKELETAL_SHADER_METHOD::DQS_RIGID : strcmp(methodName, "lbs") == 0
            ? SKELETAL_SHADER_METHOD::LBS : SKELETAL_SHADER_METHOD::NONE;
        const lua_Integer overrideIndex = luaL_optinteger(lua, 5, 0);
        SKELETAL_KEY_INFO overrideLocal;
        const SKELETAL_KEY_INFO *overridePtr = nullptr;
        if (overrideIndex > 0)
        {
            overrideLocal.localTranslation = VEC3(static_cast<float>(luaL_checknumber(lua, 6)),
                static_cast<float>(luaL_checknumber(lua, 7)), static_cast<float>(luaL_checknumber(lua, 8)));
            overrideLocal.localRotationX = static_cast<float>(luaL_checknumber(lua, 9));
            overrideLocal.localRotationY = static_cast<float>(luaL_checknumber(lua, 10));
            overrideLocal.localRotationZ = static_cast<float>(luaL_checknumber(lua, 11));
            overrideLocal.localRotationW = static_cast<float>(luaL_checknumber(lua, 12));
            overrideLocal.localScale = VEC3(static_cast<float>(luaL_checknumber(lua, 13)),
                static_cast<float>(luaL_checknumber(lua, 14)), static_cast<float>(luaL_checknumber(lua, 15)));
            overridePtr = &overrideLocal;
        }
        char errorOut[255] = "";
        if (!meshDebug->mesh.evaluateSkeletalAuthoringPose(static_cast<uint32_t>(clip - 1), time,
                overrideIndex > 0 ? static_cast<int32_t>(overrideIndex - 1) : -1,
                overridePtr, method, errorOut, static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua, errorOut);
        lua_createtable(lua, 0, 4);
        lua_pushnumber(lua, time); lua_setfield(lua, -2, "time");
        lua_pushstring(lua, methodName); lua_setfield(lua, -2, "method");
        const uint32_t boneCount = meshDebug->mesh.getSkeletalAuthoringPoseBoneCount();
        lua_createtable(lua, static_cast<int>(boneCount), 0);
        for (uint32_t boneIndex = 0; boneIndex < boneCount; ++boneIndex)
        {
            SKELETAL_POSE_BONE_INFO bone;
            if (!meshDebug->mesh.getSkeletalAuthoringPoseBone(boneIndex, bone)) continue;
            lua_createtable(lua, 0, 5);
            const char *name = meshDebug->mesh.getSkeletonBindBoneName(boneIndex);
            lua_pushstring(lua, name ? name : ""); lua_setfield(lua, -2, "name");
            pushSkeletonBindVector(lua, bone.localTranslation); lua_setfield(lua, -2, "localTranslation");
            lua_createtable(lua, 0, 4);
            lua_pushnumber(lua, bone.localRotationX); lua_setfield(lua, -2, "x");
            lua_pushnumber(lua, bone.localRotationY); lua_setfield(lua, -2, "y");
            lua_pushnumber(lua, bone.localRotationZ); lua_setfield(lua, -2, "z");
            lua_pushnumber(lua, bone.localRotationW); lua_setfield(lua, -2, "w");
            lua_setfield(lua, -2, "localRotation");
            pushSkeletonBindVector(lua, bone.localScale); lua_setfield(lua, -2, "localScale");
            pushSkeletonBindMatrix(lua, bone.globalMatrix); lua_setfield(lua, -2, "globalMatrix");
            lua_rawseti(lua, -2, boneIndex + 1);
        }
        lua_setfield(lua, -2, "bones");
        lua_createtable(lua, static_cast<int>(boneCount), 0);
        for (uint32_t boneIndex=0; boneIndex<boneCount; ++boneIndex)
        {
            SKELETAL_POSE_BONE_INFO bone;
            if (!meshDebug->mesh.getSkeletalAuthoringPoseBone(boneIndex,bone))
                return luaL_error(lua,"failed to read canonical authoring bone identity");
            char boneId[17]="";
            snprintf(boneId,sizeof(boneId),"%016llx",static_cast<unsigned long long>(bone.boneId));
            lua_pushstring(lua,boneId);
            lua_rawseti(lua,-2,boneIndex+1);
        }
        lua_setfield(lua,-2,"boneIds");
        const uint32_t paletteSize = meshDebug->mesh.getSkeletalAuthoringPaletteSize();
        std::vector<float> palette(paletteSize);
        if (!meshDebug->mesh.copySkeletalAuthoringPalette(palette.data(), paletteSize))
            return luaL_error(lua, "failed to copy canonical authoring palette");
        lua_createtable(lua, static_cast<int>(paletteSize), 0);
        for (uint32_t index = 0; index < paletteSize; ++index)
        { lua_pushnumber(lua, palette[index]); lua_rawseti(lua, -2, index + 1); }
        lua_setfield(lua, -2, "palette");
        return 1;
    }

    int onCommitSkeletalAuthoringKeyDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug=getMeshDebugFromRawTable(lua,1,1);
        const lua_Integer clip=luaL_checkinteger(lua,2);
        const lua_Integer bone=luaL_checkinteger(lua,3);
        if (clip<=0 || bone<=0) return luaL_error(lua,"canonical clip and bone indices must be one-based");
        SKELETAL_KEY_INFO local;
        local.time=static_cast<float>(luaL_checknumber(lua,4));
        const uint8_t channelMask=static_cast<uint8_t>(luaL_checkinteger(lua,5));
        local.localTranslation=VEC3(static_cast<float>(luaL_checknumber(lua,6)),
            static_cast<float>(luaL_checknumber(lua,7)),static_cast<float>(luaL_checknumber(lua,8)));
        local.localRotationX=static_cast<float>(luaL_checknumber(lua,9));
        local.localRotationY=static_cast<float>(luaL_checknumber(lua,10));
        local.localRotationZ=static_cast<float>(luaL_checknumber(lua,11));
        local.localRotationW=static_cast<float>(luaL_checknumber(lua,12));
        local.localScale=VEC3(static_cast<float>(luaL_checknumber(lua,13)),
            static_cast<float>(luaL_checknumber(lua,14)),static_cast<float>(luaL_checknumber(lua,15)));
        bool created=false;
        char errorOut[255]="";
        if (!meshDebug->mesh.commitSkeletalAuthoringKey(static_cast<uint32_t>(clip-1),
                static_cast<uint32_t>(bone-1),local.time,channelMask,local,&created,errorOut,
                static_cast<int>(sizeof(errorOut))))
            return lua_error_debug(lua,errorOut);
        lua_pushboolean(lua,created);
        return 1;
    }

    int onGetTotalSkeletalWeightBonesDebugLua(lua_State *lua)
    {
        MESH_DEBUG_LUA *meshDebug=getMeshDebugFromRawTable(lua,1,1);
        lua_pushinteger(lua,static_cast<lua_Integer>(meshDebug->mesh.getTotalSkeletalWeightBones()));
        return 1;
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
        return setDynamicVariable(lua, meshDebug->lsDynamicVar, what);
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
        return getDynamicVariable(lua, meshDebug->lsDynamicVar, what);
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
        luaL_Reg regFrameMeshMethods[] = {{"fakeRelease", onFakeReleaseMeshManagerLua},
                                          {"load", onLoadMeshDebugLua},
                                          {"save", onSaveMeshDebugLua},
                                          {"setType", onSetTypeMeshDebugLua},
                                          {"getType", onGetTypeMeshDebugLua},
			                              {"setModeDraw", onSetMode_drawMeshDebugLua},
			                              {"getModeDraw", onGetMode_drawMeshDebugLua},
										  {"setModeCullFace", onSetMode_CullFaceMeshDebugLua},
			                              {"getModeCullFace", onGetMode_CullFaceMeshDebugLua},
										  {"setModeFrontFace", onSetMode_FrontFaceMeshDebugLua},
			                              {"getModeFrontFace", onGetMode_FrontFaceMeshDebugLua},
                                          {"getVersion", onGetVersionMeshDebugLua},
                                          {"getMaterial", onGetMaterialMeshDebugLua},
                                          {"setMaterial", onSetMaterialMeshDebugLua},
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
                                          {"getMaterialTexture", onGetMaterialTextureNameMeshDebugLua},
                                          {"setMaterialTexture", onSetMaterialTextureNameMeshDebugLua},
                                          {"getFxTexture", onGetFxTextureMeshDebugLua},
                                          {"setFxTexture", onSetFxTextureMeshDebugLua},
                                          {"addFrame", onAddFrameDebugLua},
                                          {"removeFrame", onRemoveFrameDebugLua},
                                          {"addSubSet", onAddSubsetDebugLua},
                                          {"removeSubset", onRemoveSubsetDebugLua},
                                          {"moveSubsetUp", onMoveSubsetUpDebugLua},
                                          {"copyFrameFrom", onCopyFrameFromDebugLua},
                                          {"copySubsetFrom", onCopySubsetFromDebugLua},
                                          {"addAnim", onAddAnimationDebugLua},
                                          {"removeAnim", onRemoveAnimationDebugLua},
                                          {"centralize", onCentralizeMeshDebugLua},
                                          {"centralizeItself", onCentralizeItselfMeshDebugLua},
                                          {"rotateFrame", onRotateFrameDebugLua},
                                          {"scaleFrame", onScaleFrameDebugLua},
                                          {"scaleSkeletalAsset", onScaleSkeletalAssetDebugLua},
                                          {"translateFrame", onTranslateFrameDebugLua},
                                          {"check", onCheckMeshDebugLua},
                                          {"getStride", onGetStrideMeshDebugLua},
                                          {"setStride", onSetStrideMeshDebugLua},
                                          {"enableNormal", onEnableNormalsMeshDebugLua},
                                          {"removeNormals", onRemoveNormalsMeshDebugLua},
                                          {"addNormals", onAddNormalsMeshDebugLua},
                                          {"enableUv", onEnableUvMeshDebugLua},
                                          {"copyAnimationsFromMesh", onCopyAnimationsFromMeshLua},
                                          {"updateAnim", onUpdateAnimationDebugLua},
                                          {"getAnim", onGetDetailAnimationDebugLua},
                                          {"getSkeletonBindReport", onGetSkeletonBindReportDebugLua},
                                          {"renameSkeletalBone", onRenameSkeletalBoneDebugLua},
                                          {"reparentSkeletalBone", onReparentSkeletalBoneDebugLua},
                                          {"setSkeletalBoneBind", onSetSkeletalBoneBindDebugLua},
                                          {"setSkeletalBoneTail", onSetSkeletalBoneTailDebugLua},
                                          {"setSkeletalBoneHead", onSetSkeletalBoneHeadDebugLua},
                                          {"translateSkeletalBoneSegment", onTranslateSkeletalBoneSegmentDebugLua},
                                          {"setSkeletalBoneConnectedToParent", onSetSkeletalBoneConnectedToParentDebugLua},
                                          {"setSkeletalBoneRadius", onSetSkeletalBoneRadiusDebugLua},
                                          {"addSkeletalBone", onAddSkeletalBoneDebugLua},
                                          {"initializeSkeletalSkeleton", onInitializeSkeletalSkeletonDebugLua},
                                          {"addSkeletalBoneChain", onAddSkeletalBoneChainDebugLua},
                                          {"extendSkeletalBoneTail", onExtendSkeletalBoneTailDebugLua},
                                          {"mirrorSkeletalBoneSubtree", onMirrorSkeletalBoneSubtreeDebugLua},
                                          {"removeSkeletalBone", onRemoveSkeletalBoneDebugLua},
                                          {"removeSkeletalBoneRemapped", onRemoveSkeletalBoneRemappedDebugLua},
                                          {"setSkeletalVertexWeight", onSetSkeletalVertexWeightDebugLua},
                                          {"getSkeletalVertexWeight", onGetSkeletalVertexWeightDebugLua},
                                          {"hasSkeletalVertexWeights", onHasSkeletalVertexWeightsDebugLua},
                                          {"initializeSkeletalVertexWeights", onInitializeSkeletalVertexWeightsDebugLua},
                                          {"getSkeletalAnimationReport", onGetSkeletalAnimationReportDebugLua},
                                          {"addSkeletalClip", onAddSkeletalClipDebugLua},
                                          {"updateSkeletalClip", onUpdateSkeletalClipDebugLua},
                                          {"removeSkeletalClip", onRemoveSkeletalClipDebugLua},
                                          {"addSkeletalTrack", onAddSkeletalTrackDebugLua},
                                          {"updateSkeletalTrackChannels", onUpdateSkeletalTrackChannelsDebugLua},
                                          {"removeSkeletalTrack", onRemoveSkeletalTrackDebugLua},
                                          {"addSkeletalKey", onAddSkeletalKeyDebugLua},
                                          {"updateSkeletalKey", onUpdateSkeletalKeyDebugLua},
                                          {"removeSkeletalKey", onRemoveSkeletalKeyDebugLua},
                                          {"moveSkeletalKeys", onMoveSkeletalKeysDebugLua},
                                          {"duplicateSkeletalKeys", onDuplicateSkeletalKeysDebugLua},
                                          {"insertSkeletalKeysRipple", onInsertSkeletalKeysRippleDebugLua},
                                          {"insertSkeletalEmptyTime", onInsertSkeletalEmptyTimeDebugLua},
                                          {"removeSkeletalTimeRange", onRemoveSkeletalTimeRangeDebugLua},
                                          {"evaluateSkeletalAuthoringPose", onEvaluateSkeletalAuthoringPoseDebugLua},
                                          {"commitSkeletalAuthoringKey", onCommitSkeletalAuthoringKeyDebugLua},
                                          {"getTotalSkeletalWeightBones", onGetTotalSkeletalWeightBonesDebugLua},
                                          {"getTotalArticulatedParts", onGetTotalArticulatedPartsDebugLua},
                                          {"getArticulatedPart", onGetArticulatedPartDebugLua},
                                          {"initializeArticulatedParts", onInitializeArticulatedPartsDebugLua},
                                          {"removeArticulatedParts", onRemoveArticulatedPartsDebugLua},
                                          {"addArticulatedPart", onAddArticulatedPartDebugLua},
                                          {"updateArticulatedPart", onUpdateArticulatedPartDebugLua},
                                          {"getTotalArticulatedAnimations", onGetTotalArticulatedAnimationsDebugLua},
                                          {"getArticulatedAnimationName", onGetArticulatedAnimationNameDebugLua},
                                          {"getArticulatedAnimation", onGetArticulatedAnimationDebugLua},
                                          {"updateArticulatedAnimation", onUpdateArticulatedAnimationDebugLua},
                                          {"getTotalArticulatedTracks", onGetTotalArticulatedTracksDebugLua},
                                          {"getArticulatedTrack", onGetArticulatedTrackDebugLua},
                                          {"getArticulatedKey", onGetArticulatedKeyDebugLua},
                                          {"addArticulatedAnimation", onAddArticulatedAnimationDebugLua},
                                          {"removeArticulatedAnimation", onRemoveArticulatedAnimationDebugLua},
                                          {"addArticulatedTrack", onAddArticulatedTrackDebugLua},
                                          {"setArticulatedTrackChannels", onSetArticulatedTrackChannelsDebugLua},
                                          {"addArticulatedKey", onAddArticulatedKeyDebugLua},
                                          {"setArticulatedKeyEuler", onSetArticulatedKeyEulerDebugLua},
                                          {"setArticulatedKeyEasing", onSetArticulatedKeyEasingDebugLua},
                                          {"setArticulatedKeyBezier", onSetArticulatedKeyBezierDebugLua},
                                          {"updateArticulatedKey", onUpdateArticulatedKeyDebugLua},
                                          {"removeArticulatedKey", onRemoveArticulatedKeyDebugLua},
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
                                          {"fakeRelease", onFakeReleaseMeshManagerLua},
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
